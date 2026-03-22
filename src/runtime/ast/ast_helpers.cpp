/**
 * C helpers for Aria's self-hosted parser (Phase 5.2).
 *
 * Provides:
 *   - ASTNode: tagged tree node with string/numeric payloads and child arrays
 *   - ParseState: wraps the lex token list + position cursor + error list
 *   - Node constructors, child management, payload access
 *   - Precedence / type-keyword lookup tables
 *   - Error formatting helpers
 *
 * All parsing LOGIC lives in stdlib/parser.aria.
 */

#include "runtime/strings.h"
#include "runtime/gc.h"
#include "frontend/token.h"
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <string>
#include <algorithm>

using aria::frontend::TokenType;

// ═══════════════════════════════════════════════════════════════════════
// AriaString helper (same pattern as lex/json/toml helpers)
// ═══════════════════════════════════════════════════════════════════════

static AriaString* make_str(const char* data, int64_t length) {
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

static char* dup_str(const char* s) {
    if (!s) return nullptr;
    size_t len = strlen(s);
    char* d = (char*)malloc(len + 1);
    memcpy(d, s, len + 1);
    return d;
}

// ═══════════════════════════════════════════════════════════════════════
// AST Node structure
// ═══════════════════════════════════════════════════════════════════════

struct ASTNode {
    int32_t type;       // NodeType code
    int32_t line;
    int32_t col;

    // String payloads
    char* str_val;      // Primary: name, lexeme, type name
    char* str_val2;     // Secondary: member name, alias, type name 2
    char* str_val3;     // Tertiary: iterator type, etc.

    // Numeric payloads
    int64_t int_val;    // Integer literal value, operator type code, param count
    double  float_val;  // Float literal value
    int32_t bool_val;   // Boolean value
    int32_t flags;      // Packed bit flags

    // Children (dynamic array)
    ASTNode** children;
    int64_t child_count;
    int64_t child_cap;
};

static ASTNode* node_new(int32_t type, int32_t line, int32_t col) {
    ASTNode* n = (ASTNode*)malloc(sizeof(ASTNode));
    n->type = type;
    n->line = line;
    n->col = col;
    n->str_val = nullptr;
    n->str_val2 = nullptr;
    n->str_val3 = nullptr;
    n->int_val = 0;
    n->float_val = 0.0;
    n->bool_val = 0;
    n->flags = 0;
    n->child_cap = 4;
    n->child_count = 0;
    n->children = (ASTNode**)calloc(n->child_cap, sizeof(ASTNode*));
    return n;
}

static void node_add_child(ASTNode* parent, ASTNode* child) {
    if (!parent || !child) return;
    if (parent->child_count >= parent->child_cap) {
        parent->child_cap *= 2;
        parent->children = (ASTNode**)realloc(parent->children,
                                               parent->child_cap * sizeof(ASTNode*));
    }
    parent->children[parent->child_count++] = child;
}

static void node_free(ASTNode* n) {
    if (!n) return;
    free(n->str_val);
    free(n->str_val2);
    free(n->str_val3);
    for (int64_t i = 0; i < n->child_count; i++) {
        node_free(n->children[i]);
    }
    free(n->children);
    free(n);
}

// ═══════════════════════════════════════════════════════════════════════
// Forward-declare LexState internals (from lex_helpers.cpp)
// We only need the token access functions declared in extern "C" there.
// Instead of including internal structs, we re-use the lex_* API.
// ═══════════════════════════════════════════════════════════════════════

// These are available from lex_helpers.cpp (linked together)
extern "C" {
    int64_t lex_token_count(void* state);
    int32_t lex_token_type(void* state, int64_t idx);
    AriaString* lex_token_lexeme(void* state, int64_t idx);
    int32_t lex_token_line(void* state, int64_t idx);
    int32_t lex_token_col(void* state, int64_t idx);
    int64_t lex_token_int_val(void* state, int64_t idx);
    double  lex_token_float_val(void* state, int64_t idx);
    AriaString* lex_token_str_val(void* state, int64_t idx);
    AriaString* lex_token_raw_val(void* state, int64_t idx);
}

// ═══════════════════════════════════════════════════════════════════════
// ParseState: wraps token list + cursor + error list
// ═══════════════════════════════════════════════════════════════════════

struct ParseState {
    void* lex_state;        // Opaque pointer to LexState (from lex_helpers)
    int64_t current;        // Current token index
    int64_t token_count;    // Cached token count

    // Error management
    char** errors;
    int64_t error_count;
    int64_t error_cap;
};

static ParseState* ps_create(void* lex_state) {
    ParseState* ps = (ParseState*)malloc(sizeof(ParseState));
    ps->lex_state = lex_state;
    ps->current = 0;
    ps->token_count = lex_token_count(lex_state);
    ps->error_cap = 16;
    ps->error_count = 0;
    ps->errors = (char**)calloc(ps->error_cap, sizeof(char*));
    return ps;
}

// ═══════════════════════════════════════════════════════════════════════
// Type-keyword lookup table
// ═══════════════════════════════════════════════════════════════════════
// Maps token type codes → 1 if the token is a type keyword, 0 otherwise.
// Built from the isTypeKeyword() function in parser.cpp.

static int8_t type_kw_table[256];
static bool type_kw_inited = false;

static void init_type_kw_table() {
    if (type_kw_inited) return;
    type_kw_inited = true;
    memset(type_kw_table, 0, sizeof(type_kw_table));

    // Signed integers
    type_kw_table[(int)TokenType::TOKEN_KW_INT1] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT2] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT4] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT8] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT16] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT64] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT128] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT256] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT512] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT1024] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT2048] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_INT4096] = 1;

    // Unsigned integers
    type_kw_table[(int)TokenType::TOKEN_KW_UINT1] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT2] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT4] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT8] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT16] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT64] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT128] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT256] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT512] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT1024] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT2048] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_UINT4096] = 1;

    // TBB types
    type_kw_table[(int)TokenType::TOKEN_KW_TBB8] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TBB16] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TBB32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TBB64] = 1;

    // Frac types
    type_kw_table[(int)TokenType::TOKEN_KW_FRAC8] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FRAC16] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FRAC32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FRAC64] = 1;

    // TFP types
    type_kw_table[(int)TokenType::TOKEN_KW_TFP32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TFP64] = 1;

    // Floating point
    type_kw_table[(int)TokenType::TOKEN_KW_FLT32] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FLT64] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FLT128] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FLT256] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_FLT512] = 1;

    // Fixed point
    type_kw_table[(int)TokenType::TOKEN_KW_FIX256] = 1;

    // Other types
    type_kw_table[(int)TokenType::TOKEN_KW_BOOL] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_STRING] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_DYN] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_OBJ] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_RESULT] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_ARRAY] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_STRUCT] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_NIL] = 1;

    // Balanced ternary/nonary
    type_kw_table[(int)TokenType::TOKEN_KW_TRIT] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TRYTE] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_NIT] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_NYTE] = 1;

    // Math types
    type_kw_table[(int)TokenType::TOKEN_KW_VEC2] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_VEC3] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_VEC9] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TMATRIX] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TTENSOR] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_MATRIX] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_TENSOR] = 1;

    // I/O types
    type_kw_table[(int)TokenType::TOKEN_KW_BINARY] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_BUFFER] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_STREAM] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_PROCESS] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_PIPE] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_DEBUG] = 1;
    type_kw_table[(int)TokenType::TOKEN_KW_LOG] = 1;
}

