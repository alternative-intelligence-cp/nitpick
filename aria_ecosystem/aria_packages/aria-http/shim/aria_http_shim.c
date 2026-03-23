/*  aria_http_shim.c — flat-parameter bridge between Aria FFI and libcurl
 *
 *  Manages a single CURL easy handle internally.
 *  Response body is captured into a growable buffer.
 *  Custom headers are accumulated in a curl_slist.
 *
 *  Build:
 *    cc -O2 -shared -fPIC -Wall -o libaria_http_shim.so aria_http_shim.c \
 *       $(pkg-config --cflags --libs libcurl)
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

/* ── response buffer ─────────────────────────────────────────────────── */

static char  *g_resp_buf  = NULL;
static size_t g_resp_len  = 0;
static size_t g_resp_cap  = 0;

static void resp_reset(void) {
    g_resp_len = 0;
    if (g_resp_buf) g_resp_buf[0] = '\0';
}

static size_t write_cb(char *data, size_t size, size_t nmemb, void *ud) {
    (void)ud;
    size_t total = size * nmemb;
    size_t needed = g_resp_len + total + 1;
    if (needed > g_resp_cap) {
        size_t newcap = needed * 2;
        if (newcap < 4096) newcap = 4096;
        char *tmp = realloc(g_resp_buf, newcap);
        if (!tmp) return 0;
        g_resp_buf = tmp;
        g_resp_cap = newcap;
    }
    memcpy(g_resp_buf + g_resp_len, data, total);
    g_resp_len += total;
    g_resp_buf[g_resp_len] = '\0';
    return total;
}

/* ── response header capture ─────────────────────────────────────────── */

static char  *g_resp_headers     = NULL;
static size_t g_resp_headers_len = 0;
static size_t g_resp_headers_cap = 0;

static void resp_headers_reset(void) {
    g_resp_headers_len = 0;
    if (g_resp_headers) g_resp_headers[0] = '\0';
}

static size_t header_cb(char *data, size_t size, size_t nmemb, void *ud) {
    (void)ud;
    size_t total = size * nmemb;
    size_t needed = g_resp_headers_len + total + 1;
    if (needed > g_resp_headers_cap) {
        size_t newcap = needed * 2;
        if (newcap < 4096) newcap = 4096;
        char *tmp = realloc(g_resp_headers, newcap);
        if (!tmp) return 0;
        g_resp_headers = tmp;
        g_resp_headers_cap = newcap;
    }
    memcpy(g_resp_headers + g_resp_headers_len, data, total);
    g_resp_headers_len += total;
    g_resp_headers[g_resp_headers_len] = '\0';
    return total;
}

/* ── internal state ──────────────────────────────────────────────────── */

static CURL              *g_curl    = NULL;
static struct curl_slist *g_headers = NULL;
static int32_t            g_status  = 0;
static char               g_error[CURL_ERROR_SIZE] = {0};

/* ── init / cleanup ──────────────────────────────────────────────────── */

int32_t aria_http_init(void) {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) return 0;
    g_curl = curl_easy_init();
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(g_curl, CURLOPT_HEADERFUNCTION, header_cb);
    curl_easy_setopt(g_curl, CURLOPT_ERRORBUFFER, g_error);
    curl_easy_setopt(g_curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(g_curl, CURLOPT_TIMEOUT, 30L);
    return 1;
}

void aria_http_cleanup(void) {
    if (g_headers) { curl_slist_free_all(g_headers); g_headers = NULL; }
    if (g_curl)    { curl_easy_cleanup(g_curl); g_curl = NULL; }
    curl_global_cleanup();
    free(g_resp_buf);      g_resp_buf = NULL; g_resp_len = 0; g_resp_cap = 0;
    free(g_resp_headers);  g_resp_headers = NULL; g_resp_headers_len = 0; g_resp_headers_cap = 0;
}

/* ── request configuration ───────────────────────────────────────────── */

void aria_http_set_header(const char *header) {
    g_headers = curl_slist_append(g_headers, header);
}

void aria_http_clear_headers(void) {
    if (g_headers) { curl_slist_free_all(g_headers); g_headers = NULL; }
}

