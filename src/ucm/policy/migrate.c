#include "migrate.h"

#include "core.h"

/** Page must be at this tier or higher to be considered for promotion. */
// TODO: make this runtime tunable?
#define PEBS_MIN_PROMOTION_TIER (0)
/** Page must be at this tier or lower to be considered for demotion. */
#define PEBS_MAX_DEMOTION_TIER (NUM_HOTNESS_LEVELS - 2)

// Min migration interval for a page
#define MIN_MIGRATION_INTERVAL_SECS 5
#define MIN_MIGRATION_EPOCH_INTERVAL 5

struct hemem_page *find_hottest_slowmem_page(struct hemem_process *process) {
    struct hemem_page *p = 0;

    uint64_t curr_epoch = ucm_get_curr_epoch();

    for (int i = NUM_HOTNESS_LEVELS - 1; i >= PEBS_MIN_PROMOTION_TIER; i--) {
        p = 0;
        while (true) {
            p = next_page(&(process->mm.slowmem_lists[i]), p);
            if (!p) {
                break;
            }

            assert(p->va);

            uint64_t min_migration_epoch =
                (p->last_migrated_epoch == 0)
                    ? 0
                    : p->last_migrated_epoch + MIN_MIGRATION_EPOCH_INTERVAL;
            if (curr_epoch >= min_migration_epoch) {
                // found candidate
                page_list_remove(&(process->mm.slowmem_lists[i]), p);
                assert(p->pid == process->pid);
                return p;
            }
        }
    }
    return NULL;
}

struct hemem_page *find_coldest_fastmem_page(struct hemem_process *process) {
    struct hemem_page *p = 0;

    uint64_t curr_epoch = ucm_get_curr_epoch();

    for (int i = 0; i < PEBS_MAX_DEMOTION_TIER; i++) {
        p = 0;
        while (true) {
            p = next_page(&(process->mm.fastmem_lists[i]), p);
            if (!p) {
                break;
            }

            assert(p->va);

            uint64_t min_migration_epoch =
                (p->last_migrated_epoch == 0)
                    ? 0
                    : p->last_migrated_epoch + MIN_MIGRATION_EPOCH_INTERVAL;
            if (curr_epoch >= min_migration_epoch) {
                // found candidate
                page_list_remove(&(process->mm.fastmem_lists[i]), p);
                assert(p->pid == process->pid);
                return p;
            }
        }
    }
    return NULL;
}

struct hemem_page *find_oldest_fastmem_page(struct hemem_process *process) {
    // Pick the page that has stayed in DRAM the longest.
    // Works only under non-PEBS configuration.
    return dequeue_page(&(process->mm.fastmem_lists[COLD]));
}
