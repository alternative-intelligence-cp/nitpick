/**
 * amv_demo.cpp
 * Interactive demonstration of the amv (Aria Move) utility
 *
 * Shows file moving/renaming with atomic operations and safety
 */

#include "amv/file_move.hpp"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;
using namespace aria::amv;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

void setup_test_files() {
    fs::create_directories("test_move");
    std::ofstream("test_move/file1.txt") << "Test file 1\n";
    std::ofstream("test_move/file2.txt") << "Test file 2\n";
    std::ofstream("test_move/file3.txt") << "Test file 3\n";
}

void cleanup_test_files() {
    fs::remove_all("test_move");
    fs::remove_all("test_dest");
    fs::remove("renamed.txt");
    fs::remove("moved.txt");
}

// Demo 1: Simple rename
void demo_simple_rename() {
    print_section("Demo 1: Simple File Rename");

    setup_test_files();

    MoveOptions opts;
    auto result = move_file("test_move/file1.txt", "test_move/renamed.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File renamed successfully\n";
        std::cout << "  Files moved: " << result.files_moved << "\n";
        std::cout << "  Used atomic rename: " << (result.used_rename ? "Yes" : "No") << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(2) 
                  << result.elapsed_ms << " ms\n";
        
        std::cout << "\nVerifying:\n";
        std::cout << "  Original exists: " << (fs::exists("test_move/file1.txt") ? "Yes" : "No") << "\n";
        std::cout << "  Renamed exists: " << (fs::exists("test_move/renamed.txt") ? "Yes" : "No") << "\n";
    } else {
        std::cout << "✗ Rename failed: " << error_to_string(result.error) << "\n";
    }

    cleanup_test_files();
}

// Demo 2: Move to different directory
void demo_move_directory() {
    print_section("Demo 2: Move to Different Directory");

    setup_test_files();
    fs::create_directories("test_dest");

    MoveOptions opts;
    opts.verbose = true;

    auto result = move_file("test_move/file2.txt", "test_dest/file2.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File moved to different directory\n";
        std::cout << "  Files moved: " << result.files_moved << "\n";
        std::cout << "  Used atomic rename: " << (result.used_rename ? "Yes" : "No") << "\n";
        
        std::cout << "\nVerifying:\n";
        std::cout << "  Source exists: " << (fs::exists("test_move/file2.txt") ? "Yes" : "No") << "\n";
        std::cout << "  Dest exists: " << (fs::exists("test_dest/file2.txt") ? "Yes" : "No") << "\n";
    }

    cleanup_test_files();
}

// Demo 3: No-clobber protection
void demo_no_clobber() {
    print_section("Demo 3: No-Clobber (Prevent Overwrite)");

    setup_test_files();
    
    // Create existing destination
    std::ofstream("test_move/existing.txt") << "Existing content\n";

    MoveOptions opts;
    opts.no_clobber = true;

    auto result = move_file("test_move/file1.txt", "test_move/existing.txt", opts);

    if (result.error == ErrorCode::DEST_EXISTS) {
        std::cout << "✓ Correctly refused to overwrite existing file\n";
        std::cout << "  Files skipped: " << result.files_skipped << "\n";
        std::cout << "  Error: " << result.error_message << "\n";
    } else {
        std::cout << "✗ Should have protected existing file\n";
    }

    cleanup_test_files();
}

// Demo 4: Force overwrite
void demo_force_overwrite() {
    print_section("Demo 4: Force Overwrite");

    setup_test_files();
    std::ofstream("test_move/target.txt") << "Old content\n";

    MoveOptions opts;
    opts.force = true;

    auto result = move_file("test_move/file1.txt", "test_move/target.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File overwritten successfully\n";
        
        // Verify content
        std::ifstream in("test_move/target.txt");
        std::string content((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
        std::cout << "  New content: " << content;
        std::cout << "  Source removed: " << (!fs::exists("test_move/file1.txt") ? "Yes" : "No") << "\n";
    }

    cleanup_test_files();
}

// Demo 5: Batch move
void demo_batch_move() {
    print_section("Demo 5: Batch Move Multiple Files");

    setup_test_files();
    fs::create_directories("test_dest");

    std::vector<std::string> sources = {
        "test_move/file1.txt",
        "test_move/file2.txt",
        "test_move/file3.txt"
    };

    MoveOptions opts;
    auto result = move_files(sources, "test_dest", opts);

    if (result.is_ok()) {
        std::cout << "✓ Batch move completed\n";
        std::cout << "  Files moved: " << result.files_moved << "\n";
        std::cout << "  Time: " << std::fixed << std::setprecision(2) 
                  << result.elapsed_ms << " ms\n";
        
        std::cout << "\nDestination files:\n";
        for (const auto& entry : fs::directory_iterator("test_dest")) {
            std::cout << "  " << entry.path().filename().string() << "\n";
        }
    }

    cleanup_test_files();
}

// Demo 6: Backup before overwrite
void demo_backup() {
    print_section("Demo 6: Create Backup Before Overwrite");

    setup_test_files();
    std::ofstream("test_move/important.txt") << "Important data\n";

    MoveOptions opts;
    opts.backup = true;
    opts.backup_suffix = ".bak";

    auto result = move_file("test_move/file1.txt", "test_move/important.txt", opts);

    if (result.is_ok()) {
        std::cout << "✓ File moved with backup created\n";
        
        std::cout << "\nVerifying:\n";
        std::cout << "  Backup exists: " << (fs::exists("test_move/important.txt.bak") ? "Yes" : "No") << "\n";
        std::cout << "  New file exists: " << (fs::exists("test_move/important.txt") ? "Yes" : "No") << "\n";
        
        if (fs::exists("test_move/important.txt.bak")) {
            std::ifstream in("test_move/important.txt.bak");
            std::string content((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
            std::cout << "  Backup content: " << content;
        }
    }

    cleanup_test_files();
}

// Demo 7: Error handling
void demo_error_handling() {
    print_section("Demo 7: Error Handling and TBB Sentinels");

    MoveOptions opts;
    auto result = move_file("/nonexistent/file.txt", "dest.txt", opts);

    if (!result.is_ok()) {
        std::cout << "✓ Error correctly detected\n";
        std::cout << "  Error code: " << static_cast<int>(result.error) << "\n";
        std::cout << "  Error message: " << result.error_message << "\n";
        std::cout << "  Error path: " << result.error_path << "\n";
        std::cout << "  Description: " << error_to_string(result.error) << "\n";
    }

    std::cout << "\nTBB sentinel demonstration:\n";
    std::cout << "  TBB64_ERR: " << TBB64_ERR << "\n";
    std::cout << "  is_tbb_err(TBB64_ERR): " << (is_tbb_err(TBB64_ERR) ? "true" : "false") << "\n";
    
    int64_t err_result = tbb_add(TBB64_ERR, 100);
    std::cout << "  tbb_add(ERR, 100) propagates: " 
              << (is_tbb_err(err_result) ? "ERR (sticky!)" : std::to_string(err_result)) << "\n";
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║     Aria Move (amv) - Interactive Demonstration           ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  Atomic file moves with safety guarantees                 ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_simple_rename();
        demo_move_directory();
        demo_no_clobber();
        demo_force_overwrite();
        demo_batch_move();
        demo_backup();
        demo_error_handling();

        std::cout << "\n✓ All 7 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
