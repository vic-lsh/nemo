#include "util/timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// convert nanoseconds to milliseconds
inline double ns_to_ms(double ns) { return ns / 1000000.0; }

// convert microseconds to milliseconds
inline double us_to_ms(double us) { return us / 1000.0; }

/* Useful for doing arithmetic on struct timevals. M*/
void timeDiff(struct timeval *d, struct timeval *a, struct timeval *b) {
    d->tv_sec = a->tv_sec - b->tv_sec;
    d->tv_usec = a->tv_usec - b->tv_usec;
    if (d->tv_usec < 0) {
        d->tv_sec -= 1;
        d->tv_usec += 1000000;
    }
}

/* Return the no. of elapsed seconds between Starttime and Endtime. */
double elapsed_secs(struct timeval *starttime, struct timeval *endtime) {
    struct timeval diff;

    timeDiff(&diff, endtime, starttime);
    return tv_to_double(diff);
}

/* Return the no. of elapsed milliseconds between Starttime and Endtime. */
double elapsed_ms(struct timeval *starttime, struct timeval *endtime) {
    struct timeval diff;

    timeDiff(&diff, endtime, starttime);
    return (diff.tv_sec * 1000) + (diff.tv_usec / 1000.0);
}

uint64_t ts_elapsed_ns(struct timespec *start, struct timespec *end) {
    uint64_t seconds = end->tv_sec - start->tv_sec;
    uint64_t nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds * 1000000000 + nanoseconds;
}

double ts_elapsed_ms(struct timespec *start, struct timespec *end) {
    return ts_elapsed_ns(start, end) / 1000000.0;
}
