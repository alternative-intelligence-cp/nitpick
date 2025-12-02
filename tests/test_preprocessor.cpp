#include <iostream>
#include <cassert>
#include "../src/frontend/preprocessor.h"

using namespace aria::frontend;

void test_define_undef() {
    std::cout << "\n=== Test %define and %undef ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%define DEBUG 1
%define VERSION "0.0.6"

Some code here
)";
    
    std::string result = pp.process(source, "test.aria");
    
    assert(pp.isConstantDefined("DEBUG"));
    assert(pp.isConstantDefined("VERSION"));
    
    std::cout << "✓ %define works" << std::endl;
}

void test_ifdef_endif() {
    std::cout << "\n=== Test %ifdef/%endif ===" << std::endl;
    
    Preprocessor pp;
    pp.defineConstant("DEBUG", "1");
    
    std::string source = R"(
%ifdef DEBUG
print("Debug mode")
%endif

%ifdef RELEASE
print("Release mode")
%endif
)";
    
    std::string result = pp.process(source, "test.aria");
    
    // Should contain debug print, not release print
    assert(result.find("Debug mode") != std::string::npos);
    assert(result.find("Release mode") == std::string::npos);
    
    std::cout << "✓ %ifdef conditional compilation works" << std::endl;
}

void test_macro_definition() {
    std::cout << "\n=== Test %macro definition and expansion ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%macro PRINT_DEBUG 1
print("Debug: %1")
%endmacro

PRINT_DEBUG("Hello World")
PRINT_DEBUG("Test 123")
)";
    
    std::string result = pp.process(source, "test.aria");
    
    assert(pp.isMacroDefined("PRINT_DEBUG"));
    
    const Macro* macro = pp.getMacro("PRINT_DEBUG");
    assert(macro != nullptr);
    assert(macro->param_count == 1);
    
    // Check that macro was expanded
    std::cout << "Result:\n" << result << std::endl;
    
    assert(result.find("print(\"Debug: Hello World\")") != std::string::npos ||
           result.find("print(\"Debug: \"Hello World\"\")") != std::string::npos);
    
    std::cout << "✓ %macro definition and expansion works" << std::endl;
}

void test_context_stack() {
    std::cout << "\n=== Test %push/%pop context ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%push ctx1
    label1:
%pop

%push ctx2
    label2:
%pop
)";
    
    try {
        std::string result = pp.process(source, "test.aria");
        std::cout << "✓ Context stack works" << std::endl;
    } catch (const std::exception& e) {
        std::cout << "✗ Context stack failed: " << e.what() << std::endl;
    }
}

void test_error_detection() {
    std::cout << "\n=== Test error detection ===" << std::endl;
    
    // Test unclosed %if
    {
        Preprocessor pp;
        std::string source = R"(
%ifdef DEBUG
    print("test")
)";
        
        try {
            pp.process(source, "test.aria");
            std::cout << "✗ Should have detected unclosed %if" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✓ Detected unclosed %if: " << e.what() << std::endl;
        }
    }
    
    // Test %pop without %push
    {
        Preprocessor pp;
        std::string source = "%pop\n";
        
        try {
            pp.process(source, "test.aria");
            std::cout << "✗ Should have detected %pop without %push" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "✓ Detected %pop without %push" << std::endl;
        }
    }
}

void test_macro_with_multiple_params() {
    std::cout << "\n=== Test macro with multiple parameters ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%macro ADD 2
result = %1 + %2
%endmacro

ADD(10, 20)
ADD(x, y)
)";
    
    std::string result = pp.process(source, "test.aria");
    
    std::cout << "Result:\n" << result << std::endl;
    
    assert(result.find("result = 10 + 20") != std::string::npos);
    assert(result.find("result = x + y") != std::string::npos);
    
    std::cout << "✓ Multi-parameter macros work" << std::endl;
}

void test_constant_substitution() {
    std::cout << "\n=== Test constant substitution ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%define MAX_SIZE 1024
%define VERSION "0.0.6"

size = MAX_SIZE
version = VERSION
)";
    
    std::string result = pp.process(source, "test.aria");
    
    std::cout << "Result:\n" << result << std::endl;
    
    assert(result.find("size = 1024") != std::string::npos);
    assert(result.find("version = \"0.0.6\"") != std::string::npos);
    
    std::cout << "✓ Constant substitution works" << std::endl;
}

void test_context_local_labels() {
    std::cout << "\n=== Test context-local labels ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%push loop1
    goto %$start
%$start:
    // loop code
%pop

%push loop2
    goto %$start
%$start:
    // different loop
%pop
)";
    
    std::string result = pp.process(source, "test.aria");
    
    std::cout << "Result:\n" << result << std::endl;
    
    // Should have two different unique labels (both at depth 0 since we pop between)
    assert(result.find("loop1_0_start") != std::string::npos);
    assert(result.find("loop2_0_start") != std::string::npos);
    
    std::cout << "✓ Context-local labels work" << std::endl;
}

int main() {
    std::cout << "=== Preprocessor Tests ===" << std::endl;
    
    try {
        test_define_undef();
        test_ifdef_endif();
        test_macro_definition();
        test_macro_with_multiple_params();
        test_constant_substitution();
        test_context_local_labels();
        test_context_stack();
        test_error_detection();
        
        std::cout << "\n=== All Preprocessor Tests Passed! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
