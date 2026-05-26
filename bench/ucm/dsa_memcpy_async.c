#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "common.h"
#include "shared/hemem-shared.h"
#include "support/dsa.h"
#include "util/shared.h"

#define NUM_PAGE_MOVES 128

void bench_memcpy(void) {
    static const size_t kWarmupIters = 200;
    static const size_t kBenchIters = 1000;
    static const size_t kPageMoves = NUM_PAGE_MOVES;
    static const char *kBenchName = "synchronous dsa memcpy";

    sleep(2);

    uint64_t half_sz = MB(2);
    uint64_t mmap_sz = half_sz * 2;

    void *addr =
        mmap(NULL,  // Let kernel choose address
             mmap_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1,  // No file descriptor needed for MAP_ANONYMOUS
             0    // No offset
        );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        assert(0);
    }

    unsigned char *first_half = (unsigned char *)addr;
    unsigned char *second_half = first_half + half_sz;

    memset(first_half, 1, half_sz);
    memset(second_half, 2, half_sz);

    for (size_t i = 0; i < kWarmupIters; i++) {
        dsa_memcpy(first_half, second_half, half_sz);
    }

    uint64_t measurements[kBenchIters];
    for (size_t i = 0; i < kBenchIters; i++) {
        uint64_t start = get_ns();

        for (size_t j = 0; j < kPageMoves; j++) {
            dsa_memcpy(first_half, second_half, half_sz);
        }

        uint64_t end = get_ns();
        measurements[i] = end - start;
        usleep(100);
    }

    postbench(kBenchName, measurements, kBenchIters);

    munmap(addr, mmap_sz);
}

void bench_memcpy_async(void) {
    // benchmark parameters
    static const size_t kWarmupIters = 200;
    static const size_t kBenchIters = 1000;
    static const size_t kPageMoves = NUM_PAGE_MOVES;
    static const char *kBenchName = "async dsa memcpy";

    sleep(2);

    uint64_t chunk_sz = MB(2);
    uint64_t mmap_sz = chunk_sz * (kPageMoves + 1);

    uint8_t *addr =
        mmap(NULL,  // Let kernel choose address
             mmap_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,
             -1,  // No file descriptor needed for MAP_ANONYMOUS
             0    // No offset
        );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        assert(0);
    }

    // warmup
    for (size_t i = 0; i < kWarmupIters; i++) {
        dsa_memcpy(addr, addr + chunk_sz, chunk_sz);
    }

    __attribute__((aligned(32))) struct dsa_completion_record comps[kPageMoves];

    uint64_t measurements[kBenchIters];
    for (size_t i = 0; i < kBenchIters; i++) {
        uint64_t start = get_ns();

        // actual benchmark logic: left-shifts each chunk by one.
        for (uint64_t j = 0; j < kPageMoves; j++) {
            void *dst = addr + chunk_sz * j;
            void *src = addr + chunk_sz * (j + 1);
            dsa_memcpy_nb(dst, src, chunk_sz, &comps[j]);
        }
        // polls for completion for all records.
        for (size_t j = 0; j < kPageMoves; j++) {
            dsa_poll_complete(&comps[j]);
        }

        uint64_t end = get_ns();
        measurements[i] = end - start;
        usleep(100);
    }

    postbench(kBenchName, measurements, kBenchIters);

    munmap(addr, mmap_sz);
}

int main() {
    assert(dsa_init() == 0);

    bench_memcpy();
    bench_memcpy_async();

    dsa_shutdown();
}
