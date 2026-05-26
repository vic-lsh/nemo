#include "signal-handler.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "policy/policy.h"
#include "proc-mgr.h"
#include "type/process.h"
#include "ucm-config.h"
#include "util/log.h"
#ifdef CONFIG_PEBS
#include "telem/source/pebs.h"
#endif

static void ucm_update_miss_ratio(int signum) {
    if (signum != SIGUSR2) {
        return;
    }

    const char *new_miss_fname = "/tmp/miss_ratio_update";
    struct hemem_process *p;

    FILE *new_miss_file = fopen(new_miss_fname, "r");
    assert(new_miss_file != NULL);
    int pid = -1;
    float new_miss = -1.0;
    int ret = fscanf(new_miss_file, "%d:%f", &pid, &new_miss);
    assert(ret >= 0);

    p = find_process(pid);

    if (p == NULL) {
        LOG_ERR("No process with PID %d currently managed", pid);
        return;
    }
    process_policy_set_miss_ratio(&p->policy, (double)new_miss);
    LOG("updated miss ratio of proc: %d to %f\n", pid, new_miss);
}

void signal_handlers_init() {
#ifdef CONFIG_PEBS
    if (signal(SIGUSR1, pebs_shutdown_signal_handler) == SIG_ERR) {
        assert(0 && "Failed to map SIGUSR1 to its handler.");
    }
#endif

    if (signal(SIGUSR2, ucm_update_miss_ratio) == SIG_ERR) {
        assert(0 && "Failed to map SIGUSR2 to the missratio update.");
    }
}
