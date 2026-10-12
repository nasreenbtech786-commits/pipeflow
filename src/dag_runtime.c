#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "dag_runtime.h"

#define BUF_SIZE 4096

static int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);

        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }

        if (n == 0)
            return -1;

        off += (size_t)n;
    }

    return 0;
}

static void close_all_pipes(
    int edge_pipes[][2],
    int edge_count,
    int stage_input[][2],
    int stage_output[][2],
    int stage_count)
{
    for (int e = 0; e < edge_count; e++) {
        for (int k = 0; k < 2; k++) {
            if (edge_pipes[e][k] >= 0) {
                close(edge_pipes[e][k]);
                edge_pipes[e][k] = -1;
            }
        }
    }

    for (int i = 0; i < stage_count; i++) {
        for (int k = 0; k < 2; k++) {
            if (stage_input[i][k] >= 0) {
                close(stage_input[i][k]);
                stage_input[i][k] = -1;
            }

            if (stage_output[i][k] >= 0) {
                close(stage_output[i][k]);
                stage_output[i][k] = -1;
            }
        }
    }
}

/* Copy a producer's output to all its outgoing edges. */
static void router_process(
    int input_fd,
    int output_fds[],
    int output_count)
{
    signal(SIGPIPE, SIG_IGN);

    char buf[BUF_SIZE];
    int active[MAX_EDGES];

    for (int i = 0; i < output_count; i++)
        active[i] = 1;

    for (;;) {
        ssize_t n = read(input_fd, buf, sizeof(buf));

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < output_count; i++) {
            if (active[i] &&
                write_all(output_fds[i], buf, (size_t)n) < 0) {
                active[i] = 0;
                close(output_fds[i]);
                output_fds[i] = -1;
            }
        }
    }

    close(input_fd);

    for (int i = 0; i < output_count; i++) {
        if (active[i] && output_fds[i] >= 0)
            close(output_fds[i]);
    }

    _exit(0);
}

/* Merge all incoming edge streams into one stage's stdin. */
static void merge_process(
    int input_fds[],
    int input_count,
    int output_fd)
{
    signal(SIGPIPE, SIG_DFL);

    struct pollfd pfds[MAX_EDGES];
    char buf[BUF_SIZE];

    for (int i = 0; i < input_count; i++) {
        pfds[i].fd = input_fds[i];
        pfds[i].events = POLLIN | POLLHUP;
        pfds[i].revents = 0;
    }

    int remaining = input_count;

    while (remaining > 0) {
        int ready = poll(pfds, (nfds_t)input_count, -1);

        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }

        for (int i = 0; i < input_count; i++) {
            if (pfds[i].fd < 0 || pfds[i].revents == 0)
                continue;

            ssize_t n = read(pfds[i].fd, buf, sizeof(buf));

            if (n > 0) {
                if (write_all(output_fd, buf, (size_t)n) < 0) {
                    for (int j = 0; j < input_count; j++) {
                        if (pfds[j].fd >= 0)
                            close(pfds[j].fd);
                    }

                    close(output_fd);
                    _exit(1);
                }
            } else if (n == 0) {
                close(pfds[i].fd);
                pfds[i].fd = -1;
                remaining--;
            } else if (errno != EINTR) {
                close(pfds[i].fd);
                pfds[i].fd = -1;
                remaining--;
            }

            pfds[i].revents = 0;
        }
    }

    close(output_fd);
    _exit(0);
}

