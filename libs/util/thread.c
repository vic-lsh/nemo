#include "util/thread.h"

#include <assert.h>
#include <stdio.h>

void thread_pin_self(size_t cpu_core) {
    int s;
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_core, &cpuset);
    s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        perror("thread_pin_self");
        assert(0);
    }
}
