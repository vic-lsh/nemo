#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <immintrin.h>
#include <time.h>
#include <stdbool.h>
#include <stdatomic.h>

#define GB (1024L * 1024L * 1024L)
#define SLOWMEM_SIZE (1550L * GB / 100L)
//#define SLOWMEM_SIZE (128L * GB)
#define TARGET 0x4080000000
#define MEM_START (0L * GB)
#define MEM_END (MEM_START + (16L * GB))
//#define MEM_END1 (MEM_START + (31L * GB))
//#define MEM_END1 (MEM_START + (1L * GB))
#define MEM_END1 (MEM_START + (SLOWMEM_SIZE))
#define MEM_END3 (MEM_START + (15L * GB))
//#define REPEAT (8)

//#define WRAP_AROUND (0x80000000L)
#define WRAP_AROUND (0x8000000000000000L)
#define TIMES_FACTOR (3L)

#define KB (1L << 10)
#define MB (1L << 20)

//#define DISPLAY_MEMOPS
#define DISPLAY_UPDATES

#define N (2)
/*
const bool do_write = false;
const bool do_read = true;
const bool even_cachelines_only = false;
const bool odd_cachelines_actually = false;
const uint64_t even_only_factor = even_cachelines_only ? 2 : 1;
const uint64_t odd_factor = odd_cachelines_actually ? 64 : 0;
*/

atomic_int bar = 0;

void print_m512i(const __m512i* vec_ptr) {
    __attribute__((aligned(64))) int64_t values[8];
    _mm512_store_si512((__m512i*)values, *vec_ptr);

    //printf("Contents of __m512i (int64_t):\n");
    for (int i = 0; i < 8; ++i) {
        fprintf(stderr, "%ld,", values[i]);
    }
    fprintf(stderr, "\n");
}

__attribute__((noinline))
void do_memread(volatile uint64_t * __restrict buffer, uint64_t start_byte, uint64_t end_byte, uint64_t every_other_factor, uint64_t odd_factor) {
    const uint64_t step_bytes = 64; // 512bit instructions
    const uint64_t step = step_bytes / sizeof(uint64_t);
    if ((uint64_t)buffer % step_bytes != 0) {
        printf("err2\n");
        return;
    }
    if (start_byte % step_bytes != 0 || end_byte % step_bytes != 0) {
        printf("err3\n");
        return;
    }
    if ((end_byte - start_byte) % step_bytes != 0) {
        printf("err\n");
        return;
    }
    if (start_byte % 128 != 0) {
        printf("err4\n");
        return;
    }
    /*
    volatile __m512i rd;
    volatile __m512i *rd_p = &rd;
    */
    __m512i rd;
    __m512i *rd_p = &rd;
    //__m512i base_vec = _mm512_set_epi64(7,6,5,4,3,2,1,0);
    __m512i base_vec = _mm512_set_epi64(7*TIMES_FACTOR,6*TIMES_FACTOR,5*TIMES_FACTOR,4*TIMES_FACTOR,3*TIMES_FACTOR,2*TIMES_FACTOR,1*TIMES_FACTOR,0*TIMES_FACTOR);
    for (uint64_t i = start_byte + odd_factor; i < end_byte; i += step_bytes*every_other_factor) {
        *rd_p = _mm512_load_si512((__m512i*)(&buffer[i / step]));
        __m512i offset = _mm512_set1_epi64((i / sizeof(uint64_t)) * (8*TIMES_FACTOR) % WRAP_AROUND);
        __m512i vec = _mm512_add_epi64(base_vec, offset);
        if (memcmp(&rd, &vec, 64) != 0) {
        //if (rd != vec) {
            fprintf(stderr, "READ FAILURE %lu\n", i);
            fprintf(stderr, "iter %lu / %lu\n", (i - start_byte)/(step_bytes*every_other_factor),
            (end_byte-start_byte)/(step_bytes*every_other_factor));
            fprintf(stderr, "expected: ");
            print_m512i(&vec);
            fprintf(stderr, "got: ");
            print_m512i(&rd);
            exit(1);
            return;
        }
        /*
        if ((uint64_t)((void*)(&buffer[i/step])) % 128 != 0) {
            printf("ack\n");
        }
        */
#ifdef DISPLAY_MEMOPS
        // TODO: display rdtsc and core count
        // log to different files per core
        fprintf(stderr, "r %lu\n", (size_t)(&buffer[i / step]));
        fflush(stderr);
#endif
#ifdef DISPLAY_UPDATES
        if ((i % (8 * MB)) == 0) {
            fprintf(stderr, "r %lu\n", (size_t)(&buffer[i / step]));
            fflush(stderr);
        }
#endif
    }
    //(rd);

}
__attribute__((noinline))
void do_memset(uint64_t * __restrict buffer, uint64_t start_byte, uint64_t end_byte, uint64_t every_other_factor, uint64_t odd_factor) {
    const uint64_t step_bytes = 64; // 512bit instructions
    const uint64_t step = step_bytes / sizeof(uint64_t);
    if ((uint64_t)buffer % step_bytes != 0) {
        printf("err2\n");
        return;
    }
    if (start_byte % step_bytes != 0 || end_byte % step_bytes != 0) {
        printf("err3\n");
        return;
    }
    if ((end_byte - start_byte) % step_bytes != 0) {
        printf("err\n");
        return;
    }
    __m512i base_vec = _mm512_set_epi64(7*TIMES_FACTOR,6*TIMES_FACTOR,5*TIMES_FACTOR,4*TIMES_FACTOR,3*TIMES_FACTOR,2*TIMES_FACTOR,1*TIMES_FACTOR,0*TIMES_FACTOR);
    for (uint64_t i = start_byte + odd_factor; i < end_byte; i += step_bytes*every_other_factor) {
        __m512i offset = _mm512_set1_epi64((i / sizeof(uint64_t)) * (8*TIMES_FACTOR) % WRAP_AROUND);
        __m512i vec = _mm512_add_epi64(base_vec, offset);

        if (i + step_bytes*every_other_factor >= end_byte) {
            fprintf(stderr, "last write: addr=0x%lx\n", i/step_bytes);
            print_m512i(&vec);
        }

        //_mm512_store_si512((__m512i*)(&buffer[i / step]), vec);
        _mm512_stream_si512((__m512i*)(&buffer[i / step]), vec);
#ifdef DISPLAY_MEMOPS
        fprintf(stderr, "w %lu\n", (size_t)(&buffer[i / step]));
        fflush(stderr);
#endif
#ifdef DISPLAY_UPDATES
        if ((i % (8 * MB)) == 0) {
            fprintf(stderr, "w %lu\n", (size_t)(&buffer[i / step]));
            fflush(stderr);
        }
#endif

    }
}

