/**
 * @file archive.hpp
 * @brief Archive Manager - TAR format creation/extraction with six-stream I/O
 * 
 * Implements archive utilities with Aria's architectural patterns:
 * - Six-stream topology (stdout/UI, stddbg/telemetry, stddato/binary)
 * - TBB type safety for dimensions and offsets
 * - Binary protocol for structured archive metadata
 * - Zero-copy streaming for high throughput
 * 
 * Reference: sysUtils_10_tar.txt (575 lines)
 */

#ifndef ARIA_ATAR_ARCHIVE_HPP
#define ARIA_ATAR_ARCHIVE_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <functional>
#include <sys/stat.h>

namespace aria {
namespace atar {

// ============================================================================
// TBB Type Definitions
// ============================================================================

using tbb8 = int8_t;
using tbb32 = int32_t;
using tbb64 = int64_t;

// TBB Sentinels - sticky error values
constexpr tbb64 TBB64_ERR = INT64_MIN;
constexpr tbb32 TBB32_ERR = INT32_MIN;
constexpr tbb8 TBB8_ERR = INT8_MIN;

// TBB safety check
inline bool is_tbb_error(tbb64 val) {
    return val == TBB64_ERR;
}

// ============================================================================
// Result Monad for Error Handling
// ============================================================================

template<typename T>
struct Result {
    T value;
    bool ok;
    std::string error_msg;

    static Result success(const T& v) {
        return Result{v, true, ""};
    }

    static Result error(const std::string& msg) {
        return Result{T{}, false, msg};
    }
};

// ============================================================================
// Archive Entry - Logical representation of archived file
// ============================================================================

struct ArchiveEntry {
    // Identity and metadata
    std::string path;           // Normalized file path
    std::string link_name;      // Target for symlinks/hardlinks
    
    // Dimensions (TBB safety)
    tbb64 size;                 // File size in bytes
    tbb64 header_offset;        // Position of header in archive
    tbb64 payload_offset;       // Position of data in archive
    
    // Permissions and ownership
    tbb64 mode;                 // File permissions (octal)
    tbb64 uid;                  // User ID
    tbb64 gid;                  // Group ID
    std::string uname;          // User name
    std::string gname;          // Group name
    
    // Timestamps
    tbb64 mtime;                // Modification time (Unix epoch)
    
    // Type flags (USTAR standard)
    // '0' or '\0' = regular file
    // '5' = directory
    // '2' = symbolic link
    // '1' = hard link
    char type_flag;
    
    // Integrity
    tbb64 checksum_read;        // Checksum from header
    tbb64 checksum_calc;        // Calculated checksum
    bool is_valid;              // Checksum validation result
    
    // Constructor
    ArchiveEntry()
        : size(0), header_offset(0), payload_offset(0)
        , mode(0), uid(0), gid(0)
        , mtime(0)
        , type_flag('0')
        , checksum_read(0), checksum_calc(0)
        , is_valid(false)
    {}
};

// ============================================================================
// Archive Statistics - Telemetry data
// ============================================================================

struct ArchiveStats {
    tbb64 files_processed;      // Total files handled
    tbb64 files_skipped;        // Files filtered/skipped
    tbb64 bytes_read;           // Raw bytes input
    tbb64 bytes_written;        // Raw bytes output
    tbb64 errors_count;         // Non-fatal error count
    
    ArchiveStats()
        : files_processed(0), files_skipped(0)
        , bytes_read(0), bytes_written(0)
        , errors_count(0)
    {}
};

// ============================================================================
// Archive Options
// ============================================================================

struct ArchiveOptions {
    bool verbose;               // Print file names
    bool preserve_permissions;  // Preserve mode/ownership
    bool extract_to_stdout;     // Extract to stdout instead of disk
    std::string output_dir;     // Base directory for extraction
    
