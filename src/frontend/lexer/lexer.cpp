#include "frontend/lexer/lexer.h"
#include <sstream>
#include <unordered_map>
#include <algorithm>

namespace aria {
namespace frontend {

// ============================================================================
// Keyword Map - Maps identifier strings to keyword tokens
// ============================================================================

static const std::unordered_map<std::string, TokenType> keywords = {
    // Memory qualifiers
    {"wild", TokenType::TOKEN_KW_WILD},
    {"wildx", TokenType::TOKEN_KW_WILDX},
    {"stack", TokenType::TOKEN_KW_STACK},
    {"gc", TokenType::TOKEN_KW_GC},
    {"defer", TokenType::TOKEN_KW_DEFER},
    
    // Control flow
    {"if", TokenType::TOKEN_KW_IF},
    {"else", TokenType::TOKEN_KW_ELSE},
    {"while", TokenType::TOKEN_KW_WHILE},
    {"for", TokenType::TOKEN_KW_FOR},
    {"loop", TokenType::TOKEN_KW_LOOP},
    {"till", TokenType::TOKEN_KW_TILL},
    {"when", TokenType::TOKEN_KW_WHEN},
    {"then", TokenType::TOKEN_KW_THEN},
    {"end", TokenType::TOKEN_KW_END},
    {"pick", TokenType::TOKEN_KW_PICK},
    {"fall", TokenType::TOKEN_KW_FALL},
    {"break", TokenType::TOKEN_KW_BREAK},
    {"continue", TokenType::TOKEN_KW_CONTINUE},
    {"return", TokenType::TOKEN_KW_RETURN},
    {"pass", TokenType::TOKEN_KW_PASS},
    {"fail", TokenType::TOKEN_KW_FAIL},
    
    // Async
    {"async", TokenType::TOKEN_KW_ASYNC},
    {"await", TokenType::TOKEN_KW_AWAIT},
    
    // Module system
    {"use", TokenType::TOKEN_KW_USE},
    {"mod", TokenType::TOKEN_KW_MOD},
    {"pub", TokenType::TOKEN_KW_PUB},
    {"extern", TokenType::TOKEN_KW_EXTERN},
    {"cfg", TokenType::TOKEN_KW_CFG},
    {"as", TokenType::TOKEN_KW_AS},
    
    // Other
    {"const", TokenType::TOKEN_KW_CONST},
    {"is", TokenType::TOKEN_KW_IS},
    
    // Type keywords - integers
    {"int1", TokenType::TOKEN_KW_INT1},
    {"int2", TokenType::TOKEN_KW_INT2},
    {"int4", TokenType::TOKEN_KW_INT4},
    {"int8", TokenType::TOKEN_KW_INT8},
    {"int16", TokenType::TOKEN_KW_INT16},
    {"int32", TokenType::TOKEN_KW_INT32},
    {"int64", TokenType::TOKEN_KW_INT64},
    {"int128", TokenType::TOKEN_KW_INT128},
    {"int256", TokenType::TOKEN_KW_INT256},
    {"int512", TokenType::TOKEN_KW_INT512},
    
    // Type keywords - unsigned integers
    {"uint8", TokenType::TOKEN_KW_UINT8},
    {"uint16", TokenType::TOKEN_KW_UINT16},
    {"uint32", TokenType::TOKEN_KW_UINT32},
    {"uint64", TokenType::TOKEN_KW_UINT64},
    {"uint128", TokenType::TOKEN_KW_UINT128},
    {"uint256", TokenType::TOKEN_KW_UINT256},
    {"uint512", TokenType::TOKEN_KW_UINT512},
    
    // Type keywords - TBB
    {"tbb8", TokenType::TOKEN_KW_TBB8},
    {"tbb16", TokenType::TOKEN_KW_TBB16},
    {"tbb32", TokenType::TOKEN_KW_TBB32},
    {"tbb64", TokenType::TOKEN_KW_TBB64},
    
    // Type keywords - floats
    {"flt32", TokenType::TOKEN_KW_FLT32},
    {"flt64", TokenType::TOKEN_KW_FLT64},
    {"flt128", TokenType::TOKEN_KW_FLT128},
    {"flt256", TokenType::TOKEN_KW_FLT256},
    {"flt512", TokenType::TOKEN_KW_FLT512},
    
    // Type keywords - special
    {"bool", TokenType::TOKEN_KW_BOOL},
    {"string", TokenType::TOKEN_KW_STRING},
    {"dyn", TokenType::TOKEN_KW_DYN},
    {"obj", TokenType::TOKEN_KW_OBJ},
    {"result", TokenType::TOKEN_KW_RESULT},
    {"array", TokenType::TOKEN_KW_ARRAY},
    {"func", TokenType::TOKEN_KW_FUNC},
    
    // Type keywords - balanced ternary/nonary
    {"trit", TokenType::TOKEN_KW_TRIT},
    {"tryte", TokenType::TOKEN_KW_TRYTE},
    {"nit", TokenType::TOKEN_KW_NIT},
    {"nyte", TokenType::TOKEN_KW_NYTE},
    
    // Type keywords - vectors and special math
    {"vec2", TokenType::TOKEN_KW_VEC2},
    {"vec3", TokenType::TOKEN_KW_VEC3},
    {"vec9", TokenType::TOKEN_KW_VEC9},
    {"tensor", TokenType::TOKEN_KW_TENSOR},
    {"matrix", TokenType::TOKEN_KW_MATRIX},
    
    // Literals
    {"true", TokenType::TOKEN_KW_TRUE},
    {"false", TokenType::TOKEN_KW_FALSE},
    {"NULL", TokenType::TOKEN_KW_NULL},
    {"ERR", TokenType::TOKEN_KW_ERR},
};

// ============================================================================
// Constructor and Main Tokenization
// ============================================================================

Lexer::Lexer(const std::string& source)
    : source(source), current(0), start(0), line(1), column(1), start_line(1), start_column(1) {
}

std::vector<Token> Lexer::tokenize() {
    tokens.clear();
    errors.clear();
    
    while (!isAtEnd()) {
        start = current;
        scanToken();
    }
    
    // Add EOF token so parser can reliably detect end of input
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
    start_line = line;
    start_column = column;
    
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
        case '`': scanTemplateLiteral(); break; // Template literals
        
        // String and character literals
        case '"': scanString(); break;
        case '\'': scanCharacter(); break;
        
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
            // Check for identifiers (variable names, keywords)
            if (isAlpha(c)) {
                scanIdentifier();
            }
            // Check for numbers
            else if (isDigit(c)) {
                scanNumber();
            }
            // Unknown character
            else {
                std::ostringstream oss;
                oss << "Unexpected character: '" << c << "'";
                error(oss.str());
            }
            break;
    }
}

// ============================================================================
// Token Creation Methods
// ============================================================================

void Lexer::addToken(TokenType type) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column));
}

