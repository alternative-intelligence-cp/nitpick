#include "../src/frontend/tokens.h"
#include <iostream>

int main() {
    std::cout << "Looking for token type 86..." << std::endl;
    std::cout << "TOKEN_TYPE_UINT8 = " << aria::frontend::TOKEN_TYPE_UINT8 << std::endl;
    std::cout << "TOKEN_TYPE_UINT16 = " << aria::frontend::TOKEN_TYPE_UINT16 << std::endl;
    std::cout << "TOKEN_TYPE_FLT32 = " << aria::frontend::TOKEN_TYPE_FLT32 << std::endl;
    std::cout << "TOKEN_TYPE_FLT64 = " << aria::frontend::TOKEN_TYPE_FLT64 << std::endl;
    std::cout << "TOKEN_TYPE_TRIT = " << aria::frontend::TOKEN_TYPE_TRIT << std::endl;
    std::cout << "TOKEN_TYPE_TRYTE = " << aria::frontend::TOKEN_TYPE_TRYTE << std::endl;
    std::cout << "TOKEN_TYPE_NIT = " << aria::frontend::TOKEN_TYPE_NIT << std::endl;
    std::cout << "TOKEN_TYPE_NYTE = " << aria::frontend::TOKEN_TYPE_NYTE << std::endl;
    std::cout << "TOKEN_TYPE_VEC2 = " << aria::frontend::TOKEN_TYPE_VEC2 << std::endl;
    return 0;
}
