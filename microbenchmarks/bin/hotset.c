#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#ifndef likely
# define likely(x)   __builtin_expect(!!(x), 1)
# define unlikely(x) __builtin_expect(!!(x), 0)
#endif

typedef struct {
    uint64_t *base;
    uint64_t  n_elems;          // total elements
    uint64_t  hot_begin;        // inclusive
    uint64_t  hot_len;          // elements in hot
    uint64_t  cold_begin;       // inclusive
    uint64_t  cold_len;         // elements in cold
    double    hot_prob;         // 0..1 (0.9)
    int       seconds;          // per phase
    int       tid;
    pthread_barrier_t *bar;
    volatile uint64_t sink;

    uint64_t *op_counter;       // pointer to per-thread counter
} worker_args_t;

typedef struct {
    int threads;
    uint64_t *op_counters;
} reporter_args_t;

static volatile int run_progress = 1; // reporter runs while this is 1

// Simple per-thread xorshift64* RNG
static inline uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 2685821657736338717ULL;
}

// Timer helper
static inline int time_not_expired(struct timespec start, int seconds) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    time_t ds = now.tv_sec - start.tv_sec;
    long   dn = now.tv_nsec - start.tv_nsec;
    if (dn < 0) { ds -= 1; dn += 1000000000L; }
    return ds < seconds;
}

// Prefault pages
static void prefault_pages(uint8_t *p, size_t bytes) {
    const size_t page = sysconf(_SC_PAGESIZE);
    for (size_t off = 0; off < bytes; off += page) {
        p[off] = (uint8_t)(off >> 12);
    }
    p[bytes - 1] ^= 1;
}

static void set_phase(worker_args_t *a, double hot_frac, int hot_is_front) {
    uint64_t hot_len = (uint64_t)((double)a->n_elems * hot_frac);
    if (hot_len == 0) hot_len = 1;
    if (hot_len > a->n_elems) hot_len = a->n_elems;

    if (hot_is_front) {
        a->hot_begin  = 0;
    } else {
        a->hot_begin  = a->n_elems - hot_len;
    }
    a->hot_len   = hot_len;

    if (hot_is_front) {
        a->cold_begin = a->hot_begin + a->hot_len;
        a->cold_len   = a->n_elems - a->hot_len;
    } else {
        a->cold_begin = 0;
        a->cold_len   = a->n_elems - a->hot_len;
    }
    if (a->cold_len == 0) { a->cold_len = 1; }
}

// Extracted hot/cold loop for one phase
static void run_phase(worker_args_t *a, int seconds) {
    uint64_t seed = 0x9e3779b97f4a7c15ULL ^ ((uint64_t)a->tid << 32) ^ (uint64_t)time(NULL);
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    uint64_t local_ops = 0;

    while (time_not_expired(start, seconds)) {
        uint64_t r = rng_next(&seed);
        int hot = (r % 10) < 9; // 90% hot

        uint64_t idx;
        if (likely(hot)) {
            uint64_t off = rng_next(&seed) % a->hot_len;
            idx = a->hot_begin + off;
        } else {
            uint64_t off = rng_next(&seed) % a->cold_len;
            idx = a->cold_begin + off;
        }

        // RMW to avoid DCE
        uint64_t val = a->base[idx];
        val ^= (r | 1ULL);
        a->base[idx] = val;
        a->sink ^= val;

        local_ops++;
        if ((local_ops & 0xFFFF) == 0) { // batched flush
            __atomic_fetch_add(a->op_counter, local_ops, __ATOMIC_RELAXED);
            local_ops = 0;
        }
    }

    if (local_ops > 0) {
        __atomic_fetch_add(a->op_counter, local_ops, __ATOMIC_RELAXED);
    }
}

// Worker: two phases with barrier between
static void* worker(void *arg) {
    worker_args_t *a = (worker_args_t*)arg;

    // Phase 1: hot = front 10%
    set_phase(a, 0.10, 1);
    pthread_barrier_wait(a->bar);
    run_phase(a, a->seconds);
    
    printf("Begin phase 2\n");

    // Phase 2: hot = tail 20%
    set_phase(a, 0.20, 0);
    pthread_barrier_wait(a->bar);
    run_phase(a, a->seconds);

    return NULL;
}

