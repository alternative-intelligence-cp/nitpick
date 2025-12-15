#ifndef ARIA_FRONTEND_LEXER_H
#define ARIA_FRONTEND_LEXER_H

#include "tokens.h"
#include <string>
#include <stack>

namespace aria {
namespace frontend {

// Lexer State for Template String Parsing
enum LexerState {
    STATE_ROOT,             // Normal code parsing
    STATE_STRING_TEMPLATE,  // Inside template literal (backtick string)
    STATE_INTERPOLATION     // Inside interpolation block &{...}
};

// Aria Lexer
// Tokenizes Aria source code into a stream of tokens
class AriaLexer {
private:
    std::string source;
    size_t pos;
    size_t line;
    size_t col;
    std::stack<LexerState> stateStack;

    char peek();
    char peekNext();
    void advance();
    std::string parseIdentifier();

public:
    AriaLexer(std::string src);
    Token nextToken();
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_LEXER_H