// ═══════════════════════════════════════════════════════════════════════
// Precedence table
// ═══════════════════════════════════════════════════════════════════════
// Maps token type → operator precedence (-1 means not an operator).

static int8_t prec_table[256];
static bool prec_inited = false;

static void init_prec_table() {
    if (prec_inited) return;
    prec_inited = true;
    memset(prec_table, -1, sizeof(prec_table));  // -1 = not an operator

    // Assignment (lowest)
    prec_table[(int)TokenType::TOKEN_EQUAL] = 0;
    prec_table[(int)TokenType::TOKEN_PLUS_EQUAL] = 0;
    prec_table[(int)TokenType::TOKEN_MINUS_EQUAL] = 0;
    prec_table[(int)TokenType::TOKEN_STAR_EQUAL] = 0;
    prec_table[(int)TokenType::TOKEN_SLASH_EQUAL] = 0;
    prec_table[(int)TokenType::TOKEN_PERCENT_EQUAL] = 0;

    // Ternary
    prec_table[(int)TokenType::TOKEN_KW_IS] = 1;

    // Null coalescing
    prec_table[(int)TokenType::TOKEN_NULL_COALESCE] = 2;

    // Logical OR
    prec_table[(int)TokenType::TOKEN_OR_OR] = 3;

    // Logical AND
    prec_table[(int)TokenType::TOKEN_AND_AND] = 4;

    // Bitwise OR
    prec_table[(int)TokenType::TOKEN_PIPE] = 5;

    // Bitwise XOR
    prec_table[(int)TokenType::TOKEN_CARET] = 6;

    // Bitwise AND
    prec_table[(int)TokenType::TOKEN_AMPERSAND] = 7;

    // Equality
    prec_table[(int)TokenType::TOKEN_EQUAL_EQUAL] = 8;
    prec_table[(int)TokenType::TOKEN_BANG_EQUAL] = 8;

    // Comparison
    prec_table[(int)TokenType::TOKEN_LESS] = 9;
    prec_table[(int)TokenType::TOKEN_LESS_EQUAL] = 9;
    prec_table[(int)TokenType::TOKEN_GREATER] = 9;
    prec_table[(int)TokenType::TOKEN_GREATER_EQUAL] = 9;
    prec_table[(int)TokenType::TOKEN_SPACESHIP] = 9;

    // Range
    prec_table[(int)TokenType::TOKEN_DOT_DOT] = 10;
    prec_table[(int)TokenType::TOKEN_DOT_DOT_DOT] = 10;

    // Shift
    prec_table[(int)TokenType::TOKEN_SHIFT_LEFT] = 11;
    prec_table[(int)TokenType::TOKEN_SHIFT_RIGHT] = 11;

    // Additive
    prec_table[(int)TokenType::TOKEN_PLUS] = 12;
    prec_table[(int)TokenType::TOKEN_MINUS] = 12;

    // Multiplicative
    prec_table[(int)TokenType::TOKEN_STAR] = 13;
    prec_table[(int)TokenType::TOKEN_SLASH] = 13;
    prec_table[(int)TokenType::TOKEN_PERCENT] = 13;

    // Pipeline
    prec_table[(int)TokenType::TOKEN_PIPE_RIGHT] = 14;
    prec_table[(int)TokenType::TOKEN_PIPE_LEFT] = 14;

    // Cast shorthand
    prec_table[(int)TokenType::TOKEN_FAT_ARROW] = 15;

    // Postfix
    prec_table[(int)TokenType::TOKEN_PLUS_PLUS] = 16;
    prec_table[(int)TokenType::TOKEN_MINUS_MINUS] = 16;
    prec_table[(int)TokenType::TOKEN_LEFT_PAREN] = 16;
    prec_table[(int)TokenType::TOKEN_LEFT_BRACKET] = 16;
    prec_table[(int)TokenType::TOKEN_DOT] = 16;
    prec_table[(int)TokenType::TOKEN_ARROW] = 16;
    prec_table[(int)TokenType::TOKEN_SAFE_NAV] = 16;
}

