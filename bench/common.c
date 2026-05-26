#include "common.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

inline uint64_t get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000LL) + (ts.tv_nsec / 1000LL);
}

inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int uint64_t_compare(const void *a, const void *b) {
    uint64_t ua = *(const uint64_t *)a;
    uint64_t ub = *(const uint64_t *)b;
    return (ua > ub) - (ua < ub);
}

void postbench_processing(uint64_t *measurements, size_t len, uint64_t *p_min,
                          uint64_t *p_max, double *p_mean, double *p_stddev) {
    uint64_t sum = 0;
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;

    for (size_t i = 0; i < len; i++) {
        sum += measurements[i];
        if (measurements[i] < min) min = measurements[i];
        if (measurements[i] > max) max = measurements[i];
    }

    double mean = (double)sum / len;

    // Calculate standard deviation
    double sum_sq_diff = 0;
    for (size_t i = 0; i < len; i++) {
        double diff = measurements[i] - mean;
        sum_sq_diff += diff * diff;
    }
    double stddev = sqrt(sum_sq_diff / len);

    qsort(measurements, len, sizeof(uint64_t),
          (int (*)(const void *, const void *))(__compar_fn_t)uint64_t_compare);

    if (p_min) *p_min = min;
    if (p_max) *p_max = max;
    if (p_mean) *p_mean = mean;
    if (p_stddev) *p_stddev = stddev;
}

void postbench(const char *bench_name, uint64_t *measurements, size_t len) {
    uint64_t min = UINT64_MAX;
    uint64_t max = 0;
    double mean = 0;
    double stddev = 0;

    postbench_processing(measurements, len, &min, &max, &mean, &stddev);

    printf("Benchmark %s\n", bench_name);
    printf("Results over %ld iterations:\n", len);
    printf("  Mean: %.2f μs\n", mean / 1000.0);
    printf("  Stddev: %.2f μs\n", stddev / 1000.0);
    printf("  Min: %.2f μs\n", min / 1000.0);
    printf("  Max: %.2f μs\n", max / 1000.0);
    printf("  P50: %.2f μs\n", measurements[len / 2] / 1000.0);
    printf("  P95: %.2f μs\n", measurements[(int)(len * 0.95)] / 1000.0);
    printf("  P99: %.2f μs\n", measurements[(int)(len * 0.99)] / 1000.0);
}
