#ifndef ARIA_LEXER_H
#define ARIA_LEXER_H

#include "frontend/token.h"
#include <string>
#include <vector>

namespace aria {
namespace frontend {

// ============================================================================
// Lexer Class - Tokenizes Aria source code
// ============================================================================
// Reference: aria_specs.txt
// Converts raw source text into a stream of tokens for the parser

class Lexer {
public:
    // Constructor
    explicit Lexer(const std::string& source);
    
    // Main tokenization method
    std::vector<Token> tokenize();
    
    // Get all errors encountered during lexing
    const std::vector<std::string>& getErrors() const;
    
private:
    // Source code and position tracking
    std::string source;
    size_t current;      // Current character position
    size_t start;        // Start of current token
    int line;            // Current line (1-indexed)
    int column;          // Current column (1-indexed)
    
    // Token collection and error tracking
    std::vector<Token> tokens;
    std::vector<std::string> errors;
    
    // ========================================================================
    // Character Navigation Methods
    // ========================================================================
    
    // Advance to next character and return current
    char advance();
    
    // Look at current character without consuming
    char peek() const;
    
    // Look ahead one character
    char peekNext() const;
    
    // Check if at end of source
    bool isAtEnd() const;
    
    // Conditionally advance if current matches expected
    bool match(char expected);
    
    // ========================================================================
    // Whitespace and Comment Handling
    // ========================================================================
    
    // Skip whitespace (spaces, tabs, newlines)
    void skipWhitespace();
    
    // Skip line comment (// to end of line)
    void skipLineComment();
    
    // Skip block comment (/* to */)
    void skipBlockComment();
    
    // ========================================================================
    // Token Scanning Methods
    // ========================================================================
    
    // Scan next token from source
    void scanToken();
    
    // Add token to token list
    void addToken(TokenType type);
    void addToken(TokenType type, int64_t value);
    void addToken(TokenType type, double value);
    void addToken(TokenType type, bool value);
    void addToken(TokenType type, const std::string& value);
    
    // Report lexer error
    void error(const std::string& message);
    
    // ========================================================================
    // Literal Scanning Methods
    // ========================================================================
    
    // Scan identifier or keyword
    void scanIdentifier();
    
    // Scan number literal (integer or float)
    void scanNumber();
    
    // Scan string literal (double quotes)
    void scanString();
    
    // Scan character literal (single quotes)
    void scanCharacter();
    
    // Scan template literal (backticks with &{} interpolation)
    void scanTemplateLiteral();
    
    // Check if identifier is a keyword and return appropriate token type
    TokenType identifierType();
    
    // ========================================================================
    // Character Classification Helpers
    // ========================================================================
    
    // Check if character is valid identifier start (letter or underscore)
    static bool isAlpha(char c);
    
    // Check if character is digit
    static bool isDigit(char c);
    
    // Check if character is alphanumeric or underscore
    static bool isAlphaNumeric(char c);
    
    // Check if character is hex digit
    static bool isHexDigit(char c);
    
    // Check if character is binary digit (0 or 1)
    static bool isBinaryDigit(char c);
    
    // Check if character is octal digit (0-7)
    static bool isOctalDigit(char c);
};

} // namespace frontend
} // namespace aria

#endif // ARIA_LEXER_H