// ═══════════════════════════════════════════════════════════════════════
// consumeName-compatible keyword set
// ═══════════════════════════════════════════════════════════════════════
// Tokens that consumeName() accepts (besides TOKEN_IDENTIFIER).

static int8_t name_kw_table[256];
static bool name_kw_inited = false;

static void init_name_kw_table() {
    if (name_kw_inited) return;
    name_kw_inited = true;
    memset(name_kw_table, 0, sizeof(name_kw_table));
    name_kw_table[(int)TokenType::TOKEN_KW_RESULT] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_FUNC] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_OBJ] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_BUFFER] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_PROCESS] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_STACK] = 1;
    name_kw_table[(int)TokenType::TOKEN_KW_GC] = 1;
}

// ═══════════════════════════════════════════════════════════════════════
// extern "C" API
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// -----------------------------------------------------------------------
// AST node management
// -----------------------------------------------------------------------

void* ast_null_ptr() { return nullptr; }

void* ast_new_node(int32_t type, int32_t line, int32_t col) {
    return node_new(type, line, col);
}

void ast_set_str(void* node, const char* s) {
    ASTNode* n = (ASTNode*)node;
    free(n->str_val);
    n->str_val = dup_str(s);
}

void ast_set_str2(void* node, const char* s) {
    ASTNode* n = (ASTNode*)node;
    free(n->str_val2);
    n->str_val2 = dup_str(s);
}

void ast_set_str3(void* node, const char* s) {
    ASTNode* n = (ASTNode*)node;
    free(n->str_val3);
    n->str_val3 = dup_str(s);
}

void ast_set_int(void* node, int64_t val) {
    ((ASTNode*)node)->int_val = val;
}

void ast_set_float(void* node, double val) {
    ((ASTNode*)node)->float_val = val;
}

void ast_set_bool(void* node, int32_t val) {
    ((ASTNode*)node)->bool_val = val;
}

void ast_set_flags(void* node, int32_t flags) {
    ((ASTNode*)node)->flags = flags;
}