int dag_execute(PipelineDAG *dag)
{
    if (dag == NULL ||
        dag->pipeline.count < 1 ||
        dag->pipeline.count > MAX_STAGES ||
        dag->edge_count < 0 ||
        dag->edge_count > MAX_EDGES ||
        dag_validate(dag) != 0) {
        fprintf(stderr, "Invalid DAG\n");
        return -1;
    }

    int n = dag->pipeline.count;
    int m = dag->edge_count;

    int edge_pipes[MAX_EDGES][2];
    int stage_input[MAX_STAGES][2];
    int stage_output[MAX_STAGES][2];

    pid_t stage_pids[MAX_STAGES];
    pid_t router_pids[MAX_STAGES];
    pid_t merge_pids[MAX_STAGES];

    int incoming[MAX_STAGES] = {0};
    int outgoing[MAX_STAGES] = {0};

    for (int i = 0; i < MAX_EDGES; i++)
        edge_pipes[i][0] = edge_pipes[i][1] = -1;

    for (int i = 0; i < MAX_STAGES; i++) {
        stage_input[i][0] = stage_input[i][1] = -1;
        stage_output[i][0] = stage_output[i][1] = -1;

        stage_pids[i] = -1;
        router_pids[i] = -1;
        merge_pids[i] = -1;
    }

    for (int e = 0; e < m; e++) {
        int from = dag->edges[e].from;
        int to = dag->edges[e].to;

        if (from < 0 || from >= n || to < 0 || to >= n) {
            fprintf(stderr, "Invalid edge %d -> %d\n", from, to);
            return -1;
        }

        incoming[to]++;
        outgoing[from]++;
    }

    for (int e = 0; e < m; e++) {
        if (pipe(edge_pipes[e]) < 0) {
            perror("pipe edge");
            goto failure;
        }
    }

    for (int i = 0; i < n; i++) {
        if (pipe(stage_input[i]) < 0 ||
            pipe(stage_output[i]) < 0) {
            perror("pipe stage");
            goto failure;
        }
    }

    /* Launch the stage executables. */
    for (int i = 0; i < n; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork stage");
            goto failure;
        }

        if (pid == 0) {
            if (incoming[i] > 0) {
                if (dup2(stage_input[i][0], STDIN_FILENO) < 0)
                    _exit(126);
            } else {
                int nullfd = open("/dev/null", O_RDONLY);

                if (nullfd < 0 ||
                    dup2(nullfd, STDIN_FILENO) < 0)
                    _exit(126);

                if (nullfd != STDIN_FILENO)
                    close(nullfd);
            }

            if (outgoing[i] > 0) {
                if (dup2(stage_output[i][1], STDOUT_FILENO) < 0)
                    _exit(126);
            }

            close_all_pipes(
                edge_pipes, m,
                stage_input, stage_output, n);

            execvp(dag->pipeline.stages[i].argv[0],
                   dag->pipeline.stages[i].argv);

            perror("execvp DAG stage");
            _exit(127);
        }

        stage_pids[i] = pid;
    }

    /* Launch one router for each stage that has outgoing edges. */
    for (int node = 0; node < n; node++) {
        if (outgoing[node] == 0)
            continue;

        int fds[MAX_EDGES];
        int count = 0;

        for (int e = 0; e < m; e++) {
            if (dag->edges[e].from == node)
                fds[count++] = edge_pipes[e][1];
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork router");
            goto failure;
        }

        if (pid == 0) {
            /* Keep only this router's output pipe reader and edge writers. */
            for (int i = 0; i < n; i++) {
                if (stage_output[i][0] != stage_output[node][0])
                    close(stage_output[i][0]);

                if (stage_output[i][1] >= 0)
                    close(stage_output[i][1]);

                if (stage_input[i][0] >= 0)
                    close(stage_input[i][0]);

                if (stage_input[i][1] >= 0)
                    close(stage_input[i][1]);
            }

            for (int e = 0; e < m; e++) {
                close(edge_pipes[e][0]);

                if (dag->edges[e].from != node)
                    close(edge_pipes[e][1]);
            }

            router_process(stage_output[node][0], fds, count);
        }

        router_pids[node] = pid;
    }

    /* Launch one merge process for every stage with incoming edges. */
    for (int node = 0; node < n; node++) {
        if (incoming[node] == 0)
            continue;

        int fds[MAX_EDGES];
        int count = 0;

        for (int e = 0; e < m; e++) {
            if (dag->edges[e].to == node)
                fds[count++] = edge_pipes[e][0];
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork merge");
            goto failure;
        }

        if (pid == 0) {
            /* Keep only the incoming edge readers and target input writer. */
            for (int i = 0; i < n; i++) {
                if (stage_input[i][0] >= 0)
                    close(stage_input[i][0]);

                if (i != node && stage_input[i][1] >= 0)
                    close(stage_input[i][1]);

                if (stage_output[i][0] >= 0)
                    close(stage_output[i][0]);

                if (stage_output[i][1] >= 0)
                    close(stage_output[i][1]);
            }

            for (int e = 0; e < m; e++) {
                close(edge_pipes[e][1]);

                if (dag->edges[e].to != node)
                    close(edge_pipes[e][0]);
            }

            merge_process(fds, count, stage_input[node][1]);
        }

        merge_pids[node] = pid;
    }

    close_all_pipes(
        edge_pipes, m,
        stage_input, stage_output, n);

    int failed = 0;

    for (int i = 0; i < n; i++) {
        int status;

        if (waitpid(stage_pids[i], &status, 0) < 0 ||
            !WIFEXITED(status) ||
            WEXITSTATUS(status) != 0)
            failed = 1;
    }

    for (int i = 0; i < n; i++) {
        int status;

        if (router_pids[i] > 0) {
            if (waitpid(router_pids[i], &status, 0) < 0 ||
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0)
                failed = 1;
        }

        if (merge_pids[i] > 0) {
            if (waitpid(merge_pids[i], &status, 0) < 0 ||
                !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0)
                failed = 1;
        }
    }

    printf("DAG execution %s\n",
           failed ? "failed" : "completed");

    return failed ? -1 : 0;

failure:
    close_all_pipes(
        edge_pipes, m,
        stage_input, stage_output, n);

    for (int i = 0; i < n; i++) {
        if (stage_pids[i] > 0)
            kill(stage_pids[i], SIGTERM);

        if (router_pids[i] > 0)
            kill(router_pids[i], SIGTERM);

        if (merge_pids[i] > 0)
            kill(merge_pids[i], SIGTERM);
    }

    for (int i = 0; i < n; i++) {
        if (stage_pids[i] > 0)
            waitpid(stage_pids[i], NULL, 0);

        if (router_pids[i] > 0)
            waitpid(router_pids[i], NULL, 0);

        if (merge_pids[i] > 0)
            waitpid(merge_pids[i], NULL, 0);
    }

    return -1;
}

