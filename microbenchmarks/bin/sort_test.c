#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define NUM_ITERATIONS 5
#define ELEMENT_SIZE sizeof(int)

// Comparison function for qsort
int compare_ints(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (ia > ib) - (ia < ib);
}

// Generate random data
void generate_random_data(int *data, size_t count) {
    printf("Generating %zu random integers...\n", count);
    srand(time(NULL));

    for (size_t i = 0; i < count; i++) {
        data[i] = rand();
    }
}

// Create a backup copy of original data for repeated sorting
void create_backup(int *original, int *backup, size_t count) {
    memcpy(backup, original, count * ELEMENT_SIZE);
}

// Restore data from backup
void restore_data(int *data, int *backup, size_t count) {
    memcpy(data, backup, count * ELEMENT_SIZE);
}

// Get current time in seconds
double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <data_size_in_GB>\n", argv[0]);
        fprintf(stderr, "Example: %s 1.5\n", argv[0]);
        return 1;
    }

    double gb_size = atof(argv[1]);
    if (gb_size <= 0) {
        fprintf(stderr, "Error: Data size must be positive\n");
        return 1;
    }

    // Calculate number of integers
    size_t total_bytes = (size_t)(gb_size * 1024 * 1024 * 1024);
    size_t num_elements = total_bytes / ELEMENT_SIZE;
    size_t actual_bytes = num_elements * ELEMENT_SIZE;

    printf("Sort Benchmark Configuration:\n");
    printf("- Requested size: %.2f GB\n", gb_size);
    printf("- Actual size: %.2f GB (%zu bytes)\n",
           actual_bytes / (1024.0 * 1024.0 * 1024.0), actual_bytes);
    printf("- Number of integers: %zu\n", num_elements);
    printf("- Iterations: %d\n\n", NUM_ITERATIONS);

    // Create anonymous memory mappings
    int *data = mmap(NULL, actual_bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int *backup = mmap(NULL, actual_bytes, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (data == MAP_FAILED || backup == MAP_FAILED) {
        perror("Anonymous mmap failed");
        if (data != MAP_FAILED) munmap(data, actual_bytes);
        if (backup != MAP_FAILED) munmap(backup, actual_bytes);
        return 1;
    }

    // Generate initial random data
    generate_random_data(data, num_elements);

    // Create backup for repeated sorting
    printf("Creating backup copy...\n");
    create_backup(data, backup, num_elements);

    printf("\nStarting benchmark...\n");
    printf("%-10s %-15s %-15s\n", "Iteration", "Time (seconds)", "Rate (MB/s)");
    printf("%-10s %-15s %-15s\n", "---------", "--------------", "-----------");

    double total_time = 0;
    double min_time = 1e9;
    double max_time = 0;

    for (int i = 0; i < NUM_ITERATIONS; i++) {
        // Restore original data
        restore_data(data, backup, num_elements);

        // Sort and measure time
        double start_time = get_time();
        qsort(data, num_elements, ELEMENT_SIZE, compare_ints);
        double end_time = get_time();

        double elapsed = end_time - start_time;
        double mb_per_sec = (actual_bytes / (1024.0 * 1024.0)) / elapsed;

        printf("%-10d %-15.3f %-15.1f\n", i + 1, elapsed, mb_per_sec);

        total_time += elapsed;
        if (elapsed < min_time) min_time = elapsed;
        if (elapsed > max_time) max_time = elapsed;

        // Verify sort worked (check first few elements)
        if (i == 0) {
            int is_sorted = 1;
            for (size_t j = 1; j < 100 && j < num_elements; j++) {
                if (data[j - 1] > data[j]) {
                    is_sorted = 0;
                    break;
                }
            }
            if (!is_sorted) {
                printf("Warning: Sort verification failed!\n");
            }
        }
    }

    // Print summary statistics
    double avg_time = total_time / NUM_ITERATIONS;
    double avg_rate = (actual_bytes / (1024.0 * 1024.0)) / avg_time;

    printf("\nBenchmark Results:\n");
    printf("- Average time: %.3f seconds\n", avg_time);
    printf("- Minimum time: %.3f seconds\n", min_time);
    printf("- Maximum time: %.3f seconds\n", max_time);
    printf("- Average rate: %.1f MB/s\n", avg_rate);
    printf("- Data throughput: %.1f million elements/s\n",
           (num_elements / 1e6) / avg_time);

    // Cleanup
    munmap(data, actual_bytes);
    munmap(backup, actual_bytes);

    printf("\nBenchmark completed successfully.\n");
    return 0;
}
