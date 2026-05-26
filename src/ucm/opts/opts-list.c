#include "opts-list.h"

#include <ctype.h>
#include <stdbool.h>

#include "config.h"
#include "opts.h"
#include "physmem/config.h"
#include "policy/alloc.h"
#include "util/compiler.h"
#include "util/log.h"
#include "util/shared.h"

#define FASTMEM_SIZE (25 * (1024L * 1024L * 1024L))
#define SLOWMEM_SIZE (CXL_USABLE_SIZE)

// ===================== ArgList Definition ==================================

static void arg_no_migrate_set_default(struct ucm_opts *opts) {
    opts->policy.enable_migration = true;
}

static int arg_no_migrate_set_usr_opt(struct ucm_opts *opts,
                                      const char *value __always_unused) {
    opts->policy.enable_migration = false;
    return 0;
}

static void arg_no_dax_set_default(struct ucm_opts *opts) {
#ifdef CONFIG_CXL_TELEM
    opts->mm.physmem_mode = USE_DAX;
#else
    opts->mm.physmem_mode = USE_SHM;
#endif
}

static int arg_no_dax_set_usr_opt(struct ucm_opts *opts, const char *value) {
    size_t len = strlen(value);
    char lower_value[len + 1];

    for (size_t i = 0; i < len; i++) {
        lower_value[i] = tolower((unsigned char)value[i]);
    }
    lower_value[len] = '\0';

    if (strcmp(lower_value, "shm") == 0) {
        opts->mm.physmem_mode = USE_SHM;
    } else if (strcmp(lower_value, "dax") == 0) {
        opts->mm.physmem_mode = USE_DAX;
    } else {
        LOG_ERR(
            "Error: invalid value for --physmem-mode: '%s'. "
            "Expected 'shm' or 'dax'.\n",
            value);
        return -1;
    }

    return 0;
}

static void alloc_pref_set_default(struct ucm_opts *opts) {
    opts->policy.alloc_mode = FASTMEM_PREFERRED;
}

static int alloc_pref_set_usr_opt(struct ucm_opts *opts, const char *value) {
    size_t len = strlen(value);
    char lower_value[len + 1];

    for (size_t i = 0; i < len; i++) {
        lower_value[i] = tolower((unsigned char)value[i]);
    }
    lower_value[len] = '\0';

    enum ALLOC_MODE mode_parsed;
    if (strcmp(lower_value, "fast") == 0) {
        mode_parsed = FASTMEM_PREFERRED;
    } else if (strcmp(lower_value, "slow") == 0) {
        mode_parsed = SLOWMEM_PREFERRED;
    } else {
        LOG_ERR(
            "Error: invalid value for --alloc-pref '%s'. "
            "Expected 'fast' or 'slow'.\n",
            value);
        return -1;
    }

    opts->policy.alloc_mode = mode_parsed;

    return 0;
}

/**
 * @brief Parses a string representing a file size (e.g., "1GB", "2.5mb").
 *
 * The size string must start with a positive floating-point number,
 * followed by a suffix like 'g', 'gb', 'm', 'mb', 'k', or 'kb'
 * (case-insensitive).
 *
 * @param size_str The input string to parse.
 * @param parsed_bytes A pointer to a double where the parsed size in bytes will
 * be stored.
 * @return 0 on success, or -1 if failed.
 */
