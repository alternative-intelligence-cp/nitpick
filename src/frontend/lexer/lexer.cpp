#include "frontend/lexer/lexer.h"
#include <sstream>
#include <iostream>
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
    {"move", TokenType::TOKEN_KW_MOVE},
    
    // Memory ordering
    {"relaxed", TokenType::TOKEN_KW_RELAXED},
    {"acquire", TokenType::TOKEN_KW_ACQUIRE},
    {"release", TokenType::TOKEN_KW_RELEASE},
    {"acq_rel", TokenType::TOKEN_KW_ACQ_REL},
    {"seq_cst", TokenType::TOKEN_KW_SEQ_CST},
    
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
    {"exit", TokenType::TOKEN_KW_EXIT},
    {"prove", TokenType::TOKEN_KW_PROVE},
    {"assert_static", TokenType::TOKEN_KW_ASSERT_STATIC},
    {"raw", TokenType::TOKEN_KW_RAW},
    {"drop", TokenType::TOKEN_KW_DROP},
    {"ok", TokenType::TOKEN_KW_OK},
    {"defaults", TokenType::TOKEN_KW_DEFAULTS},
    {"apop", TokenType::TOKEN_KW_APOP},
    {"apush", TokenType::TOKEN_KW_APUSH},
    {"apeek", TokenType::TOKEN_KW_APEEK},
    {"astack", TokenType::TOKEN_KW_ASTACK},
    {"acap", TokenType::TOKEN_KW_ACAP},
    {"asize", TokenType::TOKEN_KW_ASIZE},
    {"afits", TokenType::TOKEN_KW_AFITS},
    {"atype", TokenType::TOKEN_KW_ATYPE},
    
    // User hash table
    {"ahash", TokenType::TOKEN_KW_AHASH},
    {"ahset", TokenType::TOKEN_KW_AHSET},
    {"ahget", TokenType::TOKEN_KW_AHGET},
    {"ahcount", TokenType::TOKEN_KW_AHCOUNT},
    {"ahsize", TokenType::TOKEN_KW_AHSIZE},
    {"ahfits", TokenType::TOKEN_KW_AHFITS},
    {"ahtype", TokenType::TOKEN_KW_AHTYPE},
    {"ahdelete", TokenType::TOKEN_KW_AHDELETE},
    {"ahhas", TokenType::TOKEN_KW_AHHAS},
    {"ahclear", TokenType::TOKEN_KW_AHCLEAR},
    {"ahkeys", TokenType::TOKEN_KW_AHKEYS},
    
    // Async
    {"async", TokenType::TOKEN_KW_ASYNC},
    {"await", TokenType::TOKEN_KW_AWAIT},
    {"catch", TokenType::TOKEN_KW_CATCH},
    {"in", TokenType::TOKEN_KW_IN},
    
    // Module system
    {"use", TokenType::TOKEN_KW_USE},
    {"mod", TokenType::TOKEN_KW_MOD},
    {"pub", TokenType::TOKEN_KW_PUB},
    {"extern", TokenType::TOKEN_KW_EXTERN},
    {"cfg", TokenType::TOKEN_KW_CFG},
    {"as", TokenType::TOKEN_KW_AS},
    {"comptime", TokenType::TOKEN_KW_COMPTIME},
    {"inline", TokenType::TOKEN_KW_INLINE},
    {"noinline", TokenType::TOKEN_KW_NOINLINE},
    {"macro", TokenType::TOKEN_KW_MACRO},
    {"derive", TokenType::TOKEN_KW_DERIVE},
    
    // Type declaration
    {"struct", TokenType::TOKEN_KW_STRUCT},
    {"enum", TokenType::TOKEN_KW_ENUM},
    {"Type", TokenType::TOKEN_KW_TYPE},
    {"opaque", TokenType::TOKEN_KW_OPAQUE},
    {"trait", TokenType::TOKEN_KW_TRAIT},
    {"impl", TokenType::TOKEN_KW_IMPL},
    {"Rules", TokenType::TOKEN_KW_RULES},
    {"limit", TokenType::TOKEN_KW_LIMIT},

    // Other
    {"const", TokenType::TOKEN_KW_CONST},
    {"fixed", TokenType::TOKEN_KW_FIXED},
    {"is", TokenType::TOKEN_KW_IS},
    
    // Design by Contract (P1-4 Formal Verification)
    {"requires", TokenType::TOKEN_KW_REQUIRES},
    {"ensures", TokenType::TOKEN_KW_ENSURES},
    {"invariant", TokenType::TOKEN_KW_INVARIANT},
    
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
    {"int1024", TokenType::TOKEN_KW_INT1024},
    {"int2048", TokenType::TOKEN_KW_INT2048},
    {"int4096", TokenType::TOKEN_KW_INT4096},
    
    // Type keywords - unsigned integers
    {"uint1", TokenType::TOKEN_KW_UINT1},
    {"uint2", TokenType::TOKEN_KW_UINT2},
    {"uint4", TokenType::TOKEN_KW_UINT4},
    {"uint8", TokenType::TOKEN_KW_UINT8},
    {"uint16", TokenType::TOKEN_KW_UINT16},
    {"uint32", TokenType::TOKEN_KW_UINT32},
    {"uint64", TokenType::TOKEN_KW_UINT64},
    {"uint128", TokenType::TOKEN_KW_UINT128},
    {"uint256", TokenType::TOKEN_KW_UINT256},
    {"uint512", TokenType::TOKEN_KW_UINT512},
    {"uint1024", TokenType::TOKEN_KW_UINT1024},
    {"uint2048", TokenType::TOKEN_KW_UINT2048},
    {"uint4096", TokenType::TOKEN_KW_UINT4096},
    
    // Type keywords - TBB
    {"tbb8", TokenType::TOKEN_KW_TBB8},
    {"tbb16", TokenType::TOKEN_KW_TBB16},
    {"tbb32", TokenType::TOKEN_KW_TBB32},
    {"tbb64", TokenType::TOKEN_KW_TBB64},
    
    // Type keywords - fractions
    {"frac8", TokenType::TOKEN_KW_FRAC8},
    {"frac16", TokenType::TOKEN_KW_FRAC16},
    {"frac32", TokenType::TOKEN_KW_FRAC32},
    {"frac64", TokenType::TOKEN_KW_FRAC64},
    
    // Type keywords - twisted floating point
    {"tfp32", TokenType::TOKEN_KW_TFP32},
    {"tfp64", TokenType::TOKEN_KW_TFP64},
    
    // Type keywords - floats
    {"flt32", TokenType::TOKEN_KW_FLT32},
    {"flt64", TokenType::TOKEN_KW_FLT64},
    {"flt128", TokenType::TOKEN_KW_FLT128},
    {"flt256", TokenType::TOKEN_KW_FLT256},
    {"flt512", TokenType::TOKEN_KW_FLT512},
    
    // Type keywords - fixed point
    {"fix256", TokenType::TOKEN_KW_FIX256},
    
    // Type keywords - special
    {"bool", TokenType::TOKEN_KW_BOOL},
    {"string", TokenType::TOKEN_KW_STRING},
    {"dyn", TokenType::TOKEN_KW_DYN},
    {"obj", TokenType::TOKEN_KW_OBJ},
    {"any", TokenType::TOKEN_KW_ANY},
    {"Result", TokenType::TOKEN_KW_RESULT},
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
    {"matrix", TokenType::TOKEN_KW_MATRIX},
    {"tmatrix", TokenType::TOKEN_KW_TMATRIX},
    {"tensor", TokenType::TOKEN_KW_TENSOR},
    {"ttensor", TokenType::TOKEN_KW_TTENSOR},
    
    // Type keywords - I/O and system
    {"binary", TokenType::TOKEN_KW_BINARY},
    {"buffer", TokenType::TOKEN_KW_BUFFER},
    {"stream", TokenType::TOKEN_KW_STREAM},
    {"process", TokenType::TOKEN_KW_PROCESS},
    {"pipe", TokenType::TOKEN_KW_PIPE},
    {"debug", TokenType::TOKEN_KW_DEBUG},
    
    // Literals
    {"true", TokenType::TOKEN_KW_TRUE},
    {"false", TokenType::TOKEN_KW_FALSE},
    {"NULL", TokenType::TOKEN_KW_NULL},
    {"NIL", TokenType::TOKEN_KW_NIL},
    {"ERR", TokenType::TOKEN_KW_ERR},
    {"unknown", TokenType::TOKEN_KW_UNKNOWN},
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
        case '@': 
            // Check for @cast or @cast_unchecked keywords
            // At this point, '@' has been consumed, so peek() returns the next char
            if (peek() == 'c' && isAtCastKeyword()) {
                scanCastKeyword();
            } else {
                addToken(TokenType::TOKEN_AT); // Plain @ operator
            }
            break;
        case '$':
            if (peek() == '$') {
                advance(); // consume second $
                if (peek() == 'i' && !isAlphaNumeric(peekNext())) {
                    advance(); // consume 'i'
                    addToken(TokenType::TOKEN_KW_BORROW_IMM);
                } else if (peek() == 'm' && !isAlphaNumeric(peekNext())) {
                    advance(); // consume 'm'
                    addToken(TokenType::TOKEN_KW_BORROW_MUT);
                } else {
                    // Two dollars but not $$i or $$m — emit two TOKEN_DOLLAR
                    addToken(TokenType::TOKEN_DOLLAR);
                    addToken(TokenType::TOKEN_DOLLAR);
                }
            } else {
                addToken(TokenType::TOKEN_DOLLAR);
            }
            break;
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
            } else if (match('>')) {
                addToken(TokenType::TOKEN_FAT_ARROW);  // => cast operator
            } else {
                addToken(TokenType::TOKEN_EQUAL);
            }
            break;
            
        case '!':
            if (match('!')) {
                // We have !! - check for third !
                if (match('!')) {
                    addToken(TokenType::TOKEN_BANG_BANG_BANG);  // !!!
                } else {
                    addToken(TokenType::TOKEN_BANG_BANG);  // !! (sys!! modifier)
                }
            } else if (match('=')) {
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
            } else if (match('-')) {
                addToken(TokenType::TOKEN_LEFT_ARROW);  // <- for pointer dereference
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
            } else if (match('!')) {
                addToken(TokenType::TOKEN_QUESTION_BANG);  // ?! emphatic unwrap
            } else if (match('|')) {
                addToken(TokenType::TOKEN_QUESTION_PIPE);  // ?| defaults shorthand
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
                } else if (match('?')) {
                    addToken(TokenType::TOKEN_VARIADIC);
                } else if (match('^')) {
                    addToken(TokenType::TOKEN_SPREAD);
                } else {
                    addToken(TokenType::TOKEN_DOT_DOT);
                }
            } else {
                addToken(TokenType::TOKEN_DOT);
            }
            break;
            
        case '_':
            // _? (drop shorthand) and _! (raw shorthand)
            if (peek() == '?') {
                advance();
                addToken(TokenType::TOKEN_UNDERSCORE_QUESTION);
            } else if (peek() == '!') {
                advance();
                addToken(TokenType::TOKEN_UNDERSCORE_BANG);
            } else {
                // Plain underscore or _identifier — scan as identifier
                scanIdentifier();
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

// High-precision literal support (Phase 3.2.5)
void Lexer::addToken(TokenType type, int64_t value, const std::string& raw_text) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value, raw_text));
}

