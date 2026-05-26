#ifndef UTIL_LOG_H
#define UTIL_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

extern FILE *hememlogf;
extern FILE *hotpagesf;

#define LOG(...)                                         \
    {                                                    \
        fprintf(stderr, "INFO\t");                       \
        fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                    \
    }

#define LOG_NOPATH(...)               \
    {                                 \
        fprintf(stderr, "INFO\t");    \
        fprintf(stderr, __VA_ARGS__); \
    }

#define LOG_WARN(...)                                    \
    {                                                    \
        fprintf(stderr, "\033[33mWARN\033[0m\t");        \
        fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                    \
    }

#define LOG_ERR(...)                                     \
    {                                                    \
        fprintf(stderr, "\033[1;31mERROR\033[0m\t");     \
        fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                    \
    }

#ifndef HEMEM_DEBUG
// Helper function to avoid unused variable error in the case where debug
// log statements are compiled away.
static inline void logger_unused_arg(int dummy_for_empty_va_args_case
                                     __attribute__((unused)),
                                     ...) {
    (void)dummy_for_empty_va_args_case;
}
#endif

#ifdef HEMEM_DEBUG
#define LOG_DEBUG(...)                                          \
    {                                                           \
        fprintf(stderr, "DEBUG\t[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                           \
    }
#else
#define LOG_DEBUG(...) logger_unused_arg(0, ##__VA_ARGS__)
#endif

#ifdef HEMEM_DEBUG
#define LOG_WARN_DEBUG(...)                              \
    {                                                    \
        fprintf(stderr, "\033[33mWARN\033[0m ");         \
        fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                    \
    }
#else
#define LOG_WARN_DEBUG(...) logger_unused_arg(0, ##__VA_ARGS__)
#endif

#ifdef HEMEM_DEBUG
#define LOG_ERR_DEBUG(...)                               \
    {                                                    \
        fprintf(stderr, "\033[1;31mERROR\033[0m ");      \
        fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                    \
    }
#else
#define LOG_ERR_DEBUG(...) logger_unused_arg(0, ##__VA_ARGS__)
#endif

#define LOG_HOT(...) \
    { fprintf(hotpagesf, __VA_ARGS__); }

/*
#define LOG(...)                         \
    {                                    \
        fprintf(hememlogf, __VA_ARGS__); \
        fflush(hememlogf);               \
    }
#define LOG(str, ...) while (0) {}
*/

extern FILE *timef;
extern bool timing;

static inline void log_time(const char *fmt, ...) {
    if (timing) {
        va_list args;
        va_start(args, fmt);
        vfprintf(timef, fmt, args);
        va_end(args);
    }
}

//#define LOG_TIME(str, ...) log_time(str, __VA_ARGS__)
// #define LOG_TIME(str, ...) fprintf(timef, str, __VA_ARGS__)
#define LOG_TIME(str, ...) \
    while (0) {            \
    }

extern FILE *statsf;
#define LOG_STATS(str, ...) fprintf(stdout, str, ##__VA_ARGS__)
//#define LOG_STATS(str, ...) fprintf(statsf, str, __VA_ARGS__)
//#define LOG_STATS(str, ...) while (0) {}

void log_init(const char *log_name);
void print_stacktrace();

inline char *bool_str(bool b) { return (b) ? "true" : "false"; }

#endif
