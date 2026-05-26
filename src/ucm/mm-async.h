#ifndef HEMEM_UCM_MM_ASYNC_H
#define HEMEM_UCM_MM_ASYNC_H

#include "type/page.h"
#include "type/process.h"
#include "ucm-config.h"

typedef struct cool_stats {
    uint64_t cools_started[NPBUFTYPES];
    uint64_t cools_completed[NPBUFTYPES];
} cool_stats_t;

void mm_async_init();

cool_stats_t *mm_get_cool_stats();

bool mm_mark_page_hot_async(struct hemem_process *process,
                            struct hemem_page *page);
bool mm_mark_page_cold_async(struct hemem_process *process,
                             struct hemem_page *page);

void mm_request_soft_cool_fastmem(struct hemem_process *proc);
void mm_request_cool_fastmem(struct hemem_process *proc);
void mm_request_soft_cool_slowmem(struct hemem_process *proc);
void mm_request_cool_slowmem(struct hemem_process *proc);

void mm_drain_async_requests(struct hemem_process *process);
struct hemem_page *incremental_fastmem_cooling(struct hemem_process *process);
struct hemem_page *incremental_slowmem_cooling(struct hemem_process *process);

// Impl functions below: invoked only by mm.c
void mm_internal_enqueue_page_removal(struct hemem_process *process,
                                      struct hemem_page *page);

#endif /* HEMEM_UCM_MM_ASYNC_H */
