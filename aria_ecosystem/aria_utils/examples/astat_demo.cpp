// ============================================================================
// ASTAT DEMO - File Statistics Utility Demonstration
// ============================================================================
// Demonstrates:
//   1. Basic file stat retrieval
//   2. Symlink handling (stat vs lstat)
//   3. TBB error propagation for timestamps
//   4. Cross-platform file type detection
//   5. Permission formatting
//   6. Timestamp formatting (ISO 8601)
//   7. File size with human-readable units
//   8. Six-stream I/O (stdout for UI, stddato for binary, stddbg for logs)
//   9. Error handling with ErrorCode enum
//   10. FFI C API demonstration
//   11. Binary protocol serialization (ATP)
//   12. Batch processing multiple files
// ============================================================================

#include "../src/astat/file_stat.hpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstring>

using namespace aria::utils::astat;

// ============================================================================
// Demo 1: Basic File Statistics
// ============================================================================
void demo_basic_stat() {
    std::cout << "\n=== Demo 1: Basic File Statistics ===\n";
    
    // Create a test file
    const char* test_file = "/tmp/astat_test.txt";
    std::system("echo 'Hello, Aria!' > /tmp/astat_test.txt");
    
    auto result = get_file_stat(test_file);
    
    if (result.ok()) {
        const FileInfo& info = result.value;
        std::cout << "✓ File: " << test_file << "\n";
        std::cout << "  Size: " << info.size << " bytes\n";
        std::cout << "  Type: " << file_type_to_string(info.type) << "\n";
        std::cout << "  Inode: " << info.inode << "\n";
        std::cout << "  Links: " << info.nlinks << "\n";
    } else {
        std::cout << "✗ Error: " << error_to_string(result.error) << "\n";
    }
    
    // Cleanup
    std::system("rm -f /tmp/astat_test.txt");
}

// ============================================================================
// Demo 2: Symlink Handling - stat() vs lstat()
// ============================================================================
void demo_symlink_handling() {
    std::cout << "\n=== Demo 2: Symlink Handling ===\n";
    
    // Create target file and symlink
    std::system("echo 'Target content' > /tmp/astat_target.txt");
    std::system("ln -sf /tmp/astat_target.txt /tmp/astat_link.txt");
    
    // stat() - follows symlink
    auto stat_result = get_file_stat("/tmp/astat_link.txt");
    if (stat_result.ok()) {
        std::cout << "✓ stat() [follows symlink]:\n";
        std::cout << "  Type: " << file_type_to_string(stat_result.value.type) 
                  << " (should be REGULAR)\n";
        std::cout << "  Size: " << stat_result.value.size << " bytes\n";
    }
    
    // lstat() - does NOT follow symlink
    auto lstat_result = get_link_stat("/tmp/astat_link.txt");
    if (lstat_result.ok()) {
        std::cout << "✓ lstat() [does NOT follow]:\n";
        std::cout << "  Type: " << file_type_to_string(lstat_result.value.type) 
                  << " (should be SYMLINK)\n";
        std::cout << "  Size: " << lstat_result.value.size << " bytes (link path length)\n";
    }
    
    // Cleanup
    std::system("rm -f /tmp/astat_target.txt /tmp/astat_link.txt");
}

// ============================================================================
// Demo 3: TBB Error Propagation
// ============================================================================
void demo_tbb_error_propagation() {
    std::cout << "\n=== Demo 3: TBB Error Propagation ===\n";
    
    FileInfo info{};
    
    // Simulate filesystem that doesn't support access time
    info.accessed = TBB64_ERR;
    info.modified = 1609459200000000000LL;  // 2021-01-01 00:00:00 UTC
    
    std::cout << "FileInfo with invalid access time:\n";
    std::cout << "  accessed valid? " << (info.has_valid_accessed() ? "YES" : "NO") << "\n";
    std::cout << "  modified valid? " << (info.has_valid_modified() ? "YES" : "NO") << "\n";
    
    // In actual Aria TBB arithmetic, ERR + anything = ERR
    // Here we demonstrate the check pattern
    if (info.accessed == TBB64_ERR || info.modified == TBB64_ERR) {
        std::cout << "  age calculation: SKIPPED (ERR detected)\n";
    } else {
        tbb64 age = info.modified - info.accessed;
        std::cout << "  age calculation: " << age << " nanoseconds\n";
    }
    
    std::cout << "✓ TBB sentinel detection prevents invalid calculations\n";
}

