#include "../src/frontend/lexer.h"
#include <iostream>

using namespace aria::frontend;

int main() {
    AriaLexer lexer("`The value is &{x}`");
    
    Token tok;
    do {
        tok = lexer.nextToken();
        std::cout << "Token: type=" << tok.type 
                  << " value=\"" << tok.value << "\""
                  << " line=" << tok.line 
                  << " col=" << tok.col << std::endl;
    } while (tok.type != TOKEN_EOF && tok.type != TOKEN_INVALID);
    
    return 0;
}
