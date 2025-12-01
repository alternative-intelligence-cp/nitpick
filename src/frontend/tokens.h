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
    TOKEN_KW_WHEN,          // When loop/expression
    TOKEN_KW_THEN,          // Then clause for when
    TOKEN_KW_END,           // End clause for when
    TOKEN_KW_TILL,          // Till loop
    TOKEN_KW_FOR,           // For loop
    TOKEN_KW_WHILE,         // While loop
    TOKEN_KW_BREAK,         // Break from loop
    TOKEN_KW_CONTINUE,      // Continue to next iteration
    TOKEN_KW_FALL,          // Fallthrough in pick
    TOKEN_KW_ASYNC,         // Async function
    TOKEN_KW_AWAIT,         // Await async result
    TOKEN_KW_CATCH,         // Catch clause
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
    TOKEN_KW_MUT,           // Mutable
    TOKEN_KW_IMPORT,
    TOKEN_KW_EXPORT,
    TOKEN_KW_PUB,           // Public
    TOKEN_KW_USE,           // Import modules
    TOKEN_KW_MOD,           // Define module
    TOKEN_KW_EXTERN,        // External C functions
    TOKEN_KW_CFG,           // Conditional compilation
    TOKEN_KW_CONST,         // Compile-time constant
    TOKEN_KW_IS,            // Ternary is operator
    TOKEN_KW_IN,            // For-in iterator
    TOKEN_KW_TRUE,
    TOKEN_KW_FALSE,
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
    TOKEN_TYPE_UINT16,
    TOKEN_TYPE_UINT32,
    TOKEN_TYPE_UINT64,
    TOKEN_TYPE_UINT128,
    TOKEN_TYPE_UINT256,
    TOKEN_TYPE_UINT512,
    
    // Floating Point Types
    TOKEN_TYPE_FLT32,
    TOKEN_TYPE_FLT64,
    TOKEN_TYPE_FLT128,
    TOKEN_TYPE_FLT256,
    TOKEN_TYPE_FLT512,
    
    // Exotic Types (Ternary and Nonary)
    TOKEN_TYPE_TRIT,        // Ternary digit (-1, 0, 1)
    TOKEN_TYPE_TRYTE,       // 6 trits (ternary byte)
    TOKEN_TYPE_NIT,         // Nonary digit (0-8)
    TOKEN_TYPE_NYTE,        // Nonary byte
    TOKEN_TYPE_BYTE,        // Standard byte (uint8 alias)
    
    // Vector Types
    TOKEN_TYPE_VEC2,        // 2D vector
    TOKEN_TYPE_VEC3,        // 3D vector
    TOKEN_TYPE_VEC9,        // 9D vector
    
    // Compound Types
    TOKEN_TYPE_MATRIX,      // Matrix type
    TOKEN_TYPE_TENSOR,      // Tensor type
    TOKEN_TYPE_FUNC,        // Function type
    TOKEN_TYPE_RESULT,      // Result<T,E> type
    TOKEN_TYPE_BINARY,      // Binary data
    TOKEN_TYPE_BUFFER,      // Buffer type
    TOKEN_TYPE_STREAM,      // Stream type
    TOKEN_TYPE_PROCESS,     // Process type
    TOKEN_TYPE_PIPE,        // Pipe type
    TOKEN_TYPE_DYN,         // Dynamic type
    TOKEN_TYPE_OBJ,         // Object type
    TOKEN_TYPE_ARRAY,       // Array type
    TOKEN_TYPE_STRING,      // String type
    
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
    
    // Delimiters and Punctuation
    TOKEN_COLON,            // :
    TOKEN_DOT,              // .
    TOKEN_RANGE,            // .. (range operator - inclusive)
    TOKEN_RANGE_EXCLUSIVE,  // ... (range operator - exclusive)

    // String Template Tokens
    TOKEN_BACKTICK,         // `
    TOKEN_INTERP_START,     // &{
    TOKEN_STRING_CONTENT,   // String content between interpolations
    
    // Delimiters
    TOKEN_LPAREN,           // (
    TOKEN_RPAREN,           // )
    TOKEN_LBRACE,           // {
    TOKEN_RBRACE,           // }
    TOKEN_LBRACKET,         // [
    TOKEN_RBRACKET,         // ]
    TOKEN_COMMA,            // ,
    TOKEN_SEMICOLON,        // ;
    
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
