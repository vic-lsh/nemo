#ifndef HEMEM_FIFO_H
#define HEMEM_FIFO_H

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct page_list {
    struct hemem_page *first, *last;
    pthread_mutex_t list_lock;
    volatile size_t numentries;
};

struct process_list {
    struct hemem_process *first, *last;
    pthread_mutex_t list_lock;
    volatile size_t numentries;
};

#define PROCESS_LIST_FOR_EACH(list, iter)                          \
    for (struct hemem_process *iter = (list)->first, *next = NULL; \
         iter != NULL && ({                                        \
             next = iter->next;                                    \
             1;                                                    \
         });                                                       \
         iter = next)

void enqueue_page(struct page_list *list, struct hemem_page *page);
struct hemem_page *dequeue_page(struct page_list *list);
void page_list_remove(struct page_list *list, struct hemem_page *page);
struct hemem_page *next_page(struct page_list *list, struct hemem_page *page);
struct hemem_page *prev_page(struct page_list *list, struct hemem_page *page);

void enqueue_process(struct process_list *list, struct hemem_process *process);
void process_list_remove(struct process_list *list,
                         struct hemem_process *process);
struct hemem_process *peek_process(struct process_list *list);
#endif
