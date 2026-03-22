/**
 * toml_helpers.cpp — C helper layer for the self-hosted TOML module.
 *
 * Provides value-tree operations, parser, and public API.
 * Encoder logic lives in stdlib/toml.aria.
 *
 * Naming: th_* = internal helpers, toml_* = public API (extern from Aria)
 *
 * TOML Type codes:
 *   0 = string, 1 = int, 2 = float, 3 = bool, 4 = table, 5 = array
 *
 * Aria string calling convention:
 *   - string PARAMETER in extern → const char* in C (null-terminated)
 *   - string RETURN in extern → AriaString* in C (data + length struct)
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cmath>

// ═══════════════════════════════════════════════════════════════════════
// TomlValue — tagged-union value node
// ═══════════════════════════════════════════════════════════════════════

enum ThType : int32_t {
    TH_STRING = 0, TH_INT = 1, TH_FLOAT = 2, TH_BOOL = 3,
    TH_TABLE  = 4, TH_ARRAY = 5
};

struct TomlValue {
    ThType type;
    union { char* str_val; int64_t int_val; double float_val; int32_t bool_val; };
    char**       tbl_keys;
    TomlValue**  tbl_vals;
    int64_t      tbl_len, tbl_cap;
    TomlValue**  arr_items;
    int64_t      arr_len, arr_cap;
};

static TomlValue* tv_alloc(ThType t) {
    TomlValue* v = (TomlValue*)calloc(1, sizeof(TomlValue));
    if (!v) std::abort();
    v->type = t;
    return v;
}

static void tv_free(TomlValue* v) {
    if (!v) return;
    if (v->type == TH_STRING) { free(v->str_val); }
    else if (v->type == TH_TABLE) {
        for (int64_t i = 0; i < v->tbl_len; i++) {
            free(v->tbl_keys[i]);
            tv_free(v->tbl_vals[i]);
        }
        free(v->tbl_keys); free(v->tbl_vals);
    } else if (v->type == TH_ARRAY) {
        for (int64_t i = 0; i < v->arr_len; i++) tv_free(v->arr_items[i]);
        free(v->arr_items);
    }
    free(v);
}

static TomlValue* tv_clone(TomlValue* v) {
    if (!v) return nullptr;
    TomlValue* c = tv_alloc(v->type);
    switch (v->type) {
        case TH_STRING: c->str_val = v->str_val ? strdup(v->str_val) : nullptr; break;
        case TH_INT:    c->int_val = v->int_val; break;
        case TH_FLOAT:  c->float_val = v->float_val; break;
        case TH_BOOL:   c->bool_val = v->bool_val; break;
        case TH_TABLE:
            if (v->tbl_len > 0) {
                c->tbl_cap = v->tbl_len;
                c->tbl_keys = (char**)malloc(c->tbl_cap * sizeof(char*));
                c->tbl_vals = (TomlValue**)malloc(c->tbl_cap * sizeof(TomlValue*));
                if (!c->tbl_keys || !c->tbl_vals) std::abort();
                for (int64_t i = 0; i < v->tbl_len; i++) {
                    c->tbl_keys[i] = strdup(v->tbl_keys[i]);
                    c->tbl_vals[i] = tv_clone(v->tbl_vals[i]);
                }
                c->tbl_len = v->tbl_len;
            }
            break;
        case TH_ARRAY:
            if (v->arr_len > 0) {
                c->arr_cap = v->arr_len;
                c->arr_items = (TomlValue**)malloc(c->arr_cap * sizeof(TomlValue*));
                if (!c->arr_items) std::abort();
                for (int64_t i = 0; i < v->arr_len; i++)
                    c->arr_items[i] = tv_clone(v->arr_items[i]);
                c->arr_len = v->arr_len;
            }
            break;
    }
    return c;
}

static AriaString* make_aria_str(const char* data, int64_t length) {
    char* buf = (char*)aria_gc_alloc(length + 1, 0);
    if (!buf) std::abort();
    if (length > 0) memcpy(buf, data, length);
    buf[length] = '\0';
    AriaString* s = (AriaString*)aria_gc_alloc(sizeof(AriaString), 0);
    if (!s) std::abort();
    s->data = buf;
    s->length = length;
    return s;
}

// ═══════════════════════════════════════════════════════════════════════
// DynBuf — growable character buffer
// ═══════════════════════════════════════════════════════════════════════

struct DynBuf {
    char* data; int64_t len, cap;
};

static void buf_ensure(DynBuf* b, int64_t extra) {
    while (b->len + extra >= b->cap) {
        b->cap *= 2;
        b->data = (char*)realloc(b->data, b->cap);
        if (!b->data) std::abort();
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Table / Array internal helpers
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

static void tbl_set(TomlValue* t, const char* key, TomlValue* child) {
    if (!t || t->type != TH_TABLE || !key) return;
    int64_t keylen = (int64_t)strlen(key);
    for (int64_t i = 0; i < t->tbl_len; i++) {
        if (t->tbl_keys[i] && keylen == (int64_t)strlen(t->tbl_keys[i]) &&
            memcmp(t->tbl_keys[i], key, keylen) == 0) {
            tv_free(t->tbl_vals[i]);
            t->tbl_vals[i] = child;
            return;
        }
    }
    tbl_grow(t);
    t->tbl_keys[t->tbl_len] = strdup(key);
    t->tbl_vals[t->tbl_len] = child;
    t->tbl_len++;
}

static TomlValue* tbl_find(TomlValue* t, const char* key) {
    if (!t || t->type != TH_TABLE || !key) return nullptr;
    int64_t keylen = (int64_t)strlen(key);
    for (int64_t i = 0; i < t->tbl_len; i++) {
        if (t->tbl_keys[i] && keylen == (int64_t)strlen(t->tbl_keys[i]) &&
            memcmp(t->tbl_keys[i], key, keylen) == 0)
            return t->tbl_vals[i];
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════════════════════
// TOML Parser (recursive descent, entirely in C)
// ═══════════════════════════════════════════════════════════════════════

static TomlValue* tp_parse_value(const char* src, int64_t len, int64_t& pos);

static void tp_skip_ws(const char* src, int64_t len, int64_t& pos) {
    while (pos < len && (src[pos] == ' ' || src[pos] == '\t'))
        pos++;
}

static void tp_skip_line(const char* src, int64_t len, int64_t& pos) {
    while (pos < len && src[pos] != '\n') pos++;
    if (pos < len) pos++;
}

static void tp_skip_ws_nl(const char* src, int64_t len, int64_t& pos) {
    while (pos < len) {
        char c = src[pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { pos++; }
        else if (c == '#') {
            while (pos < len && src[pos] != '\n') pos++;
            if (pos < len) pos++;
        } else break;
    }
}

static char* tp_parse_bare_key(const char* src, int64_t len, int64_t& pos) {
    int64_t start = pos;
    while (pos < len) {
        char c = src[pos];
        bool bk = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (bk) pos++; else break;
    }
    int64_t klen = pos - start;
    if (klen == 0) return strdup("");
    char* key = (char*)malloc(klen + 1);
    if (!key) std::abort();
    memcpy(key, src + start, klen);
    key[klen] = '\0';
    return key;
}

static char* tp_parse_quoted_key(const char* src, int64_t len, int64_t& pos) {
    if (pos >= len || src[pos] != '"') return strdup("");
    pos++;
    DynBuf b; b.cap = 64; b.len = 0;
    b.data = (char*)malloc(b.cap);
    if (!b.data) std::abort();
    while (pos < len && src[pos] != '"') {
        if (src[pos] == '\\' && pos + 1 < len) {
            pos++;
            switch (src[pos]) {
                case '"':  buf_ensure(&b, 1); b.data[b.len++] = '"'; break;
                case '\\': buf_ensure(&b, 1); b.data[b.len++] = '\\'; break;
                case 'n':  buf_ensure(&b, 1); b.data[b.len++] = '\n'; break;
                case 't':  buf_ensure(&b, 1); b.data[b.len++] = '\t'; break;
                case 'r':  buf_ensure(&b, 1); b.data[b.len++] = '\r'; break;
                default:   buf_ensure(&b, 1); b.data[b.len++] = src[pos]; break;
            }
            pos++;
        } else {
            buf_ensure(&b, 1);
            b.data[b.len++] = src[pos++];
        }
    }
    if (pos < len) pos++;
    char* result = (char*)malloc(b.len + 1);
    if (!result) std::abort();
    if (b.len > 0) memcpy(result, b.data, b.len);
    result[b.len] = '\0';
    free(b.data);
    return result;
}

static char* tp_parse_key(const char* src, int64_t len, int64_t& pos) {
    tp_skip_ws(src, len, pos);
    if (pos < len && src[pos] == '"')
        return tp_parse_quoted_key(src, len, pos);
    return tp_parse_bare_key(src, len, pos);
}

static TomlValue* tp_parse_string(const char* src, int64_t len, int64_t& pos) {
    if (pos >= len || src[pos] != '"') {
        TomlValue* v = tv_alloc(TH_STRING); v->str_val = strdup(""); return v;
    }
    pos++;
    DynBuf b; b.cap = 64; b.len = 0;
    b.data = (char*)malloc(b.cap);
    if (!b.data) std::abort();
    while (pos < len && src[pos] != '"') {
        if (src[pos] == '\\' && pos + 1 < len) {
            pos++;
            switch (src[pos]) {
                case '"':  buf_ensure(&b, 1); b.data[b.len++] = '"'; break;
                case '\\': buf_ensure(&b, 1); b.data[b.len++] = '\\'; break;
                case 'n':  buf_ensure(&b, 1); b.data[b.len++] = '\n'; break;
                case 't':  buf_ensure(&b, 1); b.data[b.len++] = '\t'; break;
                case 'r':  buf_ensure(&b, 1); b.data[b.len++] = '\r'; break;
                default:   buf_ensure(&b, 1); b.data[b.len++] = src[pos]; break;
            }
            pos++;
        } else {
            buf_ensure(&b, 1);
            b.data[b.len++] = src[pos++];
        }
    }
    if (pos < len) pos++;
    TomlValue* v = tv_alloc(TH_STRING);
    v->str_val = (char*)malloc(b.len + 1);
    if (!v->str_val) std::abort();
    if (b.len > 0) memcpy(v->str_val, b.data, b.len);
    v->str_val[b.len] = '\0';
    free(b.data);
    return v;
}

static TomlValue* tp_parse_number(const char* src, int64_t len, int64_t& pos) {
    int64_t start = pos;
    bool is_float = false;
    if (pos < len && (src[pos] == '+' || src[pos] == '-')) pos++;
    if (pos + 3 <= len) {
        if (memcmp(src + pos, "inf", 3) == 0) {
            pos += 3;
            TomlValue* v = tv_alloc(TH_FLOAT);
            v->float_val = (src[start] == '-') ? -INFINITY : INFINITY;
            return v;
        }
        if (memcmp(src + pos, "nan", 3) == 0) {
            pos += 3;
            TomlValue* v = tv_alloc(TH_FLOAT);
            v->float_val = NAN;
            return v;
        }
    }
    while (pos < len && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '_'))
        pos++;
    if (pos < len && src[pos] == '.') {
        is_float = true; pos++;
        while (pos < len && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '_'))
            pos++;
    }
    if (pos < len && (src[pos] == 'e' || src[pos] == 'E')) {
        is_float = true; pos++;
        if (pos < len && (src[pos] == '+' || src[pos] == '-')) pos++;
        while (pos < len && ((src[pos] >= '0' && src[pos] <= '9') || src[pos] == '_'))
            pos++;
    }
    char tmp[128]; int64_t j = 0;
    for (int64_t i = start; i < pos && i < len && j < 126; i++)
        if (src[i] != '_') tmp[j++] = src[i];
    tmp[j] = '\0';
    if (is_float) {
        TomlValue* v = tv_alloc(TH_FLOAT); v->float_val = strtod(tmp, nullptr); return v;
    }
    TomlValue* v = tv_alloc(TH_INT); v->int_val = strtoll(tmp, nullptr, 10); return v;
}

static TomlValue* tp_pv_arr(const char* src, int64_t len, int64_t& pos) {
    TomlValue* arr = tv_alloc(TH_ARRAY);
    tp_skip_ws_nl(src, len, pos);
    if (pos < len && src[pos] != ']') {
        bool more = true;
        while (more) {
            tp_skip_ws_nl(src, len, pos);
            TomlValue* val = tp_parse_value(src, len, pos);
            arr_grow(arr);
            arr->arr_items[arr->arr_len++] = val;
            tp_skip_ws_nl(src, len, pos);
            if (pos < len && src[pos] == ',') pos++;
            else more = false;
        }
    }
    tp_skip_ws_nl(src, len, pos);
    if (pos < len && src[pos] == ']') pos++;
    return arr;
}

static TomlValue* tp_pv_tbl(const char* src, int64_t len, int64_t& pos) {
    TomlValue* tbl = tv_alloc(TH_TABLE);
    tp_skip_ws(src, len, pos);
    if (pos < len && src[pos] != '}') {
        bool more = true;
        while (more) {
            tp_skip_ws(src, len, pos);
            char* key = tp_parse_key(src, len, pos);
            if (!key || key[0] == '\0') { free(key); more = false; continue; }
            tp_skip_ws(src, len, pos);
            if (pos < len && src[pos] == '=') pos++;
            tp_skip_ws(src, len, pos);
            TomlValue* val = tp_parse_value(src, len, pos);
            tbl_set(tbl, key, val);
            free(key);
            tp_skip_ws(src, len, pos);
            if (pos < len && src[pos] == ',') pos++;
            else more = false;
        }
    }
    tp_skip_ws(src, len, pos);
    if (pos < len && src[pos] == '}') pos++;
    return tbl;
}

static TomlValue* tp_parse_value(const char* src, int64_t len, int64_t& pos) {
    tp_skip_ws(src, len, pos);
    if (pos >= len) { TomlValue* v = tv_alloc(TH_STRING); v->str_val = strdup(""); return v; }
    char c = src[pos];
    if (c == '"') return tp_parse_string(src, len, pos);
    if (c == 't' && pos + 4 <= len && memcmp(src + pos, "true", 4) == 0) {
        pos += 4; TomlValue* v = tv_alloc(TH_BOOL); v->bool_val = 1; return v;
    }
    if (c == 'f' && pos + 5 <= len && memcmp(src + pos, "false", 5) == 0) {
        pos += 5; TomlValue* v = tv_alloc(TH_BOOL); v->bool_val = 0; return v;
    }
    if (c == '[') { pos++; return tp_pv_arr(src, len, pos); }
    if (c == '{') { pos++; return tp_pv_tbl(src, len, pos); }
    if (c == 'i' || c == 'n' || c == '+' || c == '-' || (c >= '0' && c <= '9'))
        return tp_parse_number(src, len, pos);
    TomlValue* v = tv_alloc(TH_STRING); v->str_val = strdup(""); return v;
}

static TomlValue* tp_ensure_path(TomlValue* root, const char* path) {
    if (!path) return root;
    int64_t plen = (int64_t)strlen(path);
    TomlValue* cur = root;
    int64_t seg_start = 0;
    for (int64_t i = 0; i <= plen; i++) {
        bool is_sep = (i == plen) || (path[i] == '.');
        if (is_sep && i > seg_start) {
            char* seg = (char*)malloc(i - seg_start + 1);
            if (!seg) std::abort();
            memcpy(seg, path + seg_start, i - seg_start);
            seg[i - seg_start] = '\0';
            TomlValue* next = tbl_find(cur, seg);
            if (!next) {
                tbl_set(cur, seg, tv_alloc(TH_TABLE));
                next = tbl_find(cur, seg);
            }
            cur = next;
            free(seg);
            seg_start = i + 1;
        }
    }
    return cur;
}

static void tp_set_dotted(TomlValue* root, const char* path, TomlValue* val) {
    if (!path) return;
    int64_t plen = (int64_t)strlen(path);
    int64_t last_dot = -1;
    for (int64_t i = 0; i < plen; i++)
        if (path[i] == '.') last_dot = i;
    if (last_dot < 0) {
        tbl_set(root, path, val);
    } else {
        char* pp = (char*)malloc(last_dot + 1);
        if (!pp) std::abort();
        memcpy(pp, path, last_dot);
        pp[last_dot] = '\0';
        TomlValue* parent = tp_ensure_path(root, pp);
        tbl_set(parent, path + last_dot + 1, val);
        free(pp);
    }
}

static char* tp_build_path(const char* prefix, const char* key) {
    if (!prefix || prefix[0] == '\0') return strdup(key);
    int64_t plen = (int64_t)strlen(prefix);
    int64_t klen = (int64_t)strlen(key);
    char* r = (char*)malloc(plen + 1 + klen + 1);
    if (!r) std::abort();
    memcpy(r, prefix, plen);
    r[plen] = '.';
    memcpy(r + plen + 1, key, klen);
    r[plen + 1 + klen] = '\0';
    return r;
}

static TomlValue* tp_parse_document(const char* src, int64_t len) {
    TomlValue* root = tv_alloc(TH_TABLE);
    char* cur_sect = strdup("");
    int64_t pos = 0;
    while (pos < len) {
        tp_skip_ws_nl(src, len, pos);
        if (pos >= len) break;
        char c = src[pos];
        if (c == '#') { tp_skip_line(src, len, pos); continue; }
        if (c == '[') {
            bool is_aot = (pos + 1 < len && src[pos + 1] == '[');
            if (is_aot) {
                pos += 2;
                tp_skip_ws(src, len, pos);
                DynBuf nb; nb.cap = 64; nb.len = 0;
                nb.data = (char*)malloc(nb.cap);
                if (!nb.data) std::abort();
                while (pos < len && src[pos] != ']') {
                    buf_ensure(&nb, 1); nb.data[nb.len++] = src[pos++];
                }
                if (pos < len) pos++;
                if (pos < len && src[pos] == ']') pos++;
                char* aot_path = (char*)malloc(nb.len + 1);
                if (!aot_path) std::abort();
                if (nb.len > 0) memcpy(aot_path, nb.data, nb.len);
                aot_path[nb.len] = '\0';
                free(nb.data);
                int64_t aplen = (int64_t)strlen(aot_path);
                int64_t aldot = -1;
                for (int64_t i = 0; i < aplen; i++)
                    if (aot_path[i] == '.') aldot = i;
                TomlValue* aparent = root;
                const char* arr_key = aot_path;
                char* pp = nullptr;
                if (aldot > 0) {
                    pp = (char*)malloc(aldot + 1);
                    if (!pp) std::abort();
                    memcpy(pp, aot_path, aldot);
                    pp[aldot] = '\0';
                    aparent = tp_ensure_path(root, pp);
                    arr_key = aot_path + aldot + 1;
                }
                TomlValue* exarr = tbl_find(aparent, arr_key);
                if (!exarr) {
                    tbl_set(aparent, arr_key, tv_alloc(TH_ARRAY));
                    exarr = tbl_find(aparent, arr_key);
                }
                TomlValue* aentry = tv_alloc(TH_TABLE);
                arr_grow(exarr);
                exarr->arr_items[exarr->arr_len++] = aentry;
                while (pos < len) {
                    tp_skip_ws_nl(src, len, pos);
                    if (pos >= len) break;
                    if (src[pos] == '[') break;
                    if (src[pos] == '#') { tp_skip_line(src, len, pos); continue; }
                    char* akey = tp_parse_key(src, len, pos);
                    if (!akey || akey[0] == '\0') { free(akey); tp_skip_line(src, len, pos); continue; }
                    tp_skip_ws(src, len, pos);
                    if (pos < len && src[pos] == '=') pos++;
                    tp_skip_ws(src, len, pos);
                    TomlValue* aval = tp_parse_value(src, len, pos);
                    tp_skip_ws(src, len, pos);
                    if (pos < len && src[pos] == '#') tp_skip_line(src, len, pos);
                    tbl_set(aentry, akey, aval);
                    free(akey);
                }
                free(pp);
                free(aot_path);
            } else {
                pos++;
                tp_skip_ws(src, len, pos);
                DynBuf sb; sb.cap = 64; sb.len = 0;
                sb.data = (char*)malloc(sb.cap);
                if (!sb.data) std::abort();
                while (pos < len && src[pos] != ']') {
                    buf_ensure(&sb, 1); sb.data[sb.len++] = src[pos++];
                }
                if (pos < len) pos++;
                free(cur_sect);
                cur_sect = (char*)malloc(sb.len + 1);
                if (!cur_sect) std::abort();
                if (sb.len > 0) memcpy(cur_sect, sb.data, sb.len);
                cur_sect[sb.len] = '\0';
                free(sb.data);
                tp_ensure_path(root, cur_sect);
            }
        } else {
            char* key = tp_parse_key(src, len, pos);
            if (!key || key[0] == '\0') { free(key); tp_skip_line(src, len, pos); continue; }
            tp_skip_ws(src, len, pos);
            if (pos < len && src[pos] == '=') pos++;
            tp_skip_ws(src, len, pos);
            TomlValue* val = tp_parse_value(src, len, pos);
            tp_skip_ws(src, len, pos);
            if (pos < len && src[pos] == '#') tp_skip_line(src, len, pos);
            if (cur_sect[0] != '\0') {
                char* full_path = tp_build_path(cur_sect, key);
                tp_set_dotted(root, full_path, val);
                free(full_path);
            } else {
                tbl_set(root, key, val);
            }
            free(key);
        }
    }
    free(cur_sect);
    return root;
}

// ═══════════════════════════════════════════════════════════════════════
// Public C helpers (extern "C" — called from Aria)
//
// String params: const char* (Aria passes string as null-terminated ptr)
// String returns: AriaString* (Aria expects struct with data + length)
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// ── Node constructors ────────────────────────────────────────────────

void* th_new_string(const char* val) {
    TomlValue* v = tv_alloc(TH_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    return v;
}

void* th_new_int(int64_t val) {
    TomlValue* v = tv_alloc(TH_INT); v->int_val = val; return v;
}

void* th_new_float(double val) {
    TomlValue* v = tv_alloc(TH_FLOAT); v->float_val = val; return v;
}

void* th_new_bool(int32_t val) {
    TomlValue* v = tv_alloc(TH_BOOL); v->bool_val = val ? 1 : 0; return v;
}

void* th_new_table() { return tv_alloc(TH_TABLE); }
void* th_new_array() { return tv_alloc(TH_ARRAY); }

// ── Node query ───────────────────────────────────────────────────────

int32_t th_type(void* h) {
    if (!h) return TH_TABLE;
    return ((TomlValue*)h)->type;
}

AriaString* th_get_string(void* h) {
    if (!h) return make_aria_str("", 0);
    TomlValue* v = (TomlValue*)h;
    if (v->type != TH_STRING || !v->str_val) return make_aria_str("", 0);
    return make_aria_str(v->str_val, (int64_t)strlen(v->str_val));
}

int64_t th_get_int(void* h) {
    if (!h) return 0;
    TomlValue* v = (TomlValue*)h;
    if (v->type == TH_INT) return v->int_val;
    if (v->type == TH_FLOAT) return (int64_t)v->float_val;
    return 0;
}

double th_get_float(void* h) {
    if (!h) return 0.0;
    TomlValue* v = (TomlValue*)h;
    if (v->type == TH_FLOAT) return v->float_val;
    if (v->type == TH_INT) return (double)v->int_val;
    return 0.0;
}

int32_t th_get_bool(void* h) {
    if (!h) return 0;
    TomlValue* v = (TomlValue*)h;
    if (v->type != TH_BOOL) return 0;
    return v->bool_val;
}

// ── Table helpers (Aria-facing) ──────────────────────────────────────

void th_tbl_set(void* h, const char* key, void* child) {
    tbl_set((TomlValue*)h, key, (TomlValue*)child);
}

void* th_tbl_find(void* h, const char* key) {
    return tbl_find((TomlValue*)h, key);
}

int64_t th_tbl_count(void* h) {
    if (!h || ((TomlValue*)h)->type != TH_TABLE) return 0;
    return ((TomlValue*)h)->tbl_len;
}

AriaString* th_tbl_key_at(void* h, int64_t idx) {
    TomlValue* t = (TomlValue*)h;
    if (!t || t->type != TH_TABLE || idx < 0 || idx >= t->tbl_len)
        return make_aria_str("", 0);
    return make_aria_str(t->tbl_keys[idx], (int64_t)strlen(t->tbl_keys[idx]));
}

void* th_tbl_val_at(void* h, int64_t idx) {
    TomlValue* t = (TomlValue*)h;
    if (!t || t->type != TH_TABLE || idx < 0 || idx >= t->tbl_len)
        return nullptr;
    return t->tbl_vals[idx];
}

// ── Array helpers (Aria-facing) ──────────────────────────────────────

void th_arr_push(void* h, void* child) {
    if (!h) return;
    TomlValue* a = (TomlValue*)h;
    if (a->type != TH_ARRAY) return;
    arr_grow(a);
    a->arr_items[a->arr_len++] = (TomlValue*)child;
}

void* th_arr_get(void* h, int64_t idx) {
    TomlValue* a = (TomlValue*)h;
    if (!a || a->type != TH_ARRAY || idx < 0 || idx >= a->arr_len)
        return nullptr;
    return a->arr_items[idx];
}

int64_t th_arr_len(void* h) {
    if (!h || ((TomlValue*)h)->type != TH_ARRAY) return 0;
    return ((TomlValue*)h)->arr_len;
}

// ── Clone / Free ─────────────────────────────────────────────────────

void* th_clone(void* h) { return tv_clone((TomlValue*)h); }
void  th_free(void* h)  { tv_free((TomlValue*)h); }

// ── DynBuf helpers (Aria-facing) ─────────────────────────────────────

void* th_buf_new() {
    DynBuf* b = (DynBuf*)malloc(sizeof(DynBuf));
    if (!b) std::abort();
    b->cap = 256; b->len = 0;
    b->data = (char*)malloc(b->cap);
    if (!b->data) std::abort();
    return b;
}

void th_buf_char(void* buf, int32_t c) {
    DynBuf* b = (DynBuf*)buf;
    buf_ensure(b, 1);
    b->data[b->len++] = (char)c;
}

void th_buf_str(void* buf, const char* s) {
    if (!s || !s[0]) return;
    DynBuf* b = (DynBuf*)buf;
    int64_t slen = (int64_t)strlen(s);
    buf_ensure(b, slen);
    memcpy(b->data + b->len, s, slen);
    b->len += slen;
}

AriaString* th_buf_to_string(void* buf) {
    DynBuf* b = (DynBuf*)buf;
    AriaString* result = make_aria_str(b->data, b->len);
    free(b->data);
    free(b);
    return result;
}

// ── Number formatting ────────────────────────────────────────────────

AriaString* th_int_to_cstr(int64_t val) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", (long)val);
    return make_aria_str(tmp, (int64_t)strlen(tmp));
}

AriaString* th_float_to_cstr(double val) {
    char tmp[64];
    if (std::isnan(val)) return make_aria_str("nan", 3);
    if (std::isinf(val))
        return val > 0 ? make_aria_str("inf", 3) : make_aria_str("-inf", 4);
    snprintf(tmp, sizeof(tmp), "%.17g", val);
    bool has_dot = false;
    for (char* c = tmp; *c; c++)
        if (*c == '.' || *c == 'e' || *c == 'E') has_dot = true;
    if (!has_dot) { int64_t l = (int64_t)strlen(tmp); tmp[l] = '.'; tmp[l+1] = '0'; tmp[l+2] = '\0'; }
    return make_aria_str(tmp, (int64_t)strlen(tmp));
}

// ── String byte access (called from Aria encoder) ────────────────────

int32_t th_byte_at(const char* s, int64_t idx) {
    if (!s || idx < 0) return 0;
    return (int32_t)(unsigned char)s[idx];
}

int64_t th_strlen(const char* s) {
    if (!s) return 0;
    return (int64_t)strlen(s);
}

// ═══════════════════════════════════════════════════════════════════════
// Public API (called directly from Aria as externs)
// ═══════════════════════════════════════════════════════════════════════

void* toml_new_table() { return tv_alloc(TH_TABLE); }
void* toml_new_array() { return tv_alloc(TH_ARRAY); }

void toml_set_str(void* h, const char* key, const char* val) {
    TomlValue* v = tv_alloc(TH_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    tbl_set((TomlValue*)h, key, v);
}
void toml_set_int(void* h, const char* key, int64_t val) {
    TomlValue* v = tv_alloc(TH_INT); v->int_val = val;
    tbl_set((TomlValue*)h, key, v);
}
void toml_set_flt(void* h, const char* key, double val) {
    TomlValue* v = tv_alloc(TH_FLOAT); v->float_val = val;
    tbl_set((TomlValue*)h, key, v);
}
void toml_set_bool(void* h, const char* key, int32_t val) {
    TomlValue* v = tv_alloc(TH_BOOL); v->bool_val = val ? 1 : 0;
    tbl_set((TomlValue*)h, key, v);
}
void toml_set_tbl(void* h, const char* key, void* child) {
    tbl_set((TomlValue*)h, key, tv_clone((TomlValue*)child));
}
void toml_set_arr(void* h, const char* key, void* child) {
    tbl_set((TomlValue*)h, key, tv_clone((TomlValue*)child));
}

void toml_arr_push_str(void* h, const char* val) {
    TomlValue* v = tv_alloc(TH_STRING);
    v->str_val = val ? strdup(val) : strdup("");
    th_arr_push(h, v);
}
void toml_arr_push_int(void* h, int64_t val) {
    TomlValue* v = tv_alloc(TH_INT); v->int_val = val;
    th_arr_push(h, v);
}
void toml_arr_push_flt(void* h, double val) {
    TomlValue* v = tv_alloc(TH_FLOAT); v->float_val = val;
    th_arr_push(h, v);
}
void toml_arr_push_bool(void* h, int32_t val) {
    TomlValue* v = tv_alloc(TH_BOOL); v->bool_val = val ? 1 : 0;
    th_arr_push(h, v);
}
void toml_arr_push_tbl(void* h, void* child) {
    th_arr_push(h, tv_clone((TomlValue*)child));
}
void toml_arr_push_arr(void* h, void* child) {
    th_arr_push(h, tv_clone((TomlValue*)child));
}

void* toml_parse(const char* toml_str) {
    if (!toml_str || !toml_str[0])
        return tv_alloc(TH_TABLE);
    return tp_parse_document(toml_str, (int64_t)strlen(toml_str));
}

int32_t toml_type(void* h) { return th_type(h); }

int32_t toml_has(void* h, const char* key) {
    return tbl_find((TomlValue*)h, key) ? 1 : 0;
}

int64_t toml_count(void* h) { return th_tbl_count(h); }

AriaString* toml_get_str(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found || found->type != TH_STRING || !found->str_val) return make_aria_str("", 0);
    return make_aria_str(found->str_val, (int64_t)strlen(found->str_val));
}

int64_t toml_get_int(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found) return 0;
    if (found->type == TH_INT) return found->int_val;
    if (found->type == TH_FLOAT) return (int64_t)found->float_val;
    return 0;
}

double toml_get_flt(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found) return 0.0;
    if (found->type == TH_FLOAT) return found->float_val;
    if (found->type == TH_INT) return (double)found->int_val;
    return 0.0;
}

int32_t toml_get_bool(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found || found->type != TH_BOOL) return 0;
    return found->bool_val;
}

void* toml_get_tbl(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found || found->type != TH_TABLE) return tv_alloc(TH_TABLE);
    return tv_clone(found);
}

void* toml_get_arr(void* h, const char* key) {
    TomlValue* found = tbl_find((TomlValue*)h, key);
    if (!found || found->type != TH_ARRAY) return tv_alloc(TH_ARRAY);
    return tv_clone(found);
}

int64_t toml_arr_len(void* h) { return th_arr_len(h); }

AriaString* toml_arr_get_str(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem || elem->type != TH_STRING || !elem->str_val) return make_aria_str("", 0);
    return make_aria_str(elem->str_val, (int64_t)strlen(elem->str_val));
}

int64_t toml_arr_get_int(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem) return 0;
    if (elem->type == TH_INT) return elem->int_val;
    if (elem->type == TH_FLOAT) return (int64_t)elem->float_val;
    return 0;
}

double toml_arr_get_flt(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem) return 0.0;
    if (elem->type == TH_FLOAT) return elem->float_val;
    if (elem->type == TH_INT) return (double)elem->int_val;
    return 0.0;
}

int32_t toml_arr_get_bool(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem || elem->type != TH_BOOL) return 0;
    return elem->bool_val;
}

void* toml_arr_get_tbl(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem || elem->type != TH_TABLE) return tv_alloc(TH_TABLE);
    return tv_clone(elem);
}

void* toml_arr_get_arr(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem || elem->type != TH_ARRAY) return tv_alloc(TH_ARRAY);
    return tv_clone(elem);
}

int32_t toml_arr_get_type(void* h, int64_t idx) {
    TomlValue* elem = (TomlValue*)th_arr_get(h, idx);
    if (!elem) return TH_STRING;
    return (int32_t)elem->type;
}

void toml_free(void* h) { tv_free((TomlValue*)h); }

} // extern "C"
