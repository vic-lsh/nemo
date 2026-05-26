#include "mmap.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "ipc-shared.h"
#include "ipc.h"
#include "uffd.h"

_Atomic size_t mem_amplification = 0;

static void hemem_mmap_populate(app_ctx_t* ctx, void* addr, size_t length) {
    void* newptr;
    uint64_t offset;
    struct hemem_page_app* page;
    bool in_dram;
    uint64_t page_boundary;
    uint64_t pagesize;
    struct alloc_response* response;
#ifndef ONE_MEM_REQUEST
    size_t req_mem_size;
    size_t remaining_length = length;
#endif

    LOG_DEBUG("hemem_mmap_populate start %p len %lu end %p\n", addr, length,
              (char*)addr + length);

    assert(addr != 0);
    assert(length != 0);

#ifdef ONE_MEM_REQUEST
    response = alloc_space((void*)addr, length);
    if (response->header.status != 0) {
        perror("hemem_mmap_populate alloc fails");
        assert(0);
    }
#endif

    __attribute__((unused)) size_t i = 0;
    for (page_boundary = (uint64_t)addr;
         page_boundary < (uint64_t)addr + length; i++) {
        int index = 0;
#ifndef ONE_MEM_REQUEST
        req_mem_size = remaining_length > MAX_MEM_LEN_PER_REQ
                           ? MAX_MEM_LEN_PER_REQ
                           : remaining_length;
        response = ipc_alloc_space(ctx, (void*)page_boundary, req_mem_size,
                                   /*map_fixed=*/false);
        if (response->header.status != SUCCESS) {
            LOG_ERR("ucm failed to allocate memory");
            // TODO: return to app gracefully; clean up mmaped memory.
            assert(0);
        }

        remaining_length -= req_mem_size;
#endif
        int num_pages = response->num_pages;
        while (index < num_pages) {
            page = &(response->pages[index++]);
            assert(page != NULL);

            offset = page->devdax_offset;
            in_dram = page->in_dram;
            pagesize = pt_to_pagesize(page->pt);

            int fd =
                in_dram ? ctx->mem_ctx.fastmem_fd : ctx->mem_ctx.slowmem_fd;

            // now that we have an offset determined via the policy algorithm,
            // actually map the page for the application
            newptr = libc_mmap(
                (void*)page_boundary, pagesize, PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, offset);
            if (newptr == MAP_FAILED) {
                perror("newptr mmap");
                assert(0);
            }

            if (newptr != (void*)page_boundary) {
                LOG_ERR("mmap populate: newptr %p != page boundary %p\n",
                        newptr, (void*)page_boundary);
                assert(0);
            }

            LOG_DEBUG("mmap_populate mapping page va %p\n",
                      (void*)page_boundary);

            // register mmap region with userfaultfd
            if (uffd_register_page(ctx->uffd, page_boundary, pagesize) < 0) {
                LOG_ERR("uffd registration failed for page %p\n",
                        (void*)page_boundary);
                // assert(0);
            }

            assert((uint64_t)newptr != 0);
            assert((uint64_t)newptr % pagesize == 0);

            page_boundary += pagesize;
        }
    }

    LOG_DEBUG("mmap_populate done\n");
}

static void hemem_mmap_fixed(app_ctx_t* ctx, void* addr, size_t length) {
    assert(addr != 0);
    assert(length != 0);

    uint64_t page_boundary = PAGE_ROUND_DOWN((uint64_t)addr);

    struct alloc_response* response =
        ipc_alloc_space(ctx, (void*)page_boundary, length,
                        /*map_fixed=*/true);
    if (response->header.status != 0) {
        perror("hemem_mmap_fixed alloc fails");
        assert(0);
    }

    int num_pages = response->num_pages;
    assert(num_pages > 0);
    int index = 0;
    uint64_t curr_mmap_addr = (uint64_t)addr;
    while (index < num_pages) {
        struct hemem_page_app* page = &(response->pages[index]);
        assert(page != NULL);

        uint64_t pagesize = pt_to_pagesize(page->pt);

        curr_mmap_addr = page->va;
        size_t mmap_len = pagesize;

        LOG_DEBUG("hemem_mmap_fixed va %p len %lu\n", (void*)curr_mmap_addr,
                  mmap_len);

        int fd =
            (page->in_dram) ? ctx->mem_ctx.fastmem_fd : ctx->mem_ctx.slowmem_fd;
        size_t offset = page->devdax_offset;

        void* mmaped =
            libc_mmap((void*)curr_mmap_addr, mmap_len, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, offset);
        if (mmaped == MAP_FAILED) {
            LOG_ERR("mmap_fixed failed at va %p len %lu page # %d errno %u\n",
                    (void*)curr_mmap_addr, mmap_len, index, errno);
            exit(1);
        }

        // introducing another MAP_FIXED mapping unmaps the original uffd.
        // Let's add it back.
        if (uffd_register_page(ctx->uffd, PAGE_ROUND_DOWN(curr_mmap_addr),
                               pagesize) < 0) {
            assert(0);
        }

        curr_mmap_addr += mmap_len;
        index++;
    }
}

