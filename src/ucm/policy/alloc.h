#pragma once

#include "type/page.h"
#include "type/process.h"

enum ALLOC_MODE {
    FASTMEM_PREFERRED,
    SLOWMEM_PREFERRED,
};

const char *pprint_alloc_mode(enum ALLOC_MODE mode);

typedef struct hemem_page *(*page_alloc_fn_t)(struct hemem_process *process);

page_alloc_fn_t get_alloc_fn_by_mode(enum ALLOC_MODE mode);
