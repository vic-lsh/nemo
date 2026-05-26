#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#include "shared/hemem-shared.h"
#include "support/dsa.h"
#include "unity.h"
#include "util/shared.h"

bool init_success = false;

#define NUM_PAGE_MOVES 128

// This is run before EACH test
void setUp(void) {
    int ret = dsa_init();
    if (ret != 0) {
        printf("dsa ret: %d\n", ret);
    }
    init_success = ret == 0;
}

// This is run after EACH test
void tearDown(void) { dsa_shutdown(); }

__attribute__((unused)) static long long get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000000LL) + (ts.tv_nsec / 1000LL);
}

static inline uint64_t get_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void test_memcpy_once(void) {
    TEST_ASSERT_TRUE_MESSAGE(init_success, "DSA didn't initialize correctly");

    uint64_t mmap_sz = MB(4);
    uint64_t half_sz = MB(2);

    void *addr =
        mmap(NULL,  // Let kernel choose address
             mmap_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1,  // No file descriptor needed for MAP_ANONYMOUS
             0    // No offset
        );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        TEST_ABORT();
    }

    unsigned char *first_half = (unsigned char *)addr;
    unsigned char *second_half = first_half + half_sz;

    memset(first_half, 1, half_sz);
    memset(second_half, 2, half_sz);

    dsa_memcpy(first_half, second_half, half_sz);

    TEST_ASSERT_TRUE_MESSAGE(memcmp(first_half, second_half, half_sz) == 0,
                             "memcpy was incorrect");

    munmap(addr, mmap_sz);
}

static bool is_all_same_value(char *data, size_t len, char expected) {
    for (size_t i = 0; i < len; i++) {
        if (data[i] != expected) {
            printf("expected %d found %d\n", expected, data[i]);
            return false;
        }
    }
    return true;
}

void test_memcpy_nb(void) {
    TEST_ASSERT_TRUE_MESSAGE(init_success, "DSA didn't initialize correctly");

    static const uint64_t ITERS = 64;
    uint64_t chunk_sz = MB(2);
    uint64_t mmap_sz = chunk_sz * (ITERS + 1);

    uint8_t *addr =
        mmap(NULL,  // Let kernel choose address
             mmap_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1,  // No file descriptor needed for MAP_ANONYMOUS
             0    // No offset
        );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        TEST_ABORT();
    }

    // fill each chunk with initial data
    for (uint64_t i = 0; i < (ITERS + 1); i++) {
        uint8_t *chunk = addr + chunk_sz * i;
        memset(chunk, i, chunk_sz);
    }

    __attribute__((aligned(32))) struct dsa_completion_record comps[ITERS];

    // left-shifts each chunk by one.
    for (uint64_t i = 0; i < ITERS; i++) {
        void *dst = addr + chunk_sz * i;
        void *src = addr + chunk_sz * (i + 1);
        int rc = dsa_memcpy_nb(dst, src, chunk_sz, &comps[i]);
        TEST_ASSERT_TRUE_MESSAGE(rc == 0, "dsa_memcpy_nb failed");
    }

    // polls for completion for all records.
    // do this based on request submission order, because our DSA configuration
    // guarantees FIFO request handling order.
    for (uint64_t i = 0; i < ITERS; i++) {
        dsa_poll_complete(&comps[i]);
        // TODO: what about the case where DSA complains about a page fault?
        TEST_ASSERT_TRUE_MESSAGE(comps[i].status == DSA_COMP_SUCCESS,
                                 "dsa_memcpy failed");
    }

    // verify that memcpys actually worked by inspecting chunk content
    for (uint64_t i = 0; i < ITERS; i++) {
        void *chunk = addr + chunk_sz * i;
        char expected_val = i + 1;
        if (!is_all_same_value(chunk, chunk_sz, expected_val)) {
            printf("memcpy was incorrect for chunk %lu\n", i);
            TEST_FAIL();
        }
    }

    munmap(addr, mmap_sz);
}
