/*  aria_log_shim.c — Structured logging for Aria FFI
 *
 *  Provides leveled logging (TRACE, DEBUG, INFO, WARN, ERROR, FATAL)
 *  with timestamps, configurable output, and log level filtering.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_log_shim.so aria_log_shim.c
 */

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

/* ── log levels ──────────────────────────────────────────────────────── */

#define LOG_TRACE 0
#define LOG_DEBUG 1
#define LOG_INFO  2
#define LOG_WARN  3
#define LOG_ERROR 4
#define LOG_FATAL 5

static int32_t g_min_level = LOG_INFO;
static FILE   *g_output    = NULL;  /* NULL means stderr */
static int     g_show_timestamp = 1;
static int     g_show_level     = 1;
static int64_t g_log_count      = 0;

static const char *level_name(int32_t level) {
    switch (level) {
        case LOG_TRACE: return "TRACE";
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default:        return "?????";
    }
}

/* ── init / cleanup ──────────────────────────────────────────────────── */

void aria_log_init(void) {
    g_min_level = LOG_INFO;
    g_output = NULL;
    g_show_timestamp = 1;
    g_show_level = 1;
    g_log_count = 0;
}

void aria_log_cleanup(void) {
    if (g_output && g_output != stdout && g_output != stderr) {
        fclose(g_output);
    }
    g_output = NULL;
}

/* ── configuration ───────────────────────────────────────────────────── */

void aria_log_set_level(int32_t level) {
    if (level >= LOG_TRACE && level <= LOG_FATAL) g_min_level = level;
}

int32_t aria_log_get_level(void) {
    return g_min_level;
}

void aria_log_set_output_file(const char *path) {
    if (g_output && g_output != stdout && g_output != stderr) {
        fclose(g_output);
    }
    g_output = fopen(path, "a");
}

void aria_log_set_output_stderr(void) {
    if (g_output && g_output != stdout && g_output != stderr) {
        fclose(g_output);
    }
    g_output = NULL;
}

void aria_log_set_output_stdout(void) {
    if (g_output && g_output != stdout && g_output != stderr) {
        fclose(g_output);
    }
    g_output = stdout;
}

void aria_log_show_timestamp(int32_t show) { g_show_timestamp = show ? 1 : 0; }
void aria_log_show_level(int32_t show)     { g_show_level = show ? 1 : 0; }

/* ── core log function ───────────────────────────────────────────────── */

static void do_log(int32_t level, const char *msg) {
    if (level < g_min_level) return;
    FILE *out = g_output ? g_output : stderr;

    if (g_show_timestamp) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        fprintf(out, "%04d-%02d-%02d %02d:%02d:%02d ",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
    }
    if (g_show_level) {
        fprintf(out, "[%s] ", level_name(level));
    }
    fprintf(out, "%s\n", msg);
    fflush(out);
    g_log_count++;
}

/* ── log by level ────────────────────────────────────────────────────── */

void aria_log_trace(const char *msg) { do_log(LOG_TRACE, msg); }
void aria_log_debug(const char *msg) { do_log(LOG_DEBUG, msg); }
void aria_log_info(const char *msg)  { do_log(LOG_INFO, msg); }
void aria_log_warn(const char *msg)  { do_log(LOG_WARN, msg); }
void aria_log_error(const char *msg) { do_log(LOG_ERROR, msg); }
void aria_log_fatal(const char *msg) { do_log(LOG_FATAL, msg); }

/* ── stats ───────────────────────────────────────────────────────────── */

int64_t aria_log_count(void) { return g_log_count; }

/* ── test helpers (C-side assertions) ────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_log_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_log_assert_int64_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %ld, got %ld)\n", msg, expected, actual); }
}

void aria_log_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

void aria_log_test_summary(void) {
    printf("\n=== aria-log test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
