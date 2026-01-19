/**
 * lexer.hpp
 * Lexer for Aria Build Configuration (ABC) Parser
 *
 * Tokenizes build.aria files with:
 * - Zero-copy string_view tokens referencing source buffer
 * - Line/column tracking for error reporting
 * - Support for Aria-style comments (//)
 * - Unquoted identifier keys
 * - Efficient single-pass scanning
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_LEXER_HPP
#define ARIA_ABC_LEXER_HPP

#include "token.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// Lexer Error
// -----------------------------------------------------------------------------
struct LexerError {
    std::string message;
    SourceLocation loc;
};

// -----------------------------------------------------------------------------
// Lexer
// -----------------------------------------------------------------------------
class Lexer {
public:
    /**
     * Construct lexer with source buffer.
     * The source must remain valid for the lifetime of the lexer.
     */
    explicit Lexer(std::string_view source);

    // No copying
    Lexer(const Lexer&) = delete;
    Lexer& operator=(const Lexer&) = delete;

    /**
     * Peek at the current token without consuming it.
     * Multiple calls return the same token until advance() is called.
     */
    const Token& peek();

    /**
     * Consume the current token and advance to the next.
     * Returns the consumed token.
     */
    Token advance();

    /**
     * Check if we've reached the end of input.
     */
    bool is_at_end() const;

    /**
     * Get all lexer errors accumulated so far.
     */
    const std::vector<LexerError>& errors() const { return errors_; }

    /**
     * Check if any errors occurred.
     */
    bool has_errors() const { return !errors_.empty(); }

    /**
     * Get the source being lexed.
     */
    std::string_view source() const { return source_; }

    /**
     * Get current position in source.
     */
    size_t position() const { return current_; }

    /**
     * Get current line number (1-indexed).
     */
    size_t line() const { return line_; }

    /**
     * Get current column number (1-indexed).
     */
    size_t column() const { return column_; }

private:
    // Scan the next token
    Token scan_token();

    // Character navigation
    char current_char() const;
    char peek_char() const;
    char peek_char(size_t offset) const;
    char consume_char();
    bool match_char(char expected);

    // Skip whitespace and comments
    void skip_whitespace_and_comments();

    // Token scanners
    Token scan_string();
    Token scan_identifier();
    Token scan_number();

    // Helpers
    bool is_identifier_start(char c) const;
    bool is_identifier_char(char c) const;
    bool is_digit(char c) const;

    // Create token from current range
    Token make_token(TokenType type, size_t start);

    // Error handling
    Token error_token(const std::string& message);
    void add_error(const std::string& message);

private:
    std::string_view source_;
    size_t current_ = 0;        // Current position in source
    size_t start_ = 0;          // Start of current token
    size_t line_ = 1;           // Current line (1-indexed)
    size_t column_ = 1;         // Current column (1-indexed)
    size_t start_line_ = 1;     // Line at token start
    size_t start_column_ = 1;   // Column at token start

    Token current_token_;       // Current peeked token
    bool has_current_ = false;  // Whether current_token_ is valid

    std::vector<LexerError> errors_;
};

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_LEXER_HPP
