#include "alloc.h"

#include <sys/time.h>

#include "mm.h"
#include "util/log.h"

static struct hemem_page *allocate_fastmem_preferred(
    struct hemem_process *process) {
    struct timeval start, end;
    struct hemem_page *page;

    gettimeofday(&start, NULL);

    pthread_mutex_lock(&(process->process_lock));

    if (process->mm.current_fastmem < process->mm.max_fastmem) {
        page = get_free_fastmem_page();
        if (page != NULL) {
            assert(page->in_dram);
            assert(!page->present);

            page->present = true;
            page->pid = process->pid;

            gettimeofday(&end, NULL);
            LOG_TIME("mem_policy_allocate_page: %f s\n",
                     elapsed_secs(&start, &end));

            process->mm.current_fastmem += pt_to_pagesize(page->pt);

            pthread_mutex_unlock(&(process->process_lock));

            return page;
        }
    }

    // fastmem is full, fall back to slowmem
    page = get_free_slowmem_page();
    if (page != NULL) {
        assert(!page->in_dram);
        assert(!page->present);

        page->present = true;
        page->pid = process->pid;

        gettimeofday(&end, NULL);
        LOG_TIME("mem_policy_allocate_page: %f s\n",
                 elapsed_secs(&start, &end));

        process->mm.current_slowmem += pt_to_pagesize(page->pt);

        pthread_mutex_unlock(&(process->process_lock));

        return page;
    }

    LOG_ERR("Out of memory!\n");

    return NULL;
}

static struct hemem_page *allocate_slowmem_preferred(
    struct hemem_process *process) {
    struct timeval start, end;
    struct hemem_page *page;

    gettimeofday(&start, NULL);

    pthread_mutex_lock(&(process->process_lock));

    page = get_free_slowmem_page();
    if (page != NULL) {
        assert(!page->in_dram);
        assert(!page->present);

        page->present = true;
        page->pid = process->pid;

        gettimeofday(&end, NULL);
        LOG_TIME("mem_policy_allocate_page: %f s\n",
                 elapsed_secs(&start, &end));

        process->mm.current_slowmem += pt_to_pagesize(page->pt);

        pthread_mutex_unlock(&(process->process_lock));
        return page;
    }

    page = get_free_fastmem_page();
    if (page != NULL) {
        assert(page->in_dram);
        assert(!page->present);

        page->present = true;
        page->pid = process->pid;

        gettimeofday(&end, NULL);
        LOG_TIME("mem_policy_allocate_page: %f s\n",
                 elapsed_secs(&start, &end));

        process->mm.current_fastmem += pt_to_pagesize(page->pt);

        pthread_mutex_unlock(&(process->process_lock));
        return page;
    }

    LOG_ERR("Out of memory!\n");

    return NULL;
}

page_alloc_fn_t get_alloc_fn_by_mode(enum ALLOC_MODE mode) {
    switch (mode) {
        case FASTMEM_PREFERRED:
            return allocate_fastmem_preferred;
        case SLOWMEM_PREFERRED:
            return allocate_slowmem_preferred;
        default:
            LOG_ERR("unrecognized allocation mode %d\n", mode);
            assert(0);
    }
}

const char *pprint_alloc_mode(enum ALLOC_MODE mode) {
    switch (mode) {
        case FASTMEM_PREFERRED:
            return "fastmem preferred";
        case SLOWMEM_PREFERRED:
            return "slowmem_preferred";
        default:
            LOG_ERR("unrecognized allocation mode %d\n", mode);
            assert(0);
    }
}
