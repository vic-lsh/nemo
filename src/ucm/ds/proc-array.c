#include "proc-array.h"

bool process_array_init(process_array_t *arr, size_t capacity) {
    arr->container = calloc(capacity, sizeof(struct hemem_process *));
    if (!arr->container) {
        return false;
    }

    arr->size = 0;
    arr->capacity = capacity;

    return true;
}

void process_array_destroy(process_array_t *arr) { free(arr->container); }

void process_array_clear(process_array_t *arr) { arr->size = 0; }

bool process_array_append(process_array_t *arr, struct hemem_process *p) {
    if (unlikely(arr->size >= arr->capacity)) {
        return false;
    }
    arr->container[arr->size++] = p;
    return true;
}

struct hemem_process *process_array_next(process_array_t *arr,
                                         process_array_iter_t *it) {
    if (it->curr >= arr->size) {
        return NULL;
    }
    return arr->container[it->curr++];
}

void process_array_iter_init(process_array_iter_t *it) { it->curr = 0; }
