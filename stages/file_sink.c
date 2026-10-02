#include <stdio.h>

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: file_sink <filename>\n");
        return 1;
    }

    FILE *file = fopen(argv[1], "w");

    if (file == NULL) {
        perror("fopen");
        return 1;
    }

    char line[1024];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        fputs(line, file);
    }

    fclose(file);

    return 0;
}

