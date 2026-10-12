#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "pipeflow_metrics.h"

int main(void)
{
    char line[256];
    PipeflowMetrics metrics;
    pf_metrics_start(&metrics);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (printf("TRANSFORMED: %s", line) < 0) {
            perror("transform: write");
            return 1;
        }
        pf_metrics_record(&metrics, 1);
    }

    if (ferror(stdin)) {
        perror("transform: read");
        return 1;
    }

    if (fflush(stdout) == EOF) {
        perror("transform: flush");
        return 1;
    }

    pf_metrics_report("transform", &metrics);
    return 0;
}