// Reporter: print aggregate throughput every second
static void* progress_reporter(void *arg) {
    reporter_args_t *ra = (reporter_args_t*)arg;
    uint64_t last_total = 0;
    int elapsed = 0;

    while (run_progress) {
        sleep(1);
        elapsed += 1;

        uint64_t total = 0;
        for (int i = 0; i < ra->threads; i++) {
            total += __atomic_load_n(&ra->op_counters[i], __ATOMIC_RELAXED);
        }

        uint64_t delta = total - last_total;    // ops in the last second
        double mops = (double)delta / 1e6;      // million ops per second
        printf("[+%ds] Throughput: %.2f MOPS (agg)\n", elapsed, mops);
        fflush(stdout);

        last_total = total;
    }
    return NULL;
}

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <size_gib> <threads> [seconds_per_phase]\n"
        "  size_gib          Total allocation size in GiB (e.g., 8)\n"
        "  threads           Number of worker threads (e.g., 8)\n"
        "  seconds_per_phase Optional; default 5 seconds per phase\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 3) { usage(argv[0]); return 1; }

    double size_gib = atof(argv[1]);
    int threads = atoi(argv[2]);
    int seconds = (argc >= 4) ? atoi(argv[3]) : 5;

    if (size_gib <= 0.0 || threads <= 0 || seconds <= 0) {
        usage(argv[0]); return 1;
    }

    const size_t bytes = (size_t)(size_gib * 1024.0 * 1024.0 * 1024.0);
    const uint64_t n_elems = bytes / sizeof(uint64_t);
    if (n_elems == 0) {
        fprintf(stderr, "Allocation too small.\n");
        return 1;
    }

    void *mem = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) {
        fprintf(stderr, "mmap(%zu bytes) failed: %s\n", bytes, strerror(errno));
        return 1;
    }
#ifdef MADV_HUGEPAGE
    madvise(mem, bytes, MADV_HUGEPAGE);
#endif

    prefault_pages((uint8_t*)mem, bytes);

    printf("Allocated %.2f GiB (%zu bytes, %llu elements of 8B)\n",
           size_gib, (size_t)bytes, (unsigned long long)n_elems);
    printf("Threads: %d | Seconds per phase: %d | Hot:Cold = 9:1 | Reporting: 1s\n",
           threads, seconds);

    pthread_t *t = (pthread_t*)malloc(sizeof(pthread_t) * (size_t)threads);
    worker_args_t *args = (worker_args_t*)calloc((size_t)threads, sizeof(worker_args_t));
    pthread_barrier_t bar;
    pthread_barrier_init(&bar, NULL, (unsigned)threads);

    uint64_t *op_counters = (uint64_t*)calloc(threads, sizeof(uint64_t));

    // Reporter thread (starts before workers; prints wall-time since reporter start)
    pthread_t reporter;
    reporter_args_t ra = { threads, op_counters };
    pthread_create(&reporter, NULL, progress_reporter, &ra);

    for (int i = 0; i < threads; i++) {
        args[i].base       = (uint64_t*)mem;
        args[i].n_elems    = n_elems;
        args[i].hot_prob   = 0.9;
        args[i].seconds    = seconds;
        args[i].tid        = i;
        args[i].bar        = &bar;
        args[i].sink       = 0;
        args[i].op_counter = &op_counters[i];
        int rc = pthread_create(&t[i], NULL, worker, &args[i]);
        if (rc != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
            return 1;
        }
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(t[i], NULL);
    }

    run_progress = 0;
    pthread_join(reporter, NULL);

    uint64_t chk = 0;
    for (int i = 0; i < threads; i++) chk ^= args[i].sink;
    printf("Checksum: 0x%016llx\n", (unsigned long long)chk);

    pthread_barrier_destroy(&bar);
    free(args);
    free(t);
    free(op_counters);
    munmap(mem, bytes);
    return 0;
}