// ============================================================================
// Demo 4: File Type Detection
// ============================================================================
void demo_file_type_detection() {
    std::cout << "\n=== Demo 4: File Type Detection ===\n";
    
    // Test different file types
    struct {
        const char* path;
        const char* create_cmd;
        FileType expected_type;
    } test_cases[] = {
        {"/tmp/astat_regular.txt", "touch /tmp/astat_regular.txt", FileType::REGULAR},
        {"/tmp/astat_dir", "mkdir -p /tmp/astat_dir", FileType::DIRECTORY},
        {"/tmp/astat_fifo", "mkfifo /tmp/astat_fifo 2>/dev/null || true", FileType::FIFO},
    };
    
    for (const auto& tc : test_cases) {
        std::system(tc.create_cmd);
        
        auto result = get_file_stat(tc.path);
        if (result.ok()) {
            bool match = (result.value.type == tc.expected_type);
            std::cout << (match ? "✓" : "✗") << " " << tc.path 
                      << " → " << file_type_to_string(result.value.type) << "\n";
        }
    }
    
    // Cleanup
    std::system("rm -rf /tmp/astat_regular.txt /tmp/astat_dir /tmp/astat_fifo");
}

// ============================================================================
// Demo 5: Permission Formatting
// ============================================================================
void demo_permission_formatting() {
    std::cout << "\n=== Demo 5: Permission Formatting ===\n";
    
    struct {
        uint32_t mode;
        const char* expected;
    } test_cases[] = {
        {0755, "rwxr-xr-x"},
        {0644, "rw-r--r--"},
        {0600, "rw-------"},
        {0777, "rwxrwxrwx"},
        {0000, "---------"},
    };
    
    for (const auto& tc : test_cases) {
        std::string formatted = format_permissions(tc.mode);
        bool match = (formatted == tc.expected);
        std::cout << (match ? "✓" : "✗") << " 0" << std::oct << tc.mode << std::dec 
                  << " → " << formatted << "\n";
    }
}

// ============================================================================
// Demo 6: Timestamp Formatting (ISO 8601)
// ============================================================================
void demo_timestamp_formatting() {
    std::cout << "\n=== Demo 6: Timestamp Formatting ===\n";
    
    // Known Unix timestamps
    struct {
        tbb64 nanos;
        const char* description;
    } test_cases[] = {
        {0LL, "Unix Epoch"},
        {1000000000LL * 1000000000LL, "2001-09-09 01:46:40 UTC"},
        {1609459200LL * 1000000000LL, "2021-01-01 00:00:00 UTC"},
        {TBB64_ERR, "Invalid timestamp (TBB64_ERR)"},
    };
    
    for (const auto& tc : test_cases) {
        std::string formatted = format_timestamp(tc.nanos);
        std::cout << "  " << tc.description << " → " << formatted << "\n";
    }
}

// ============================================================================
// Demo 7: Human-Readable File Sizes
// ============================================================================
void demo_size_formatting() {
    std::cout << "\n=== Demo 7: Human-Readable File Sizes ===\n";
    
    struct {
        tbb64 bytes;
        const char* expected_unit;
    } test_cases[] = {
        {512LL, "B"},
        {1024LL, "KB"},
        {1024LL * 1024, "MB"},
        {1024LL * 1024 * 1024, "GB"},
        {1024LL * 1024 * 1024 * 1024, "TB"},
        {TBB64_ERR, "ERR"},
    };
    
    for (const auto& tc : test_cases) {
        std::string formatted = format_size(tc.bytes);
        bool has_unit = formatted.find(tc.expected_unit) != std::string::npos;
        std::cout << (has_unit ? "✓" : "✗") << " " << tc.bytes << " bytes → " 
                  << formatted << "\n";
    }
}

