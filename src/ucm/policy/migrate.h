#ifndef UCM_POLICY_MIGRATE_H
#define UCM_POLICY_MIGRATE_H

#include "type/page.h"
#include "type/process.h"

struct hemem_page *find_hottest_slowmem_page(struct hemem_process *process);
struct hemem_page *find_coldest_fastmem_page(struct hemem_process *process);
struct hemem_page *find_oldest_fastmem_page(struct hemem_process *process);

#endif /* UCM_POLICY_MIGRATE_H */
