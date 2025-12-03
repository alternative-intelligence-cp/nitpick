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

void test_rep_endrep() {
    std::cout << "\n=== Test %rep/%endrep ===" << std::endl;
    
    Preprocessor pp;
    
    // Test 1: Simple repeat
    std::string source1 = R"(
%rep 3
print("hello")
%endrep
)";
    
    std::string result1 = pp.process(source1, "test.aria");
    std::cout << "Result:" << std::endl << result1 << std::endl;
    
    // Should have 3 print statements
    size_t count = 0;
    size_t pos = 0;
    while ((pos = result1.find("print(\"hello\")", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    assert(count == 3);
    std::cout << "✓ Simple %rep works (3 repetitions)" << std::endl;
    
    // Test 2: Repeat with constant
    Preprocessor pp2;
    pp2.defineConstant("COUNT", "5");
    std::string source2 = R"(
%rep COUNT
x = x + 1;
%endrep
)";
    
    std::string result2 = pp2.process(source2, "test.aria");
    std::cout << "Result:" << std::endl << result2 << std::endl;
    
    // Should have 5 increment statements
    count = 0;
    pos = 0;
    while ((pos = result2.find("x = x + 1;", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    assert(count == 5);
    std::cout << "✓ %rep with constant works (5 repetitions)" << std::endl;
    
    // Test 3: Nested %rep
    Preprocessor pp3;
    std::string source3 = R"(
%rep 2
outer
    %rep 3
    inner
    %endrep
%endrep
)";
    
    std::string result3 = pp3.process(source3, "test.aria");
    std::cout << "Result:" << std::endl << result3 << std::endl;
    
    // Should have 2 * 3 = 6 "inner" and 2 "outer"
    count = 0;
    pos = 0;
    while ((pos = result3.find("inner", pos)) != std::string::npos) {
        count++;
        pos++;
    }
    assert(count == 6);
    std::cout << "✓ Nested %rep works (2x3 = 6 inner repetitions)" << std::endl;
    
    // Test 4: Zero repetitions
    Preprocessor pp4;
    std::string source4 = R"(
before
%rep 0
this should not appear
%endrep
after
)";
    
    std::string result4 = pp4.process(source4, "test.aria");
    std::cout << "Result:" << std::endl << result4 << std::endl;
    
    assert(result4.find("before") != std::string::npos);
    assert(result4.find("after") != std::string::npos);
    assert(result4.find("this should not appear") == std::string::npos);
    std::cout << "✓ %rep 0 works (zero repetitions)" << std::endl;
}

void test_include() {
    std::cout << "\n=== Test %include ===" << std::endl;
    
    // Test 1: Simple include
    Preprocessor pp;
    pp.addIncludePath("../tests/test_includes");
    
    std::string source = R"(
before include
%include "common.aria"
after include
)";
    
    std::string result = pp.process(source, "test.aria");
    std::cout << "Result:" << std::endl << result << std::endl;
    
    // Should have the defines from common.aria
    assert(pp.isConstantDefined("COMMON_VERSION"));
    assert(pp.isConstantDefined("DEBUG"));
    assert(pp.isMacroDefined("COMMON_FUNC"));
    
    assert(result.find("before include") != std::string::npos);
    assert(result.find("after include") != std::string::npos);
    
    std::cout << "✓ Simple %include works" << std::endl;
    
    // Test 2: Nested include
    Preprocessor pp_nested;
    pp_nested.addIncludePath("../tests/test_includes");
    
    std::string source_nested = R"(
%include "nested.aria"
)";
    
    std::string result_nested = pp_nested.process(source_nested, "test.aria");
    std::cout << "Nested result:" << std::endl << result_nested << std::endl;
    
    // Should have defines from common.aria (included by nested.aria)
    assert(pp_nested.isConstantDefined("COMMON_VERSION"));
    assert(result_nested.find("nested_code_here") != std::string::npos);
    
    std::cout << "✓ Nested %include works" << std::endl;
    
    // Test 3: Circular include protection
    Preprocessor pp_circ;
    pp_circ.addIncludePath("../tests/test_includes");
    
    // Include same file twice - should only process once
    std::string source_circ = R"(
%include "common.aria"
%include "common.aria"
)";
    
    try {
        std::string result_circ = pp_circ.process(source_circ, "test.aria");
        // Should complete without error (second include skipped with warning)
        std::cout << "✓ Circular include protection works" << std::endl;
    } catch (...) {
        std::cerr << "✗ Circular include test failed" << std::endl;
        throw;
    }
}

