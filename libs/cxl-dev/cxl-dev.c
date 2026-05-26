#include "cxl-dev/cxl-dev.h"

#include <assert.h>
#include <emmintrin.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "util/log.h"

// The FPGA's BAR resource path
#define RESOURCE_PATH "/sys/bus/pci/devices/0000:40:00.1/resource2"
// The FPGA's control regiser BAR size
#define BYTES_TO_READ (MB(2))
//#define DDR_SIZE (GB(16))
//#define END_ADDR (CXL_MEM_START_ADDR + DDR_SIZE)

#define N_PIPELINES (3lu)
#define DATAPATHS_PER_PIPELINE (2lu)

// Public API
typedef struct fpga_t fpga_t;
void set_added_stall(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val);
void set_filter_start(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val);
void set_filter_end(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val);
void set_map_shift(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val);
void set_sample_bitmask(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val);
void set_post_translation_shift(fpga_t *fpga, uint64_t pipeline_idx,
                                uint64_t val);
void set_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                           uint64_t sram_addr_from, uint64_t sram_addr_to);
void clear_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                             uint64_t sram_addr_from);  // invalidates entry

typedef struct {
    char overflow[4];
} overflow_t;
overflow_t get_overflows(fpga_t *fpga);
uint64_t get_added_stall(fpga_t *fpga, uint64_t pipeline_idx);
uint64_t get_filter_start(fpga_t *fpga, uint64_t pipeline_idx);
uint64_t get_filter_end(fpga_t *fpga, uint64_t pipeline_idx);
uint64_t get_map_shift(fpga_t *fpga, uint64_t pipeline_idx);
uint64_t get_sample_bitmask(fpga_t *fpga, uint64_t pipeline_idx);
uint64_t get_post_translation_shift(fpga_t *fpga, uint64_t pipeline_idx);
typedef struct {
    bool valid;
    uint64_t sram_addr_to;
} translation_result_t;
translation_result_t get_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                                           uint64_t sram_addr_from);
uint64_t get_dp_state(
    fpga_t *fpga, uint64_t pipeline_idx, uint64_t datapath_idx,
    uint64_t state_addr);  // does not go through translation table!
void get_all_dp_state(fpga_t *fpga, uint64_t pipeline_idx,
                      uint64_t datapath_idx, uint64_t *ret);
void get_some_dp_state(fpga_t *fpga, uint64_t pipeline_idx,
                       uint64_t datapath_idx, uint64_t start_state_addr,
                       uint64_t end_state_addr,
                       uint64_t *ret);  // noninclusive end
// accumulate over all DPs
uint64_t get_state(
    fpga_t *fpga, uint64_t pipeline_idx,
    uint64_t state_addr);  // does not go through translation table!
void get_all_state(fpga_t *fpga, uint64_t pipeline_idx, uint64_t *ret);
void get_some_state(fpga_t *fpga, uint64_t pipeline_idx,
                    uint64_t start_state_addr, uint64_t end_state_addr,
                    uint64_t *ret);  // noninclusive end

int cxl_dev_init(
    void *hdm_ptr);  // return code 0 => all good. called by library

// installs pipeline with verification
// sets translation unit to identity map
void install_pipeline(fpga_t *fpga, uint64_t pipeline_idx,
                      uint64_t filter_start, uint64_t filter_end,
                      uint64_t map_shift, uint64_t sample_bitmask,
                      uint64_t post_translation_shift);

// Implementation
#define SYSTEM_BIT (18lu)
#define PIPELINE_SELECTOR_BIT_LSB \
    (13lu)  // goes up to 15, max 8 pipelines (for now)
#define MAX_PIPELINES (8lu)
#define PIPELINE_CONFIG_BIT (17lu)
#define VIRTUAL_PIPELINE_CONFIG_BIT (19lu)
#define DATAPATH_SELECTOR_BIT (16lu)

#define USER_ROUTING_BITS_LSB (13lu)
#define USER_ROUTING_BITS_MSB (17lu)  // out of date
#define N_USER_ROUTING_BITS \
    (USER_ROUTING_BITS_MSB - USER_ROUTING_BITS_LSB + 1lu)
#define USER_ROUTING_BITMASK \
    (((1lu << N_USER_ROUTING_BITS) - 1lu) << USER_ROUTING_BITS_LSB)
#define ADDR_LSB (3lu)
#define ADDR_BITMASK (((1lu << USER_ROUTING_BITS_LSB) - 1lu) << ADDR_LSB)

