#include "src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "TOKEN_SAFE_NAV = " << (int)aria::frontend::TOKEN_SAFE_NAV << std::endl;
    std::cout << "TOKEN_COLON = " << (int)aria::frontend::TOKEN_COLON << std::endl;
    std::cout << "TOKEN_INVALID = " << (int)aria::frontend::TOKEN_INVALID << std::endl;
    return 0;
}
