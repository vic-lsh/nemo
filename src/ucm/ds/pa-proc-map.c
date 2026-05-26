#include "pa-proc-map.h"

#include <pthread.h>
#include <stdlib.h>

#include "type/page.h"
#include "type/process.h"
#include "util/compiler.h"
#include "util/log.h"

void pa_proc_map_add(struct pa_proc_map* map, struct hemem_page* page,
                     struct hemem_process* proc) {
    assert(page);
    assert(proc);
    assert(!page->in_dram);

    struct pa_proc_map_entry* entry = &page->pa_proc_entry;

    pthread_mutex_lock(&(map->mutex));

    assert(!entry->used);
    entry->pa = page->devdax_offset;
    entry->proc = proc;
    entry->used = true;
    HASH_ADD(hh, map->root, pa, sizeof(uint64_t), entry);

    pthread_mutex_unlock(&(map->mutex));
}

void pa_proc_map_remove(struct pa_proc_map* map, struct hemem_page* page) {
    struct pa_proc_map_entry* entry = &page->pa_proc_entry;
    if (!entry->used) {
        return;
    }
    pthread_mutex_lock(&(map->mutex));
    if (entry->used) {
        HASH_DELETE(hh, map->root, entry);
        memset(&page->pa_proc_entry, 0, sizeof(struct pa_proc_map_entry));
    }
    pthread_mutex_unlock(&(map->mutex));
}

struct hemem_process* pa_proc_map_find(struct pa_proc_map* map, uint64_t pa) {
    struct pa_proc_map_entry* entry = NULL;
    pthread_mutex_lock(&(map->mutex));
    HASH_FIND(hh, map->root, &pa, sizeof(uint64_t), entry);
    pthread_mutex_unlock(&(map->mutex));
    if (entry) {
        assert(entry->proc);
        return entry->proc;
    }
    return NULL;
}

void pa_proc_map_init(struct pa_proc_map* map) {
    pthread_mutex_init(&(map->mutex), NULL);
    map->root = 0;
}
