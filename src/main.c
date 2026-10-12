#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "parser.h"
#include "dag.h"
#include "dag_runtime.h"

#define MAX_PIPES (MAX_STAGES - 1)
#define CONFIG_LINE_SIZE 1024

/*
 * Configure PATH so executables inside ./stages can be found.
 */
static void configure_path(void)
{
    if (setenv(
            "PATH",
            "./stages:/usr/local/sbin:/usr/local/bin:"
            "/usr/sbin:/usr/bin:/sbin:/bin",
            1) == -1) {
        perror("setenv");
        exit(EXIT_FAILURE);
    }
}

/*
 * Close all initialized pipes.
 */
static void close_pipes(int pipes[][2], int count)
{
    for (int i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

/*
 * Execute a traditional linear pipeline.
 *
 * Example:
 * file_source tests/input.txt | transform | count
 */
static int execute_linear_pipeline(Pipeline *pipeline)
{
    if (pipeline == NULL || pipeline->count <= 0) {
        fprintf(stderr, "Error: empty pipeline.\n");
        return 1;
    }

    if (pipeline->count > MAX_STAGES) {
        fprintf(stderr, "Error: too many pipeline stages.\n");
        return 1;
    }

    int pipes[MAX_PIPES][2];
    pid_t pids[MAX_STAGES];

    int pipe_count = pipeline->count - 1;
    int started = 0;

    /*
     * Create pipes.
     */
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");

            close_pipes(pipes, i);
            free_pipeline(pipeline);

            return 1;
        }
    }

    printf("Starting pipeline...\n");
    fflush(stdout);

    /*
     * Create one process for each pipeline stage.
     */
    for (int i = 0; i < pipeline->count; i++) {

        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");

            /*
             * Close parent copies of the pipes.
             */
            close_pipes(pipes, pipe_count);

            /*
             * Stop processes already created.
             */
            for (int j = 0; j < started; j++) {
                kill(pids[j], SIGTERM);
            }

            /*
             * Reap all started children.
             */
            for (int j = 0; j < started; j++) {
                while (waitpid(pids[j], NULL, 0) == -1 &&
                       errno == EINTR) {
                    /* Retry interrupted wait. */
                }
            }

            free_pipeline(pipeline);
            return 1;
        }

        /*
         * Child process.
         */
        if (pid == 0) {

            /*
             * Restore default SIGPIPE handling.
             */
            signal(SIGPIPE, SIG_DFL);

            /*
             * Connect previous stage to standard input.
             */
            if (i > 0) {
                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    _exit(126);
                }
            }

            /*
             * Connect standard output to the next stage.
             */
            if (i < pipeline->count - 1) {
                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    _exit(126);
                }
            }

            /*
             * Close all inherited pipe descriptors.
             */
            close_pipes(pipes, pipe_count);

            /*
             * Execute the requested stage.
             */
            execvp(
                pipeline->stages[i].argv[0],
                pipeline->stages[i].argv
            );

            perror(pipeline->stages[i].argv[0]);
            _exit(127);
        }

        /*
         * Parent process.
         */
        pids[i] = pid;
        started++;

        printf(
            "Stage %d (%s) started with PID=%ld\n",
            i,
            pipeline->stages[i].argv[0],
            (long)pid
        );
    }

    /*
     * Parent must close every pipe descriptor.
     * This allows EOF and SIGPIPE to propagate correctly.
     */
    close_pipes(pipes, pipe_count);

    /*
     * Wait for all pipeline stages.
     */
    int overall_status = 0;

    for (int i = 0; i < started; i++) {

        int status;

        pid_t result;

        do {
            result = waitpid(pids[i], &status, 0);
        } while (result == -1 && errno == EINTR);

        if (result == -1) {
            perror("waitpid");
            overall_status = 1;
            continue;
        }

        if (WIFEXITED(status)) {

            int exit_code = WEXITSTATUS(status);

            printf(
                "Stage %d exited with status %d\n",
                i,
                exit_code
            );

            if (exit_code != 0) {
                overall_status = 1;
            }

        } else if (WIFSIGNALED(status)) {

            int signal_number = WTERMSIG(status);

            if (signal_number == SIGPIPE) {
                printf(
                    "Stage %d received SIGPIPE: "
                    "downstream closed the pipe.\n",
                    i
                );
            } else {
                printf(
                    "Stage %d terminated by signal %d\n",
                    i,
                    signal_number
                );
            }

            /*
             * SIGPIPE is expected when a downstream stage
             * intentionally stops reading. Other signals
             * indicate abnormal termination.
             */
            if (signal_number != SIGPIPE) {
                overall_status = 1;
            }
        }
    }

    free_pipeline(pipeline);

    if (overall_status == 0) {
        printf("Pipeline completed successfully.\n");
    } else {
        printf("Pipeline completed with errors.\n");
    }

    return overall_status;
}