#define TRANSLATION_ENTRY_VALID_BIT \
    (13lu)  // TODO: change this if the state size ever changes... assert this
            // maybe?

#define FAST_CSR 1

#if FAST_CSR
//#define FAST_CSR_OFFSET (GB(8))
#define FAST_CSR_OFFSET (GB(16))
#define CTRL_READ_FAST(x, y, z) fast_read_cached(x, y, z)
#define FLUSH_FPGA_COUNTERS(x, y) flush_fpga_counters(x, y)
#define COMPRESSION (1)
#else
#define FAST_CSR_OFFSET (GB(16))
#define CTRL_READ_FAST(x, y, z) ctrl_read(x, y, z)
#define FLUSH_FPGA_COUNTERS(x, y) \
    (void)(x);                    \
    (void)(y)
#endif

#define FPGA_PHYS_ADDR_BASE \
    (0x4080000000lu)  // physical address of FPGA. TODO combine with other
                      // variable in .h
#define FPGA_PHYS_ADDR (FPGA_PHYS_ADDR_BASE + (CXL_CAPACITY / 64) + MB(2))
#define FAST_CSR_PHYS_ADDR (FAST_CSR_OFFSET + FPGA_PHYS_ADDR)

// typedef struct datapath_config {
// }

typedef struct virtual_pipeline_config {
    uint64_t _d_translation_table[N_COUNTERS];
    // datapath_config _datapaths[DATAPATHS_PER_PIPELINE];
} virtual_pipeline_config_t;

typedef struct pipeline_config {
    uint64_t _d_added_stall;
    uint64_t _d_filter_start;
    uint64_t _d_filter_end;
    uint64_t _d_map_shift;  // aka log_state_size. 12 => 4096 byte pages
    uint64_t _d_sample_bitmask;
    uint64_t
        _d_post_translation_shift;  // if different from map shift, will have
                                    // strange and sometimes desiarable effects
    uint64_t _max_translation_value;  // safety, set by install_pipeline
    virtual_pipeline_config_t _virtual_pipeline;
} pipeline_config_t;

typedef struct nemo_fpga_config {
    // overflows, but those are read-only
    uint64_t _d_pipeline_selector;
    uint64_t _d_controlpath_start_physical_address;
    pipeline_config_t _pipeline_configs[N_PIPELINES];
} nemo_fpga_config_t;

struct fpga_t {
    volatile uint64_t *_register_space;
    uint64_t *_hdm_space;
    nemo_fpga_config_t _current_config;
};

__attribute__((unused)) static uint64_t fast_read_cached_raw(fpga_t *fpga,
                                                             uint64_t addr) {
    addr += FAST_CSR_OFFSET;
    assert(addr < CXL_CAPACITY);
    uint64_t ret = fpga->_hdm_space[addr / sizeof(uint64_t)];
    // if (ret != 0) {
    // LOG_ERR("accessing hdm addr 0x%lx = %lx\n", addr, ret); // TODO:
    // uncomment and debug
    //}
    if (ret >> 63lu) {
        LOG_ERR("got back likely invalid result: mem[0x%lx] = 0x%lx\n", addr,
                ret);
    }
    return ret;
}

// TODO: this is just a merge of above and below functions. should clean it up
// somehow.
__attribute((unused)) static uint64_t *get_fast_read_address(
    fpga_t *fpga, uint64_t route_addr, uint64_t sub_addr) {
    uint64_t addr = 0;
    //(void)(route_addr + sub_addr);
    uint64_t addressibility = COMPRESSION ? 0 : 3;
    addr |= (route_addr >> 3) << (SRAM_LOG_DEPTH + 3 + addressibility);
    addr |= (sub_addr << addressibility);
    addr += FAST_CSR_OFFSET;
    // LOG_ERR("accessing ptr hdm addr 0x%lx\n", addr);
    assert(addr < CXL_CAPACITY);
    return &fpga->_hdm_space[addr / sizeof(uint64_t)];
}

__attribute__((unused)) static uint64_t fast_read_cached(fpga_t *fpga,
                                                         uint64_t route_addr,
                                                         uint64_t sub_addr) {
    uint64_t addr = 0;
    //(void)(route_addr + sub_addr);
    uint64_t addressibility = COMPRESSION ? 0 : 3;
    addr |= (route_addr >> 3) << (SRAM_LOG_DEPTH + 3 + addressibility);
    addr |= (sub_addr << addressibility);
    return fast_read_cached_raw(fpga, addr);
}

