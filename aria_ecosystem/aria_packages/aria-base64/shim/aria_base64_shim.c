/*
 * aria_base64_shim.c — Base64 encode / decode for Aria
 * Standard base64 (RFC 4648) and URL-safe variant.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ═══════════════════ tables ═══════════════════ */

static const char b64_std[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char b64_url[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static int decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

/* ═══════════════════ state ═══════════════════ */

static char *g_enc_buf = NULL;
static char *g_dec_buf = NULL;
static int64_t g_dec_len = 0;
static const char *g_last_result = "";

/* ═══════════════════ encode ═══════════════════ */

static const char *do_encode(const char *data, int32_t len, const char *table, int pad) {
    if (g_enc_buf) { free(g_enc_buf); g_enc_buf = NULL; }

    size_t out_len = 4 * (((size_t)len + 2) / 3);
    g_enc_buf = (char *)malloc(out_len + 1);
    if (!g_enc_buf) return "";

    size_t j = 0;
    for (int32_t i = 0; i < len; i += 3) {
        uint32_t a = (uint8_t)data[i];
        uint32_t b = (i + 1 < len) ? (uint8_t)data[i + 1] : 0;
        uint32_t c = (i + 2 < len) ? (uint8_t)data[i + 2] : 0;
        uint32_t triple = (a << 16) | (b << 8) | c;

        g_enc_buf[j++] = table[(triple >> 18) & 0x3F];
        g_enc_buf[j++] = table[(triple >> 12) & 0x3F];

        if (i + 1 < len)
            g_enc_buf[j++] = table[(triple >> 6) & 0x3F];
        else if (pad)
            g_enc_buf[j++] = '=';
        else
            break;

        if (i + 2 < len)
            g_enc_buf[j++] = table[triple & 0x3F];
        else if (pad)
            g_enc_buf[j++] = '=';
    }
    g_enc_buf[j] = '\0';
    return g_enc_buf;
}

const char *aria_base64_encode(const char *data, int32_t len) {
    g_last_result = do_encode(data, len, b64_std, 1);
    return g_last_result;
}

const char *aria_base64_encode_str(const char *str) {
    g_last_result = do_encode(str, (int32_t)strlen(str), b64_std, 1);
    return g_last_result;
}

const char *aria_base64_encode_url(const char *data, int32_t len) {
    g_last_result = do_encode(data, len, b64_url, 0);
    return g_last_result;
}

const char *aria_base64_encode_url_str(const char *str) {
    g_last_result = do_encode(str, (int32_t)strlen(str), b64_url, 0);
    return g_last_result;
}

/* ═══════════════════ decode ═══════════════════ */

const char *aria_base64_decode(const char *encoded) {
    if (g_dec_buf) { free(g_dec_buf); g_dec_buf = NULL; g_dec_len = 0; }

    size_t elen = strlen(encoded);
    if (elen == 0) { g_dec_len = 0; return ""; }

    /* strip padding for length calc */
    size_t pad = 0;
    if (elen >= 1 && encoded[elen - 1] == '=') pad++;
    if (elen >= 2 && encoded[elen - 2] == '=') pad++;

    size_t out_len = (elen / 4) * 3;
    /* handle unpadded input (URL-safe) */
    size_t rem = elen % 4;
    if (rem == 2) out_len += 1;
    else if (rem == 3) out_len += 2;
    out_len -= pad;

    g_dec_buf = (char *)malloc(out_len + 1);
    if (!g_dec_buf) { g_dec_len = 0; return ""; }

    size_t j = 0;
    for (size_t i = 0; i < elen; ) {
        int v[4] = {0, 0, 0, 0};
        int count = 0;
        while (i < elen && count < 4) {
            if (encoded[i] == '=') { i++; continue; }
            int d = decode_char(encoded[i]);
            if (d < 0) { i++; continue; }
            v[count++] = d;
            i++;
        }
        if (count >= 2) {
            uint32_t triple = ((uint32_t)v[0] << 18) | ((uint32_t)v[1] << 12) |
                              ((uint32_t)v[2] << 6)  | (uint32_t)v[3];
            if (j < out_len) g_dec_buf[j++] = (char)((triple >> 16) & 0xFF);
            if (j < out_len && count >= 3) g_dec_buf[j++] = (char)((triple >> 8) & 0xFF);
            if (j < out_len && count >= 4) g_dec_buf[j++] = (char)(triple & 0xFF);
        }
    }
    g_dec_buf[j] = '\0';
    g_dec_len = (int64_t)j;
    g_last_result = g_dec_buf;
    return g_last_result;
}

const char *aria_base64_last_result(void) { return g_last_result; }

int64_t aria_base64_decode_length(void) { return g_dec_len; }

/* ═══════════════════ utility ═══════════════════ */

int64_t aria_base64_encoded_length(int32_t input_len) {
    return (int64_t)(4 * (((size_t)input_len + 2) / 3));
}

int32_t aria_base64_is_valid(const char *str) {
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        char c = str[i];
        if (c == '=' && i >= len - 2) continue; /* padding ok at end */
        if (decode_char(c) < 0) return 0;
    }
    return 1;
}

/* ═══════════════════ cleanup ═══════════════════ */

void aria_base64_cleanup(void) {
    if (g_enc_buf) { free(g_enc_buf); g_enc_buf = NULL; }
    if (g_dec_buf) { free(g_dec_buf); g_dec_buf = NULL; g_dec_len = 0; }
}

/* ═══════════════════ test helpers ═══════════════════ */

static int32_t g_test_passed = 0;
static int32_t g_test_failed = 0;

void aria_base64_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %d, got %d)\n", msg, expected, actual); }
}

