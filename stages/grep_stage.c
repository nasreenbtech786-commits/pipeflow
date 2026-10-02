#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: grep_stage <pattern>\n");
        return 1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (strstr(line, argv[1]) != NULL) {
            fputs(line, stdout);
        }
    }

    return 0;
}

