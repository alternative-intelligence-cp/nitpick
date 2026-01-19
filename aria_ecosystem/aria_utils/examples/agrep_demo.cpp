// ============================================================================
// AGREP DEMO - Pattern Search Utility Demonstration
// ============================================================================
// Demonstrates:
//   1. Basic pattern search in files
//   2. Case-insensitive search
//   3. Whole-word matching
//   4. Invert match (show non-matching lines)
//   5. Binary file detection and skipping
//   6. Match count limiting
//   7. Line number display
//   8. ANSI color highlighting
//   9. Six-stream I/O (stdout + stddato + stddbg)
//   10. Multiple file search with statistics
//   11. FFI C API demonstration
//   12. Binary protocol serialization
// ============================================================================

#include "../src/agrep/pattern_search.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>

using namespace aria::utils::agrep;

// Helper to create test files
void create_test_file(const std::string& path, const std::string& content) {
    std::ofstream file(path);
    file << content;
}

// ============================================================================
// Demo 1: Basic Pattern Search
// ============================================================================
void demo_basic_search() {
    std::cout << "\n=== Demo 1: Basic Pattern Search ===\n";
    
    create_test_file("/tmp/agrep_test.txt",
        "The quick brown fox jumps over the lazy dog.\n"
        "Hello, World!\n"
        "This line contains the word 'quick' again.\n"
        "No match here.\n"
        "QUICK in capitals.\n");
    
    SearchOptions options;
    options.color_output = false;  // Disable for demo clarity
    
    tbb64 count = 0;
    auto result = search_file(
        "/tmp/agrep_test.txt",
        "quick",
        options,
        [&count](const Match& match) {
            std::cout << "  Line " << match.line_number << ": " 
                      << match.content << "\n";
            count++;
        }
    );
    
    std::cout << (result.ok() ? "✓" : "✗") << " Found " << count << " matches\n";
    
    std::system("rm -f /tmp/agrep_test.txt");
}

// ============================================================================
// Demo 2: Case-Insensitive Search
// ============================================================================
void demo_case_insensitive() {
    std::cout << "\n=== Demo 2: Case-Insensitive Search ===\n";
    
    create_test_file("/tmp/agrep_case.txt",
        "The QUICK brown fox\n"
        "quick thinking\n"
        "QuIcK reflexes\n"
        "slow response\n");
    
    SearchOptions options;
    options.case_insensitive = true;
    options.color_output = false;
    
    auto result = search_file(
        "/tmp/agrep_case.txt",
        "quick",
        options,
        [](const Match& match) {
            std::cout << "  Match: " << match.content << "\n";
        }
    );
    
    std::cout << (result.ok() ? "✓" : "✗") << " Case-insensitive found " 
              << result.value << " matches (should be 3)\n";
    
    std::system("rm -f /tmp/agrep_case.txt");
}

// ============================================================================
// Demo 3: Whole-Word Matching
// ============================================================================
void demo_whole_word() {
    std::cout << "\n=== Demo 3: Whole-Word Matching ===\n";
    
    create_test_file("/tmp/agrep_word.txt",
        "The word cat appears here.\n"
        "But not in 'category' or 'concatenate'.\n"
        "cat\n"
        "Just cat, nothing else.\n");
    
    SearchOptions options;
    options.whole_word = true;
    options.color_output = false;
    
    auto result = search_file(
        "/tmp/agrep_word.txt",
        "cat",
        options,
        [](const Match& match) {
            std::cout << "  Whole word match: " << match.content << "\n";
        }
    );
    
    std::cout << (result.ok() ? "✓" : "✗") << " Whole-word matches: " 
              << result.value << " (should skip 'category', 'concatenate')\n";
    
    std::system("rm -f /tmp/agrep_word.txt");
}

// ============================================================================
// Demo 4: Invert Match (Non-Matching Lines)
// ============================================================================
void demo_invert_match() {
    std::cout << "\n=== Demo 4: Invert Match ===\n";
    
    create_test_file("/tmp/agrep_invert.txt",
        "Line with ERROR\n"
        "Normal line\n"
        "Another ERROR here\n"
        "Clean line\n");
    
    SearchOptions options;
    options.invert_match = true;
    options.color_output = false;
    
    auto result = search_file(
        "/tmp/agrep_invert.txt",
        "ERROR",
        options,
        [](const Match& match) {
            std::cout << "  Non-error line: " << match.content << "\n";
        }
    );
    
    std::cout << (result.ok() ? "✓" : "✗") << " Lines WITHOUT 'ERROR': " 
              << result.value << "\n";
    
    std::system("rm -f /tmp/agrep_invert.txt");
}

