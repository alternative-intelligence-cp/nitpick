/**
 * acat_demo.cpp
 * Interactive demonstration of the acat (file concatenation) utility
 *
 * Shows six-stream architecture and TBB-safe file operations
 */

#include "acat/file_cat.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <unistd.h>

using namespace aria::acat;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Create test files for demos
void setup_test_files() {
    // Test file 1
    std::ofstream f1("/tmp/acat_test1.txt");
    f1 << "Line 1: First file content\n";
    f1 << "Line 2: More data here\n";
    f1 << "Line 3: Final line\n";
    f1.close();

    // Test file 2
    std::ofstream f2("/tmp/acat_test2.txt");
    f2 << "Line 1: Second file begins\n";
    f2 << "Line 2: Additional content\n";
    f2.close();

    // Test file 3 with special characters
    std::ofstream f3("/tmp/acat_special.txt");
    f3 << "Tab\there\n";
    f3 << "Newline test\n";
    f3 << "\n";  // Blank line
    f3 << "After blank\n";
    f3.close();

    // Large file for performance testing
    std::ofstream f4("/tmp/acat_large.txt");
    for (int i = 0; i < 1000; ++i) {
        f4 << "Line " << i << ": This is a test line with some content\n";
    }
    f4.close();
}

// Demo 1: Simple file concatenation
void demo_simple_cat() {
    print_section("Demo 1: Simple File Concatenation");

    CatOptions opts;
    auto result = cat_file("/tmp/acat_test1.txt", STDOUT_FILENO, opts);

    if (result.is_ok()) {
        std::cout << "\n✓ Read " << result.bytes_read << " bytes in "
                  << std::fixed << std::setprecision(2) << result.elapsed_ms << " ms\n";
    } else {
        std::cout << "✗ Error: " << result.error_message << "\n";
    }
}

// Demo 2: Concatenate multiple files
void demo_multi_file() {
    print_section("Demo 2: Concatenate Multiple Files");

    std::vector<std::string> files = {
        "/tmp/acat_test1.txt",
        "/tmp/acat_test2.txt"
    };

    CatOptions opts;
    auto result = cat_files(files, STDOUT_FILENO, opts);

    std::cout << "\n✓ Processed " << result.files_processed << " files\n";
    std::cout << "  Total bytes: " << result.bytes_read << "\n";
    std::cout << "  Total lines: " << result.lines_read << "\n";
}

// Demo 3: Line numbering
void demo_line_numbers() {
    print_section("Demo 3: Line Numbering");

    CatOptions opts;
    opts.show_line_numbers = true;

    auto result = cat_file("/tmp/acat_test1.txt", STDOUT_FILENO, opts);

    std::cout << "\n✓ Numbered " << result.lines_read << " lines\n";
}

// Demo 4: Squeeze blank lines
void demo_squeeze_blank() {
    print_section("Demo 4: Squeeze Multiple Blank Lines");

    CatOptions opts;
    opts.squeeze_blank = true;
    opts.show_line_numbers = true;

    auto result = cat_file("/tmp/acat_special.txt", STDOUT_FILENO, opts);

    std::cout << "\n✓ Blank lines squeezed\n";
}

// Demo 5: Line limits
void demo_limits() {
    print_section("Demo 5: Line Limits");

    std::cout << "First 2 lines only:\n";
    CatOptions opts;
    opts.line_limit = 2;
    opts.show_line_numbers = true;
    auto result = cat_file("/tmp/acat_test1.txt", STDOUT_FILENO, opts);
    std::cout << "\n✓ Limited to " << result.lines_read << " lines\n";
}

// Demo 6: TBB-safe byte counting
void demo_tbb_safe() {
    print_section("Demo 6: TBB-Safe Byte Counting");

    std::cout << "Processing large file with TBB arithmetic:\n\n";

    CatOptions opts;
    opts.show_line_numbers = true;
    opts.line_limit = 5;  // Just show first 5 lines
    
    auto result = cat_file("/tmp/acat_large.txt", STDOUT_FILENO, opts);

    std::cout << "\nTBB-safe counters:\n";
    std::cout << "  bytes_read: " << result.bytes_read 
              << (is_tbb_err(result.bytes_read) ? " (ERR sentinel)" : " (valid)") << "\n";
    std::cout << "  lines_read: " << result.lines_read 
              << (is_tbb_err(result.lines_read) ? " (ERR sentinel)" : " (valid)") << "\n";
}

// Demo 7: Error handling
void demo_error_handling() {
    print_section("Demo 7: Error Handling");

    // Try to cat non-existent file
    auto result1 = cat_file("/tmp/nonexistent_file.txt");

    std::cout << "Non-existent file:\n";
    std::cout << "  Error code: " << static_cast<int>(result1.error) << "\n";
    std::cout << "  Message: " << result1.error_message << "\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   acat - Interactive Demonstration                        ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  File concatenation with six-stream architecture          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    // Setup test environment
    setup_test_files();

    try {
        demo_simple_cat();
        demo_multi_file();
        demo_line_numbers();
        demo_squeeze_blank();
        demo_limits();
        demo_tbb_safe();
        demo_error_handling();

        std::cout << "\n✓ All 7 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Cleanup
    std::remove("/tmp/acat_test1.txt");
    std::remove("/tmp/acat_test2.txt");
    std::remove("/tmp/acat_special.txt");
    std::remove("/tmp/acat_large.txt");

    return 0;
}
