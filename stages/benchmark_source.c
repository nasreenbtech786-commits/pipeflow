#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <time.h>

int main(void)
{
    struct timespec start, end;
    char record[64];

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 1; i <= 1000; i++) {
        snprintf(record, sizeof(record), "record-%d\n", i);

        if (fputs(record, stdout) == EOF) {
            perror("benchmark_source: write");
            return 1;
        }
    }

    if (fflush(stdout) == EOF) {
        perror("benchmark_source: flush");
        return 1;
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed =
        (end.tv_sec - start.tv_sec) +
        (end.tv_nsec - start.tv_nsec) / 1000000000.0;

    if (elapsed <= 0.0)
        elapsed = 0.000001;

    fprintf(stderr,
            "[metrics] stage=benchmark_source records=1000 elapsed=%.6f s throughput=%.2f records/sec\n",
            elapsed, 1000.0 / elapsed);

    return 0;
}
