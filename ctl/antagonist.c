// compile: gcc -O3 -march=native antagonist.c -lpthread -o antagonist
#define _GNU_SOURCE
#include <immintrin.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <unistd.h>

typedef struct { char *buf; size_t nbytes; } args_t;

void* worker(void* p) {
    args_t *a = (args_t*)p;
    char *b = a->buf;
    size_t n = a->nbytes;
    for (;;) {
        for (size_t i = 0; i < n; i += 64) {
            __m512i z = _mm512_set1_epi32((int)i);
            _mm512_stream_si512((__m512i*)(b + i), z);
        }
        _mm_sfence(); // ensure write combining drains
    }
    return NULL;
}

int main(int argc, char** argv) {
    size_t nbytes = (size_t)2<<30; // 8 GB per thread
    int threads = sysconf(_SC_NPROCESSORS_ONLN) / 2;
    if (argc > 1) threads = atoi(argv[1]);

    printf("Using %d threads\n", threads);

    pthread_t *ts = calloc(threads, sizeof(*ts));
    for (int t = 0; t < threads; t++) {
        char *buf = aligned_alloc(64, nbytes);
        if (!buf) { perror("alloc"); return 1; }
        args_t *a = malloc(sizeof(args_t));
        a->buf = buf; a->nbytes = nbytes;
        pthread_create(&ts[t], NULL, worker, a);
    }
    pause();
    return 0;
}