    ArchiveOptions()
        : verbose(false)
        , preserve_permissions(false)
        , extract_to_stdout(false)
        , output_dir(".")
    {}
};

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {
    // Write to stddbg (FD 3) - JSON structured telemetry
    void write_telemetry(const std::string& event, const std::string& data);
    
    // Write to stddato (FD 5) - Binary archive metadata protocol
    // Format: [Magic:0x41544152][MetaLen:tbb64][DataLen:tbb64][JSON metadata][payload]
    void write_entry_frame(const ArchiveEntry& entry, const std::vector<uint8_t>& payload);
    
    // Check if FD is connected to TTY or /dev/null
    bool is_fd_active(int fd);
}

// ============================================================================
// TAR Header Parser
// ============================================================================

class TarParser {
public:
    // Parse USTAR header block (512 bytes)
    static Result<ArchiveEntry> parse_header(const uint8_t* block, tbb64 offset);
    
    // Create USTAR header from entry
    static Result<std::vector<uint8_t>> create_header(const ArchiveEntry& entry);
    
private:
    // Parse octal string to tbb64 (TBB-safe, returns ERR on invalid)
    static tbb64 parse_octal(const uint8_t* ptr, size_t len);
    
    // Calculate TAR header checksum
    static tbb64 calculate_checksum(const uint8_t* block);
    
    // Format octal string for header
    static void format_octal(uint8_t* ptr, size_t len, tbb64 value);
};

// ============================================================================
// Archive Reader
// ============================================================================

class ArchiveReader {
public:
    explicit ArchiveReader(const std::string& filename);
    ~ArchiveReader();
    
    // Read next entry header
    Result<ArchiveEntry> read_entry();
    
    // Extract entry to file
    Result<bool> extract_entry(const ArchiveEntry& entry, const ArchiveOptions& opts);
    
    // Extract entry to callback (streaming)
    Result<bool> extract_entry_stream(
        const ArchiveEntry& entry,
        std::function<void(const uint8_t*, size_t)> callback
    );
    
    // Skip to next entry
    Result<bool> skip_entry(const ArchiveEntry& entry);
    
    // Get current statistics
    const ArchiveStats& get_stats() const { return stats_; }
    
    // Check if at end of archive
    bool is_eof() const;
    
private:
    int fd_;                    // File descriptor
    tbb64 position_;            // Current read position
    ArchiveStats stats_;
    std::vector<uint8_t> buffer_; // Reusable I/O buffer
};

// ============================================================================
// Archive Writer
// ============================================================================

class ArchiveWriter {
public:
    explicit ArchiveWriter(const std::string& filename);
    ~ArchiveWriter();
    
    // Add file to archive
    Result<bool> add_file(const std::string& path);
    
    // Add directory to archive (recursive)
    Result<bool> add_directory(const std::string& path, bool recursive = true);
    
    // Write entry with explicit data
    Result<bool> write_entry(const ArchiveEntry& entry, const uint8_t* data, size_t len);
    
    // Finalize archive (write end-of-archive markers)
    Result<bool> finalize();
    
    // Get current statistics
    const ArchiveStats& get_stats() const { return stats_; }
    
private:
    int fd_;                    // File descriptor
    tbb64 position_;            // Current write position
    ArchiveStats stats_;
    std::vector<uint8_t> buffer_; // Reusable I/O buffer
    
    // Helper: create entry from file stat
    Result<ArchiveEntry> create_entry_from_stat(
        const std::string& path,
        const struct stat& st
    );
};

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {
    // Extract archive to directory
    int aria_extract_archive(
        const char* archive_path,
        const char* output_dir,
        int verbose,
        void (*progress_callback)(const char* filename, int64_t bytes)
    );
    
    // Create archive from file list
    int aria_create_archive(
        const char* archive_path,
        const char** file_list,
        int file_count
    );
    
    // List archive contents
    int aria_list_archive(
        const char* archive_path,
        void (*entry_callback)(const char* filename, int64_t size, int64_t mtime)
    );
    
    // Get error string for return code
    const char* aria_archive_error_string(int code);
}

} // namespace atar
} // namespace aria

#endif // ARIA_ATAR_ARCHIVE_HPP
