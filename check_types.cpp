#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_TYPE_TRIT = " << (int)aria::frontend::TOKEN_TYPE_TRIT << std::endl;
    std::cout << "TOKEN_TYPE_VEC2 = " << (int)aria::frontend::TOKEN_TYPE_VEC2 << std::endl;
    std::cout << "TOKEN_TYPE_FUNC = " << (int)aria::frontend::TOKEN_TYPE_FUNC << std::endl;
    std::cout << "TOKEN_TYPE_TENSOR = " << (int)aria::frontend::TOKEN_TYPE_TENSOR << std::endl;
    return 0;
}
