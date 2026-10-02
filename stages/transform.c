#include <stdio.h>

int main(void)
{
    char line[256];

    while (fgets(line, sizeof(line), stdin)) {
        printf("TRANSFORMED: %s", line);
    }

    return 0;
}

