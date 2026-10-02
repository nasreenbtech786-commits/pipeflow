#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(void)
{
    int fd[2];

    if (pipe(fd) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        close(fd[1]);

        char buffer[100];
        ssize_t n = read(fd[0], buffer, sizeof(buffer) - 1);

        if (n < 0) {
            perror("read");
            close(fd[0]);
            return 1;
        }

        buffer[n] = '\0';

        printf("Child received: %s\n", buffer);

        close(fd[0]);
        return 0;
    }

    close(fd[0]);

    const char *message = "Hello from parent";

    if (write(fd[1], message, strlen(message)) == -1) {
        perror("write");
        close(fd[1]);
        waitpid(pid, NULL, 0);
        return 1;
    }

    close(fd[1]);

    waitpid(pid, NULL, 0);

    return 0;
}

