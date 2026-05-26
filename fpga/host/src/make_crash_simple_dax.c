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

#define SLOWMEM_PATH "/dev/dax1.0"

#define GB (1024L * 1024L * 1024L)
//#define SLOWMEM_SIZE (256L * GB)
//#define SLOWMEM_SIZE (16L * GB)
#define SLOWMEM_SIZE (64L * GB)
//#define TARGET 0x4080000000
#define MEM_START (0L * GB)
//#define MEM_END (MEM_START + (32 * GB))
#define MEM_END (MEM_START + (SLOWMEM_SIZE))
//#define REPEAT (8)

//#define WRAP_AROUND (0x8000000000000000L)
//#define TIMES_FACTOR (3L)

#define WRITE_EVERY (262144) // times 8 bytes
//#define WRITE_EVERY (134217728)

#define KB (1L << 10)
#define MB (1L << 20)

#define DISPLAY_UPDATES 1

int main(int argc, char ** argv) {

    int slowmem_fd = open(SLOWMEM_PATH, O_RDWR);
        if (slowmem_fd < 0) {
        printf("fd invalid. run with sudo?\n");
        return 1;
    }

    // Allocate memory using mmap
    void* addr = mmap(
        NULL,                         // Let the kernel choose the address
        SLOWMEM_SIZE,
        PROT_READ | PROT_WRITE,       // Read and write permissions
        MAP_SHARED | MAP_POPULATE,
        slowmem_fd,
        0    // Offset (not used with MAP_ANONYMOUS)
    );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    printf("Successfully mapped memory at %p\n", addr);
    volatile uint64_t* p = addr;

#if DISPLAY_UPDATES
    printf("Displaying updates\n");
#endif

#define N_GB (SLOWMEM_SIZE / GB)
    struct gb_mapping {
        bool dst_gb_found[N_GB];
    };
    struct gb_mapping mappings[N_GB];
    for (uint64_t src = 0; src < N_GB; src++) {
        for (uint64_t dst = 0; dst < N_GB; dst++) {
            mappings[src].dst_gb_found[dst] = 0;
        }
    }

    for (uint64_t i = MEM_START; i < MEM_END; i += sizeof(uint64_t) * WRITE_EVERY) {
        uint64_t addr = i / sizeof(uint64_t);
        uint64_t valu = addr;
        p[addr] = valu;
#if DISPLAY_UPDATES
        if ((addr % (1 * MB)) == 0) {
            fprintf(stderr, "w 0x%lx 0x%lx\n", addr, valu);
            fflush(stderr);
        }
#endif

    }

    for (uint64_t i = MEM_START; i < MEM_END; i += sizeof(uint64_t) * WRITE_EVERY) {
        uint64_t src_gb = i / GB;
        uint64_t addr = i / sizeof(uint64_t);
        //uint64_t x = addr;
        uint64_t a = p[addr];
        uint64_t dst_gb = a * sizeof(uint64_t) / GB;

#if DISPLAY_UPDATES
        if ((addr % (1 * MB)) == 0) {
            fprintf(stderr, "r 0x%lx 0x%lx\n", addr, a);
            fflush(stderr);
        }
#endif


        
        mappings[src_gb].dst_gb_found[dst_gb] = 1;
        /*
        if (x != a) {
            printf("Mismatch on i=%ld addr=0x%lx expected=0x%lx actual=0x%lx\n", i, i, x, a);
            exit(1);
        }
        */
    }
    for (uint64_t src = 0; src < N_GB; src++) {
        printf("GB %ld maps to: ", src);
        for (uint64_t dst = 0; dst < N_GB; dst++) {
            if (mappings[src].dst_gb_found[dst]) {
                printf("%ld ", dst);
            }
        }
        printf("\n");
    }
 

    // Unmap the memory (optional, as program exit will clean up)
    if (munmap(addr, SLOWMEM_SIZE) == -1) {
        perror("munmap failed");
        return 1;
    }

    printf("Memory unmapped successfully\n");
    return 0;
}
