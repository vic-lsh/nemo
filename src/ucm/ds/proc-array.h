#ifndef UCM_DS_PROC_ARRAY_H
#define UCM_DS_PROC_ARRAY_H

/**
 * Simple array container utility for managing an array of process pointers.
 */

#include <stddef.h>

#include "type/process.h"

/**
 * The process array.
 */
typedef struct {
    struct hemem_process **container;
    size_t size;
    size_t capacity;
} process_array_t;

/**
 * Iterator for the process array.
 */
typedef struct {
    size_t curr;
} process_array_iter_t;

/**
 * Initializes a process array. After initialization, the array can store
 * up to `capacity` number of process pointers.
 *
 * @return
 * Whether initialization was successful.
 */
bool process_array_init(process_array_t *arr, size_t capacity);

/**
 * Deallocates memory within a process array.
 *
 * Since `process_array_init` does not allocate the `process_array_t` struct,
 * the caller is responsible for freeing the process array itself.
 */
void process_array_destroy(process_array_t *arr);

/**
 * Resets the array's content without freeing the memory it manages.
 */
void process_array_clear(process_array_t *arr);

/**
 * Adds a process to the end of the array.
 *
 * @return
 * Whether the insertion was successful. Insertion fails when the array is
 * already at capacity.
 */
bool process_array_append(process_array_t *arr, struct hemem_process *p);

/**
 * Initializes an array iterator.
 */
void process_array_iter_init(process_array_iter_t *it);

/**
 * Advances the array iterator.
 *
 * @return
 * The next process within the array, or NULL if end of array is reached.
 */
struct hemem_process *process_array_next(process_array_t *arr,
                                         process_array_iter_t *it);

#endif /* UCM_DS_PROC_ARRAY_H */