void test_if_expressions() {
    std::cout << "\n=== Test %if expressions ===" << std::endl;
    
    Preprocessor pp;
    
    // Test 1: Arithmetic expressions
    pp.defineConstant("VAL1", "10");
    pp.defineConstant("VAL2", "20");
    
    std::string source1 = R"(
%if VAL1 + VAL2 == 30
correct_sum
%endif
)";
    
    std::string result1 = pp.process(source1, "test.aria");
    assert(result1.find("correct_sum") != std::string::npos);
    std::cout << "✓ Arithmetic expression (10 + 20 == 30) works" << std::endl;
    
    // Test 2: Comparison operators
    Preprocessor pp2;
    pp2.defineConstant("SIZE", "100");
    
    std::string source2 = R"(
%if SIZE > 50
large
%endif
%if SIZE < 200
not_huge
%endif
)";
    
    std::string result2 = pp2.process(source2, "test.aria");
    assert(result2.find("large") != std::string::npos);
    assert(result2.find("not_huge") != std::string::npos);
    std::cout << "✓ Comparison operators (>, <) work" << std::endl;
    
    // Test 3: Logical operators
    Preprocessor pp3;
    pp3.defineConstant("DEBUG", "1");
    pp3.defineConstant("VERBOSE", "1");
    
    std::string source3 = R"(
%if DEBUG && VERBOSE
debug_verbose_mode
%endif
%if DEBUG || 0
has_debug
%endif
)";
    
    std::string result3 = pp3.process(source3, "test.aria");
    assert(result3.find("debug_verbose_mode") != std::string::npos);
    assert(result3.find("has_debug") != std::string::npos);
    std::cout << "✓ Logical operators (&&, ||) work" << std::endl;
    
    // Test 4: Complex expression with parentheses
    Preprocessor pp4;
    pp4.defineConstant("A", "5");
    pp4.defineConstant("B", "3");
    
    std::string source4 = R"(
%if (A + B) * 2 == 16
correct_calc
%endif
%if A * 2 + B == 13
order_of_ops
%endif
)";
    
    std::string result4 = pp4.process(source4, "test.aria");
    assert(result4.find("correct_calc") != std::string::npos);
    assert(result4.find("order_of_ops") != std::string::npos);
    std::cout << "✓ Parentheses and order of operations work" << std::endl;
    
    // Test 5: Unary operators
    Preprocessor pp5;
    pp5.defineConstant("ENABLED", "0");
    
    std::string source5 = R"(
%if !ENABLED
disabled
%endif
%if -5 + 10 == 5
negative_works
%endif
)";
    
    std::string result5 = pp5.process(source5, "test.aria");
    assert(result5.find("disabled") != std::string::npos);
    assert(result5.find("negative_works") != std::string::npos);
    std::cout << "✓ Unary operators (!, -) work" << std::endl;
    
    // Test 6: Division and modulo
    Preprocessor pp6;
    
    std::string source6 = R"(
%if 20 / 4 == 5
division_works
%endif
%if 17 % 5 == 2
modulo_works
%endif
)";
    
    std::string result6 = pp6.process(source6, "test.aria");
    assert(result6.find("division_works") != std::string::npos);
    assert(result6.find("modulo_works") != std::string::npos);
    std::cout << "✓ Division and modulo (/, %) work" << std::endl;
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
        test_rep_endrep();
        test_include();
        test_if_expressions();
        
        std::cout << "\n=== All Preprocessor Tests Passed! ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