static uint64_t ctrl_read_raw(fpga_t *fpga, uint64_t addr) {
    assert(addr < MB(1));
    assert(addr % sizeof(uint64_t) == 0);
    uint64_t ret = fpga->_register_space[addr / sizeof(uint64_t)];
    // LOG("read reg 0x%lx = %lx\n", addr, ret);
    return ret;
}
static void ctrl_write_raw(fpga_t *fpga, uint64_t addr, uint64_t data) {
    assert(addr < MB(1));
    assert(addr % sizeof(uint64_t) == 0);
    // LOG("write reg 0x%lx = %lx\n", addr, data);
    fpga->_register_space[addr / sizeof(uint64_t)] = data;
}

static uint64_t pipeline_selector_address() {
    uint64_t addr = 0;
    addr |= 1lu << SYSTEM_BIT;
    addr |= 0x20lu;
    return addr;
}
static uint64_t controlpath_start_physical_address_address() {
    uint64_t addr = 0;
    addr |= 1lu << SYSTEM_BIT;
    addr |= 0x28lu;
    return addr;
}

void set_pipeline_selector(fpga_t *fpga, uint64_t new) {
    // assert((new & ADDR_BITMASK) == 0);
    assert((new &(1lu << SYSTEM_BIT)) == 0);
    if (fpga->_current_config._d_pipeline_selector == new) return;
    fpga->_current_config._d_pipeline_selector = new;
    ctrl_write_raw(fpga, pipeline_selector_address(), new);
}

void set_controlpath_start_physical_address(fpga_t *fpga, uint64_t new) {
    if (fpga->_current_config._d_controlpath_start_physical_address == new)
        return;
    fpga->_current_config._d_controlpath_start_physical_address = new;
    LOG_ERR("Setting controlpath_start_physical_address = 0x%lx\n", new);
    ctrl_write_raw(fpga, controlpath_start_physical_address_address(), new);
}

void ctrl_write(fpga_t *fpga, uint64_t route_addr, uint64_t sub_addr,
                uint64_t data) {
    if ((route_addr & (1lu << SYSTEM_BIT)) != 0) {
        // system write
        ctrl_write_raw(fpga, route_addr | sub_addr, data);
    } else {
        // user write
        // uint64_t routing_bits = route_addr & USER_ROUTING_BITMASK;
        // assert(routing_bits == route_addr);
        // set_pipeline_selector(fpga, routing_bits);
        set_pipeline_selector(fpga, route_addr);
        assert(sub_addr == (sub_addr & ADDR_BITMASK));
        ctrl_write_raw(fpga, sub_addr, data);
    }
}

uint64_t ctrl_read(fpga_t *fpga, uint64_t route_addr, uint64_t sub_addr) {
    if ((route_addr & (1lu << SYSTEM_BIT)) != 0) {
        // system read
        return ctrl_read_raw(fpga, route_addr | sub_addr);
    } else {
        // user read
        // uint64_t routing_bits = route_addr & USER_ROUTING_BITMASK;
        // assert(routing_bits == route_addr);
        // set_pipeline_selector(fpga, routing_bits);
        set_pipeline_selector(fpga, route_addr);
        assert(sub_addr == (sub_addr & ADDR_BITMASK));
        return ctrl_read_raw(fpga, sub_addr);
    }
}

