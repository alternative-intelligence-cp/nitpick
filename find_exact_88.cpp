#include "src/frontend/tokens.h"
#include <iostream>

using namespace aria::frontend;

int main() {
    std::cout << "TOKEN_TYPE_TRYTE = " << TOKEN_TYPE_TRYTE << std::endl;  // Should be around 76
    std::cout << "TOKEN_TYPE_NIT = " << TOKEN_TYPE_NIT << std::endl;
    std::cout << "TOKEN_TYPE_NYTE = " << TOKEN_TYPE_NYTE << std::endl;
    std::cout << "TOKEN_TYPE_BYTE = " << TOKEN_TYPE_BYTE << std::endl;
    std::cout << "TOKEN_TYPE_VEC2 = " << TOKEN_TYPE_VEC2 << std::endl;  // Around 80
    std::cout << "TOKEN_TYPE_VEC3 = " << TOKEN_TYPE_VEC3 << std::endl;
    std::cout << "TOKEN_TYPE_VEC9 = " << TOKEN_TYPE_VEC9 << std::endl;
    std::cout << "TOKEN_TYPE_MATRIX = " << TOKEN_TYPE_MATRIX << std::endl;
    std::cout << "TOKEN_TYPE_TENSOR = " << TOKEN_TYPE_TENSOR << std::endl;
    std::cout << "TOKEN_TYPE_FUNC = " << TOKEN_TYPE_FUNC << std::endl;
    std::cout << "TOKEN_TYPE_RESULT = " << TOKEN_TYPE_RESULT << std::endl;  // Around 86
    std::cout << "TOKEN_TYPE_BINARY = " << TOKEN_TYPE_BINARY << std::endl;
    std::cout << "TOKEN_TYPE_BUFFER = " << TOKEN_TYPE_BUFFER << std::endl;
    std::cout << "TOKEN_TYPE_STREAM = " << TOKEN_TYPE_STREAM << std::endl;
    std::cout << "TOKEN_TYPE_PROCESS = " << TOKEN_TYPE_PROCESS << std::endl;
    std::cout << "TOKEN_TYPE_PIPE = " << TOKEN_TYPE_PIPE << std::endl;
    std::cout << "TOKEN_TYPE_DYN = " << TOKEN_TYPE_DYN << std::endl;
    return 0;
}