void Lexer::addToken(TokenType type, int64_t value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value));
}

void Lexer::addToken(TokenType type, double value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value));
}

void Lexer::addToken(TokenType type, bool value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value));
}

void Lexer::addToken(TokenType type, const std::string& value) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value));
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
// Identifier and Keyword Scanning
// ============================================================================

void Lexer::scanIdentifier() {
    // Consume all alphanumeric characters
    while (isAlphaNumeric(peek())) {
        advance();
    }
    
    // Get the identifier text
    std::string text = source.substr(start, current - start);
    
    // Check if it's a keyword
    TokenType type = identifierType();
    
    // All identifiers (including keywords) use the same token type
    addToken(type);
}

TokenType Lexer::identifierType() {
    std::string text = source.substr(start, current - start);
    
    // Look up in keyword map
    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return it->second;
    }
    
    // Not a keyword, it's an identifier
    return TokenType::TOKEN_IDENTIFIER;
}

// ============================================================================
// Number Literal Scanning
// ============================================================================

void Lexer::scanNumber() {
    // Check for special number bases (hex, binary, octal)
    // Note: first digit already consumed, check if it was '0'
    if (source[start] == '0' && !isAtEnd()) {
        char next = peek(); // This is the character after '0'
        
        // Hexadecimal: 0x
        if (next == 'x' || next == 'X') {
            advance(); // consume 'x' (0 already consumed)
            
            if (!isHexDigit(peek())) {
                error("Expected hexadecimal digits after '0x'");
                return;
            }
            
            while (isHexDigit(peek()) || peek() == '_') {
                advance();
            }
            
            // Convert hex string to integer
            std::string text = source.substr(start, current - start);
            // Remove '0x' prefix and underscores
            text = text.substr(2);
            text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
            
            int64_t value = std::stoll(text, nullptr, 16);
            addToken(TokenType::TOKEN_INTEGER, value);
            return;
        }
        
        // Binary: 0b
        if (next == 'b' || next == 'B') {
            advance(); // consume 'b' (0 already consumed)
            
            if (!isBinaryDigit(peek())) {
                error("Expected binary digits after '0b'");
                return;
            }
            
            while (isBinaryDigit(peek()) || peek() == '_') {
                advance();
            }
            
            // Convert binary string to integer
            std::string text = source.substr(start, current - start);
            // Remove '0b' prefix and underscores
            text = text.substr(2);
            text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
            
            int64_t value = std::stoll(text, nullptr, 2);
            addToken(TokenType::TOKEN_INTEGER, value);
            return;
        }
        
        // Octal: 0o
        if (next == 'o' || next == 'O') {
            advance(); // consume 'o' (0 already consumed)
            
            if (!isOctalDigit(peek())) {
                error("Expected octal digits after '0o'");
                return;
            }
            
            while (isOctalDigit(peek()) || peek() == '_') {
                advance();
            }
            
            // Convert octal string to integer
            std::string text = source.substr(start, current - start);
            // Remove '0o' prefix and underscores
            text = text.substr(2);
            text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
            
            int64_t value = std::stoll(text, nullptr, 8);
            addToken(TokenType::TOKEN_INTEGER, value);
            return;
        }
    }
    
    // Decimal number (integer or float)
    while (isDigit(peek()) || peek() == '_') {
        advance();
    }
    
    // Check for decimal point
    bool isFloat = false;
    if (peek() == '.' && isDigit(peekNext())) {
        isFloat = true;
        advance(); // consume '.'
        
        while (isDigit(peek()) || peek() == '_') {
            advance();
        }
    }
    
    // Check for scientific notation
    if (peek() == 'e' || peek() == 'E') {
        isFloat = true;
        advance(); // consume 'e'
        
        if (peek() == '+' || peek() == '-') {
            advance(); // consume sign
        }
        
        if (!isDigit(peek())) {
            error("Expected digits in exponent");
            return;
        }
        
        while (isDigit(peek()) || peek() == '_') {
            advance();
        }
    }
    
    // Convert string to number
    std::string text = source.substr(start, current - start);
    // Remove underscores
    text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
    
    if (isFloat) {
        double value = std::stod(text);
        addToken(TokenType::TOKEN_FLOAT, value);
    } else {
        int64_t value = std::stoll(text);
        addToken(TokenType::TOKEN_INTEGER, value);
    }
}

