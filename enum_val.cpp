#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    // Print various token values
    std::cout << "TOKEN_TYPE_STRING = " << (int)aria::frontend::TOKEN_TYPE_STRING << std::endl;
    std::cout << "TOKEN_PLUS = " << (int)aria::frontend::TOKEN_PLUS << std::endl;
    std::cout << "TOKEN_LBRACKET = " << (int)aria::frontend::TOKEN_LBRACKET << std::endl;
    std::cout << "TOKEN_RBRACKET = " << (int)aria::frontend::TOKEN_RBRACKET << std::endl;
    return 0;
}
