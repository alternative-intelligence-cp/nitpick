/**
 * lexer.cpp
 * Lexer implementation for ABC Parser
 *
 * Efficient single-pass tokenization with:
 * - Zero-copy string_view tokens
 * - Full line/column tracking
 * - // comment support
 * - Unquoted identifier keys
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "abc/lexer.hpp"
#include <cctype>

namespace aria {
namespace abc {

Lexer::Lexer(std::string_view source)
    : source_(source) {}

const Token& Lexer::peek() {
    if (!has_current_) {
        current_token_ = scan_token();
        has_current_ = true;
    }
    return current_token_;
}

Token Lexer::advance() {
    if (!has_current_) {
        current_token_ = scan_token();
    }
    has_current_ = false;
    return current_token_;
}

bool Lexer::is_at_end() const {
    return current_ >= source_.size();
}

char Lexer::current_char() const {
    if (is_at_end()) return '\0';
    return source_[current_];
}

char Lexer::peek_char() const {
    return current_char();
}

char Lexer::peek_char(size_t offset) const {
    if (current_ + offset >= source_.size()) return '\0';
    return source_[current_ + offset];
}

char Lexer::consume_char() {
    char c = current_char();
    current_++;
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match_char(char expected) {
    if (is_at_end() || current_char() != expected) return false;
    consume_char();
    return true;
}

void Lexer::skip_whitespace_and_comments() {
    while (!is_at_end()) {
        char c = current_char();

        // Whitespace
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            consume_char();
            continue;
        }

        // Line comment: //
        if (c == '/' && peek_char(1) == '/') {
            // Consume until end of line
            while (!is_at_end() && current_char() != '\n') {
                consume_char();
            }
            continue;
        }

        // Not whitespace or comment
        break;
    }
}

Token Lexer::scan_token() {
    skip_whitespace_and_comments();

    start_ = current_;
    start_line_ = line_;
    start_column_ = column_;

    if (is_at_end()) {
        return Token(TokenType::END_OF_FILE, "", start_line_, start_column_, start_);
    }

    char c = consume_char();

    // Single-character tokens
    switch (c) {
        case '{': return make_token(TokenType::LBRACE, start_);
        case '}': return make_token(TokenType::RBRACE, start_);
        case '[': return make_token(TokenType::LBRACKET, start_);
        case ']': return make_token(TokenType::RBRACKET, start_);
        case ':': return make_token(TokenType::COLON, start_);
        case ',': return make_token(TokenType::COMMA, start_);
    }

    // String literal
    if (c == '"') {
        return scan_string();
    }

    // Number
    if (is_digit(c) || (c == '-' && is_digit(peek_char()))) {
        return scan_number();
    }

    // Identifier or keyword
    if (is_identifier_start(c)) {
        return scan_identifier();
    }

    // Unknown character
    return error_token(std::string("Unexpected character '") + c + "'");
}

Token Lexer::scan_string() {
    // Opening quote already consumed
    while (!is_at_end() && current_char() != '"') {
        if (current_char() == '\\') {
            // Escape sequence - consume backslash and next char
            consume_char();
            if (!is_at_end()) {
                consume_char();
            }
        } else if (current_char() == '\n') {
            // Unterminated string at newline
            add_error("Unterminated string literal");
            return error_token("Unterminated string literal");
        } else {
            consume_char();
        }
    }

    if (is_at_end()) {
        add_error("Unterminated string literal");
        return error_token("Unterminated string literal");
    }

    // Consume closing quote
    consume_char();

    return make_token(TokenType::STRING, start_);
}

Token Lexer::scan_identifier() {
    // First char already consumed
    while (!is_at_end() && is_identifier_char(current_char())) {
        consume_char();
    }

    std::string_view text = source_.substr(start_, current_ - start_);

    // Check for keywords
    TokenType type = TokenType::IDENTIFIER;
    if (text == "true") {
        type = TokenType::TRUE_LITERAL;
    } else if (text == "false") {
        type = TokenType::FALSE_LITERAL;
    }

    return make_token(type, start_);
}

Token Lexer::scan_number() {
    // First char (digit or minus) already consumed
    while (!is_at_end() && is_digit(current_char())) {
        consume_char();
    }

    return make_token(TokenType::INTEGER, start_);
}

bool Lexer::is_identifier_start(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::is_identifier_char(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool Lexer::is_digit(char c) const {
    return c >= '0' && c <= '9';
}

Token Lexer::make_token(TokenType type, size_t start) {
    std::string_view text = source_.substr(start, current_ - start);
    return Token(type, text, start_line_, start_column_, start);
}

Token Lexer::error_token(const std::string& message) {
    Token t;
    t.type = TokenType::ERROR;
    t.loc = {start_line_, start_column_, start_};
    t.error_message = message;
    return t;
}

void Lexer::add_error(const std::string& message) {
    errors_.push_back({message, {start_line_, start_column_, start_}});
}

} // namespace abc
} // namespace aria