void ast_add_child(void* parent, void* child) {
    node_add_child((ASTNode*)parent, (ASTNode*)child);
}

int64_t ast_child_count(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->child_count;
}

void* ast_child_at(void* node, int64_t idx) {
    ASTNode* n = (ASTNode*)node;
    if (!n || idx < 0 || idx >= n->child_count) {
        return nullptr;
    }
    return n->children[idx];
}

int32_t ast_get_type(void* node) {
    if (!node) return -1;
    return ((ASTNode*)node)->type;
}

AriaString* ast_get_str(void* node) {
    ASTNode* n = (ASTNode*)node;
    if (!n || !n->str_val) return make_str("", 0);
    return make_str(n->str_val, (int64_t)strlen(n->str_val));
}

AriaString* ast_get_str2(void* node) {
    ASTNode* n = (ASTNode*)node;
    if (!n || !n->str_val2) return make_str("", 0);
    return make_str(n->str_val2, (int64_t)strlen(n->str_val2));
}

AriaString* ast_get_str3(void* node) {
    ASTNode* n = (ASTNode*)node;
    if (!n || !n->str_val3) return make_str("", 0);
    return make_str(n->str_val3, (int64_t)strlen(n->str_val3));
}

int64_t ast_get_int(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->int_val;
}

double ast_get_float(void* node) {
    if (!node) return 0.0;
    return ((ASTNode*)node)->float_val;
}

int32_t ast_get_bool(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->bool_val;
}

int32_t ast_get_flags(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->flags;
}

int32_t ast_get_line(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->line;
}

int32_t ast_get_col(void* node) {
    if (!node) return 0;
    return ((ASTNode*)node)->col;
}

void ast_free(void* node) {
    node_free((ASTNode*)node);
}

// -----------------------------------------------------------------------
// ParseState management
// -----------------------------------------------------------------------

void* ps_new(void* lex_state) {
    init_type_kw_table();
    init_prec_table();
    init_name_kw_table();
    return ps_create(lex_state);
}

void ps_free(void* state) {
    if (!state) return;
    ParseState* ps = (ParseState*)state;
    for (int64_t i = 0; i < ps->error_count; i++) {
        free(ps->errors[i]);
    }
    free(ps->errors);
    free(ps);
}

// -----------------------------------------------------------------------
// Token access (via ParseState cursor)
// -----------------------------------------------------------------------

// Get type of current token
int32_t ps_peek(void* state) {
    ParseState* ps = (ParseState*)state;
    if (ps->current >= ps->token_count) {
        // Return TOKEN_EOF
        return lex_token_type(ps->lex_state, ps->token_count - 1);
    }
    return lex_token_type(ps->lex_state, ps->current);
}

// Get type of next token (current + 1)
int32_t ps_peek_next(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current + 1;
    if (idx >= ps->token_count) {
        return lex_token_type(ps->lex_state, ps->token_count - 1);
    }
    return lex_token_type(ps->lex_state, idx);
}

// Get type of token at offset n from current
int32_t ps_peek_at(void* state, int64_t offset) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current + offset;
    if (idx < 0 || idx >= ps->token_count) {
        return lex_token_type(ps->lex_state, ps->token_count - 1);
    }
    return lex_token_type(ps->lex_state, idx);
}

// Advance and return the PREVIOUS token type
void ps_advance(void* state) {
    ParseState* ps = (ParseState*)state;
    if (ps->current < ps->token_count) {
        ps->current++;
    }
}

int32_t ps_is_at_end(void* state) {
    ParseState* ps = (ParseState*)state;
    // At end if current token is TOKEN_EOF
    int32_t tok_type = lex_token_type(ps->lex_state, ps->current);
    int32_t eof_type = (int32_t)TokenType::TOKEN_EOF;
    if (tok_type == eof_type) return 1;
    return 0;
}

// Check if current token matches type (don't consume)
int32_t ps_check(void* state, int32_t type) {
    ParseState* ps = (ParseState*)state;
    if (ps_is_at_end(state)) return 0;
    return (lex_token_type(ps->lex_state, ps->current) == type) ? 1 : 0;
}

// If current token matches type, consume it and return 1; else return 0
int32_t ps_match(void* state, int32_t type) {
    if (ps_check(state, type)) {
        ps_advance(state);
        return 1;
    }
    return 0;
}

// Get/set raw position
int64_t ps_current_pos(void* state) {
    return ((ParseState*)state)->current;
}

void ps_set_pos(void* state, int64_t pos) {
    ((ParseState*)state)->current = pos;
}

