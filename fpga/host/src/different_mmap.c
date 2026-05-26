#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <immintrin.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>

#define GB (1024L * 1024L * 1024L)
#define SLOWMEM_SIZE (128L * GB)
//#define SLOWMEM_SIZE (1L * GB)
#define TARGET 0x4080000000
#define MEM_START (0L * GB)
#define MEM_END (MEM_START + (32 * GB))
//#define MEM_END (MEM_START + (SLOWMEM_SIZE))
//#define REPEAT (8)

//#define WRAP_AROUND (0x8000000000000000L)
//#define TIMES_FACTOR (3L)

#define WRITE_EVERY (262144) // times 8 bytes

#define KB (1L << 10)
#define MB (1L << 20)

#define DISPLAY_UPDATES 1

int main(int argc, char ** argv) {

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("fd invalid. run with sudo?\n");
        return 1;
    }

    // Allocate memory using mmap
    void* addr1 = mmap(
        NULL,                         // Let the kernel choose the address
        16lu * GB,
        PROT_READ | PROT_WRITE,       // Read and write permissions
        MAP_SHARED,// | MAP_POPULATE,
        fd,
        TARGET
    );

    void* addr2 = mmap(
        NULL,                         // Let the kernel choose the address
        16lu * GB,
        PROT_READ | PROT_WRITE,       // Read and write permissions
        MAP_SHARED,// | MAP_POPULATE,
        fd,
        TARGET + (16lu * GB)
    );



    if (addr1 == MAP_FAILED) {
        perror("mmap1 failed");
        return 1;
    }
    if (addr2 == MAP_FAILED) {
        perror("mmap1 failed");
        return 1;
    }

    printf("Successfully mapped memory at %p %p\n", addr1, addr2);
    volatile uint64_t* p1 = addr1;
    volatile uint64_t* p2 = addr2;


    for (uint64_t addr = 0; addr < 16lu * GB / sizeof(uint64_t); addr += 1lu * GB / sizeof(uint64_t)) {
        p1[addr] = addr;
    }
    for (uint64_t addr = 0; addr < 16lu * GB / sizeof(uint64_t); addr += 1lu * GB / sizeof(uint64_t)) {
        p2[addr] = addr + 16lu * GB / sizeof(uint64_t);
    }
    for (uint64_t addr = 0; addr < 16lu * GB / sizeof(uint64_t); addr += 1lu * GB / sizeof(uint64_t)) {
        printf("p1 u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld\n", addr * sizeof(uint64_t), addr, p1[addr]);
    }
    for (uint64_t addr = 0; addr < 16lu * GB / sizeof(uint64_t); addr += 1lu * GB / sizeof(uint64_t)) {
        printf("p2 u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld\n", addr * sizeof(uint64_t), addr, p2[addr]);
    }

    return 0;
}
