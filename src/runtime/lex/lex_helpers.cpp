/**
 * C helpers for Aria's self-hosted lexer (Phase 5.1).
 *
 * Provides irreducible operations for lexer self-hosting:
 *   - LexState: opaque mutable state (source, position, line/col tracking)
 *   - Token list management (push, get, count)
 *   - Keyword lookup via hash table (~121 keywords)
 *   - Type suffix → token type mapping
 *   - Number parsing (stoll/stod)
 *   - AriaString helpers for lexeme/error extraction
 *
 * All scanning LOGIC (whitespace, operators, identifiers, numbers,
 * strings, template literals) lives in stdlib/lexer.aria.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include "frontend/token.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <algorithm>

using npk::frontend::TokenType;

// ═══════════════════════════════════════════════════════════════════════
// AriaString helper (same pattern as json/toml helpers)
// ═══════════════════════════════════════════════════════════════════════

static AriaString* make_str(const char* data, int64_t length) {
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
// Internal token structure
// ═══════════════════════════════════════════════════════════════════════

struct LexToken {
    int32_t type;
    char* lexeme;
    int32_t line;
    int32_t col;
    int64_t int_val;
    double float_val;
    char* str_val;      // For string/char token values
    char* raw_text;     // For high-precision numeric raw text
};

// ═══════════════════════════════════════════════════════════════════════
// Token list (dynamic array)
// ═══════════════════════════════════════════════════════════════════════

struct TokenList {
    LexToken* items;
    int64_t count;
    int64_t cap;
};

static TokenList* tl_new() {
    TokenList* tl = (TokenList*)malloc(sizeof(TokenList));
    tl->cap = 256;
    tl->count = 0;
    tl->items = (LexToken*)calloc(tl->cap, sizeof(LexToken));
    return tl;
}

static void tl_grow(TokenList* tl) {
    tl->cap *= 2;
    tl->items = (LexToken*)realloc(tl->items, tl->cap * sizeof(LexToken));
}

static char* dup_str(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    memcpy(d, s, len + 1);
    return d;
}

static void tl_push(TokenList* tl, int32_t type, const char* lexeme,
                     int32_t line, int32_t col,
                     int64_t int_val, double float_val,
                     const char* str_val, const char* raw_text) {
    if (tl->count >= tl->cap) tl_grow(tl);
    LexToken* t = &tl->items[tl->count++];
    t->type = type;
    t->lexeme = dup_str(lexeme);
    t->line = line;
    t->col = col;
    t->int_val = int_val;
    t->float_val = float_val;
    t->str_val = dup_str(str_val);
    t->raw_text = dup_str(raw_text);
}

static void tl_free(TokenList* tl) {
    if (!tl) return;
    for (int64_t i = 0; i < tl->count; i++) {
        free(tl->items[i].lexeme);
        free(tl->items[i].str_val);
        free(tl->items[i].raw_text);
    }
    free(tl->items);
    free(tl);
}

// ═══════════════════════════════════════════════════════════════════════
// Lexer state (opaque handle managed in C)
// ═══════════════════════════════════════════════════════════════════════

struct LexState {
    const char* source;
    int64_t length;
    int64_t current;
    int64_t start;
    int32_t line;
    int32_t column;
    int32_t start_line;
    int32_t start_column;
    TokenList* tokens;
    // Error tracking
    char** errors;
    int64_t error_count;
    int64_t error_cap;
};

// ═══════════════════════════════════════════════════════════════════════
// Keyword hash table
// ═══════════════════════════════════════════════════════════════════════

#define KW_TABLE_SIZE 512

struct KWEntry {
    const char* keyword;
    int32_t type;
};

static KWEntry kw_table[KW_TABLE_SIZE];
static bool kw_initialized = false;

static uint32_t kw_hash(const char* s) {
    uint32_t h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h;
}

static void kw_insert(const char* keyword, TokenType tt) {
    int32_t type = static_cast<int32_t>(tt);
    uint32_t idx = kw_hash(keyword) & (KW_TABLE_SIZE - 1);
    for (int i = 0; i < KW_TABLE_SIZE; i++) {
        uint32_t probe = (idx + i) & (KW_TABLE_SIZE - 1);
        if (!kw_table[probe].keyword) {
            kw_table[probe].keyword = keyword;
            kw_table[probe].type = type;
            return;
        }
    }
}

static void kw_init() {
    if (kw_initialized) return;
    memset(kw_table, 0, sizeof(kw_table));

    // Memory qualifiers
    kw_insert("wild",    TokenType::TOKEN_KW_WILD);
    kw_insert("wildx",   TokenType::TOKEN_KW_WILDX);
    kw_insert("stack",   TokenType::TOKEN_KW_STACK);
    kw_insert("gc",      TokenType::TOKEN_KW_GC);
    kw_insert("defer",   TokenType::TOKEN_KW_DEFER);
    kw_insert("move",    TokenType::TOKEN_KW_MOVE);

    // Memory ordering
    kw_insert("relaxed", TokenType::TOKEN_KW_RELAXED);
    kw_insert("acquire", TokenType::TOKEN_KW_ACQUIRE);
    kw_insert("release", TokenType::TOKEN_KW_RELEASE);
    kw_insert("acq_rel", TokenType::TOKEN_KW_ACQ_REL);
    kw_insert("seq_cst", TokenType::TOKEN_KW_SEQ_CST);

    // Control flow
    kw_insert("if",       TokenType::TOKEN_KW_IF);
    kw_insert("else",     TokenType::TOKEN_KW_ELSE);
    kw_insert("while",    TokenType::TOKEN_KW_WHILE);
    kw_insert("for",      TokenType::TOKEN_KW_FOR);
    kw_insert("loop",     TokenType::TOKEN_KW_LOOP);
    kw_insert("till",     TokenType::TOKEN_KW_TILL);
    kw_insert("when",     TokenType::TOKEN_KW_WHEN);
    kw_insert("then",     TokenType::TOKEN_KW_THEN);
    kw_insert("end",      TokenType::TOKEN_KW_END);
    kw_insert("pick",     TokenType::TOKEN_KW_PICK);
    kw_insert("fall",     TokenType::TOKEN_KW_FALL);
    kw_insert("break",    TokenType::TOKEN_KW_BREAK);
    kw_insert("continue", TokenType::TOKEN_KW_CONTINUE);
    kw_insert("return",   TokenType::TOKEN_KW_RETURN);
    kw_insert("pass",     TokenType::TOKEN_KW_PASS);
    kw_insert("fail",     TokenType::TOKEN_KW_FAIL);

    // Async
    kw_insert("async",  TokenType::TOKEN_KW_ASYNC);
    kw_insert("await",  TokenType::TOKEN_KW_AWAIT);
    kw_insert("catch",  TokenType::TOKEN_KW_CATCH);
    kw_insert("in",     TokenType::TOKEN_KW_IN);

    // Module system
    kw_insert("use",    TokenType::TOKEN_KW_USE);
    kw_insert("mod",    TokenType::TOKEN_KW_MOD);
    kw_insert("pub",    TokenType::TOKEN_KW_PUB);
    kw_insert("extern", TokenType::TOKEN_KW_EXTERN);
    kw_insert("cfg",    TokenType::TOKEN_KW_CFG);
    kw_insert("as",     TokenType::TOKEN_KW_AS);

    // Type declarations
    kw_insert("struct", TokenType::TOKEN_KW_STRUCT);
    kw_insert("enum",   TokenType::TOKEN_KW_ENUM);
    kw_insert("Type",   TokenType::TOKEN_KW_TYPE);
    kw_insert("opaque", TokenType::TOKEN_KW_OPAQUE);
    kw_insert("trait",  TokenType::TOKEN_KW_TRAIT);
    kw_insert("impl",   TokenType::TOKEN_KW_IMPL);

    // Other keywords
    kw_insert("const",  TokenType::TOKEN_KW_CONST);
    kw_insert("fixed",  TokenType::TOKEN_KW_FIXED);
    kw_insert("is",     TokenType::TOKEN_KW_IS);

    // Design by Contract
    kw_insert("requires",  TokenType::TOKEN_KW_REQUIRES);
    kw_insert("ensures",   TokenType::TOKEN_KW_ENSURES);
    kw_insert("invariant", TokenType::TOKEN_KW_INVARIANT);

    // Signed integers
    kw_insert("int1",    TokenType::TOKEN_KW_INT1);
    kw_insert("int2",    TokenType::TOKEN_KW_INT2);
    kw_insert("int4",    TokenType::TOKEN_KW_INT4);
    kw_insert("int8",    TokenType::TOKEN_KW_INT8);
    kw_insert("int16",   TokenType::TOKEN_KW_INT16);
    kw_insert("int32",   TokenType::TOKEN_KW_INT32);
    kw_insert("int64",   TokenType::TOKEN_KW_INT64);
    kw_insert("int128",  TokenType::TOKEN_KW_INT128);
    kw_insert("int256",  TokenType::TOKEN_KW_INT256);
    kw_insert("int512",  TokenType::TOKEN_KW_INT512);
    kw_insert("int1024", TokenType::TOKEN_KW_INT1024);
    kw_insert("int2048", TokenType::TOKEN_KW_INT2048);
    kw_insert("int4096", TokenType::TOKEN_KW_INT4096);

    // Unsigned integers
    kw_insert("uint1",    TokenType::TOKEN_KW_UINT1);
    kw_insert("uint2",    TokenType::TOKEN_KW_UINT2);
    kw_insert("uint4",    TokenType::TOKEN_KW_UINT4);
    kw_insert("uint8",    TokenType::TOKEN_KW_UINT8);
    kw_insert("uint16",   TokenType::TOKEN_KW_UINT16);
    kw_insert("uint32",   TokenType::TOKEN_KW_UINT32);
    kw_insert("uint64",   TokenType::TOKEN_KW_UINT64);
    kw_insert("uint128",  TokenType::TOKEN_KW_UINT128);
    kw_insert("uint256",  TokenType::TOKEN_KW_UINT256);
    kw_insert("uint512",  TokenType::TOKEN_KW_UINT512);
    kw_insert("uint1024", TokenType::TOKEN_KW_UINT1024);
    kw_insert("uint2048", TokenType::TOKEN_KW_UINT2048);
    kw_insert("uint4096", TokenType::TOKEN_KW_UINT4096);

    // TBB
    kw_insert("tbb8",  TokenType::TOKEN_KW_TBB8);
    kw_insert("tbb16", TokenType::TOKEN_KW_TBB16);
    kw_insert("tbb32", TokenType::TOKEN_KW_TBB32);
    kw_insert("tbb64", TokenType::TOKEN_KW_TBB64);

    // Fractions
    kw_insert("frac8",  TokenType::TOKEN_KW_FRAC8);
    kw_insert("frac16", TokenType::TOKEN_KW_FRAC16);
    kw_insert("frac32", TokenType::TOKEN_KW_FRAC32);
    kw_insert("frac64", TokenType::TOKEN_KW_FRAC64);

    // Twisted Floating Point
    kw_insert("tfp32", TokenType::TOKEN_KW_TFP32);
    kw_insert("tfp64", TokenType::TOKEN_KW_TFP64);

    // Floats
    kw_insert("flt32",  TokenType::TOKEN_KW_FLT32);
    kw_insert("flt64",  TokenType::TOKEN_KW_FLT64);
    kw_insert("flt128", TokenType::TOKEN_KW_FLT128);
    kw_insert("flt256", TokenType::TOKEN_KW_FLT256);
    kw_insert("flt512", TokenType::TOKEN_KW_FLT512);

    // Fixed point
    kw_insert("fix256", TokenType::TOKEN_KW_FIX256);

    // Special types
    kw_insert("bool",   TokenType::TOKEN_KW_BOOL);
    kw_insert("string", TokenType::TOKEN_KW_STRING);
    kw_insert("dyn",    TokenType::TOKEN_KW_DYN);
    kw_insert("obj",    TokenType::TOKEN_KW_OBJ);
    kw_insert("Result", TokenType::TOKEN_KW_RESULT);
    kw_insert("array",  TokenType::TOKEN_KW_ARRAY);
    kw_insert("func",   TokenType::TOKEN_KW_FUNC);

    // Balanced ternary/nonary
    kw_insert("trit",  TokenType::TOKEN_KW_TRIT);
    kw_insert("tryte", TokenType::TOKEN_KW_TRYTE);
    kw_insert("nit",   TokenType::TOKEN_KW_NIT);
    kw_insert("nyte",  TokenType::TOKEN_KW_NYTE);

    // Mathematical
    kw_insert("vec2",    TokenType::TOKEN_KW_VEC2);
    kw_insert("vec3",    TokenType::TOKEN_KW_VEC3);
    kw_insert("vec9",    TokenType::TOKEN_KW_VEC9);
    kw_insert("matrix",  TokenType::TOKEN_KW_MATRIX);
    kw_insert("tmatrix", TokenType::TOKEN_KW_TMATRIX);
    kw_insert("tensor",  TokenType::TOKEN_KW_TENSOR);
    kw_insert("ttensor", TokenType::TOKEN_KW_TTENSOR);

    // I/O and system
    kw_insert("binary",  TokenType::TOKEN_KW_BINARY);
    kw_insert("buffer",  TokenType::TOKEN_KW_BUFFER);
    kw_insert("stream",  TokenType::TOKEN_KW_STREAM);
    kw_insert("process", TokenType::TOKEN_KW_PROCESS);
    kw_insert("pipe",    TokenType::TOKEN_KW_PIPE);
    kw_insert("debug",   TokenType::TOKEN_KW_DEBUG);

    // Literals
    kw_insert("true",    TokenType::TOKEN_KW_TRUE);
    kw_insert("false",   TokenType::TOKEN_KW_FALSE);
    kw_insert("NULL",    TokenType::TOKEN_KW_NULL);
    kw_insert("NIL",     TokenType::TOKEN_KW_NIL);
    kw_insert("ERR",     TokenType::TOKEN_KW_ERR);
    kw_insert("unknown", TokenType::TOKEN_KW_UNKNOWN);

    kw_initialized = true;
}

// ═══════════════════════════════════════════════════════════════════════
// Suffix → token type mappings
// ═══════════════════════════════════════════════════════════════════════

static int32_t suffix_to_int(const char* suffix) {
    // Unsigned
    if (strcmp(suffix, "u8")    == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U8);
    if (strcmp(suffix, "u16")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U16);
    if (strcmp(suffix, "u32")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U32);
    if (strcmp(suffix, "u64")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U64);
    if (strcmp(suffix, "u128")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U128);
    if (strcmp(suffix, "u256")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U256);
    if (strcmp(suffix, "u512")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U512);
    if (strcmp(suffix, "u1024") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U1024);
    if (strcmp(suffix, "u2048") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U2048);
    if (strcmp(suffix, "u4096") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_U4096);
    // Signed
    if (strcmp(suffix, "i8")    == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I8);
    if (strcmp(suffix, "i16")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I16);
    if (strcmp(suffix, "i32")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I32);
    if (strcmp(suffix, "i64")   == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I64);
    if (strcmp(suffix, "i128")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I128);
    if (strcmp(suffix, "i256")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I256);
    if (strcmp(suffix, "i512")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I512);
    if (strcmp(suffix, "i1024") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I1024);
    if (strcmp(suffix, "i2048") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I2048);
    if (strcmp(suffix, "i4096") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_I4096);
    // TBB
    if (strcmp(suffix, "tbb8")  == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_TBB8);
    if (strcmp(suffix, "tbb16") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_TBB16);
    if (strcmp(suffix, "tbb32") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_TBB32);
    if (strcmp(suffix, "tbb64") == 0) return static_cast<int32_t>(TokenType::TOKEN_INTEGER_TBB64);
    return -1;
}

static int32_t suffix_to_flt(const char* suffix) {
    if (strcmp(suffix, "f32")    == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_F32);
    if (strcmp(suffix, "f64")    == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_F64);
    if (strcmp(suffix, "f128")   == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_F128);
    if (strcmp(suffix, "f256")   == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_F256);
    if (strcmp(suffix, "f512")   == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_F512);
    if (strcmp(suffix, "fix256") == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_FIX256);
    if (strcmp(suffix, "tfp32")  == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_TFP32);
    if (strcmp(suffix, "tfp64")  == 0) return static_cast<int32_t>(TokenType::TOKEN_FLOAT_TFP64);
    return -1;
}

// ═══════════════════════════════════════════════════════════════════════
// Public C API
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// -----------------------------------------------------------------------
// State management
// -----------------------------------------------------------------------

void* lex_new(const char* source) {
    kw_init();
    LexState* s = (LexState*)malloc(sizeof(LexState));
    s->length = (int64_t)strlen(source);
    // Copy source so we own it
    char* src_copy = (char*)malloc(s->length + 1);
    memcpy(src_copy, source, s->length + 1);
    s->source = src_copy;
    s->current = 0;
    s->start = 0;
    s->line = 1;
    s->column = 1;
    s->start_line = 1;
    s->start_column = 1;
    s->tokens = tl_new();
    s->error_cap = 16;
    s->error_count = 0;
    s->errors = (char**)calloc(s->error_cap, sizeof(char*));
    return s;
}

void lex_free(void* state) {
    if (!state) return;
    LexState* s = (LexState*)state;
    free((void*)s->source);
    tl_free(s->tokens);
    for (int64_t i = 0; i < s->error_count; i++) free(s->errors[i]);
    free(s->errors);
    free(s);
}

// -----------------------------------------------------------------------
// Character navigation
// -----------------------------------------------------------------------

int32_t lex_advance(void* state) {
    LexState* s = (LexState*)state;
    if (s->current >= s->length) return 0;
    char c = s->source[s->current];
    s->current++;
    s->column++;
    if (c == '\n') {
        s->line++;
        s->column = 1;
    }
    return (int32_t)(unsigned char)c;
}

int32_t lex_peek(void* state) {
    LexState* s = (LexState*)state;
    if (s->current >= s->length) return 0;
    return (int32_t)(unsigned char)s->source[s->current];
}

int32_t lex_peek_next(void* state) {
    LexState* s = (LexState*)state;
    if (s->current + 1 >= s->length) return 0;
    return (int32_t)(unsigned char)s->source[s->current + 1];
}

int32_t lex_peek_n(void* state, int32_t n) {
    LexState* s = (LexState*)state;
    int64_t pos = s->current + n;
    if (pos >= s->length) return 0;
    return (int32_t)(unsigned char)s->source[pos];
}

int32_t lex_at_end(void* state) {
    LexState* s = (LexState*)state;
    return s->current >= s->length ? 1 : 0;
}

int32_t lex_match(void* state, int32_t expected) {
    LexState* s = (LexState*)state;
    if (s->current >= s->length) return 0;
    if ((int32_t)(unsigned char)s->source[s->current] != expected) return 0;
    // Match succeeded — advance
    lex_advance(state);
    return 1;
}

// -----------------------------------------------------------------------
// Position tracking
// -----------------------------------------------------------------------

void lex_start_token(void* state) {
    LexState* s = (LexState*)state;
    s->start = s->current;
    s->start_line = s->line;
    s->start_column = s->column;
}

int32_t lex_get_line(void* state) {
    return ((LexState*)state)->line;
}

int32_t lex_get_col(void* state) {
    return ((LexState*)state)->column;
}

int32_t lex_get_start_line(void* state) {
    return ((LexState*)state)->start_line;
}

int32_t lex_get_start_col(void* state) {
    return ((LexState*)state)->start_column;
}

int32_t lex_source_char(void* state, int64_t pos) {
    LexState* s = (LexState*)state;
    if (pos < 0 || pos >= s->length) return 0;
    return (int32_t)(unsigned char)s->source[pos];
}

int64_t lex_get_start(void* state) {
    return ((LexState*)state)->start;
}

int64_t lex_get_current(void* state) {
    return ((LexState*)state)->current;
}

// -----------------------------------------------------------------------
// Lexeme extraction
// -----------------------------------------------------------------------

AriaString lex_get_lexeme(void* state) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    if (len <= 0) return *make_str("", 0);
    return *make_str(s->source + s->start, len);
}

// Get a substring of source[from..from+len)
AriaString lex_get_substr(void* state, int64_t from, int64_t len) {
    LexState* s = (LexState*)state;
    if (from < 0 || from + len > s->length || len <= 0) return *make_str("", 0);
    return *make_str(s->source + from, len);
}

// -----------------------------------------------------------------------
// Token creation
// -----------------------------------------------------------------------

void lex_add_token(void* state, int32_t type) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            0, 0.0, nullptr, nullptr);
    free(lexeme);
}

void lex_add_token_int(void* state, int32_t type, int64_t value) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            value, 0.0, nullptr, nullptr);
    free(lexeme);
}

void lex_add_token_float(void* state, int32_t type, double value) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            0, value, nullptr, nullptr);
    free(lexeme);
}

void lex_add_token_str(void* state, int32_t type, const char* value) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            0, 0.0, value, nullptr);
    free(lexeme);
}

void lex_add_token_int_raw(void* state, int32_t type, int64_t value,
                            const char* raw_text) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            value, 0.0, nullptr, raw_text);
    free(lexeme);
}

void lex_add_token_float_raw(void* state, int32_t type, double value,
                              const char* raw_text) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    char* lexeme = (char*)malloc(len + 1);
    if (len > 0) memcpy(lexeme, s->source + s->start, len);
    lexeme[len] = '\0';
    tl_push(s->tokens, type, lexeme, s->start_line, s->start_column,
            0, value, nullptr, raw_text);
    free(lexeme);
}

// Push a token with explicit lexeme (for template literal parts)
void lex_push_token(void* state, int32_t type, const char* lexeme,
                    int32_t line, int32_t col) {
    LexState* s = (LexState*)state;
    tl_push(s->tokens, type, lexeme, line, col, 0, 0.0, nullptr, nullptr);
}

void lex_push_token_str(void* state, int32_t type, const char* lexeme,
                        int32_t line, int32_t col, const char* str_val) {
    LexState* s = (LexState*)state;
    tl_push(s->tokens, type, lexeme, line, col, 0, 0.0, str_val, nullptr);
}

// -----------------------------------------------------------------------
// Token list access
// -----------------------------------------------------------------------

int64_t lex_token_count(void* state) {
    return ((LexState*)state)->tokens->count;
}

int32_t lex_token_type(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return -1;
    return s->tokens->items[idx].type;
}

AriaString* lex_token_lexeme(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return make_str("", 0);
    const char* lex = s->tokens->items[idx].lexeme;
    return make_str(lex ? lex : "", lex ? (int64_t)strlen(lex) : 0);
}

int32_t lex_token_line(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return 0;
    return s->tokens->items[idx].line;
}

int32_t lex_token_col(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return 0;
    return s->tokens->items[idx].col;
}

int64_t lex_token_int_val(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return 0;
    return s->tokens->items[idx].int_val;
}

double lex_token_float_val(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return 0.0;
    return s->tokens->items[idx].float_val;
}

AriaString* lex_token_str_val(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return make_str("", 0);
    const char* sv = s->tokens->items[idx].str_val;
    return make_str(sv ? sv : "", sv ? (int64_t)strlen(sv) : 0);
}

AriaString* lex_token_raw_val(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->tokens->count) return make_str("", 0);
    const char* rv = s->tokens->items[idx].raw_text;
    return make_str(rv ? rv : "", rv ? (int64_t)strlen(rv) : 0);
}

// -----------------------------------------------------------------------
// Error management
// -----------------------------------------------------------------------

void lex_error(void* state, const char* msg) {
    LexState* s = (LexState*)state;
    if (s->error_count >= s->error_cap) {
        s->error_cap *= 2;
        s->errors = (char**)realloc(s->errors, s->error_cap * sizeof(char*));
    }
    // Format: [Line N, Col M] Error: message
    char buf[1024];
    snprintf(buf, sizeof(buf), "[Line %d, Col %d] Error: %s",
             s->line, s->column, msg);
    s->errors[s->error_count++] = dup_str(buf);
}

int64_t lex_error_count(void* state) {
    return ((LexState*)state)->error_count;
}

AriaString lex_get_error(void* state, int64_t idx) {
    LexState* s = (LexState*)state;
    if (idx < 0 || idx >= s->error_count) return *make_str("", 0);
    const char* e = s->errors[idx];
    return *make_str(e, (int64_t)strlen(e));
}

// -----------------------------------------------------------------------
// Keyword lookup
// -----------------------------------------------------------------------

// Combined lexeme→keyword lookup for Aria self-hosted lexer (avoids AriaString ABI mismatch)
int32_t lex_scan_keyword(void* state) {
    LexState* s = (LexState*)state;
    int64_t len = s->current - s->start;
    if (len <= 0 || len >= 256) return -1;
    char buf[256];
    memcpy(buf, s->source + s->start, len);
    buf[len] = '\0';
    kw_init();
    uint32_t idx = kw_hash(buf) & (KW_TABLE_SIZE - 1);
    for (int i = 0; i < KW_TABLE_SIZE; i++) {
        uint32_t probe = (idx + i) & (KW_TABLE_SIZE - 1);
        if (!kw_table[probe].keyword) return -1;
        if (strcmp(kw_table[probe].keyword, buf) == 0)
            return kw_table[probe].type;
    }
    return -1;
}

int32_t kw_lookup(const char* word) {
    kw_init();
    uint32_t idx = kw_hash(word) & (KW_TABLE_SIZE - 1);
    for (int i = 0; i < KW_TABLE_SIZE; i++) {
        uint32_t probe = (idx + i) & (KW_TABLE_SIZE - 1);
        if (!kw_table[probe].keyword) return -1;
        if (strcmp(kw_table[probe].keyword, word) == 0)
            return kw_table[probe].type;
    }
    return -1;
}

// -----------------------------------------------------------------------
// Type suffix mapping
// -----------------------------------------------------------------------

int32_t suffix_to_int_type(const char* suffix) {
    return suffix_to_int(suffix);
}

int32_t suffix_to_float_type(const char* suffix) {
    return suffix_to_flt(suffix);
}

// -----------------------------------------------------------------------
// Number parsing helpers
// -----------------------------------------------------------------------

int64_t lex_parse_int(const char* text, int32_t base) {
    if (!text || !*text) return 0;
    try {
        if (base == 10) {
            // Check if value would overflow signed — try unsigned first
            // for unsigned token types
            return std::stoll(text, nullptr, base);
        }
        return std::stoll(text, nullptr, base);
    } catch (...) {
        return 0;
    }
}

// Parse unsigned, return as int64 bit pattern (for u64 values >= 2^63)
int64_t lex_parse_uint(const char* text, int32_t base) {
    if (!text || !*text) return 0;
    try {
        uint64_t val = std::stoull(text, nullptr, base);
        return static_cast<int64_t>(val);
    } catch (...) {
        return 0;
    }
}

double lex_parse_float(const char* text) {
    if (!text || !*text) return 0.0;
    try {
        return std::stod(text);
    } catch (...) {
        return 0.0;
    }
}

// Remove underscores from numeric text
AriaString lex_strip_underscores(const char* text) {
    if (!text) return *make_str("", 0);
    size_t len = strlen(text);
    char* buf = (char*)malloc(len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] != '_') buf[j++] = text[i];
    }
    buf[j] = '\0';
    AriaString result = *make_str(buf, (int64_t)j);
    free(buf);
    return result;
}

// -----------------------------------------------------------------------
// Token type name (for debugging/testing)
// -----------------------------------------------------------------------

AriaString lex_type_name(int32_t type) {
    // Self-contained: return numeric string representation
    // Token type names can be matched via the TK_* constants in lexer.aria
    std::string name = std::to_string(type);
    return *make_str(name.c_str(), (int64_t)name.length());
}

// -----------------------------------------------------------------------
// Token type constants (expose enum values for Aria)
// -----------------------------------------------------------------------

int32_t lex_tk_value(int32_t ordinal) {
    // Identity function — token types are sequential from 0
    // This allows Aria to get the int32 value of any token type
    return ordinal;
}

// Expose specific token type constants
int32_t lex_tk_eof()        { return static_cast<int32_t>(TokenType::TOKEN_EOF); }
int32_t lex_tk_error()      { return static_cast<int32_t>(TokenType::TOKEN_ERROR); }
int32_t lex_tk_identifier() { return static_cast<int32_t>(TokenType::TOKEN_IDENTIFIER); }
int32_t lex_tk_integer()    { return static_cast<int32_t>(TokenType::TOKEN_INTEGER); }
int32_t lex_tk_float()      { return static_cast<int32_t>(TokenType::TOKEN_FLOAT); }
int32_t lex_tk_string()     { return static_cast<int32_t>(TokenType::TOKEN_STRING); }
int32_t lex_tk_char()       { return static_cast<int32_t>(TokenType::TOKEN_CHAR); }

// -----------------------------------------------------------------------
// Character classification (moved from Aria to C to avoid struct-return ABI)
// -----------------------------------------------------------------------

int32_t lex_is_alpha(int32_t c) {
    return ((c >= 65 && c <= 90) || (c >= 97 && c <= 122) || c == 95) ? 1 : 0;
}

int32_t lex_is_digit(int32_t c) {
    return (c >= 48 && c <= 57) ? 1 : 0;
}

int32_t lex_is_alnum(int32_t c) {
    return (lex_is_alpha(c) || lex_is_digit(c)) ? 1 : 0;
}

int32_t lex_is_hex(int32_t c) {
    return ((c >= 48 && c <= 57) || (c >= 65 && c <= 70) || (c >= 97 && c <= 102)) ? 1 : 0;
}

int32_t lex_is_bin(int32_t c) {
    return (c == 48 || c == 49) ? 1 : 0;
}

int32_t lex_is_oct(int32_t c) {
    return (c >= 48 && c <= 55) ? 1 : 0;
}

} // extern "C"
