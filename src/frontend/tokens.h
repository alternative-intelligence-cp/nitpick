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
    TOKEN_TRIT_LITERAL,     // Ternary digit: 0, 1, 2

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
    TOKEN_WILD = TOKEN_KW_WILD,    // Alias for parser compatibility
    TOKEN_STACK = TOKEN_KW_STACK,  // Alias for parser compatibility
    TOKEN_KW_PIN,           // Pin to nursery
    TOKEN_KW_UNPIN,         // Unpin from nursery
    TOKEN_KW_RESULT,        // Result<T> type
    TOKEN_KW_STRUCT,
    TOKEN_KW_ENUM,
    TOKEN_KW_TYPE,
    TOKEN_KW_IMPORT,
    TOKEN_KW_EXPORT,
    TOKEN_KW_MUT,           // Mutable
    TOKEN_KW_PUB,           // Public
    TOKEN_KW_TRUE,          // Boolean literal: true
    TOKEN_KW_FALSE,         // Boolean literal: false

    // Primitive Types
    TOKEN_TYPE_VOID,
    TOKEN_TYPE_BOOL,
    TOKEN_TYPE_INT1,
    TOKEN_TYPE_INT8,
    TOKEN_TYPE_INT16,
    TOKEN_TYPE_INT32,
    TOKEN_TYPE_INT64,
    TOKEN_TYPE_INT128,
    TOKEN_TYPE_INT256,
    TOKEN_TYPE_INT512,
    TOKEN_TYPE_TRIT,        // Ternary digit type
    TOKEN_TYPE_TRYTE,       // 6-trit (9-bit) type
    TOKEN_TYPE_BYTE,
    TOKEN_TYPE_FLT32,
    TOKEN_TYPE_FLT64,
    TOKEN_TYPE_STRING,

    // Operators
    TOKEN_PLUS,             // +
    TOKEN_MINUS,            // -
    TOKEN_STAR,             // *
    TOKEN_SLASH,            // /
    TOKEN_PERCENT,          // %
    TOKEN_AMPERSAND,        // &
    TOKEN_PIPE,             // |
    TOKEN_CARET,            // ^
    TOKEN_TILDE,            // ~
    TOKEN_LSHIFT,           // <<
    TOKEN_RSHIFT,           // >>
    TOKEN_EQ,               // ==
    TOKEN_NE,               // !=
    TOKEN_LT,               // <
    TOKEN_GT,               // >
    TOKEN_LE,               // <=
    TOKEN_GE,               // >=
    TOKEN_LOGICAL_AND,      // &&
    TOKEN_LOGICAL_OR,       // ||
    TOKEN_LOGICAL_NOT,      // !
    TOKEN_ASSIGN,           // =
    TOKEN_PLUS_ASSIGN,      // +=
    TOKEN_MINUS_ASSIGN,     // -=
    TOKEN_STAR_ASSIGN,      // *=
    TOKEN_SLASH_ASSIGN,     // /=
    TOKEN_ARROW,            // ->
    TOKEN_FAT_ARROW,        // =>
    TOKEN_DOUBLE_COLON,     // ::
    TOKEN_AT,               // @ (address-of for pinned, or directive)
    TOKEN_DOLLAR,           // $ (iterator variable)
    TOKEN_QUESTION,         // ? (result type)

    // Delimiters
    TOKEN_LPAREN,           // (
    TOKEN_RPAREN,           // )
    TOKEN_LBRACE,           // {
    TOKEN_RBRACE,           // }
    TOKEN_LBRACKET,         // [
    TOKEN_RBRACKET,         // ]
    TOKEN_COMMA,            // ,
    TOKEN_SEMICOLON,        // ;
    TOKEN_COLON,            // :
    TOKEN_DOT,              // .
    TOKEN_RANGE,            // .. (range operator)

    // String Template Tokens
    TOKEN_BACKTICK,         // `
    TOKEN_INTERP_START,     // &{
    TOKEN_STRING_CONTENT,   // String content between interpolations
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