// Function to compute the difference between two timespecs in seconds
double time_diff(struct timespec start, struct timespec end) {
        return (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
}

typedef struct {
    int thread_id;
    int core_id;
    uint64_t *p;
} thread_arg_t;

void *worker_function(void *arg) {
    thread_arg_t *targ = (thread_arg_t *)arg;

    // Set thread affinity to specific core
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(targ->core_id, &cpuset);

    int s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        perror("pthread_setaffinity_np");
    }

    printf("Thread %d running on core %d with write every_other %ld and start offset %ld\n", targ->thread_id, targ->core_id,
    1lu, 64lu);

    sleep(1);

    /*
    int old = atomic_fetch_add(&bar, 1);
    while (old != N) {
        old = atomic_load(&bar);
    }
    */

    
    /*
    for (int i = 0; i < 100; i++) {
        int dest = rand() % 4;
        fprintf(stdout, "thread %d i=%d running dest %d\n", targ->thread_id, i, dest);
        fflush(stdout);
        if (dest == 0) do_memset(targ->p, MEM_START, MEM_END1, 1, 64);
        if (dest == 1) do_memset(targ->p, MEM_END3, MEM_END, 1, 64);
        if (dest == 2) do_memread(targ->p, MEM_START, MEM_END1, 1, 64);
        if (dest == 3) do_memread(targ->p, MEM_END3, MEM_END, 1, 64);
    }
    */
    
    //if (targ->thread_id == 0) sleep(2);
    if (targ->thread_id == 1) {
        fprintf(stdout, "thread %d i=%d running dest %d\n", targ->thread_id, 0, 3);
        fflush(stdout);
        do_memread(targ->p, MEM_START, MEM_END1, 1, 0);
        //do_memread(targ->p, MEM_END3, MEM_END, 1, 64);
    } else {
        fprintf(stdout, "thread %d i=%d running dest %d\n", targ->thread_id, 0, 1);
        fflush(stdout);
        do_memset(targ->p, MEM_START, MEM_END1, 1, 0);
        //do_memset(targ->p, MEM_END3, MEM_END, 1, 64);
    }

    printf("Thread %d finished\n", targ->thread_id);
    return NULL;
}

int main(int argc, char ** argv) {
    // Size: 16GB in bytes
    //size_t size = 16ULL * 1024 * 1024 * 1024;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
	printf("fd invalid. run with sudo?\n");
	return 1;
    }

    // Allocate memory using mmap
    void* addr = mmap(
        NULL,                         // Let the kernel choose the address
        SLOWMEM_SIZE,
        PROT_READ | PROT_WRITE,       // Read and write permissions
        MAP_SHARED,// | MAP_POPULATE,
        fd,
        TARGET
    );

    if (addr == MAP_FAILED) {
        perror("mmap failed");
        return 1;
    }

    // FILE* file = fopen("numbers.txt", "w");

    printf("Successfully mapped 16GB of memory at %p\n", addr);
    //volatile char* p = addr;
    uint64_t* p = addr;

    printf("spawning threads...\n");
    struct timespec start_time, end_time;
	pthread_t threads[N];
    thread_arg_t args[N];
    // Spawn N threads pinned to cores 0 to N-1
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    for (int i = 0; i < N; ++i) {
        args[i].thread_id = i;
        args[i].core_id = i;
        args[i].p = p;
        if (pthread_create(&threads[i], NULL, worker_function, &args[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }

        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed = time_diff(start_time, end_time);
    printf("Memory set complete in %.9f seconds.\n", elapsed - 1); // -1 for sleep(1)
    //return 0;

    // Unmap the memory (optional, as program exit will clean up)
    if (munmap(addr, SLOWMEM_SIZE) == -1) {
        perror("munmap failed");
        return 1;
    }

    printf("Memory unmapped successfully\n");
    return 0;
}
