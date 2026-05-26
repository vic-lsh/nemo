#include "parse_impl.h"

#include "opts-list.h"
#include "opts.h"
#include "util/log.h"

#define ARRAY_FOR_EACH(array, item)        \
    for (typeof(array[0]) *item = (array); \
         item < (array) + (sizeof(array) / sizeof((array)[0])); ++item)

#define ARGLIST_FOR_EACH(arg)                                              \
    for (const struct ucm_arg *arg = get_arg_list(),                       \
                              *_end = get_arg_list() + get_arg_list_len(); \
         arg < _end; arg++)

static void set_arg_defaults(struct ucm_opts *opts) {
    memset(opts, 0, sizeof(*opts));

    ARGLIST_FOR_EACH(arg) {
        if (arg->set_default) {
            arg->set_default(opts);
        }
    }
}

static void print_help() {
    LOG_NOPATH("ucm args list:\n");
    ARGLIST_FOR_EACH(arg) {
        char *value = (arg->has_value) ? "<value>" : "";
        LOG_NOPATH("    %s %s\t%s\n", arg->name, value, arg->desc);
    }
}

static enum arg_parse_result parse_args_internal(struct ucm_opts *opts,
                                                 int argc, char *argv[]) {
    // TODO: we don't validate whether non-optional args are provided rn.

    for (int i = 1; i < argc; i++) {
        // special-case help option;
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return ARG_PARSE_HELP;
        }

        bool handled = false;
        ARGLIST_FOR_EACH(arg) {
            if (strcmp(argv[i], arg->name) == 0) {
                const char *value = NULL;
                if (arg->has_value) {
                    if (i + 1 >= argc) {
                        LOG_ERR(
                            "Error: option '%s' requires an "
                            "argument.\n",
                            arg->name);
                        return ARG_PARSE_FAILED;
                    }
                    // check that the value doesn't look like another
                    // option.
                    if (argv[i + 1][0] == '-') {
                        LOG_ERR(
                            "Error: option '%s' requires an argument, "
                            "but "
                            "found another option '%s' instead.\n",
                            arg->name, argv[i + 1]);
                        return ARG_PARSE_FAILED;
                    }
                    value = argv[i + 1];
                    i++;
                }

                if (arg->set(opts, value) != 0) {
                    LOG_ERR("failed to parse option '%s'\n", arg->name);
                    return ARG_PARSE_FAILED;
                }
                handled = true;
                break;
            }
        }
        if (!handled) {
            LOG_ERR("encountered invalid option '%s'\n", argv[i]);
            print_help();
            return ARG_PARSE_FAILED;
        }
    }

    return ARG_PARSE_SUCCEEDED;
}

enum arg_parse_result parse_args_impl(struct ucm_opts *opts, int argc,
                                      char *argv[]) {
    set_arg_defaults(opts);
    return parse_args_internal(opts, argc, argv);
}
