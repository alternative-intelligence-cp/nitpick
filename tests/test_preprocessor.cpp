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
    std::cout << "\n=== Test %macro definition ===" << std::endl;
    
    Preprocessor pp;
    std::string source = R"(
%macro PRINT_DEBUG 1
    print("Debug: %1")
%endmacro

Some code
)";
    
    std::string result = pp.process(source, "test.aria");
    
    assert(pp.isMacroDefined("PRINT_DEBUG"));
    
    const Macro* macro = pp.getMacro("PRINT_DEBUG");
    assert(macro != nullptr);
    assert(macro->param_count == 1);
    
    std::cout << "✓ %macro definition works" << std::endl;
    std::cout << "  Macro body: " << macro->body << std::endl;
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

int main() {
    std::cout << "=== Preprocessor Tests ===" << std::endl;
    
    try {
        test_define_undef();
        test_ifdef_endif();
        test_macro_definition();
        test_context_stack();
        test_error_detection();
        
        std::cout << "\n=== All Preprocessor Tests Passed! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
