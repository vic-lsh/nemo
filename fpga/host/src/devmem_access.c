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
#define MAP_SIZE (128lu * GB)

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

    volatile uint64_t* physmem = (uint64_t*)mapped_base;
    //physmem[0] = 1;
    //physmem[2147483648lu] = 5;
    //mem_flush(&physmem[0], 16);
    /*
    printf("val %lx %lx\n", physmem[0], physmem[1]);
    printf("val %lx %lx\n", physmem[8], physmem[9]);
    printf("val %lx %lx\n", physmem[16], physmem[17]);
    printf("val %lx %lx\n", physmem[2147483648lu], physmem[2147483649lu]);
    printf("val %lx %lx\n", physmem[2147483648lu], physmem[2147483649lu]);
    //printf("val %lx %lx\n", physmem[120259084288lu], physmem[120259084288lu]);
    */

    /*
    int shift = 0;
    for (uint64_t addr = 1; addr < (MAP_SIZE/sizeof(uint64_t)); addr <<= 1) {
        printf("shift=%d, u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld %ld\n", shift, addr * sizeof(uint64_t), addr, physmem[addr], physmem[addr+1]);
        shift++;
    }
    */

    /*
    for (uint64_t addr = 0; addr < (MAP_SIZE / sizeof(uint64_t)); addr += 16lu * GB / sizeof(uint64_t)) {
        printf("u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld %ld\n", addr * sizeof(uint64_t), addr, physmem[addr], physmem[addr+1]);
    }
    */
    //for (uint64_t addr = 16lu * GB / sizeof(uint64_t); addr < 32lu * GB / sizeof(uint64_t); addr += 64lu / sizeof(uint64_t)) {
        //printf("u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld\n", addr * sizeof(uint64_t), addr, physmem[addr]);
    //}
    uint64_t addr = (16lu * GB + 0x696900lu) / sizeof(uint64_t);
    printf("u8 addr = 0x%lx, u64 addr=0x%lx, vals=%ld, ptr %lx\n", addr * sizeof(uint64_t), addr, physmem[addr], target + addr * sizeof(uint64_t));

    /*
    // Calculate the offset within the page
    off_t offset = target & (MAP_SIZE - 1);
    uint8_t *virt_addr = (uint8_t *)mapped_base + offset;

    printf("Memory contents at physical address 0x%lx:\n", target);
    for (int i = 0; i < 16; i++) {
        printf("Reading at pa 0x%lx ", target + i);
        printf("%02X\n", virt_addr[i]);
        // if ((i + 1) % 8 == 0)
        //     printf("\n");
    }
    */

    if (munmap(mapped_base, MAP_SIZE) == -1) {
        printf("Error un-mapping memory: %s\n", strerror(errno));
    }

    close(fd);
    return 0;
}
