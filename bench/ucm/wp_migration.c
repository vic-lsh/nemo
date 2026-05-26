#include <errno.h>
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "common.h"
#include "physmem/config.h"
#include "shared/hemem-shared.h"
#include "support/dsa.h"
#include "util/shared.h"

#define NUM_PAGE_MOVES 128

#define MEM_SZ GB(4)

int memfd = -1;
int uffd = -1;
void *fastmem_devdax = 0;
void *slowmem_devdax = 0;

void mem_setup() {
    // create mock fastmem / slowmem regions with mmaping the same devdax file

    memfd = open(FASTMEM_DEVDAX_PATH, O_RDWR);
    if (memfd < 0) {
        perror("fastmem open");
    }
    assert(memfd >= 0);

    fastmem_devdax = mmap(NULL, MEM_SZ, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, memfd, 0);
    if (fastmem_devdax == MAP_FAILED) {
        perror("dram devdax mmap");
        assert(0);
    }

    slowmem_devdax = mmap(NULL, MEM_SZ, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, memfd, MEM_SZ);
    if (slowmem_devdax == MAP_FAILED) {
        perror("nvm devdax mmap");
        assert(0);
    }
}

void uffd_wp_register(uint64_t start, uint64_t len) {
    struct uffdio_register uffdio_register;

    uffdio_register.range.start = start;
    uffdio_register.range.len = len;
    uffdio_register.mode = UFFDIO_REGISTER_MODE_WP;
    uffdio_register.ioctls = 0;
    if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
        perror("ioctl uffdio_register");
        assert(0);
    }
}

void uffd_setup() {
    // first, create uffd
    uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
    if (uffd == -1) {
        perror("uffd");
        assert(0);
    }

    // register APIs of interest
    struct uffdio_api uffdio_api;
    uffdio_api.api = UFFD_API;
    uffdio_api.features = UFFD_FEATURE_PAGEFAULT_FLAG_WP;
    uffdio_api.ioctls = 0;
    if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
        perror("ioctl uffdio_api");
        assert(0);
    }

    // finally, register our memory regions
    uffd_wp_register((uint64_t)fastmem_devdax, MEM_SZ);
    uffd_wp_register((uint64_t)slowmem_devdax, MEM_SZ);
}

void bench_setup() {
    mem_setup();
    uffd_setup();
    printf("bench setup success\n");
}

void wp_region(void *address, uint64_t len, bool protect) {
    struct uffdio_writeprotect wp;
    int ret;
    uint64_t addr = (uint64_t)address;

    assert(addr != 0);
    assert(addr % HUGEPAGE_SIZE == 0);

    wp.range.start = addr;
    wp.range.len = len;
    wp.mode = (protect ? UFFDIO_WRITEPROTECT_MODE_WP : 0);
    ret = ioctl(uffd, UFFDIO_WRITEPROTECT, &wp);

    if (ret < 0) {
        perror("uffdio writeprotect");
        assert(0);
    }
}

void *__remap_page(uint64_t va, uint64_t devdax_offset, bool is_fastmem,
                   bool register_uffd) {
    void *newptr;
    uint64_t pagesize;
    int fd;

    pagesize = HUGEPAGE_SIZE;

    if (is_fastmem) {
        fd = memfd;
    } else {
        fd = memfd;
    }

    if (!is_fastmem) {
        devdax_offset += MEM_SZ;
    }

    newptr = mmap((void *)va, pagesize, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, devdax_offset);
    if (newptr == MAP_FAILED) {
        perror("newptr mmap");
        assert(0);
    }
    assert((uint64_t)newptr != 0);
    assert((uint64_t)newptr % pagesize == 0);

    // re-register new mmap region with userfaultfd
    if (register_uffd) {
        uffd_wp_register((uint64_t)newptr, pagesize);
    }

    return newptr;
}

void *remap_page_no_uffd_register(uint64_t va, uint64_t devdax_offset,
                                  bool is_fastmem) {
    return __remap_page(va, devdax_offset, is_fastmem, false);
}

void remap_page(uint64_t va, uint64_t devdax_offset, bool is_fastmem) {
    __remap_page(va, devdax_offset, is_fastmem, true);
}

