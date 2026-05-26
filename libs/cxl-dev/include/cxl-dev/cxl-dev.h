#ifndef CXL_DEV_H
#define CXL_DEV_H

#include <stddef.h>
#include <stdint.h>

#include "util/shared.h"

#define CXL_MEM_START_ADDR 0x4080000000

//#define CXL_CAPACITY (16 * (1024L * 1024L * 1024L))
#define CXL_CAPACITY (32 * (1024L * 1024L * 1024L))

/**
 * Defines the width of the access counter exposed from the FPGA.
 */
typedef uint64_t access_counter_t;

#define SRAM_SIZE (KB(64))
//#define SRAM_SIZE (KB(32))
#define N_COUNTERS (SRAM_SIZE / sizeof(uint64_t))
#define SRAM_LOG_DEPTH (13)  // log2 of N_COUNTERS, too lazy to put it in code

int cxl_dev_init();
access_counter_t cxl_dev_read_counter_diff(size_t counter_index);
void cxl_dev_update_all_counters(uint64_t* counters);

#endif /* CXL_DEV_H */
