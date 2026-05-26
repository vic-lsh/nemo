#include "print.h"

#include <stdio.h>

#include "util/shared.h"

void hello_world() {
    printf("fpga_ctl hello world!\n");
    printf("testing printing that uses shared util lib: 1GB = %lu\n", GB(1));
}
