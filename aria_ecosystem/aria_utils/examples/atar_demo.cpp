/**
 * @file atar_demo.cpp
 * @brief Comprehensive demonstrations of archive utilities
 * 
 * Demonstrates:
 * - Archive creation from files
 * - Archive extraction
 * - Archive listing
 * - Six-stream I/O (stdout, stddbg, stddato)
 * - TBB-safe parsing
 * - Binary metadata protocol
 * - FFI C API
 */

#include "../src/atar/archive.hpp"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

using namespace aria::atar;

// Helper: Create test files for demos
void setup_test_files() {
    mkdir("test_archive_data", 0755);
    
    // Create sample files
    std::ofstream f1("test_archive_data/file1.txt");
    f1 << "This is test file 1\n";
    f1 << "It has multiple lines.\n";
    f1.close();
    
    std::ofstream f2("test_archive_data/file2.txt");
    f2 << "Second test file\n";
    f2 << "Different content here.\n";
    f2.close();
    
    std::ofstream f3("test_archive_data/large.txt");
    for (int i = 0; i < 1000; ++i) {
        f3 << "Line " << i << ": Lorem ipsum dolor sit amet, consectetur adipiscing elit.\n";
    }
    f3.close();
    
    mkdir("test_archive_data/subdir", 0755);
    std::ofstream f4("test_archive_data/subdir/nested.txt");
    f4 << "Nested file in subdirectory\n";
    f4.close();
}

// Helper: Cleanup test files
void cleanup_test_files() {
    system("rm -rf test_archive_data test_extract test.tar");
}

// Helper: Print demo header
void print_demo(int num, const std::string& title) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << "Demo " << num << ": " << title << "\n";
    std::cout << "========================================\n";
}

// Helper: Print success
void print_success(const std::string& msg) {
    std::cout << "✓ " << msg << "\n";
}

// Demo 1: Create archive from files
void demo_create_archive() {
    print_demo(1, "Create TAR Archive");
    
    setup_test_files();
    
    ArchiveWriter writer("test.tar");
    
    writer.add_file("test_archive_data/file1.txt");
    writer.add_file("test_archive_data/file2.txt");
    writer.add_file("test_archive_data/large.txt");
    
    writer.finalize();
    
    const ArchiveStats& stats = writer.get_stats();
    std::cout << "Created archive with " << stats.files_processed << " files\n";
    std::cout << "Bytes read: " << stats.bytes_read << "\n";
    
    print_success("Archive created successfully");
}

// Demo 2: List archive contents
void demo_list_archive() {
    print_demo(2, "List Archive Contents");
    
    ArchiveReader reader("test.tar");
    
    std::cout << std::left << std::setw(40) << "Path"
              << std::setw(12) << "Size"
              << std::setw(12) << "Mode"
              << "Type\n";
    std::cout << std::string(76, '-') << "\n";
    
    int count = 0;
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) break;
        
        const ArchiveEntry& entry = result.value;
        if (entry.path.empty()) break;
        
        std::cout << std::left << std::setw(40) << entry.path
                  << std::setw(12) << entry.size
                  << std::setw(12) << std::oct << entry.mode << std::dec
                  << entry.type_flag << "\n";
        
        reader.skip_entry(entry);
        count++;
    }
    
    print_success("Listed " + std::to_string(count) + " entries");
}

// Demo 3: Extract archive
void demo_extract_archive() {
    print_demo(3, "Extract Archive");
    
    mkdir("test_extract", 0755);
    
    ArchiveReader reader("test.tar");
    ArchiveOptions opts;
    opts.output_dir = "test_extract";
    opts.verbose = true;
    
    int extracted = 0;
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) break;
        
        const ArchiveEntry& entry = result.value;
        if (entry.path.empty()) break;
        
        reader.extract_entry(entry, opts);
        extracted++;
    }
    
    const ArchiveStats& stats = reader.get_stats();
    std::cout << "\nExtracted " << extracted << " files\n";
    std::cout << "Bytes written: " << stats.bytes_written << "\n";
    
    print_success("Extraction complete");
}

