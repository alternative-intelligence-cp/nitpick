#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "Token 151 = " << 151 << std::endl;
    std::cout << "Token 119 = " << 119 << std::endl;
    std::cout << "TOKEN_PLUS = " << (int)aria::frontend::TOKEN_PLUS << std::endl;
    std::cout << "TOKEN_INT_LITERAL = " << (int)aria::frontend::TOKEN_INT_LITERAL << std::endl;
    std::cout << "TOKEN_IDENTIFIER = " << (int)aria::frontend::TOKEN_IDENTIFIER << std::endl;
    std::cout << "TOKEN_COMMA = " << (int)aria::frontend::TOKEN_COMMA << std::endl;
    return 0;
}
