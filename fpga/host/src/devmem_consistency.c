#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

//#define PAGE_SIZE 4096
//#define MAP_SIZE PAGE_SIZE
//#define MAP_SIZE (34359738368lu)
#define GB (1024L * 1024L * 1024L)
//#define MAP_SIZE (128lu * GB)
#define MAP_SIZE (16lu * GB)
#define MEM_START (0L * GB)
#define MEM_END (MEM_START + (MAP_SIZE))

// https://stackoverflow.com/questions/11277984/how-to-flush-the-cpu-cache-in-linux-from-a-c-program
void mem_flush(const volatile void *p, uint64_t allocation_size){
    const uint64_t cache_line = 64;
    const char *cp = (const char *)p;
    uint64_t i = 0;

    if (p == NULL || allocation_size <= 0)
            return;

    asm volatile("mfence\n\t"
                 :
                 :
                 : "memory");

    for (i = 0; i < allocation_size; i += cache_line) {
            asm volatile("clflush (%0)\n\t"
                         : 
                         : "r"(&cp[i])
                         : "memory");
    }

    asm volatile("mfence\n\t"
                 :
                 :
                 : "memory");
}

int main() {
    int fd;
    void *mapped_base;
    //off_t target = 0x40d0200000; // Physical address to access
    off_t target = 0x4080000000; // Physical address to access
    //off_t target = 0x4090200000; // Physical address to access
    //off_t target = 0;

    //fd = open("/dev/pmem1", O_RDWR | O_SYNC);
    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        printf("Error opening /dev/mem: %s\n", strerror(errno));
        return -1;
    }

    mapped_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                       //target & ~(MAP_SIZE - 1));
                        target);

    if (mapped_base == (void *)-1) {
        printf("Error mapping memory: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    //volatile uint64_t* physmem = (uint64_t*)mapped_base;

    char* p = mapped_base;

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
    return 0;
}