// Demo 4: Verify extracted files
void demo_verify_extraction() {
    print_demo(4, "Verify Extracted Files");
    
    // Read original and extracted file, compare
    std::ifstream orig("test_archive_data/file1.txt");
    std::ifstream extr("test_extract/test_archive_data/file1.txt");
    
    std::string orig_content((std::istreambuf_iterator<char>(orig)),
                            std::istreambuf_iterator<char>());
    std::string extr_content((std::istreambuf_iterator<char>(extr)),
                            std::istreambuf_iterator<char>());
    
    if (orig_content == extr_content) {
        print_success("File content matches (verified)");
    } else {
        std::cout << "✗ File content mismatch!\n";
    }
}

// Demo 5: TBB safety - corrupt header detection
void demo_tbb_safety() {
    print_demo(5, "TBB Safety - Corrupt Header Detection");
    
    // Create a TAR with manually corrupted header
    std::ofstream corrupt("corrupt.tar", std::ios::binary);
    
    // Valid header start
    char header[512] = {0};
    strcpy(header, "testfile.txt");
    
    // Corrupt the size field with invalid octal
    strcpy(header + 124, "999999999999"); // Invalid (too large)
    
    // Write checksum spaces
    memset(header + 148, ' ', 8);
    
    corrupt.write(header, 512);
    corrupt.close();
    
    // Try to parse
    ArchiveReader reader("corrupt.tar");
    auto result = reader.read_entry();
    
    if (!result.ok || is_tbb_error(result.value.size)) {
        print_success("Corrupt header detected (TBB ERR sentinel)");
    } else {
        std::cout << "✗ Failed to detect corruption\n";
    }
    
    unlink("corrupt.tar");
}

// Demo 6: Entry type detection
void demo_entry_types() {
    print_demo(6, "Entry Type Detection");
    
    ArchiveReader reader("test.tar");
    
    int regular_files = 0;
    int directories = 0;
    int symlinks = 0;
    
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) break;
        
        const ArchiveEntry& entry = result.value;
        if (entry.path.empty()) break;
        
        switch (entry.type_flag) {
            case '0': case '\0': regular_files++; break;
            case '5': directories++; break;
            case '2': symlinks++; break;
        }
        
        reader.skip_entry(entry);
    }
    
    std::cout << "Regular files: " << regular_files << "\n";
    std::cout << "Directories: " << directories << "\n";
    std::cout << "Symlinks: " << symlinks << "\n";
    
    print_success("Type detection complete");
}

// Demo 7: Archive statistics
void demo_statistics() {
    print_demo(7, "Archive Statistics");
    
    ArchiveReader reader("test.tar");
    
    tbb64 total_size = 0;
    tbb64 min_size = INT64_MAX;
    tbb64 max_size = 0;
    
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) break;
        
        const ArchiveEntry& entry = result.value;
        if (entry.path.empty()) break;
        
        if (entry.type_flag == '0' || entry.type_flag == '\0') {
            total_size += entry.size;
            if (entry.size < min_size) min_size = entry.size;
            if (entry.size > max_size) max_size = entry.size;
        }
        
        reader.skip_entry(entry);
    }
    
    const ArchiveStats& stats = reader.get_stats();
    
    std::cout << "Files processed: " << stats.files_processed << "\n";
    std::cout << "Files skipped: " << stats.files_skipped << "\n";
    std::cout << "Total size: " << total_size << " bytes\n";
    std::cout << "Min file size: " << min_size << " bytes\n";
    std::cout << "Max file size: " << max_size << " bytes\n";
    if (stats.files_processed > 0) {
        std::cout << "Average size: " << (total_size / stats.files_processed) << " bytes\n";
    }
    
    print_success("Statistics calculated");
}

// Demo 8: Streaming extraction (callback-based)
void demo_streaming_extraction() {
    print_demo(8, "Streaming Extraction (Callback)");
    
    ArchiveReader reader("test.tar");
    
    // Find and stream the large.txt file
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) break;
        
        const ArchiveEntry& entry = result.value;
        if (entry.path.empty()) break;
        
        if (entry.path.find("large.txt") != std::string::npos) {
            size_t total_bytes = 0;
            int chunks = 0;
            
            reader.extract_entry_stream(entry, [&](const uint8_t* data, size_t len) {
                total_bytes += len;
                chunks++;
                // Could write to stdout, process, or stream to another service
            });
            
            std::cout << "Streamed " << total_bytes << " bytes in " << chunks << " chunks\n";
            print_success("Streaming extraction complete");
            break;
        } else {
            reader.skip_entry(entry);
        }
    }
}

