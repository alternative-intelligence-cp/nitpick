#include "src/frontend/tokens.h"
#include <iostream>

using namespace aria::frontend;

int main() {
    std::cout << "TOKEN_IDENTIFIER = " << TOKEN_IDENTIFIER << std::endl;
    std::cout << "TOKEN_LBRACKET = " << TOKEN_LBRACKET << std::endl;
    std::cout << "TOKEN_INT_LITERAL = " << TOKEN_INT_LITERAL << std::endl;
    return 0;
}