void set_added_stall(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current =
        &(fpga->_current_config._pipeline_configs[pipeline_idx]._d_added_stall);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x00lu;
    route_addr |= 0x00lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    *current = val;
}
void set_filter_start(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current = &(
        fpga->_current_config._pipeline_configs[pipeline_idx]._d_filter_start);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x08lu;
    route_addr |= 0x08lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    *current = val;
}
void set_filter_end(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current =
        &(fpga->_current_config._pipeline_configs[pipeline_idx]._d_filter_end);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x10lu;
    route_addr |= 0x10lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    *current = val;
}
void set_map_shift(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current =
        &(fpga->_current_config._pipeline_configs[pipeline_idx]._d_map_shift);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x18lu;
    route_addr |= 0x18lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    *current = val;
}
void set_sample_bitmask(fpga_t *fpga, uint64_t pipeline_idx, uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current = &(fpga->_current_config._pipeline_configs[pipeline_idx]
                              ._d_sample_bitmask);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x18lu;
    route_addr |= 0x20lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    *current = val;
}
void set_post_translation_shift(fpga_t *fpga, uint64_t pipeline_idx,
                                uint64_t val) {
    assert(pipeline_idx < N_PIPELINES);
    uint64_t *current = &(fpga->_current_config._pipeline_configs[pipeline_idx]
                              ._d_post_translation_shift);
    if (*current == val) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    // uint64_t sub_addr = 0x18lu;
    route_addr |= 0x28lu;
    uint64_t sub_addr = 0;
    ctrl_write(fpga, route_addr, sub_addr, val);
    LOG_ERR("SETTING POST TRANS SHIFT TO: %ld\n", val);
    *current = val;
}
uint64_t compute_max_translation_value(fpga_t *fpga, uint64_t pipeline_idx,
                                       uint64_t map_shift,
                                       uint64_t post_translation_shift) {
    uint64_t max = 0;
    if (map_shift >= post_translation_shift) {
        if (map_shift - post_translation_shift < SRAM_LOG_DEPTH) {
            max = (1lu << (SRAM_LOG_DEPTH -
                           (map_shift - post_translation_shift))) -
                  1;
        }
    }
    fpga->_current_config._pipeline_configs[pipeline_idx]
        ._max_translation_value = max;
    return max;
}
void set_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                           uint64_t sram_addr_from, uint64_t sram_addr_to) {
    assert(pipeline_idx < N_PIPELINES);
    assert(sram_addr_from < N_COUNTERS);
    assert(sram_addr_to < N_COUNTERS);
    assert(sram_addr_to <= fpga->_current_config._pipeline_configs[pipeline_idx]
                               ._max_translation_value);
    sram_addr_to |= 1lu << TRANSLATION_ENTRY_VALID_BIT;
    uint64_t *current =
        &(fpga->_current_config._pipeline_configs[pipeline_idx]
              ._virtual_pipeline._d_translation_table[sram_addr_from]);
    if (*current == sram_addr_to) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << VIRTUAL_PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    uint64_t sub_addr = sram_addr_from << ADDR_LSB;
    ctrl_write(fpga, route_addr, sub_addr, sram_addr_to);
    *current = sram_addr_to;
}
void clear_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                             uint64_t sram_addr_from) {
    assert(pipeline_idx < N_PIPELINES);
    assert(sram_addr_from < N_COUNTERS);
    uint64_t sram_addr_to = 0;
    uint64_t *current =
        &(fpga->_current_config._pipeline_configs[pipeline_idx]
              ._virtual_pipeline._d_translation_table[sram_addr_from]);
    if (*current == sram_addr_to) return;
    uint64_t route_addr = 0;
    route_addr |= 1lu << VIRTUAL_PIPELINE_CONFIG_BIT;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    uint64_t sub_addr = sram_addr_from << ADDR_LSB;
    ctrl_write(fpga, route_addr, sub_addr, sram_addr_to);
    *current = sram_addr_to;
}

