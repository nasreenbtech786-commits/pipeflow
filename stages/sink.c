#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include "pipeflow_metrics.h"

int main(void)
{
    char line[256];
    PipeflowMetrics metrics;
    pf_metrics_start(&metrics);

    while (fgets(line, sizeof(line), stdin) != NULL) {
        if (printf("SINK: %s", line) < 0) {
            perror("sink: write");
            return 1;
        }
        pf_metrics_record(&metrics, 1);
    }

    if (ferror(stdin)) {
        perror("sink: read");
        return 1;
    }

    if (fflush(stdout) == EOF) {
        perror("sink: flush");
        return 1;
    }

    pf_metrics_report("sink", &metrics);
    return 0;
}
