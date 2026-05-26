#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#include "common.h"
#include "shared/hemem-shared.h"
#include "support/dsa.h"
#include "util/shared.h"

#define NUM_PAGE_MOVES 128

void bench_memcpy_sz(size_t copy_sz) {
    static const size_t kWarmupIters = 200;
    static const size_t kBenchIters = 1000;

    char bench_name[256];
    snprintf(bench_name, sizeof(bench_name), "dsa memcpy (%lu bytes)", copy_sz);

    sleep(2);

    uint64_t mmap_sz = copy_sz * 2;

    void *addr = mmap(NULL, mmap_sz, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        assert(0);
    }

    unsigned char *first_half = (unsigned char *)addr;
    unsigned char *second_half = first_half + copy_sz;

    memset(first_half, 1, copy_sz);
    memset(second_half, 2, copy_sz);

    for (size_t i = 0; i < kWarmupIters; i++) {
        dsa_memcpy(first_half, second_half, copy_sz);
    }

    uint64_t measurements[kBenchIters];
    for (size_t i = 0; i < kBenchIters; i++) {
        uint64_t start = get_ns();
        dsa_memcpy(first_half, second_half, copy_sz);
        uint64_t end = get_ns();
        measurements[i] = end - start;
    }

    postbench(bench_name, measurements, kBenchIters);

    munmap(addr, mmap_sz);
}

int main() {
    static const size_t kMemcpySizes[] = {64,   256,   1024,  4096,
                                          8192, 16384, 65536, MB(2)};
    assert(dsa_init() == 0);

    size_t len = sizeof(kMemcpySizes) / sizeof(kMemcpySizes[0]);
    for (size_t i = 0; i < len; i++) {
        bench_memcpy_sz(kMemcpySizes[i]);
    }

    dsa_shutdown();
}
