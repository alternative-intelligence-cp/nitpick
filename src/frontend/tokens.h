#ifndef ARIA_FRONTEND_TOKENS_H
#define ARIA_FRONTEND_TOKENS_H

#include <string>
#include <cstdint>

namespace aria {
namespace frontend {

// Token Types
// Based on Aria Language Specification v0.0.6
enum TokenType {
    // Special Tokens
    TOKEN_EOF,
    TOKEN_INVALID,
    TOKEN_UNKNOWN,

    // Literals
    TOKEN_INT_LITERAL,
    TOKEN_FLOAT_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,
    TOKEN_TRIT_LITERAL,     // Ternary digit: -1, 0, 1 (BALANCED ternary)
    TOKEN_TEMPLATE_LITERAL, // Template literal with interpolation

    // Identifiers and Keywords
    TOKEN_IDENTIFIER,
    TOKEN_TYPE_IDENTIFIER,  // Type identifier (for user-defined types)
    TOKEN_KW_FUNC,
    TOKEN_KW_RETURN,
    TOKEN_KW_IF,
    TOKEN_KW_ELSE,
    TOKEN_KW_PICK,          // Pattern matching
    TOKEN_KW_WHEN,          // When loop
    TOKEN_KW_TILL,          // Till loop
    TOKEN_KW_DEFER,
    TOKEN_KW_WILD,          // Wild heap allocation
    TOKEN_KW_STACK,         // Stack allocation
    TOKEN_KW_GC,            // GC-managed allocation (default)
    TOKEN_WILD = TOKEN_KW_WILD,    // Alias for parser compatibility
    TOKEN_STACK = TOKEN_KW_STACK,  // Alias for parser compatibility
    TOKEN_GC = TOKEN_KW_GC,        // Alias for parser compatibility
    TOKEN_KW_PIN,           // Pin to nursery
    TOKEN_KW_UNPIN,         // Unpin from nursery
    TOKEN_KW_RESULT,        // Result<T> type
    TOKEN_KW_STRUCT,
    TOKEN_KW_ENUM,
    TOKEN_KW_TYPE,
    TOKEN_KW_IMPORT,
    TOKEN_KW_PUB,           // Public
    TOKEN_KW_USE,           // Import modules
    TOKEN_KW_MOD,           // Define module
    TOKEN_KW_EXTERN,        // External C functions
    TOKEN_KW_CFG,           // Conditional compilation
    TOKEN_KW_CONST,         // Compile-time constant
    // Primitive Types
    TOKEN_TYPE_VOID,
    TOKEN_TYPE_BOOL,
    
    // Integer Types (Signed)
    TOKEN_TYPE_INT1,
    TOKEN_TYPE_INT2,
    TOKEN_TYPE_INT4,
    TOKEN_TYPE_INT8,
    TOKEN_TYPE_INT16,
    TOKEN_TYPE_INT32,
    TOKEN_TYPE_INT64,
    TOKEN_TYPE_INT128,
    TOKEN_TYPE_INT256,
    TOKEN_TYPE_INT512,
    
    // Integer Types (Unsigned)
    TOKEN_TYPE_UINT8,
    // Operators - Arithmetic
    TOKEN_PLUS,             // +
    TOKEN_MINUS,            // -
    TOKEN_STAR,             // * (also TOKEN_MULTIPLY per spec)
    TOKEN_SLASH,            // / (also TOKEN_DIVIDE per spec)
    TOKEN_PERCENT,          // % (also TOKEN_MODULO per spec)
    TOKEN_INCREMENT,        // ++
    TOKEN_DECREMENT,        // --
    
    // Operators - Bitwise
    TOKEN_AMPERSAND,        // &
    TOKEN_PIPE,             // |
    TOKEN_CARET,            // ^
    TOKEN_TILDE,            // ~
    TOKEN_LSHIFT,           // <<
    TOKEN_RSHIFT,           // >>
    
    // Operators - Comparison
    TOKEN_EQ,               // ==
    TOKEN_NE,               // !=
    TOKEN_LT,               // <
    TOKEN_GT,               // >
    TOKEN_LE,               // <=
    TOKEN_GE,               // >=
    TOKEN_SPACESHIP,        // <=> (three-way comparison)
    
