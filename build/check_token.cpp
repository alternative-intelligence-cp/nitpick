#include <iostream>
int main() {
    // Based on tokens.h enum counting from 0
    const char* tokens[] = {
        "TOKEN_EOF", "TOKEN_INVALID", "TOKEN_UNKNOWN",
        "TOKEN_INT_LITERAL", "TOKEN_FLOAT_LITERAL", "TOKEN_STRING_LITERAL", "TOKEN_CHAR_LITERAL", "TOKEN_TRIT_LITERAL", "TOKEN_TEMPLATE_LITERAL",
        "TOKEN_IDENTIFIER"
    };
    std::cout << "Token 9: " << tokens[9] << std::endl;
    return 0;
}