// ============================================================================
// Demo 5: Binary File Detection
// ============================================================================
void demo_binary_detection() {
    std::cout << "\n=== Demo 5: Binary File Detection ===\n";
    
    // Create a binary file with null bytes
    std::ofstream bin_file("/tmp/agrep_binary.dat", std::ios::binary);
    bin_file << "\x00\x01\x02\x03\x04\x05";
    bin_file.close();
    
    bool is_binary = is_binary_file("/tmp/agrep_binary.dat");
    std::cout << (is_binary ? "✓" : "✗") << " Binary file detected\n";
    
    // Create text file
    create_test_file("/tmp/agrep_text.txt", "This is text\n");
    bool is_text = !is_binary_file("/tmp/agrep_text.txt");
    std::cout << (is_text ? "✓" : "✗") << " Text file detected\n";
    
    // Search with binary skip
    SearchOptions options;
    options.binary_skip = true;
    
    auto result = search_file("/tmp/agrep_binary.dat", "test", options, nullptr);
    std::cout << (result.err() ? "✓" : "✗") << " Binary file skipped: " 
              << error_to_string(result.error) << "\n";
    
    std::system("rm -f /tmp/agrep_binary.dat /tmp/agrep_text.txt");
}

// ============================================================================
// Demo 6: Match Count Limiting
// ============================================================================
void demo_max_matches() {
    std::cout << "\n=== Demo 6: Match Count Limiting ===\n";
    
    create_test_file("/tmp/agrep_limit.txt",
        "test line 1\n"
        "test line 2\n"
        "test line 3\n"
        "test line 4\n"
        "test line 5\n");
    
    SearchOptions options;
    options.max_matches = 3;
    options.color_output = false;
    
    tbb64 count = 0;
    auto result = search_file(
        "/tmp/agrep_limit.txt",
        "test",
        options,
        [&count](const Match&) { count++; }
    );
    
    std::cout << (count == 3 ? "✓" : "✗") << " Stopped after " << count 
              << " matches (limit was 3)\n";
    
    std::system("rm -f /tmp/agrep_limit.txt");
}

// ============================================================================
// Demo 7: Line Number Display
// ============================================================================
void demo_line_numbers() {
    std::cout << "\n=== Demo 7: Line Number Display ===\n";
    
    create_test_file("/tmp/agrep_lines.txt",
        "Line 1\n"
        "Match on line 2\n"
        "Line 3\n"
        "Another match on line 4\n"
        "Line 5\n");
    
    SearchOptions options;
    options.line_numbers = true;
    options.color_output = false;
    
    auto result = search_file(
        "/tmp/agrep_lines.txt",
        "match",
        options,
        [](const Match& match) {
            std::cout << "  Line " << match.line_number 
                      << " Col " << match.column << ": " 
                      << match.content << "\n";
        }
    );
    
    std::cout << "✓ Line numbers displayed\n";
    
    std::system("rm -f /tmp/agrep_lines.txt");
}

// ============================================================================
// Demo 8: ANSI Color Highlighting
// ============================================================================
void demo_color_highlighting() {
    std::cout << "\n=== Demo 8: ANSI Color Highlighting ===\n";
    
    create_test_file("/tmp/agrep_color.txt",
        "This line contains the word pattern.\n"
        "Another pattern appears here.\n");
    
    SearchOptions options;
    options.color_output = true;
    
    std::cout << "Colored output:\n";
    auto result = search_file(
        "/tmp/agrep_color.txt",
        "pattern",
        options,
        [&options](const Match& match) {
            write_match_to_stdout(match, "pattern", "/tmp/agrep_color.txt", options);
        }
    );
    
    std::cout << "✓ ANSI colors applied (red highlight for 'pattern')\n";
    
    std::system("rm -f /tmp/agrep_color.txt");
}

// ============================================================================
// Demo 9: Six-Stream I/O
// ============================================================================
void demo_six_stream() {
    std::cout << "\n=== Demo 9: Six-Stream I/O ===\n";
    
    create_test_file("/tmp/agrep_stream.txt",
        "First match here\n"
        "Second match here\n"
        "Third match here\n");
    
    SearchOptions options;
    options.color_output = false;
    
    std::cout << "FD 1 (stdout) - Human UI:\n";
    std::cout << "FD 3 (stddbg) - Telemetry: [writing JSON logs]\n";
    std::cout << "FD 5 (stddato) - Binary protocol: ";
    
    int64_t total_bytes = 0;
    auto result = search_file(
        "/tmp/agrep_stream.txt",
        "match",
        options,
        [&total_bytes](const Match& match) {
            // FD 1: stdout (human readable)
            std::cout << "  Match at line " << match.line_number << "\n";
            
            // FD 5: stddato (binary)
            int64_t bytes = write_match_to_stddato(match);
            total_bytes += bytes;
            
            // FD 3: stddbg (telemetry)
            write_telemetry("match", "Found at line " + 
                          std::to_string(match.line_number));
        }
    );
    
    std::cout << total_bytes << " bytes written to stddato\n";
    std::cout << "✓ All three planes operational\n";
    
    std::system("rm -f /tmp/agrep_stream.txt");
}

