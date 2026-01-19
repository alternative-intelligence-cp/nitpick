/**
 * acp_demo.cpp
 * Interactive demonstration of the acp (Aria Copy) utility
 *
 * Shows file copying with various options and safety features
 */

#include "acp/file_copy.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace aria::acp;

// Helper: Print section header
void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Helper: Create test directory structure
void setup_test_files() {
    fs::create_directories("test_source");
    
    // Create test files
    std::ofstream("test_source/file1.txt") << "Hello, World!\n";
    std::ofstream("test_source/file2.txt") << "Second test file\n";
    std::ofstream("test_source/large.dat") << std::string(1024 * 100, 'X'); // 100 KB
    
    // Create subdirectory
    fs::create_directories("test_source/subdir");
    std::ofstream("test_source/subdir/nested.txt") << "Nested file\n";
}

// Helper: Cleanup test files
void cleanup_test_files() {
    fs::remove_all("test_source");
    fs::remove_all("test_dest");
    fs::remove_all("test_dest2");
    fs::remove("copied_file.txt");
}

// Demo 1: Simple file copy
void demo_simple_copy() {
    print_section("Demo 1: Simple File Copy");

    setup_test_files();

    CopyOptions opts;
    auto result = copy_file("test_source/file1.txt", "copied_file.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File copied successfully\n";
        std::cout << "  Bytes copied: " << format_bytes(result.bytes_copied) << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(2) 
                  << result.elapsed_ms << " ms\n";
        std::cout << "  Throughput: " << result.throughput_mbps << " MB/s\n";
    } else {
        std::cout << "✗ Copy failed: " << error_to_string(result.error) << "\n";
    }

    cleanup_test_files();
}

// Demo 2: Copy with overwrite protection
void demo_no_clobber() {
    print_section("Demo 2: No-Clobber (Don't Overwrite)");

    setup_test_files();

    // First copy
    CopyOptions opts;
    auto result1 = copy_file("test_source/file1.txt", "copied_file.txt", opts);
    std::cout << "First copy: " << (result1.is_ok() ? "✓ Success" : "✗ Failed") << "\n";

    // Try to copy again with no-clobber
    opts.no_clobber = true;
    auto result2 = copy_file("test_source/file2.txt", "copied_file.txt", opts);
    
    if (result2.error == ErrorCode::DEST_EXISTS) {
        std::cout << "✓ Correctly refused to overwrite existing file\n";
        std::cout << "  Files skipped: " << result2.files_skipped << "\n";
    } else {
        std::cout << "✗ Should have skipped existing file\n";
    }

    cleanup_test_files();
}

