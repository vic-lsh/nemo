cmake_minimum_required(VERSION 3.10)


include(cmake/options.cmake)
include(cmake/config.cmake)
include(cmake/validate_opts.cmake)
include(cmake/gen_config_template.cmake)

# Set the C and C++ compilers before the project command
set(CMAKE_C_COMPILER "/usr/bin/gcc")      # Adjust this path if necessary
set(CMAKE_CXX_COMPILER "/usr/bin/g++")    # Adjust this path if necessary

project(HememProject C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check for DEBUG environment variable
if(DEFINED ENV{DEBUG})
    set(DEBUG_MODE $ENV{DEBUG})
else()
    set(DEBUG_MODE OFF)
endif()

# Common compiler flags
set(COMMON_FLAGS
    -Wall
    -Wextra
    -Werror
    -g3
    -fPIC
)

if(DEBUG_MODE)
    message(STATUS "Compiling in debug mode with -O0")
    list(APPEND COMMON_FLAGS -O0)
else()
    message(STATUS "Compiling in release mode with -O3")
    list(APPEND COMMON_FLAGS -O3)
endif()

# Set specific flags for each target type
set(APP_CFLAGS ${COMMON_FLAGS} -fPIC)
set(UCM_CFLAGS ${COMMON_FLAGS})
set(SHARED_CFLAGS ${COMMON_FLAGS})
set(BENCH_CFLAGS ${COMMON_FLAGS} -O3)
set(FPGA_CTL_CFLAGS ${COMMON_FLAGS})

# let CMake recompile whenever the build config file changed.
set_property(DIRECTORY APPEND PROPERTY
    CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cmake/config"
)

# Include directories
include_directories(
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/linux/usr/include
    ${CMAKE_SOURCE_DIR}/linux/include/uapi
    ${CMAKE_SOURCE_DIR}/src/shared
)

# Libraries
find_library(MATH_LIBRARY m REQUIRED)
find_library(PTHREAD_LIBRARY pthread REQUIRED)
find_library(ACCEL_CONFIG_LIBRARY accel-config REQUIRED HINTS /usr/lib64)
find_library(DL_LIBRARY dl REQUIRED)
find_library(SYSCALL_INTERCEPT_LIBRARY syscall_intercept REQUIRED)

message(STATUS "accel-config library found at: ${ACCEL_CONFIG_LIBRARY}")


# Gather source files
file(GLOB UCM_SOURCES "${CMAKE_SOURCE_DIR}/src/ucm/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/ds/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/ipc/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/opts/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/physmem/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/policy/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/stats/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/support/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/source/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/telem/handler/*.c"
                      "${CMAKE_SOURCE_DIR}/src/ucm/type/*.c")
file(GLOB SHARED_SOURCES "${CMAKE_SOURCE_DIR}/src/shared/*.c"
                         "${CMAKE_SOURCE_DIR}/src/shared/util/*.c")
file(GLOB APP_SOURCES "${CMAKE_SOURCE_DIR}/src/app/*.c")

file(GLOB CXL_DEV_SOURCES
    "${CMAKE_SOURCE_DIR}/libs/cxl-dev/*.c"
    "${CMAKE_SOURCE_DIR}/libs/cxl-dev/**/*.c"
)

file(GLOB IPC_CHAN_SOURCES
    "${CMAKE_SOURCE_DIR}/libs/ipc-chan/*.c"
    "${CMAKE_SOURCE_DIR}/libs/ipc-chan/**/*.c"
)


file(GLOB LIB_UTIL_SOURCES
    "${CMAKE_SOURCE_DIR}/libs/util/*.c"
    "${CMAKE_SOURCE_DIR}/libs/util/**/*.c"
)

file(GLOB FPGA_CTL_BIN_SOURCES "${CMAKE_SOURCE_DIR}/fpga_ctl/bin/*.c")
file(GLOB FPGA_CTL_LIB_SOURCES "${CMAKE_SOURCE_DIR}/fpga_ctl/*.c")

# Targets
add_library(hemem SHARED ${APP_SOURCES} ${SHARED_SOURCES})
add_library(hemem-llama SHARED ${APP_SOURCES} ${SHARED_SOURCES})
add_library(hemem-qemu SHARED ${APP_SOURCES} ${SHARED_SOURCES})


add_library(lib-util STATIC ${LIB_UTIL_SOURCES})
target_include_directories(lib-util
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/util/include
    PRIVATE ${CMAKE_SOURCE_DIR}/libs/util
)
target_compile_options(lib-util PRIVATE ${SHARED_CFLAGS})

add_library(lib-cxl-dev STATIC ${CXL_DEV_SOURCES})
target_include_directories(lib-cxl-dev
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/cxl-dev/include
    PRIVATE ${CMAKE_SOURCE_DIR}/libs/cxl-dev
)
target_compile_options(lib-cxl-dev PRIVATE ${SHARED_CFLAGS})
target_link_libraries(lib-cxl-dev PUBLIC lib-util)

add_library(lib-ipc-chan STATIC ${IPC_CHAN_SOURCES})
target_include_directories(lib-ipc-chan
    PUBLIC ${CMAKE_SOURCE_DIR}/libs/ipc-chan/include
    PRIVATE ${CMAKE_SOURCE_DIR}/libs/ipc-chan
)
target_compile_options(lib-ipc-chan PRIVATE ${SHARED_CFLAGS})
target_link_libraries(lib-ipc-chan PUBLIC lib-util)

target_compile_definitions(hemem PRIVATE ALLOC_HEMEM)
target_compile_definitions(hemem-llama PRIVATE LLAMA ALLOC_HEMEM)
target_compile_definitions(hemem-qemu PRIVATE QEMU)

add_executable(ucm ${UCM_SOURCES} ${SHARED_SOURCES})
add_executable(ucm-qemu ${UCM_SOURCES} ${SHARED_SOURCES})

target_include_directories(ucm PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ucm
)
target_include_directories(ucm-qemu PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ucm
)

target_compile_definitions(ucm      PRIVATE)
target_compile_definitions(ucm-qemu PRIVATE QEMU)

foreach(fpga_bin_source ${FPGA_CTL_BIN_SOURCES})
    get_filename_component(fpga_name ${fpga_bin_source} NAME_WE)
    add_executable(${fpga_name} ${fpga_bin_source} ${FPGA_CTL_LIB_SOURCES})
    target_include_directories(${fpga_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/fpga_ctl
    )
    target_link_libraries(${fpga_name} PRIVATE lib-cxl-dev lib-util)
    target_compile_options(${fpga_name} PRIVATE ${FPGA_CTL_CFLAGS})
endforeach()

# Linking
target_link_libraries(hemem PRIVATE ${MATH_LIBRARY} ${PTHREAD_LIBRARY} ${DL_LIBRARY} ${SYSCALL_INTERCEPT_LIBRARY} lib-ipc-chan lib-util)
target_link_libraries(hemem-llama PRIVATE ${MATH_LIBRARY} ${PTHREAD_LIBRARY} ${DL_LIBRARY} ${SYSCALL_INTERCEPT_LIBRARY} lib-ipc-chan lib-util)
target_link_libraries(hemem-qemu PRIVATE ${MATH_LIBRARY} ${PTHREAD_LIBRARY} ${DL_LIBRARY} ${SYSCALL_INTERCEPT_LIBRARY} lib-ipc-chan lib-util)
target_link_libraries(ucm PRIVATE ${PTHREAD_LIBRARY} ${ACCEL_CONFIG_LIBRARY} lib-cxl-dev lib-ipc-chan lib-util)
target_link_libraries(ucm-qemu PRIVATE ${PTHREAD_LIBRARY} ${ACCEL_CONFIG_LIBRARY} lib-cxl-dev lib-ipc-chan lib-util)

# Compile options
target_compile_options(hemem PRIVATE ${APP_CFLAGS})
target_compile_options(hemem-llama PRIVATE ${APP_CFLAGS})
target_compile_options(hemem-qemu PRIVATE ${APP_CFLAGS})
target_compile_options(ucm PRIVATE ${UCM_CFLAGS})
target_compile_options(ucm-qemu PRIVATE ${UCM_CFLAGS})

# Set output directory for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Add this at the end to print the compiler being used
message(STATUS "C compiler: ${CMAKE_C_COMPILER}")

# =========================== setup testing =================================

# Add Unity library
add_library(unity STATIC
    3rd_party/unity/src/unity.c
)

target_include_directories(unity PUBLIC
    3rd_party/unity/src
)

# Find Ruby interpreter, minimum version required by unity test-runner generator
find_package(Ruby 2.6 REQUIRED)

# Set the path to generate_test_runner.rb
set(UNITY_GENERATOR ${CMAKE_SOURCE_DIR}/3rd_party/unity/auto/generate_test_runner.rb)

# Function to generate test runner and add test
function(add_unity_test TEST_NAME TEST_SOURCE)
    # Generate the runner source file name
    get_filename_component(test_filename ${TEST_SOURCE} NAME_WE)
    set(RUNNER_SOURCE ${CMAKE_CURRENT_BINARY_DIR}/${test_filename}_Runner.c)

    # Add custom command to generate the runner
    add_custom_command(
        OUTPUT ${RUNNER_SOURCE}
        COMMAND ${RUBY_EXECUTABLE} ${UNITY_GENERATOR} ${TEST_SOURCE} ${RUNNER_SOURCE}
        DEPENDS ${TEST_SOURCE}
        COMMENT "Generating test runner for ${TEST_SOURCE}"
    )

    # Add the test executable
    add_executable(${TEST_NAME} ${TEST_SOURCE} ${RUNNER_SOURCE} ${ARGN})
    target_link_libraries(${TEST_NAME} PRIVATE unity ${MATH_LIBRARY} lib-cxl-dev)
    target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/3rd_party/unity/src)

    # Add the test
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endfunction()

# Gather all test files under tests/ucm
file(GLOB UCM_TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/ucm/*.c"
                           "${CMAKE_SOURCE_DIR}/tests/ucm/ds/*.c"
                           "${CMAKE_SOURCE_DIR}/tests/ucm/support/*.c")
# include everything but the main function
set(TEST_UCM_SOURCES ${UCM_SOURCES})
list(FILTER TEST_UCM_SOURCES EXCLUDE REGEX ".*(main)\\.c$")

foreach(test_source ${UCM_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_unity_test(${test_name} ${test_source} ${TEST_UCM_SOURCES} ${SHARED_SOURCES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd_party/unity/src
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/ucm
        ${CMAKE_SOURCE_DIR}/src/shared
    )
    target_link_libraries(${test_name} PRIVATE
        unity
        ${PTHREAD_LIBRARY}
        ${ACCEL_CONFIG_LIBRARY}
        lib-cxl-dev
        lib-ipc-chan
        lib-util
    )
    target_compile_options(${test_name} PRIVATE ${UCM_CFLAGS})
    target_compile_definitions(${test_name} PRIVATE)
endforeach()

file(GLOB SHARED_TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/shared/*.c")
foreach(test_source ${SHARED_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_unity_test(${test_name} ${test_source} ${SHARED_SOURCES})
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/3rd_party/unity/src
        ${CMAKE_SOURCE_DIR}/src/shared
    )
    target_link_libraries(${test_name} PRIVATE
        unity
        ${MATH_LIBRARY}
    )
    target_compile_options(${test_name} PRIVATE ${SHARED_CFLAGS})
endforeach()

# Add e2e test files.
# Note that these files are not linked with the unity test library. They're
# simple c sources that would be compiled, then LD_PRELOADED with our library
# for end-to-end testing.
file(GLOB E2E_TEST_SOURCES "${CMAKE_SOURCE_DIR}/tests/e2e/*.c")
foreach(test_source ${E2E_TEST_SOURCES})
    get_filename_component(test_name ${test_source} NAME_WE)
    add_executable(${test_name} ${test_source})
    set_target_properties(${test_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "tests/e2e")
    target_link_libraries(${test_name} PRIVATE
        ${PTHREAD_LIBRARY}
        lib-util
    )
endforeach()

# Enable testing
enable_testing()


# =========================== setup testing end ============================

# Gather benchmark sources
file(GLOB BENCH_UCM_SOURCES "${CMAKE_SOURCE_DIR}/bench/ucm/*.c")
file(GLOB BENCH_APP_SOURCES "${CMAKE_SOURCE_DIR}/bench/app/*.c")
file(GLOB BENCH_SHARED_SOURCES "${CMAKE_SOURCE_DIR}/bench/shared/*.c")
file(GLOB BENCH_COMMON_SOURCES "${CMAKE_SOURCE_DIR}/bench/*.c")

# include everything but the main function
set(UCM_SOURCES_FOR_BENCH ${UCM_SOURCES})
list(FILTER UCM_SOURCES_FOR_BENCH EXCLUDE REGEX ".*(main)\\.c$")

# Benchmark output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bench)

# Function to create benchmark targets
function(add_benchmark_target NAME SOURCES INCLUDE_DIRS)
    add_executable(${NAME} ${SOURCES})
    target_include_directories(${NAME} PRIVATE
        ${INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/bench
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/shared
    )
    target_compile_options(${NAME} PRIVATE ${BENCH_CFLAGS})
endfunction()

# UCM benchmarks
foreach(bench_source ${BENCH_UCM_SOURCES})
    get_filename_component(bench_name ${bench_source} NAME_WE)
    add_benchmark_target(
        bench_ucm_${bench_name}
        "${bench_source};${UCM_SOURCES_FOR_BENCH};${SHARED_SOURCES};${BENCH_COMMON_SOURCES}"
        "${CMAKE_SOURCE_DIR}/src/ucm"
    )
    target_link_libraries(bench_ucm_${bench_name} PRIVATE
        ${MATH_LIBRARY}
        ${PTHREAD_LIBRARY}
        ${ACCEL_CONFIG_LIBRARY}
        lib-cxl-dev
        lib-ipc-chan
        lib-util
    )
    target_compile_definitions(bench_ucm_${bench_name} PRIVATE)
endforeach()

# APP benchmarks
foreach(bench_source ${BENCH_APP_SOURCES})
    get_filename_component(bench_name ${bench_source} NAME_WE)
    add_benchmark_target(
        bench_app_${bench_name}
        "${bench_source};${APP_SOURCES};${SHARED_SOURCES};${BENCH_COMMON_SOURCES}"
        "${CMAKE_SOURCE_DIR}/src/app"
    )
    target_link_libraries(bench_app_${bench_name} PRIVATE
        ${MATH_LIBRARY}
        ${PTHREAD_LIBRARY}
        ${DL_LIBRARY}
        ${SYSCALL_INTERCEPT_LIBRARY}
        lib-ipc-chan
        lib-util
    )
    target_compile_definitions(bench_app_${bench_name} PRIVATE ALLOC_HEMEM)
endforeach()

# Shared benchmarks
foreach(bench_source ${BENCH_SHARED_SOURCES})
    get_filename_component(bench_name ${bench_source} NAME_WE)
    add_benchmark_target(
        bench_shared_${bench_name}
        "${bench_source};${SHARED_SOURCES};${BENCH_COMMON_SOURCES}"
        "${CMAKE_SOURCE_DIR}/src/shared"
    )
    target_link_libraries(bench_shared_${bench_name} PRIVATE
        ${MATH_LIBRARY}
        ${PTHREAD_LIBRARY}
    )
endforeach()

# ==============================================================================
# Microbenchmarks
# ==============================================================================

# Find all C and C++ files in microbenchmarks/bin to be treated as executable targets
file(GLOB MICROBENCH_TARGETS 
    "${CMAKE_SOURCE_DIR}/microbenchmarks/bin/*.c" 
    "${CMAKE_SOURCE_DIR}/microbenchmarks/bin/*.cpp"
)

# Find all C and C++ source files in the root of microbenchmarks/ to be compiled with the targets
file(GLOB MICROBENCH_SOURCES
    LIST_DIRECTORIES false
    "${CMAKE_SOURCE_DIR}/microbenchmarks/*.c"
    "${CMAKE_SOURCE_DIR}/microbenchmarks/*.cpp"
)

# Set the output directory for the microbenchmark binaries
set(MICROBENCH_BIN_DIR ${CMAKE_BINARY_DIR}/microbenchmarks)

foreach(target_source ${MICROBENCH_TARGETS})
    # Get the name of the target from the source file name (e.g., "my_benchmark")
    get_filename_component(target_name ${target_source} NAME_WE)

    # Add the executable target, including the common microbenchmark sources
    add_executable(${target_name} ${target_source} ${MICROBENCH_SOURCES})

    # Add the microbenchmarks/ directory to the include path for headers
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/microbenchmarks
    )

    # Add the libs/ directory to the linker search path for shared libraries
    target_link_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/libs
    )

    # Find and link all shared libraries (.so) in the libs/ directory
    file(GLOB SHARED_LIBS "${CMAKE_SOURCE_DIR}/libs/lib*.so")
    foreach(shared_lib ${SHARED_LIBS})
        get_filename_component(lib_name_we ${shared_lib} NAME_WE)
        # Remove the 'lib' prefix for the linker flag (e.g., libexample.so -> -lexample)
        string(SUBSTRING "${lib_name_we}" 3 -1 lib_name)
        target_link_libraries(${target_name} PRIVATE ${lib_name})
    endforeach()
    
    # Link against other necessary libraries defined in this project
    target_link_libraries(${target_name} PRIVATE 
        ${PTHREAD_LIBRARY} 
        ${MATH_LIBRARY} 
        lib-util 
        lib-cxl-dev
    )

    # Set the output directory for the compiled microbenchmark executable
    set_target_properties(${target_name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${MICROBENCH_BIN_DIR})
endforeach()
