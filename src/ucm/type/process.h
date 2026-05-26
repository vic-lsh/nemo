#ifndef HEMEM_PROCESS_H
#define HEMEM_PROCESS_H

#include <stddef.h>
#include <stdio.h>

#include "ds/fifo.h"
#include "ds/pa-proc-map.h"
#include "ds/spsc-ring.h"
#include "ds/uthash.h"
#include "hemem-shared.h"
#include "mm.h"
#include "policy/policy-state.h"
#include "ucm-config.h"
#include "util/compiler.h"

struct process_pagefault_stats {
    // Track the number of page-missing page faults;
    uint64_t miss_faults;
    // Track the number of page-missing page faults where the page is already
    // allocated by the UCM (but is unmapped by the app).
    uint64_t miss_but_allocated_faults;
};

struct process_migration_stats {
    // Number of pages migrated up.
    uint64_t up_count;
    // Number of pages migrated down.
    uint64_t down_count;
    // Number of times the application is stalled due to write-protection.
    uint64_t wp_wait_count;
    // Application stall time due to write-protection.
    uint64_t wp_wait_us;
};

struct cxl_stats {
    // Number of cxl counters that can't be tied to a page in this process (but
    // we know this page was allocated to this process).
    _Atomic uint64_t unknown_page_count;
};

struct process_stats {
    struct process_pagefault_stats pf;
    struct process_migration_stats migration;
    struct cxl_stats cxl;
};

struct hemem_process {
    pid_t pid;
    long uffd;
    bool exited;
    bool valid_uffd;

    int remap_fd;
    /**
     * Lock used to serialize request-response pairs via the remap fd channel.
     *
     * There could be concurrent producers to the remap channel: one thread
     * could be doing page migration, while another thread handles a uffd page
     * missing fault. Both threads would produce remap requests to the app.
     *
     * If request-sending and response-receiving is not done as one atomic step,
     * a thread could wait for the wrong response. This situation arises when
     * there're multiple outstanding requests in the channel, and multiple
     * producers are waiting on the channel via read(). In this case, there is
     * no guarantee that the producer is reading the response to its request.
     *
     * Not reading back its response is akin to getting a bad ACK. For example,
     * a migration thread could prematurely declare that a page has completed
     * migration.
     *
     * Intended usage:
     * Lock should be held before sending the request, and released after
     * response has been read from the channel.
     *
     * Future impls could optimize with multiple remap channels. However, we
     * should still maintain the invariant that each channel has at most one
     * outstanding request.
     */
    pthread_mutex_t remap_fd_lock;

    bool zero;
    bool still_migrating;

    uint64_t process_clock[NPBUFTYPES];

    UT_hash_handle phh;

    struct hemem_process *next, *prev;
    struct process_list *list;

    pthread_mutex_t process_lock;

    struct process_mem mm;
#if defined(CONFIG_POLICY_QOS) || defined(CONFIG_POLICY_FAIR_SHARE)
    struct process_policy policy;
#endif
    struct process_stats stats;
};

#define mm_to_proc(ptr) container_of(ptr, struct hemem_process, mm)

struct hemem_process *hemem_process_init(pid_t pid);
void hemem_process_destroy(struct hemem_process *process);

#endif /* HEMEM_PROCESS_H */
