#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_SEMICOLON = " << (int)aria::frontend::TOKEN_SEMICOLON << std::endl;
    std::cout << "TOKEN_RBRACE = " << (int)aria::frontend::TOKEN_RBRACE << std::endl;
    std::cout << "TOKEN_EOF = " << (int)aria::frontend::TOKEN_EOF << std::endl;
    return 0;
}
