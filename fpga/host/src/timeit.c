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
//#define SLOWMEM_SIZE (1550L * GB / 100L)
#define SLOWMEM_SIZE (16 * GB)
#define TARGET 0x4080000000
#define MEM_START (1L * GB)
#define MEM_END (MEM_START + (8L * GB))
#define MEM_END1 (MEM_START + (1L * GB))
#define MEM_END3 (MEM_START + (7L * GB))
#define REPEAT (8)

#define KB (1L << 10)
#define MB (1L << 20)

#define N (4)
/*
const bool do_write = false;
const bool do_read = true;
const bool even_cachelines_only = false;
const bool odd_cachelines_actually = false;
const uint64_t even_only_factor = even_cachelines_only ? 2 : 1;
const uint64_t odd_factor = odd_cachelines_actually ? 64 : 0;
*/

atomic_int bar = 0;

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
    volatile __m512i rd;
    volatile __m512i *rd_p = &rd;
    for (uint64_t i = start_byte + odd_factor; i < end_byte; i += step_bytes*every_other_factor) {
        *rd_p = _mm512_load_si512((__m512i*)(&buffer[i / step]));
        /*
        if ((uint64_t)((void*)(&buffer[i/step])) % 128 != 0) {
            printf("ack\n");
        }
        */
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
    __m512i base_vec = _mm512_set_epi64(7,6,5,4,3,2,1,0);
    for (uint64_t i = start_byte + odd_factor; i < end_byte; i += step_bytes*every_other_factor) {
        __m512i offset = _mm512_set1_epi64(i / sizeof(uint64_t));
        __m512i vec = _mm512_add_epi64(base_vec, offset);

        //_mm512_store_si512((__m512i*)(&buffer[i / step]), vec);
        _mm512_stream_si512((__m512i*)(&buffer[i / step]), vec);
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
    bool do_write;
    uint64_t write_every_other;
    uint64_t write_odd_factor;
    bool do_read;
    uint64_t read_every_other;
    uint64_t read_odd_factor;
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
    targ->write_every_other, targ->write_odd_factor);

    sleep(1);

    int old = atomic_fetch_add(&bar, 1);
    while (old != N) {
        old = atomic_load(&bar);
    }

    if (targ->do_write) {
        //memset(((char*)p) + MEM_START, 0, MEM_END-MEM_START);
		if (targ->thread_id == 0) {
			do_memset(targ->p, MEM_START, MEM_END1, targ->write_every_other, targ->write_odd_factor);
			//start_hw_counter(targ->register_space);
            for (int i = 0; i < REPEAT; i++) {
                do_memset(targ->p, MEM_END1, MEM_END3, targ->write_every_other, targ->write_odd_factor);
            }
			//if (!targ->do_read) stop_hw_counter(targ->register_space);
			do_memset(targ->p, MEM_END3, MEM_END, targ->write_every_other, targ->write_odd_factor);
		} else {
            for (int i = 0; i < REPEAT; i++) {
                do_memset(targ->p, MEM_START, MEM_END, targ->write_every_other, targ->write_odd_factor);
            }
		}
    }
    if (targ->do_read) {
		if (targ->thread_id == 0) {
			do_memread(targ->p, MEM_START, MEM_END1, targ->read_every_other, targ->read_odd_factor);
			//if (!targ->do_write) start_hw_counter(targ->register_space);
            for (int i = 0; i < REPEAT; i++) {
                do_memread(targ->p, MEM_END1, MEM_END3, targ->read_every_other, targ->read_odd_factor);
            }
			//stop_hw_counter(targ->register_space);
			do_memread(targ->p, MEM_END3, MEM_END, targ->read_every_other, targ->read_odd_factor);
		} else {
            for (int i = 0; i < REPEAT; i++) {
                do_memread(targ->p, MEM_START, MEM_END, targ->read_every_other, targ->read_odd_factor);
            }
		}
    }

    printf("Thread %d finished\n", targ->thread_id);
    return NULL;
}

int main(int argc, char ** argv) {
    // Size: 16GB in bytes
    //size_t size = 16ULL * 1024 * 1024 * 1024;

    if (argc != 3) {
        printf("usage: %s <do_writes> <do_reads>, where do_x is 0=disabled, 1=enabled, 2=even addrs only, 3=odd addrs only.\n", argv[0]);
        return 1;
    }
    unsigned long do_writes_arg = strtol(argv[1], NULL, 0);
    unsigned long do_reads_arg = strtol(argv[2], NULL, 0);
    if (do_writes_arg > 3 || do_reads_arg > 3) {
        printf("reads, writes <= 3\n");
        return 2;
    }
    bool do_writes = do_writes_arg != 0;
    bool write_every_other_bool = do_writes_arg >= 2;
    bool write_odds_bool = do_writes_arg == 3;
    bool do_reads = do_reads_arg != 0;
    bool read_every_other_bool = do_reads_arg >= 2;
    bool read_odds_bool = do_reads_arg == 3;

    uint64_t write_every_other = write_every_other_bool ? 2 : 1;
    uint64_t read_every_other = read_every_other_bool ? 2 : 1;

    uint64_t write_odd_factor = write_odds_bool ? 64 : 0;
    uint64_t read_odd_factor = read_odds_bool ? 64 : 0;

    printf("Writes: %d\tWrite increment multiplier: %ld\tWrite offset: %ld\n", do_writes, write_every_other, write_odd_factor);
    printf("Reads: %d\tRead increment multiplier: %ld\tRead offset: %ld\n", do_reads, read_every_other, read_odd_factor);

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
        MAP_SHARED | MAP_POPULATE,
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
        args[i].do_write = do_writes;
        //args[i].do_write = (i / 2 == 0) ? 1 : 0;
        args[i].write_every_other = write_every_other;
        //args[i].write_odd_factor = (i % 2 == 0) ? 64 : 0;
        args[i].write_odd_factor = write_odd_factor;
        args[i].do_read = do_reads;
        //args[i].do_read = (i / 2 == 0) ? 0 : 1;
        args[i].read_every_other = read_every_other;
        args[i].read_odd_factor = read_odd_factor;
        if (pthread_create(&threads[i], NULL, worker_function, &args[i]) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // Join threads later
    for (int i = 0; i < N; ++i) {
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
