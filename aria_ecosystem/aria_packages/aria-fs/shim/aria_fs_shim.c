/*
 * aria_fs_shim.c — Filesystem utilities shim for Aria
 * Wraps POSIX file operations: read/write files, stat, mkdir, readdir, etc.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

/* ═══════════════════ error buffer ═══════════════════ */

static char g_err[512] = "";
static const char *g_last_result = "";

static void set_err(const char *msg) {
    snprintf(g_err, sizeof(g_err), "%s", msg);
}

static void set_errno_err(const char *prefix) {
    snprintf(g_err, sizeof(g_err), "%s: %s", prefix, strerror(errno));
}

const char *aria_fs_error(void) { return g_err; }

/* ═══════════════════ file read / write ═══════════════════ */

static char  *g_read_buf  = NULL;
static int64_t g_read_len = 0;

const char *aria_fs_read_file(const char *path) {
    g_err[0] = '\0';
    if (g_read_buf) { free(g_read_buf); g_read_buf = NULL; g_read_len = 0; }

    FILE *f = fopen(path, "rb");
    if (!f) { set_errno_err("read_file"); return ""; }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    g_read_buf = (char *)malloc((size_t)sz + 1);
    if (!g_read_buf) { fclose(f); set_err("read_file: out of memory"); g_read_len = 0; return ""; }

    size_t n = fread(g_read_buf, 1, (size_t)sz, f);
    fclose(f);
    g_read_buf[n] = '\0';
    g_read_len = (int64_t)n;
    g_last_result = g_read_buf;
    return g_last_result;
}

int64_t aria_fs_read_length(void) { return g_read_len; }

int32_t aria_fs_write_file(const char *path, const char *data) {
    g_err[0] = '\0';
    FILE *f = fopen(path, "wb");
    if (!f) { set_errno_err("write_file"); return 0; }
    size_t len = strlen(data);
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    if (n != len) { set_err("write_file: incomplete write"); return 0; }
    return 1;
}

int32_t aria_fs_append_file(const char *path, const char *data) {
    g_err[0] = '\0';
    FILE *f = fopen(path, "ab");
    if (!f) { set_errno_err("append_file"); return 0; }
    size_t len = strlen(data);
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    if (n != len) { set_err("append_file: incomplete write"); return 0; }
    return 1;
}

/* ═══════════════════ existence / stat ═══════════════════ */

int32_t aria_fs_exists(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0) ? 1 : 0;
}

int32_t aria_fs_is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISREG(st.st_mode) ? 1 : 0;
}

int32_t aria_fs_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

int64_t aria_fs_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_size;
}

int64_t aria_fs_modified_time(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (int64_t)st.st_mtime;
}

/* ═══════════════════ directory operations ═══════════════════ */

int32_t aria_fs_mkdir(const char *path) {
    g_err[0] = '\0';
    if (mkdir(path, 0755) != 0) {
        if (errno == EEXIST) return 1; /* already exists, ok */
        set_errno_err("mkdir");
        return 0;
    }
    return 1;
}

int32_t aria_fs_rmdir(const char *path) {
    g_err[0] = '\0';
    if (rmdir(path) != 0) { set_errno_err("rmdir"); return 0; }
    return 1;
}

/* readdir — returns entries as newline-separated string */
static char *g_dir_buf = NULL;

const char *aria_fs_list_dir(const char *path) {
    g_err[0] = '\0';
    if (g_dir_buf) { free(g_dir_buf); g_dir_buf = NULL; }

    DIR *d = opendir(path);
    if (!d) { set_errno_err("list_dir"); return ""; }

    size_t cap = 1024, len = 0;
    g_dir_buf = (char *)malloc(cap);
    if (!g_dir_buf) { closedir(d); set_err("list_dir: out of memory"); return ""; }
    g_dir_buf[0] = '\0';

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        /* skip . and .. */
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        size_t nlen = strlen(ent->d_name);
        /* need space for name + newline + null */
        while (len + nlen + 2 > cap) {
            cap *= 2;
            char *tmp = (char *)realloc(g_dir_buf, cap);
            if (!tmp) { closedir(d); set_err("list_dir: out of memory"); return g_dir_buf; }
            g_dir_buf = tmp;
        }
        if (len > 0) g_dir_buf[len++] = '\n';
        memcpy(g_dir_buf + len, ent->d_name, nlen);
        len += nlen;
        g_dir_buf[len] = '\0';
    }
    closedir(d);
    g_last_result = g_dir_buf;
    return g_last_result;
}

