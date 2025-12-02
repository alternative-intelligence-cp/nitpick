#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_TYPE_UINT8 = " << (int)aria::frontend::TOKEN_TYPE_UINT8 << std::endl;
    std::cout << "TOKEN_TYPE_INT64 = " << (int)aria::frontend::TOKEN_TYPE_INT64 << std::endl;
    std::cout << "TOKEN_IDENTIFIER = " << (int)aria::frontend::TOKEN_IDENTIFIER << std::endl;
    return 0;
}
