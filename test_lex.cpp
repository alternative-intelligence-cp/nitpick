#include "src/frontend/lexer.h"
#include <iostream>

int main() {
    std::string source = "int64:x = 42;";
    aria::frontend::Lexer lexer(source);
    
    while (true) {
        auto tok = lexer.next();
        std::cout << "Token " << (int)tok.type << ": '" << tok.value << "'" << std::endl;
        if (tok.type == aria::frontend::TOKEN_EOF) break;
    }
    return 0;
}
