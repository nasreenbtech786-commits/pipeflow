#include <stdio.h>

int main(void)
{
    char line[1024];

    if (fgets(line, sizeof(line), stdin) != NULL) {
        fputs(line, stdout);
        fflush(stdout);
    }

    return 0;
}

