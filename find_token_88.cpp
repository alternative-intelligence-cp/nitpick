#include "src/frontend/tokens.h"
#include <iostream>

using namespace aria::frontend;

int main() {
    std::cout << "Checking token value 88..." << std::endl;
    std::cout << "TOKEN_TYPE_UINT64 = " << TOKEN_TYPE_UINT64 << std::endl;
    std::cout << "TOKEN_TYPE_UINT128 = " << TOKEN_TYPE_UINT128 << std::endl;
    std::cout << "TOKEN_TYPE_UINT256 = " << TOKEN_TYPE_UINT256 << std::endl;
    std::cout << "TOKEN_TYPE_UINT512 = " << TOKEN_TYPE_UINT512 << std::endl;
    std::cout << "TOKEN_TYPE_FLT32 = " << TOKEN_TYPE_FLT32 << std::endl;
    std::cout << "TOKEN_TYPE_FLT64 = " << TOKEN_TYPE_FLT64 << std::endl;
    std::cout << "TOKEN_TYPE_FLT128 = " << TOKEN_TYPE_FLT128 << std::endl;
    std::cout << "TOKEN_TYPE_FLT256 = " << TOKEN_TYPE_FLT256 << std::endl;
    return 0;
}