void aria_base64_assert_int64_eq(int64_t actual, int64_t expected, const char *msg) {
    if (actual == expected) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected %ld, got %ld)\n", msg, expected, actual); }
}

void aria_base64_assert_true(int32_t val, const char *msg) {
    if (val) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s\n", msg); }
}

/* encode str and compare result with expected */
void aria_base64_assert_encode_eq(const char *input, const char *expected, const char *msg) {
    aria_base64_encode_str(input);
    if (strcmp(g_last_result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, g_last_result); }
}

/* encode with length and compare */
void aria_base64_assert_encode_len_eq(const char *input, int32_t len, const char *expected, const char *msg) {
    aria_base64_encode(input, len);
    if (strcmp(g_last_result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, g_last_result); }
}

/* encode URL-safe and compare */
void aria_base64_assert_encode_url_eq(const char *input, const char *expected, const char *msg) {
    aria_base64_encode_url_str(input);
    if (strcmp(g_last_result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, g_last_result); }
}

/* decode and compare result with expected */
void aria_base64_assert_decode_eq(const char *encoded, const char *expected, const char *msg) {
    aria_base64_decode(encoded);
    if (strcmp(g_last_result, expected) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, expected, g_last_result); }
}

/* roundtrip: encode then decode, check matches original */
void aria_base64_assert_roundtrip(const char *input, const char *msg) {
    aria_base64_encode_str(input);
    const char *encoded = g_last_result;
    /* need a copy since decode will overwrite g_last_result */
    char enc_copy[4096];
    snprintf(enc_copy, sizeof(enc_copy), "%s", encoded);
    aria_base64_decode(enc_copy);
    if (strcmp(g_last_result, input) == 0) { g_test_passed++; printf("[PASS] %s\n", msg); }
    else { g_test_failed++; printf("[FAIL] %s (expected '%s', got '%s')\n", msg, input, g_last_result); }
}

void aria_base64_test_summary(void) {
    printf("\n=== aria-base64 test summary ===\n");
    printf("passed=%d failed=%d total=%d\n", g_test_passed, g_test_failed, g_test_passed + g_test_failed);
    if (g_test_failed == 0) printf("ALL TESTS PASSED\n");
    else printf("SOME TESTS FAILED\n");
}
