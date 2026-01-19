/**
 * Aria Build Configuration (ABC) Lexer
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 *
 * The lexer transforms raw source text into a stream of tokens for the parser.
 * Key features:
 * - Whitespace-insensitive (eliminates "invisible" syntax errors)
 * - Backtick strings with &{VAR} interpolation support
 * - C++ style comments (//)
 * - Unquoted identifiers for keys
 */

#ifndef ABC_LEXER_HPP
#define ABC_LEXER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace abc {

/**
 * Token types for the ABC format
 */
enum class TokenType {
    // Structural tokens
    LEFT_BRACE,     // {
    RIGHT_BRACE,    // }
    LEFT_BRACKET,   // [
    RIGHT_BRACKET,  // ]

    // Separators
    COLON,          // :
    COMMA,          // ,

    // Data tokens
    STRING_LITERAL, // `text` or `text with &{var}`
    IDENTIFIER,     // unquoted key names
    INTEGER,        // numeric literal
    BOOLEAN_TRUE,   // true
    BOOLEAN_FALSE,  // false
    NULL_LITERAL,   // null

    // Interpolation tokens
    INTERP_START,   // &{
    INTERP_END,     // } (within interpolation context)

    // Control tokens
    END_OF_FILE,    // end of input
    INVALID         // lexical error
};

/**
 * Token structure - lightweight, trivially copyable
 *
 * Contains the token type, the raw lexeme, and source location.
 */
struct Token {
    TokenType type;
    std::string_view lexeme;    // View into source buffer (zero-copy)
    uint32_t line;
    uint32_t column;

    Token() : type(TokenType::INVALID), lexeme(""), line(0), column(0) {}
    Token(TokenType t, std::string_view lex, uint32_t ln, uint32_t col)
        : type(t), lexeme(lex), line(ln), column(col) {}

    // Helper to check token type
    bool is(TokenType t) const { return type == t; }
    bool isNot(TokenType t) const { return type != t; }

    // Check if this is a value-starting token
    bool isValueStart() const {
        return type == TokenType::STRING_LITERAL ||
               type == TokenType::IDENTIFIER ||
               type == TokenType::INTEGER ||
               type == TokenType::BOOLEAN_TRUE ||
               type == TokenType::BOOLEAN_FALSE ||
               type == TokenType::NULL_LITERAL ||
               type == TokenType::LEFT_BRACE ||
               type == TokenType::LEFT_BRACKET;
    }
};

/**
 * Get string representation of token type (for debugging/errors)
 */
const char* tokenTypeName(TokenType type);

/**
 * Lexer class - transforms source text into tokens
 *
 * Implements a deterministic finite automaton (DFA) with modal states
 * for handling string interpolation.
 */
class Lexer {
public:
    /**
     * Construct lexer from source string
     *
     * @param source The ABC configuration source text
     * @param filename Optional filename for error reporting
     */
    explicit Lexer(std::string_view source, std::string_view filename = "<input>");

    /**
     * Get the next token from the input stream
     *
     * @return The next token (EOF when input exhausted)
     */
    Token nextToken();

    /**
     * Peek at the next token without consuming it
     *
     * @return The next token
     */
    Token peekToken();

    /**
     * Check if at end of input
     */
    bool isAtEnd() const { return current >= source.size(); }

    /**
     * Get current line number
     */
    uint32_t getLine() const { return line; }

    /**
     * Get current column number
     */
    uint32_t getColumn() const { return column; }

    /**
     * Get error messages from lexing
     */
    const std::vector<std::string>& getErrors() const { return errors; }

    /**
     * Check if there were errors
     */
    bool hasErrors() const { return !errors.empty(); }

private:
    std::string_view source;
    std::string_view filename;
    size_t start = 0;           // Start of current token
    size_t current = 0;         // Current position
    uint32_t line = 1;
    uint32_t column = 1;
    bool hasPeeked = false;
    Token peekedToken;
    std::vector<std::string> errors;

    // State machine modes
    enum class LexState {
        ROOT,           // Normal scanning
        STRING,         // Inside backtick string
        INTERPOLATION   // Inside &{...}
    };
    LexState state = LexState::ROOT;

    // Character helpers
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);
    bool isDigit(char c) const;
    bool isAlpha(char c) const;
    bool isAlphaNumeric(char c) const;

    // Token scanning
    void skipWhitespace();
    void skipComment();
    Token scanToken();
    Token scanString();
    Token scanIdentifier();
    Token scanNumber();

    // Token construction
    Token makeToken(TokenType type);
    Token makeToken(TokenType type, std::string_view lexeme);
    Token errorToken(const char* message);
};

} // namespace abc

#endif // ABC_LEXER_HPP