overflow_t get_overflows(fpga_t *fpga) {
    overflow_t ret;
    uint64_t base_addr = 0;
    base_addr |= 1lu << SYSTEM_BIT;
    for (int i = 0; i < 4; i++) {
        uint64_t overflow = ctrl_read(fpga, base_addr, i << ADDR_LSB);
        if (overflow) ret.overflow[i] = 1;
    }
    return ret;
}
uint64_t get_added_stall(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]._d_added_stall;
}
uint64_t get_filter_start(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]
        ._d_filter_start;
}
uint64_t get_filter_end(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]._d_filter_end;
}
uint64_t get_map_shift(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]._d_map_shift;
}
uint64_t get_sample_bitmask(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]
        ._d_sample_bitmask;
}
uint64_t get_post_translation_shift(fpga_t *fpga, uint64_t pipeline_idx) {
    assert(pipeline_idx < N_PIPELINES);
    return fpga->_current_config._pipeline_configs[pipeline_idx]
        ._d_post_translation_shift;
}
translation_result_t get_translation_table(fpga_t *fpga, uint64_t pipeline_idx,
                                           uint64_t sram_addr_from) {
    assert(pipeline_idx < N_PIPELINES);
    assert(sram_addr_from < N_COUNTERS);
    translation_result_t ret;
    uint64_t sram_addr_to_raw =
        fpga->_current_config._pipeline_configs[pipeline_idx]
            ._virtual_pipeline._d_translation_table[sram_addr_from];
    if ((sram_addr_to_raw & (1lu << TRANSLATION_ENTRY_VALID_BIT)) == 0) {
        ret.valid = 0;
        ret.sram_addr_to = -1;
    } else {
        ret.valid = 1;
        ret.sram_addr_to = sram_addr_to_raw;
        ret.sram_addr_to ^= 1lu << TRANSLATION_ENTRY_VALID_BIT;  // turn it off
        assert(ret.sram_addr_to < N_COUNTERS);
    }
    return ret;
}
uint64_t get_dp_state(
    fpga_t *fpga, uint64_t pipeline_idx, uint64_t datapath_idx,
    uint64_t state_addr) {  // does not go through translation table!
    assert(pipeline_idx < N_PIPELINES);
    assert(datapath_idx < DATAPATHS_PER_PIPELINE);
    assert(state_addr < N_COUNTERS);
    uint64_t route_addr = 0;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    route_addr |= datapath_idx << DATAPATH_SELECTOR_BIT;
    uint64_t sub_addr = state_addr << ADDR_LSB;
    return CTRL_READ_FAST(fpga, route_addr, sub_addr);
}
void get_all_dp_state(fpga_t *fpga, uint64_t pipeline_idx,
                      uint64_t datapath_idx, uint64_t *ret) {
    get_some_dp_state(fpga, pipeline_idx, datapath_idx, 0, N_COUNTERS, ret);
}
void get_some_dp_state(fpga_t *fpga, uint64_t pipeline_idx,
                       uint64_t datapath_idx, uint64_t start_addr,
                       uint64_t end_addr, uint64_t *ret) {  // noninclusive end
    assert(pipeline_idx < N_PIPELINES);
    assert(datapath_idx < DATAPATHS_PER_PIPELINE);
    uint64_t route_addr = 0;
    route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
    route_addr |= datapath_idx << DATAPATH_SELECTOR_BIT;
    for (uint64_t state_addr = start_addr; state_addr < end_addr;
         state_addr++) {
        uint64_t res = CTRL_READ_FAST(fpga, route_addr, state_addr << ADDR_LSB);
        ret[state_addr] += res;
    }
}
// accumulate over all DPs
uint64_t get_state(
    fpga_t *fpga, uint64_t pipeline_idx,
    uint64_t state_addr) {  // does not go through translation table!
    assert(pipeline_idx < N_PIPELINES);
    assert(state_addr < N_COUNTERS);
    uint64_t ret = 0;
    for (uint64_t dp = 0; dp < DATAPATHS_PER_PIPELINE; dp++) {
        uint64_t route_addr = 0;
        route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
        route_addr |= dp << DATAPATH_SELECTOR_BIT;
        uint64_t sub_addr = state_addr << ADDR_LSB;
        ret += CTRL_READ_FAST(fpga, route_addr, sub_addr);
    }
    return ret;
}
void get_all_state(fpga_t *fpga, uint64_t pipeline_idx, uint64_t *ret) {
    get_some_state(fpga, pipeline_idx, 0, N_COUNTERS, ret);
}
void get_some_state(fpga_t *fpga, uint64_t pipeline_idx, uint64_t start_addr,
                    uint64_t end_addr, uint64_t *ret) {  // noninclusive end
    assert(pipeline_idx < N_PIPELINES);
    // more efficient to keep dp constant since that keeps routing addr constant
    for (uint64_t dp = 0; dp < DATAPATHS_PER_PIPELINE; dp++) {
        uint64_t route_addr = 0;
        route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
        route_addr |= dp << DATAPATH_SELECTOR_BIT;
        for (uint64_t state_addr = start_addr; state_addr < end_addr;
             state_addr++) {
            // LOG("Getting state 0x%lx datapath 0x%lx\n", state_addr, dp);
            uint64_t sub_addr = state_addr << ADDR_LSB;
            uint64_t res = CTRL_READ_FAST(fpga, route_addr, sub_addr);
            ret[state_addr] += res;
        }
    }
}