/*
 * Read and execute a DAG configuration.
 *
 * Supported format:
 *
 * DAG
 * stage file_source tests/input.txt
 * stage count
 * stage count
 * edge 0 1
 * edge 0 2
 *
 * All stage declarations must precede edge declarations.
 */
static int execute_dag_config(FILE *file)
{
    PipelineDAG dag;
    dag_init(&dag);

    char line[CONFIG_LINE_SIZE];

    int line_number = 0;
    int edge_from[MAX_EDGES];
    int edge_to[MAX_EDGES];
    int pending_edges = 0;

    printf("\nPipeFlow DAG Runtime\n");
    printf("====================\n");

    while (fgets(line, sizeof(line), file) != NULL) {

        line_number++;

        /*
         * Remove leading whitespace.
         */
        char *text = line;

        while (*text == ' ' || *text == '\t') {
            text++;
        }

        /*
         * Ignore blank lines and comments.
         */
        if (*text == '\0' ||
            *text == '\n' ||
            *text == '#' ||
            *text == '\r') {
            continue;
        }

        /*
         * Ignore the DAG header.
         */
        if (strncmp(text, "DAG", 3) == 0 &&
            (text[3] == '\0' ||
             text[3] == '\n' ||
             text[3] == '\r' ||
             text[3] == ' ' ||
             text[3] == '\t')) {
            continue;
        }

        /*
         * Parse a stage declaration.
         */
        if (strncmp(text, "stage ", 6) == 0 ||
            strncmp(text, "stage\t", 6) == 0) {

            if (dag.pipeline.count >= MAX_STAGES) {
                fprintf(
                    stderr,
                    "Error at line %d: maximum stage count exceeded.\n",
                    line_number
                );

                free_pipeline(&dag.pipeline);
                return 1;
            }

            char *command = text + 5;

            while (*command == ' ' || *command == '\t') {
                command++;
            }

            Pipeline parsed;

            int count = parse_pipeline(command, &parsed);

            if (count <= 0 || parsed.count != 1) {
                fprintf(
                    stderr,
                    "Error at line %d: invalid stage command.\n",
                    line_number
                );

                if (count > 0) {
                    free_pipeline(&parsed);
                }

                free_pipeline(&dag.pipeline);
                return 1;
            }

            /*
             * Transfer ownership of the parsed stage
             * into the DAG.
             */
            int index = dag.pipeline.count;

            dag.pipeline.stages[index] = parsed.stages[0];
            dag.pipeline.count++;

            /*
             * Prevent the temporary pipeline from freeing
             * the transferred argument strings.
             */
            parsed.count = 0;

            printf(
                "Registered stage %d: %s\n",
                index,
                dag.pipeline.stages[index].argv[0]
            );

            continue;
        }

        /*
         * Parse an edge declaration.
         */
        if (strncmp(text, "edge ", 5) == 0 ||
            strncmp(text, "edge\t", 5) == 0) {

            int from;
            int to;
            char extra;

            if (pending_edges >= MAX_EDGES ||
                sscanf(
                    text + 4,
                    "%d %d %c",
                    &from,
                    &to,
                    &extra
                ) != 2) {

                fprintf(
                    stderr,
                    "Error at line %d: invalid edge declaration.\n",
                    line_number
                );

                free_pipeline(&dag.pipeline);
                return 1;
            }

            edge_from[pending_edges] = from;
            edge_to[pending_edges] = to;
            pending_edges++;

            continue;
        }

        fprintf(
            stderr,
            "Error at line %d: unknown configuration entry: %s",
            line_number,
            text
        );

        free_pipeline(&dag.pipeline);
        return 1;
    }

    if (ferror(file)) {
        perror("Reading DAG configuration");
        free_pipeline(&dag.pipeline);
        return 1;
    }

    /*
     * Add edges after all stages have been registered.
     */
    for (int i = 0; i < pending_edges; i++) {

        if (dag_add_edge(
                &dag,
                edge_from[i],
                edge_to[i]
            ) != 0) {

            fprintf(
                stderr,
                "Error: could not add edge %d -> %d.\n",
                edge_from[i],
                edge_to[i]
            );

            free_pipeline(&dag.pipeline);
            return 1;
        }
    }

    /*
     * Validate the graph before starting processes.
     */
    if (dag_validate(&dag) != 0) {
        fprintf(
            stderr,
            "Error: DAG validation failed. "
            "Check stage indices and cycles.\n"
        );

        free_pipeline(&dag.pipeline);
        return 1;
    }

    printf("Stages: %d\n", dag.pipeline.count);
    printf("Edges: %d\n", dag.edge_count);
    printf("Starting DAG execution...\n");

    /*
     * Execute using the existing DAG runtime.
     */
    int result = dag_execute(&dag);

    free_pipeline(&dag.pipeline);

    if (result == 0) {
        printf("DAG execution completed successfully.\n");
        return 0;
    }

    fprintf(stderr, "DAG execution failed.\n");
    return 1;
}

