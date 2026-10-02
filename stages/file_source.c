#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: file_source <filename>\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "r");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), file) != NULL) {
        fputs(line, stdout);
    }

    fclose(file);

    return 0;
}