static void init_fpga(fpga_t *fpga) {
    // assuming register_space is set up, program with initial values to match
    // sw
    fpga->_current_config._d_pipeline_selector = 1;
    set_pipeline_selector(fpga, 0);
    fpga->_current_config._d_controlpath_start_physical_address = 1;
    set_controlpath_start_physical_address(fpga, FAST_CSR_PHYS_ADDR);
    // set_controlpath_start_physical_address(fpga,
    // &fpga->_hdm_space[FAST_CSR_OFFSET / sizeof(uint64_t)])

    for (uint64_t pipeline_idx = 0; pipeline_idx < N_PIPELINES;
         pipeline_idx++) {
        fpga->_current_config._pipeline_configs[pipeline_idx]._d_added_stall =
            1;
        set_added_stall(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]._d_filter_start =
            1;
        set_filter_start(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]._d_filter_end = 1;
        set_filter_end(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]._d_map_shift = 1;
        set_map_shift(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]
            ._d_sample_bitmask = 1;
        set_sample_bitmask(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]
            ._d_post_translation_shift = 1;
        set_post_translation_shift(fpga, pipeline_idx, 0);
        fpga->_current_config._pipeline_configs[pipeline_idx]
            ._max_translation_value =
            0;  // you need to call compute_max_translation_value to set this
        for (uint64_t sram_address_from = 0; sram_address_from < N_COUNTERS;
             sram_address_from++) {
            fpga->_current_config._pipeline_configs[pipeline_idx]
                ._virtual_pipeline._d_translation_table[sram_address_from] = 1;
            clear_translation_table(fpga, pipeline_idx, sram_address_from);
        }
    }
}

// https://stackoverflow.com/questions/11277984/how-to-flush-the-cpu-cache-in-linux-from-a-c-program
void mem_flush(const volatile void *p, uint64_t allocation_size) {
    const uint64_t cache_line = 64;
    const char *cp = (const char *)p;
    uint64_t i = 0;

    if (p == NULL || allocation_size <= 0) return;

    asm volatile("mfence\n\t" : : : "memory");

    for (i = 0; i < allocation_size; i += cache_line) {
        asm volatile("clflush (%0)\n\t" : : "r"(&cp[i]) : "memory");
    }

    asm volatile("mfence\n\t" : : : "memory");
}

void flush_fpga_counters(fpga_t *fpga, uint64_t pipeline_idx) {
    for (uint64_t dp = 0; dp < DATAPATHS_PER_PIPELINE; dp++) {
        uint64_t route_addr = 0;
        route_addr |= pipeline_idx << PIPELINE_SELECTOR_BIT_LSB;
        route_addr |= dp << DATAPATH_SELECTOR_BIT;
        uint64_t sub_addr_start = 0 << ADDR_LSB;
        uint64_t sub_addr_end = N_COUNTERS << ADDR_LSB;
        uint64_t *raw_addr_start =
            get_fast_read_address(fpga, route_addr, sub_addr_start);
        uint64_t *raw_addr_end =
            get_fast_read_address(fpga, route_addr, sub_addr_end);
        uint64_t size = (uint64_t)raw_addr_end - (uint64_t)raw_addr_start;
        mem_flush(raw_addr_start, size);
    }
}

#define PLUS1_PIPELINE_INDEX (0lu)

static fpga_t _global_fpga;
// fpga_t _global_fpga;

// now the only function that's used
void cxl_dev_update_all_counters(uint64_t *counters) {
    // cxl_dev_read_counters_internal(&_global_fpga, counters, N_COUNTERS);
    FLUSH_FPGA_COUNTERS(&_global_fpga, PLUS1_PIPELINE_INDEX);
    get_all_state(&_global_fpga, PLUS1_PIPELINE_INDEX, counters);
}

// the only function actually used rn
access_counter_t cxl_dev_read_counter_diff(size_t counter_index) {
    assert(false);  // making sure it's unused (not broken, just for clarity)
    // LOG("getting just idx 0x%lx\n", counter_index);
    // return cxl_dev_read_counter_internal(&_global_fpga, counter_index);
    return get_state(&_global_fpga, PLUS1_PIPELINE_INDEX, counter_index);
}

