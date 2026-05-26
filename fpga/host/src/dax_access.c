#include <emmintrin.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

// Ensure compatibility with _mm_stream_si64, which is for 64-bit integers
#if defined(__GNUC__) || defined(__clang__)
#define MM_STREAM_SI64 _mm_stream_si64
#else
#error "Compiler not supported for this example."
#endif

#define KB (1UL << 10)
#define MB (1UL << 20)
#define GB (1UL << 30)

#define DEVICE_PATH "/dev/dax1.0"

// Ensure this is >= the DAX device page size
#define MAP_SIZE (16 * GB)

#define DEFAULT_WRITE_VAL 0xfffffffaaaaaaaaa

void non_temporal_write_ul(unsigned long *dst, unsigned long value) {
    // Use _mm_stream_si64 for non-temporal store of 64-bit value
    MM_STREAM_SI64((long long int *)dst, (long long int)value);
}

// Function to perform non-temporal load of unsigned long
unsigned long non_temporal_load(const unsigned long *addr) {
    __m128i wide = _mm_loadu_si64((void const *)addr);
    return (unsigned long)_mm_cvtsi128_si64(wide);
}

void non_temporal_write_loop(void *cxl_mem) {
    // uint num_iters = MAP_SIZE / sizeof(unsigned long);
    volatile unsigned long *region = cxl_mem;

    printf("Starting write...\n");
    size_t start_oft = (2UL * GB) / sizeof(unsigned long);
    size_t pg_iters = (2UL * MB) / sizeof(unsigned long);
    start_oft = start_oft - pg_iters;

    for (uint i = start_oft; i < start_oft + 16; i++) {
        printf("Writing %u\n", i);
        non_temporal_write_ul((unsigned long *)&region[i], DEFAULT_WRITE_VAL);
        sleep(1);
    }
    printf("Finished writing...\n");

    start_oft = (0UL * GB) / sizeof(unsigned long);
    // while (1) {
    for (uint i = 0; i < 16; i = i + 1) {
        printf("Reading at offset %d ", i);
        unsigned long val = non_temporal_load((unsigned long *)&region[i]);
        printf("val %lu\n", val);
        // sleep(1);
    }
    // }
}

int main() {
    int cxl_dax_dev;
    void *cxl_mem;

    // Open the device file
    cxl_dax_dev = open(DEVICE_PATH, O_RDWR);
    if (cxl_dax_dev == -1) {
        perror("Error opening device file");
        return EXIT_FAILURE;
    }

    // Map the device memory into our address space
    cxl_mem = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
                   cxl_dax_dev, 0);
    if (cxl_mem == MAP_FAILED) {
        perror("Error mapping device memory");
        close(cxl_dax_dev);
        return EXIT_FAILURE;
    }
    printf("mmap successful\n");

    non_temporal_write_loop(cxl_mem);

    printf("run successful\n");

    // Clean up
    if (munmap(cxl_mem, MAP_SIZE) == -1) {
        perror("Error unmapping memory");
    }

    close(cxl_dax_dev);
    return EXIT_SUCCESS;
}
