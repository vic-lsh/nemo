# Contributing to Nemo

This document currently focuses on contributing to the SW portion of Nemo.

## Compilation

See instructions in README.md.

### Language Server support

You may find tools like `clangd` helpful for navigating around the project in your favorite text editor. To help these language servers understand the project structure, you can use a tool called `bear` to generate the exact commands used to compile each target. Using this, language servers should enable features like go to definition and project-wide renaming.

To use `bear`, do:

```shell
$ bear -- make
```

This generates a `compile_commands.json` file. Copy this to the root directory where you'd open your text editor (typically project root, `src/` and `libs/`).

## Adding new functionality

Here's a decision tree how to add new functionality to the codebase:

- Does this functionality belong solely in the UCM, the app-side library, or potentially both?
  - For shared functionality, contribute to `libs/`. Otherwise, add to `src/app` or `src/ucm`.
- For libraries:
  - Does this functionality belong to any of the existing libraries? If so, add files directly.
  - Otherwise, see subsection on creating a new library.
- For adding to `src/app` or `src/ucm`:
  - Does this functionality fit the existing directory hierarchy? If so, add files directly.
  - Otherwise, see subsection on updating the build system to discover a new directory.

### Adding a new library

Edit the `CMakeLists.txt` file to declare a new library. The library declaration looks like the following:

```
# Declare the library name, and its source files
add_library(lib-cxl-dev STATIC ${CXL_DEV_SOURCES})

# Add the paths for header discovery.
# Separate public headers from private ones:
# public headers are importable by code outside of the library.
target_include_directories(lib-cxl-dev
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/cxl-dev/include
    PRIVATE ${CMAKE_SOURCE_DIR}/libs/cxl-dev
)

# List compile options; inheriting the project's shared CFLAGS is a good starting point
target_compile_options(lib-cxl-dev PRIVATE ${SHARED_CFLAGS})

# List the other libraries that this library depends on, if any
target_link_libraries(lib-cxl-dev PUBLIC lib-util)
```

Next, link the library to the target that uses it. Note that there are multiple targets for the UCM with varying compilation flags. You may need to edit the library link list for each target.

```
target_link_libraries(ucm PRIVATE ${PTHREAD_LIBRARY} ${ACCEL_CONFIG_LIBRARY} <your-new-lib> <other-existing-libs>...)
# more targets below ...
```

We include the library name when using a library from elsewhere in the project (e.g., `#include <lib>/<header.h>`). To use this structure, place the public header of a library under the path `libs/<lib>/include/<lib>/<your-actual-header.h>`.

#### Note: failing to compile auxilary targets (tests, benchmarks)

Tests and benchmarks may transitively depend on the library you have created (e.g., a test on ucm depends on the libraries that the ucm uses). To fix compilation errors, add the library to the list of linked libraries for the failing target (you will likely need to modify the testing or benchmarking section of the `CMakeLists.txt`).

### Adding a new directory

Edit the `CMakeLists.txt` file to discover a new directory. Below, we use the `ucm` as an example:

```
file(GLOB UCM_SOURCES "${CMAKE_SOURCE_DIR}/src/ucm/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/ds/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/ipc/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/physmem/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/policy/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/stats/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/support/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/source/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/handler/*.c"
                      # add a new directory here...
                      "${CMAKE_SOURCE_DIR}/src/ucm/<your-new-dir>/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/type/*.c")
```

Adding files to existing directories does not require edits to `CMakeLists.txt`.

### Adding a new feature flag

Feature flags are defined in one central file: `cmake/options.cmake`. When adding a new feature flag, be sure to add it to the list `PROJECT_CONFIG_OPTIONS` at the top of the file. Only options listed in this list are processed by CMake.

Using this list of config options as well as user-defined config file at `cmake/config`, CMake generates file `build/config.h`, which is then included by source code in the `src/` directory.

Certain combinations of feature flags are invalid. To add validation rules, append to file `cmake/validate_opts.cmake`.
