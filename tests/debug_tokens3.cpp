#include "../src/frontend/tokens.h"
#include <iostream>

int main() {
    // Token 86 must be between VEC2=80 and something else
    std::cout << "TOKEN_TYPE_VEC2 = " << aria::frontend::TOKEN_TYPE_VEC2 << std::endl;
    std::cout << "TOKEN_TYPE_VEC3 = " << aria::frontend::TOKEN_TYPE_VEC3 << std::endl;
    std::cout << "TOKEN_TYPE_VEC9 = " << aria::frontend::TOKEN_TYPE_VEC9 << std::endl;
    std::cout << "TOKEN_TYPE_MATRIX = " << aria::frontend::TOKEN_TYPE_MATRIX << std::endl;
    std::cout << "TOKEN_TYPE_TENSOR = " << aria::frontend::TOKEN_TYPE_TENSOR << std::endl;
    std::cout << "TOKEN_TYPE_FUNC = " << aria::frontend::TOKEN_TYPE_FUNC << std::endl;
    std::cout << "TOKEN_TYPE_RESULT = " << aria::frontend::TOKEN_TYPE_RESULT << std::endl;
    std::cout << "TOKEN_TYPE_BINARY = " << aria::frontend::TOKEN_TYPE_BINARY << std::endl;
    std::cout << "TOKEN_TYPE_BUFFER = " << aria::frontend::TOKEN_TYPE_BUFFER << std::endl;
    std::cout << "TOKEN_TYPE_STREAM = " << aria::frontend::TOKEN_TYPE_STREAM << std::endl;
    std::cout << "TOKEN_TYPE_PROCESS = " << aria::frontend::TOKEN_TYPE_PROCESS << std::endl;
    std::cout << "TOKEN_TYPE_PIPE = " << aria::frontend::TOKEN_TYPE_PIPE << std::endl;
    std::cout << "TOKEN_TYPE_DYN = " << aria::frontend::TOKEN_TYPE_DYN << std::endl;
    std::cout << "TOKEN_TYPE_OBJ = " << aria::frontend::TOKEN_TYPE_OBJ << std::endl;
    std::cout << "TOKEN_TYPE_ARRAY = " << aria::frontend::TOKEN_TYPE_ARRAY << std::endl;
    std::cout << "TOKEN_TYPE_STRING = " << aria::frontend::TOKEN_TYPE_STRING << std::endl;
    return 0;
}
