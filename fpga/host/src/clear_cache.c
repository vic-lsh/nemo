#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define MULTIPLIER 4
#define CACHE_SYSFS_PATH "/sys/devices/system/cpu/cpu0/cache/index3/size"

size_t get_llc_size_bytes() {
    FILE *f = fopen(CACHE_SYSFS_PATH, "r");
    if (!f) {
        perror("Failed to open LLC size file");
        return 0;
    }

    char buf[32];
    if (!fgets(buf, sizeof(buf), f)) {
        perror("Failed to read LLC size");
        fclose(f);
        return 0;
    }
    fclose(f);

    size_t size;
    char unit;
    if (sscanf(buf, "%zu%c", &size, &unit) != 2) {
        fprintf(stderr, "Failed to parse LLC size string: %s\n", buf);
        return 0;
    }

    if (unit == 'K') return size * 1024;
    if (unit == 'M') return size * 1024 * 1024;

    fprintf(stderr, "Unknown unit in LLC size: %c\n", unit);
    return 0;
}

int main() {
    size_t llc_size = get_llc_size_bytes();
    if (llc_size == 0) {
        fprintf(stderr, "LLC size could not be determined.\n");
        return 1;
    }

    size_t buffer_size = llc_size * MULTIPLIER;
    printf("Detected LLC size: %zu bytes\n", llc_size);
    printf("Allocating %zu bytes to flush LLC\n", buffer_size);

    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) {
        perror("malloc failed");
        return 1;
    }

    volatile uint64_t sum = 0;
    size_t stride = 64; // typical cache line size

    for (size_t i = 0; i < buffer_size; i += stride) {
        sum += buffer[i];
    }

    printf("Eviction sum: %lu\n", sum);

    free((void *)buffer);
    return 0;
}
