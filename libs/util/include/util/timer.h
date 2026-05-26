#ifndef HEMEM_TIMER_H
#define HEMEM_TIMER_H

#include <stdint.h>
#include <sys/time.h>
#include <time.h>

/* Returns the number of seconds encoded in T, a "struct timeval". */
#define tv_to_double(t) (t.tv_sec + (t.tv_usec / 1000000.0))

// Struct to hold both CPU and wall time with explicit unit suffixes
typedef struct {
    double wall_time_us;  // Wall time in microseconds
    double cpu_time_ns;   // CPU time in nanoseconds
} wall_and_cpu_time_t;

#define TIME_OP(timing_info, operation)                          \
    do {                                                         \
        struct timespec start_cpu, end_cpu;                      \
        struct timeval start_wall, end_wall;                     \
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start_cpu);      \
        gettimeofday(&start_wall, NULL);                         \
        { operation; }                                           \
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end_cpu);        \
        gettimeofday(&end_wall, NULL);                           \
        (timing_info).wall_time_us =                             \
            (end_wall.tv_sec - start_wall.tv_sec) * 1000000.0 +  \
            (end_wall.tv_usec - start_wall.tv_usec);             \
        (timing_info).cpu_time_ns =                              \
            (end_cpu.tv_sec - start_cpu.tv_sec) * 1000000000.0 + \
            (end_cpu.tv_nsec - start_cpu.tv_nsec);               \
    } while (0)

// convert nanoseconds to milliseconds
double ns_to_ms(double ns);

// convert microseconds to milliseconds
double us_to_ms(double us);

static inline uint64_t rdtscp(void) {
    uint32_t eax, edx;
    // why is "ecx" in clobber list here, anyway? -SG&MH,2017-10-05
    __asm volatile("rdtscp" : "=a"(eax), "=d"(edx)::"ecx", "memory");
    return ((uint64_t)edx << 32) | eax;
}

void timeDiff(struct timeval *d, struct timeval *a, struct timeval *b);
double elapsed_secs(struct timeval *starttime, struct timeval *endtime);
double elapsed_ms(struct timeval *starttime, struct timeval *endtime);
uint64_t ts_elapsed_ns(struct timespec *start, struct timespec *end);
double ts_elapsed_ms(struct timespec *start, struct timespec *end);

#endif /* HEMEM_TIMER_H */