void bench_wp_hugepage(void) {
    static const size_t kBenchIters = MEM_SZ / HUGEPAGE_SIZE;
    static const char *kBenchName = "bench 2mb write-protection";

    uint64_t measurements[kBenchIters];
    for (size_t i = 0; i < kBenchIters; i++) {
        uint64_t start = get_ns();
        wp_region(fastmem_devdax + i * HUGEPAGE_SIZE, HUGEPAGE_SIZE, true);
        uint64_t end = get_ns();
        measurements[i] = end - start;
    }

    wp_region(fastmem_devdax, MEM_SZ, false);

    postbench(kBenchName, measurements, kBenchIters);
}

void *setup_virtmem_on_slowmem() {
    void *virt_mem = mmap(NULL, MEM_SZ, PROT_READ | PROT_WRITE,
                          MAP_SHARED | MAP_POPULATE, memfd, MEM_SZ);
    if (virt_mem == MAP_FAILED) {
        perror("virt mmap");
        assert(0);
    }

    uffd_wp_register((uint64_t)virt_mem, MEM_SZ);

    return virt_mem;
}

void bench_remap(void) {
    static const size_t kBenchIters = MEM_SZ / HUGEPAGE_SIZE;
    static const char *kBenchName = "bench 2mb remap";

    // this mocks an application mmap that is allocated to the slow mem.
    void *virt_mem = setup_virtmem_on_slowmem();

    // remaps all pages in virtmem to fastmem.
    uint64_t measurements[kBenchIters];
    for (size_t i = 0; i < kBenchIters; i++) {
        uint64_t start = get_ns();

        uint64_t va = (uint64_t)virt_mem + i * HUGEPAGE_SIZE;
        uint64_t devdax_offset = i * HUGEPAGE_SIZE;
        bool is_fastmem = true;
        remap_page(va, devdax_offset, is_fastmem);

        uint64_t end = get_ns();
        measurements[i] = end - start;
    }

    munmap(virt_mem, MEM_SZ);

    postbench(kBenchName, measurements, kBenchIters);
}

void bench_wp_migration(void) {
    static const size_t kBenchIters = MEM_SZ / HUGEPAGE_SIZE;
    static const char *kBenchName = "bench 2mb wp migration";

    uint64_t start, end;

    // this mocks an application mmap that is allocated to the slow mem.
    void *virt_mem = setup_virtmem_on_slowmem();

    uint64_t measurements[kBenchIters];
    uint64_t wp_time[kBenchIters];
    uint64_t memcpy_time[kBenchIters];
    uint64_t remap_time[kBenchIters];
    uint64_t uffd_register_time[kBenchIters];

    // migrate all pages in virtmem to fastmem.
    for (size_t i = 0; i < kBenchIters; i++) {
        start = get_ns();
        wp_region(virt_mem + i * HUGEPAGE_SIZE, HUGEPAGE_SIZE, true);
        end = get_ns();
        wp_time[i] = end - start;

        start = get_ns();
        dsa_memcpy(fastmem_devdax + i * HUGEPAGE_SIZE,
                   slowmem_devdax + i * HUGEPAGE_SIZE, HUGEPAGE_SIZE);
        end = get_ns();
        memcpy_time[i] = end - start;

        start = get_ns();
        uint64_t va = (uint64_t)virt_mem + i * HUGEPAGE_SIZE;
        uint64_t new_devdax_offset = i * HUGEPAGE_SIZE;
        bool is_fastmem = true;
        void *new_mapping =
            remap_page_no_uffd_register(va, new_devdax_offset, is_fastmem);
        end = get_ns();
        remap_time[i] = end - start;

        start = get_ns();
        uffd_wp_register((uint64_t)new_mapping, HUGEPAGE_SIZE);
        end = get_ns();
        uffd_register_time[i] = end - start;

        measurements[i] =
            wp_time[i] + memcpy_time[i] + remap_time[i] + uffd_register_time[i];
        // measurements[i] = memcpy_time[i];
    }

    munmap(virt_mem, MEM_SZ);

    postbench(kBenchName, measurements, kBenchIters);
    postbench("op: write-protection", wp_time, kBenchIters);
    postbench("op: memcpy", memcpy_time, kBenchIters);
    postbench("op: remap page", remap_time, kBenchIters);
    postbench("op: uffd wp-register", uffd_register_time, kBenchIters);
}

int main() {
    assert(dsa_init() == 0);

    bench_setup();
    // bench_wp_hugepage();
    // bench_remap();
    bench_wp_migration();

    dsa_shutdown();
}