static int parse_capacity_str(const char *size_str, size_t *parsed_bytes) {
    if (size_str == NULL || parsed_bytes == NULL) {
        LOG_ERR(
            "Error: Invalid input pointers (size_str or parsed_bytes is "
            "NULL).\n");
        return -1;
    }

    double value;
    char *endptr;

    // Use strtod to parse the floating-point number and get a pointer to the
    // suffix
    value = strtod(size_str, &endptr);

    // Check if no number was found at the beginning or if the entire string was
    // consumed without a suffix
    if (endptr == size_str) {
        LOG_ERR(
            "Error: '%s' is an invalid size string. No valid number found "
            "at the beginning.\n",
            size_str);
        return -1;
    }

    // Check if the number is positive
    if (value <= 0) {
        LOG_ERR(
            "Error: '%s' is an invalid size string. The size must be a "
            "positive number.\n",
            size_str);
        return -1;
    }

    // Determine the suffix based on the remaining part of the string
    const char *suffix = endptr;

    // Skip any potential whitespace between number and suffix
    while (*suffix != '\0' && (*suffix == ' ' || *suffix == '\t' ||
                               *suffix == '\n' || *suffix == '\r')) {
        suffix++;
    }

    // Check for suffix
    if (*suffix == '\0') {
        LOG_ERR(
            "Error: '%s' is an invalid size string. Missing size unit "
            "(e.g., 'GB', 'MB', 'KB').\n",
            size_str);
        return -1;
    }

    // Convert suffix to lowercase for case-insensitive comparison
    // We'll create a temporary buffer for the lowercase suffix.
    // Max length of "KB" or "MB" is 2. Let's make it 4 to be safe (including
    // null terminator).
    char lower_suffix[4];
    size_t suffix_len = strlen(suffix);
    if (suffix_len > 3) {
        LOG_ERR(
            "Error: '%s' is an invalid size string. Unrecognized suffix "
            "'%s'.\n",
            size_str, suffix);
        return -1;
    }

    for (size_t i = 0; i < suffix_len; i++) {
        lower_suffix[i] = (char)tolower((unsigned char)suffix[i]);
    }
    lower_suffix[suffix_len] = '\0';  // Null-terminate the temporary string

    if (strcmp(lower_suffix, "g") == 0 || strcmp(lower_suffix, "gb") == 0) {
        *parsed_bytes = GB(value);
    } else if (strcmp(lower_suffix, "m") == 0 ||
               strcmp(lower_suffix, "mb") == 0) {
        *parsed_bytes = MB(value);
    } else if (strcmp(lower_suffix, "k") == 0 ||
               strcmp(lower_suffix, "kb") == 0) {
        *parsed_bytes = KB(value);
    } else {
        LOG_ERR(
            "Error: '%s' is an invalid size string. Unrecognized suffix "
            "'%s'.\n",
            size_str, suffix);
        return -1;
    }

    return 0;  // Success
}

static void fastmem_size_set_default(struct ucm_opts *opts) {
    opts->mm.fastmem_size = FASTMEM_SIZE;
}

static int fastmem_size_set_usr_opt(struct ucm_opts *opts, const char *value) {
    size_t fastmem_sz = 0;
    if (parse_capacity_str(value, &fastmem_sz) < 0) {
        LOG_ERR("failed to parse fastmem size str '%s'\n", value);
        return -1;
    }

    opts->mm.fastmem_size = fastmem_sz;

    return 0;
}

static void slowmem_size_set_default(struct ucm_opts *opts) {
    opts->mm.slowmem_size = SLOWMEM_SIZE;
}

static int slowmem_size_set_usr_opt(struct ucm_opts *opts, const char *value) {
    size_t slowmem_sz = 0;
    if (parse_capacity_str(value, &slowmem_sz) < 0) {
        LOG_ERR("failed to parse slowmem size str '%s'\n", value);
        return -1;
    }

    opts->mm.slowmem_size = slowmem_sz;

    return 0;
}

static struct ucm_arg kArgList[] = {
    {
        .name = "--no-migrate",
        .desc = "Optional argument that disables migration.",
        .has_value = false,
        .set = arg_no_migrate_set_usr_opt,
        .set_default = arg_no_migrate_set_default,
    },
    {
        .name = "--physmem",
        .desc = "Configures how to set up physical memory. Options: dax; shm.",
        .has_value = true,
        .set = arg_no_dax_set_usr_opt,
        .set_default = arg_no_dax_set_default,
    },
    {
        .name = "--alloc-pref",
        .desc = "Whether to allocate from fast or slow "
                "memory if both tiers have free memory. Options: fast; slow.",
        .has_value = true,
        .set = alloc_pref_set_usr_opt,
        .set_default = alloc_pref_set_default,
    },
    {
        .name = "--fastmem-sz",
        .desc = "Configures fast memory capacity. Example: 10G, 512mb, 5.5g.",
        .has_value = true,
        .set = fastmem_size_set_usr_opt,
        .set_default = fastmem_size_set_default,
    },
    {
        .name = "--slowmem-sz",
        .desc = "Configures slow memory capacity. Example: 10G, 512mb, 5.5g",
        .has_value = true,
        .set = slowmem_size_set_usr_opt,
        .set_default = slowmem_size_set_default,
    },
};

__attribute__((unused)) static const size_t kArgListLength =
    sizeof(kArgList) / sizeof(kArgList[0]);

const struct ucm_arg *get_arg_list() { return kArgList; }

size_t get_arg_list_len() { return kArgListLength; }
