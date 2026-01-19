/**
 * token.hpp
 * Token Types for Aria Build Configuration (ABC) Parser
 *
 * Defines the lexical vocabulary for build.aria files.
 * Supports JSON-like syntax with Aria extensions:
 * - Unquoted identifier keys
 * - Trailing commas
 * - // line comments
 * - &{var} interpolation (detected, resolved later)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_TOKEN_HPP
#define ARIA_ABC_TOKEN_HPP

#include <string>
#include <string_view>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// Token Types
// -----------------------------------------------------------------------------
enum class TokenType {
    // Structural delimiters
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    COLON,          // :
    COMMA,          // ,

    // Literals
    IDENTIFIER,     // Unquoted key: name, sources, depends_on
    STRING,         // "quoted string value"
    INTEGER,        // 42, -1, 0
    TRUE_LITERAL,   // true
    FALSE_LITERAL,  // false

    // Special
    END_OF_FILE,    // End of input
    ERROR           // Lexical error
};

// Convert token type to string for diagnostics
const char* token_type_string(TokenType type);

// -----------------------------------------------------------------------------
// Source Location
// -----------------------------------------------------------------------------
struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
    size_t offset = 0;  // Byte offset into source

    std::string to_string() const;
};

// -----------------------------------------------------------------------------
// Token
// -----------------------------------------------------------------------------
struct Token {
    TokenType type;
    std::string_view text;      // Zero-copy reference into source buffer
    SourceLocation loc;
    std::string error_message;  // Only set when type == ERROR

    Token() : type(TokenType::END_OF_FILE) {}

    Token(TokenType t, std::string_view txt, SourceLocation l)
        : type(t), text(txt), loc(l) {}

    Token(TokenType t, std::string_view txt, size_t line, size_t col, size_t offset = 0)
        : type(t), text(txt), loc{line, col, offset} {}

    // Create an error token
    static Token make_error(const std::string& msg, SourceLocation loc) {
        Token t;
        t.type = TokenType::ERROR;
        t.loc = loc;
        t.error_message = msg;
        return t;
    }

    // Check if this is an error token
    bool is_error() const { return type == TokenType::ERROR; }

    // Check if this is end of file
    bool is_eof() const { return type == TokenType::END_OF_FILE; }
};

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_TOKEN_HPP