void* hemem_mmap_impl(app_ctx_t* ctx, void* addr, size_t length, int prot,
                      int flags, int fd, off_t offset) {
    void* p;

    internal_call = true;

    assert(fd == -1);

    __attribute__((unused)) bool did_change_map_priv_to_shared = false;
    __attribute__((unused)) bool did_unset_map_anon = false;
    __attribute__((unused)) bool did_unset_map_huge = false;
    __attribute__((unused)) bool did_add_prot_write = false;

    assert(length != 0);

    LOG_DEBUG(
        "hemem_mmap user args: addr %p, length %lu prot %d flags %d fd %d offt "
        "%lu\n",
        addr, length, prot, flags, fd, offset);

    if (fd != -1) {
        LOG_ERR("non-anonymous mappings not supported; received fd %d\n", fd);
        return MAP_FAILED;
    }
    if (offset != 0) {
        // offset != 0 is invalid for non-file mappings
        LOG_ERR("non-zero offset not supported; received offset %lu\n", offset);
        return MAP_FAILED;
    }

    if ((flags & MAP_PRIVATE) == MAP_PRIVATE) {
        flags &= ~MAP_PRIVATE;
        flags |= MAP_SHARED;
        did_change_map_priv_to_shared = true;
    }

    if ((flags & MAP_ANONYMOUS) == MAP_ANONYMOUS) {
        flags &= ~MAP_ANONYMOUS;
        did_unset_map_anon = true;
    }

    if ((flags & MAP_HUGETLB) == MAP_HUGETLB) {
        flags &= ~MAP_HUGETLB;
        did_unset_map_huge = true;
    }

    // if the user wanted MAP_ANON, then we need to zero the mmaped memory.
    // this requires the mmaped region to be writeable. therefore, add
    // PROT_WRITE if user didn't set this, and revert after zeroing memory.
    if (did_unset_map_anon && ((prot & PROT_WRITE) != PROT_WRITE)) {
        prot |= PROT_WRITE;
        did_add_prot_write = true;
    }

    if (addr != NULL) {
        if ((flags & MAP_FIXED) == MAP_FIXED) {
            // application requested MAP_FIXED. request ucm to map at this
            // address. we assume that the app already has memory allocated
            // here; err or abort if not.
            hemem_mmap_fixed(ctx, addr, length);
            internal_call = false;
            return addr;
        } else {
            LOG_WARN("provided addr %p but not configured to MAP_FIXED\n",
                     (void*)addr);
            // we use MAP_FIXED anyway to avoid mmap issues on our memory fd.
            flags |= MAP_FIXED;
        }
    }

    LOG_DEBUG("hemem_mmap add MAP_SHARED %d rm MAP_ANON %d rm MAP_HUGETLB %d\n",
              did_change_map_priv_to_shared, did_unset_map_anon,
              did_unset_map_huge);

    size_t length_orig = length;
    // reserve block of memory
    length = PAGE_ROUND_UP(length);
    if (length > length_orig) {
        mem_amplification += (length - length_orig);
    }

    // this is only for allocating a virtual address for the allocation.
    // therefore, the fd argument doesn't matter here.
    p = libc_mmap(addr, length, prot, flags, /*fd=*/ctx->mem_ctx.fastmem_fd,
                  /*offset=*/0);
    if (p == MAP_FAILED) {
        return MAP_FAILED;
    }
    if (addr != NULL) {
        assert(p == addr);
    }

    hemem_mmap_populate(ctx, p, length);

    if (did_unset_map_anon) {
        // MAP_ANONYMOUS guarantees that mmapped memory is zero.
        memset(p, 0x0, length);
    }

    // if the user didn't want PROT_WRITE (but we added it), we unset it.
    if (did_add_prot_write) {
        flags &= ~PROT_WRITE;
        mprotect(p, length, flags);
    }

    internal_call = false;

    LOG_DEBUG("hemem_mmap done\n");

    return p;
}

int hemem_munmap_impl(app_ctx_t* ctx, void* addr, size_t length) {
    int ret;
    uint64_t page_boundary;

    internal_call = true;

    LOG_DEBUG("hemem_munmap addr %p len %lu\n", addr, length);

#ifdef ONE_MEM_REQUEST
    free_space(addr, length);
#else
    size_t remaining_length = length;
    size_t req_mem_size;
    for (page_boundary = (uint64_t)addr;
         page_boundary < (uint64_t)addr + length;) {
        req_mem_size = (remaining_length > MAX_MEM_LEN_PER_REQ)
                           ? MAX_MEM_LEN_PER_REQ
                           : remaining_length;
        ipc_free_space(ctx, (void*)page_boundary, req_mem_size);
        remaining_length -= req_mem_size;
        page_boundary += req_mem_size;
    }
#endif

    // we unmap the uffdio_range one page at a time.
    // this is because we registered once per page in hemem_mmap_populate.
    // TODO: revisit page size.
    size_t iters = 0;
    for (page_boundary = (uint64_t)addr;
         page_boundary < (uint64_t)addr + length;
         page_boundary += HUGEPAGE_SIZE, iters++) {
        if (uffd_unregister_page(ctx->uffd, page_boundary, HUGEPAGE_SIZE) < 0) {
            LOG_ERR("range start: %p, len %lu iter %lu\n", addr, length, iters);
            assert(0);
        }
    }

    // we round up the length in the munmap, because we did page-size rounding
    // in the mapping code in hemem_mmap.
    // munmap fails if the <addr, length> pair does not match what was provided
    // when the mapping was originally created.
    ret = libc_munmap(addr, PAGE_ROUND_UP(length));
    if (ret != 0) {
        perror("libc_munmap");
        LOG_ERR("libc_munmap: %p, len %lu\n", addr, length);

        assert(0);
    }

    internal_call = false;

    LOG_DEBUG("hemem_munmap done\n");

    return ret;
}
