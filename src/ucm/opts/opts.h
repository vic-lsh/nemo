#pragma once

#include <stdbool.h>

#include "mm.h"
#include "policy/policy.h"

struct ucm_opts {
    struct policy_opts policy;
    mm_opts_t mm;
};

enum arg_parse_result {
    ARG_PARSE_SUCCEEDED,
    ARG_PARSE_FAILED,
    ARG_PARSE_HELP,
};

/**
 * Parses input to main into runtime options.
 */
enum arg_parse_result parse_args(struct ucm_opts *opts, int argc, char *argv[]);

void pprint_ucm_opts(struct ucm_opts *opts);