/* ═══════════════════ file operations ═══════════════════ */

int32_t aria_fs_remove(const char *path) {
    g_err[0] = '\0';
    if (remove(path) != 0) { set_errno_err("remove"); return 0; }
    return 1;
}

int32_t aria_fs_rename(const char *oldpath, const char *newpath) {
    g_err[0] = '\0';
    if (rename(oldpath, newpath) != 0) { set_errno_err("rename"); return 0; }
    return 1;
}

int32_t aria_fs_copy(const char *src, const char *dst) {
    g_err[0] = '\0';
    FILE *fin = fopen(src, "rb");
    if (!fin) { set_errno_err("copy(src)"); return 0; }

    FILE *fout = fopen(dst, "wb");
    if (!fout) { fclose(fin); set_errno_err("copy(dst)"); return 0; }

    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
        if (fwrite(buf, 1, n, fout) != n) {
            fclose(fin); fclose(fout);
            set_err("copy: write error");
            return 0;
        }
    }
    fclose(fin);
    fclose(fout);
    return 1;
}

/* ═══════════════════ path helpers ═══════════════════ */

static char g_cwd[4096] = "";

const char *aria_fs_get_cwd(void) {
    if (getcwd(g_cwd, sizeof(g_cwd)) == NULL) {
        set_errno_err("get_cwd");
        return "";
    }
    return g_cwd;
}

static char g_abs[4096] = "";

const char *aria_fs_absolute(const char *path) {
    if (realpath(path, g_abs) == NULL) {
        set_errno_err("absolute");
        g_last_result = "";
        return g_last_result;
    }
    g_last_result = g_abs;
    return g_last_result;
}

static char g_ext[256] = "";

const char *aria_fs_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    const char *sep = strrchr(path, '/');
    if (!dot || (sep && dot < sep)) { g_ext[0] = '\0'; g_last_result = g_ext; return g_last_result; }
    snprintf(g_ext, sizeof(g_ext), "%s", dot);
    g_last_result = g_ext;
    return g_last_result;
}

static char g_base[256] = "";

const char *aria_fs_basename(const char *path) {
    const char *sep = strrchr(path, '/');
    const char *name = sep ? sep + 1 : path;
    snprintf(g_base, sizeof(g_base), "%s", name);
    g_last_result = g_base;
    return g_last_result;
}

static char g_dirname[4096] = "";

const char *aria_fs_dirname(const char *path) {
    snprintf(g_dirname, sizeof(g_dirname), "%s", path);
    char *sep = strrchr(g_dirname, '/');
    if (sep) {
        if (sep == g_dirname) { g_dirname[1] = '\0'; }
        else { *sep = '\0'; }
    } else {
        g_dirname[0] = '.'; g_dirname[1] = '\0';
    }
    g_last_result = g_dirname;
    return g_last_result;
}

const char *aria_fs_last_result(void) { return g_last_result; }

/* ═══════════════════ cleanup ═══════════════════ */

void aria_fs_cleanup(void) {
    if (g_read_buf) { free(g_read_buf); g_read_buf = NULL; g_read_len = 0; }
    if (g_dir_buf)  { free(g_dir_buf);  g_dir_buf = NULL; }
}

/* ═══════════════════ test helpers (C-side assertions) ═══════════════════ */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_fs_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_fs_assert_int64_positive(int64_t val, const char *msg) {
    if (val > 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (got %ld, expected > 0)\n", msg, val); }
}

void aria_fs_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

void aria_fs_assert_file_content_eq(const char *path, const char *expected, const char *msg) {
    const char *content = aria_fs_read_file(path);
    if (strcmp(content, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, content); }
}

void aria_fs_assert_extension_eq(const char *path, const char *expected, const char *msg) {
    const char *result = aria_fs_extension(path);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_fs_assert_basename_eq(const char *path, const char *expected, const char *msg) {
    const char *result = aria_fs_basename(path);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_fs_assert_dirname_eq(const char *path, const char *expected, const char *msg) {
    const char *result = aria_fs_dirname(path);
    if (strcmp(result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, result); }
}

void aria_fs_test_summary(void) {
    printf("\n=== aria-fs test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
