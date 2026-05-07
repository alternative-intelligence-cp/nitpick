/**
 * TOML serialization/deserialization runtime for Aria stdlib.
 *
 * Supports TOML v1.0 subset:
 *  - String, integer, float, boolean values
 *  - Tables (sections via [table])
 *  - Arrays of values
 *  - Nested tables via [table.subtable]
 *  - Inline tables and arrays
 *
 * API mirrors JSON module:
 *  - Builder: toml_new_table, toml_set_*, toml_new_array, toml_arr_push_*
 *  - Encoder: toml_encode(handle) → string
 *  - Parser:  toml_parse(toml_str) → handle
 *  - Reader:  toml_get_*, toml_has, toml_count, toml_arr_len, toml_arr_get_*
 *  - Cleanup: toml_free(handle)
 *
 * ABI: string params as const char*, string returns as AriaString*.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════
// Internal TOML value tree (reuses similar structure to JSON)
// ═══════════════════════════════════════════════════════════════════════

enum TomlType : int32_t {
    TOML_STRING = 0,
    TOML_INT    = 1,
    TOML_FLOAT  = 2,
    TOML_BOOL   = 3,
    TOML_TABLE  = 4,
    TOML_ARRAY  = 5
};

struct TomlValue {
    TomlType type;
    union {
        char*    str_val;
        int64_t  int_val;
        double   float_val;
        int32_t  bool_val;
    };
    // For tables: key/value pairs
    char**      tbl_keys;
    TomlValue** tbl_vals;
    int64_t     tbl_len;
    int64_t     tbl_cap;
    // For arrays
    TomlValue** arr_items;
    int64_t     arr_len;
    int64_t     arr_cap;
};

static TomlValue* toml_alloc(TomlType t) {
    TomlValue* v = (TomlValue*)calloc(1, sizeof(TomlValue));
    if (!v) std::abort();
    v->type = t;
    return v;
}

static void toml_value_free(TomlValue* v) {
    if (!v) return;
    if (v->type == TOML_STRING) {
        free(v->str_val);
    } else if (v->type == TOML_TABLE) {
        for (int64_t i = 0; i < v->tbl_len; i++) {
            free(v->tbl_keys[i]);
            toml_value_free(v->tbl_vals[i]);
        }
        free(v->tbl_keys);
        free(v->tbl_vals);
    } else if (v->type == TOML_ARRAY) {
        for (int64_t i = 0; i < v->arr_len; i++)
            toml_value_free(v->arr_items[i]);
        free(v->arr_items);
    }
    free(v);
}

static TomlValue* toml_value_clone(TomlValue* v) {
    if (!v) return nullptr;
    TomlValue* c = toml_alloc(v->type);
    switch (v->type) {
        case TOML_STRING: c->str_val = v->str_val ? strdup(v->str_val) : nullptr; break;
        case TOML_INT:    c->int_val = v->int_val; break;
        case TOML_FLOAT:  c->float_val = v->float_val; break;
        case TOML_BOOL:   c->bool_val = v->bool_val; break;
        case TOML_TABLE:
            if (v->tbl_len > 0) {
                c->tbl_cap = v->tbl_len;
                c->tbl_keys = (char**)malloc(c->tbl_cap * sizeof(char*));
                c->tbl_vals = (TomlValue**)malloc(c->tbl_cap * sizeof(TomlValue*));
                if (!c->tbl_keys || !c->tbl_vals) std::abort();
                for (int64_t i = 0; i < v->tbl_len; i++) {
                    c->tbl_keys[i] = strdup(v->tbl_keys[i]);
                    c->tbl_vals[i] = toml_value_clone(v->tbl_vals[i]);
                }
                c->tbl_len = v->tbl_len;
            }
            break;
        case TOML_ARRAY:
            if (v->arr_len > 0) {
                c->arr_cap = v->arr_len;
                c->arr_items = (TomlValue**)malloc(c->arr_cap * sizeof(TomlValue*));
                if (!c->arr_items) std::abort();
                for (int64_t i = 0; i < v->arr_len; i++)
                    c->arr_items[i] = toml_value_clone(v->arr_items[i]);
                c->arr_len = v->arr_len;
            }
            break;
    }
    return c;
}

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
// Dynamic buffer
// ═══════════════════════════════════════════════════════════════════════

struct DynBuf {
    char*   data;
    int64_t len;
    int64_t cap;
};

static void buf_init(DynBuf* b) {
    b->cap = 256; b->len = 0;
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

static void buf_char(DynBuf* b, char c) {
    buf_ensure(b, 1); b->data[b->len++] = c;
}

static void buf_str(DynBuf* b, const char* s) {
    int64_t n = (int64_t)strlen(s);
    buf_ensure(b, n); memcpy(b->data + b->len, s, n); b->len += n;
}

// ═══════════════════════════════════════════════════════════════════════
// Table / Array helpers
// ═══════════════════════════════════════════════════════════════════════

static void tbl_grow(TomlValue* t) {
    if (t->tbl_len >= t->tbl_cap) {
        int64_t nc = t->tbl_cap < 4 ? 4 : t->tbl_cap * 2;
        t->tbl_keys = (char**)realloc(t->tbl_keys, nc * sizeof(char*));
        t->tbl_vals = (TomlValue**)realloc(t->tbl_vals, nc * sizeof(TomlValue*));
        if (!t->tbl_keys || !t->tbl_vals) std::abort();
        t->tbl_cap = nc;
    }
}

static void arr_grow(TomlValue* a) {
    if (a->arr_len >= a->arr_cap) {
        int64_t nc = a->arr_cap < 4 ? 4 : a->arr_cap * 2;
        a->arr_items = (TomlValue**)realloc(a->arr_items, nc * sizeof(TomlValue*));
        if (!a->arr_items) std::abort();
        a->arr_cap = nc;
    }
}

static void tbl_set(TomlValue* t, const char* key, TomlValue* val) {
    if (!t || t->type != TOML_TABLE || !key) return;
    for (int64_t i = 0; i < t->tbl_len; i++) {
        if (strcmp(t->tbl_keys[i], key) == 0) {
            toml_value_free(t->tbl_vals[i]);
            t->tbl_vals[i] = val;
            return;
        }
    }
    tbl_grow(t);
    t->tbl_keys[t->tbl_len] = strdup(key);
    t->tbl_vals[t->tbl_len] = val;
    t->tbl_len++;
}

static void arr_push(TomlValue* a, TomlValue* val) {
    if (!a || a->type != TOML_ARRAY) return;
    arr_grow(a);
    a->arr_items[a->arr_len++] = val;
}

static TomlValue* tbl_find(TomlValue* t, const char* key) {
    if (!t || t->type != TOML_TABLE || !key) return nullptr;
    for (int64_t i = 0; i < t->tbl_len; i++)
        if (strcmp(t->tbl_keys[i], key) == 0) return t->tbl_vals[i];
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// Encoder (TOML format output)
// ═══════════════════════════════════════════════════════════════════════

// Check if a key needs quoting
static bool needs_quote(const char* key) {
    if (!key || !*key) return true;
    for (const char* p = key; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return true;
    }
    return false;
}

static void encode_key(DynBuf* b, const char* key) {
    if (needs_quote(key)) {
        buf_char(b, '"');
        for (const char* p = key; *p; p++) {
            if (*p == '"') buf_str(b, "\\\"");
            else if (*p == '\\') buf_str(b, "\\\\");
            else buf_char(b, *p);
        }
        buf_char(b, '"');
    } else {
        buf_str(b, key);
    }
}

static void encode_inline_value(DynBuf* b, TomlValue* v);

static void encode_inline_value(DynBuf* b, TomlValue* v) {
    if (!v) return;
    switch (v->type) {
        case TOML_STRING:
            buf_char(b, '"');
            if (v->str_val) {
                for (const char* p = v->str_val; *p; p++) {
                    switch (*p) {
                        case '"':  buf_str(b, "\\\""); break;
                        case '\\': buf_str(b, "\\\\"); break;
                        case '\n': buf_str(b, "\\n");  break;
                        case '\t': buf_str(b, "\\t");  break;
                        case '\r': buf_str(b, "\\r");  break;
                        default:   buf_char(b, *p);
                    }
                }
            }
            buf_char(b, '"');
            break;
        case TOML_INT: {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%ld", (long)v->int_val);
            buf_str(b, tmp);
            break;
        }
        case TOML_FLOAT: {
            char tmp[64];
            if (std::isnan(v->float_val)) buf_str(b, "nan");
            else if (std::isinf(v->float_val))
                buf_str(b, v->float_val > 0 ? "inf" : "-inf");
            else {
                snprintf(tmp, sizeof(tmp), "%.17g", v->float_val);
                // Ensure there's a decimal point for TOML float
                bool has_dot = false;
                for (char* c = tmp; *c; c++) if (*c == '.' || *c == 'e' || *c == 'E') has_dot = true;
                buf_str(b, tmp);
                if (!has_dot) buf_str(b, ".0");
            }
            break;
        }
        case TOML_BOOL:
            buf_str(b, v->bool_val ? "true" : "false");
            break;
        case TOML_TABLE:
            buf_char(b, '{');
            for (int64_t i = 0; i < v->tbl_len; i++) {
                if (i > 0) buf_str(b, ", ");
                encode_key(b, v->tbl_keys[i]);
                buf_str(b, " = ");
                encode_inline_value(b, v->tbl_vals[i]);
            }
            buf_char(b, '}');
            break;
        case TOML_ARRAY:
            buf_char(b, '[');
            for (int64_t i = 0; i < v->arr_len; i++) {
                if (i > 0) buf_str(b, ", ");
                encode_inline_value(b, v->arr_items[i]);
            }
            buf_char(b, ']');
            break;
    }
}

// Encode a full TOML document with proper [section] headers
static void encode_table(DynBuf* b, TomlValue* t, const char* prefix) {
    if (!t || t->type != TOML_TABLE) return;

    // First pass: emit non-table, non-array-of-tables values
    for (int64_t i = 0; i < t->tbl_len; i++) {
        TomlValue* v = t->tbl_vals[i];
        if (v->type == TOML_TABLE) continue;
        if (v->type == TOML_ARRAY && v->arr_len > 0 && v->arr_items[0]->type == TOML_TABLE) continue;
        encode_key(b, t->tbl_keys[i]);
        buf_str(b, " = ");
        encode_inline_value(b, v);
        buf_char(b, '\n');
    }

    // Second pass: emit sub-tables with [prefix.key] headers
    for (int64_t i = 0; i < t->tbl_len; i++) {
        TomlValue* v = t->tbl_vals[i];
        if (v->type != TOML_TABLE) continue;

        // Build the full key path
        char fullkey[1024];
        if (prefix && prefix[0]) {
            snprintf(fullkey, sizeof(fullkey), "%s.%s", prefix, t->tbl_keys[i]);
        } else {
            snprintf(fullkey, sizeof(fullkey), "%s", t->tbl_keys[i]);
        }
        buf_char(b, '\n');
        buf_char(b, '[');
        buf_str(b, fullkey);
        buf_str(b, "]\n");
        encode_table(b, v, fullkey);
    }

    // Third pass: emit arrays of tables with [[prefix.key]]
    for (int64_t i = 0; i < t->tbl_len; i++) {
        TomlValue* v = t->tbl_vals[i];
        if (v->type != TOML_ARRAY || v->arr_len == 0 || v->arr_items[0]->type != TOML_TABLE) continue;

        char fullkey[1024];
        if (prefix && prefix[0]) {
            snprintf(fullkey, sizeof(fullkey), "%s.%s", prefix, t->tbl_keys[i]);
        } else {
            snprintf(fullkey, sizeof(fullkey), "%s", t->tbl_keys[i]);
        }
        for (int64_t j = 0; j < v->arr_len; j++) {
            buf_char(b, '\n');
            buf_str(b, "[[");
            buf_str(b, fullkey);
            buf_str(b, "]]\n");
            encode_table(b, v->arr_items[j], fullkey);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Parser
// ═══════════════════════════════════════════════════════════════════════

struct TParser {
    const char* src;
    int64_t pos;
    int64_t len;
};

static void tp_skip_ws(TParser* p) {
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t') p->pos++;
        else break;
    }
}

static void tp_skip_line(TParser* p) {
    while (p->pos < p->len && p->src[p->pos] != '\n') p->pos++;
    if (p->pos < p->len) p->pos++; // skip \n
}

static void tp_skip_ws_and_newlines(TParser* p) {
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { p->pos++; continue; }
        if (c == '#') { tp_skip_line(p); continue; }
        break;
    }
}

// Parse a bare key: [a-zA-Z0-9_-]+
static char* tp_parse_bare_key(TParser* p) {
    int64_t start = p->pos;
    while (p->pos < p->len) {
        char c = p->src[p->pos];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            p->pos++;
        else break;
    }
    if (p->pos == start) return nullptr;
    int64_t klen = p->pos - start;
    char* k = (char*)malloc(klen + 1);
    memcpy(k, p->src + start, klen);
    k[klen] = '\0';
    return k;
}

static char* tp_parse_quoted_key(TParser* p) {
    if (p->pos >= p->len || p->src[p->pos] != '"') return nullptr;
    p->pos++; // skip "
    DynBuf b; buf_init(&b);
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos < p->len) {
                switch (p->src[p->pos]) {
                    case '"':  buf_char(&b, '"'); break;
                    case '\\': buf_char(&b, '\\'); break;
                    case 'n':  buf_char(&b, '\n'); break;
                    case 't':  buf_char(&b, '\t'); break;
                    case 'r':  buf_char(&b, '\r'); break;
                    default:   buf_char(&b, p->src[p->pos]);
                }
            }
        } else {
            buf_char(&b, p->src[p->pos]);
        }
        p->pos++;
    }
    if (p->pos < p->len) p->pos++; // skip closing "
    buf_char(&b, '\0');
    return b.data;
}

static char* tp_parse_key(TParser* p) {
    tp_skip_ws(p);
    if (p->pos < p->len && p->src[p->pos] == '"')
        return tp_parse_quoted_key(p);
    return tp_parse_bare_key(p);
}

static TomlValue* tp_parse_value(TParser* p);

static TomlValue* tp_parse_string(TParser* p) {
    if (p->pos >= p->len || p->src[p->pos] != '"') return nullptr;
    p->pos++;
    DynBuf b; buf_init(&b);
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos < p->len) {
                switch (p->src[p->pos]) {
                    case '"':  buf_char(&b, '"'); break;
                    case '\\': buf_char(&b, '\\'); break;
                    case 'n':  buf_char(&b, '\n'); break;
                    case 't':  buf_char(&b, '\t'); break;
                    case 'r':  buf_char(&b, '\r'); break;
                    default:   buf_char(&b, p->src[p->pos]);
                }
            }
        } else {
            buf_char(&b, p->src[p->pos]);
        }
        p->pos++;
    }
    if (p->pos < p->len) p->pos++;
    buf_char(&b, '\0');
    TomlValue* v = toml_alloc(TOML_STRING);
    v->str_val = b.data;
    return v;
}

static TomlValue* tp_parse_number(TParser* p) {
    int64_t start = p->pos;
    bool is_float = false;
    if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) p->pos++;
    // Check for special float values
    if (p->pos + 3 <= p->len && memcmp(p->src + p->pos, "inf", 3) == 0) {
        p->pos += 3;
        TomlValue* v = toml_alloc(TOML_FLOAT);
        v->float_val = (p->src[start] == '-') ? -INFINITY : INFINITY;
        return v;
    }
    if (p->pos + 3 <= p->len && memcmp(p->src + p->pos, "nan", 3) == 0) {
        p->pos += 3;
        TomlValue* v = toml_alloc(TOML_FLOAT);
        v->float_val = NAN;
        return v;
    }

    while (p->pos < p->len && ((p->src[p->pos] >= '0' && p->src[p->pos] <= '9') || p->src[p->pos] == '_'))
        p->pos++;
    if (p->pos < p->len && p->src[p->pos] == '.') {
        is_float = true; p->pos++;
        while (p->pos < p->len && ((p->src[p->pos] >= '0' && p->src[p->pos] <= '9') || p->src[p->pos] == '_'))
            p->pos++;
    }
    if (p->pos < p->len && (p->src[p->pos] == 'e' || p->src[p->pos] == 'E')) {
        is_float = true; p->pos++;
        if (p->pos < p->len && (p->src[p->pos] == '+' || p->src[p->pos] == '-')) p->pos++;
        while (p->pos < p->len && ((p->src[p->pos] >= '0' && p->src[p->pos] <= '9') || p->src[p->pos] == '_'))
            p->pos++;
    }

    int64_t nlen = p->pos - start;
    // Copy without underscores
    char* tmp = (char*)malloc(nlen + 1);
    int64_t j = 0;
    for (int64_t i = start; i < p->pos; i++)
        if (p->src[i] != '_') tmp[j++] = p->src[i];
    tmp[j] = '\0';

    TomlValue* v;
    if (is_float) {
        v = toml_alloc(TOML_FLOAT);
        v->float_val = strtod(tmp, nullptr);
    } else {
        v = toml_alloc(TOML_INT);
        v->int_val = strtoll(tmp, nullptr, 10);
    }
    free(tmp);
    return v;
}

static TomlValue* tp_parse_value(TParser* p) {
    tp_skip_ws(p);
    if (p->pos >= p->len) return nullptr;
    char c = p->src[p->pos];

    // String
    if (c == '"') return tp_parse_string(p);

    // Boolean
    if (c == 't' && p->pos + 4 <= p->len && memcmp(p->src + p->pos, "true", 4) == 0) {
        p->pos += 4;
        TomlValue* v = toml_alloc(TOML_BOOL);
        v->bool_val = 1;
        return v;
    }
    if (c == 'f' && p->pos + 5 <= p->len && memcmp(p->src + p->pos, "false", 5) == 0) {
        p->pos += 5;
        TomlValue* v = toml_alloc(TOML_BOOL);
        v->bool_val = 0;
        return v;
    }

    // Inline array
    if (c == '[') {
        p->pos++;
        TomlValue* arr = toml_alloc(TOML_ARRAY);
        tp_skip_ws_and_newlines(p);
        if (p->pos < p->len && p->src[p->pos] != ']') {
            for (;;) {
                tp_skip_ws_and_newlines(p);
                TomlValue* val = tp_parse_value(p);
                if (val) arr_push(arr, val);
                tp_skip_ws_and_newlines(p);
                if (p->pos < p->len && p->src[p->pos] == ',') { p->pos++; continue; }
                break;
            }
        }
        tp_skip_ws_and_newlines(p);
        if (p->pos < p->len && p->src[p->pos] == ']') p->pos++;
        return arr;
    }

    // Inline table
    if (c == '{') {
        p->pos++;
        TomlValue* tbl = toml_alloc(TOML_TABLE);
        tp_skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] != '}') {
            for (;;) {
                tp_skip_ws(p);
                char* key = tp_parse_key(p);
                if (!key) break;
                tp_skip_ws(p);
                if (p->pos < p->len && p->src[p->pos] == '=') p->pos++;
                tp_skip_ws(p);
                TomlValue* val = tp_parse_value(p);
                tbl_set(tbl, key, val);
                free(key);
                tp_skip_ws(p);
                if (p->pos < p->len && p->src[p->pos] == ',') { p->pos++; continue; }
                break;
            }
        }
        tp_skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == '}') p->pos++;
        return tbl;
    }

    // Number (or inf/nan)
    if ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == 'i' || c == 'n') {
        // Check for standalone inf/nan
        if (c == 'i' && p->pos + 3 <= p->len && memcmp(p->src + p->pos, "inf", 3) == 0) {
            p->pos += 3;
            TomlValue* v = toml_alloc(TOML_FLOAT);
            v->float_val = INFINITY;
            return v;
        }
        if (c == 'n' && p->pos + 3 <= p->len && memcmp(p->src + p->pos, "nan", 3) == 0) {
            p->pos += 3;
            TomlValue* v = toml_alloc(TOML_FLOAT);
            v->float_val = NAN;
            return v;
        }
        return tp_parse_number(p);
    }

    return nullptr;
}

// Parse a dotted key path like "a.b.c" and set value in root table
static void set_dotted(TomlValue* root, const char* path, TomlValue* val) {
    // Split path on '.'
    char* pcopy = strdup(path);
    char* parts[64];
    int nparts = 0;
    char* tok = strtok(pcopy, ".");
    while (tok && nparts < 64) { parts[nparts++] = tok; tok = strtok(nullptr, "."); }

    TomlValue* cur = root;
    for (int i = 0; i < nparts - 1; i++) {
        TomlValue* next = tbl_find(cur, parts[i]);
        if (!next || next->type != TOML_TABLE) {
            next = toml_alloc(TOML_TABLE);
            tbl_set(cur, parts[i], next);
        }
        cur = tbl_find(cur, parts[i]);
    }
    if (nparts > 0) tbl_set(cur, parts[nparts - 1], val);
    free(pcopy);
}

// Parse a full TOML document
static TomlValue* tp_parse_document(TParser* p) {
    TomlValue* root = toml_alloc(TOML_TABLE);
    char current_section[1024] = "";

    while (p->pos < p->len) {
        tp_skip_ws_and_newlines(p);
        if (p->pos >= p->len) break;

        char c = p->src[p->pos];

        // Comment
        if (c == '#') { tp_skip_line(p); continue; }

        // Array of tables [[key]]
        if (c == '[' && p->pos + 1 < p->len && p->src[p->pos + 1] == '[') {
            p->pos += 2;
            tp_skip_ws(p);
            // Collect key path
            DynBuf kb; buf_init(&kb);
            while (p->pos < p->len && p->src[p->pos] != ']') {
                buf_char(&kb, p->src[p->pos]); p->pos++;
            }
            if (p->pos < p->len) p->pos++; // first ]
            if (p->pos < p->len) p->pos++; // second ]
            buf_char(&kb, '\0');

            // Navigate/create array at path
            char* kcopy = strdup(kb.data);
            char* parts[64];
            int np = 0;
            char* tk = strtok(kcopy, ".");
            while (tk && np < 64) { parts[np++] = tk; tk = strtok(nullptr, "."); }

            TomlValue* cur = root;
            for (int i = 0; i < np - 1; i++) {
                TomlValue* next = tbl_find(cur, parts[i]);
                if (!next) { next = toml_alloc(TOML_TABLE); tbl_set(cur, parts[i], next); }
                cur = tbl_find(cur, parts[i]);
            }
            if (np > 0) {
                TomlValue* arr = tbl_find(cur, parts[np - 1]);
                if (!arr || arr->type != TOML_ARRAY) {
                    arr = toml_alloc(TOML_ARRAY);
                    tbl_set(cur, parts[np - 1], arr);
                    arr = tbl_find(cur, parts[np - 1]);
                }
                TomlValue* entry = toml_alloc(TOML_TABLE);
                arr_push(arr, entry);
                // Set current section to this array entry for subsequent key=val
                snprintf(current_section, sizeof(current_section), "%s", kb.data);
                // We need to track the last entry; store the path
            }
            free(kcopy);
            free(kb.data);

            // Now read key=value pairs into the last array entry
            while (p->pos < p->len) {
                tp_skip_ws_and_newlines(p);
                if (p->pos >= p->len) break;
                c = p->src[p->pos];
                if (c == '[') break;
                if (c == '#') { tp_skip_line(p); continue; }
                if (c == '\n') { p->pos++; continue; }
                char* key = tp_parse_key(p);
                if (!key) break;
                tp_skip_ws(p);
                if (p->pos < p->len && p->src[p->pos] == '=') p->pos++;
                tp_skip_ws(p);
                TomlValue* val = tp_parse_value(p);
                tp_skip_ws(p);
                if (p->pos < p->len && p->src[p->pos] == '#') tp_skip_line(p);

                // Find the last entry in the array
                // Re-navigate to the array
                TomlValue* cur2 = root;
                char* kcopy2 = strndup(current_section, strlen(current_section));
                char* parts2[64]; int np2 = 0;
                char* tk2 = strtok(kcopy2, ".");
                while (tk2 && np2 < 64) { parts2[np2++] = tk2; tk2 = strtok(nullptr, "."); }
                for (int i = 0; i < np2 - 1; i++) {
                    TomlValue* n = tbl_find(cur2, parts2[i]);
                    if (n) cur2 = n;
                }
                if (np2 > 0) {
                    TomlValue* a = tbl_find(cur2, parts2[np2 - 1]);
                    if (a && a->type == TOML_ARRAY && a->arr_len > 0) {
                        tbl_set(a->arr_items[a->arr_len - 1], key, val);
                    }
                }
                free(kcopy2);
                free(key);
            }
            continue;
        }

        // Table header [key]
        if (c == '[') {
            p->pos++;
            tp_skip_ws(p);
            DynBuf kb; buf_init(&kb);
            while (p->pos < p->len && p->src[p->pos] != ']') {
                buf_char(&kb, p->src[p->pos]); p->pos++;
            }
            if (p->pos < p->len) p->pos++; // skip ]
            buf_char(&kb, '\0');
            snprintf(current_section, sizeof(current_section), "%s", kb.data);

            // Ensure the path exists
            char* kcopy = strdup(kb.data);
            char* parts[64]; int np = 0;
            char* tk = strtok(kcopy, ".");
            while (tk && np < 64) { parts[np++] = tk; tk = strtok(nullptr, "."); }
            TomlValue* cur = root;
            for (int i = 0; i < np; i++) {
                TomlValue* next = tbl_find(cur, parts[i]);
                if (!next) { next = toml_alloc(TOML_TABLE); tbl_set(cur, parts[i], next); next = tbl_find(cur, parts[i]); }
                cur = next;
            }
            free(kcopy);
            free(kb.data);
            continue;
        }

        // Key = Value
        char* key = tp_parse_key(p);
        if (!key) { tp_skip_line(p); continue; }
        tp_skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == '=') p->pos++;
        tp_skip_ws(p);
        TomlValue* val = tp_parse_value(p);
        tp_skip_ws(p);
        if (p->pos < p->len && p->src[p->pos] == '#') tp_skip_line(p);

        if (current_section[0]) {
            char fullpath[2048];
            snprintf(fullpath, sizeof(fullpath), "%s.%s", current_section, key);
            set_dotted(root, fullpath, val);
        } else {
            tbl_set(root, key, val);
        }
        free(key);
    }
    return root;
}

// ═══════════════════════════════════════════════════════════════════════
// Public C API
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Constructors ─────────────────────────────────────────────────────

void* toml_new_table() { return (void*)toml_alloc(TOML_TABLE); }
void* toml_new_array() { return (void*)toml_alloc(TOML_ARRAY); }

// ── Table setters ────────────────────────────────────────────────────

void toml_set_str(void* h, const char* key, const char* val) {
    if (!h || !key) return;
    TomlValue* v = toml_alloc(TOML_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    tbl_set((TomlValue*)h, key, v);
}

void toml_set_int(void* h, const char* key, int64_t val) {
    if (!h || !key) return;
    TomlValue* v = toml_alloc(TOML_INT);
    v->int_val = val;
    tbl_set((TomlValue*)h, key, v);
}

void toml_set_flt(void* h, const char* key, double val) {
    if (!h || !key) return;
    TomlValue* v = toml_alloc(TOML_FLOAT);
    v->float_val = val;
    tbl_set((TomlValue*)h, key, v);
}

void toml_set_bool(void* h, const char* key, int32_t val) {
    if (!h || !key) return;
    TomlValue* v = toml_alloc(TOML_BOOL);
    v->bool_val = val ? 1 : 0;
    tbl_set((TomlValue*)h, key, v);
}

void toml_set_tbl(void* h, const char* key, void* child) {
    if (!h || !key || !child) return;
    tbl_set((TomlValue*)h, key, toml_value_clone((TomlValue*)child));
}

void toml_set_arr(void* h, const char* key, void* child) {
    if (!h || !key || !child) return;
    tbl_set((TomlValue*)h, key, toml_value_clone((TomlValue*)child));
}

// ── Array pushers ────────────────────────────────────────────────────

void toml_arr_push_str(void* h, const char* val) {
    if (!h) return;
    TomlValue* v = toml_alloc(TOML_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    arr_push((TomlValue*)h, v);
}

void toml_arr_push_int(void* h, int64_t val) {
    if (!h) return;
    TomlValue* v = toml_alloc(TOML_INT); v->int_val = val;
    arr_push((TomlValue*)h, v);
}

void toml_arr_push_flt(void* h, double val) {
    if (!h) return;
    TomlValue* v = toml_alloc(TOML_FLOAT); v->float_val = val;
    arr_push((TomlValue*)h, v);
}

void toml_arr_push_bool(void* h, int32_t val) {
    if (!h) return;
    TomlValue* v = toml_alloc(TOML_BOOL); v->bool_val = val ? 1 : 0;
    arr_push((TomlValue*)h, v);
}

void toml_arr_push_tbl(void* h, void* child) {
    if (!h || !child) return;
    arr_push((TomlValue*)h, toml_value_clone((TomlValue*)child));
}

void toml_arr_push_arr(void* h, void* child) {
    if (!h || !child) return;
    arr_push((TomlValue*)h, toml_value_clone((TomlValue*)child));
}

// ── Encoder ──────────────────────────────────────────────────────────

AriaString* toml_encode(void* handle) {
    if (!handle) return make_aria_string("", 0);
    DynBuf b; buf_init(&b);
    encode_table(&b, (TomlValue*)handle, "");
    AriaString* result = make_aria_string(b.data, b.len);
    free(b.data);
    return result;
}

// ── Parser ───────────────────────────────────────────────────────────

void* toml_parse(const char* toml_str) {
    if (!toml_str) return (void*)toml_alloc(TOML_TABLE);
    TParser p;
    p.src = toml_str;
    p.pos = 0;
    p.len = (int64_t)strlen(toml_str);
    return (void*)tp_parse_document(&p);
}

// ── Type query ───────────────────────────────────────────────────────

int32_t toml_type(void* handle) {
    if (!handle) return TOML_TABLE;
    return ((TomlValue*)handle)->type;
}

// ── Table getters ────────────────────────────────────────────────────

int32_t toml_has(void* h, const char* key) {
    return tbl_find((TomlValue*)h, key) ? 1 : 0;
}

int64_t toml_count(void* h) {
    if (!h || ((TomlValue*)h)->type != TOML_TABLE) return 0;
    return ((TomlValue*)h)->tbl_len;
}

AriaString* toml_get_str(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v || v->type != TOML_STRING || !v->str_val) return make_aria_string("", 0);
    return make_aria_string(v->str_val, (int64_t)strlen(v->str_val));
}

int64_t toml_get_int(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v) return 0;
    if (v->type == TOML_INT) return v->int_val;
    if (v->type == TOML_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double toml_get_flt(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v) return 0.0;
    if (v->type == TOML_FLOAT) return v->float_val;
    if (v->type == TOML_INT) return (double)v->int_val;
    return 0.0;
}

int32_t toml_get_bool(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v || v->type != TOML_BOOL) return 0;
    return v->bool_val;
}

void* toml_get_tbl(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v || v->type != TOML_TABLE) return (void*)toml_alloc(TOML_TABLE);
    return (void*)toml_value_clone(v);
}

void* toml_get_arr(void* h, const char* key) {
    TomlValue* v = tbl_find((TomlValue*)h, key);
    if (!v || v->type != TOML_ARRAY) return (void*)toml_alloc(TOML_ARRAY);
    return (void*)toml_value_clone(v);
}

// ── Array getters ────────────────────────────────────────────────────

int64_t toml_arr_len(void* h) {
    if (!h || ((TomlValue*)h)->type != TOML_ARRAY) return 0;
    return ((TomlValue*)h)->arr_len;
}

static TomlValue* arr_at(TomlValue* a, int64_t idx) {
    if (!a || a->type != TOML_ARRAY || idx < 0 || idx >= a->arr_len) return nullptr;
    return a->arr_items[idx];
}

AriaString* toml_arr_get_str(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v || v->type != TOML_STRING || !v->str_val) return make_aria_string("", 0);
    return make_aria_string(v->str_val, (int64_t)strlen(v->str_val));
}

int64_t toml_arr_get_int(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v) return 0;
    if (v->type == TOML_INT) return v->int_val;
    if (v->type == TOML_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double toml_arr_get_flt(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v) return 0.0;
    if (v->type == TOML_FLOAT) return v->float_val;
    if (v->type == TOML_INT) return (double)v->int_val;
    return 0.0;
}

int32_t toml_arr_get_bool(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v || v->type != TOML_BOOL) return 0;
    return v->bool_val;
}

void* toml_arr_get_tbl(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v || v->type != TOML_TABLE) return (void*)toml_alloc(TOML_TABLE);
    return (void*)toml_value_clone(v);
}

void* toml_arr_get_arr(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v || v->type != TOML_ARRAY) return (void*)toml_alloc(TOML_ARRAY);
    return (void*)toml_value_clone(v);
}

int32_t toml_arr_get_type(void* h, int64_t idx) {
    TomlValue* v = arr_at((TomlValue*)h, idx);
    if (!v) return TOML_STRING;
    return v->type;
}

// ── Cleanup ──────────────────────────────────────────────────────────

void toml_free(void* h) { toml_value_free((TomlValue*)h); }

} // extern "C"
