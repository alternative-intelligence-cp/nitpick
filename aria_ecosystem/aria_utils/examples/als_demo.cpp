/**
 * als_demo.cpp
 * Interactive demonstration of the als (Aria List) utility
 *
 * Shows directory listing with filtering, sorting, and six-stream output
 */

#include "als/file_list.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>

using namespace aria::als;

// Helper: Print section header
void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Helper: Format file type
std::string format_type(FileType type) {
    switch (type) {
        case FileType::REGULAR: return "FILE";
        case FileType::DIRECTORY: return "DIR ";
        case FileType::SYMLINK: return "LINK";
        case FileType::CHAR_DEVICE: return "CHAR";
        case FileType::BLOCK_DEVICE: return "BLCK";
        case FileType::FIFO: return "FIFO";
        case FileType::SOCKET: return "SOCK";
        default: return "????";
    }
}

// Demo 1: Basic directory listing
void demo_basic_list() {
    print_section("Demo 1: Basic Directory Listing");

    TraversalOptions opts;
    opts.recursive = false;
    opts.max_depth = 1;

    auto result = list_directory("../src", opts);

    if (result.error != ErrorCode::OK) {
        std::cout << "Error: Failed to list directory\n";
        return;
    }

    std::cout << "Found " << result.entries.size() << " entries in ../src/\n\n";
    std::cout << std::left 
              << std::setw(25) << "Name"
              << std::setw(8) << "Type"
              << std::setw(12) << "Size"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    for (const auto& entry : result.entries) {
        std::cout << std::setw(25) << entry.name
                  << std::setw(8) << format_type(entry.file_type)
                  << std::setw(12) << format_size(entry.size)
                  << "\n";
    }
}

// Demo 2: Recursive listing with depth limit
void demo_recursive() {
    print_section("Demo 2: Recursive Listing (max depth 2)");

    TraversalOptions opts;
    opts.recursive = true;
    opts.max_depth = 2;

    auto result = list_directory("../src/aglob", opts);

    std::cout << "Recursive scan of ../src/aglob/\n";
    std::cout << "Total entries: " << result.entries.size() << "\n\n";

    for (const auto& entry : result.entries) {
        // Simple listing (depth not in FileEntry struct)
        std::cout << "  " << entry.name 
                  << " (" << format_type(entry.file_type) << ")\n";
    }
}

// Demo 3: Filter by file type
void demo_filter_type() {
    print_section("Demo 3: Filter by Type (Regular Files Only)");

    TraversalOptions opts;
    opts.recursive = true;
    opts.filter.type = FileType::REGULAR;
    opts.filter.include_hidden = false;

    auto result = list_directory("../src/aglob/src", opts);

    std::cout << "Found " << result.entries.size() << " regular files\n\n";

    for (const auto& entry : result.entries) {
        std::cout << "  " << entry.path << "  (" << format_size(entry.size) << ")\n";
    }
}

// Demo 4: Filter by glob pattern
void demo_filter_glob() {
    print_section("Demo 4: Filter by Glob Pattern (*.cpp)");

    TraversalOptions opts;
    opts.recursive = true;
    opts.filter.name_glob = "*.cpp";

    auto result = list_directory("../src", opts);

    std::cout << "Found " << result.entries.size() << " .cpp files\n\n";

    for (const auto& entry : result.entries) {
        std::cout << "  " << entry.path << "\n";
    }
}

// Demo 5: Sort by size
void demo_sort_by_size() {
    print_section("Demo 5: Sort by Size (Largest First)");

    TraversalOptions opts;
    opts.recursive = true;
    opts.sort.field = SortField::SIZE;
    opts.sort.descending = true;
    opts.filter.type = FileType::REGULAR;

    auto result = list_directory("../src/aglob", opts);

    std::cout << "Top 10 largest files:\n\n";
    std::cout << std::left
              << std::setw(10) << "Size"
              << std::setw(40) << "Path"
              << "\n";
    std::cout << std::string(55, '-') << "\n";

    size_t count = std::min(result.entries.size(), size_t(10));
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = result.entries[i];
        std::cout << std::setw(10) << format_size(entry.size)
                  << std::setw(40) << entry.path
                  << "\n";
    }
}

// Demo 6: Sort by modification time
void demo_sort_by_mtime() {
    print_section("Demo 6: Sort by Modification Time (Newest First)");

    TraversalOptions opts;
    opts.recursive = true;
    opts.max_depth = 2;
    opts.sort.field = SortField::MTIME;
    opts.sort.descending = true;
    opts.filter.type = FileType::REGULAR;

    auto result = list_directory("../src", opts);

    std::cout << "5 most recently modified files:\n\n";
    std::cout << std::left
              << std::setw(17) << "Modified"
              << std::setw(40) << "Path"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    size_t count = std::min(result.entries.size(), size_t(5));
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = result.entries[i];
        std::cout << std::setw(17) << format_time(entry.mtime)
                  << std::setw(40) << entry.path
                  << "\n";
    }
}

// Demo 7: TBB Error Propagation
void demo_error_handling() {
    print_section("Demo 7: Error Handling and TBB Sentinels");

    TraversalOptions opts;
    opts.recursive = false;

    auto result = list_directory("/nonexistent/path", opts);

    if (result.error != ErrorCode::OK) {
        std::cout << "✓ Error correctly detected\n";
        std::cout << "  Error code: " << static_cast<int>(result.error) << "\n";
        std::cout << "  Error message: " << result.error_message << "\n";
        std::cout << "  Description: ";
        
        switch (result.error) {
            case ErrorCode::NOT_FOUND:
                std::cout << "Directory not found\n";
                break;
            case ErrorCode::PERMISSION_DENIED:
                std::cout << "Permission denied\n";
                break;
            case ErrorCode::NOT_A_DIRECTORY:
                std::cout << "Not a directory\n";
                break;
            default:
                std::cout << "Unknown error\n";
        }
    }

    std::cout << "\nDemonstrating TBB sentinel behavior:\n";
    std::cout << "  TBB64_ERR constant: " << TBB64_ERR << "\n";
    std::cout << "  is_tbb_err(TBB64_ERR): " << (is_tbb_err(TBB64_ERR) ? "true" : "false") << "\n";
    std::cout << "  is_tbb_err(12345): " << (is_tbb_err(12345) ? "true" : "false") << "\n";
}

// Demo 8: Performance statistics
void demo_statistics() {
    print_section("Demo 8: Directory Statistics");

    TraversalOptions opts;
    opts.recursive = true;

    auto result = list_directory("../src", opts);

    std::cout << "Statistics for ../src/:\n\n";
    std::cout << "  Total entries:    " << result.entries.size() << "\n";
    std::cout << "  Regular files:    " << result.file_count << "\n";
    std::cout << "  Directories:      " << result.dir_count << "\n";
    std::cout << "  Symbolic links:   " << result.symlink_count << "\n";
    std::cout << "  Total size:       " << format_size(result.total_size) << "\n";
    std::cout << "  Errors:           " << result.error_count << "\n";
}

// Main program
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        Aria List (als) - Interactive Demonstration        ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  High-performance directory enumeration with six-stream   ║\n";
    std::cout << "║  architecture and TBB error propagation                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_basic_list();
        demo_recursive();
        demo_filter_type();
        demo_filter_glob();
        demo_sort_by_size();
        demo_sort_by_mtime();
        demo_error_handling();
        demo_statistics();

        std::cout << "\n✓ All 8 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
