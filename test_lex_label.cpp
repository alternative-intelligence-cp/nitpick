#include "src/frontend/lexer.h"
#include <iostream>

int main() {
    std::string code = "positive: (>0)";
    aria::frontend::AriaLexer lexer(code);
    
    while (true) {
        aria::frontend::Token tok = lexer.getNextToken();
        std::cout << "Token: " << tok.type << " = '" << tok.value << "' at " << tok.line << ":" << tok.col << std::endl;
        if (tok.type == aria::frontend::TOKEN_EOF) break;
    }
    return 0;
}