// Previous token (current - 1) type
int32_t ps_previous_type(void* state) {
    ParseState* ps = (ParseState*)state;
    if (ps->current <= 0) return lex_token_type(ps->lex_state, 0);
    return lex_token_type(ps->lex_state, ps->current - 1);
}

// -----------------------------------------------------------------------
// Current token accessors
// -----------------------------------------------------------------------

AriaString* ps_peek_lexeme(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_lexeme(ps->lex_state, idx);
}

// Return the string value from a string literal token (lexeme with quotes stripped)
AriaString* ps_peek_string_value(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    AriaString* lex = lex_token_lexeme(ps->lex_state, idx);
    if (!lex || lex->length < 2) return lex;
    // Strip leading and trailing quote characters
    if ((lex->data[0] == '"' && lex->data[lex->length - 1] == '"') ||
        (lex->data[0] == '\'' && lex->data[lex->length - 1] == '\'')) {
        return make_str(lex->data + 1, lex->length - 2);
    }
    return lex;
}

int32_t ps_peek_line(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_line(ps->lex_state, idx);
}

int32_t ps_peek_col(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_col(ps->lex_state, idx);
}

int64_t ps_peek_int_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_int_val(ps->lex_state, idx);
}

double ps_peek_float_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_float_val(ps->lex_state, idx);
}

AriaString* ps_peek_str_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_str_val(ps->lex_state, idx);
}

AriaString* ps_peek_raw_text(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current;
    if (idx >= ps->token_count) idx = ps->token_count - 1;
    return lex_token_raw_val(ps->lex_state, idx);
}

// -----------------------------------------------------------------------
// Previous token accessors
// -----------------------------------------------------------------------

AriaString* ps_previous_lexeme(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_lexeme(ps->lex_state, idx);
}

int32_t ps_previous_line(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_line(ps->lex_state, idx);
}

int32_t ps_previous_col(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_col(ps->lex_state, idx);
}

int64_t ps_previous_int_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_int_val(ps->lex_state, idx);
}

double ps_previous_float_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_float_val(ps->lex_state, idx);
}

AriaString* ps_previous_str_val(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_str_val(ps->lex_state, idx);
}

AriaString* ps_previous_raw_text(void* state) {
    ParseState* ps = (ParseState*)state;
    int64_t idx = ps->current > 0 ? ps->current - 1 : 0;
    return lex_token_raw_val(ps->lex_state, idx);
}

// -----------------------------------------------------------------------
// Error management
// -----------------------------------------------------------------------

void ps_add_error(void* state, const char* msg) {
    ParseState* ps = (ParseState*)state;
    if (ps->error_count >= ps->error_cap) {
        ps->error_cap *= 2;
        ps->errors = (char**)realloc(ps->errors, ps->error_cap * sizeof(char*));
    }
    // Format with location from current token
    int32_t line = ps_peek_line(state);
    int32_t col  = ps_peek_col(state);
    char buf[2048];
    snprintf(buf, sizeof(buf), "Parse error at line %d, column %d: %s", line, col, msg);
    ps->errors[ps->error_count++] = dup_str(buf);
}

int64_t ps_error_count(void* state) {
    if (!state) return 0;
    return ((ParseState*)state)->error_count;
}

AriaString* ps_get_error(void* state, int64_t idx) {
    ParseState* ps = (ParseState*)state;
    if (!ps || idx < 0 || idx >= ps->error_count) return make_str("", 0);
    const char* e = ps->errors[idx];
    return make_str(e, (int64_t)strlen(e));
}

// -----------------------------------------------------------------------
// Lookup tables (exposed to Aria)
// -----------------------------------------------------------------------

int32_t ast_is_type_keyword(int32_t token_type) {
    init_type_kw_table();
    if (token_type < 0 || token_type >= 256) return 0;
    return type_kw_table[token_type];
}

int32_t ast_get_precedence(int32_t token_type) {
    init_prec_table();
    if (token_type < 0 || token_type >= 256) return -1;
    return prec_table[token_type];
}

int32_t ast_is_binary_op(int32_t token_type) {
    int32_t prec = ast_get_precedence(token_type);
    return (prec >= 0 && prec <= 14) ? 1 : 0;
}

int32_t ast_is_unary_op(int32_t token_type) {
    return (token_type == (int32_t)TokenType::TOKEN_MINUS ||
            token_type == (int32_t)TokenType::TOKEN_BANG ||
            token_type == (int32_t)TokenType::TOKEN_TILDE ||
            token_type == (int32_t)TokenType::TOKEN_AT ||
            token_type == (int32_t)TokenType::TOKEN_LEFT_ARROW ||
            token_type == (int32_t)TokenType::TOKEN_HASH ||
            token_type == (int32_t)TokenType::TOKEN_DOLLAR) ? 1 : 0;
}

