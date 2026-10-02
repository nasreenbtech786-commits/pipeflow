#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>

int main(void)
{
    char line[128];

    /*
     * Ignore SIGPIPE so the process can detect EPIPE
     * instead of being terminated by the signal.
     */
    signal(SIGPIPE, SIG_IGN);

    for (int i = 1; i <= 100000; i++) {

        int n = snprintf(
            line,
            sizeof(line),
            "record-%d\n",
            i
        );

        if (write(STDOUT_FILENO, line, n) == -1) {

            if (errno == EPIPE) {
                fprintf(stderr,
                        "fast_source: downstream closed pipe\n");
                return 0;
            }

            perror("write");
            return 1;
        }
    }

    return 0;
}