// ============================================================================
// String and Character Literal Scanning
// ============================================================================

void Lexer::scanString() {
    int startLine = line;
    std::string value;
    
    // Opening quote already consumed by scanToken()
    
    while (!isAtEnd() && peek() != '"') {
        // Handle escape sequences
        if (peek() == '\\') {
            advance(); // consume backslash
            
            if (isAtEnd()) {
                error("Unterminated string literal");
                return;
            }
            
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '0': value += '\0'; break;
                default:
                    std::ostringstream oss;
                    oss << "Unknown escape sequence: \\" << escaped;
                    error(oss.str());
                    value += escaped; // Include the character anyway
                    break;
            }
        } else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        std::ostringstream oss;
        oss << "Unterminated string literal starting at line " << startLine;
        error(oss.str());
        return;
    }
    
    // Consume closing quote
    advance();
    
    addToken(TokenType::TOKEN_STRING, value);
}

void Lexer::scanCharacter() {
    int startLine = line;
    
    // Opening quote already consumed by scanToken()
    
    if (isAtEnd() || peek() == '\'') {
        error("Empty character literal");
        return;
    }
    
    char value;
    
    // Handle escape sequences
    if (peek() == '\\') {
        advance(); // consume backslash
        
        if (isAtEnd()) {
            error("Unterminated character literal");
            return;
        }
        
        char escaped = advance();
        switch (escaped) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '0': value = '\0'; break;
            default:
                std::ostringstream oss;
                oss << "Unknown escape sequence: \\" << escaped;
                error(oss.str());
                value = escaped;
                break;
        }
    } else {
        value = advance();
    }
    
    if (isAtEnd() || peek() != '\'') {
        std::ostringstream oss;
        oss << "Unterminated character literal starting at line " << startLine;
        error(oss.str());
        return;
    }
    
    // Consume closing quote
    advance();
    
    // Store as string since Token doesn't have a char constructor
    std::string charStr(1, value);
    addToken(TokenType::TOKEN_CHAR, charStr);
}

