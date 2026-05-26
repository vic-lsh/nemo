#include "qemu-pml.h"

#include <stdint.h>

#include "ds/fifo.h"
#include "mm.h"
#include "proc-mgr.h"
#include "type/process.h"
#include "util/bitmap.h"
#include "util/bitops.h"
#include "util/log.h"

void process_pml_bitmap(void* bitmap, size_t bytes, pid_t pid, uint64_t hva) {
    const size_t bitmap_page_sz = 4096;

    struct hemem_process* process = find_process(pid);
    assert(process);

    size_t nbits = bytes * 8;
    unsigned long ndirty_bits = bitmap_count_one_with_offset(bitmap, 0, nbits);

    size_t pages_found = 0;

    unsigned long bit = 0;
    while (1) {
        bit = find_next_bit(bitmap, nbits, bit);
        if (bit >= nbits) {
            break;
        }
        assert(test_and_clear_bit(bit, bitmap));
        size_t vm_pa_offset = bit * bitmap_page_sz;

        uint64_t va = hva + vm_pa_offset;
        struct hemem_page* page = mm_find_page(&process->mm, va);
        if (page) {
            pages_found++;
        }
    }

    LOG("got %lu dirty base pages %lu found\n", ndirty_bits, pages_found);
}
