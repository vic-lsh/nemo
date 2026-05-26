#ifndef UTIL_THREAD_H
#define UTIL_THREAD_H

#define _GNU_SOURCE
#include <pthread.h>
#include <stddef.h>

void thread_pin_self(size_t cpu_core);

#endif /* End of UTIL_THREAD_H */