int32_t ast_is_assignment_op(int32_t token_type) {
    return (token_type == (int32_t)TokenType::TOKEN_EQUAL ||
            token_type == (int32_t)TokenType::TOKEN_PLUS_EQUAL ||
            token_type == (int32_t)TokenType::TOKEN_MINUS_EQUAL ||
            token_type == (int32_t)TokenType::TOKEN_STAR_EQUAL ||
            token_type == (int32_t)TokenType::TOKEN_SLASH_EQUAL ||
            token_type == (int32_t)TokenType::TOKEN_PERCENT_EQUAL) ? 1 : 0;
}

// Check if a token type is valid as a name (identifier or contextual keyword)
int32_t ast_is_name_token(int32_t token_type) {
    init_name_kw_table();
    if (token_type == (int32_t)TokenType::TOKEN_IDENTIFIER) return 1;
    if (token_type < 0 || token_type >= 256) return 0;
    return name_kw_table[token_type];
}

// -----------------------------------------------------------------------
// Synchronization keywords (for error recovery)
// -----------------------------------------------------------------------

int32_t ast_is_sync_keyword(int32_t token_type) {
    return (token_type == (int32_t)TokenType::TOKEN_KW_FUNC ||
            token_type == (int32_t)TokenType::TOKEN_KW_IF ||
            token_type == (int32_t)TokenType::TOKEN_KW_ELSE ||
            token_type == (int32_t)TokenType::TOKEN_KW_WHILE ||
            token_type == (int32_t)TokenType::TOKEN_KW_FOR ||
            token_type == (int32_t)TokenType::TOKEN_KW_LOOP ||
            token_type == (int32_t)TokenType::TOKEN_KW_TILL ||
            token_type == (int32_t)TokenType::TOKEN_KW_WHEN ||
            token_type == (int32_t)TokenType::TOKEN_KW_PICK ||
            token_type == (int32_t)TokenType::TOKEN_KW_RETURN ||
            token_type == (int32_t)TokenType::TOKEN_KW_PASS ||
            token_type == (int32_t)TokenType::TOKEN_KW_FAIL ||
            token_type == (int32_t)TokenType::TOKEN_KW_BREAK ||
            token_type == (int32_t)TokenType::TOKEN_KW_CONTINUE ||
            token_type == (int32_t)TokenType::TOKEN_KW_DEFER ||
            token_type == (int32_t)TokenType::TOKEN_KW_USE ||
            token_type == (int32_t)TokenType::TOKEN_KW_MOD ||
            token_type == (int32_t)TokenType::TOKEN_KW_EXTERN ||
            token_type == (int32_t)TokenType::TOKEN_KW_STRUCT ||
            token_type == (int32_t)TokenType::TOKEN_KW_ENUM ||
            token_type == (int32_t)TokenType::TOKEN_KW_TRAIT) ? 1 : 0;
}

// -----------------------------------------------------------------------
// Token type constants
// -----------------------------------------------------------------------

