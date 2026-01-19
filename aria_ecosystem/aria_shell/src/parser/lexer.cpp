/**
 * Shell Lexer Implementation
 */

#include "parser/lexer.hpp"
#include <cctype>
#include <stdexcept>

namespace ariash {
namespace parser {

// =============================================================================
// Token Type Strings
// =============================================================================

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        
        case TokenType::KW_IF: return "if";
        case TokenType::KW_ELSE: return "else";
        case TokenType::KW_WHILE: return "while";
        case TokenType::KW_FOR: return "for";
        
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::EQ: return "==";
        case TokenType::NE: return "!=";
        case TokenType::LT: return "<";
        case TokenType::GT: return ">";
        case TokenType::AND: return "&&";
        case TokenType::OR: return "||";
        case TokenType::ASSIGN: return "=";
        
        case TokenType::LPAREN: return "(";
        case TokenType::RPAREN: return ")";
        case TokenType::LBRACE: return "{";
        case TokenType::RBRACE: return "}";
        case TokenType::SEMICOLON: return ";";
        
        case TokenType::PIPE: return "|";
        case TokenType::REDIRECT_OUT: return ">";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN";
    }
}

// =============================================================================
// ShellLexer Implementation
// =============================================================================

ShellLexer::ShellLexer(const std::string& source)
    : source_(source)
    , current_(0)
    , line_(1)
    , column_(1)
{
    stateStack_.push(LexerState::ROOT);
    initKeywords();
}

void ShellLexer::initKeywords() {
    // Control flow
    keywords_["if"] = TokenType::KW_IF;
    keywords_["else"] = TokenType::KW_ELSE;
    keywords_["while"] = TokenType::KW_WHILE;
    keywords_["for"] = TokenType::KW_FOR;
    keywords_["in"] = TokenType::KW_IN;
    keywords_["func"] = TokenType::KW_FUNC;
    keywords_["return"] = TokenType::KW_RETURN;
    keywords_["break"] = TokenType::KW_BREAK;
    keywords_["continue"] = TokenType::KW_CONTINUE;
    keywords_["spawn"] = TokenType::KW_SPAWN;
    
    // Types
    keywords_["int8"] = TokenType::KW_INT8;
    keywords_["int16"] = TokenType::KW_INT16;
    keywords_["int32"] = TokenType::KW_INT32;
    keywords_["int64"] = TokenType::KW_INT64;
    keywords_["tbb8"] = TokenType::KW_TBB8;
    keywords_["tbb16"] = TokenType::KW_TBB16;
    keywords_["tbb32"] = TokenType::KW_TBB32;
    keywords_["tbb64"] = TokenType::KW_TBB64;
    keywords_["string"] = TokenType::KW_STRING;
    keywords_["buffer"] = TokenType::KW_BUFFER;
    keywords_["bool"] = TokenType::KW_BOOL;
    keywords_["gc"] = TokenType::KW_GC;
    keywords_["wild"] = TokenType::KW_WILD;
}

std::vector<Token> ShellLexer::tokenize() {
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = nextToken();
        tokens.push_back(token);
        
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }
    
    // Ensure EOF is always present (in case loop exited naturally)
    if (tokens.empty() || tokens.back().type != TokenType::END_OF_FILE) {
        tokens.push_back(makeToken(TokenType::END_OF_FILE, ""));
    }
    
    return tokens;
}

Token ShellLexer::nextToken() {
    // Skip whitespace (unless in string)
    LexerState state = stateStack_.top();
    if (state == LexerState::ROOT || state == LexerState::INTERPOLATION) {
        skipWhitespace();
    }
    
    if (isAtEnd()) {
        return makeToken(TokenType::END_OF_FILE, "");
    }
    
    char c = peek();
    SourceLocation loc = currentLocation();
    
    // String literals
    if (c == '"' || c == '\'') {
        return scanString(c);
    }
    
    // Template strings (backticks)
    if (c == '`') {
        stateStack_.push(LexerState::STRING_TEMPLATE);
        advance();
        return makeToken(TokenType::STRING, scanTemplateString());
    }
    
    // Numbers
    if (std::isdigit(c)) {
        return scanNumber();
    }
    
    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return scanIdentifier();
    }
    
    // Operators and delimiters
    return scanOperator();
}

char ShellLexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[current_];
}

char ShellLexer::peekNext() const {
    if (current_ + 1 >= source_.length()) return '\0';
    return source_[current_ + 1];
}

char ShellLexer::advance() {
    char c = source_[current_++];
    
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    
    return c;
}

bool ShellLexer::match(char expected) {
    if (isAtEnd()) return false;
    if (source_[current_] != expected) return false;
    
    advance();
    return true;
}

Token ShellLexer::makeToken(TokenType type, const std::string& lexeme) {
    return Token(type, lexeme, currentLocation());
}

SourceLocation ShellLexer::currentLocation() const {
    return SourceLocation(line_, column_);
}

void ShellLexer::skipWhitespace() {
    while (!isAtEnd() && isWhitespace(peek())) {
        char c = peek();
        
        // Newline can be a token in interactive mode
        if (c == '\n') {
            // For now, treat as whitespace (require semicolons)
            advance();
        } else {
            advance();
        }
    }
}

