#include <stdio.h>

int main(void)
{
    char line[256];

    while (fgets(line, sizeof(line), stdin)) {
        printf("SINK: %s", line);
    }

    return 0;
}

