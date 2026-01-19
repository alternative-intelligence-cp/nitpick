/**
 * Aria Build Configuration (ABC) Lexer Implementation
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 */

#include "abc/abc_lexer.hpp"
#include <sstream>

namespace abc {

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::LEFT_BRACE:     return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE:    return "RIGHT_BRACE";
        case TokenType::LEFT_BRACKET:   return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET:  return "RIGHT_BRACKET";
        case TokenType::COLON:          return "COLON";
        case TokenType::COMMA:          return "COMMA";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::IDENTIFIER:     return "IDENTIFIER";
        case TokenType::INTEGER:        return "INTEGER";
        case TokenType::BOOLEAN_TRUE:   return "BOOLEAN_TRUE";
        case TokenType::BOOLEAN_FALSE:  return "BOOLEAN_FALSE";
        case TokenType::NULL_LITERAL:   return "NULL_LITERAL";
        case TokenType::INTERP_START:   return "INTERP_START";
        case TokenType::INTERP_END:     return "INTERP_END";
        case TokenType::END_OF_FILE:    return "END_OF_FILE";
        case TokenType::INVALID:        return "INVALID";
    }
    return "UNKNOWN";
}

Lexer::Lexer(std::string_view source, std::string_view filename)
    : source(source), filename(filename) {}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.size()) return '\0';
    return source[current + 1];
}

char Lexer::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    advance();
    return true;
}

bool Lexer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    skipComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipComment() {
    // Skip the //
    advance();
    advance();

    // Skip until end of line
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

Token Lexer::makeToken(TokenType type) {
    std::string_view lexeme = source.substr(start, current - start);
    return Token(type, lexeme, line, column - (current - start));
}

Token Lexer::makeToken(TokenType type, std::string_view lexeme) {
    return Token(type, lexeme, line, column - lexeme.size());
}

Token Lexer::errorToken(const char* message) {
    std::ostringstream oss;
    oss << filename << ":" << line << ":" << column << ": error: " << message;
    errors.push_back(oss.str());
    return Token(TokenType::INVALID, message, line, column);
}

Token Lexer::peekToken() {
    if (!hasPeeked) {
        peekedToken = nextToken();
        hasPeeked = true;
    }
    return peekedToken;
}

Token Lexer::nextToken() {
    if (hasPeeked) {
        hasPeeked = false;
        return peekedToken;
    }
    return scanToken();
}

Token Lexer::scanToken() {
    skipWhitespace();

    start = current;

    if (isAtEnd()) {
        return makeToken(TokenType::END_OF_FILE, "");
    }

    char c = advance();

    // Structural tokens
    switch (c) {
        case '{': return makeToken(TokenType::LEFT_BRACE);
        case '}': return makeToken(TokenType::RIGHT_BRACE);
        case '[': return makeToken(TokenType::LEFT_BRACKET);
        case ']': return makeToken(TokenType::RIGHT_BRACKET);
        case ':': return makeToken(TokenType::COLON);
        case ',': return makeToken(TokenType::COMMA);
        case '`': return scanString();
    }

    // Identifiers and keywords
    if (isAlpha(c)) {
        return scanIdentifier();
    }

    // Numbers (including negative)
    if (isDigit(c) || (c == '-' && isDigit(peek()))) {
        return scanNumber();
    }

    return errorToken("Unexpected character");
}

Token Lexer::scanString() {
    // We're inside a backtick string
    // Need to handle interpolation &{...}

    std::vector<Token> segments;
    size_t stringStart = current;
    uint32_t startLine = line;
    uint32_t startColumn = column;

    while (!isAtEnd() && peek() != '`') {
        // Check for interpolation
        if (peek() == '&' && peekNext() == '{') {
            // If we have accumulated string content, return it first
            if (current > stringStart) {
                std::string_view content = source.substr(stringStart, current - stringStart);
                return Token(TokenType::STRING_LITERAL, content, startLine, startColumn);
            }

            // Emit INTERP_START
            advance(); // &
            advance(); // {
            return Token(TokenType::INTERP_START, "&{", line, column - 2);
        }

        // Check for closing interpolation brace
        if (peek() == '}' && state == LexState::INTERPOLATION) {
            advance();
            state = LexState::STRING;
            return Token(TokenType::INTERP_END, "}", line, column - 1);
        }

        advance();
    }

    if (isAtEnd()) {
        return errorToken("Unterminated string");
    }

    // Consume closing backtick
    std::string_view content = source.substr(stringStart, current - stringStart);
    advance(); // `

    return Token(TokenType::STRING_LITERAL, content, startLine, startColumn);
}

Token Lexer::scanIdentifier() {
    while (isAlphaNumeric(peek()) || peek() == '.') {
        advance();
    }

    std::string_view text = source.substr(start, current - start);

    // Check for keywords
    if (text == "true") return makeToken(TokenType::BOOLEAN_TRUE);
    if (text == "false") return makeToken(TokenType::BOOLEAN_FALSE);
    if (text == "null") return makeToken(TokenType::NULL_LITERAL);

    return makeToken(TokenType::IDENTIFIER);
}

Token Lexer::scanNumber() {
    // Handle negative numbers
    if (source[start] == '-') {
        // Already consumed the '-'
    }

    while (isDigit(peek())) {
        advance();
    }

    return makeToken(TokenType::INTEGER);
}

} // namespace abc
