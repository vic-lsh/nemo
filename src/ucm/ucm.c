#define _GNU_SOURCE
#include "ucm.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "core.h"
#include "hemem-shared.h"
#include "mm.h"
#include "policy/policy.h"
#include "proc-mgr.h"
#include "stats/stats.h"
#include "support/memops.h"
#include "util/log.h"

ssize_t ucm_allocate_memory(struct hemem_process *process, uint64_t start_addr,
                            size_t length, struct hemem_page_app *pages) {
    struct hemem_page *page;
    struct hemem_page_app *page_resp;
    size_t pages_curr = 0;
    ucm_stats_t *stats = ucm_stats_get();

    LOG_DEBUG("ucm_allocate_memory va %p len %lu\n", (void *)start_addr,
              length);

    // we assume that addr is page-aligned. see app-side logic for detail.
    for (uint64_t page_boundary = start_addr;
         page_boundary < start_addr + length; pages_curr++) {
        bool is_new_alloc = false;

        page = mm_find_page(&process->mm, page_boundary);
        assert(!page);

        page = ucm_allocate_page(process);
        if (!page) {
            goto out_of_mem;
        }
        is_new_alloc = true;

        assert(page != NULL);

        uint64_t offset = page->devdax_offset;
        bool in_dram = page->in_dram;
        uint64_t pagesize = pt_to_pagesize(page->pt);

        void *mmapped_addr = devdax_offset_to_addr(offset, in_dram);

        if (is_new_alloc) {
            hemem_memset(mmapped_addr, 0, pagesize);
            stats->mem.memsets++;
        }

        LOG_DEBUG("allocated page for process %d at va %p\n", process->pid,
                  (void *)page_boundary);

        page->va = page_boundary;
        page->pid = process->pid;
        page->uffd = process->uffd;
        assert(page->va != 0);
        assert(page->va % HUGEPAGE_SIZE == 0);
        page->migrating = false;
        page->migrations_up = page->migrations_down = 0;

        mm_add_page(&process->mm, page);

        struct page_list *pg_list = (page->in_dram)
                                        ? &process->mm.fastmem_lists[COLD]
                                        : &process->mm.slowmem_lists[COLD];
        enqueue_page(pg_list, page);

        page_resp = &(pages[pages_curr]);
        page_resp->va = page_boundary;
        page_resp->devdax_offset = offset;
        page_resp->in_dram = in_dram;
        page_resp->pt = page->pt;
        page_boundary += pagesize;

        ucm_stats_record_page_allocated(process, pagesize);
    }

    return pages_curr;

out_of_mem:
    LOG_ERR("could not allocate %lu bytes, rolling back %lu allocated pages\n",
            length, pages_curr);
    for (size_t i = 0; i < pages_curr; i++) {
        struct hemem_page_app *p = &pages[i];
        page = mm_find_page(&process->mm, p->va);
        assert(page);
        mm_remove_page(&process->mm, page);
    }
    return -1;
}

ssize_t ucm_map_fixed(struct hemem_process *process, uint64_t start_addr,
                      size_t length, struct hemem_page_app *pages) {
    struct hemem_page *page;
    struct hemem_page_app *page_resp;
    size_t pages_curr = 0;

    LOG_DEBUG("ucm_map_fixed va 0x%p len %lu\n", (void *)start_addr, length);

    // we assume that addr is page-aligned. see app-side logic for detail.
    for (uint64_t page_boundary = start_addr;
         page_boundary < start_addr + length; pages_curr++) {
        LOG("searching page boundary %p\n", (void *)page_boundary);
        page = mm_find_page(&process->mm, page_boundary);
        assert(page != NULL);
        LOG("found page at boundary %p\n", (void *)page_boundary);

        uint64_t offset = page->devdax_offset;
        bool in_dram = page->in_dram;
        uint64_t pagesize = pt_to_pagesize(page->pt);

        LOG_DEBUG("found page for fixed mapping for process %d at va %p\n",
                  process->pid, (void *)page_boundary);

        page->map_fixed_uffd_unmap_pending = true;

        page_resp = &(pages[pages_curr]);
        page_resp->va = page_boundary;
        page_resp->devdax_offset = offset;
        page_resp->in_dram = in_dram;
        page_resp->pt = page->pt;
        page_boundary += pagesize;

        ucm_stats_record_page_allocated(process, pagesize);
    }

    return pages_curr;
}

size_t ucm_deallocate_memory(struct hemem_process *process, uint64_t start_addr,
                             size_t length) {
    uint64_t end = start_addr + length;
    // TODO: is this necessarily the address to start deallocating from after
    // supporting page split? revisit.
    uint64_t page_boundary = start_addr & ~(HUGEPAGE_SIZE - 1);
    uint64_t pagesize;
    struct hemem_page *page;
    uint64_t n_freed = 0;

    LOG_DEBUG("ucm_deallocate_memory va %p len %lu\n", (void *)start_addr,
              length);

    while (page_boundary < end) {
        page = mm_find_page(&process->mm, page_boundary);
        if (page != NULL) {
            pagesize = pt_to_pagesize(page->pt);

            mm_remove_page(&process->mm, page);
            ucm_stats_record_page_freed(process, pagesize);
            n_freed += 1;
            page_boundary += pagesize;
        } else {
            page_boundary += get_default_page_size();
        }
    }

    return n_freed;
}

void ucm_remove_process(struct hemem_process *process) {
    remove_process(process);
    hemem_process_destroy(process);
}
