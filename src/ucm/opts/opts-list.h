#pragma once

#include <stdbool.h>
#include <stddef.h>

struct ucm_opts;

struct ucm_arg {
    /**
     * The string of the flag the user passes in.
     */
    char *name;
    /**
     * Short description of what the flag does.
     */
    char *desc;
    /**
     * Whether the argument has a user-provided value. If yes, argument is of
     * the form `--flag <value>`. Otherwise, the user passes in `--flag`.
     */
    bool has_value;
    /**
     * Sets the default value for this argument.
     *
     * This is a nullable fn pointer. If provided, the argument is optional.
     */
    void (*set_default)(struct ucm_opts *);
    /**
     * Sets the option based on user argument.
     *
     * Value is NULL if `has_value == false`.
     */
    int (*set)(struct ucm_opts *, const char *value);
};

const struct ucm_arg *get_arg_list();

size_t get_arg_list_len();
