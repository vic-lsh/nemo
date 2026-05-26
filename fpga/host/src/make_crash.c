#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE 4096
#define MAP_SIZE PAGE_SIZE

#define MB (1024L * 1024L)
#define GB (1024L * 1024L * 1024L)
//#define SLOWMEM_SIZE (1550L * GB / 100L)
//#define SLOWMEM_SIZE (16L * GB)
#define SLOWMEM_SIZE (16L * GB)
//#define MEM_START (1L * GB)
//#define MEM_START (0L * GB)
#define MEM_START (15L * GB)
//#define MEM_END (MEM_START + (1L * GB))
#define MEM_END (MEM_START + (8L * MB))
//#define SLOWMEM_SIZE (155 * (1024L * 1024L * 1024L) / 100)

#define TARGET 0x4080000000
//#define TARGET 0x4090200000

int main() {
    // Size: 16GB in bytes
    //size_t size = 16ULL * 1024 * 1024 * 1024;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        printf("Error opening /dev/mem: %s\n", strerror(errno));
        printf("fd invalid. run with sudo?\n");
        return 1;
    }

    // Allocate memory using mmap
    void* addr = mmap(
        NULL,                         // Let the kernel choose the address
        SLOWMEM_SIZE,
        PROT_READ | PROT_WRITE,       // Read and write permissions
        MAP_SHARED | MAP_POPULATE,
        fd,
        TARGET
    );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    printf("Successfully mapped 16GB of memory at %p\n", addr);
    //volatile char* p = addr;
    char* p = addr;

    // Fill the entire region with 0xdd
    printf("Setting memory. This may take some time...\n");
    for (size_t i = MEM_START; i < MEM_END; i++) {
        p[i] = (uint8_t)i;
    }
    //memset((void*)p, test_val, SLOWMEM_SIZE);
    printf("Memory set complete.\n");

    unsigned long fail_count = 0;
    unsigned long correct_count = 0;
    int all_correct = 1;
    for (size_t i = MEM_START; i < MEM_END; i++) {
        uint8_t val = p[i];
        uint8_t correct = i;
        if (val != correct) {
            all_correct = 0;
            fail_count++;
	    if (fail_count < 1000) printf("Addr %ld expected %d got %d\n", i, correct, val);
        } else {
            if (correct_count < 1000) printf("Successfully got Addr %ld value %d\n", i, correct);
            correct_count++;
        }
        // if (val != 0) printf("failed at %lu output %d\n", i, val);
        // fprintf(file, "%u\n", val);
	    /*
        printf("p[%ld] == %d\n", i, val);
	    if (i == 10000) break;
        */
    }

    if (all_correct) {
        printf("Verification successful\n");
    } else {
        printf("Verification failed count %lu correct %lu\n", fail_count,
               correct_count);
    }

    // Unmap the memory (optional, as program exit will clean up)
    if (munmap(addr, SLOWMEM_SIZE) == -1) {
        perror("munmap failed");
        return 1;
    }

    printf("Memory unmapped successfully\n");
    return all_correct;
}
