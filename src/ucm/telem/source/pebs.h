#pragma once

#include <asm/unistd.h>
#include <linux/perf_event.h>
#include <linux/types.h>
#include <stddef.h>

#define PEBS_NUM_PERF_PAGES (1 + (1 << 15))  // Has to be == 1+2^n, here 1MB
#define PEBS_SAMPLE_PERIOD (1UL << 14)
// #define PEBS_SAMPLE_PERIOD (1UL << 10)
// #define PEBS_SAMPLE_PERIOD (1UL << 13)
// #define PEBS_SAMPLE_PERIOD (1)

//#define PEBS_SAMPLE_IP
#define PEBS_SAMPLE_TID
#define PEBS_SAMPLE_ADDR
#define PEBS_SAMPLE_PHYS_ADDR
#define PEBS_SAMPLE_TIME

// Docs: https://man7.org/linux/man-pages/man2/perf_event_open.2.html
struct perf_sample {
    struct perf_event_header header;
#ifdef PEBS_SAMPLE_IP
    __u64 ip;
#endif
#ifdef PEBS_SAMPLE_TID
    __u32 pid, tid;
#endif
#ifdef PEBS_SAMPLE_TIME
    __u64 time;
#endif
#ifdef PEBS_SAMPLE_ADDR
    __u64 addr;
#endif
#ifdef PEBS_SAMPLE_PHYS_ADDR
    __u64 phys_addr;
#endif
};

typedef void (*pebs_sample_handler_t)(struct perf_event_header* ph,
                                      size_t core_idx);

void pebs_init();
void pebs_shutdown();
void pebs_shutdown_signal_handler(int arg);
void pebs_epoch_scan(pebs_sample_handler_t handler);
