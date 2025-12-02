#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_LBRACKET = " << (int)aria::frontend::TOKEN_LBRACKET << std::endl;
    std::cout << "TOKEN_RBRACE = " << (int)aria::frontend::TOKEN_RBRACE << std::endl;
    std::cout << "TOKEN_PIPE_BACKWARD = " << (int)aria::frontend::TOKEN_PIPE_BACKWARD << std::endl;
    return 0;
}
