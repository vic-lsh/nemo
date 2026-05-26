#include "opts/opts.h"

#include <ctype.h>
#include <stdbool.h>

#include "opts/parse_impl.h"
#include "physmem/config.h"
#include "policy/alloc.h"
#include "support/memops.h"
#include "util/compiler.h"
#include "util/log.h"
#include "util/shared.h"

enum arg_parse_result parse_args(struct ucm_opts *opts, int argc,
                                 char *argv[]) {
    return parse_args_impl(opts, argc, argv);
}

void pprint_ucm_opts(struct ucm_opts *opts) {
    LOG_NOPATH("=====================================\n");
    LOG_NOPATH("UCM runtime opts\n\n");

    pprint_mm_opts(&opts->mm);
    pprint_policy_opts(&opts->policy);
    pprint_memops_opts();

    LOG_NOPATH("=====================================\n");
}
