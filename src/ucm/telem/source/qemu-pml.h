#pragma once

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

void process_pml_bitmap(void* bitmap, size_t bytes, pid_t pid, uint64_t hva);
