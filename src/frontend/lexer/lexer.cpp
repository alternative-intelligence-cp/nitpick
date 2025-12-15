#include "frontend/lexer/lexer.h"
#include <sstream>

namespace aria {
namespace frontend {

// ============================================================================
// Constructor and Main Tokenization
// ============================================================================

Lexer::Lexer(const std::string& source)
    : source(source), current(0), start(0), line(1), column(1) {
}

std::vector<Token> Lexer::tokenize() {
    tokens.clear();
    errors.clear();
    
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    
    // Add EOF token
    tokens.push_back(Token(TokenType::TOKEN_EOF, "", line, column));
    
    return tokens;
}

const std::vector<std::string>& Lexer::getErrors() const {
    return errors;
}

// ============================================================================
// Character Navigation Methods
// ============================================================================

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    
    char c = source[current];
    current++;
    column++;
    
    // Track newlines for line/column counting
    if (c == '\n') {
        line++;
        column = 1;
    }
    
    return c;
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Lexer::peekNext() const {
    if (current + 1 >= source.length()) return '\0';
    return source[current + 1];
}

bool Lexer::isAtEnd() const {
    return current >= source.length();
}

bool Lexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    
    advance();
    return true;
}

// ============================================================================
// Whitespace and Comment Handling
// ============================================================================

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
            case '\n':
                advance();
                break;
            case '/':
                // Check for comments
                if (peekNext() == '/') {
                    skipLineComment();
                } else if (peekNext() == '*') {
                    skipBlockComment();
                } else {
                    return; // Not a comment, stop skipping
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip the //
    advance();
    advance();
    
    // Skip until end of line or end of file
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    // Skip the /*
    advance();
    advance();
    
    int startLine = line;
    
    // Skip until we find */
    while (!isAtEnd()) {
        if (peek() == '*' && peekNext() == '/') {
            advance(); // *
            advance(); // /
            return;
        }
        advance();
    }
    
    // If we get here, we hit EOF without closing comment
    std::ostringstream oss;
    oss << "Unterminated block comment starting at line " << startLine;
    error(oss.str());
}

// ============================================================================
// Token Scanning
// ============================================================================

void Lexer::scanToken() {
    // Skip whitespace and comments first
    skipWhitespace();
    
    if (isAtEnd()) return;
    
    // Update start position for this token
    start = current;
    
    char c = advance();
    
    // Single-character tokens and operators
    switch (c) {
        case '(': addToken(TokenType::TOKEN_LEFT_PAREN); break;
        case ')': addToken(TokenType::TOKEN_RIGHT_PAREN); break;
        case '{': addToken(TokenType::TOKEN_LEFT_BRACE); break;
        case '}': addToken(TokenType::TOKEN_RIGHT_BRACE); break;
        case '[': addToken(TokenType::TOKEN_LEFT_BRACKET); break;
        case ']': addToken(TokenType::TOKEN_RIGHT_BRACKET); break;
        case ';': addToken(TokenType::TOKEN_SEMICOLON); break;
        case ',': addToken(TokenType::TOKEN_COMMA); break;
        case '~': addToken(TokenType::TOKEN_TILDE); break;
        case '@': addToken(TokenType::TOKEN_AT); break;
        case '$': addToken(TokenType::TOKEN_DOLLAR); break;
        case '#': addToken(TokenType::TOKEN_HASH); break;
        case '`': addToken(TokenType::TOKEN_BACKTICK); break;
        
        // Operators that may be multi-character
        case '+':
            if (match('+')) {
                addToken(TokenType::TOKEN_PLUS_PLUS);
            } else if (match('=')) {
                addToken(TokenType::TOKEN_PLUS_EQUAL);
            } else {
                addToken(TokenType::TOKEN_PLUS);
            }
            break;
            
        case '-':
            if (match('-')) {
                addToken(TokenType::TOKEN_MINUS_MINUS);
            } else if (match('=')) {
                addToken(TokenType::TOKEN_MINUS_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::TOKEN_ARROW);
            } else {
                addToken(TokenType::TOKEN_MINUS);
            }
            break;
            
        case '*':
            if (match('=')) {
                addToken(TokenType::TOKEN_STAR_EQUAL);
            } else {
                addToken(TokenType::TOKEN_STAR);
            }
            break;
            
        case '/':
            if (match('=')) {
                addToken(TokenType::TOKEN_SLASH_EQUAL);
            } else {
                addToken(TokenType::TOKEN_SLASH);
            }
            break;
            
        case '%':
            if (match('=')) {
                addToken(TokenType::TOKEN_PERCENT_EQUAL);
            } else {
                addToken(TokenType::TOKEN_PERCENT);
            }
            break;
            
        case '=':
            if (match('=')) {
                addToken(TokenType::TOKEN_EQUAL_EQUAL);
            } else {
                addToken(TokenType::TOKEN_EQUAL);
            }
            break;
            
        case '!':
            if (match('=')) {
                addToken(TokenType::TOKEN_BANG_EQUAL);
            } else {
                addToken(TokenType::TOKEN_BANG);
            }
            break;
            
        case '<':
            if (match('=')) {
                if (match('>')) {
                    addToken(TokenType::TOKEN_SPACESHIP);
                } else {
                    addToken(TokenType::TOKEN_LESS_EQUAL);
                }
            } else if (match('<')) {
                addToken(TokenType::TOKEN_SHIFT_LEFT);
            } else if (match('|')) {
                addToken(TokenType::TOKEN_PIPE_LEFT);
            } else {
                addToken(TokenType::TOKEN_LESS);
            }
            break;
            
        case '>':
            if (match('=')) {
                addToken(TokenType::TOKEN_GREATER_EQUAL);
            } else if (match('>')) {
                addToken(TokenType::TOKEN_SHIFT_RIGHT);
            } else {
                addToken(TokenType::TOKEN_GREATER);
            }
            break;
            
        case '&':
            if (match('&')) {
                addToken(TokenType::TOKEN_AND_AND);
            } else {
                addToken(TokenType::TOKEN_AMPERSAND);
            }
            break;
            
        case '|':
            if (match('|')) {
                addToken(TokenType::TOKEN_OR_OR);
            } else if (match('>')) {
                addToken(TokenType::TOKEN_PIPE_RIGHT);
            } else {
                addToken(TokenType::TOKEN_PIPE);
            }
            break;
            
        case '^':
            addToken(TokenType::TOKEN_CARET);
            break;
            
        case '?':
            if (match('.')) {
                addToken(TokenType::TOKEN_SAFE_NAV);
            } else if (match('?')) {
                addToken(TokenType::TOKEN_NULL_COALESCE);
            } else {
                addToken(TokenType::TOKEN_QUESTION);
            }
            break;
            
        case ':':
            addToken(TokenType::TOKEN_COLON);
            break;
            
        case '.':
            if (match('.')) {
                if (match('.')) {
                    addToken(TokenType::TOKEN_DOT_DOT_DOT);
                } else {
                    addToken(TokenType::TOKEN_DOT_DOT);
                }
            } else {
                addToken(TokenType::TOKEN_DOT);
            }
            break;
            
        default:
            // Unknown character
            std::ostringstream oss;
            oss << "Unexpected character: '" << c << "'";
            error(oss.str());
            break;
    }
}

// ============================================================================
// Token Creation Methods
// ============================================================================

void Lexer::addToken(TokenType type) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, line, column - (current - start)));
}

void Lexer::addToken(TokenType type, int64_t value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, line, column - (current - start), value));
}

void Lexer::addToken(TokenType type, double value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, line, column - (current - start), value));
}

void Lexer::addToken(TokenType type, bool value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, line, column - (current - start), value));
}

void Lexer::addToken(TokenType type, const std::string& value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, line, column - (current - start), value));
}

// ============================================================================
// Error Reporting
// ============================================================================

void Lexer::error(const std::string& message) {
    std::ostringstream oss;
    oss << "[Line " << line << ", Col " << column << "] Error: " << message;
    errors.push_back(oss.str());
}

// ============================================================================
// Character Classification Helpers
// ============================================================================

bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

bool Lexer::isHexDigit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

bool Lexer::isBinaryDigit(char c) {
    return c == '0' || c == '1';
}

bool Lexer::isOctalDigit(char c) {
    return c >= '0' && c <= '7';
}

} // namespace frontend
} // namespace aria
