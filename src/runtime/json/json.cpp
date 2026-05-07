/**
 * JSON serialization/deserialization runtime for Aria stdlib.
 *
 * Provides:
 *  - JSON value tree (object, array, string, number, bool, null)
 *  - Builder API: json_new_object/array, json_obj_set_*, json_arr_push_*
 *  - Encoder: json_encode(handle) → AriaString*
 *  - Parser:  json_parse(json_str) → handle
 *  - Reader:  json_obj_get_*, json_arr_get_*, json_obj_has, json_arr_len
 *  - Cleanup: json_free(handle)
 *
 * ABI: string params arrive as const char* (codegen extracts .data).
 *       string returns are AriaString* via npk_gc_alloc.
 *       Opaque handles are void* (wild int8* in Aria).
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
        char*    str_val;       // owned, malloc'd, null-terminated
    };
    // For arrays: dynamic array of JsonValue*
    JsonValue** arr_items;
    int64_t     arr_len;
    int64_t     arr_cap;
    // For objects: parallel key/value arrays
    char**      obj_keys;       // owned, malloc'd
    JsonValue** obj_vals;
    int64_t     obj_len;
    int64_t     obj_cap;
};

static JsonValue* json_alloc(JsonType t) {
    JsonValue* v = (JsonValue*)calloc(1, sizeof(JsonValue));
    if (!v) std::abort();
    v->type = t;
    return v;
}

static void json_value_free(JsonValue* v) {
    if (!v) return;
    if (v->type == JSON_STRING) {
        free(v->str_val);
    } else if (v->type == JSON_ARRAY) {
        for (int64_t i = 0; i < v->arr_len; i++)
            json_value_free(v->arr_items[i]);
        free(v->arr_items);
    } else if (v->type == JSON_OBJECT) {
        for (int64_t i = 0; i < v->obj_len; i++) {
            free(v->obj_keys[i]);
            json_value_free(v->obj_vals[i]);
        }
        free(v->obj_keys);
        free(v->obj_vals);
    }
    free(v);
}

// Deep-clone a JsonValue tree (so parent and child can be freed independently)
static JsonValue* json_value_clone(JsonValue* v) {
    if (!v) return nullptr;
    JsonValue* c = json_alloc(v->type);
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
                    c->arr_items[i] = json_value_clone(v->arr_items[i]);
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
                    c->obj_vals[i] = json_value_clone(v->obj_vals[i]);
                }
                c->obj_len = v->obj_len;
            }
            break;
    }
    return c;
}

// Helper: make AriaString from C string
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
// Dynamic buffer for encoding
// ═══════════════════════════════════════════════════════════════════════

struct DynBuf {
    char*   data;
    int64_t len;
    int64_t cap;
};

static void buf_init(DynBuf* b) {
    b->cap = 256;
    b->len = 0;
    b->data = (char*)malloc(b->cap);
    if (!b->data) std::abort();
}

static void buf_ensure(DynBuf* b, int64_t extra) {
    while (b->len + extra >= b->cap) {
        b->cap *= 2;
        b->data = (char*)realloc(b->data, b->cap);
        if (!b->data) std::abort();
    }
}

static void buf_append(DynBuf* b, const char* s, int64_t n) {
    buf_ensure(b, n);
    memcpy(b->data + b->len, s, n);
    b->len += n;
}

static void buf_append_char(DynBuf* b, char c) {
    buf_ensure(b, 1);
    b->data[b->len++] = c;
}

static void buf_append_cstr(DynBuf* b, const char* s) {
    buf_append(b, s, strlen(s));
}

// ═══════════════════════════════════════════════════════════════════════
// Object / Array growth
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

static void arr_grow(JsonValue* arr) {
    if (arr->arr_len >= arr->arr_cap) {
        int64_t newcap = arr->arr_cap < 4 ? 4 : arr->arr_cap * 2;
        arr->arr_items = (JsonValue**)realloc(arr->arr_items, newcap * sizeof(JsonValue*));
        if (!arr->arr_items) std::abort();
        arr->arr_cap = newcap;
    }
}

static void obj_set(JsonValue* obj, const char* key, JsonValue* val) {
    if (!obj || obj->type != JSON_OBJECT || !key) return;
    int64_t klen = (int64_t)strlen(key);
    // Overwrite if key already exists
    for (int64_t i = 0; i < obj->obj_len; i++) {
        if (strcmp(obj->obj_keys[i], key) == 0) {
            json_value_free(obj->obj_vals[i]);
            obj->obj_vals[i] = val;
            return;
        }
    }
    obj_grow(obj);
    char* kcopy = (char*)malloc(klen + 1);
    if (!kcopy) std::abort();
    memcpy(kcopy, key, klen + 1);
    obj->obj_keys[obj->obj_len] = kcopy;
    obj->obj_vals[obj->obj_len] = val;
    obj->obj_len++;
}

static void arr_push(JsonValue* arr, JsonValue* val) {
    if (!arr || arr->type != JSON_ARRAY) return;
    arr_grow(arr);
    arr->arr_items[arr->arr_len++] = val;
}

// ═══════════════════════════════════════════════════════════════════════
// Encoder (value tree → JSON string)
// ═══════════════════════════════════════════════════════════════════════

static void encode_string_escaped(DynBuf* b, const char* s) {
    buf_append_char(b, '"');
    if (s) {
        for (const char* p = s; *p; p++) {
            switch (*p) {
                case '"':  buf_append_cstr(b, "\\\""); break;
                case '\\': buf_append_cstr(b, "\\\\"); break;
                case '\b': buf_append_cstr(b, "\\b");  break;
                case '\f': buf_append_cstr(b, "\\f");  break;
                case '\n': buf_append_cstr(b, "\\n");  break;
                case '\r': buf_append_cstr(b, "\\r");  break;
                case '\t': buf_append_cstr(b, "\\t");  break;
                default:
                    if ((unsigned char)*p < 0x20) {
                        char esc[8];
                        snprintf(esc, sizeof(esc), "\\u%04x", (unsigned char)*p);
                        buf_append_cstr(b, esc);
                    } else {
                        buf_append_char(b, *p);
                    }
            }
        }
    }
    buf_append_char(b, '"');
}

static void encode_value(DynBuf* b, JsonValue* v) {
    if (!v) { buf_append_cstr(b, "null"); return; }

    switch (v->type) {
        case JSON_NULL:
            buf_append_cstr(b, "null");
            break;
        case JSON_BOOL:
            buf_append_cstr(b, v->bool_val ? "true" : "false");
            break;
        case JSON_INT: {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%ld", (long)v->int_val);
            buf_append_cstr(b, tmp);
            break;
        }
        case JSON_FLOAT: {
            char tmp[64];
            if (std::isnan(v->float_val)) {
                snprintf(tmp, sizeof(tmp), "null");
            } else if (std::isinf(v->float_val)) {
                snprintf(tmp, sizeof(tmp), "null");
            } else {
                snprintf(tmp, sizeof(tmp), "%.17g", v->float_val);
            }
            buf_append_cstr(b, tmp);
            break;
        }
        case JSON_STRING:
            encode_string_escaped(b, v->str_val);
            break;
        case JSON_ARRAY:
            buf_append_char(b, '[');
            for (int64_t i = 0; i < v->arr_len; i++) {
                if (i > 0) buf_append_char(b, ',');
                encode_value(b, v->arr_items[i]);
            }
            buf_append_char(b, ']');
            break;
        case JSON_OBJECT:
            buf_append_char(b, '{');
            for (int64_t i = 0; i < v->obj_len; i++) {
                if (i > 0) buf_append_char(b, ',');
                encode_string_escaped(b, v->obj_keys[i]);
                buf_append_char(b, ':');
                encode_value(b, v->obj_vals[i]);
            }
            buf_append_char(b, '}');
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Parser (JSON string → value tree)
// ═══════════════════════════════════════════════════════════════════════

struct Parser {
    const char* src;
    int64_t     pos;
    int64_t     len;
};

static void skip_ws(Parser* p) {
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
            p->pos++;
        else
            break;
    }
}

static char peek(Parser* p) {
    skip_ws(p);
    return p->pos < p->len ? p->src[p->pos] : '\0';
}

static bool expect(Parser* p, char c) {
    skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == c) {
        p->pos++;
        return true;
    }
    return false;
}

static JsonValue* parse_value(Parser* p);

static char* parse_string_raw(Parser* p) {
    if (!expect(p, '"')) return nullptr;
    DynBuf b;
    buf_init(&b);
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            switch (p->src[p->pos]) {
                case '"':  buf_append_char(&b, '"');  break;
                case '\\': buf_append_char(&b, '\\'); break;
                case '/':  buf_append_char(&b, '/');  break;
                case 'b':  buf_append_char(&b, '\b'); break;
                case 'f':  buf_append_char(&b, '\f'); break;
                case 'n':  buf_append_char(&b, '\n'); break;
                case 'r':  buf_append_char(&b, '\r'); break;
                case 't':  buf_append_char(&b, '\t'); break;
                case 'u': {
                    // Parse 4 hex digits, emit as raw byte if < 128
                    unsigned int cp = 0;
                    for (int i = 0; i < 4 && p->pos + 1 < p->len; i++) {
                        p->pos++;
                        char h = p->src[p->pos];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (h - 'A' + 10);
                    }
                    if (cp < 0x80) {
                        buf_append_char(&b, (char)cp);
                    } else if (cp < 0x800) {
                        buf_append_char(&b, (char)(0xC0 | (cp >> 6)));
                        buf_append_char(&b, (char)(0x80 | (cp & 0x3F)));
                    } else {
                        buf_append_char(&b, (char)(0xE0 | (cp >> 12)));
                        buf_append_char(&b, (char)(0x80 | ((cp >> 6) & 0x3F)));
                        buf_append_char(&b, (char)(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: buf_append_char(&b, p->src[p->pos]); break;
            }
        } else {
            buf_append_char(&b, p->src[p->pos]);
        }
        p->pos++;
    }
    if (p->pos < p->len) p->pos++;  // skip closing quote
    buf_append_char(&b, '\0');
    return b.data;
}

static JsonValue* parse_string(Parser* p) {
    char* s = parse_string_raw(p);
    if (!s) return nullptr;
    JsonValue* v = json_alloc(JSON_STRING);
    v->str_val = s;
    return v;
}

static JsonValue* parse_number(Parser* p) {
    skip_ws(p);
    int64_t start = p->pos;
    bool is_float = false;

    if (p->pos < p->len && p->src[p->pos] == '-') p->pos++;
    while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') p->pos++;
    if (p->pos < p->len && p->src[p->pos] == '.') {
        is_float = true;
        p->pos++;
        while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') p->pos++;
    }
    if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) {
        is_float = true;
        p->pos++;
        if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) p->pos++;
        while (p->pos < p->len && p->src[p->pos] >= '0' && p->src[p->pos] <= '9') p->pos++;
    }

    int64_t numlen = p->pos - start;
    char* tmp = (char*)malloc(numlen + 1);
    if (!tmp) std::abort();
    memcpy(tmp, p->src + start, numlen);
    tmp[numlen] = '\0';

    JsonValue* v;
    if (is_float) {
        v = json_alloc(JSON_FLOAT);
        v->float_val = strtod(tmp, nullptr);
    } else {
        v = json_alloc(JSON_INT);
        v->int_val = strtoll(tmp, nullptr, 10);
    }
    free(tmp);
    return v;
}

static bool match_literal(Parser* p, const char* lit) {
    int64_t n = (int64_t)strlen(lit);
    if (p->pos + n > p->len) return false;
    if (memcmp(p->src + p->pos, lit, n) != 0) return false;
    p->pos += n;
    return true;
}

static JsonValue* parse_value(Parser* p) {
    skip_ws(p);
    if (p->pos >= p->len) return nullptr;

    char c = p->src[p->pos];
    if (c == '"') return parse_string(p);
    if (c == '{') {
        p->pos++;
        JsonValue* obj = json_alloc(JSON_OBJECT);
        if (peek(p) != '}') {
            for (;;) {
                char* key = parse_string_raw(p);
                if (!key) break;
                if (!expect(p, ':')) { free(key); break; }
                JsonValue* val = parse_value(p);
                obj_set(obj, key, val);
                free(key);
                if (!expect(p, ',')) break;
            }
        }
        expect(p, '}');
        return obj;
    }
    if (c == '[') {
        p->pos++;
        JsonValue* arr = json_alloc(JSON_ARRAY);
        if (peek(p) != ']') {
            for (;;) {
                JsonValue* val = parse_value(p);
                if (val) arr_push(arr, val);
                if (!expect(p, ',')) break;
            }
        }
        expect(p, ']');
        return arr;
    }
    if (c == 't') { match_literal(p, "true");  JsonValue* v = json_alloc(JSON_BOOL); v->bool_val = 1; return v; }
    if (c == 'f') { match_literal(p, "false"); JsonValue* v = json_alloc(JSON_BOOL); v->bool_val = 0; return v; }
    if (c == 'n') { match_literal(p, "null");  return json_alloc(JSON_NULL); }
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(p);

    return nullptr;  // parse error
}

// ═══════════════════════════════════════════════════════════════════════
// Public C API (extern "C")
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Constructors ─────────────────────────────────────────────────────

void* json_new_object() {
    return (void*)json_alloc(JSON_OBJECT);
}

void* json_new_array() {
    return (void*)json_alloc(JSON_ARRAY);
}

// ── Object setters ───────────────────────────────────────────────────

void json_obj_set_str(void* handle, const char* key, const char* val) {
    if (!handle || !key) return;
    JsonValue* v = json_alloc(JSON_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    obj_set((JsonValue*)handle, key, v);
}

void json_obj_set_int(void* handle, const char* key, int64_t val) {
    if (!handle || !key) return;
    JsonValue* v = json_alloc(JSON_INT);
    v->int_val = val;
    obj_set((JsonValue*)handle, key, v);
}

void json_obj_set_flt(void* handle, const char* key, double val) {
    if (!handle || !key) return;
    JsonValue* v = json_alloc(JSON_FLOAT);
    v->float_val = val;
    obj_set((JsonValue*)handle, key, v);
}

void json_obj_set_bool(void* handle, const char* key, int32_t val) {
    if (!handle || !key) return;
    JsonValue* v = json_alloc(JSON_BOOL);
    v->bool_val = val ? 1 : 0;
    obj_set((JsonValue*)handle, key, v);
}

void json_obj_set_null(void* handle, const char* key) {
    if (!handle || !key) return;
    obj_set((JsonValue*)handle, key, json_alloc(JSON_NULL));
}

void json_obj_set_obj(void* handle, const char* key, void* child) {
    if (!handle || !key || !child) return;
    obj_set((JsonValue*)handle, key, json_value_clone((JsonValue*)child));
}

void json_obj_set_arr(void* handle, const char* key, void* child) {
    if (!handle || !key || !child) return;
    obj_set((JsonValue*)handle, key, json_value_clone((JsonValue*)child));
}

// ── Array pushers ────────────────────────────────────────────────────

void json_arr_push_str(void* handle, const char* val) {
    if (!handle) return;
    JsonValue* v = json_alloc(JSON_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    arr_push((JsonValue*)handle, v);
}

void json_arr_push_int(void* handle, int64_t val) {
    if (!handle) return;
    JsonValue* v = json_alloc(JSON_INT);
    v->int_val = val;
    arr_push((JsonValue*)handle, v);
}

void json_arr_push_flt(void* handle, double val) {
    if (!handle) return;
    JsonValue* v = json_alloc(JSON_FLOAT);
    v->float_val = val;
    arr_push((JsonValue*)handle, v);
}

void json_arr_push_bool(void* handle, int32_t val) {
    if (!handle) return;
    JsonValue* v = json_alloc(JSON_BOOL);
    v->bool_val = val ? 1 : 0;
    arr_push((JsonValue*)handle, v);
}

void json_arr_push_null(void* handle) {
    if (!handle) return;
    arr_push((JsonValue*)handle, json_alloc(JSON_NULL));
}

void json_arr_push_obj(void* handle, void* child) {
    if (!handle || !child) return;
    arr_push((JsonValue*)handle, json_value_clone((JsonValue*)child));
}

void json_arr_push_arr(void* handle, void* child) {
    if (!handle || !child) return;
    arr_push((JsonValue*)handle, json_value_clone((JsonValue*)child));
}

// ── Encoder ──────────────────────────────────────────────────────────

AriaString* json_encode(void* handle) {
    if (!handle) return make_aria_string("{}", 2);
    DynBuf b;
    buf_init(&b);
    encode_value(&b, (JsonValue*)handle);
    AriaString* result = make_aria_string(b.data, b.len);
    free(b.data);
    return result;
}

// ── Parser ───────────────────────────────────────────────────────────

void* json_parse(const char* json_str) {
    if (!json_str) return (void*)json_alloc(JSON_NULL);
    Parser p;
    p.src = json_str;
    p.pos = 0;
    p.len = (int64_t)strlen(json_str);
    JsonValue* v = parse_value(&p);
    return v ? (void*)v : (void*)json_alloc(JSON_NULL);
}

// ── Type query ───────────────────────────────────────────────────────

int32_t json_type(void* handle) {
    if (!handle) return JSON_NULL;
    return ((JsonValue*)handle)->type;
}

// ── Object getters ───────────────────────────────────────────────────

static JsonValue* obj_find(JsonValue* obj, const char* key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return nullptr;
    for (int64_t i = 0; i < obj->obj_len; i++) {
        if (strcmp(obj->obj_keys[i], key) == 0)
            return obj->obj_vals[i];
    }
    return nullptr;
}

AriaString* json_obj_get_str(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v || v->type != JSON_STRING || !v->str_val)
        return make_aria_string("", 0);
    return make_aria_string(v->str_val, (int64_t)strlen(v->str_val));
}

int64_t json_obj_get_int(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v) return 0;
    if (v->type == JSON_INT) return v->int_val;
    if (v->type == JSON_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double json_obj_get_flt(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v) return 0.0;
    if (v->type == JSON_FLOAT) return v->float_val;
    if (v->type == JSON_INT) return (double)v->int_val;
    return 0.0;
}

int32_t json_obj_get_bool(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v || v->type != JSON_BOOL) return 0;
    return v->bool_val;
}

void* json_obj_get_obj(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v || v->type != JSON_OBJECT) return (void*)json_alloc(JSON_OBJECT);
    return (void*)json_value_clone(v);
}

void* json_obj_get_arr(void* handle, const char* key) {
    JsonValue* v = obj_find((JsonValue*)handle, key);
    if (!v || v->type != JSON_ARRAY) return (void*)json_alloc(JSON_ARRAY);
    return (void*)json_value_clone(v);
}

int32_t json_obj_has(void* handle, const char* key) {
    return obj_find((JsonValue*)handle, key) ? 1 : 0;
}

int64_t json_obj_count(void* handle) {
    if (!handle || ((JsonValue*)handle)->type != JSON_OBJECT) return 0;
    return ((JsonValue*)handle)->obj_len;
}

// ── Array getters ────────────────────────────────────────────────────

int64_t json_arr_len(void* handle) {
    if (!handle || ((JsonValue*)handle)->type != JSON_ARRAY) return 0;
    return ((JsonValue*)handle)->arr_len;
}

static JsonValue* arr_at(JsonValue* arr, int64_t idx) {
    if (!arr || arr->type != JSON_ARRAY || idx < 0 || idx >= arr->arr_len)
        return nullptr;
    return arr->arr_items[idx];
}

AriaString* json_arr_get_str(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v || v->type != JSON_STRING || !v->str_val)
        return make_aria_string("", 0);
    return make_aria_string(v->str_val, (int64_t)strlen(v->str_val));
}

int64_t json_arr_get_int(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v) return 0;
    if (v->type == JSON_INT) return v->int_val;
    if (v->type == JSON_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double json_arr_get_flt(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v) return 0.0;
    if (v->type == JSON_FLOAT) return v->float_val;
    if (v->type == JSON_INT) return (double)v->int_val;
    return 0.0;
}

int32_t json_arr_get_bool(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v || v->type != JSON_BOOL) return 0;
    return v->bool_val;
}

void* json_arr_get_obj(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v || v->type != JSON_OBJECT) return (void*)json_alloc(JSON_OBJECT);
    return (void*)json_value_clone(v);
}

void* json_arr_get_arr(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v || v->type != JSON_ARRAY) return (void*)json_alloc(JSON_ARRAY);
    return (void*)json_value_clone(v);
}

int32_t json_arr_get_type(void* handle, int64_t idx) {
    JsonValue* v = arr_at((JsonValue*)handle, idx);
    if (!v) return JSON_NULL;
    return v->type;
}

// ── Cleanup ──────────────────────────────────────────────────────────

void json_free(void* handle) {
    json_value_free((JsonValue*)handle);
}

} // extern "C"
