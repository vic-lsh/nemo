#pragma once

#include <stddef.h>

#include "cxl-dev/cxl-dev.h"

typedef void (*cxl_counter_update_handler_t)(size_t devdax_offset,
                                             access_counter_t prev_cnt,
                                             access_counter_t curr_cnt);

int cxl_init();
void cxl_shutdown();
void cxl_epoch_scan(cxl_counter_update_handler_t handler);

access_counter_t cxl_count_get(size_t counter_idx);

// Fetch the latest count for the provided counter index from the FPGA.
// Update the counter stored in this module.
access_counter_t cxl_count_refresh(size_t counter_idx);
