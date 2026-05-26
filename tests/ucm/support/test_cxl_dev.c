#include "cxl-dev/cxl-dev.h"
#include "unity.h"

void setUp(void) {
    // This is run before EACH test
}

void tearDown(void) {
    // This is run after EACH test
}

int add(int a, int b) { return a + b; }

void test_function(void) { TEST_ASSERT_EQUAL(4, add(2, 2)); }
