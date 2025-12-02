#include <iostream>
#include <vector>
#include "../src/frontend/lexer.h"

using namespace aria::frontend;

std::vector<Token> tokenizeAll(AriaLexer& lexer) {
    std::vector<Token> tokens;
    while (true) {
        Token tok = lexer.nextToken();
        if (tok.type == TOKEN_EOF) break;
        tokens.push_back(tok);
    }
    return tokens;
}

void test_hex_escapes() {
    std::cout << "\n=== Testing Hex Escapes ===" << std::endl;
    
    AriaLexer lexer("'\\x41' '\\x0A' '\\x09'");
    auto tokens = tokenizeAll(lexer);
    
    for (const auto& tok : tokens) {
        if (tok.type == TOKEN_CHAR_LITERAL) {
            std::cout << "Char literal: '" << tok.value << "' (";
            for (unsigned char c : tok.value) {
                std::cout << (int)c << " ";
            }
            std::cout << ")" << std::endl;
        }
    }
    
    // Verify first token is 'A' (0x41 = 65)
    if (tokens.size() >= 1 && tokens[0].type == TOKEN_CHAR_LITERAL) {
        if (tokens[0].value.length() > 0 && tokens[0].value[0] == 'A') {
            std::cout << "✓ Hex escape \\x41 = 'A' works!" << std::endl;
        }
    }
}

void test_preprocessor() {
    std::cout << "\n=== Testing Preprocessor Directives ===" << std::endl;
    
    AriaLexer lexer("%macro TEST 2\n%define FOO\n%1 %2 %$label");
    auto tokens = tokenizeAll(lexer);
    
    for (const auto& tok : tokens) {
        std::cout << "Token: " << tok.value << " (type=" << tok.type << ")" << std::endl;
    }
    
    // Verify first token is %macro
    if (tokens.size() >= 1 && tokens[0].type == TOKEN_PREPROC_MACRO) {
        std::cout << "✓ Preprocessor directive %macro recognized!" << std::endl;
    }
}

void test_modulo() {
    std::cout << "\n=== Testing Modulo Operator ===" << std::endl;
    
    AriaLexer lexer("10 % 3");
    auto tokens = tokenizeAll(lexer);
    
    std::cout << "Token 0: " << tokens[0].value << " (type=" << tokens[0].type << ")" << std::endl;
    std::cout << "Token 1: " << tokens[1].value << " (type=" << tokens[1].type << ")" << std::endl;
    std::cout << "Token 2: " << tokens[2].value << " (type=" << tokens[2].type << ")" << std::endl;
    
    if (tokens[1].type == TOKEN_PERCENT) {
        std::cout << "✓ Modulo operator still works!" << std::endl;
    } else {
        std::cout << "✗ Modulo operator broken!" << std::endl;
    }
}

int main() {
    test_hex_escapes();
    test_preprocessor();
    test_modulo();
    
    std::cout << "\n=== All Lexer Tests Complete ===" << std::endl;
    return 0;
}
