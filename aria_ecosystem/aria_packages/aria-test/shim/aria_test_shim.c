/*
 * aria_test_shim.c — Lightweight test framework for Aria
 * Provides assertion helpers, pass/fail counting, suite grouping, and summary output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ═══════════════════ state ═══════════════════ */

static int64_t g_passed  = 0;
static int64_t g_failed  = 0;
static int64_t g_skipped = 0;
static char g_suite[256] = "";
static int32_t g_verbose = 1; /* 1 = print each test, 0 = summary only */

/* ═══════════════════ lifecycle ═══════════════════ */

void aria_test_begin(const char *suite_name) {
    g_passed  = 0;
    g_failed  = 0;
    g_skipped = 0;
    snprintf(g_suite, sizeof(g_suite), "%s", suite_name);
    printf("\n══════════════════════════════════════\n");
    printf("  %s\n", g_suite);
    printf("══════════════════════════════════════\n\n");
}

void aria_test_set_verbose(int32_t v) { g_verbose = v; }

int32_t aria_test_end(void) {
    printf("\n──────────────────────────────────────\n");
    printf("  Suite: %s\n", g_suite);
    printf("  Passed:  %ld\n", (long)g_passed);
    printf("  Failed:  %ld\n", (long)g_failed);
    printf("  Skipped: %ld\n", (long)g_skipped);
    printf("  Total:   %ld\n", (long)(g_passed + g_failed + g_skipped));
    printf("──────────────────────────────────────\n");
    if (g_failed == 0) {
        printf("  ✓ ALL TESTS PASSED\n");
    } else {
        printf("  ✗ %ld TEST(S) FAILED\n", (long)g_failed);
    }
    printf("──────────────────────────────────────\n\n");
    return (g_failed == 0) ? 0 : 1;
}

/* ═══════════════════ stats ═══════════════════ */

int64_t aria_test_passed_count(void)  { return g_passed; }
int64_t aria_test_failed_count(void)  { return g_failed; }
int64_t aria_test_skipped_count(void) { return g_skipped; }
int64_t aria_test_total_count(void)   { return g_passed + g_failed + g_skipped; }

/* ═══════════════════ assertions ═══════════════════ */

static void pass_test(const char *name) {
    g_passed++;
    if (g_verbose) printf("[PASS] %s\n", name);
}

static void fail_test(const char *name, const char *detail) {
    g_failed++;
    printf("[FAIL] %s — %s\n", name, detail);
}

void aria_test_skip(const char *name) {
    g_skipped++;
    if (g_verbose) printf("[SKIP] %s\n", name);
}

/* bool assertion */
void aria_test_assert(const char *name, int32_t condition) {
    if (condition) pass_test(name);
    else fail_test(name, "condition was false");
}

/* int32 equality */
void aria_test_assert_eq_i32(const char *name, int32_t actual, int32_t expected) {
    if (actual == expected) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %d, got %d", expected, actual);
        fail_test(name, buf);
    }
}

/* int64 equality */
void aria_test_assert_eq_i64(const char *name, int64_t actual, int64_t expected) {
    if (actual == expected) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %ld, got %ld", (long)expected, (long)actual);
        fail_test(name, buf);
    }
}

/* string equality */
void aria_test_assert_eq_str(const char *name, const char *actual, const char *expected) {
    if (strcmp(actual, expected) == 0) {
        pass_test(name);
    } else {
        char buf[512];
        snprintf(buf, sizeof(buf), "expected \"%s\", got \"%s\"", expected, actual);
        fail_test(name, buf);
    }
}

/* float64 near-equal */
void aria_test_assert_near_f64(const char *name, double actual, double expected, double epsilon) {
    if (fabs(actual - expected) <= epsilon) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected %f ± %f, got %f", expected, epsilon, actual);
        fail_test(name, buf);
    }
}

/* not-null (for pointers passed as strings) */
void aria_test_assert_not_empty(const char *name, const char *value) {
    if (value && value[0] != '\0') {
        pass_test(name);
    } else {
        fail_test(name, "string was empty or null");
    }
}

/* int32 comparison: actual > threshold */
void aria_test_assert_gt_i32(const char *name, int32_t actual, int32_t threshold) {
    if (actual > threshold) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected > %d, got %d", threshold, actual);
        fail_test(name, buf);
    }
}

/* int64 comparison: actual > threshold */
void aria_test_assert_gt_i64(const char *name, int64_t actual, int64_t threshold) {
    if (actual > threshold) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected > %ld, got %ld", (long)threshold, (long)actual);
        fail_test(name, buf);
    }
}

/* not-equal int32 */
void aria_test_assert_ne_i32(const char *name, int32_t actual, int32_t unexpected) {
    if (actual != unexpected) {
        pass_test(name);
    } else {
        char buf[128];
        snprintf(buf, sizeof(buf), "expected != %d, got %d", unexpected, actual);
        fail_test(name, buf);
    }
}
