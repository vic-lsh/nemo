#pragma once

/**
 * Given a path possibly including directories, create parent directories if
 * they do not exist.
 *
 * Example:
 * For path `/tmp/test/out.txt`, this function would ensure that `/tmp/test` is
 * a directory, or return with error.
 *
 * @return
 * 0 on success, -1 on failure.
 */
int mk_parent_dirs(const char* path);
