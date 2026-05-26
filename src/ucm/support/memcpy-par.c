#include "memcpy-par.h"

#define _GNU_SOURCE

#include <assert.h>
#include <emmintrin.h>  // For SSE2 intrinsics
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ucm-config.h"
#include "util/thread.h"

pthread_t copy_threads[MAX_COPY_THREADS];
struct pmemcpy {
    pthread_mutex_t lock;
    pthread_barrier_t barrier;
    _Atomic bool write_zeros;
    _Atomic void *dst;
    _Atomic const void *src;
    _Atomic size_t length;
};
static struct pmemcpy pmemcpy;

__attribute__((unused)) static void memcpy_nt(void *dest, const void *src,
                                              size_t size) {
    const __m128i *src128 = (__m128i *)src;
    __m128i *dest128 = (__m128i *)dest;

    size_t i;
    for (i = 0; i < size / 16; ++i) {
        __m128i data = _mm_loadu_si128(&src128[i]);  // Load 16 bytes
        _mm_stream_si128(&dest128[i], data);         // Non-temporal store
    }

    // Handle any remaining bytes
    char *char_dest = (char *)dest128;
    const char *char_src = (const char *)src128;
    for (i = (size / 16) * 16; i < size; ++i) {
        char_dest[i] = char_src[i];
    }

    _mm_sfence();  // Ensure the stores are ordered and completed
}

void *hemem_parallel_memcpy_thread(void *arg) {
#ifdef CONFIG_PAR_MEMCPY
    uint64_t tid = (uint64_t)arg;
    void *src;
    void *dst;
    size_t length;
    size_t chunk_size;

    assert(tid < MAX_COPY_THREADS);

    thread_pin_self(PARALLEL_MIGRATE_THREAD_CPU + tid);

    char thread_name[15];
    sprintf(thread_name, "memcpy-thr-%ld", tid);
    pthread_setname_np(pthread_self(), thread_name);

    for (;;) {
        int r = pthread_barrier_wait(&pmemcpy.barrier);
        assert(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);

        // grab data out of shared struct
        length = pmemcpy.length;
        chunk_size = length / MAX_COPY_THREADS;
        dst = pmemcpy.dst + (tid * chunk_size);
        if (!pmemcpy.write_zeros) {
            src = (void *)((uint64_t)pmemcpy.src + (tid * chunk_size));
            // LOG("tid %lu: memcpy dst %p src %p sz %lu\n", tid, dst, src,
            //     chunk_size);
            assert(((uint64_t)src + chunk_size) < (uint64_t)dst ||
                   ((uint64_t)dst + chunk_size) < (uint64_t)src);
            memcpy_nt(dst, src, chunk_size);
        } else {
            memset(dst, 0, chunk_size);
        }

        // LOG("thread %lu done copying\n", tid);

        r = pthread_barrier_wait(&pmemcpy.barrier);
        assert(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);
    }
#else
    (void)arg;
#endif

    return NULL;
}

void memcpy_par_init() {
    int ret;
    uint64_t i;
    ret = pthread_barrier_init(&pmemcpy.barrier, NULL, MAX_COPY_THREADS + 1);
    assert(ret == 0);

    ret = pthread_mutex_init(&pmemcpy.lock, NULL);
    assert(ret == 0);

    for (i = 0; i < MAX_COPY_THREADS; i++) {
        ret = pthread_create(&copy_threads[i], NULL,
                             hemem_parallel_memcpy_thread, (void *)i);
        assert(ret == 0);
    }
}

void memcpy_par_shutdown() {}

void memcpy_par(void *dst, const void *src, size_t len) {
    pthread_mutex_lock(&(pmemcpy.lock));
    pmemcpy.dst = dst;
    pmemcpy.src = src;
    pmemcpy.length = len;
    pmemcpy.write_zeros = false;

    int r = pthread_barrier_wait(&pmemcpy.barrier);
    assert(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);

    r = pthread_barrier_wait(&pmemcpy.barrier);
    assert(r == 0 || r == PTHREAD_BARRIER_SERIAL_THREAD);

    pthread_mutex_unlock(&(pmemcpy.lock));
}
