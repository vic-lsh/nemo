#include <unistd.h>

#include "core.h"
#include "ipc/ipc.h"
#include "mm.h"
#include "opts/opts.h"
#include "proc-mgr.h"
#include "signal-handler.h"
#include "stats/stats.h"
#include "support/memops.h"
#include "uffd.h"
#include "util/log.h"

int main(int argc, char *argv[]) {
    int ret;

    struct ucm_opts opts;
    switch (parse_args(&opts, argc, argv)) {
        case ARG_PARSE_HELP:
            exit(0);
        case ARG_PARSE_FAILED:
            exit(1);
        default:
            break;
    }
    pprint_ucm_opts(&opts);

    signal_handlers_init();

    mm_init(&opts.mm);
    proc_mgr_init();

    ret = uffd_init();
    if (ret != 0) {
        LOG_ERR("failed to initialize uffd handling logic\n");
        exit(1);
    }

    ret = ipc_init_ucm();
    if (ret != 0) {
        LOG_ERR("ipc_init_ucm failed\n");
        exit(1);
    }

    memops_init();
    ucm_stats_init(&opts);

    ret = ucm_core_enter(&opts);
    if (ret != 0) {
        LOG_ERR("ucm exiting in error\n");
        exit(1);
    }
}