static void *map_registers(char *bar_file_path) {
    int fd;
    void *register_space;
    fd = open(bar_file_path, O_RDWR | O_SYNC);
    if (fd < 0) {
        perror("open BAR file failed");
        return NULL;
    }

    register_space =
        mmap(NULL, BYTES_TO_READ, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (register_space == MAP_FAILED) {
        perror("mmap failed");
        return NULL;
    }

    return register_space;
}

__attribute__((unused)) static void unmap_registers(void *register_space) {
    munmap((void *)register_space, BYTES_TO_READ);
}

void install_pipeline(fpga_t *fpga, uint64_t pipeline_idx,
                      uint64_t filter_start, uint64_t filter_end,
                      uint64_t map_shift, uint64_t sample_bitmask,
                      uint64_t post_translation_shift) {
    size_t requested_size = filter_end - filter_start;
    size_t requested_num_states = requested_size >> map_shift;
    if (requested_num_states == 0) requested_num_states = 1;
    uint64_t sample_amt = sample_bitmask + 1;
    LOG("***** CONFIGURING FPGA PIPELINE %ld *****\n", pipeline_idx);
    LOG("Start=0x%lx\n", filter_start);
    LOG("End=0x%lx\n", filter_end);
    LOG("LSS=%ld\n", map_shift);
    LOG("Requested size=%lx\n", requested_size);
    LOG("Num datapaths=%ld\n", DATAPATHS_PER_PIPELINE);
    LOG("Requested num states per datapath=%ld\n", requested_num_states);
    LOG("Sample bitmask=0x%lx\n", sample_bitmask);
    if (__builtin_popcount(sample_bitmask + 1) != 1) {
        LOG_ERR("Sample bitmask not being 1 below a power of 2 is nonsense!\n");
        assert(false);
    }
    LOG("Sample rate=1 in %ld\n", sample_amt);
    LOG("Post translation shift=%ld\n", post_translation_shift);
    LOG("Number of SRAM states per datapath=%ld\n", N_COUNTERS);
    LOG("Translation table=Identity\n");
    LOG("\n");
    if (requested_num_states > N_COUNTERS) {
        LOG_ERR(
            "requested address range is too big for the number of "
            "states you want to track!\n");
        size_t realend = (N_COUNTERS << map_shift) + filter_start;
        LOG_WARN("NOTE: A more realistic end address would be 0x%lx\n",
                 realend);
        LOG_WARN(
            "NOTE: Currently, the SRAM addresses will wrap-around, meaning "
            "you may be getting garbage results.\n");
    }
    if (requested_num_states < N_COUNTERS) {
        LOG_WARN(
            "You are only using %ld states when the SRAM can fit %ld "
            "states.\n",
            requested_num_states, N_COUNTERS);
    }
    uint64_t max_translation_value = compute_max_translation_value(
        fpga, pipeline_idx, map_shift, post_translation_shift);
    if (post_translation_shift != map_shift) {
        uint64_t shift_page_size = (1lu << map_shift) / 1024;
        uint64_t post_trans_page_size = (1lu << post_translation_shift) / 1024;
        if (post_translation_shift > map_shift) {
            LOG_ERR(
                "You are reinterpreting the address post-translation. "
                "In this case, the overall effect will be a possible applied "
                "sampling from %ldKiB pages to %ldKiB pages."
                "You should not do this, and prefer to instead set map and "
                "sample_bitmask appropriately.\n",
                shift_page_size, post_trans_page_size);
        } else {
            LOG_WARN(
                "You are reinterpreting the address post-translation. "
                "Be careful. The translation table will operate at a %ldKiB "
                "granularity, and the counters will operate at "
                "a %ldKiB granularity. This means the max value in the "
                "translation table must be: %ld\n",
                shift_page_size, post_trans_page_size, max_translation_value);
        }
    }

    set_added_stall(fpga, pipeline_idx, 0);
    set_filter_start(fpga, pipeline_idx, filter_start);
    set_filter_end(fpga, pipeline_idx, filter_end);
    set_map_shift(fpga, pipeline_idx, map_shift);
    set_sample_bitmask(fpga, pipeline_idx, sample_bitmask);
    set_post_translation_shift(fpga, pipeline_idx, post_translation_shift);
    max_translation_value = 8191;
    for (uint64_t sram_address_from = 0;
         sram_address_from <= max_translation_value; sram_address_from++) {
        set_translation_table(fpga, pipeline_idx, sram_address_from,
                              sram_address_from);
        // clear_translation_table(fpga, pipeline_idx, sram_address_from);
    }
    for (uint64_t sram_address_from = max_translation_value + 1;
         sram_address_from < N_COUNTERS; sram_address_from++) {
        clear_translation_table(fpga, pipeline_idx, sram_address_from);
    }

    LOG("Clearing counters...");

    // clear stale counter values
    uint64_t counters[N_COUNTERS];
    get_all_state(fpga, pipeline_idx, counters);
    LOG("FPGA pipeline installed.\n");
}

void install_skew() {
    const uint64_t phys_map_addr = 0x4080000000L;
    const uint64_t dax_size_start = MB(2) + CXL_CAPACITY / 64;
    const uint64_t phys_size_usable_actual = 0x3E0000000L;  // trial and error
    const uint64_t dax_end = dax_size_start + phys_size_usable_actual;
    // const uint64_t dax_end = dax_size_start + MB(16);

    uint64_t fpga_start = phys_map_addr + dax_size_start;
    uint64_t fpga_end = phys_map_addr + dax_end;
    install_pipeline(&_global_fpga, PLUS1_PIPELINE_INDEX, fpga_start, fpga_end,
                     21, 0, 12);
}

static int create_fpga(char *bar_file_path, void *hdm_ptr, fpga_t *fpga) {
    fpga->_register_space = map_registers(bar_file_path);
    fpga->_hdm_space = hdm_ptr;
    if (!fpga->_register_space) {
        LOG_ERR("mapping device registers failed\n");
        return -1;
    }
    init_fpga(fpga);
    return 0;
}

/*
void partition_cxl_dev_memory(void *dev_mem_start, size_t dev_mem_len, void **
app_mem_start, size_t *app_mem_len) { _global_fpga._hdm_space = dev_mem_start;
    assert(dev_mem_len == GB(16));

    *app_mem_start = dev_mem_start;
    *app_mem_len = FAST_CSR_OFFSET;//dev_mem_len
}
*/

#define PAGE_SHIFT_4K 12
#define PAGE_SIZE_4K (1UL << PAGE_SHIFT_4K)
#define PFN_MASK ((1ULL << 55) - 1)

uintptr_t virt_to_phys(void *virt_addr) {
    int fd;
    uintptr_t vaddr = (uintptr_t)virt_addr;
    uintptr_t paddr = 0;

    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open pagemap");
        return 0;
    }

    off_t offset = (vaddr / PAGE_SIZE_4K) * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        close(fd);
        return 0;
    }

    uint64_t entry;
    if (read(fd, &entry, sizeof(entry)) != sizeof(entry)) {
        perror("read");
        close(fd);
        return 0;
    }

    close(fd);

    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "Page not present\n");
        return 0;
    }

    uint64_t pfn = entry & PFN_MASK;

    // Physical address is PFN * base-page-size + offset-within-page
    // Works regardless of 4K/2M/1G, because PFN always indexes 4K units
    paddr = (pfn << PAGE_SHIFT_4K) | (vaddr & (PAGE_SIZE_4K - 1));

    return paddr;
}

