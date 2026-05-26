#include "mm.h"

#include "ds/fifo.h"
#include "ds/pa-proc-map.h"
#include "ds/uthash.h"
#include "hemem-shared.h"
#include "mm-async.h"
#include "physmem/config.h"
#include "physmem/physmem.h"
#include "type/page.h"
#include "type/process.h"
#include "util/log.h"

// Mapping from page-aligned CXL physical address to processes.
static struct pa_proc_map pa_to_proc_map;

// Free lists for different tiers.
struct page_list fastmem_free_list;
struct page_list slowmem_free_list;

void mm_add_page(struct process_mem *mm, struct hemem_page *page) {
    bool should_add_dram = true;
    struct hemem_page *p;
    pthread_mutex_lock(&(mm->pages_lock));
    HASH_FIND(hh, mm->pages, &(page->va), sizeof(uint64_t), p);
    if (p != NULL) {
        // don't re-add page to hash table
        should_add_dram = false;
    }

    if (should_add_dram) {
        HASH_ADD(hh, mm->pages, va, sizeof(uint64_t), page);
    }

    if (!page->in_dram) {
        HASH_FIND(hh_pa, mm->cxl_offset_to_page, &(page->devdax_offset),
                  sizeof(uint64_t), p);
        if (p == NULL) {
            HASH_ADD(hh_pa, mm->cxl_offset_to_page, devdax_offset,
                     sizeof(uint64_t), page);
        }
    }
    pthread_mutex_unlock(&(mm->pages_lock));

    if (!page->in_dram) {
        pa_proc_map_add(&pa_to_proc_map, page, mm_to_proc(mm));
    }
}

struct hemem_page *mm_find_page(struct process_mem *mm, uint64_t va) {
    struct hemem_page *page;
    pthread_mutex_lock(&(mm->pages_lock));
    HASH_FIND(hh, mm->pages, &va, sizeof(uint64_t), page);
    pthread_mutex_unlock(&(mm->pages_lock));
    return page;
}

struct hemem_page *mm_find_page_by_cxl_offset(struct process_mem *mm,
                                              uint64_t devdax_offset) {
    struct hemem_page *page = 0;
    pthread_mutex_lock(&(mm->pages_lock));
    HASH_FIND(hh_pa, mm->cxl_offset_to_page, &devdax_offset, sizeof(uint64_t),
              page);
    pthread_mutex_unlock(&(mm->pages_lock));
    return page;
}

void mm_remove_page(struct process_mem *mm, struct hemem_page *page) {
    pthread_mutex_lock(&(mm->pages_lock));
    if (likely(mm->pages)) {
        HASH_DELETE(hh, mm->pages, page);
    }
    if (!page->in_dram) {
        if (likely(mm->cxl_offset_to_page)) {
            HASH_DELETE(hh_pa, mm->cxl_offset_to_page, page);
        } else {
            // suspected BUG: if this page isn't in fastmem, then it should be
            // in the `cxl_offset_to_page` hashmap; i.e., the hashmap shouldn't
            // be empty. For now we let this be.
        }
    }
    pthread_mutex_unlock(&(mm->pages_lock));
    if (!page->in_dram) {
        pa_proc_map_remove(&pa_to_proc_map, page);
    }
    mm_internal_enqueue_page_removal(mm_to_proc(mm), page);
}

void mm_add_page_to_cxl(struct process_mem *mm, struct hemem_page *page) {
    assert(!page->in_dram);

    struct hemem_page *p;
    pthread_mutex_lock(&(mm->pages_lock));

    HASH_FIND(hh_pa, mm->cxl_offset_to_page, &(page->devdax_offset),
              sizeof(uint64_t), p);
    if (p != NULL) {
        // should this be a bug?
        pthread_mutex_unlock(&(mm->pages_lock));
        return;
    }

    HASH_ADD(hh_pa, mm->cxl_offset_to_page, devdax_offset, sizeof(uint64_t),
             page);

    pthread_mutex_unlock(&(mm->pages_lock));

    pa_proc_map_add(&pa_to_proc_map, page, mm_to_proc(mm));
}

