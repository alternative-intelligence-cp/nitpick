/*  aria_csv_shim.c — CSV parser/writer for Aria FFI
 *
 *  Parses CSV text into a row×column grid accessible by index.
 *  Also builds CSV output field-by-field.
 *  RFC 4180 compliant: handles quoted fields, embedded commas,
 *  embedded newlines in quotes, escaped quotes ("").
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_csv_shim.so aria_csv_shim.c
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── parsed grid ─────────────────────────────────────────────────────── */

#define MAX_ROWS   4096
#define MAX_COLS   256
#define MAX_FIELD  8192

static char *g_grid[MAX_ROWS][MAX_COLS];
static int32_t g_num_rows = 0;
static int32_t g_num_cols = 0;    /* columns in first row (header) */
static const char *g_last_result = "";

static void grid_clear(void) {
    for (int r = 0; r < g_num_rows; r++)
        for (int c = 0; c < MAX_COLS; c++) {
            free(g_grid[r][c]);
            g_grid[r][c] = NULL;
        }
    g_num_rows = 0;
    g_num_cols = 0;
}

/* ── parsing ─────────────────────────────────────────────────────────── */

int32_t aria_csv_parse(const char *csv_text) {
    grid_clear();
    if (!csv_text || !*csv_text) return 0;

    const char *p = csv_text;
    int32_t row = 0, col = 0;
    char field[MAX_FIELD];
    int flen = 0;

    while (*p && row < MAX_ROWS) {
        flen = 0;
        if (*p == '"') {
            /* quoted field */
            p++; /* skip opening quote */
            while (*p) {
                if (*p == '"') {
                    if (*(p+1) == '"') {
                        /* escaped quote */
                        field[flen++] = '"';
                        p += 2;
                    } else {
                        /* end of quoted field */
                        p++; /* skip closing quote */
                        break;
                    }
                } else {
                    if (flen < MAX_FIELD - 1) field[flen++] = *p;
                    p++;
                }
            }
        } else {
            /* unquoted field */
            while (*p && *p != ',' && *p != '\n' && *p != '\r') {
                if (flen < MAX_FIELD - 1) field[flen++] = *p;
                p++;
            }
        }
        field[flen] = '\0';

        if (col < MAX_COLS) {
            g_grid[row][col] = strdup(field);
        }
        col++;

        if (*p == ',') {
            p++;
        } else if (*p == '\r') {
            p++;
            if (*p == '\n') p++;
            if (row == 0) g_num_cols = col;
            row++;
            col = 0;
        } else if (*p == '\n') {
            p++;
            if (row == 0) g_num_cols = col;
            row++;
            col = 0;
        } else if (*p == '\0') {
            if (row == 0) g_num_cols = col;
            row++;
            col = 0;
        }
    }

    g_num_rows = row;
    return g_num_rows;
}

/* ── grid accessors ──────────────────────────────────────────────────── */

int32_t aria_csv_num_rows(void) { return g_num_rows; }
int32_t aria_csv_num_cols(void) { return g_num_cols; }

const char *aria_csv_get(int32_t row, int32_t col) {
    if (row < 0 || row >= g_num_rows || col < 0 || col >= MAX_COLS) {
        g_last_result = "";
        return g_last_result;
    }
    g_last_result = g_grid[row][col] ? g_grid[row][col] : "";
    return g_last_result;
}

int32_t aria_csv_get_s(int32_t row, int32_t col) {
    aria_csv_get(row, col);
    return 1;
}

const char *aria_csv_last_result(void) { return g_last_result; }

/* ── writer ──────────────────────────────────────────────────────────── */

static char  *g_out     = NULL;
static size_t g_out_len = 0;
static size_t g_out_cap = 0;
static int    g_out_col = 0;    /* current column within row */

static void out_append(const char *s, size_t len) {
    size_t needed = g_out_len + len + 1;
    if (needed > g_out_cap) {
        size_t newcap = needed * 2;
        if (newcap < 1024) newcap = 1024;
        char *tmp = realloc(g_out, newcap);
        if (!tmp) return;
        g_out = tmp;
        g_out_cap = newcap;
    }
    memcpy(g_out + g_out_len, s, len);
    g_out_len += len;
    g_out[g_out_len] = '\0';
}

void aria_csv_write_begin(void) {
    free(g_out);
    g_out = NULL;
    g_out_len = 0;
    g_out_cap = 0;
    g_out_col = 0;
}

void aria_csv_write_field(const char *value) {
    if (g_out_col > 0) out_append(",", 1);

    /* Check if quoting is needed */
    int needs_quote = 0;
    for (const char *c = value; *c; c++) {
        if (*c == ',' || *c == '"' || *c == '\n' || *c == '\r') {
            needs_quote = 1;
            break;
        }
    }

    if (needs_quote) {
        out_append("\"", 1);
        for (const char *c = value; *c; c++) {
            if (*c == '"') out_append("\"\"", 2);
            else { out_append(c, 1); }
        }
        out_append("\"", 1);
    } else {
        out_append(value, strlen(value));
    }
    g_out_col++;
}

void aria_csv_write_row_end(void) {
    out_append("\n", 1);
    g_out_col = 0;
}

const char *aria_csv_write_result(void) {
    return g_out ? g_out : "";
}

int32_t aria_csv_write_length(void) {
    return (int32_t)g_out_len;
}

void aria_csv_free(void) {
    grid_clear();
    free(g_out);
    g_out = NULL;
    g_out_len = 0;
    g_out_cap = 0;
}

/* alias: Aria compiler bug — "free" in extern name generates local stub */
void aria_csv_cleanup(void) { aria_csv_free(); }

/* ── test helpers (C-side assertions) ────────────────────────────────── */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_csv_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual);
    }
}

void aria_csv_assert_true(int32_t val, const char *msg) {
    if (val) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s\n", msg);
    }
}

void aria_csv_assert_field_eq(int32_t row, int32_t col, const char *expected, const char *msg) {
    const char *actual = aria_csv_get(row, col);
    if (actual && strcmp(actual, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, actual ? actual : "NULL");
    }
}

void aria_csv_assert_write_contains(const char *substring, const char *msg) {
    const char *result = aria_csv_write_result();
    if (result && strstr(result, substring)) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (substring '%s' not found)\n", msg, substring);
    }
}

void aria_csv_assert_roundtrip_field(int32_t row, int32_t col, const char *expected, const char *msg) {
    /* Parse the current write result, then check a field */
    const char *wr = aria_csv_write_result();
    aria_csv_parse(wr);
    aria_csv_assert_field_eq(row, col, expected, msg);
}

int32_t aria_csv_test_passed(void) { return g_test_passed; }
int32_t aria_csv_test_failed(void) { return g_test_failed; }

void aria_csv_test_summary(void) {
    printf("\n=== aria-csv test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
