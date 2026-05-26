#include "type/page.h"

struct hemem_page *hemem_page_alloc() {
    struct hemem_page *p = calloc(1, sizeof(struct hemem_page));
    if (!p) return NULL;
    p->devdax_offset = -1;
    p->pid = -1;  // other parts of the code base depends on pids == -1 for
                  // uninitialized pid.
    pthread_mutex_init(&(p->remap_lock), NULL);
    return p;
}
