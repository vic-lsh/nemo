#ifndef HEMEM_UCM_PAGE_H
#define HEMEM_UCM_PAGE_H

#include <stdio.h>

#include "ds/fifo.h"
#include "ds/pa-proc-map.h"
#include "ds/spsc-ring.h"
#include "ds/uthash.h"
#include "hemem-shared.h"
#include "ucm-config.h"

struct hemem_page {
    uint64_t va;
    pid_t pid;
    uint64_t devdax_offset;
    bool in_dram;
    long uffd;
    enum pagetypes pt;
    volatile bool migrating;
    // checks whether we're expecting an uffd unmap event on this page.
    // when we migrate a page, the migration generates an unmap event.
    // to disambiguate unmaps from the application versus from ourselves,
    // we use this boolean to find and ignore uffd unmap events from migration.
    _Atomic bool migration_uffd_unmap_pending;
    // introducing another MAP_FIXED mapping to an already allocated page
    // would trigger a uffd unmap event.
    _Atomic bool map_fixed_uffd_unmap_pending;
    bool present;
    uint64_t hot;
    uint64_t migrations_up, migrations_down;
    /** Store the last epoch at which the page was migrated. */
    _Atomic uint64_t last_migrated_epoch;
    /**
     * Stores the access counts migrations incurred on the remote memory.
     * Incremented by the migration thread and cleared by the scanning thread.
     */
    _Atomic uint64_t migration_access_comp;
    uint64_t local_clock[NPBUFTYPES];
    _Atomic bool volatile ring_present;

    bool in_free_ring;
    uint64_t accesses[NPBUFTYPES];
    uint64_t tot_accesses[NPBUFTYPES];

#ifdef CONFIG_PEBS_SKEWNESS
    uint32_t subpage_skewness[N_BASEPAGES_IN_HUGEPAGE];
#endif

    struct pa_proc_map_entry pa_proc_entry;

    // Mutex to synchronize migration-remapping and missed mapping pagefault
    // handling. For ease of reasoning, make either operation "atomic" and
    // allow no overlap.
    // TODO: maybe we can optimize this without a mutex.
    pthread_mutex_t remap_lock;

    UT_hash_handle hh;
    UT_hash_handle hh_pa; /* hash handle for pa to page mapping */
    struct hemem_page *next, *prev;
    struct page_list *list;
};

struct hemem_page *hemem_page_alloc();

#endif /* HEMEM_UCM_PAGE_H */
