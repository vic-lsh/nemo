#ifndef HEMEM_PA_PROC_MAP_H
#define HEMEM_PA_PROC_MAP_H

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include "ds/uthash.h"
#include "hemem-shared.h"
#include "type/process.h"

struct hemem_page;

struct pa_proc_map_entry {
    uint64_t pa;
    struct hemem_process* proc;
    bool used;
    UT_hash_handle hh;
};

struct pa_proc_map {
    pthread_mutex_t mutex;
    /* hashmap root */
    struct pa_proc_map_entry* root;
};

struct hemem_page;

void pa_proc_map_init(struct pa_proc_map* map);
void pa_proc_map_add(struct pa_proc_map* map, struct hemem_page* page,
                     struct hemem_process* proc);
void pa_proc_map_remove(struct pa_proc_map* map, struct hemem_page* page);
struct hemem_process* pa_proc_map_find(struct pa_proc_map* map, uint64_t pa);

#endif
