#ifndef HEMEM_CORE_H
#define HEMEM_CORE_H

#include <inttypes.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <stdint.h>

#include "opts/opts.h"
#include "type/page.h"
#include "type/process.h"
#include "ucm-config.h"

int ucm_core_enter(struct ucm_opts* opts);

size_t ucm_get_curr_epoch();

#endif /*  HEMEM_CORE_H  */
