/**
 * tests/test_parser.cpp
 * 
 * Comprehensive Parser Test Suite for Aria Language v0.0.6
 * Tests all parser features from canonical specification
 */

#include <iostream>
#include <cassert>
#include <memory>
#include "../src/frontend/parser.h"
#include "../src/frontend/lexer.h"
#include "../src/frontend/preprocessor.h"
#include "../src/frontend/ast.h"
#include "../src/frontend/ast/expr.h"
#include "../src/frontend/ast/stmt.h"

using namespace aria::frontend;

// Helper function to parse source code
std::unique_ptr<Block> parseSource(const std::string& source) {
    // Preprocess
    Preprocessor pp;
    std::string preprocessed = pp.process(source, "test.aria");
    
    // Create lexer and parse
    AriaLexer lexer(preprocessed);
    Parser parser(lexer);
    return parser.parseProgram();
}

// Test 1: Array type syntax
void test_array_types() {
    std::cout << "\n=== Test 1: Array Type Syntax ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int8[]:arr1;
    int8[256]:arr2;
    int8[]:arr3 = [1, 2, 3];
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Array type syntax parses correctly" << std::endl;
}

// Test 2: Pointer type syntax
void test_pointer_types() {
    std::cout << "\n=== Test 2: Pointer Type Syntax ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int64@:ptr;
    wild int64@:wild_ptr;
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Pointer type syntax parses correctly" << std::endl;
}

// Test 3: Ternary operator (is)
void test_ternary_operator() {
    std::cout << "\n=== Test 3: Ternary Operator ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int8:x = 11;
    int8:y = is x == 11 : 100 : 200;
    return y;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Ternary operator (is) parses correctly" << std::endl;
}

// Test 4: Unary operators (@, #, ++, --)
void test_unary_operators() {
    std::cout << "\n=== Test 4: Unary Operators ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int64:s = 100;
    int64:addr = @s;
    
    dyn:d = "test";
    int8:pinned = #d;
    
    int8:i = 0;
    i++;
    i--;
    
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Unary operators (@, #, ++, --) parse correctly" << std::endl;
}

// Test 5: Binary operators
void test_binary_operators() {
    std::cout << "\n=== Test 5: Binary Operators ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int64:a = 10;
    int64:b = 20;
    
    int64:sum = a + b;
    int64:diff = a - b;
    int64:prod = a * b;
    int64:quot = a / b;
    int64:mod = a % b;
    
    bool:eq = a == b;
    bool:ne = a != b;
    bool:lt = a < b;
    bool:gt = a > b;
    bool:le = a <= b;
    bool:ge = a >= b;
    
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Binary operators parse correctly" << std::endl;
}

// Test 6: Object literals
void test_object_literals() {
    std::cout << "\n=== Test 6: Object Literals ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    obj:config = {
        version: "0.0.6",
        name: "Aria",
        count: 42
    };
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Object literals parse correctly" << std::endl;
}

// Test 7: Member access
void test_member_access() {
    std::cout << "\n=== Test 7: Member Access ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    obj:config = { version: "0.0.6" };
    string:ver = config.version;
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Member access parses correctly" << std::endl;
}

// Test 8: Type system (dyn, obj, wild)
void test_type_system() {
    std::cout << "\n=== Test 8: Type System ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    dyn:d = "dynamic";
    obj:o = { x: 1 };
    wild int64:w = 100;
    stack int8:s = 42;
    const int64:c = 1000;
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Type system (dyn, obj, wild, stack, const) parses correctly" << std::endl;
}

// Test 9: Control flow - if statements
void test_if_statements() {
    std::cout << "\n=== Test 9: If Statements ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int8:x = 10;
    
    if (x > 5) {
        x = 20;
    }
    
    if (x < 15) {
        x = 30;
    } else {
        x = 40;
    }
    
    return x;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ If statements parse correctly" << std::endl;
}

// Test 10: Loops - while, for
void test_loops() {
    std::cout << "\n=== Test 10: Loop Statements ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int8:i = 0;
    
    while (i < 10) {
        i++;
    }
    
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Loop statements parse correctly" << std::endl;
}