// Expose specific TokenType values as int32_t so Aria code can use them
// without needing to know the C++ enum.
int32_t tk_eof()            { return (int32_t)TokenType::TOKEN_EOF; }
int32_t tk_semicolon()      { return (int32_t)TokenType::TOKEN_SEMICOLON; }
int32_t tk_comma()          { return (int32_t)TokenType::TOKEN_COMMA; }
int32_t tk_colon()          { return (int32_t)TokenType::TOKEN_COLON; }
int32_t tk_left_paren()     { return (int32_t)TokenType::TOKEN_LEFT_PAREN; }
int32_t tk_right_paren()    { return (int32_t)TokenType::TOKEN_RIGHT_PAREN; }
int32_t tk_left_brace()     { return (int32_t)TokenType::TOKEN_LEFT_BRACE; }
int32_t tk_right_brace()    { return (int32_t)TokenType::TOKEN_RIGHT_BRACE; }
int32_t tk_left_bracket()   { return (int32_t)TokenType::TOKEN_LEFT_BRACKET; }
int32_t tk_right_bracket()  { return (int32_t)TokenType::TOKEN_RIGHT_BRACKET; }
int32_t tk_equal()          { return (int32_t)TokenType::TOKEN_EQUAL; }
int32_t tk_less()           { return (int32_t)TokenType::TOKEN_LESS; }
int32_t tk_greater()        { return (int32_t)TokenType::TOKEN_GREATER; }
int32_t tk_dot()            { return (int32_t)TokenType::TOKEN_DOT; }
int32_t tk_arrow()          { return (int32_t)TokenType::TOKEN_ARROW; }
int32_t tk_star()           { return (int32_t)TokenType::TOKEN_STAR; }
int32_t tk_dollar()         { return (int32_t)TokenType::TOKEN_DOLLAR; }
int32_t tk_question()       { return (int32_t)TokenType::TOKEN_QUESTION; }
int32_t tk_bang()           { return (int32_t)TokenType::TOKEN_BANG; }
int32_t tk_hash()           { return (int32_t)TokenType::TOKEN_HASH; }
int32_t tk_identifier()     { return (int32_t)TokenType::TOKEN_IDENTIFIER; }
int32_t tk_fat_arrow()      { return (int32_t)TokenType::TOKEN_FAT_ARROW; }
int32_t tk_safe_nav()       { return (int32_t)TokenType::TOKEN_SAFE_NAV; }
int32_t tk_dot_dot()        { return (int32_t)TokenType::TOKEN_DOT_DOT; }
int32_t tk_dot_dot_dot()    { return (int32_t)TokenType::TOKEN_DOT_DOT_DOT; }
int32_t tk_pipe_right()     { return (int32_t)TokenType::TOKEN_PIPE_RIGHT; }
int32_t tk_pipe_left()      { return (int32_t)TokenType::TOKEN_PIPE_LEFT; }
int32_t tk_null_coalesce()  { return (int32_t)TokenType::TOKEN_NULL_COALESCE; }
int32_t tk_question_bang()  { return (int32_t)TokenType::TOKEN_QUESTION_BANG; }
int32_t tk_plus_plus()      { return (int32_t)TokenType::TOKEN_PLUS_PLUS; }
int32_t tk_minus_minus()    { return (int32_t)TokenType::TOKEN_MINUS_MINUS; }
int32_t tk_bang_bang_bang()  { return (int32_t)TokenType::TOKEN_BANG_BANG_BANG; }
int32_t tk_ampersand()      { return (int32_t)TokenType::TOKEN_AMPERSAND; }
int32_t tk_minus()          { return (int32_t)TokenType::TOKEN_MINUS; }
int32_t tk_left_arrow()     { return (int32_t)TokenType::TOKEN_LEFT_ARROW; }

// -----------------------------------------------------------------------
// Additional query helpers
// -----------------------------------------------------------------------

int32_t ps_expect(void* state, int32_t type, const char* msg) {
    if (ps_check(state, type)) {
        ps_advance(state);
        return 1;
    }
    ps_add_error(state, msg);
    return 0;
}

int32_t ast_is_int_literal(int32_t tok_type) {
    if (tok_type == (int32_t)TokenType::TOKEN_INTEGER) return 1;
    if (tok_type >= (int32_t)TokenType::TOKEN_INTEGER_U8 &&
        tok_type <= (int32_t)TokenType::TOKEN_INTEGER_TBB64) return 1;
    return 0;
}

int32_t ast_is_float_literal(int32_t tok_type) {
    if (tok_type == (int32_t)TokenType::TOKEN_FLOAT) return 1;
    if (tok_type >= (int32_t)TokenType::TOKEN_FLOAT_F32 &&
        tok_type <= (int32_t)TokenType::TOKEN_FLOAT_FIX256) return 1;
    return 0;
}

int32_t ast_is_memory_qualifier(int32_t tok_type) {
    return (tok_type == (int32_t)TokenType::TOKEN_KW_WILD ||
            tok_type == (int32_t)TokenType::TOKEN_KW_WILDX ||
            tok_type == (int32_t)TokenType::TOKEN_KW_STACK ||
            tok_type == (int32_t)TokenType::TOKEN_KW_GC) ? 1 : 0;
}

int32_t ast_is_postfix_op(int32_t tok_type) {
    return (tok_type == (int32_t)TokenType::TOKEN_DOT ||
            tok_type == (int32_t)TokenType::TOKEN_ARROW ||
            tok_type == (int32_t)TokenType::TOKEN_SAFE_NAV ||
            tok_type == (int32_t)TokenType::TOKEN_LEFT_PAREN ||
            tok_type == (int32_t)TokenType::TOKEN_LEFT_BRACKET ||
            tok_type == (int32_t)TokenType::TOKEN_PLUS_PLUS ||
            tok_type == (int32_t)TokenType::TOKEN_MINUS_MINUS ||
            tok_type == (int32_t)TokenType::TOKEN_QUESTION) ? 1 : 0;
}

