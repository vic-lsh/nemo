#pragma once

#include "telem/source/pebs.h"
#include "util/type-assert.h"

void on_pebs_sample_received(struct perf_event_header* ph, size_t core_idx);
ASSERT_SIGNATURE(&on_pebs_sample_received, pebs_sample_handler_t);

void pebs_handler_init();