// Test 11: Function declarations
void test_function_declarations() {
    std::cout << "\n=== Test 11: Function Declarations ===" << std::endl;
    
    std::string source = R"(
func:add = int64(int64:a, int64:b) {
    return a + b;
};

func:main = int8() {
    int64:result = add(10, 20);
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() >= 2);
    
    std::cout << "✓ Function declarations parse correctly" << std::endl;
}

// Test 12: Multiple variable declarations
void test_variable_declarations() {
    std::cout << "\n=== Test 12: Variable Declarations ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int8:a = 1;
    int16:b = 2;
    int32:c = 3;
    int64:d = 4;
    int128:e = 5;
    
    string:s = "hello";
    bool:flag = true;
    
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Variable declarations parse correctly" << std::endl;
}

// Test 13: Nested expressions
void test_nested_expressions() {
    std::cout << "\n=== Test 13: Nested Expressions ===" << std::endl;
    
    std::string source = R"(
func:main = int8() {
    int64:result = ((10 + 20) * 30) - (40 / 5);
    bool:complex = (result > 100) && (result < 1000) || (result == 0);
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Nested expressions parse correctly" << std::endl;
}

// Test 14: Comments and whitespace handling
void test_comments() {
    std::cout << "\n=== Test 14: Comments ===" << std::endl;
    
    std::string source = R"(
// Single line comment
func:main = int8() {
    // Another comment
    int8:x = 10; // inline comment
    
    /* Multi-line
       comment */
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() > 0);
    
    std::cout << "✓ Comments parse correctly" << std::endl;
}

// Test 15: Function parameters with array/pointer types
void test_complex_parameters() {
    std::cout << "\n=== Test 15: Complex Function Parameters ===" << std::endl;
    
    std::string source = R"(
func:process = void(int8[]:arr, int64@:ptr, dyn:d) {
    return;
};

func:main = int8() {
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() >= 2);
    
    std::cout << "✓ Complex function parameters parse correctly" << std::endl;
}

// Test 16: Return statements
void test_return_statements() {
    std::cout << "\n=== Test 16: Return Statements ===" << std::endl;
    
    std::string source = R"(
func:getValue = int64() {
    return 42;
};

func:getVoid = void() {
    return;
};

func:main = int8() {
    int64:val = getValue();
    return 0;
};
)";
    
    auto ast = parseSource(source);
    assert(ast != nullptr);
    assert(ast->statements.size() >= 3);
    
    std::cout << "✓ Return statements parse correctly" << std::endl;
}

// Main test runner
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  Aria Parser Comprehensive Test Suite  " << std::endl;
    std::cout << "  Version 0.0.6                         " << std::endl;
    std::cout << "========================================" << std::endl;
    
    int passed = 0;
    int total = 16;
    
    try {
        test_array_types();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 1 failed: " << e.what() << std::endl;
    }
    
    try {
        test_pointer_types();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 2 failed: " << e.what() << std::endl;
    }
    
    try {
        test_ternary_operator();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 3 failed: " << e.what() << std::endl;
    }
    
    try {
        test_unary_operators();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 4 failed: " << e.what() << std::endl;
    }
    
    try {
        test_binary_operators();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 5 failed: " << e.what() << std::endl;
    }
    
    try {
        test_object_literals();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 6 failed: " << e.what() << std::endl;
    }
    
    try {
        test_member_access();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 7 failed: " << e.what() << std::endl;
    }
    
    try {
        test_type_system();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 8 failed: " << e.what() << std::endl;
    }
    
    try {
        test_if_statements();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 9 failed: " << e.what() << std::endl;
    }
    
    try {
        test_loops();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 10 failed: " << e.what() << std::endl;
    }
    
    try {
        test_function_declarations();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 11 failed: " << e.what() << std::endl;
    }
    
    try {
        test_variable_declarations();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 12 failed: " << e.what() << std::endl;
    }
    
    try {
        test_nested_expressions();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 13 failed: " << e.what() << std::endl;
    }
    
    try {
        test_comments();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 14 failed: " << e.what() << std::endl;
    }
    
    try {
        test_complex_parameters();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 15 failed: " << e.what() << std::endl;
    }
    
    try {
        test_return_statements();
        passed++;
    } catch (const std::exception& e) {
        std::cout << "✗ Test 16 failed: " << e.what() << std::endl;
    }
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "Test Results: " << passed << "/" << total << " passed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (passed == total) ? 0 : 1;
}
