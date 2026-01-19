/**
 * Shell Token Definitions
 * 
 * Token types for the whitespace-insensitive Aria shell parser.
 * Extends the Aria compiler's token set with shell-specific operators.
 */

#pragma once

#include <string>
#include <cstdint>

namespace ariash {
namespace parser {

/**
 * Token types for shell parsing
 */
enum class TokenType {
    // Literals
    INTEGER,
    FLOAT,
    STRING,
    IDENTIFIER,
    
    // Keywords
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_IN,
    KW_FUNC,
    KW_RETURN,
    KW_BREAK,
    KW_CONTINUE,
    KW_SPAWN,
    
    // Type keywords
    KW_INT8,
    KW_INT16,
    KW_INT32,
    KW_INT64,
    KW_TBB8,
    KW_TBB16,
    KW_TBB32,
    KW_TBB64,
    KW_STRING,
    KW_BUFFER,
    KW_BOOL,
    KW_GC,
    KW_WILD,
    
    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    PERCENT,        // %
    
    // Comparison
    EQ,             // ==
    NE,             // !=
    LT,             // <
    LE,             // <=
    GT,             // >
    GE,             // >=
    
    // Logical
    AND,            // &&
    OR,             // ||
    NOT,            // !
    
    // Assignment
    ASSIGN,         // =
    PLUS_ASSIGN,    // +=
    MINUS_ASSIGN,   // -=
    
    // Delimiters
    LPAREN,         // (
    RPAREN,         // )
    LBRACE,         // {
    RBRACE,         // }
    LBRACKET,       // [
    RBRACKET,       // ]
    SEMICOLON,      // ;
    COMMA,          // ,
    DOT,            // .
    COLON,          // :
    
    // Shell-specific
    PIPE,           // |
    REDIRECT_OUT,   // >
    REDIRECT_APPEND,// >>
    REDIRECT_IN,    // <
    BACKGROUND,     // &
    INTERP_START,   // &{
    NEWLINE,        // \n (in interactive mode)
    
    // Special
    END_OF_FILE,
    UNKNOWN
};

/**
 * Source location for error reporting
 */
struct SourceLocation {
    size_t line;
    size_t column;
    
    SourceLocation() : line(1), column(1) {}
    SourceLocation(size_t l, size_t c) : line(l), column(c) {}
};

/**
 * Token structure
 */
struct Token {
    TokenType type;
    std::string lexeme;      // Raw text
    SourceLocation location;
    
    // Type-specific values
    int64_t intValue;
    double floatValue;
    
    Token() 
        : type(TokenType::UNKNOWN)
        , intValue(0)
        , floatValue(0.0)
    {}
    
    Token(TokenType t, const std::string& lex, SourceLocation loc)
        : type(t)
        , lexeme(lex)
        , location(loc)
        , intValue(0)
        , floatValue(0.0)
    {}
    
    bool isKeyword() const {
        return type >= TokenType::KW_IF && type <= TokenType::KW_WILD;
    }
    
    bool isType() const {
        return type >= TokenType::KW_INT8 && type <= TokenType::KW_WILD;
    }
    
    bool isOperator() const {
        return (type >= TokenType::PLUS && type <= TokenType::NOT) ||
               (type >= TokenType::ASSIGN && type <= TokenType::MINUS_ASSIGN);
    }
};

/**
 * Convert token type to string (for error messages)
 */
const char* tokenTypeToString(TokenType type);

} // namespace parser
} // namespace ariash
