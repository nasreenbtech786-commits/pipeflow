#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "pipeflow_metrics.h"

int main(void)
{
    char line[1024];
    unsigned long long count = 0;
    PipeflowMetrics metrics;
    pf_metrics_start(&metrics);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        count++;
        pf_metrics_record(&metrics, 1);
    }

    if (ferror(stdin)) {
        perror("count: read");
        return 1;
    }

    if (printf("Records: %llu\n", count) < 0 || fflush(stdout) == EOF) {
        perror("count: write");
        return 1;
    }

    pf_metrics_report("count", &metrics);
    return 0;
}
