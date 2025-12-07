#include <iostream>
#include "../src/frontend/lexer.h"

int main() {
    std::string code = "while (x > 0) { x = x - 1; }";
    aria::frontend::AriaLexer lexer(code);
    
    aria::frontend::Token tok;
    while ((tok = lexer.nextToken()).type != aria::frontend::TOKEN_EOF) {
        std::cout << "Token: type=" << tok.type << " value='" << tok.value << "'\n";
    }
    return 0;
}
