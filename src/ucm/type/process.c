#include "type/process.h"

#include "ds/fifo.h"
#include "mm-async.h"
#include "mm.h"
#include "policy/policy.h"
#include "ucm-config.h"
#include "util/compiler.h"
#include "util/log.h"

static void process_mem_init(struct process_mem *mm) {
    uint64_t **buffer;

    for (int i = 0; i < NUM_HOTNESS_LEVELS; i++) {
        pthread_mutex_init(&(mm->fastmem_lists[i].list_lock), NULL);
        pthread_mutex_init(&(mm->slowmem_lists[i].list_lock), NULL);
    }
    pthread_mutex_init(&(mm->pages_lock), NULL);

    buffer = (uint64_t **)malloc(sizeof(uint64_t *) * CAPACITY);
    assert(buffer);
    mm->hot_ring = ring_buf_init(buffer, CAPACITY);
    buffer = (uint64_t **)malloc(sizeof(uint64_t *) * CAPACITY);
    assert(buffer);
    mm->cold_ring = ring_buf_init(buffer, CAPACITY);
    buffer = (uint64_t **)malloc(sizeof(uint64_t *) * CAPACITY);
    assert(buffer);
    mm->free_page_ring = ring_buf_init(buffer, CAPACITY);
    pthread_mutex_init(&(mm->free_page_ring_lock), NULL);

    mm->pages = NULL;
    mm->cur_cool_in_fastmem = NULL;
    mm->cur_cool_in_slowmem = NULL;
    mm->cur_cool_in_fastmem_list = 0;
    mm->cur_cool_in_slowmem_list = 0;
    mm->need_cool_fastmem = false;
    mm->need_cool_slowmem = false;
    mm->current_slowmem = 0;
    mm->current_fastmem = 0;
}

static size_t free_mem_list(struct process_mem *mm, struct page_list *list) {
    size_t pages_freed = 0;
    struct hemem_page *page = NULL;

    // NOTE: don't directly remove the page from the list yet.
    // The logic that handles free ring requests would do that.
    while ((page = next_page(list, page))) {
        mm_remove_page(mm, page);
        pages_freed++;
    }

    return pages_freed;
}

static void process_mem_destroy(struct process_mem *mm) {
    size_t n_freed = 0;
    for (size_t i = 0; i < NUM_HOTNESS_LEVELS; i++) {
        n_freed += free_mem_list(mm, &mm->fastmem_lists[i]);
    }
    for (size_t i = 0; i < NUM_HOTNESS_LEVELS; i++) {
        n_freed += free_mem_list(mm, &mm->slowmem_lists[i]);
    }

    struct hemem_process *process = mm_to_proc(mm);
    LOG("freeing %lu pages for process %d\n", n_freed, process->pid);

    // drain all pending requests.
    // NOTE: we really only care about the free requests.
    //
    // TODO: simplify the memory-freeing logic. there's really no need to
    // enqueue free requests to a request ring here.
    //
    // NOTE: deadlock could occur in `free_mem_list` if the free list is full.
    mm_drain_async_requests(process);

    free(mm->hot_ring);
    free(mm->cold_ring);
    free(mm->free_page_ring);
}

static void process_stats_init(struct process_stats *stats) {
    // TODO: no need to memset if the process is allocated with calloc().
    memset(stats, 0, sizeof(struct process_stats));
}

static void process_stats_destroy(struct process_stats *stats) {
    UNUSED(stats);
    // no-op for now
}

struct hemem_process *hemem_process_init(pid_t pid) {
    struct hemem_process *process =
        (struct hemem_process *)calloc(1, sizeof(struct hemem_process));
    if (process == NULL) {
        perror("calloc");
        return NULL;
    }

    process->pid = pid;
    process->exited = false;
    process->valid_uffd = false;

    pthread_mutex_init(&(process->process_lock), NULL);
    pthread_mutex_init(&(process->remap_fd_lock), NULL);

    process->still_migrating = false;

    process_mem_init(&process->mm);
    process_policy_init(&process->policy);
    process_stats_init(&process->stats);

    return process;
}

void hemem_process_destroy(struct hemem_process *process) {
    process_mem_destroy(&process->mm);
    process_policy_destroy(&process->policy);
    process_stats_destroy(&process->stats);
    free(process);
}
