#ifndef PIPEFLOW_METRICS_H
#define PIPEFLOW_METRICS_H

#include <time.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    struct timespec start;
    struct timespec end;
    uint64_t records;
} PipeflowMetrics;

static inline void pf_metrics_start(PipeflowMetrics *m)
{
    m->records = 0;
    clock_gettime(CLOCK_MONOTONIC, &m->start);
}

static inline void pf_metrics_record(PipeflowMetrics *m, uint64_t count)
{
    m->records += count;
}

static inline void pf_metrics_report(
    const char *stage, const PipeflowMetrics *m)
{
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed =
        (end.tv_sec - m->start.tv_sec) +
        (end.tv_nsec - m->start.tv_nsec) / 1000000000.0;

    if (elapsed <= 0.0)
        elapsed = 0.000001;

    fprintf(stderr,
        "[metrics] stage=%s records=%llu elapsed=%.6f s throughput=%.2f records/sec\n",
        stage,
        (unsigned long long)m->records,
        elapsed,
        m->records / elapsed);
}

#endif
