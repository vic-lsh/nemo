#ifndef HEMEM_UCM_MIGRATION_H
#define HEMEM_UCM_MIGRATION_H

#include <stdbool.h>
#include <stdint.h>

#include "type/page.h"
#include "type/process.h"

uint64_t process_migrate_up_bytes(struct hemem_process *process,
                                  uint64_t migrate_up_bytes);
uint64_t process_migrate_down_bytes(struct hemem_process *process,
                                    uint64_t migrate_down_bytes);
void demote_one_page(struct hemem_process *process, struct hemem_page *page,
                     uint64_t slowmem_offset);
void promote_one_page(struct hemem_process *process, struct hemem_page *page,
                      uint64_t fastmem_offset);

#endif /* HEMEM_UCM_MIGRATION_H */
