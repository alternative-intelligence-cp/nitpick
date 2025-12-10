/**
 * tests/test_diagnostic.cpp
 * 
 * Test suite for DiagnosticEngine
 * Demonstrates multi-error reporting
 */

#include "../src/frontend/diagnostic.h"
#include <cassert>
#include <sstream>
#include <iostream>

using namespace aria::frontend;

void test_single_error() {
    std::cout << "\n=== Test: Single Error ===\n";
    
    std::string source = "int x = \"hello\";  // Type mismatch\n";
    DiagnosticEngine diag("test.aria", source, false);  // Disable color for testing
    
    diag.error(1, 9, "Cannot assign string literal to int variable");
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    assert(output.find("error:") != std::string::npos);
    assert(output.find("test.aria:1:9") != std::string::npos);
    assert(diag.has_errors());
    assert(diag.get_error_count() == 1);
    
    std::cout << output;
    std::cout << "✓ Single error reported correctly\n";
}

void test_multiple_errors() {
    std::cout << "\n=== Test: Multiple Errors ===\n";
    
    std::string source = 
        "int x = \"hello\";\n"
        "int y = 42\n"         // Missing semicolon
        "func foo() {\n"
        "    return \"test\";\n"
        "}\n";
    
    DiagnosticEngine diag("test.aria", source, false);
    
    // Report multiple errors
    diag.error(1, 9, "Cannot assign string literal to int variable");
    diag.error(2, 11, "Expected ';' after statement");
    diag.error(4, 12, "Function 'foo' has no return type specified");
    
    assert(diag.get_error_count() == 3);
    assert(diag.has_errors());
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    assert(output.find("3 errors") != std::string::npos);
    
    std::cout << output;
    std::cout << "✓ Multiple errors collected and reported\n";
}

void test_warnings() {
    std::cout << "\n=== Test: Warnings ===\n";
    
    std::string source = 
        "int x = 42;\n"
        "int y = x + 1;\n"
        "// x is never used after this\n";
    
    DiagnosticEngine diag("test.aria", source, false);
    
    diag.warning(1, 5, "Variable 'x' is assigned but never used", "Remove unused variable");
    diag.warning(2, 5, "Variable 'y' is declared but never used");
    
    assert(diag.get_warning_count() == 2);
    assert(!diag.has_errors());  // Warnings don't count as errors
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    assert(output.find("warning:") != std::string::npos);
    assert(output.find("2 warnings") != std::string::npos);
    assert(output.find("help: Remove unused variable") != std::string::npos);
    
    std::cout << output;
    std::cout << "✓ Warnings reported with suggestions\n";
}

void test_mixed_diagnostics() {
    std::cout << "\n=== Test: Mixed Errors and Warnings ===\n";
    
    std::string source = 
        "int x = 42;\n"
        "int y = \"wrong\";\n"
        "int z = x + y;\n";
    
    DiagnosticEngine diag("test.aria", source, false);
    
    diag.error(2, 9, "Cannot assign string literal to int variable");
    diag.warning(1, 5, "Variable 'x' may be uninitialized");
    diag.error(3, 13, "Cannot add int and string types");
    diag.note(3, 9, "Variable 'y' was declared as string here");
    
    assert(diag.get_error_count() == 2);
    assert(diag.get_warning_count() == 1);
    assert(diag.get_diagnostic_count() == 4);
    assert(diag.has_errors());
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    assert(output.find("2 errors") != std::string::npos);
    assert(output.find("1 warning") != std::string::npos);
    
    std::cout << output;
    std::cout << "✓ Mixed diagnostics reported correctly\n";
}

void test_clear() {
    std::cout << "\n=== Test: Clear Diagnostics ===\n";
    
    std::string source = "int x;\n";
    DiagnosticEngine diag("test.aria", source, false);
    
    diag.error(1, 5, "Error 1");
    diag.warning(1, 5, "Warning 1");
    
    assert(diag.get_error_count() == 1);
    assert(diag.get_warning_count() == 1);
    
    diag.clear();
    
    assert(diag.get_error_count() == 0);
    assert(diag.get_warning_count() == 0);
    assert(!diag.has_errors());
    
    std::cout << "✓ Clear() resets all counters\n";
}

void test_source_context() {
    std::cout << "\n=== Test: Source Context Highlighting ===\n";
    
    std::string source = 
        "func add(int a, int b) -> int {\n"
        "    return a + b\n"  // Missing semicolon
        "}\n";
    
    DiagnosticEngine diag("math.aria", source, false);
    
    diag.error(2, 17, "Expected ';' after return statement");
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    
    // Check for line number in gutter
    assert(output.find("2 |") != std::string::npos);
    
    // Check for source line
    assert(output.find("return a + b") != std::string::npos);
    
    // Check for column indicator
    assert(output.find("^") != std::string::npos);
    
    std::cout << output;
    std::cout << "✓ Source context shows line and column\n";
}

void test_did_you_mean() {
    std::cout << "\n=== Test: 'Did You Mean?' Suggestions ===\n";
    
    std::string source = "int x = ture;\n";  // Typo: ture instead of true
    DiagnosticEngine diag("test.aria", source, false);
    
    diag.error(1, 9, "Undefined identifier 'ture'", "Did you mean 'true'?");
    
    std::ostringstream out;
    diag.print_diagnostics(out);
    
    std::string output = out.str();
    assert(output.find("help: Did you mean 'true'?") != std::string::npos);
    
    std::cout << output;
    std::cout << "✓ Suggestions displayed correctly\n";
}

int main() {
    std::cout << "=====================================\n";
    std::cout << "DiagnosticEngine Test Suite\n";
    std::cout << "Testing Multi-Error Reporting\n";
    std::cout << "=====================================\n";
    
    try {
        test_single_error();
        test_multiple_errors();
        test_warnings();
        test_mixed_diagnostics();
        test_clear();
        test_source_context();
        test_did_you_mean();
        
        std::cout << "\n=====================================\n";
        std::cout << "✅ ALL TESTS PASSED\n";
        std::cout << "=====================================\n";
        std::cout << "\nDiagnosticEngine Features:\n";
        std::cout << "- Multi-error collection (no early exit)\n";
        std::cout << "- Source context highlighting\n";
        std::cout << "- Color-coded output (errors/warnings/notes)\n";
        std::cout << "- 'Did you mean?' suggestions\n";
        std::cout << "- Comprehensive error summaries\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
}
