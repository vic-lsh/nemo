#include "util/fs.h"

#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int mk_parent_dirs(const char* path) {
    char* path_copy = strdup(path);
    if (!path_copy) {
        perror("strdup");
        return -1;
    }

    char* dir_path = dirname(path_copy);

    // If dirname returns "." or "/", no directories to create
    if (strcmp(dir_path, ".") == 0 || strcmp(dir_path, "/") == 0) {
        free(path_copy);
        return 0;
    }

    struct stat st;
    if (stat(dir_path, &st) == 0) {
        // Directory already exists
        free(path_copy);
        return 0;
    }

    // Recursively create parent directories
    char* parent_copy = strdup(dir_path);
    if (!parent_copy) {
        perror("strdup");
        free(path_copy);
        return -1;
    }

    if (mk_parent_dirs(parent_copy) == -1) {
        free(parent_copy);
        free(path_copy);
        return -1;
    }
    free(parent_copy);

    // Create this directory
    if (mkdir(dir_path, 0755) == -1) {
        perror("mkdir");
        free(path_copy);
        return -1;
    }

    free(path_copy);
    return 0;
}
