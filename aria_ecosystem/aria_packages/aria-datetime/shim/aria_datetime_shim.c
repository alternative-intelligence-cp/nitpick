/*  aria_datetime_shim.c — Date/time bridge for Aria FFI
 *
 *  Wraps POSIX time.h: current time, time components, formatting,
 *  duration arithmetic, sleep.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_datetime_shim.so aria_datetime_shim.c
 */

#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

/* ── current time ────────────────────────────────────────────────────── */

int64_t aria_datetime_now(void) {
    return (int64_t)time(NULL);
}

int64_t aria_datetime_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

int64_t aria_datetime_now_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}

/* ── monotonic clock (for elapsed timing) ────────────────────────────── */

int64_t aria_datetime_monotonic_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
}

/* ── time components (local time) ────────────────────────────────────── */

static struct tm g_tm;

static void fill_tm(int64_t epoch) {
    time_t t = (time_t)epoch;
    localtime_r(&t, &g_tm);
}

int32_t aria_datetime_year(int64_t epoch)   { fill_tm(epoch); return (int32_t)(g_tm.tm_year + 1900); }
int32_t aria_datetime_month(int64_t epoch)  { fill_tm(epoch); return (int32_t)(g_tm.tm_mon + 1); }
int32_t aria_datetime_day(int64_t epoch)    { fill_tm(epoch); return (int32_t)g_tm.tm_mday; }
int32_t aria_datetime_hour(int64_t epoch)   { fill_tm(epoch); return (int32_t)g_tm.tm_hour; }
int32_t aria_datetime_minute(int64_t epoch) { fill_tm(epoch); return (int32_t)g_tm.tm_min; }
int32_t aria_datetime_second(int64_t epoch) { fill_tm(epoch); return (int32_t)g_tm.tm_sec; }
int32_t aria_datetime_weekday(int64_t epoch){ fill_tm(epoch); return (int32_t)g_tm.tm_wday; }
int32_t aria_datetime_yearday(int64_t epoch){ fill_tm(epoch); return (int32_t)(g_tm.tm_yday + 1); }

/* ── time components (UTC) ───────────────────────────────────────────── */

static struct tm g_utm;

static void fill_utm(int64_t epoch) {
    time_t t = (time_t)epoch;
    gmtime_r(&t, &g_utm);
}

int32_t aria_datetime_utc_year(int64_t epoch)   { fill_utm(epoch); return (int32_t)(g_utm.tm_year + 1900); }
int32_t aria_datetime_utc_month(int64_t epoch)  { fill_utm(epoch); return (int32_t)(g_utm.tm_mon + 1); }
int32_t aria_datetime_utc_day(int64_t epoch)    { fill_utm(epoch); return (int32_t)g_utm.tm_mday; }
int32_t aria_datetime_utc_hour(int64_t epoch)   { fill_utm(epoch); return (int32_t)g_utm.tm_hour; }
int32_t aria_datetime_utc_minute(int64_t epoch) { fill_utm(epoch); return (int32_t)g_utm.tm_min; }
int32_t aria_datetime_utc_second(int64_t epoch) { fill_utm(epoch); return (int32_t)g_utm.tm_sec; }

/* ── formatting ──────────────────────────────────────────────────────── */

static char g_fmt_buf[256];
static const char *g_last_result = "";

const char *aria_datetime_format(int64_t epoch, const char *fmt) {
    fill_tm(epoch);
    strftime(g_fmt_buf, sizeof(g_fmt_buf), fmt, &g_tm);
    g_last_result = g_fmt_buf;
    return g_last_result;
}

const char *aria_datetime_format_utc(int64_t epoch, const char *fmt) {
    fill_utm(epoch);
    strftime(g_fmt_buf, sizeof(g_fmt_buf), fmt, &g_utm);
    g_last_result = g_fmt_buf;
    return g_last_result;
}

const char *aria_datetime_iso8601(int64_t epoch) {
    aria_datetime_format_utc(epoch, "%Y-%m-%dT%H:%M:%SZ");
    return g_last_result;
}

const char *aria_datetime_last_result(void) { return g_last_result; }

/* ── construction ────────────────────────────────────────────────────── */

int64_t aria_datetime_make(int32_t year, int32_t month, int32_t day,
                           int32_t hour, int32_t minute, int32_t second) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    t.tm_isdst = -1;
    return (int64_t)mktime(&t);
}

int64_t aria_datetime_make_utc(int32_t year, int32_t month, int32_t day,
                               int32_t hour, int32_t minute, int32_t second) {
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    return (int64_t)timegm(&t);
}

/* ── duration helpers ────────────────────────────────────────────────── */

int64_t aria_datetime_diff(int64_t a, int64_t b) {
    return a - b;
}

int64_t aria_datetime_add_seconds(int64_t epoch, int64_t seconds) {
    return epoch + seconds;
}

int64_t aria_datetime_add_days(int64_t epoch, int32_t days) {
    return epoch + (int64_t)days * 86400;
}

/* ── sleep ───────────────────────────────────────────────────────────── */

void aria_datetime_sleep_ms(int32_t ms) {
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

/* ── test helpers (C-side assertions for Aria compiler workaround) ──── */

#include <stdio.h>

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_datetime_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_datetime_assert_int64_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %ld, got %ld)\n", msg, expected, actual); }
}

void aria_datetime_assert_int64_positive(int64_t val, const char *msg) {
    if (val > 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (got %ld, expected > 0)\n", msg, val); }
}

void aria_datetime_assert_int64_gt(int64_t a, int64_t b, const char *msg) {
    if (a > b) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (%ld not > %ld)\n", msg, a, b); }
}

void aria_datetime_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

void aria_datetime_assert_iso8601_eq(int64_t epoch, const char *expected, const char *msg) {
    const char *result = aria_datetime_iso8601(epoch);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_datetime_assert_format_utc_eq(int64_t epoch, const char *fmt,
                                        const char *expected, const char *msg) {
    const char *result = aria_datetime_format_utc(epoch, fmt);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_datetime_test_summary(void) {
    printf("\n=== aria-datetime test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