/*
 * Main entry point.
 */
int main(int argc, char *argv[])
{
    configure_path();

    if (argc != 2) {
        fprintf(
            stderr,
            "Usage: %s <configuration-file>\n",
            argv[0]
        );

        fprintf(
            stderr,
            "\nExamples:\n"
            "  %s configs/basic.conf\n"
            "  %s configs/fanout.conf\n",
            argv[0],
            argv[0]
        );

        return EXIT_FAILURE;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        perror("Opening configuration file");
        return EXIT_FAILURE;
    }

    char line[CONFIG_LINE_SIZE];

    if (fgets(line, sizeof(line), file) == NULL) {

        if (ferror(file)) {
            perror("Reading configuration file");
        } else {
            fprintf(stderr, "Error: configuration file is empty.\n");
        }

        fclose(file);
        return EXIT_FAILURE;
    }

    /*
     * Detect DAG configuration.
     * Leading whitespace is allowed.
     */
    char *first = line;

    while (*first == ' ' || *first == '\t') {
        first++;
    }

    if (strncmp(first, "DAG", 3) == 0 &&
        (first[3] == '\0' ||
         first[3] == '\n' ||
         first[3] == '\r' ||
         first[3] == ' ' ||
         first[3] == '\t')) {

        rewind(file);

        int result = execute_dag_config(file);

        fclose(file);

        return result;
    }

    /*
     * Otherwise, execute a traditional linear pipeline.
     */
    fclose(file);

    Pipeline pipeline;

    int parsed_count = parse_pipeline(line, &pipeline);

    if (parsed_count <= 0) {
        fprintf(stderr, "Error: invalid pipeline configuration.\n");
        return EXIT_FAILURE;
    }

    printf("\nPipeFlow\n");
    printf("========\n");
    printf("Configuration: %s", line);
    printf("Stages: %d\n", pipeline.count);

    for (int i = 0; i < pipeline.count; i++) {

        printf("Stage %d:", i);

        for (int j = 0;
             j < pipeline.stages[i].argc;
             j++) {

            printf(
                " %s",
                pipeline.stages[i].argv[j]
            );
        }

        printf("\n");
    }

    return execute_linear_pipeline(&pipeline);
}