// ============================================================================
// Template Literal Scanning
// ============================================================================

void Lexer::scanTemplateLiteral() {
    int startLine = line;
    std::string value;
    
    // Opening backtick already consumed by scanToken()
    
    while (!isAtEnd() && peek() != '`') {
        // Handle escape sequences
        if (peek() == '\\') {
            advance(); // consume backslash
            
            if (isAtEnd()) {
                error("Unterminated template literal");
                return;
            }
            
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '`': value += '`'; break;  // Escaped backtick
                case '0': value += '\0'; break;
                default:
                    std::ostringstream oss;
                    oss << "Unknown escape sequence: \\" << escaped;
                    error(oss.str());
                    value += escaped; // Include the character anyway
                    break;
            }
        }
        // Handle interpolation syntax &{expression}
        else if (peek() == '&' && peekNext() == '{') {
            advance(); // consume '&'
            advance(); // consume '{'
            
            // For now, we'll store the interpolation markers in the string
            // The parser will need to handle the actual expression parsing
            // This is a simplified approach - a full implementation would
            // need to recursively tokenize the expression inside &{}
            
            value += "${"; // Convert &{ to ${ for easier processing later
            
            int braceDepth = 1;
            while (!isAtEnd() && braceDepth > 0) {
                char c = peek();
                if (c == '{') {
                    braceDepth++;
                } else if (c == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        value += '}';
                        advance(); // consume closing '}'
                        break;
                    }
                }
                value += advance();
            }
            
            if (braceDepth > 0) {
                error("Unterminated interpolation expression in template literal");
                return;
            }
        }
        // Handle newlines (templates can be multi-line)
        else if (peek() == '\n') {
            value += advance();
            // Note: line tracking already handled in advance()
        }
        else {
            value += advance();
        }
    }
    
    if (isAtEnd()) {
        std::ostringstream oss;
        oss << "Unterminated template literal starting at line " << startLine;
        error(oss.str());
        return;
    }
    
    // Consume closing backtick
    advance();
    
    // For now, we use TOKEN_STRING to store template literals
    // A more sophisticated approach would use a dedicated TOKEN_TEMPLATE type
    // and parse interpolations into separate tokens
    addToken(TokenType::TOKEN_STRING, value);
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
