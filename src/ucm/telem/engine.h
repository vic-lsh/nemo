#pragma once
#define _GNU_SOURCE

#include "config.h"
#include "util/compiler.h"
#include "util/timer.h"

typedef struct {
    /** measure time spent in reading PEBS samples */
    wall_and_cpu_time_t pebs;

    /** measure time spent in reading CXL counters */
    wall_and_cpu_time_t cxl;
} telem_poll_ctx_t;

int __no_discard telem_engine_init();
void telem_engine_poll(telem_poll_ctx_t* ctx);
