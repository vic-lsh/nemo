#include "dsa.h"

#include <accel-config/libaccel_config.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/idxd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <xmmintrin.h>

#include "util/log.h"

#define NOP_RETRY 10000

struct wq_info {
    bool wq_mapped;
    void *wq_portal;
    int wq_fd;
};

// DSA Work Queue info shared across various impl functions in this file.
struct wq_info wq_ctx;

static inline uint64_t repeat_byte(uint8_t byte) {
    return 0x0101010101010101ULL * byte;
}

static inline int enqcmd(volatile void *reg, struct dsa_hw_desc *desc) {
    uint8_t retry;

    asm volatile(
        ".byte 0xf2, 0x0f, 0x38, 0xf8, 0x02\t\n"
        "setz %0\t\n"
        : "=r"(retry)
        : "a"(reg), "d"(desc));
    return (int)retry;
}

static inline void submit_desc(void *wq_portal, struct dsa_hw_desc *hw) {
    while (enqcmd(wq_portal, hw)) _mm_pause();
}

__attribute__((unused)) static uint8_t op_status(uint8_t status) {
    return status & DSA_COMP_STATUS_MASK;
}

static bool is_write_syscall_success(int fd) {
    struct dsa_hw_desc desc = {0};
    struct dsa_completion_record comp __attribute__((aligned(32)));
    int retry = 0;
    int rc;

    desc.opcode = DSA_OPCODE_NOOP;
    desc.flags = IDXD_OP_FLAG_CRAV | IDXD_OP_FLAG_RCR;
    comp.status = 0;

    desc.completion_addr = (unsigned long)&comp;

    rc = write(fd, &desc, sizeof(desc));

    if (rc == sizeof(desc)) {
        while (comp.status == 0 && retry++ < NOP_RETRY) _mm_pause();

        if (comp.status == DSA_COMP_SUCCESS) return true;
    }

    return false;
}

static int map_wq(struct wq_info *wq_info) {
    void *wq_portal;
    struct accfg_ctx *ctx;
    struct accfg_wq *wq;
    struct accfg_device *device;
    char path[PATH_MAX];
    int fd;
    int wq_found = 0;

    wq_portal = MAP_FAILED;

    accfg_new(&ctx);

    accfg_device_foreach(ctx, device) {
        /*
         * Use accfg_device_(*) functions to select enabled device
         * based on name, numa node
         */
        accfg_wq_foreach(device, wq) {
            if (accfg_wq_get_user_dev_path(wq, path, sizeof(path))) continue;
            /* Use accfg_wq_(*) functions select WQ of type ACCFG_WQT_USER
             * and desired mode
             */
            wq_found = accfg_wq_get_type(wq) == ACCFG_WQT_USER &&
                       accfg_wq_get_mode(wq) == ACCFG_WQ_SHARED;

            if (wq_found) break;
        }

        if (wq_found) break;
    }

    accfg_unref(ctx);

    if (!wq_found) return -ENODEV;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        wq_portal =
            mmap(NULL, 0x1000, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
    }

    if (wq_portal == MAP_FAILED) {
        /* EPERM means the driver doesn't support mmap but can support write
         * syscall. So fallback to write syscall */
        if (errno == EPERM && is_write_syscall_success(fd)) {
            wq_info->wq_mapped = false;

            wq_info->wq_fd = fd;

            return 0;
        }

        return -errno;
    }

    wq_info->wq_portal = wq_portal;
    wq_info->wq_mapped = true;
    wq_info->wq_fd = -1;

    return 0;
}

int dsa_init() { return map_wq(&wq_ctx); }

void dsa_shutdown() {}

void dsa_poll_complete(struct dsa_completion_record *cr) {
    while (cr->status == 0) _mm_pause();
}

static int __dsa_memcpy(struct wq_info *wq_info, void *dst, const void *src,
                        size_t len) {
    int rc = 0;

    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32)));

    memset(&desc, 0, sizeof(struct dsa_hw_desc));
    memset(&comp, 0, sizeof(struct dsa_completion_record));

    desc.opcode = DSA_OPCODE_MEMMOVE;

    /* Request a completion – since we poll on status, this flag needs to be
     * 1 for status to be updated on successful completion */
    // Request Completion Record: completion record is always written in case of
    // success or failure.
    desc.flags |= IDXD_OP_FLAG_RCR;

    /* CRAV should be 1 since RCR = 1 */
    // Completion Record Address Valid: tells DSA that the `completion_addr`
    // field contains a valid address.
    desc.flags |= IDXD_OP_FLAG_CRAV;

    /* Hint to direct data writes to CPU cache */
    // TODO: make this configurable
    // TODO: benchmark perf impact of this flag
    desc.flags |= IDXD_OP_FLAG_CC;
    desc.xfer_size = len;
    desc.src_addr = (uintptr_t)src;
    desc.dst_addr = (uintptr_t)dst;
    desc.completion_addr = (uintptr_t)&comp;

    LOG_DEBUG("memmove size %lu\n", len);

