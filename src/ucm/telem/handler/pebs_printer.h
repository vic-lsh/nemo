#pragma once

#include "telem/source/pebs.h"
#include "util/type-assert.h"

void log_pebs_sample(struct perf_event_header* ph, size_t core_idx);
ASSERT_SIGNATURE(&log_pebs_sample, pebs_sample_handler_t);

void pebs_printer_handler_init(char* log_filename);