void ps_synchronize(void* state) {
    while (!ps_is_at_end(state)) {
        // Stop at semicolons (consume them)
        if (ps_check(state, (int32_t)TokenType::TOKEN_SEMICOLON)) {
            ps_advance(state);
            return;
        }
        // Stop before sync keywords
        if (ast_is_sync_keyword(ps_peek(state))) {
            return;
        }
        ps_advance(state);
    }
}

// Error message building helper — concat two strings in C
// (Workaround: Aria codegen can't do string concat)
AriaString* ast_concat_str(const char* a, const char* b) {
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    char* buf = (char*)malloc(la + lb + 1);
    if (la > 0) memcpy(buf, a, la);
    if (lb > 0) memcpy(buf + la, b, lb);
    buf[la + lb] = '\0';
    AriaString* result = make_str(buf, (int64_t)(la + lb));
    free(buf);
    return result;
}

// Concat three strings
AriaString* ast_concat_str3(const char* a, const char* b, const char* c) {
    size_t la = a ? strlen(a) : 0;
    size_t lb = b ? strlen(b) : 0;
    size_t lc = c ? strlen(c) : 0;
    char* buf = (char*)malloc(la + lb + lc + 1);
    if (la > 0) memcpy(buf, a, la);
    if (lb > 0) memcpy(buf + la, b, lb);
    if (lc > 0) memcpy(buf + la + lb, c, lc);
    buf[la + lb + lc] = '\0';
    AriaString* result = make_str(buf, (int64_t)(la + lb + lc));
    free(buf);
    return result;
}

// ---------------------------------------------------------------------------
// Scalar AST query helpers — avoid returning pointers cross-module
// ---------------------------------------------------------------------------

static ASTNode* walk_child(void* node, int64_t idx) {
    ASTNode* n = (ASTNode*)node;
    if (!n || idx < 0 || idx >= n->child_count) return nullptr;
    return n->children[idx];
}

// Single-level child queries
int32_t ast_child_type(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    return c ? c->type : -1;
}

AriaString* ast_child_str(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    if (!c || !c->str_val) return make_str("", 0);
    return make_str(c->str_val, (int64_t)strlen(c->str_val));
}

int64_t ast_child_int(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    return c ? c->int_val : 0;
}

int32_t ast_child_bool(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    return c ? c->bool_val : 0;
}

int32_t ast_child_flags(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    return c ? c->flags : 0;
}

int64_t ast_child_child_count(void* node, int64_t idx) {
    ASTNode* c = walk_child(node, idx);
    return c ? c->child_count : 0;
}

// Two-level child queries (node -> child[i] -> child[j])
int32_t ast_child2_type(void* node, int64_t i, int64_t j) {
    ASTNode* c = walk_child(node, i);
    if (!c) return -1;
    ASTNode* cc = walk_child(c, j);
    return cc ? cc->type : -1;
}

AriaString* ast_child2_str(void* node, int64_t i, int64_t j) {
    ASTNode* c = walk_child(node, i);
    if (!c) return make_str("", 0);
    ASTNode* cc = walk_child(c, j);
    if (!cc || !cc->str_val) return make_str("", 0);
    return make_str(cc->str_val, (int64_t)strlen(cc->str_val));
}

int64_t ast_child2_child_count(void* node, int64_t i, int64_t j) {
    ASTNode* c = walk_child(node, i);
    if (!c) return 0;
    ASTNode* cc = walk_child(c, j);
    return cc ? cc->child_count : 0;
}

int32_t ast_child2_bool(void* node, int64_t i, int64_t j) {
    ASTNode* c = walk_child(node, i);
    if (!c) return 0;
    ASTNode* cc = walk_child(c, j);
    return cc ? cc->bool_val : 0;
}

int64_t ast_child2_int(void* node, int64_t i, int64_t j) {
    ASTNode* c = walk_child(node, i);
    if (!c) return 0;
    ASTNode* cc = walk_child(c, j);
    return cc ? cc->int_val : 0;
}

// Three-level child queries (node -> child[i] -> child[j] -> child[k])
int32_t ast_child3_type(void* node, int64_t i, int64_t j, int64_t k) {
    ASTNode* c = walk_child(node, i);
    if (!c) return -1;
    ASTNode* cc = walk_child(c, j);
    if (!cc) return -1;
    ASTNode* ccc = walk_child(cc, k);
    return ccc ? ccc->type : -1;
}

} // extern "C"
