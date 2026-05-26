#pragma once

#include "config.h"

#define MAX_PROCESSES 24

#define HEMEM_NCORES 56

#define MAX_COPY_THREADS 4

#define STATS_THREAD_CPU (0)
#define FAULT_THREAD_CPU (0)
#define LISTEN_THREAD_CPU (FAULT_THREAD_CPU)
#define REQUEST_THREAD_CPU (FAULT_THREAD_CPU + 1)
#define CORE_THREAD_CPU (REQUEST_THREAD_CPU + 1)

#ifdef CONFIG_DMA
#define LAST_HEMEM_THREAD (CORE_THREAD_CPU)
#else
#ifdef CONFIG_PAR_MEMCPY
#define PARALLEL_MIGRATE_THREAD_CPU (CORE_THREAD_CPU + 1)
#define LAST_HEMEM_THREAD (CORE_THREAD_CPU + MAX_COPY_THREADS)
#else
#define LAST_HEMEM_THREAD (CORE_THREAD_CPU)
#endif
#endif

#define MAX_EPOLL_EVENTS 128

enum pbuftype {
    FASTMEM = 0,
    SLOWMEM = 1,
    //    WRITE = 2,
    NPBUFTYPES
};

enum HOTNESS {
    COLD,
    HOT1,
    HOT2,
    HOT3,
    HOT4,
    HOT5,
    HOT6,
    HOT7,
    HOT8,
    HOT9,
    HOT10,
    HOT11,
    // HOT12,
    // HOT13,
    // HOT14,
    // HOT15,
    NUM_HOTNESS_LEVELS
};