// Demo 3: Force overwrite
void demo_force_overwrite() {
    print_section("Demo 3: Force Overwrite");

    setup_test_files();

    // Create destination file
    std::ofstream("copied_file.txt") << "Old content\n";

    CopyOptions opts;
    opts.force = true;
    opts.verbose = true;

    auto result = copy_file("test_source/file1.txt", "copied_file.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File overwritten successfully\n";
        std::cout << "  Bytes copied: " << format_bytes(result.bytes_copied) << "\n";
        
        // Verify content changed
        std::ifstream in("copied_file.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
        std::cout << "  New content: " << content;
    } else {
        std::cout << "✗ Force copy failed\n";
    }

    cleanup_test_files();
}

// Demo 4: Recursive directory copy
void demo_recursive_copy() {
    print_section("Demo 4: Recursive Directory Copy");

    setup_test_files();

    CopyOptions opts;
    opts.recursive = true;
    opts.verbose = true;

    auto result = copy_directory("test_source", "test_dest", opts);

    if (result.is_ok()) {
        std::cout << "✓ Directory copied recursively\n";
        std::cout << "  Files copied: " << result.files_copied << "\n";
        std::cout << "  Directories created: " << result.dirs_created << "\n";
        std::cout << "  Total bytes: " << format_bytes(result.bytes_copied) << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(2) 
                  << result.elapsed_ms << " ms\n";
        
        // Verify structure
        std::cout << "\nDestination structure:\n";
        for (const auto& entry : fs::recursive_directory_iterator("test_dest")) {
            std::cout << "  " << entry.path().string() << "\n";
        }
    } else {
        std::cout << "✗ Recursive copy failed: " << error_to_string(result.error) << "\n";
    }

    cleanup_test_files();
}

// Demo 5: Copy multiple files
void demo_copy_multiple() {
    print_section("Demo 5: Copy Multiple Files");

    setup_test_files();
    fs::create_directories("test_dest");

    std::vector<std::string> sources = {
        "test_source/file1.txt",
        "test_source/file2.txt",
        "test_source/large.dat"
    };

    CopyOptions opts;
    opts.verbose = true;

    auto result = copy_files(sources, "test_dest", opts);

    if (result.is_ok()) {
        std::cout << "✓ Multiple files copied\n";
        std::cout << "  Files copied: " << result.files_copied << "\n";
        std::cout << "  Total bytes: " << format_bytes(result.bytes_copied) << "\n";
        
        std::cout << "\nDestination files:\n";
        for (const auto& entry : fs::directory_iterator("test_dest")) {
            auto size = fs::file_size(entry.path());
            std::cout << "  " << entry.path().filename().string() 
                      << " (" << format_bytes(size) << ")\n";
        }
    } else {
        std::cout << "✗ Multiple copy failed\n";
    }

    cleanup_test_files();
}

// Demo 6: Progress callback
void demo_progress_callback() {
    print_section("Demo 6: Progress Tracking");

    setup_test_files();

    CopyOptions opts;
    
    // Define progress callback
    auto progress_cb = [](const CopyProgress& prog) {
        std::cout << "\r  Copying: " << prog.current_file
                  << " (" << prog.percent << "%)    " << std::flush;
        return true; // Continue copying
    };

    auto result = copy_file("test_source/large.dat", "copied_large.dat", 
                           opts, progress_cb);

    std::cout << "\n";
    
    if (result.is_ok()) {
        std::cout << "✓ Copy completed with progress tracking\n";
        std::cout << "  Bytes copied: " << format_bytes(result.bytes_copied) << "\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
                  << result.throughput_mbps << " MB/s\n";
    }

    fs::remove("copied_large.dat");
    cleanup_test_files();
}

// Demo 7: TBB error propagation
void demo_error_handling() {
    print_section("Demo 7: Error Handling and TBB Sentinels");

    CopyOptions opts;
    auto result = copy_file("/nonexistent/file.txt", "dest.txt", opts);

    if (!result.is_ok()) {
        std::cout << "✓ Error correctly detected\n";
        std::cout << "  Error code: " << static_cast<int>(result.error) << "\n";
        std::cout << "  Error message: " << result.error_message << "\n";
        std::cout << "  Error path: " << result.error_path << "\n";
        std::cout << "  Description: " << error_to_string(result.error) << "\n";
    }

    std::cout << "\nDemonstrating TBB sentinel behavior:\n";
    std::cout << "  TBB64_ERR constant: " << TBB64_ERR << "\n";
    std::cout << "  is_tbb_err(TBB64_ERR): " << (is_tbb_err(TBB64_ERR) ? "true" : "false") << "\n";
    std::cout << "  is_tbb_err(12345): " << (is_tbb_err(12345) ? "true" : "false") << "\n";
    
    // Test TBB addition
    int64_t err_sum = tbb_add(TBB64_ERR, 100);
    std::cout << "  tbb_add(ERR, 100) = " << (is_tbb_err(err_sum) ? "ERR (sticky!)" : std::to_string(err_sum)) << "\n";
}

// Demo 8: Attribute preservation
void demo_preserve_attributes() {
    print_section("Demo 8: Attribute Preservation");

    setup_test_files();

    // Set specific permissions on source
    fs::permissions("test_source/file1.txt", 
                   fs::perms::owner_read | fs::perms::owner_write,
                   fs::perm_options::replace);

    CopyOptions opts;
    opts.preserve_mode = true;
    opts.preserve_timestamps = true;

    auto result = copy_file("test_source/file1.txt", "copied_file.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File copied with attributes preserved\n";
        
        auto src_perms = fs::status("test_source/file1.txt").permissions();
        auto dst_perms = fs::status("copied_file.txt").permissions();
        
        std::cout << "  Source permissions: " << std::oct 
                  << static_cast<int>(src_perms) << std::dec << "\n";
        std::cout << "  Dest permissions: " << std::oct 
                  << static_cast<int>(dst_perms) << std::dec << "\n";
        std::cout << "  Match: " << (src_perms == dst_perms ? "✓" : "✗") << "\n";
    }

    cleanup_test_files();
}

// Main program
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     Aria Copy (acp) - Interactive Demonstration           ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  High-performance file copying with safety guarantees     ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_simple_copy();
        demo_no_clobber();
        demo_force_overwrite();
        demo_recursive_copy();
        demo_copy_multiple();
        demo_progress_callback();
        demo_error_handling();
        demo_preserve_attributes();

        std::cout << "\n✓ All 8 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