void Lexer::addToken(TokenType type, double value, const std::string& raw_text) {
    std::string lexeme = source.substr(start, current - start);
    tokens.push_back(Token(type, lexeme, start_line, start_column, value, raw_text));
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
            
            // ZERO IMPLICIT CONVERSION PHASE 2: Type suffix optional in typed contexts
            std::string suffix = scanTypeSuffix();
            
            if (suffix.empty()) {
                // Untyped hex literal - type checker will validate fit in typed contexts
                try {
                    int64_t value = std::stoll(text, nullptr, 16);
                    addToken(TokenType::TOKEN_INTEGER, value);
                } catch (std::out_of_range&) {
                    error("Hexadecimal literal value too large");
                    addToken(TokenType::TOKEN_ERROR);
                }
                return;
            }
            
            TokenType tokenType = suffixToTokenType(suffix, false);
            if (tokenType == TokenType::TOKEN_ERROR) {
                error("Invalid type suffix '" + suffix + "' for hex literal.");
                addToken(TokenType::TOKEN_ERROR);
                return;
            }
            
            // Typed hex literal
            try {
                int64_t value = std::stoll(text, nullptr, 16);
                addToken(tokenType, value);
            } catch (std::out_of_range&) {
                // Value exceeds int64 range, store as raw string for high-precision handling
                addToken(tokenType, (int64_t)0, text);
            }
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
            
            // ZERO IMPLICIT CONVERSION PHASE 2: Type suffix optional in typed contexts
            std::string suffix = scanTypeSuffix();
            if (suffix.empty()) {
                // Untyped binary literal - type checker will validate fit
                try {
                    int64_t value = std::stoll(text, nullptr, 2);
                    addToken(TokenType::TOKEN_INTEGER, value);
                } catch (std::out_of_range&) {
                    error("Binary literal value too large");
                    addToken(TokenType::TOKEN_ERROR);
                }
                return;
            }
            
            TokenType tokenType = suffixToTokenType(suffix, false);
            if (tokenType == TokenType::TOKEN_ERROR) {
                error("Invalid type suffix '" + suffix + "' for binary literal.");
                addToken(TokenType::TOKEN_ERROR);
                return;
            }
            
            // Try to convert, but store raw string if overflow
            try {
                int64_t value = std::stoll(text, nullptr, 2);
                addToken(tokenType, value);
            } catch (std::out_of_range&) {
                // Value exceeds int64 range, store as raw string for high-precision handling
                addToken(tokenType, (int64_t)0, text);
            }
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
            
            // ZERO IMPLICIT CONVERSION PHASE 2: Type suffix optional in typed contexts
            std::string suffix = scanTypeSuffix();
            if (suffix.empty()) {
                // Untyped octal literal - type checker will validate fit
                try {
                    int64_t value = std::stoll(text, nullptr, 8);
                    addToken(TokenType::TOKEN_INTEGER, value);
                } catch (std::out_of_range&) {
                    error("Octal literal value too large");
                    addToken(TokenType::TOKEN_ERROR);
                }
                return;
            }
            
            TokenType tokenType = suffixToTokenType(suffix, false);
            if (tokenType == TokenType::TOKEN_ERROR) {
                error("Invalid type suffix '" + suffix + "' for octal literal.");
                addToken(TokenType::TOKEN_ERROR);
                return;
            }
            
            // Typed octal literal
            try {
                int64_t value = std::stoll(text, nullptr, 8);
                addToken(tokenType, value);
            } catch (std::out_of_range&) {
                // Value exceeds int64 range, store as raw string for high-precision handling
                addToken(tokenType, (int64_t)0, text);
            }
            return;
        }
        
        // Ternary (Balanced Ternary): 0t
        if (next == 't' || next == 'T') {
            advance(); // consume 't' (0 already consumed)
            
            // Check for at least one ternary digit
            char c = peek();
            if (c != '0' && c != '1' && c != 'T' && c != 't') {
                error("Expected ternary digits (0, 1, T) after '0t'");
                return;
            }
            
            // Consume all ternary digits (0, 1, T/t)
            while (!isAtEnd()) {
                c = peek();
                if (c == '0' || c == '1' || c == 'T' || c == 't' || c == '_') {
                    advance();
                } else {
                    break;
                }
            }
            
            // Extract ternary string and convert to integer
            std::string text = source.substr(start, current - start);
            // Remove '0t' prefix and underscores
            text = text.substr(2);
            text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
            
            // Convert balanced ternary to integer
            // T/t represents -1, 0 represents 0, 1 represents 1
            int64_t value = 0;
            int64_t power = 1;
            
            // Process from right to left
            for (int i = text.length() - 1; i >= 0; i--) {
                char digit = text[i];
                int trit_value;
                
                if (digit == '0') {
                    trit_value = 0;
                } else if (digit == '1') {
                    trit_value = 1;
                } else if (digit == 'T' || digit == 't') {
                    trit_value = -1;
                } else {
                    error("Invalid ternary digit: " + std::string(1, digit));
                    return;
                }
                
                value += trit_value * power;
                power *= 3;
            }
            
            addToken(TokenType::TOKEN_TERNARY, value);
            return;
        }
        
        // Nonary (Balanced Nonary): 0n
        if (next == 'n' || next == 'N') {
            advance(); // consume 'n' (0 already consumed)
            
            // Check for at least one nonary digit
            char c = peek();
            bool validNonaryDigit = (c >= '0' && c <= '4') || 
                                   (c >= 'A' && c <= 'D') || 
                                   (c >= 'a' && c <= 'd');
            if (!validNonaryDigit) {
                error("Expected nonary digits (0-4, A-D) after '0n'");
                return;
            }
            
            // Consume all nonary digits (0-4, A-D/a-d)
            while (!isAtEnd()) {
                c = peek();
                bool isNonaryDigit = (c >= '0' && c <= '4') || 
                                    (c >= 'A' && c <= 'D') || 
                                    (c >= 'a' && c <= 'd') ||
                                    c == '_';
                if (isNonaryDigit) {
                    advance();
                } else {
                    break;
                }
            }
            
            // Extract nonary string and convert to integer
            std::string text = source.substr(start, current - start);
            // Remove '0n' prefix and underscores
            text = text.substr(2);
            text.erase(std::remove(text.begin(), text.end(), '_'), text.end());
            
            // Convert balanced nonary to integer
            // D/d = -4, C/c = -3, B/b = -2, A/a = -1, 0 = 0, 1 = 1, 2 = 2, 3 = 3, 4 = 4
            int64_t value = 0;
            int64_t power = 1;
            
            // Process from right to left
            for (int i = text.length() - 1; i >= 0; i--) {
                char digit = text[i];
                int nit_value;
                
                if (digit >= '0' && digit <= '4') {
                    nit_value = digit - '0';
                } else if (digit == 'A' || digit == 'a') {
                    nit_value = -1;
                } else if (digit == 'B' || digit == 'b') {
                    nit_value = -2;
                } else if (digit == 'C' || digit == 'c') {
                    nit_value = -3;
                } else if (digit == 'D' || digit == 'd') {
                    nit_value = -4;
                } else {
                    error("Invalid nonary digit: " + std::string(1, digit));
                    return;
                }
                
                value += nit_value * power;
                power *= 9;
            }
            
            addToken(TokenType::TOKEN_NONARY, value);
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
    
    // Convert string to number (Phase 3.2.5: Store raw string for high-precision literals)
    std::string text = source.substr(start, current - start);
    // Remove underscores for cleaned text
    std::string cleaned_text = text;
    cleaned_text.erase(std::remove(cleaned_text.begin(), cleaned_text.end(), '_'), cleaned_text.end());
    
    // ========================================================================
    // ZERO IMPLICIT CONVERSION POLICY - Type suffix (OPTIONAL with context)
    // ========================================================================
    // Reference: docs/programming_guide/types/zero_implicit_conversion.md
    // 
    // DESIGN: "Fits-or-Fails" Literal Inference
    // - Type suffix REQUIRED in ambiguous contexts (function args, expressions)
    // - Type suffix OPTIONAL in typed contexts (variable declarations)
    // - If untyped, type checker validates value fits in target type
    // - NO truncation, NO precision loss, NO silent conversions
    //
    // Examples:
    //   int8:a = 42;         ✅ OK: 42 fits in int8 (-128..127)
    //   int8:b = 200;        ❌ ERROR: 200 doesn't fit in int8
    //   print(42);           ❌ ERROR: Ambiguous (no suffix, no target type)
    //   print(42i32);        ✅ OK: Explicit type
    
    std::string suffix = scanTypeSuffix();
    
    if (suffix.empty()) {
        // PHASE 2: Untyped literals allowed - type checker validates fit in typed contexts
        // In ambiguous contexts (function args, expressions), type checker will error
        if (isFloat) {
            try {
                double value = std::stod(cleaned_text);
                addToken(TokenType::TOKEN_FLOAT, value);
            } catch (std::out_of_range&) {
                error("Float literal value out of range");
                addToken(TokenType::TOKEN_ERROR);
            }
        } else {
            try {
                int64_t value = std::stoll(cleaned_text);
                addToken(TokenType::TOKEN_INTEGER, value);
            } catch (std::out_of_range&) {
                error("Integer literal value too large for int64");
                addToken(TokenType::TOKEN_ERROR);
            }
        }
        return;
    }
    
    // TYPED LITERAL: Parse suffix and get appropriate token type
    TokenType tokenType = suffixToTokenType(suffix, isFloat);
    
    if (tokenType == TokenType::TOKEN_ERROR) {
        // Invalid suffix for this literal type
        if (isFloat) {
            error("Invalid type suffix '" + suffix + "' for floating-point literal. "
                  "Float literals require: f32, f64, f128, f256, f512, or fix256.\n"
                  "  Example: 3.14f32");
        } else {
            error("Invalid type suffix '" + suffix + "' for integer literal. "
                  "Integer literals require: u8-u4096, i8-i4096, or tbb8-tbb64.\n"
                  "  Example: 42u32 or -10i32");
        }
        addToken(TokenType::TOKEN_ERROR);
        return;
    }
    
    // Create typed token with appropriate value
    if (isFloat) {
        // Parse float value for immediate use, also store raw string for high-precision scenarios
        double float_value = std::stod(cleaned_text);
        addToken(tokenType, float_value, cleaned_text);
    } else {
        // Bug #21 fix: unsigned types (u8..u4096) use stoull to handle full uint64 range.
        // Values >= 2^63 are valid unsigned but overflow stoll, causing silent store-as-zero.
        // We bit-reinterpret the uint64 result as int64 so the token's int_value field
        // carries the correct bit pattern; codegenLiteral's hasExplicitType() path then
        // constructs the LLVM APInt with is_signed=false, recovering the correct constant.
        bool isUnsignedToken = (tokenType >= TokenType::TOKEN_INTEGER_U8 &&
                                tokenType <= TokenType::TOKEN_INTEGER_U4096);
        if (isUnsignedToken) {
            try {
                uint64_t uvalue = std::stoull(cleaned_text);
                addToken(tokenType, static_cast<int64_t>(uvalue));
            } catch (std::out_of_range&) {
                // Exceeds 64-bit range (u128/u256/etc.) — fall back to raw string
                addToken(tokenType, (int64_t)0, cleaned_text);
            }
        } else {
            try {
                int64_t value = std::stoll(cleaned_text);
                addToken(tokenType, value);
            } catch (std::out_of_range&) {
                // Value exceeds int64 range, store as raw string for high-precision handling
                addToken(tokenType, (int64_t)0, cleaned_text);
            }
        }
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
    int startCol = start_column;

    // Opening backtick already consumed by scanToken()
    // Emit TOKEN_TEMPLATE_START
    tokens.push_back(Token(TokenType::TOKEN_TEMPLATE_START, "`", startLine, startCol, ""));

    // Build current text part
    std::string currentPart;
    int partStartLine = line;
    int partStartCol = column;

    while (!isAtEnd()) {
        // Check for interpolation start: &{
        if (peek() == '&' && peekNext() == '{') {
            // Emit the current text part (even if empty for proper structure)
            tokens.push_back(Token(TokenType::TOKEN_TEMPLATE_PART, currentPart,
                                   partStartLine, partStartCol, currentPart));
            currentPart.clear();

            int interpLine = line;
            int interpCol = column;
            advance();  // consume '&'
            advance();  // consume '{'

            // Emit TOKEN_INTERP_START
            tokens.push_back(Token(TokenType::TOKEN_INTERP_START, "&{", interpLine, interpCol, ""));

            // Tokenize the interpolation expression by recursively scanning tokens
            // until we hit the closing } at depth 0
            int braceDepth = 1;
            while (!isAtEnd() && braceDepth > 0) {
                // Skip whitespace
                while (!isAtEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\r')) {
                    advance();
                }
                if (peek() == '\n') {
                    line++;
                    column = 1;
                    advance();
                    continue;
                }

                if (isAtEnd()) break;

                char c = peek();
                if (c == '{') {
                    braceDepth++;
                    // Scan this as a token (could be object literal start)
                    scanToken();
                } else if (c == '}') {
                    braceDepth--;
                    if (braceDepth == 0) {
                        // Don't consume - will be handled below
                        break;
                    } else {
                        scanToken();
                    }
                } else {
                    // Scan a regular token
                    scanToken();
                }
            }

            if (braceDepth > 0) {
                error("Unterminated interpolation in template literal");
                return;
            }

            // Consume and emit TOKEN_INTERP_END
            int endLine = line;
            int endCol = column;
            advance();  // consume '}'
            tokens.push_back(Token(TokenType::TOKEN_INTERP_END, "}", endLine, endCol, ""));

            // Reset part tracking for next text segment
            partStartLine = line;
            partStartCol = column;
            continue;
        }

        // Check for template end: `
        if (peek() == '`') {
            // Always emit final text part (even if empty) to maintain invariant:
            // parts.size() == interpolations.size() + 1
            tokens.push_back(Token(TokenType::TOKEN_TEMPLATE_PART, currentPart,
                                   partStartLine, partStartCol, currentPart));

            int endLine = line;
            int endCol = column;
            advance();  // Consume closing backtick

            // Emit TOKEN_TEMPLATE_END
            tokens.push_back(Token(TokenType::TOKEN_TEMPLATE_END, "`", endLine, endCol, ""));
            return;
        }

        // Handle escape sequences
        if (peek() == '\\') {
            advance(); // consume backslash

            if (isAtEnd()) {
                error("Unterminated template literal");
                return;
            }

            char escaped = advance();
            switch (escaped) {
                case 'n': currentPart += '\n'; break;
                case 't': currentPart += '\t'; break;
                case 'r': currentPart += '\r'; break;
                case '\\': currentPart += '\\'; break;
                case '`': currentPart += '`'; break;  // Escaped backtick
                case '&': currentPart += '&'; break;  // Escaped ampersand
                case '0': currentPart += '\0'; break;
                default:
                    // For unknown escapes, just include the character
                    currentPart += escaped;
                    break;
            }
            continue;
        }

        // Handle newlines (track line numbers)
        if (peek() == '\n') {
            line++;
            column = 1;
        }

        // Regular character - add to current part
        currentPart += advance();
    }

    // Reached end of file without closing backtick
    std::ostringstream oss;
    oss << "Unterminated template literal starting at line " << startLine;
    error(oss.str());
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

// ============================================================================
// Type Suffix Scanning (Zero Implicit Conversion Policy)
// ============================================================================
// Reference: docs/programming_guide/types/zero_implicit_conversion.md

// Scan type suffix after numeric literal
// Returns empty string if no suffix found
std::string Lexer::scanTypeSuffix() {
    size_t suffix_start = current;
    
    // Type suffixes start with letter (u, i, f, t) or "fix"
    if (!isAlpha(peek())) {
        return "";  // No suffix - this is an ERROR (caught by caller)
    }
    
    // Scan letters and digits for suffix (e.g., "u32", "i64", "tbb16", "fix256")
    while (isAlphaNumeric(peek())) {
        advance();
    }
    
    return source.substr(suffix_start, current - suffix_start);
}

// ============================================================================
// @cast Keyword Scanning (Zero Implicit Conversion)
// ============================================================================

// Check if we're at @cast or @cast_unchecked keyword
bool Lexer::isAtCastKeyword() {
    // We're at the character after '@' (should be 'c' in "cast")
    size_t saved = current;
    
    // Check for "cast" after @
    if (peek() == 'c' && peekN(1) == 'a' && peekN(2) == 's' && peekN(3) == 't') {
        // Could be @cast or @cast_unchecked
        size_t after_cast = current + 4;
        
        // Check if it's followed by '_unchecked'
        if (after_cast + 10 <= source.length() &&
            source.substr(after_cast, 10) == "_unchecked") {
            // Make sure it's not part of a longer identifier
            size_t check_pos = after_cast + 10;
            if (check_pos >= source.length() || !isAlphaNumeric(source[check_pos])) {
                current = saved;
                return true;  // @cast_unchecked
            }
        }
        
        // Just @cast - make sure it's not part of a longer identifier
        if (after_cast >= source.length() || !isAlphaNumeric(source[after_cast])) {
            current = saved;
            return true;  // @cast
        }
    }
    
    current = saved;
    return false;
}

// Scan @cast or @cast_unchecked keyword
void Lexer::scanCastKeyword() {
    // '@' has already been consumed by scanToken()
    // Current position is at 'c' in "cast"
    
    // Read 'cast'
    advance(); // 'c'
    advance(); // 'a'
    advance(); // 's'
    advance(); // 't'
    
    // Check for '_unchecked' suffix
    if (peek() == '_' && peekN(1) == 'u' && peekN(2) == 'n') {
        // Read '_unchecked'
        for (int i = 0; i < 10; i++) {
            advance();
        }
        addToken(TokenType::TOKEN_KW_CAST_UNCHECKED);
    } else {
        addToken(TokenType::TOKEN_KW_CAST);
    }
}

// Helper to peek N characters ahead
char Lexer::peekN(int n) const {
    if (current + n >= source.length()) return '\0';
    return source[current + n];
}

// Convert type suffix to TokenType
// Returns TOKEN_ERROR if invalid suffix for given context (int vs float)
TokenType Lexer::suffixToTokenType(const std::string& suffix, bool isFloat) {
    // Integer type suffixes (for literals without decimal point)
    if (!isFloat) {
        // Unsigned integers
        if (suffix == "u8") return TokenType::TOKEN_INTEGER_U8;
        if (suffix == "u16") return TokenType::TOKEN_INTEGER_U16;
        if (suffix == "u32") return TokenType::TOKEN_INTEGER_U32;
        if (suffix == "u64") return TokenType::TOKEN_INTEGER_U64;
        if (suffix == "u128") return TokenType::TOKEN_INTEGER_U128;
        if (suffix == "u256") return TokenType::TOKEN_INTEGER_U256;
        if (suffix == "u512") return TokenType::TOKEN_INTEGER_U512;
        if (suffix == "u1024") return TokenType::TOKEN_INTEGER_U1024;
        if (suffix == "u2048") return TokenType::TOKEN_INTEGER_U2048;
        if (suffix == "u4096") return TokenType::TOKEN_INTEGER_U4096;
        
        // Signed integers
        if (suffix == "i8") return TokenType::TOKEN_INTEGER_I8;
        if (suffix == "i16") return TokenType::TOKEN_INTEGER_I16;
        if (suffix == "i32") return TokenType::TOKEN_INTEGER_I32;
        if (suffix == "i64") return TokenType::TOKEN_INTEGER_I64;
        if (suffix == "i128") return TokenType::TOKEN_INTEGER_I128;
        if (suffix == "i256") return TokenType::TOKEN_INTEGER_I256;
        if (suffix == "i512") return TokenType::TOKEN_INTEGER_I512;
        if (suffix == "i1024") return TokenType::TOKEN_INTEGER_I1024;
        if (suffix == "i2048") return TokenType::TOKEN_INTEGER_I2048;
        if (suffix == "i4096") return TokenType::TOKEN_INTEGER_I4096;
        
        // TBB (Twisted Balanced Binary)
        if (suffix == "tbb8") return TokenType::TOKEN_INTEGER_TBB8;
        if (suffix == "tbb16") return TokenType::TOKEN_INTEGER_TBB16;
        if (suffix == "tbb32") return TokenType::TOKEN_INTEGER_TBB32;
        if (suffix == "tbb64") return TokenType::TOKEN_INTEGER_TBB64;
        
        // Float suffix on integer literal is ERROR
        if (suffix.length() > 0 && (suffix[0] == 'f' || suffix.substr(0, 3) == "fix")) {
            return TokenType::TOKEN_ERROR;
        }
    } else {
        // Float type suffixes (for literals with decimal point or scientific notation)
        if (suffix == "f32") return TokenType::TOKEN_FLOAT_F32;
        if (suffix == "f64") return TokenType::TOKEN_FLOAT_F64;
        if (suffix == "f128") return TokenType::TOKEN_FLOAT_F128;
        if (suffix == "f256") return TokenType::TOKEN_FLOAT_F256;
        if (suffix == "f512") return TokenType::TOKEN_FLOAT_F512;
        
        // Fixed-point (Q128.128 deterministic physics)
        if (suffix == "fix256") return TokenType::TOKEN_FLOAT_FIX256;
        
        // Integer suffix on float literal is ERROR
        if (suffix.length() > 0 && (suffix[0] == 'u' || suffix[0] == 'i' || suffix[0] == 't')) {
            return TokenType::TOKEN_ERROR;
        }
    }
    
    // Unknown/invalid suffix
    return TokenType::TOKEN_ERROR;
}

} // namespace frontend
} // namespace aria
