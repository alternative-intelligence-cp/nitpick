#include "../src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_TRIT_LITERAL = " << aria::frontend::TOKEN_TRIT_LITERAL << std::endl;
    std::cout << "TOKEN_TYPE_INT8 = " << aria::frontend::TOKEN_TYPE_INT8 << std::endl;
    std::cout << "TOKEN_LPAREN = " << aria::frontend::TOKEN_LPAREN << std::endl;
    std::cout << "TOKEN_IDENTIFIER = " << aria::frontend::TOKEN_IDENTIFIER << std::endl;
    return 0;
}