bool ShellLexer::isWhitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

Token ShellLexer::scanString(char quote) {
    SourceLocation loc = currentLocation();
    advance();  // Consume opening quote
    
    std::string value;
    
    while (!isAtEnd() && peek() != quote) {
        char c = advance();
        
        // Handle escape sequences
        if (c == '\\') {
            if (isAtEnd()) break;
            
            char escaped = advance();
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                default: value += escaped; break;
            }
        }
        // Check for interpolation start &{
        else if (c == '&' && peek() == '{') {
            // TODO: Handle interpolation
            // For now, just include literally
            value += c;
        }
        else {
            value += c;
        }
    }
    
    if (!isAtEnd()) {
        advance();  // Consume closing quote
    }
    
    Token token(TokenType::STRING, value, loc);
    return token;
}

std::string ShellLexer::scanTemplateString() {
    std::string value;
    
    while (!isAtEnd() && peek() != '`') {
        char c = advance();
        
        // Check for interpolation &{
        if (c == '&' && peek() == '{') {
            // TODO: Implement full interpolation parsing
            // For now, include literally
            value += c;
        } else {
            value += c;
        }
    }
    
    if (!isAtEnd()) {
        advance();  // Consume closing `
        stateStack_.pop();  // Exit template state
    }
    
    return value;
}

Token ShellLexer::scanNumber() {
    SourceLocation loc = currentLocation();
    std::string value;
    
    while (!isAtEnd() && std::isdigit(peek())) {
        value += advance();
    }
    
    // Check for decimal point
    if (peek() == '.' && std::isdigit(peekNext())) {
        value += advance();  // Consume '.'
        
        while (!isAtEnd() && std::isdigit(peek())) {
            value += advance();
        }
        
        Token token(TokenType::FLOAT, value, loc);
        token.floatValue = std::stod(value);
        return token;
    }
    
    Token token(TokenType::INTEGER, value, loc);
    token.intValue = std::stoll(value);
    return token;
}

Token ShellLexer::scanIdentifier() {
    SourceLocation loc = currentLocation();
    std::string value;
    
    while (!isAtEnd() && (std::isalnum(peek()) || peek() == '_')) {
        value += advance();
    }
    
    TokenType type = identifierType(value);
    return Token(type, value, loc);
}

TokenType ShellLexer::identifierType(const std::string& text) {
    auto it = keywords_.find(text);
    if (it != keywords_.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

Token ShellLexer::scanOperator() {
    SourceLocation loc = currentLocation();
    char c = advance();
    
    switch (c) {
        case '+':
            if (match('=')) return makeToken(TokenType::PLUS_ASSIGN, "+=");
            return makeToken(TokenType::PLUS, "+");
            
        case '-':
            if (match('=')) return makeToken(TokenType::MINUS_ASSIGN, "-=");
            return makeToken(TokenType::MINUS, "-");
            
        case '*': return makeToken(TokenType::STAR, "*");
        case '/': return makeToken(TokenType::SLASH, "/");
        case '%': return makeToken(TokenType::PERCENT, "%");
        
        case '=':
            if (match('=')) return makeToken(TokenType::EQ, "==");
            return makeToken(TokenType::ASSIGN, "=");
            
        case '!':
            if (match('=')) return makeToken(TokenType::NE, "!=");
            return makeToken(TokenType::NOT, "!");
            
        case '<':
            if (match('=')) return makeToken(TokenType::LE, "<=");
            return makeToken(TokenType::LT, "<");
            
        case '>':
            if (match('=')) return makeToken(TokenType::GE, ">=");
            if (match('>')) return makeToken(TokenType::REDIRECT_APPEND, ">>");
            return makeToken(TokenType::GT, ">");
            
        case '&':
            if (match('{')) {
                stateStack_.push(LexerState::INTERPOLATION);
                return makeToken(TokenType::INTERP_START, "&{");
            }
            if (match('&')) return makeToken(TokenType::AND, "&&");
            return makeToken(TokenType::BACKGROUND, "&");
            
        case '|':
            if (match('|')) return makeToken(TokenType::OR, "||");
            return makeToken(TokenType::PIPE, "|");
            
        case '(': return makeToken(TokenType::LPAREN, "(");
        case ')': return makeToken(TokenType::RPAREN, ")");
        case '{': return makeToken(TokenType::LBRACE, "{");
        case '}':
            // Check if closing interpolation
            if (stateStack_.top() == LexerState::INTERPOLATION) {
                stateStack_.pop();
            }
            return makeToken(TokenType::RBRACE, "}");
            
        case '[': return makeToken(TokenType::LBRACKET, "[");
        case ']': return makeToken(TokenType::RBRACKET, "]");
        case ';': return makeToken(TokenType::SEMICOLON, ";");
        case ',': return makeToken(TokenType::COMMA, ",");
        case '.': return makeToken(TokenType::DOT, ".");
        case ':': return makeToken(TokenType::COLON, ":");
        
        default:
            return makeToken(TokenType::UNKNOWN, std::string(1, c));
    }
}

} // namespace parser
} // namespace ariash
