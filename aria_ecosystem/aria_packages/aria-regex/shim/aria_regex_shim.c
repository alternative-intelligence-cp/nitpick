/*  aria_regex_shim.c — POSIX extended regex for Aria FFI
 *
 *  Compiles a regex pattern, then match/search/extract groups.
 *  Manages one compiled regex at a time internally.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_regex_shim.so aria_regex_shim.c
 */

#include <stdint.h>
#include <string.h>
#include <regex.h>

/* ── internal state ──────────────────────────────────────────────────── */

static regex_t   g_regex;
static int       g_compiled = 0;
static char      g_error[256] = {0};
static const char *g_last_result = "";

#define MAX_GROUPS 16
static regmatch_t g_matches[MAX_GROUPS];
static char       g_group_buf[4096];
static int32_t    g_num_matches = 0;

/* ── compile / free ──────────────────────────────────────────────────── */

int32_t aria_regex_compile(const char *pattern) {
    if (g_compiled) { regfree(&g_regex); g_compiled = 0; }
    int ret = regcomp(&g_regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        regerror(ret, &g_regex, g_error, sizeof(g_error));
        return 0;
    }
    g_compiled = 1;
    g_error[0] = '\0';
    return 1;
}

int32_t aria_regex_compile_icase(const char *pattern) {
    if (g_compiled) { regfree(&g_regex); g_compiled = 0; }
    int ret = regcomp(&g_regex, pattern, REG_EXTENDED | REG_ICASE);
    if (ret != 0) {
        regerror(ret, &g_regex, g_error, sizeof(g_error));
        return 0;
    }
    g_compiled = 1;
    g_error[0] = '\0';
    return 1;
}

void aria_regex_free(void) {
    if (g_compiled) { regfree(&g_regex); g_compiled = 0; }
    g_num_matches = 0;
}

const char *aria_regex_error(void) {
    return g_error;
}

/* ── matching ────────────────────────────────────────────────────────── */

int32_t aria_regex_match(const char *text) {
    if (!g_compiled) return 0;
    int ret = regexec(&g_regex, text, MAX_GROUPS, g_matches, 0);
    if (ret == 0) {
        /* Count how many groups matched */
        g_num_matches = 0;
        for (int i = 0; i < MAX_GROUPS; i++) {
            if (g_matches[i].rm_so == -1) break;
            g_num_matches++;
        }
        return 1;
    }
    g_num_matches = 0;
    return 0;
}

/* Full-string match (anchored) */
int32_t aria_regex_match_full(const char *text) {
    if (!g_compiled) return 0;
    int ret = regexec(&g_regex, text, MAX_GROUPS, g_matches, 0);
    if (ret != 0) { g_num_matches = 0; return 0; }
    /* Check that match spans entire string */
    size_t len = strlen(text);
    if (g_matches[0].rm_so == 0 && (size_t)g_matches[0].rm_eo == len) {
        g_num_matches = 0;
        for (int i = 0; i < MAX_GROUPS; i++) {
            if (g_matches[i].rm_so == -1) break;
            g_num_matches++;
        }
        return 1;
    }
    g_num_matches = 0;
    return 0;
}

/* ── groups ──────────────────────────────────────────────────────────── */

int32_t aria_regex_group_count(void) {
    return g_num_matches;
}

static const char *g_last_text = NULL;

int32_t aria_regex_search(const char *text) {
    g_last_text = text;
    return aria_regex_match(text);
}

const char *aria_regex_group(int32_t idx) {
    if (idx < 0 || idx >= g_num_matches || !g_last_text) { g_last_result = ""; return g_last_result; }
    int start = g_matches[idx].rm_so;
    int end   = g_matches[idx].rm_eo;
    if (start < 0 || end < 0) { g_last_result = ""; return g_last_result; }
    int len = end - start;
    if (len >= (int)sizeof(g_group_buf)) len = (int)sizeof(g_group_buf) - 1;
    memcpy(g_group_buf, g_last_text + start, len);
    g_group_buf[len] = '\0';
    g_last_result = g_group_buf;
    return g_last_result;
}

int32_t aria_regex_group_start(int32_t idx) {
    if (idx < 0 || idx >= g_num_matches) return -1;
    return (int32_t)g_matches[idx].rm_so;
}

int32_t aria_regex_group_end(int32_t idx) {
    if (idx < 0 || idx >= g_num_matches) return -1;
    return (int32_t)g_matches[idx].rm_eo;
}

/* ── replace (first occurrence) ──────────────────────────────────────── */

static char g_replace_buf[8192];

const char *aria_regex_replace(const char *text, const char *replacement) {
    if (!g_compiled) { g_last_result = text; return g_last_result; }
    int ret = regexec(&g_regex, text, 1, g_matches, 0);
    if (ret != 0) { g_last_result = text; return g_last_result; }

    int start = g_matches[0].rm_so;
    int end   = g_matches[0].rm_eo;
    size_t rlen = strlen(replacement);
    size_t tlen = strlen(text);
    size_t newlen = (size_t)start + rlen + (tlen - (size_t)end);

    if (newlen >= sizeof(g_replace_buf)) { g_last_result = text; return g_last_result; }

    memcpy(g_replace_buf, text, start);
    memcpy(g_replace_buf + start, replacement, rlen);
    memcpy(g_replace_buf + start + rlen, text + end, tlen - end);
    g_replace_buf[newlen] = '\0';

    g_last_result = g_replace_buf;
    return g_last_result;
}

const char *aria_regex_last_result(void) { return g_last_result; }

/* ── cleanup alias (avoids Aria compiler "free" name bug) ────────────── */

void aria_regex_cleanup(void) {
    aria_regex_free();
}

/* ── test helpers (C-side assertions for Aria compiler workaround) ──── */

#include <stdio.h>

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_regex_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_regex_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

void aria_regex_assert_group_eq(int32_t idx, const char *expected, const char *msg) {
    const char *result = aria_regex_group(idx);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_regex_assert_replace_eq(const char *text, const char *replacement,
                                  const char *expected, const char *msg) {
    const char *result = aria_regex_replace(text, replacement);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_regex_test_summary(void) {
    printf("\n=== aria-regex test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