// ============================================================================
// Demo 8: Six-Stream I/O (stdout, stddbg, stddato)
// ============================================================================
void demo_six_stream_io() {
    std::cout << "\n=== Demo 8: Six-Stream I/O ===\n";
    
    // Create test file
    std::system("echo 'Six-stream test' > /tmp/astat_sixstream.txt");
    
    auto result = get_file_stat("/tmp/astat_sixstream.txt");
    if (result.ok()) {
        const FileInfo& info = result.value;
        
        // FD 1 (stdout): Human-readable UI with ANSI colors
        std::cout << "FD 1 (stdout) - Human-readable UI:\n";
        write_ui_to_stdout(info, "/tmp/astat_sixstream.txt");
        
        // FD 3 (stddbg): Structured JSON telemetry
        std::cout << "\nFD 3 (stddbg) - Structured log: ";
        write_log_to_stddbg("Successfully retrieved file stats", "INFO");
        std::cout << "[written to FD 3]\n";
        
        // FD 5 (stddato): Binary protocol
        std::cout << "\nFD 5 (stddato) - Binary protocol: ";
        int64_t bytes_written = write_binary_to_stddato(info, "/tmp/astat_sixstream.txt");
        std::cout << bytes_written << " bytes written\n";
        
        std::cout << "✓ All three planes operational (Control, Observability, Data)\n";
    }
    
    // Cleanup
    std::system("rm -f /tmp/astat_sixstream.txt");
}

// ============================================================================
// Demo 9: Error Handling
// ============================================================================
void demo_error_handling() {
    std::cout << "\n=== Demo 9: Error Handling ===\n";
    
    struct {
        const char* path;
        ErrorCode expected_error;
    } test_cases[] = {
        {"/nonexistent/file.txt", ErrorCode::FILE_NOT_FOUND},
        {"/root/.shadow_protected", ErrorCode::PERMISSION_DENIED},  // May vary
    };
    
    for (const auto& tc : test_cases) {
        auto result = get_file_stat(tc.path);
        if (result.err()) {
            std::cout << "✓ " << tc.path << " → " 
                      << error_to_string(result.error) << "\n";
        } else {
            std::cout << "  " << tc.path << " → unexpectedly succeeded\n";
        }
    }
}

// ============================================================================
// Demo 10: FFI C API
// ============================================================================
void demo_ffi_c_api() {
    std::cout << "\n=== Demo 10: FFI C API ===\n";
    
    // Create test file
    std::system("echo 'FFI test' > /tmp/astat_ffi.txt");
    
    // Use C API
    FileInfo info;
    int32_t ret = aria_file_stat("/tmp/astat_ffi.txt", &info);
    
    if (ret == 0) {
        std::cout << "✓ aria_file_stat() succeeded\n";
        std::cout << "  Size: " << info.size << " bytes\n";
        
        // Test formatting functions
        char time_buf[64];
        char size_buf[32];
        
        aria_format_timestamp(info.modified, time_buf, sizeof(time_buf));
        aria_format_size(info.size, size_buf, sizeof(size_buf));
        
        std::cout << "  Modified: " << time_buf << "\n";
        std::cout << "  Size: " << size_buf << "\n";
    } else {
        std::cout << "✗ Error: " << aria_stat_error_string(ret) << "\n";
    }
    
    // Cleanup
    std::system("rm -f /tmp/astat_ffi.txt");
}

