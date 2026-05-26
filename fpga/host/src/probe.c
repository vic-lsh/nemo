#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define SLOWMEM_PATH "/dev/dax1.0"
#define GB (1024L * 1024L * 1024L)
//#define SLOWMEM_SIZE (1550L * GB / 100L)
#define SLOWMEM_SIZE (3100L * GB / 100L)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

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

#define PAGE_SHIFT_4K 12
#define PAGE_SIZE_4K (1UL << PAGE_SHIFT_4K)
#define PFN_MASK ((1ULL << 55) - 1)

uintptr_t virt_to_phys(void *virt_addr) {
    int fd;
    uintptr_t vaddr = (uintptr_t)virt_addr;
    uintptr_t paddr = 0;

    fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) {
        perror("open pagemap");
        return 0;
    }

    off_t offset = (vaddr / PAGE_SIZE_4K) * sizeof(uint64_t);
    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("lseek");
        close(fd);
        return 0;
    }

    uint64_t entry;
    if (read(fd, &entry, sizeof(entry)) != sizeof(entry)) {
        perror("read");
        close(fd);
        return 0;
    }

    close(fd);

    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "Page not present\n");
        return 0;
    }

    uint64_t pfn = entry & PFN_MASK;

    // Physical address is PFN * base-page-size + offset-within-page
    // Works regardless of 4K/2M/1G, because PFN always indexes 4K units
    paddr = (pfn << PAGE_SHIFT_4K) | (vaddr & (PAGE_SIZE_4K - 1));

    return paddr;
}

int main() {
    // Size: 16GB in bytes
    //size_t size = 16ULL * 1024 * 1024 * 1024;

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

    // FILE* file = fopen("numbers.txt", "w");

    printf("Successfully mapped 16GB of memory at %p\n", addr);
    //volatile char* p = addr;
    volatile char* memory_space = addr; // must be volatile to prevent loop optimizations
    uint64_t min, max;
    min = -1;
    max = 0;

    printf("Register Interface Ready. Use 'flush' or 'r <# bytes> <addr>' or 'w <# bytes> <addr> <data>' (type 'exit' to quit)\n");
    uint64_t paddr_base = (uint64_t)virt_to_phys((void*)&memory_space[0]);
    uint64_t paddr_16G = (uint64_t)virt_to_phys((void*)&memory_space[16lu * GB]);
	printf("physical address of mem_space[0]: 0x%lx\n", paddr_base);
	printf("physical address of mem_space[16G]: 0x%lx\n", paddr_16G);
#define INPUT_LINE_SIZE 128
    char line[INPUT_LINE_SIZE];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            break; // EOF or error
        }

        // Remove trailing newline
        line[strcspn(line, "\n")] = 0;

        if (strcmp(line, "exit") == 0) {
            break;
        }

        char *cmd = strtok(line, " \t");
        if (!cmd) {
            continue;
        }

        if (strcmp(cmd, "r") == 0) {
            char *numbytes_str = strtok(NULL, " \t");
            char *addr_str = strtok(NULL, " \t");
            if (!numbytes_str || !addr_str) {
                printf("Usage: r <# bytes> <addr>\n");
                continue;
            }

            char *endptr0, *endptr1;
            uint64_t numbytes = strtoull(numbytes_str, &endptr0, 0);
            uint64_t addr = strtoull(addr_str, &endptr1, 0);

            if (*endptr0 != '\0' || *endptr1 != '\0'){// || addr >= SLOWMEM_SIZE) {
                printf("Error: Invalid, unaligned, or out-of-bounds address.\n");
                continue;
            }

            for (uint64_t i = 0; i < numbytes; i++) {
                uint8_t data = memory_space[addr + i];
                if (numbytes <= 64 || i == 0 || i == numbytes-1) {
                    printf("memory_space[%lu] = 0x%02x\n", addr + i, data);
                }
            }
            min = MIN(min, addr);
            max = MAX(max, addr + numbytes);

        } else if (strcmp(cmd, "w") == 0) {
            char *numbytes_str = strtok(NULL, " \t");
            char *addr_str = strtok(NULL, " \t");
            char *data_str = strtok(NULL, " \t");

            if (!numbytes_str || !addr_str || !data_str) {
                printf("Usage: w <# bytes> <addr> <data>\n");
                continue;
            }

            char *endptr0, *endptr1, *endptr2;
            uint64_t numbytes = strtoull(numbytes_str, &endptr0, 0);
            uint64_t addr = strtoull(addr_str, &endptr1, 0);
            uint8_t data = (uint8_t)strtoull(data_str, &endptr2, 0);

            if (*endptr0 != '\0' || *endptr1 != '\0' || *endptr2 != '\0'){// || addr >= SLOWMEM_SIZE) {
                printf("Error: Invalid, unaligned, or out-of-bounds address or data.\n");
                continue;
            }

            for (uint64_t i = 0; i < numbytes; i++) {
                memory_space[addr + i] = data;
            }
            printf("Wrote 0x%02x to memory_space[%lu] - memory_space[%lu]\n", data, addr, addr + numbytes);

            min = MIN(min, addr);
            max = MAX(max, addr + numbytes);

        } else if (strcmp(cmd, "flush") == 0) {
            // flush min - max
            if (min == -1 && max == 0) continue;
            mem_flush(&memory_space[min], max - min);
            printf("Flushed %lu - %lu\n", min, max);
            min = -1;
            max = 0;
        } else {
            //printf("Unknown command. Use 'r <addr>' or 'w <addr> <data>'\n");
            printf("Unknown command.\n");
        }
    }



    // Unmap the memory (optional, as program exit will clean up)
    if (munmap(addr, SLOWMEM_SIZE) == -1) {
        perror("munmap failed");
        return 1;
    }

    printf("Memory unmapped successfully\n");
    return 0;
}
