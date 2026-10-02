#include <stdio.h>

int main(void)
{
    char line[1024];
    int count = 0;

    while (fgets(line, sizeof(line), stdin) != NULL) {
        count++;
    }

    printf("Records: %d\n", count);

    return 0;
}