void mm_remove_page_from_cxl(struct process_mem *mm, struct hemem_page *page) {
    pthread_mutex_lock(&(mm->pages_lock));
    HASH_DELETE(hh_pa, mm->cxl_offset_to_page, page);
    pthread_mutex_unlock(&(mm->pages_lock));
    pa_proc_map_remove(&pa_to_proc_map, page);
}

struct hemem_process *mm_find_proc_by_cxl_offset(uint64_t cxl_devdax_offset) {
    return pa_proc_map_find(&pa_to_proc_map, cxl_devdax_offset);
}

struct hemem_page *get_free_fastmem_page() {
    return dequeue_page(&fastmem_free_list);
}

struct hemem_page *get_free_slowmem_page() {
    return dequeue_page(&slowmem_free_list);
}

void return_free_fastmem_page(struct hemem_page *page) {
    assert(!page->present);
    enqueue_page(&fastmem_free_list, page);
}

void return_free_slowmem_page(struct hemem_page *page) {
    assert(!page->present);
    enqueue_page(&slowmem_free_list, page);
}

void assert_page_freed(struct hemem_page *page) {
    assert(!page->present);
    if (page->in_dram) {
        assert(page->list == &fastmem_free_list);
    } else {
        assert(page->list == &slowmem_free_list);
    }
}
size_t get_fastmem_size() { return physmem_get_fastmem_size(); }

size_t get_slowmem_size() { return physmem_get_slowmem_size(); }

size_t get_fastmem_free_size() {
    // TODO: update this impl once we support non-uniform page size
    return get_fastmem_free_page_cnt() * HUGEPAGE_SIZE;
}

size_t get_slowmem_free_size() {
    // TODO: update this impl once we support non-uniform page size
    return get_slowmem_free_page_cnt() * HUGEPAGE_SIZE;
}

size_t get_fastmem_free_page_cnt() { return fastmem_free_list.numentries; }

size_t get_slowmem_free_page_cnt() { return slowmem_free_list.numentries; }

static char *pprint_physmem_mode(physmem_mode_t mode) {
    switch (mode) {
        case USE_DAX:
            return "dax";
        case USE_SHM:
            return "shared_mem";
        default:
            assert(0);
    }
}

void pprint_mm_opts(const mm_opts_t *opt) {
    LOG_NOPATH("physmem_mode: %s\n", pprint_physmem_mode(opt->physmem_mode));
    LOG_NOPATH("fastmem_size (bytes): %lu\n", opt->fastmem_size);
    LOG_NOPATH("slowmem_size (bytes): %lu\n", opt->slowmem_size);
}

inline size_t get_default_page_size() { return HUGEPAGE_SIZE; }

inline size_t get_default_page_type() {
    return pagesize_to_pt(get_default_page_size());
}

static void init_free_lists() {
    pthread_mutex_init(&(fastmem_free_list.list_lock), NULL);

    size_t pagesize = get_default_page_size();

    size_t n_fast_hugepages = get_fastmem_size() / pagesize;
    assert(n_fast_hugepages);

    for (size_t i = 0; i < n_fast_hugepages; i++) {
        struct hemem_page *p = hemem_page_alloc();
        assert(p);
        p->pt = pagesize_to_pt(pagesize);
        p->devdax_offset = i * pagesize;
        p->in_dram = true;

        enqueue_page(&fastmem_free_list, p);
    }

    pthread_mutex_init(&(slowmem_free_list.list_lock), NULL);

    size_t n_slow_hugepages = get_slowmem_size() / pagesize;
    assert(n_slow_hugepages);

    for (size_t i = 0; i < n_slow_hugepages; i++) {
        struct hemem_page *p = hemem_page_alloc();
        assert(p);
        p->pt = pagesize_to_pt(pagesize);
        p->devdax_offset = i * pagesize;
        p->in_dram = false;

        enqueue_page(&slowmem_free_list, p);
    }
}

void mm_init(mm_opts_t *opts) {
    physmem_init(opts);
    mm_async_init();
    init_free_lists();
    pa_proc_map_init(&pa_to_proc_map);
}
