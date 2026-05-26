#include <numa.h>
#include <numaif.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define KB (1 << 10)
#define MB (1 << 20)
#define GB (1 << 30)

int main() {
    size_t size = 1 * GB;
    void *cxl_mem = NULL;

    // Check if NUMA is available and if NUMA node 1 is valid
    if (numa_available() < 0) {
        fprintf(stderr, "NUMA is not available on this system.\n");
        return EXIT_FAILURE;
    }

    if (numa_max_node() < 1) {
        fprintf(stderr, "NUMA node 1 is not available.\n");
        return EXIT_FAILURE;
    }

    cxl_mem = numa_alloc_onnode(size, 1);
    if (cxl_mem == NULL) {
        perror("numa_alloc_onnode");
        return EXIT_FAILURE;
    }
    printf("allocated %lu bytes on numa node 1\n", size);

    char *local_mem = numa_alloc_onnode(size, 0);
    if (!local_mem) {
        fprintf(stderr, "malloc failed\n");
        return EXIT_FAILURE;
    }

    char read = 0;
    while (1) {
        clock_t start_time = clock();
        clock_t end_time = start_time + (CLOCKS_PER_SEC * 10);
        if (read) {
            printf("reading...\n");
            while (clock() < end_time) {
                for (int i = 0; i < size; i++) {
                    local_mem[i] = ((char *)cxl_mem)[i];
                }
            }
        } else {
            printf("writing...\n");
            while (clock() < end_time) {
                for (int i = 0; i < size; i++) {
                    ((char *)cxl_mem)[i] = i;
                }
            }
        }
        read = !read;
    }

    // Free the allocated memory
    numa_free(cxl_mem, size);

    return EXIT_SUCCESS;
}