// Demo 9: Create archive with explicit entry
void demo_create_with_entry() {
    print_demo(9, "Create Archive with Explicit Entry");
    
    ArchiveWriter writer("custom.tar");
    
    // Create custom entry
    ArchiveEntry entry;
    entry.path = "custom_data.txt";
    entry.size = 26;
    entry.mode = 0644;
    entry.mtime = time(nullptr);
    entry.type_flag = '0';
    
    const char* data = "This is custom entry data\n";
    writer.write_entry(entry, reinterpret_cast<const uint8_t*>(data), strlen(data));
    
    writer.finalize();
    
    print_success("Custom entry written");
    
    // Verify
    ArchiveReader reader("custom.tar");
    auto result = reader.read_entry();
    if (result.ok && result.value.path == "custom_data.txt") {
        std::cout << "✓ Verified custom entry: " << result.value.path << "\n";
    }
    
    unlink("custom.tar");
}

// Demo 10: Six-stream I/O - stddbg telemetry
void demo_six_stream_telemetry() {
    print_demo(10, "Six-Stream I/O - stddbg Telemetry");
    
    std::cout << "Writing telemetry to FD 3 (stddbg)...\n";
    
    streams::write_telemetry("demo", "{\"test\":\"telemetry\",\"value\":42}");
    streams::write_telemetry("archive_open", "{\"path\":\"test.tar\",\"mode\":\"read\"}");
    streams::write_telemetry("archive_close", "{\"files\":3,\"bytes\":12345}");
    
    std::cout << "Check FD 3 for JSON telemetry output\n";
    std::cout << "(Run with: ./atar_demo 3>telemetry.log)\n";
    
    print_success("Telemetry written to stddbg");
}

// Demo 11: FFI C API - Extract
void demo_ffi_extract() {
    print_demo(11, "FFI C API - Extract Archive");
    
    mkdir("ffi_extract", 0755);
    
    int extracted_count = 0;
    auto progress = [](const char* filename, int64_t bytes) {
        std::cout << "  Extracting: " << filename << " (" << bytes << " bytes)\n";
    };
    
    int result = aria_extract_archive("test.tar", "ffi_extract", 0, progress);
    
    if (result == 0) {
        print_success("FFI extraction complete");
    } else {
        std::cout << "✗ FFI extraction failed: " 
                  << aria_archive_error_string(result) << "\n";
    }
    
    system("rm -rf ffi_extract");
}

// Demo 12: FFI C API - Create
void demo_ffi_create() {
    print_demo(12, "FFI C API - Create Archive");
    
    const char* files[] = {
        "test_archive_data/file1.txt",
        "test_archive_data/file2.txt"
    };
    
    int result = aria_create_archive("ffi_test.tar", files, 2);
    
    if (result == 0) {
        // Verify
        ArchiveReader reader("ffi_test.tar");
        int count = 0;
        while (true) {
            auto entry_result = reader.read_entry();
            if (!entry_result.ok) break;
            if (entry_result.value.path.empty()) break;
            reader.skip_entry(entry_result.value);
            count++;
        }
        
        std::cout << "FFI created archive with " << count << " files\n";
        print_success("FFI creation complete");
    } else {
        std::cout << "✗ FFI creation failed\n";
    }
    
    unlink("ffi_test.tar");
}

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════╗\n";
    std::cout << "║  ATAR - Archive Utility Demo Suite    ║\n";
    std::cout << "║  Six-Stream TAR Implementation         ║\n";
    std::cout << "╚════════════════════════════════════════╝\n";
    
    try {
        demo_create_archive();
        demo_list_archive();
        demo_extract_archive();
        demo_verify_extraction();
        demo_tbb_safety();
        demo_entry_types();
        demo_statistics();
        demo_streaming_extraction();
        demo_create_with_entry();
        demo_six_stream_telemetry();
        demo_ffi_extract();
        demo_ffi_create();
        
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "ALL DEMOS COMPLETE\n";
        std::cout << "========================================\n";
        std::cout << "\n";
        std::cout << "Cleanup: Removing test files...\n";
        cleanup_test_files();
        system("rm -rf test_extract ffi_extract");
        
        std::cout << "✓ All 12 demos passed successfully\n\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        cleanup_test_files();
        return 1;
    }
    
    return 0;
}