int cxl_dev_init(void *hdm_ptr) {
    int ret;
    ret = create_fpga(RESOURCE_PATH, hdm_ptr, &_global_fpga);
    if (ret < 0) return ret;

    LOG("hdm ptr v=0x%lx, p=0x%lx\n", (uint64_t)hdm_ptr,
        (uint64_t)virt_to_phys(hdm_ptr));

    /*

    | --------------- 16 -------------- |
    | start | usable              | end |

    start = 0x010200000 bytes
    claim = 0x3EFE00000 usable bytes,
    actual= 0x3E0000000 usable bytes.

    end = start_phys + actual;
    start_phys = phys_map_addr + start;

    */
    const uint64_t phys_map_addr = 0x4080000000L;
    // const uint64_t phys_size = 16L * GB;
    // const uint64_t phys_size_usable_reported = 16909336576L; // according to
    // ndctl list
    const uint64_t phys_size_usable_actual = 0x3E0000000L;  // trial and error
    // const uint64_t dax_size_start = 0x10200000L;            // 0.25 GiB +
    // 2MiB
    const uint64_t dax_size_start = MB(2) + CXL_CAPACITY / 64;
    // start + phys_size_usable_actual + end = phys_size
    // const uint64_t dax_size_end = phys_size - phys_size_usable_actual -
    // dax_size_start; // 254 MiB
    const uint64_t dax_end = dax_size_start + phys_size_usable_actual;
    // LOG("typical 0x20008 (start) = 0x%lx, typical 0x20010 (end) = 0x%lx,
    // typical 0x20018 (shift) = 9 (for a page)\n", phys_map_addr +
    // dax_size_start, phys_map_addr + phys_size - dax_size_end);
    LOG("typical 0x20008 (start) = 0x%lx, typical 0x20010 (end) = 0x%lx, "
        "typical 0x20018 (shift) = 9 (for a page)\n",
        phys_map_addr + dax_size_start, phys_map_addr + dax_end);

    uint64_t fpga_start = phys_map_addr + dax_size_start;
    uint64_t fpga_end = phys_map_addr + dax_end;
    install_pipeline(&_global_fpga, PLUS1_PIPELINE_INDEX, fpga_start, fpga_end,
                     21, 0, 21);

    return 0;
}