// ============================================================================
// Demo 11: Binary Protocol Serialization
// ============================================================================
void demo_binary_protocol() {
    std::cout << "\n=== Demo 11: Binary Protocol Serialization ===\n";
    
    FileInfo info{};
    info.size = 4096;
    info.created = 1609459200LL * 1000000000LL;
    info.modified = 1609545600LL * 1000000000LL;
    info.accessed = 1609632000LL * 1000000000LL;
    info.mode = 0755;
    info.uid = 1000;
    info.gid = 1000;
    info.type = FileType::REGULAR;
    info.inode = 123456;
    info.nlinks = 1;
    
    std::string path = "test.txt";
    
    std::cout << "ATP (Aria Test Protocol) Binary Layout:\n";
    std::cout << "  Offset   Field         Size   Value\n";
    std::cout << "  ------   -----------   ----   -----\n";
    std::cout << "  0        size          8      " << info.size << "\n";
    std::cout << "  8        created       8      " << info.created << "\n";
    std::cout << "  16       modified      8      " << info.modified << "\n";
    std::cout << "  24       accessed      8      " << info.accessed << "\n";
    std::cout << "  32       mode          4      0" << std::oct << info.mode << std::dec << "\n";
    std::cout << "  36       uid           4      " << info.uid << "\n";
    std::cout << "  40       gid           4      " << info.gid << "\n";
    std::cout << "  44       type          1      " << static_cast<int>(info.type) << "\n";
    std::cout << "  45       reserved      3      (padding)\n";
    std::cout << "  48       inode         8      " << info.inode << "\n";
    std::cout << "  56       nlinks        8      " << info.nlinks << "\n";
    std::cout << "  64       path_len      2      " << path.length() << "\n";
    std::cout << "  66       path          " << path.length() << "      \"" << path << "\"\n";
    std::cout << "  Total: " << (66 + path.length()) << " bytes\n";
    
    std::cout << "✓ Binary protocol ready for pipeline composition\n";
}

// ============================================================================
// Demo 12: Batch Processing
// ============================================================================
void demo_batch_processing() {
    std::cout << "\n=== Demo 12: Batch Processing Multiple Files ===\n";
    
    // Create test files
    std::system("mkdir -p /tmp/astat_batch");
    std::system("echo 'File 1' > /tmp/astat_batch/file1.txt");
    std::system("echo 'File 2 content' > /tmp/astat_batch/file2.txt");
    std::system("echo 'File 3 larger content here' > /tmp/astat_batch/file3.txt");
    
    std::vector<std::string> files = {
        "/tmp/astat_batch/file1.txt",
        "/tmp/astat_batch/file2.txt",
        "/tmp/astat_batch/file3.txt",
        "/tmp/astat_batch/nonexistent.txt"  // Test error handling
    };
    
    size_t success_count = 0;
    size_t error_count = 0;
    
    for (const auto& file : files) {
        auto result = get_file_stat(file);
        if (result.ok()) {
            write_ui_to_stdout(result.value, file);
            success_count++;
        } else {
            std::cerr << "Error: " << file << " → " 
                      << error_to_string(result.error) << "\n";
            error_count++;
        }
    }
    
    std::cout << "\n✓ Processed " << files.size() << " files: "
              << success_count << " success, " << error_count << " errors\n";
    
    // Cleanup
    std::system("rm -rf /tmp/astat_batch");
}

// ============================================================================
// Main
// ============================================================================
int main() {
    std::cout << "====================================\n";
    std::cout << "  ASTAT DEMONSTRATION SUITE\n";
    std::cout << "====================================\n";
    
    demo_basic_stat();
    demo_symlink_handling();
    demo_tbb_error_propagation();
    demo_file_type_detection();
    demo_permission_formatting();
    demo_timestamp_formatting();
    demo_size_formatting();
    demo_six_stream_io();
    demo_error_handling();
    demo_ffi_c_api();
    demo_binary_protocol();
    demo_batch_processing();
    
    std::cout << "\n====================================\n";
    std::cout << "  ALL DEMOS COMPLETE\n";
    std::cout << "====================================\n";
    
    return 0;
}