retry:

    if (wq_info->wq_mapped) {
        LOG_DEBUG("wq mapped -- submitting desc\n");
        submit_desc(wq_info->wq_portal, &desc);
    } else {
        LOG_DEBUG("wq not mapped -- initiating write\n");
        int rc = write(wq_info->wq_fd, &desc, sizeof(desc));

        if (rc != sizeof(desc)) return EXIT_FAILURE;
    }

    while (comp.status == 0) _mm_pause();

    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            // Gets bit 7 of the status field. See S8.2.1 of DSA spec.
            int is_fault_access_write = comp.status & DSA_COMP_STATUS_WRITE;
            volatile char *t;

            t = (char *)comp.fault_addr;
            // check if the faulting address still faults
            is_fault_access_write ? *t = *t : *t;

            // The src, dst, and xfer_size update logic depends on the result
            // field. Below is the logic for result == 0.
            // See S8.3.4 of the DSA spec.
            assert(comp.result == 0);
            desc.src_addr += comp.bytes_completed;
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;

        } else {
            LOG_DEBUG("desc failed status 0x%x\n", comp.status);
            rc = EXIT_FAILURE;
        }
    }

    return rc;
}

static int __dsa_memset(struct wq_info *wq_info, void *dst, uint8_t value,
                        size_t len) {
    int rc = 0;

    struct dsa_hw_desc desc = {};
    struct dsa_completion_record comp __attribute__((aligned(32)));

    memset(&desc, 0, sizeof(struct dsa_hw_desc));
    memset(&comp, 0, sizeof(struct dsa_completion_record));

    desc.opcode = DSA_OPCODE_MEMFILL;

    // Request a completion record so the accelerator updates status.
    desc.flags |= IDXD_OP_FLAG_RCR;
    /* CRAV should be 1 since RCR = 1 */
    // Completion Record Address Valid: tells DSA that the `completion_addr`
    // field contains a valid address.
    desc.flags |= IDXD_OP_FLAG_CRAV;
    // Cache-control hint: push writes through CPU cache.
    desc.flags |= IDXD_OP_FLAG_CC;

    desc.pattern = repeat_byte(value);
    desc.dst_addr = (uintptr_t)dst;
    desc.xfer_size = len;
    desc.completion_addr = (uintptr_t)&comp;

    LOG_DEBUG("memfill size %lu\n", len);

retry:

    if (wq_info->wq_mapped) {
        LOG_DEBUG("wq mapped -- submitting desc\n");
        submit_desc(wq_info->wq_portal, &desc);
    } else {
        LOG_DEBUG("wq not mapped -- initiating write\n");
        int rc = write(wq_info->wq_fd, &desc, sizeof(desc));

        if (rc != sizeof(desc)) return EXIT_FAILURE;
    }

    while (comp.status == 0) _mm_pause();

    if (comp.status != DSA_COMP_SUCCESS) {
        if (op_status(comp.status) == DSA_COMP_PAGE_FAULT_NOBOF) {
            volatile char *t;

            t = (char *)comp.fault_addr;
            *t = *t;

            assert(comp.result == 0);
            desc.dst_addr += comp.bytes_completed;
            desc.xfer_size -= comp.bytes_completed;
            goto retry;

        } else {
            LOG_DEBUG("desc failed status 0x%x\n", comp.status);
            rc = EXIT_FAILURE;
        }
    }

    return rc;
}

int dsa_memcpy(void *dst, const void *src, size_t len) {
    return __dsa_memcpy(&wq_ctx, dst, src, len);
}

int dsa_memset(void *dst, int value, size_t len) {
    if (len == 0) return 0;
    return __dsa_memset(&wq_ctx, dst, (uint8_t)value, len);
}

int dsa_memcpy_nb(void *dst, const void *src, size_t len,
                  struct dsa_completion_record *cr) {
    struct wq_info *wq_info = &wq_ctx;

    struct dsa_hw_desc desc = {};

    memset(&desc, 0, sizeof(struct dsa_hw_desc));
    memset(cr, 0, sizeof(struct dsa_completion_record));

    desc.opcode = DSA_OPCODE_MEMMOVE;

    /* Request a completion – since we poll on status, this flag needs to be
     * 1 for status to be updated on successful completion */
    // Request Completion Record: completion record is always written in case of
    // success or failure.
    desc.flags |= IDXD_OP_FLAG_RCR;

    /* CRAV should be 1 since RCR = 1 */
    // Completion Record Address Valid: tells DSA that the `completion_addr`
    // field contains a valid address.
    desc.flags |= IDXD_OP_FLAG_CRAV;

    /* Hint to direct data writes to CPU cache */
    // TODO: make this configurable
    desc.flags |= IDXD_OP_FLAG_CC;
    desc.xfer_size = len;
    desc.src_addr = (uintptr_t)src;
    desc.dst_addr = (uintptr_t)dst;
    desc.completion_addr = (uintptr_t)cr;

    LOG_DEBUG("memmove size %lu\n", len);

    if (wq_info->wq_mapped) {
        LOG_DEBUG("wq mapped -- submitting desc\n");
        submit_desc(wq_info->wq_portal, &desc);
    } else {
        LOG_DEBUG("wq not mapped -- initiating write\n");
        int rc = write(wq_info->wq_fd, &desc, sizeof(desc));

        if (rc != sizeof(desc)) return EXIT_FAILURE;
    }

    return 0;
}
