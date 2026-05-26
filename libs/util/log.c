#include "util/log.h"

#include <assert.h>
#include <execinfo.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

FILE* hememlogf;
FILE* hotpagesf;

FILE* timef;
bool timing = false;

FILE* statsf;

void log_init(__attribute__((unused)) const char* logname) {
    // char logbuffer[64];
    // snprintf(logbuffer, 64, "/tmp/log_%s.txt", logname);
    // hememlogf = fopen(logbuffer, "w+");
    // if (hememlogf == NULL) {
    //     perror("log file open");
    //     assert(0);
    // }

    char hotpages[64];
    snprintf(hotpages, 64, "./log_%s.txt", "hot");
    hotpagesf = fopen(hotpages, "w+");
    if (hotpagesf == NULL) {
        perror("log file open");
        assert(0);
    }

    // char timebuffer[64];
    // snprintf(timebuffer, 64, "/tmp/times_%s.txt", logname);
    // timef = fopen(timebuffer, "w+");
    // if (timef == NULL) {
    //     perror("time file open");
    //     assert(0);
    // }

    // char statsbuffer[64];
    // snprintf(statsbuffer, 64, "/tmp/stats_%s.txt", logname);
    // statsf = fopen(statsbuffer, "w+");
    // if (statsf == NULL) {
    //     perror("stats file open");
    //     assert(0);
    // }
}

void print_stacktrace() {
    void* buffer[10];
    char** symbols;
    int size;

    // Get the stack frames
    size = backtrace(buffer, 10);

    // Get the symbols for the stack frames
    symbols = backtrace_symbols(buffer, size);

    // Print the stack trace
    fprintf(stderr, "Stack trace:\n");
    for (int i = 0; i < size; i++) {
        fprintf(stderr, "%s\n", symbols[i]);
    }

    // Free the memory allocated for symbols
    free(symbols);
}
