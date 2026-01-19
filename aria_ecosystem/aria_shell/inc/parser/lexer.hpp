/**
 * Shell Lexer - Whitespace-Insensitive Tokenization
 * 
 * Converts source text into token stream with support for:
 * - String interpolation (&{...})
 * - Whitespace insensitivity (outside strings)
 * - Shell operators (|, >, <, &)
 */

#pragma once

#include "parser/token.hpp"
#include <vector>
#include <stack>
#include <string>
#include <unordered_map>

namespace ariash {
namespace parser {

/**
 * Lexer state for handling nested contexts
 */
enum class LexerState {
    ROOT,               // Normal code
    STRING,             // Inside "..."
    STRING_TEMPLATE,    // Inside `...`
    INTERPOLATION       // Inside &{...}
};

/**
 * Shell Lexer - Tokenizes source code
 */
class ShellLexer {
public:
    explicit ShellLexer(const std::string& source);
    
    /**
     * Tokenize entire source
     */
    std::vector<Token> tokenize();
    
    /**
     * Get next token
     */
    Token nextToken();
    
    /**
     * Check if at end of input
     */
    bool isAtEnd() const { return current_ >= source_.length(); }
    
private:
    std::string source_;
    size_t current_;
    size_t line_;
    size_t column_;
    
    std::stack<LexerState> stateStack_;
    std::unordered_map<std::string, TokenType> keywords_;
    
    // Character access
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    
    // Token creation
    Token makeToken(TokenType type, const std::string& lexeme);
    SourceLocation currentLocation() const;
    
    // Whitespace handling
    void skipWhitespace();
    bool isWhitespace(char c) const;
    
    // Token scanning
    Token scanString(char quote);
    std::string scanTemplateString();
    Token scanNumber();
    Token scanIdentifier();
    Token scanOperator();
    
    // Keyword lookup
    void initKeywords();
    TokenType identifierType(const std::string& text);
};

} // namespace parser
} // namespace ariash