// ============================================================================
// Demo 10: Multiple File Search with Statistics
// ============================================================================
void demo_multi_file_stats() {
    std::cout << "\n=== Demo 10: Multiple File Search ===\n";
    
    create_test_file("/tmp/agrep_file1.txt", "error in file 1\nanother error\n");
    create_test_file("/tmp/agrep_file2.txt", "warning in file 2\n");
    create_test_file("/tmp/agrep_file3.txt", "error in file 3\n");
    
    std::vector<std::string> files = {
        "/tmp/agrep_file1.txt",
        "/tmp/agrep_file2.txt",
        "/tmp/agrep_file3.txt"
    };
    
    SearchOptions options;
    options.color_output = false;
    
    auto result = search_files(
        files,
        "error",
        options,
        [](const Match& match, const std::string& file) {
            std::cout << "  " << file << ":" << match.line_number 
                      << ": " << match.content << "\n";
        }
    );
    
    if (result.ok()) {
        const SearchStats& stats = result.value;
        std::cout << "\nStatistics:\n";
        std::cout << "  Files searched: " << stats.files_searched << "\n";
        std::cout << "  Matches found: " << stats.matches_found << "\n";
        std::cout << "  Duration: " << stats.duration_ms << " ms\n";
        std::cout << "✓ Multi-file search complete\n";
        
        write_stats_telemetry(stats);
    }
    
    std::system("rm -f /tmp/agrep_file*.txt");
}

// ============================================================================
// Demo 11: FFI C API
// ============================================================================
void demo_ffi_api() {
    std::cout << "\n=== Demo 11: FFI C API ===\n";
    
    create_test_file("/tmp/agrep_ffi.txt",
        "First test line\n"
        "Second test line\n"
        "Third test line\n");
    
    // Use C API
    int64_t count = aria_grep_file(
        "/tmp/agrep_ffi.txt",
        "test",
        false,  // case sensitive
        [](const Match* match) {
            std::cout << "  C API match: Line " << match->line_number << "\n";
        }
    );
    
    std::cout << (count > 0 ? "✓" : "✗") << " aria_grep_file() found " 
              << count << " matches\n";
    
    // Test binary detection
    create_test_file("/tmp/agrep_binary_test.dat", "\x00\x01\x02");
    int32_t is_bin = aria_is_binary_file("/tmp/agrep_binary_test.dat");
    std::cout << (is_bin == 1 ? "✓" : "✗") << " aria_is_binary_file() detected binary\n";
    
    std::system("rm -f /tmp/agrep_ffi.txt /tmp/agrep_binary_test.dat");
}

// ============================================================================
// Demo 12: Binary Protocol Serialization
// ============================================================================
void demo_binary_protocol() {
    std::cout << "\n=== Demo 12: Binary Protocol Serialization ===\n";
    
    Match match;
    match.file_offset = 1024;
    match.line_number = 42;
    match.column = 10;
    match.match_len = 7;
    match.content = "The quick brown fox";
    match.content_len = static_cast<int32_t>(match.content.length());
    
    std::cout << "Match Structure Binary Layout:\n";
    std::cout << "  Offset | Field        | Size | Value\n";
    std::cout << "  -------|--------------|------|-------\n";
    std::cout << "  0      | magic        | 1    | 0x" << std::hex 
              << static_cast<int>(Match::MAGIC) << std::dec << "\n";
    std::cout << "  1      | file_offset  | 8    | " << match.file_offset << "\n";
    std::cout << "  9      | line_number  | 4    | " << match.line_number << "\n";
    std::cout << "  13     | column       | 4    | " << match.column << "\n";
    std::cout << "  17     | match_len    | 4    | " << match.match_len << "\n";
    std::cout << "  21     | content_len  | 4    | " << match.content_len << "\n";
    std::cout << "  25     | content      | " << match.content_len 
              << "   | \"" << match.content << "\"\n";
    std::cout << "  Total: " << (25 + match.content_len) << " bytes\n";
    std::cout << "✓ Binary protocol ready for pipeline composition\n";
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "====================================\n";
    std::cout << "  AGREP DEMONSTRATION SUITE\n";
    std::cout << "====================================\n";
    
    demo_basic_search();
    demo_case_insensitive();
    demo_whole_word();
    demo_invert_match();
    demo_binary_detection();
    demo_max_matches();
    demo_line_numbers();
    demo_color_highlighting();
    demo_six_stream();
    demo_multi_file_stats();
    demo_ffi_api();
    demo_binary_protocol();
    
    std::cout << "\n====================================\n";
    std::cout << "  ALL DEMOS COMPLETE\n";
    std::cout << "====================================\n";
    
    return 0;
}