void aria_http_set_timeout(int32_t seconds) {
    if (g_curl) curl_easy_setopt(g_curl, CURLOPT_TIMEOUT, (long)seconds);
}

void aria_http_set_follow_redirects(int32_t follow) {
    if (g_curl) curl_easy_setopt(g_curl, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L);
}

void aria_http_set_user_agent(const char *ua) {
    if (g_curl) curl_easy_setopt(g_curl, CURLOPT_USERAGENT, ua);
}

/* ── perform request (internal) ──────────────────────────────────────── */

static int32_t perform(void) {
    resp_reset();
    resp_headers_reset();
    if (g_headers) curl_easy_setopt(g_curl, CURLOPT_HTTPHEADER, g_headers);
    CURLcode res = curl_easy_perform(g_curl);
    if (res != CURLE_OK) {
        g_status = -1;
        return 0;
    }
    long code = 0;
    curl_easy_getinfo(g_curl, CURLINFO_RESPONSE_CODE, &code);
    g_status = (int32_t)code;
    return 1;
}

/* ── HTTP methods ────────────────────────────────────────────────────── */

int32_t aria_http_get(const char *url) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_HTTPGET, 1L);
    return perform();
}

int32_t aria_http_post(const char *url, const char *body) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_POST, 1L);
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, body);
    return perform();
}

int32_t aria_http_put(const char *url, const char *body) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, body);
    return perform();
}

int32_t aria_http_delete(const char *url) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, "");
    return perform();
}

int32_t aria_http_patch(const char *url, const char *body) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(g_curl, CURLOPT_POSTFIELDS, body);
    return perform();
}

int32_t aria_http_head(const char *url) {
    if (!g_curl) return 0;
    curl_easy_setopt(g_curl, CURLOPT_URL, url);
    curl_easy_setopt(g_curl, CURLOPT_NOBODY, 1L);
    int32_t r = perform();
    curl_easy_setopt(g_curl, CURLOPT_NOBODY, 0L);
    return r;
}

/* ── response accessors ──────────────────────────────────────────────── */

int32_t aria_http_status(void) {
    return g_status;
}

const char *aria_http_response_body(void) {
    return g_resp_buf ? g_resp_buf : "";
}

int32_t aria_http_response_length(void) {
    return (int32_t)g_resp_len;
}

const char *aria_http_response_headers(void) {
    return g_resp_headers ? g_resp_headers : "";
}

const char *aria_http_error(void) {
    return g_error;
}

/* ── URL encoding ────────────────────────────────────────────────────── */

static char *g_encoded = NULL;
static const char *g_last_result = "";

const char *aria_http_url_encode(const char *str) {
    if (g_encoded) { curl_free(g_encoded); g_encoded = NULL; }
    if (!g_curl) { g_last_result = ""; return g_last_result; }
    g_encoded = curl_easy_escape(g_curl, str, 0);
    g_last_result = g_encoded ? g_encoded : "";
    return g_last_result;
}

const char *aria_http_last_result(void) { return g_last_result; }

/* ── test helpers ────────────────────────────────────────────────────── */

#include <stdio.h>

static int g_test_passed = 0;
static int g_test_failed = 0;

void aria_http_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (got %d, expected %d)\n", msg, actual, expected);
    }
}

void aria_http_assert_true(int32_t val, const char *msg) {
    if (val) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (expected true, got 0)\n", msg);
    }
}

void aria_http_assert_url_encode_eq(const char *input, const char *expected, const char *msg) {
    const char *result = aria_http_url_encode(input);
    if (result && strcmp(result, expected) == 0) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (got \"%s\", expected \"%s\")\n", msg,
               result ? result : "(null)", expected);
    }
}

void aria_http_assert_body_contains(const char *substring, const char *msg) {
    const char *body = aria_http_response_body();
    if (body && strstr(body, substring)) {
        g_test_passed++;
        printf("[PASS] %s\n", msg);
    } else {
        g_test_failed++;
        printf("[FAIL] %s (body does not contain \"%s\")\n", msg, substring);
    }
}

void aria_http_test_summary(void) {
    printf("\n=== aria-http test summary ===\n");
    printf("passed=%d failed=%d total=%d\n",
           g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
