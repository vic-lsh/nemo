#pragma once

#include "telem/source/cxl.h"
#include "util/type-assert.h"

void on_cxl_counter_update(size_t cxl_devdax_offset, access_counter_t prev_cnt,
                           access_counter_t curr_cnt);

ASSERT_SIGNATURE(&on_cxl_counter_update, cxl_counter_update_handler_t);
