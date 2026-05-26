#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/**
 * MMAP test that first makes a big mmap region of anon memory, then create a
 * new MMAP_FIXED mapping into a small slice of the first mmap.
 *
 * Note that the second MMAP_FIXED region only needs to be 4KiB page-aligned in
 * the application-provided arguments.
 *
 * The expected behavior is that the MMAP_FIXED region should share the memory
 * with the previous mmap. The new fixed mapping should see memory content made
 * in the prior mapping, and the prior mapping should see new memory writes
 * via the fixed mapping.
 *
 * This mmap test was first driven by crashes in the Go runtime, when we run
 * Go application (e.g., the docker runtime).
 */

#define GiB (1024 * 1024 * 1024)
#define MiB (1024 * 1024)

int main() {
    void *base_addr;
    void *fixed_addr;
    size_t base_size = 1 * GiB;
    size_t offset = 2 * MiB;

    // when the MAP_FIXED size is below a hugepage, it may cause mapping error
    // within our library. Test that this doesn't happen.
    size_t fixed_size = 131072;

    printf("Starting memory mapping demonstration...\n");
    printf("Page size: %ld bytes\n", sysconf(_SC_PAGESIZE));

    printf("\nStep 1: Mapping 1 GiB of anonymous private memory...\n");
    base_addr = mmap(NULL, base_size, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (base_addr == MAP_FAILED) {
        perror("Failed to mmap base memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully mapped 1 GiB at address: %p\n", base_addr);
    printf("Address range: %p - %p\n", base_addr,
           (char *)base_addr + base_size - 1);

    void *target_addr = (char *)base_addr + offset;
    printf("\nStep 2: Creating fixed mapping at offset 2 MiB...\n");
    printf("Target address for fixed mapping: %p\n", target_addr);

    // Write to fixed mapping
    int test_value = 0xCCAACCAA;
    *(int *)target_addr = test_value;

    fixed_addr = mmap(target_addr, fixed_size, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (fixed_addr == MAP_FAILED) {
        perror("Failed to mmap fixed memory");
        if (munmap(base_addr, base_size) == -1) {
            perror("Failed to unmap base memory");
        }
        exit(EXIT_FAILURE);
    }

    printf("Successfully created fixed mapping at: %p\n", fixed_addr);

    // Verify that the fixed mapping is at the expected location
    if (fixed_addr != target_addr) {
        printf("Error: Fixed mapping not at expected address!\n");
        printf("Expected: %p, Got: %p\n", target_addr, fixed_addr);
        assert(0);
    } else {
        printf("Fixed mapping is at the correct offset within base mapping.\n");
    }

    printf("\nStep 3: Testing the memory mappings...\n");

    // Write to base mapping
    *(int *)base_addr = 0xDEADBEEF;
    printf("Wrote 0xDEADBEEF to base mapping\n");

    // Read back values
    printf("Value at base mapping: 0x%X\n", *(int *)base_addr);

    int read_value = *(int *)fixed_addr;
    if (read_value != test_value) {
        printf("error: failed to read back the value written.\n");
        printf("expected 0x%x found 0x%x\n", test_value, read_value);
        exit(1);
    }

    printf("\nMemory layout:\n");
    printf("Base mapping:  %p - %p (size: %zu bytes)\n", base_addr,
           (char *)base_addr + base_size - 1, base_size);
    printf("Fixed mapping: %p - %p (size: %zu bytes)\n", fixed_addr,
           (char *)fixed_addr + fixed_size - 1, fixed_size);
    printf("Offset from base: %ld bytes (%ld MiB)\n",
           (char *)fixed_addr - (char *)base_addr,
           ((char *)fixed_addr - (char *)base_addr) / MiB);

    printf("\nCleaning up...\n");

    if (munmap(base_addr, base_size) == -1) {
        perror("Failed to unmap base memory");
        exit(EXIT_FAILURE);
    }

    printf("Successfully unmapped all memory.\n");
    printf("Program completed successfully.\n");

    return 0;
}
