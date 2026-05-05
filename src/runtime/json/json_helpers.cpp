/**
 * Thin C helpers for Aria's pure-Aria JSON module (v0.2.0 self-hosting).
 *
 * Provides the irreducible operations for the JSON value tree:
 *   - Node allocation, deallocation, deep clone
 *   - Type query and scalar value get/set
 *   - Object key-value management (set, find, iterate)
 *   - Array element management (push, get, length)
 *   - DynBuf for efficient string building (encoder)
 *   - Number formatting/parsing
 *   - Parser state management (mutable position)
 *
 * All logic (public API, encoder, parser) lives in stdlib/json.aria.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════
// Internal JSON value tree
// ═══════════════════════════════════════════════════════════════════════

enum JsonType : int32_t {
    JSON_NULL   = 0,
    JSON_BOOL   = 1,
    JSON_INT    = 2,
    JSON_FLOAT  = 3,
    JSON_STRING = 4,
    JSON_ARRAY  = 5,
    JSON_OBJECT = 6
};

struct JsonValue {
    JsonType type;
    union {
        int32_t  bool_val;
        int64_t  int_val;
        double   float_val;
        char*    str_val;
    };
    JsonValue** arr_items;
    int64_t     arr_len;
    int64_t     arr_cap;
    char**      obj_keys;
    JsonValue** obj_vals;
    int64_t     obj_len;
    int64_t     obj_cap;
};

static JsonValue* jv_alloc(JsonType t) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (!v) std::abort();
    v->type = t;
    return v;
}

static void jv_free(JsonValue* v) {
    if (!v) return;
    if (v->type == JSON_STRING) {
        free(v->str_val);
    } else if (v->type == JSON_ARRAY) {
        for (int64_t i = 0; i < v->arr_len; i++)
            jv_free(v->arr_items[i]);
        free(v->arr_items);
    } else if (v->type == JSON_OBJECT) {
        for (int64_t i = 0; i < v->obj_len; i++) {
            free(v->obj_keys[i]);
            jv_free(v->obj_vals[i]);
        }
        free(v->obj_keys);
        free(v->obj_vals);
    }
    free(v);
}

static JsonValue* jv_clone(JsonValue* v) {
    if (!v) return nullptr;
    JsonValue* c = jv_alloc(v->type);
    switch (v->type) {
        case JSON_NULL: break;
        case JSON_BOOL: c->bool_val = v->bool_val; break;
        case JSON_INT: c->int_val = v->int_val; break;
        case JSON_FLOAT: c->float_val = v->float_val; break;
        case JSON_STRING: c->str_val = v->str_val ? strdup(v->str_val) : nullptr; break;
        case JSON_ARRAY:
            if (v->arr_len > 0) {
                c->arr_cap = v->arr_len;
                c->arr_items = (JsonValue**)malloc(c->arr_cap * sizeof(JsonValue*));
                if (!c->arr_items) std::abort();
                for (int64_t i = 0; i < v->arr_len; i++)
                    c->arr_items[i] = jv_clone(v->arr_items[i]);
                c->arr_len = v->arr_len;
            }
            break;
        case JSON_OBJECT:
            if (v->obj_len > 0) {
                c->obj_cap = v->obj_len;
                c->obj_keys = (char**)malloc(c->obj_cap * sizeof(char*));
                c->obj_vals = (JsonValue**)malloc(c->obj_cap * sizeof(JsonValue*));
                if (!c->obj_keys || !c->obj_vals) std::abort();
                for (int64_t i = 0; i < v->obj_len; i++) {
                    c->obj_keys[i] = strdup(v->obj_keys[i]);
                    c->obj_vals[i] = jv_clone(v->obj_vals[i]);
                }
                c->obj_len = v->obj_len;
            }
            break;
    }
    return c;
}

// ═══════════════════════════════════════════════════════════════════════
// AriaString helper
// ═══════════════════════════════════════════════════════════════════════

static AriaString* make_aria_string(const char* data, int64_t length) {
    char* buf = (char*)npk_gc_alloc(length + 1, 0);
    if (!buf) std::abort();
    if (length > 0) memcpy(buf, data, length);
    buf[length] = '\0';
    AriaString* s = (AriaString*)npk_gc_alloc(sizeof(AriaString), 0);
    if (!s) std::abort();
    s->data = buf;
    s->length = length;
    return s;
}

// ═══════════════════════════════════════════════════════════════════════
// DynBuf for string building
// ═══════════════════════════════════════════════════════════════════════

struct DynBuf {
    char*   data;
    int64_t len;
    int64_t cap;
};

// ═══════════════════════════════════════════════════════════════════════
// Object/Array internal helpers
// ═══════════════════════════════════════════════════════════════════════

static void obj_grow(JsonValue* obj) {
    if (obj->obj_len >= obj->obj_cap) {
        int64_t newcap = obj->obj_cap < 4 ? 4 : obj->obj_cap * 2;
        obj->obj_keys = (char**)realloc(obj->obj_keys, newcap * sizeof(char*));
        obj->obj_vals = (JsonValue**)realloc(obj->obj_vals, newcap * sizeof(JsonValue*));
        if (!obj->obj_keys || !obj->obj_vals) std::abort();
        obj->obj_cap = newcap;
    }
}

static void arr_grow_internal(JsonValue* arr) {
    if (arr->arr_len >= arr->arr_cap) {
        int64_t newcap = arr->arr_cap < 4 ? 4 : arr->arr_cap * 2;
        arr->arr_items = (JsonValue**)realloc(arr->arr_items, newcap * sizeof(JsonValue*));
        if (!arr->arr_items) std::abort();
        arr->arr_cap = newcap;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Public C API (extern "C")
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Node constructors ────────────────────────────────────────────────

void* jh_new_null()                  { return (void*)jv_alloc(JSON_NULL); }
void* jh_new_bool(int32_t val)       { auto* v = jv_alloc(JSON_BOOL); v->bool_val = val ? 1 : 0; return v; }
void* jh_new_int(int64_t val)        { auto* v = jv_alloc(JSON_INT); v->int_val = val; return v; }
void* jh_new_float(double val)       { auto* v = jv_alloc(JSON_FLOAT); v->float_val = val; return v; }
void* jh_new_string(const char* val) { auto* v = jv_alloc(JSON_STRING); v->str_val = val ? strdup(val) : strdup(""); return v; }
void* jh_new_object()               { return (void*)jv_alloc(JSON_OBJECT); }
void* jh_new_array()                { return (void*)jv_alloc(JSON_ARRAY); }

// ── Node query ───────────────────────────────────────────────────────

int32_t jh_type(void* h)       { return h ? ((JsonValue*)h)->type : JSON_NULL; }
int32_t jh_get_bool(void* h)   { auto* v = (JsonValue*)h; return (v && v->type == JSON_BOOL) ? v->bool_val : 0; }
int64_t jh_get_int(void* h)    { auto* v = (JsonValue*)h; if (!v) return 0; if (v->type == JSON_INT) return v->int_val; if (v->type == JSON_FLOAT) return (int64_t)v->float_val; return 0; }
double  jh_get_float(void* h)  { auto* v = (JsonValue*)h; if (!v) return 0.0; if (v->type == JSON_FLOAT) return v->float_val; if (v->type == JSON_INT) return (double)v->int_val; return 0.0; }

AriaString* jh_get_string(void* h) {
    auto* v = (JsonValue*)h;
    if (!v || v->type != JSON_STRING || !v->str_val)
        return make_aria_string("", 0);
    return make_aria_string(v->str_val, (int64_t)strlen(v->str_val));
}

// ── Object helpers ───────────────────────────────────────────────────

void jh_obj_set(void* h, const char* key, void* child) {
    auto* obj = (JsonValue*)h;
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    int64_t klen = (int64_t)strlen(key);
    for (int64_t i = 0; i < obj->obj_len; i++) {
        if (strcmp(obj->obj_keys[i], key) == 0) {
            jv_free(obj->obj_vals[i]);
            obj->obj_vals[i] = (JsonValue*)child;
            return;
        }
    }
    obj_grow(obj);
    char* kcopy = (char*)malloc(klen + 1);
    if (!kcopy) std::abort();
    memcpy(kcopy, key, klen + 1);
    obj->obj_keys[obj->obj_len] = kcopy;
    obj->obj_vals[obj->obj_len] = (JsonValue*)child;
    obj->obj_len++;
}

void* jh_obj_find(void* h, const char* key) {
    auto* obj = (JsonValue*)h;
    if (!obj || obj->type != JSON_OBJECT || !key) return nullptr;
    for (int64_t i = 0; i < obj->obj_len; i++) {
        if (strcmp(obj->obj_keys[i], key) == 0)
            return (void*)obj->obj_vals[i];
    }
    return nullptr;
}

int64_t jh_obj_count(void* h) {
    auto* obj = (JsonValue*)h;
    if (!obj || obj->type != JSON_OBJECT) return 0;
    return obj->obj_len;
}

AriaString* jh_obj_key_at(void* h, int64_t idx) {
    auto* obj = (JsonValue*)h;
    if (!obj || obj->type != JSON_OBJECT || idx < 0 || idx >= obj->obj_len)
        return make_aria_string("", 0);
    return make_aria_string(obj->obj_keys[idx], (int64_t)strlen(obj->obj_keys[idx]));
}

void* jh_obj_val_at(void* h, int64_t idx) {
    auto* obj = (JsonValue*)h;
    if (!obj || obj->type != JSON_OBJECT || idx < 0 || idx >= obj->obj_len)
        return nullptr;
    return (void*)obj->obj_vals[idx];
}

// ── Array helpers ────────────────────────────────────────────────────

void jh_arr_push(void* h, void* child) {
    auto* arr = (JsonValue*)h;
    if (!arr || arr->type != JSON_ARRAY) return;
    arr_grow_internal(arr);
    arr->arr_items[arr->arr_len++] = (JsonValue*)child;
}

void* jh_arr_get(void* h, int64_t idx) {
    auto* arr = (JsonValue*)h;
    if (!arr || arr->type != JSON_ARRAY || idx < 0 || idx >= arr->arr_len)
        return nullptr;
    return (void*)arr->arr_items[idx];
}

int64_t jh_arr_len(void* h) {
    auto* arr = (JsonValue*)h;
    if (!arr || arr->type != JSON_ARRAY) return 0;
    return arr->arr_len;
}

// ── Clone / Free ─────────────────────────────────────────────────────

void* jh_clone(void* h) {
    return (void*)jv_clone((JsonValue*)h);
}

void jh_free(void* h) {
    jv_free((JsonValue*)h);
}

// ── DynBuf helpers (for encoder) ─────────────────────────────────────

void* jh_buf_new() {
    DynBuf* b = (DynBuf*)malloc(sizeof(DynBuf));
    if (!b) std::abort();
    b->cap = 256;
    b->len = 0;
    b->data = (char*)malloc(b->cap);
    if (!b->data) std::abort();
    return (void*)b;
}

void jh_buf_char(void* buf, int32_t c) {
    DynBuf* b = (DynBuf*)buf;
    if (b->len + 1 >= b->cap) {
        b->cap *= 2;
        b->data = (char*)realloc(b->data, b->cap);
        if (!b->data) std::abort();
    }
    b->data[b->len++] = (char)c;
}

void jh_buf_str(void* buf, const char* s) {
    DynBuf* b = (DynBuf*)buf;
    int64_t n = (int64_t)strlen(s);
    while (b->len + n >= b->cap) {
        b->cap *= 2;
        b->data = (char*)realloc(b->data, b->cap);
        if (!b->data) std::abort();
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
}

AriaString* jh_buf_to_string(void* buf) {
    DynBuf* b = (DynBuf*)buf;
    AriaString* result = make_aria_string(b->data, b->len);
    free(b->data);
    free(b);
    return result;
}

// ── Number formatting (for encoder) ──────────────────────────────────

AriaString* jh_int_to_cstr(int64_t val) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", (long)val);
    return make_aria_string(tmp, (int64_t)strlen(tmp));
}

AriaString* jh_float_to_cstr(double val) {
    char tmp[64];
    if (std::isnan(val) || std::isinf(val)) {
        return make_aria_string("null", 4);
    }
    snprintf(tmp, sizeof(tmp), "%.17g", val);
    return make_aria_string(tmp, (int64_t)strlen(tmp));
}

int32_t jh_is_nan(double val) { return std::isnan(val) ? 1 : 0; }
int32_t jh_is_inf(double val) { return std::isinf(val) ? 1 : 0; }

// ── Number parsing (for parser) ──────────────────────────────────────

int64_t jh_parse_int(const char* s, int64_t start, int64_t len) {
    char* tmp = (char*)malloc(len + 1);
    if (!tmp) std::abort();
    memcpy(tmp, s + start, len);
    tmp[len] = '\0';
    int64_t result = strtoll(tmp, nullptr, 10);
    free(tmp);
    return result;
}

double jh_parse_float(const char* s, int64_t start, int64_t len) {
    char* tmp = (char*)malloc(len + 1);
    if (!tmp) std::abort();
    memcpy(tmp, s + start, len);
    tmp[len] = '\0';
    double result = strtod(tmp, nullptr);
    free(tmp);
    return result;
}

// ── Parser state (mutable position counter) ──────────────────────────

void* jh_ps_new(int64_t len) {
    int64_t* state = (int64_t*)malloc(2 * sizeof(int64_t));
    if (!state) std::abort();
    state[0] = 0;    // pos
    state[1] = len;  // len
    return (void*)state;
}

int64_t jh_ps_pos(void* ps)                   { return ((int64_t*)ps)[0]; }
int64_t jh_ps_len(void* ps)                   { return ((int64_t*)ps)[1]; }
void    jh_ps_set_pos(void* ps, int64_t pos)   { ((int64_t*)ps)[0] = pos; }
void    jh_ps_free(void* ps)                   { free(ps); }

// ── String byte access (for parser) ──────────────────────────────────

int32_t jh_byte_at(const char* s, int64_t idx) {
    return (int32_t)(unsigned char)s[idx];
}

int64_t jh_strlen(const char* s) {
    return s ? (int64_t)strlen(s) : 0;
}

// ── Type-specific object set helpers ─────────────────────────────────

void jh_obj_set_str(void* h, const char* key, const char* val) {
    jh_obj_set(h, key, jh_new_string(val));
}
void jh_obj_set_int(void* h, const char* key, int64_t val) {
    jh_obj_set(h, key, jh_new_int(val));
}
void jh_obj_set_flt(void* h, const char* key, double val) {
    jh_obj_set(h, key, jh_new_float(val));
}
void jh_obj_set_bool(void* h, const char* key, int32_t val) {
    jh_obj_set(h, key, jh_new_bool(val));
}
void jh_obj_set_null(void* h, const char* key) {
    jh_obj_set(h, key, jh_new_null());
}
void jh_obj_set_obj(void* h, const char* key, void* child) {
    jh_obj_set(h, key, jv_clone((JsonValue*)child));
}
void jh_obj_set_arr(void* h, const char* key, void* child) {
    jh_obj_set(h, key, jv_clone((JsonValue*)child));
}

// ── Type-specific array push helpers ─────────────────────────────────

void jh_arr_push_str(void* h, const char* val) {
    jh_arr_push(h, jh_new_string(val));
}
void jh_arr_push_int(void* h, int64_t val) {
    jh_arr_push(h, jh_new_int(val));
}
void jh_arr_push_flt(void* h, double val) {
    jh_arr_push(h, jh_new_float(val));
}
void jh_arr_push_bool(void* h, int32_t val) {
    jh_arr_push(h, jh_new_bool(val));
}
void jh_arr_push_null(void* h) {
    jh_arr_push(h, jh_new_null());
}
void jh_arr_push_obj(void* h, void* child) {
    jh_arr_push(h, jv_clone((JsonValue*)child));
}
void jh_arr_push_arr(void* h, void* child) {
    jh_arr_push(h, jv_clone((JsonValue*)child));
}

// ── Type-specific object getters ─────────────────────────────────────

int32_t jh_obj_has(void* h, const char* key) {
    return jh_obj_find(h, key) ? 1 : 0;
}

AriaString* jh_obj_get_str(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found || ((JsonValue*)found)->type != JSON_STRING)
        return make_aria_string("", 0);
    return jh_get_string(found);
}

int64_t jh_obj_get_int(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found) return 0;
    return jh_get_int(found);
}

double jh_obj_get_flt(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found) return 0.0;
    return jh_get_float(found);
}

int32_t jh_obj_get_bool(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found || ((JsonValue*)found)->type != JSON_BOOL) return 0;
    return jh_get_bool(found);
}

void* jh_obj_get_obj(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found || ((JsonValue*)found)->type != JSON_OBJECT)
        return (void*)jv_alloc(JSON_OBJECT);
    return jh_clone(found);
}

void* jh_obj_get_arr(void* h, const char* key) {
    void* found = jh_obj_find(h, key);
    if (!found || ((JsonValue*)found)->type != JSON_ARRAY)
        return (void*)jv_alloc(JSON_ARRAY);
    return jh_clone(found);
}

// ── Type-specific array getters ──────────────────────────────────────

AriaString* jh_arr_get_str(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem || ((JsonValue*)elem)->type != JSON_STRING)
        return make_aria_string("", 0);
    return jh_get_string(elem);
}

int64_t jh_arr_get_int(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem) return 0;
    return jh_get_int(elem);
}

double jh_arr_get_flt(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem) return 0.0;
    return jh_get_float(elem);
}

int32_t jh_arr_get_bool(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem || ((JsonValue*)elem)->type != JSON_BOOL) return 0;
    return jh_get_bool(elem);
}

void* jh_arr_get_obj(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem || ((JsonValue*)elem)->type != JSON_OBJECT)
        return (void*)jv_alloc(JSON_OBJECT);
    return jh_clone(elem);
}

void* jh_arr_get_arr(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem || ((JsonValue*)elem)->type != JSON_ARRAY)
        return (void*)jv_alloc(JSON_ARRAY);
    return jh_clone(elem);
}

int32_t jh_arr_get_type(void* h, int64_t idx) {
    void* elem = jh_arr_get(h, idx);
    if (!elem) return JSON_NULL;
    return ((JsonValue*)elem)->type;
}

// ═══════════════════════════════════════════════════════════════════════
// JSON Parser (recursive descent, entirely in C)
// ═══════════════════════════════════════════════════════════════════════

static void jp_skip_ws(const char* src, int64_t len, int64_t& pos) {
    while (pos < len) {
        char c = src[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            pos++;
        else
            break;
    }
}

static int32_t jp_peek(const char* src, int64_t len, int64_t& pos) {
    jp_skip_ws(src, len, pos);
    if (pos < len) return (int32_t)(unsigned char)src[pos];
    return 0;
}

static bool jp_expect(const char* src, int64_t len, int64_t& pos, char c) {
    jp_skip_ws(src, len, pos);
    if (pos < len && src[pos] == c) { pos++; return true; }
    return false;
}

static bool jp_match_lit(const char* src, int64_t len, int64_t& pos, const char* lit) {
    int64_t llen = (int64_t)strlen(lit);
    if (pos + llen > len) return false;
    if (memcmp(src + pos, lit, llen) != 0) return false;
    pos += llen;
    return true;
}

static JsonValue* jp_parse_string_raw(const char* src, int64_t len, int64_t& pos) {
    if (pos >= len || src[pos] != '"')
        return jv_alloc(JSON_STRING); // empty string node

    pos++; // skip opening quote
    DynBuf buf;
    buf.cap = 64;
    buf.len = 0;
    buf.data = (char*)malloc(buf.cap);
    if (!buf.data) std::abort();

    auto buf_putc = [&](char c) {
        if (buf.len + 1 >= buf.cap) {
            buf.cap *= 2;
            buf.data = (char*)realloc(buf.data, buf.cap);
            if (!buf.data) std::abort();
        }
        buf.data[buf.len++] = c;
    };

    while (pos < len && src[pos] != '"') {
        if (src[pos] == '\\') {
            pos++;
            if (pos >= len) break;
            char esc = src[pos];
            switch (esc) {
                case '"':  buf_putc('"'); break;
                case '\\': buf_putc('\\'); break;
                case '/':  buf_putc('/'); break;
                case 'b':  buf_putc('\b'); break;
                case 'f':  buf_putc('\f'); break;
                case 'n':  buf_putc('\n'); break;
                case 'r':  buf_putc('\r'); break;
                case 't':  buf_putc('\t'); break;
                case 'u': {
                    int32_t cp = 0;
                    for (int hx = 0; hx < 4; hx++) {
                        if (pos + 1 < len) {
                            pos++;
                            char hc = src[pos];
                            cp *= 16;
                            if (hc >= '0' && hc <= '9') cp += hc - '0';
                            else if (hc >= 'a' && hc <= 'f') cp += hc - 'a' + 10;
                            else if (hc >= 'A' && hc <= 'F') cp += hc - 'A' + 10;
                        }
                    }
                    if (cp < 0x80) {
                        buf_putc((char)cp);
                    } else if (cp < 0x800) {
                        buf_putc((char)(0xC0 | (cp >> 6)));
                        buf_putc((char)(0x80 | (cp & 0x3F)));
                    } else {
                        buf_putc((char)(0xE0 | (cp >> 12)));
                        buf_putc((char)(0x80 | ((cp >> 6) & 0x3F)));
                        buf_putc((char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: buf_putc(esc); break;
            }
            pos++;
        } else {
            buf_putc(src[pos]);
            pos++;
        }
    }
    if (pos < len) pos++; // skip closing quote

    buf.data[buf.len] = '\0';
    JsonValue* v = jv_alloc(JSON_STRING);
    v->str_val = (char*)malloc(buf.len + 1);
    if (!v->str_val) std::abort();
    memcpy(v->str_val, buf.data, buf.len + 1);
    free(buf.data);
    return v;
}

static char* jp_parse_key_string(const char* src, int64_t len, int64_t& pos) {
    jp_skip_ws(src, len, pos);
    JsonValue* node = jp_parse_string_raw(src, len, pos);
    char* result = node->str_val ? strdup(node->str_val) : strdup("");
    jv_free(node);
    return result;
}

static JsonValue* jp_parse_number(const char* src, int64_t len, int64_t& pos) {
    jp_skip_ws(src, len, pos);
    int64_t start = pos;
    bool is_float = false;

    if (pos < len && src[pos] == '-') pos++;

    while (pos < len && src[pos] >= '0' && src[pos] <= '9') pos++;

    if (pos < len && src[pos] == '.') {
        is_float = true;
        pos++;
        while (pos < len && src[pos] >= '0' && src[pos] <= '9') pos++;
    }

    if (pos < len && (src[pos] == 'e' || src[pos] == 'E')) {
        is_float = true;
        pos++;
        if (pos < len && (src[pos] == '+' || src[pos] == '-')) pos++;
        while (pos < len && src[pos] >= '0' && src[pos] <= '9') pos++;
    }

    int64_t numlen = pos - start;
    char* tmp = (char*)malloc(numlen + 1);
    if (!tmp) std::abort();
    memcpy(tmp, src + start, numlen);
    tmp[numlen] = '\0';

    JsonValue* result;
    if (is_float) {
        result = jv_alloc(JSON_FLOAT);
        result->float_val = strtod(tmp, nullptr);
    } else {
        result = jv_alloc(JSON_INT);
        result->int_val = strtoll(tmp, nullptr, 10);
    }
    free(tmp);
    return result;
}

// Forward declaration
static JsonValue* jp_parse_value(const char* src, int64_t len, int64_t& pos);

static JsonValue* jp_pv_obj(const char* src, int64_t len, int64_t& pos) {
    JsonValue* obj = jv_alloc(JSON_OBJECT);
    int32_t pk = jp_peek(src, len, pos);
    if (pk != '}') {
        bool more = true;
        while (more) {
            char* key = jp_parse_key_string(src, len, pos);
            jp_expect(src, len, pos, ':');
            JsonValue* val = jp_parse_value(src, len, pos);
            jh_obj_set(obj, key, val);
            free(key);
            if (!jp_expect(src, len, pos, ',')) more = false;
        }
    }
    jp_expect(src, len, pos, '}');
    return obj;
}

static JsonValue* jp_pv_arr(const char* src, int64_t len, int64_t& pos) {
    JsonValue* arr = jv_alloc(JSON_ARRAY);
    int32_t pk = jp_peek(src, len, pos);
    if (pk != ']') {
        bool more = true;
        while (more) {
            JsonValue* val = jp_parse_value(src, len, pos);
            arr_grow_internal(arr);
            arr->arr_items[arr->arr_len++] = val;
            if (!jp_expect(src, len, pos, ',')) more = false;
        }
    }
    jp_expect(src, len, pos, ']');
    return arr;
}

static JsonValue* jp_parse_value(const char* src, int64_t len, int64_t& pos) {
    jp_skip_ws(src, len, pos);
    if (pos >= len) return jv_alloc(JSON_NULL);

    char ch = src[pos];

    if (ch == '"') return jp_parse_string_raw(src, len, pos);

    if (ch == '{') { pos++; return jp_pv_obj(src, len, pos); }

    if (ch == '[') { pos++; return jp_pv_arr(src, len, pos); }

    if (ch == 't') { jp_match_lit(src, len, pos, "true"); auto* v = jv_alloc(JSON_BOOL); v->bool_val = 1; return v; }
    if (ch == 'f') { jp_match_lit(src, len, pos, "false"); auto* v = jv_alloc(JSON_BOOL); v->bool_val = 0; return v; }
    if (ch == 'n') { jp_match_lit(src, len, pos, "null"); return jv_alloc(JSON_NULL); }

    if (ch == '-' || (ch >= '0' && ch <= '9')) return jp_parse_number(src, len, pos);

    return jv_alloc(JSON_NULL);
}

void* json_parse(const char* json_str) {
    if (!json_str) return (void*)jv_alloc(JSON_NULL);
    int64_t len = (int64_t)strlen(json_str);
    int64_t pos = 0;
    return (void*)jp_parse_value(json_str, len, pos);
}

// ═══════════════════════════════════════════════════════════════════════
// Public API aliases (called directly as externs from Aria code)
// ═══════════════════════════════════════════════════════════════════════

void* json_new_object() { return jh_new_object(); }
void* json_new_array()  { return jh_new_array(); }

void json_obj_set_str(void* h, const char* key, const char* val)  { jh_obj_set_str(h, key, val); }
void json_obj_set_int(void* h, const char* key, int64_t val)      { jh_obj_set_int(h, key, val); }
void json_obj_set_flt(void* h, const char* key, double val)       { jh_obj_set_flt(h, key, val); }
void json_obj_set_bool(void* h, const char* key, int32_t val)     { jh_obj_set_bool(h, key, val); }
void json_obj_set_null(void* h, const char* key)                  { jh_obj_set_null(h, key); }
void json_obj_set_obj(void* h, const char* key, void* child)      { jh_obj_set_obj(h, key, child); }
void json_obj_set_arr(void* h, const char* key, void* child)      { jh_obj_set_arr(h, key, child); }

void json_arr_push_str(void* h, const char* val)    { jh_arr_push_str(h, val); }
void json_arr_push_int(void* h, int64_t val)        { jh_arr_push_int(h, val); }
void json_arr_push_flt(void* h, double val)         { jh_arr_push_flt(h, val); }
void json_arr_push_bool(void* h, int32_t val)       { jh_arr_push_bool(h, val); }
void json_arr_push_null(void* h)                    { jh_arr_push_null(h); }
void json_arr_push_obj(void* h, void* child)        { jh_arr_push_obj(h, child); }
void json_arr_push_arr(void* h, void* child)        { jh_arr_push_arr(h, child); }

int32_t     json_type(void* h)                                  { return jh_type(h); }
int32_t     json_obj_has(void* h, const char* key)              { return jh_obj_has(h, key); }
int64_t     json_obj_count(void* h)                             { return jh_obj_count(h); }
AriaString* json_obj_get_str(void* h, const char* key)          { return jh_obj_get_str(h, key); }
int64_t     json_obj_get_int(void* h, const char* key)          { return jh_obj_get_int(h, key); }
double      json_obj_get_flt(void* h, const char* key)          { return jh_obj_get_flt(h, key); }
int32_t     json_obj_get_bool(void* h, const char* key)         { return jh_obj_get_bool(h, key); }
void*       json_obj_get_obj(void* h, const char* key)          { return jh_obj_get_obj(h, key); }
void*       json_obj_get_arr(void* h, const char* key)          { return jh_obj_get_arr(h, key); }

int64_t     json_arr_len(void* h)                               { return jh_arr_len(h); }
AriaString* json_arr_get_str(void* h, int64_t idx)              { return jh_arr_get_str(h, idx); }
int64_t     json_arr_get_int(void* h, int64_t idx)              { return jh_arr_get_int(h, idx); }
double      json_arr_get_flt(void* h, int64_t idx)              { return jh_arr_get_flt(h, idx); }
int32_t     json_arr_get_bool(void* h, int64_t idx)             { return jh_arr_get_bool(h, idx); }
void*       json_arr_get_obj(void* h, int64_t idx)              { return jh_arr_get_obj(h, idx); }
void*       json_arr_get_arr(void* h, int64_t idx)              { return jh_arr_get_arr(h, idx); }
int32_t     json_arr_get_type(void* h, int64_t idx)             { return jh_arr_get_type(h, idx); }

void json_free(void* h) { jh_free(h); }

} // extern "C"
