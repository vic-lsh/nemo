#include "ds/pa-proc-map.h"
#include "type/page.h"
#include "type/process.h"
#include "unity.h"

void setUp(void) {
    // This is run before EACH test
}

void tearDown(void) {
    // This is run after EACH test
}

int add(int a, int b) { return a + b; }

void test_find_nothing_in_new_map() {
    struct pa_proc_map pa_to_proc_map;
    pa_proc_map_init(&pa_to_proc_map);
    struct hemem_process *proc = pa_proc_map_find(&pa_to_proc_map, 1234);
    TEST_ASSERT_NULL(proc);
}

void test_map_insertion() {
    struct pa_proc_map map;
    struct hemem_process proc;
    struct hemem_page page;

    pa_proc_map_init(&map);

    memset(&proc, 0, sizeof(struct hemem_process));
    memset(&page, 0, sizeof(struct hemem_page));

    page.in_dram = 0;
    page.devdax_offset = 0;

    pa_proc_map_add(&map, &page, &proc);
    struct hemem_process *proc_found =
        pa_proc_map_find(&map, page.devdax_offset);

    TEST_ASSERT(proc_found == &proc);
}

void test_map_removal() {
    struct pa_proc_map map;
    struct hemem_process proc;
    struct hemem_page page;

    pa_proc_map_init(&map);

    memset(&proc, 0, sizeof(struct hemem_process));
    memset(&page, 0, sizeof(struct hemem_page));

    page.in_dram = 0;
    page.devdax_offset = 0;

    pa_proc_map_add(&map, &page, &proc);
    pa_proc_map_remove(&map, &page);
    struct hemem_process *proc_found =
        pa_proc_map_find(&map, page.devdax_offset);

    TEST_ASSERT_NULL(proc_found);
}
