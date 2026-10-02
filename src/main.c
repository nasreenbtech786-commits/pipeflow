#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "parser.h"

#define MAX_PIPES (MAX_STAGES - 1)

int main(int argc, char *argv[])
{
    /*
     * Allow execvp() to find PipeFlow stages.
     */
    setenv(
        "PATH",
        "./stages:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
        1
    );

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config-file>\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    char line[1024];

    if (fgets(line, sizeof(line), file) == NULL) {
        fprintf(stderr, "Configuration file is empty\n");
        fclose(file);
        return 1;
    }

    fclose(file);

    Pipeline pipeline;

    if (parse_pipeline(line, &pipeline) <= 0) {
        fprintf(stderr, "Failed to parse pipeline\n");
        return 1;
    }

    printf("PipeFlow\n");
    printf("========\n\n");

    printf("Configuration: %s", line);
    printf("Stages: %d\n\n", pipeline.count);

    for (int i = 0; i < pipeline.count; i++) {
        printf("Stage %d:", i);

        for (int j = 0; j < pipeline.stages[i].argc; j++) {
            printf(" %s", pipeline.stages[i].argv[j]);
        }

        printf("\n");
    }

    printf("\nStarting pipeline...\n\n");

    int pipes[MAX_PIPES][2];
    pid_t pids[MAX_STAGES];

    /*
     * Create pipes.
     */
    for (int i = 0; i < pipeline.count - 1; i++) {

        if (pipe(pipes[i]) == -1) {
            perror("pipe");

            free_pipeline(&pipeline);

            return 1;
        }
    }

    /*
     * Create one process for each pipeline stage.
     */
    for (int i = 0; i < pipeline.count; i++) {

        pids[i] = fork();

        if (pids[i] == -1) {
            perror("fork");

            for (int j = 0; j < pipeline.count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /*
             * If some stages already started, terminate them.
             */
            for (int j = 0; j < i; j++) {
                kill(pids[j], SIGTERM);
            }

            for (int j = 0; j < i; j++) {
                waitpid(pids[j], NULL, 0);
            }

            free_pipeline(&pipeline);

            return 1;
        }

        /*
         * CHILD
         */
        if (pids[i] == 0) {

            /*
             * Put every stage into the same process group.
             *
             * First stage becomes the process-group leader.
             */
            if (i == 0) {

                if (setpgid(0, 0) == -1) {
                    perror("setpgid");
                    exit(1);
                }

            } else {

                if (setpgid(0, pids[0]) == -1) {
                    perror("setpgid");
                    exit(1);
                }
            }

            /*
             * Connect input pipe.
             */
            if (i > 0) {

                if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2 stdin");
                    exit(1);
                }
            }

            /*
             * Connect output pipe.
             */
            if (i < pipeline.count - 1) {

                if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2 stdout");
                    exit(1);
                }
            }

            /*
             * Close all original pipe descriptors.
             */
            for (int j = 0; j < pipeline.count - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            /*
             * Execute stage.
             */
            execvp(
                pipeline.stages[i].argv[0],
                pipeline.stages[i].argv
            );

            perror("execvp");
            exit(127);
        }

        /*
         * PARENT
         *
         * Put the child into the same process group.
         *
         * Doing this in the parent as well avoids a race where
         * the child may execute before the parent calls setpgid().
         */
        if (i == 0) {

            if (setpgid(pids[i], pids[i]) == -1) {

                if (errno != EACCES && errno != ESRCH) {
                    perror("setpgid");
                }
            }

        } else {

            if (setpgid(pids[i], pids[0]) == -1) {

                if (errno != EACCES && errno != ESRCH) {
                    perror("setpgid");
                }
            }
        }

        printf(
            "Stage %d: %s  PID=%d\n",
            i,
            pipeline.stages[i].argv[0],
            pids[i]
        );
    }

    /*
     * Parent closes all pipe descriptors.
     */
    for (int i = 0; i < pipeline.count - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    /*
     * The first stage is the process-group leader.
     */
    pid_t process_group = pids[0];

    printf(
        "\nPipeline process group: PGID=%d\n\n",
        process_group
    );

    /*
     * Wait for all stages.
     */
    int failed = 0;

    for (int i = 0; i < pipeline.count; i++) {

        int status;

        if (waitpid(pids[i], &status, 0) == -1) {

            perror("waitpid");

            failed = 1;

            continue;
        }

        if (WIFEXITED(status)) {

            int code = WEXITSTATUS(status);

            printf(
                "Stage %d (%s) exited with status %d\n",
                i,
                pipeline.stages[i].argv[0],
                code
            );

            if (code != 0) {
                failed = 1;
            }

        } else if (WIFSIGNALED(status)) {

            printf(
                "Stage %d (%s) terminated by signal %d\n",
                i,
                pipeline.stages[i].argv[0],
                WTERMSIG(status)
            );

            failed = 1;
        }
    }

    free_pipeline(&pipeline);

    if (failed) {
        printf("\nPipeline completed with errors.\n");
        return 1;
    }

    printf("\nPipeline completed successfully.\n");

    return 0;
}

