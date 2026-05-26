#pragma once

#include <assert.h>

/**
 * @brief Statically asserts that the signature of a function matches a
 * given function pointer typedef.
 *
 * @param func_ptr A pointer to the function to check (e.g., &my_function).
 * @param func_type_t The typedef'd function pointer type to match against.
 */
#define ASSERT_SIGNATURE(func_ptr, func_type_t)                       \
    static_assert(_Generic((func_ptr), func_type_t : 1, default : 0), \
                  "Function signature does not match the expected typedef.")
