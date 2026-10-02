#include <stdio.h>
#include <unistd.h>

int main(void)
{
    char line[1024];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        fputs(line, stdout);
        usleep(1000);
    }

    return 0;
}