    // Operators - Logical
    TOKEN_LOGICAL_AND,      // &&
    TOKEN_LOGICAL_OR,       // ||
    TOKEN_LOGICAL_NOT,      // !
    
    // Operators - Assignment
    TOKEN_ASSIGN,           // =
    TOKEN_PLUS_ASSIGN,      // +=
    TOKEN_MINUS_ASSIGN,     // -=
    TOKEN_STAR_ASSIGN,      // *= (also TOKEN_MULT_ASSIGN per spec)
    TOKEN_SLASH_ASSIGN,     // /= (also TOKEN_DIV_ASSIGN per spec)
    TOKEN_MOD_ASSIGN,       // %=
    
    // Operators - Special
    TOKEN_ARROW,            // -> (also TOKEN_FUNC_RETURN per spec)
    TOKEN_FAT_ARROW,        // => (also TOKEN_LAMBDA_ARROW per spec)
    TOKEN_DOUBLE_COLON,     // ::
    TOKEN_AT,               // @ (also TOKEN_ADDRESS per spec)
    TOKEN_HASH,             // # (also TOKEN_PIN per spec)
    TOKEN_DOLLAR,           // $ (also TOKEN_ITERATION per spec)
    TOKEN_QUESTION,         // ? (base question mark)
    TOKEN_UNWRAP,           // ? (unwrap operator - context dependent)
    TOKEN_SAFE_NAV,         // ?. (safe navigation)
    TOKEN_NULL_COALESCE,    // ?? (null coalescing)
    TOKEN_PIPE_FORWARD,     // |> (pipeline forward)
    TOKEN_PIPE_BACKWARD,    // <| (pipeline backward)
    TOKEN_TYPE_MATRIX,      // Matrix type
    TOKEN_TYPE_FUNC,        // Function type
    TOKEN_TYPE_RESULT,      // Result<T,E> type
    
    // System Types
    TOKEN_TYPE_BINARY,      // Binary data
    TOKEN_COLON,            // :
    TOKEN_DOT,              // .
    TOKEN_RANGE,            // .. (range operator - inclusive)
    TOKEN_RANGE_EXCLUSIVE,  // ... (range operator - exclusive)

    // String Template Tokens
    TOKEN_BACKTICK,         // `
    TOKEN_INTERP_START,     // &{
    TOKEN_STRING_CONTENT,   // String content between interpolations
    
    // Spec-Compliant Aliases (for compatibility with spec terminology)
    TOKEN_MULTIPLY = TOKEN_STAR,
    TOKEN_DIVIDE = TOKEN_SLASH,
    TOKEN_MODULO = TOKEN_PERCENT,
    TOKEN_MULT_ASSIGN = TOKEN_STAR_ASSIGN,
    TOKEN_DIV_ASSIGN = TOKEN_SLASH_ASSIGN,
    TOKEN_ADDRESS = TOKEN_AT,
    TOKEN_PIN = TOKEN_HASH,
    TOKEN_ITERATION = TOKEN_DOLLAR,
    TOKEN_LAMBDA_ARROW = TOKEN_FAT_ARROW,
    TOKEN_FUNC_RETURN = TOKEN_ARROW,
    TOKEN_TERNARY_IS = TOKEN_KW_IS,
    TOKEN_LEFT_PAREN = TOKEN_LPAREN,
    TOKEN_RIGHT_PAREN = TOKEN_RPAREN,
    TOKEN_LEFT_BRACE = TOKEN_LBRACE,
    TOKEN_RIGHT_BRACE = TOKEN_RBRACE,
    TOKEN_LEFT_BRACKET = TOKEN_LBRACKET,
    TOKEN_RIGHT_BRACKET = TOKEN_RBRACKET
};

// Token Structure
// Represents a single lexical token with location information
struct Token {
    TokenType type;
    std::string value;      // Literal value or identifier name
    size_t line;
    size_t col;

    Token() : type(TOKEN_INVALID), value(""), line(0), col(0) {}

    Token(TokenType t, const std::string& v, size_t l, size_t c)
        : type(t), value(v), line(l), col(c) {}
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_TOKENS_H
