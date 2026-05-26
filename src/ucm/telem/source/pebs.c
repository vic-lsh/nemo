#include "pebs.h"

#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ucm-config.h"
#include "util/compiler.h"
#include "util/log.h"

static pthread_t perf_poll_thread;

static struct perf_event_mmap_page *pebs_pages[HEMEM_NCORES];
int pebs_fds[HEMEM_NCORES];

static long perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                            int cpu, int group_fd, unsigned long flags) {
    int ret;

    ret = syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    return ret;
}

static struct perf_event_mmap_page *perf_setup(__u64 config, __u64 config1,
                                               __u64 cpu) {
    struct perf_event_attr attr;

    memset(&attr, 0, sizeof(struct perf_event_attr));

    attr.type = PERF_TYPE_RAW;
    attr.size = sizeof(struct perf_event_attr);

    attr.config = config;
    attr.config1 = config1;

    attr.sample_period = PEBS_SAMPLE_PERIOD;

    LOG_DEBUG("CPU %llu set with sample period %llu\n", cpu,
              attr.sample_period);

#ifdef PEBS_SAMPLE_IP
    attr.sample_type |= PERF_SAMPLE_IP;
#endif
#ifdef PEBS_SAMPLE_TIME
    attr.sample_type |= PERF_SAMPLE_TIME;
#endif
#ifdef PEBS_SAMPLE_TID
    attr.sample_type |= PERF_SAMPLE_TID;
#endif
#ifdef PEBS_SAMPLE_ADDR
    attr.sample_type |= PERF_SAMPLE_ADDR;
#endif
#ifdef PEBS_SAMPLE_PHYS_ADDR
    attr.sample_type |= PERF_SAMPLE_PHYS_ADDR;
#endif

    attr.disabled = 0;
    // attr.inherit = 1;
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;
    attr.exclude_callchain_kernel = 1;
    attr.exclude_callchain_user = 1;
    attr.precise_ip = 1;

    pebs_fds[cpu] = perf_event_open(&attr, -1, cpu, -1, 0);
    if (pebs_fds[cpu] == -1) {
        perror("perf_event_open");
    }
    assert(pebs_fds[cpu] != -1);

    size_t mmap_size = sysconf(_SC_PAGESIZE) * PEBS_NUM_PERF_PAGES;
    LOG("PEBS buffer mmap size %lu\n", mmap_size);
    struct perf_event_mmap_page *p = mmap(
        NULL, mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, pebs_fds[cpu], 0);
    if (p == MAP_FAILED) {
        perror("mmap");
    }
    assert(p != MAP_FAILED);

    return p;
}

static void pebs_scan(size_t core_idx, pebs_sample_handler_t handler) {
    struct perf_event_mmap_page *p;
    struct perf_event_header *ph;

    p = pebs_pages[core_idx];
    char *pbuf = (char *)p + p->data_offset;
    char *pbuf_end = pbuf + p->data_size;

    MEM_BARRIER();

    if (p->data_head == p->data_tail) {
        return;
    }

    assert(p->data_head > p->data_tail);

    while (p->data_head > p->data_tail) {
        ph = (void *)(pbuf + (p->data_tail % p->data_size));

        char *ph_end = (char *)ph + ph->size;
        if (unlikely(ph_end >= pbuf_end)) {
            // sample wrapped around. skip processing it b/c the handler assumes
            // the sample is physically contiguous.
            // NOTE: we could handle this sample by copying it to a contiguous
            // buffer, but it's likely okay to just skip one.
            goto advance_tail;
        }

        handler(ph, core_idx);

    advance_tail:
        p->data_tail += ph->size;
    }

    // Mem-barrier required after updating data_tail.
    MEM_BARRIER();
}

void pebs_epoch_scan(pebs_sample_handler_t handler) {
    for (int core_i = LAST_HEMEM_THREAD + 1; core_i < HEMEM_NCORES; core_i++) {
        pebs_scan(core_i, handler);
    }
}

void pebs_shutdown_signal_handler(int arg) {
    fprintf(stderr, "SIGUSR1... arg %d\n", arg);
    for (int i = 0; i < HEMEM_NCORES; i++) {
        if (pebs_fds[i] != 0) {
            ioctl(pebs_fds[i], PERF_EVENT_IOC_RESET, 0);
            ioctl(pebs_fds[i], PERF_EVENT_IOC_ENABLE, 0);
        }
    }
}

// For inexplicable reasons, PEBS receives fewer samples than expected after
// running for a while. Below is a hack to keep PEBS sample counts correct.
//
// This implements the same logic as scripts/run_perf.sh.
static void *perf_poll_thread_func() {
    pid_t perf_pid;

    while (1) {
        perf_pid = fork();

        if (perf_pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (perf_pid == 0) {
            // Child process
            // Execute numactl with perf stat
            execlp("numactl", "numactl", "-N0", "-m0", "--", "perf", "stat",
                   "-e", "cache-misses", NULL);
            perror("execlp failed");
            exit(EXIT_FAILURE);
        } else {
            // Parent process
            sleep(1);

            if (kill(perf_pid, SIGKILL) == -1) {
                perror("kill failed");
            }

            // WNOHANG makes waitpid non-blocking, so it won't hang if the child
            // is already dead
            waitpid(perf_pid, NULL, WNOHANG);
        }
    }

    return 0;
}

void pebs_init() {
    LOG("pebs_init: started\n");

    pthread_create(&perf_poll_thread, NULL, perf_poll_thread_func, NULL);

    LOG("initializing pebs on core %d to %d \n", LAST_HEMEM_THREAD,
        HEMEM_NCORES);

    for (int i = LAST_HEMEM_THREAD + 1; i < HEMEM_NCORES; i++) {
        pebs_pages[i] =
            perf_setup(0x1d3, 0, i);  // MEM_LOAD_L3_MISS_RETIRED.LOCAL_DRAM
    }

    LOG("pebs_init: finished\n");
}

void pebs_shutdown() {}
