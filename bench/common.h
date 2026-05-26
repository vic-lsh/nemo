#ifndef BENCH_COMMON_H

#include <stddef.h>
#include <stdint.h>

/**
 * Standard subroutine to run after benchmark measurements complete.
 *
 * Provide a name for the benchmark, as well as the measurements (in ns).
 */
void postbench(const char *bench_name, uint64_t *measurements, size_t len);

uint64_t get_microseconds();
uint64_t get_ns();
int uint64_t_compare(const void *a, const void *b);

#endif
