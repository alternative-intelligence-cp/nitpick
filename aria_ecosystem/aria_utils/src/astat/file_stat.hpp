#ifndef ARIA_UTILS_ASTAT_FILE_STAT_HPP
#define ARIA_UTILS_ASTAT_FILE_STAT_HPP

#include <cstdint>
#include <string>
#include <ctime>

// ============================================================================
// ASTAT - Cross-Platform File Statistics with Six-Stream Topology
// ============================================================================
// Purpose: Retrieve detailed file metadata (size, times, permissions, ownership)
//          and emit both human-readable UI (stdout) and machine-readable
//          binary structs (stddato FD 5) for pipeline composition.
//
// Architecture:
//   - Control Plane (FD 0-2): stdin, stdout, stderr for UI/errors
//   - Observability Plane (FD 3): stddbg for telemetry/diagnostics
//   - Data Plane (FD 4-5): stddati/stddato for binary data exchange
//
// TBB Integration:
//   - Timestamps use tbb64 with sticky ERR sentinel (-INT64_MIN)
//   - File sizes use tbb64 to handle overflow gracefully
//   - All arithmetic propagates errors automatically
//
// Cross-Platform:
//   - Linux: Uses stat64/lstat64 for full 64-bit support
//   - Windows: Uses _stati64 or GetFileAttributesEx
//   - MacOS: Uses stat with proper field mapping
// ============================================================================

namespace aria {
namespace utils {
namespace astat {

// ============================================================================
// Twisted Balanced Binary (TBB) Type Aliases
// ============================================================================
// TBB64: Symmetric range with sticky error sentinel
// Range: [-INT64_MAX, +INT64_MAX], ERR = -INT64_MIN
using tbb64 = int64_t;
using tbb32 = int32_t;
using tbb16 = int16_t;
using tbb8  = int8_t;

// TBB Error Sentinels (minimum representable values)
constexpr tbb64 TBB64_ERR = INT64_MIN;
constexpr tbb32 TBB32_ERR = INT32_MIN;
constexpr tbb16 TBB16_ERR = INT16_MIN;
constexpr tbb8  TBB8_ERR  = INT8_MIN;

// ============================================================================
// Error Codes
// ============================================================================
enum class ErrorCode : int32_t {
    SUCCESS            = 0,
    FILE_NOT_FOUND     = 1,
    PERMISSION_DENIED  = 2,
    IO_ERROR           = 3,
    INVALID_PATH       = 4,
    SYMLINK_LOOP       = 5,
    NAME_TOO_LONG      = 6,
    OVERFLOW           = 7,
    UNKNOWN_ERROR      = 99
};

// ============================================================================
// File Type Enumeration
// ============================================================================
enum class FileType : uint8_t {
    REGULAR      = 0x01,  // Regular file
    DIRECTORY    = 0x02,  // Directory
    SYMLINK      = 0x03,  // Symbolic link
    BLOCK_DEV    = 0x04,  // Block device
    CHAR_DEV     = 0x05,  // Character device
    FIFO         = 0x06,  // Named pipe (FIFO)
    SOCKET       = 0x07,  // Unix domain socket
    UNKNOWN      = 0xFF   // Unknown/unsupported type
};

// ============================================================================
// Normalized File Information Structure
// ============================================================================
// This structure is designed for binary serialization to stddato (FD 5).
// Layout is packed and platform-independent (no padding).
//
// TBB Fields:
//   - Timestamp fields use tbb64 (nanoseconds since epoch)
//   - If filesystem doesn't support a time field → TBB64_ERR
//   - Size uses tbb64 to handle overflow gracefully
//
// Example Binary Protocol (Aria Test Protocol - ATP):
//   Offset | Field        | Size | Type
//   -------|--------------|------|-----
//   0      | size         | 8    | tbb64
//   8      | created      | 8    | tbb64 (ctime)
//   16     | modified     | 8    | tbb64 (mtime)
//   24     | accessed     | 8    | tbb64 (atime)
//   32     | mode         | 4    | uint32
//   36     | uid          | 4    | uint32
//   40     | gid          | 4    | uint32
//   44     | type         | 1    | FileType
//   45     | reserved     | 3    | padding
//   48     | inode        | 8    | uint64
//   56     | nlinks       | 8    | uint64
//   64     | path_len     | 2    | uint16
//   66     | path         | N    | UTF-8 bytes
// ============================================================================
struct FileInfo {
    // File size in bytes (TBB64 - sticky error on overflow)
    tbb64 size;
    
    // Timestamps in nanoseconds since Unix epoch (1970-01-01 00:00:00 UTC)
    // TBB64_ERR if filesystem doesn't support this field
    tbb64 created;   // ctime (status change time on POSIX, creation on Windows)
    tbb64 modified;  // mtime (modification time)
    tbb64 accessed;  // atime (access time)
    
    // Unix-style permission bits (rwxrwxrwx)
    // 0xFFFFFFFF if not applicable (Windows FAT32)
    uint32_t mode;
    
    // User ID (owner)
    // 0xFFFFFFFF on Windows or if unavailable
    uint32_t uid;
    
    // Group ID
    // 0xFFFFFFFF on Windows or if unavailable
    uint32_t gid;
    
    // File type classification
    FileType type;
    
    // Reserved for alignment (future use)
    uint8_t reserved[3];
    
    // Inode number (POSIX) or file index (Windows)
    // 0 if unavailable
    uint64_t inode;
    
    // Number of hard links
    // 1 for typical files, 0 if unavailable
    uint64_t nlinks;
    
    // Path length (for binary serialization)
    uint16_t path_len;
    
    // Helpers for TBB validation
    [[nodiscard]] bool has_valid_size() const noexcept {
        return size != TBB64_ERR;
    }
    
    [[nodiscard]] bool has_valid_created() const noexcept {
        return created != TBB64_ERR;
    }
    
    [[nodiscard]] bool has_valid_modified() const noexcept {
        return modified != TBB64_ERR;
    }
    
    [[nodiscard]] bool has_valid_accessed() const noexcept {
        return accessed != TBB64_ERR;
    }
};

// ============================================================================
// Result Type (Monad Pattern)
// ============================================================================
template <typename T>
struct Result {
    T value;
    ErrorCode error;
    bool is_error;
    
    [[nodiscard]] bool ok() const noexcept { return !is_error; }
    [[nodiscard]] bool err() const noexcept { return is_error; }
    
    static Result<T> success(T val) {
        return Result<T>{val, ErrorCode::SUCCESS, false};
    }
    
    static Result<T> failure(ErrorCode ec) {
        return Result<T>{T{}, ec, true};
    }
};

// ============================================================================
// Core API
// ============================================================================

// Get file statistics (follows symlinks)
// Returns FileInfo on success, ErrorCode on failure
Result<FileInfo> get_file_stat(const std::string& path) noexcept;

// Get file statistics (does NOT follow symlinks)
// Returns FileInfo on success, ErrorCode on failure
Result<FileInfo> get_link_stat(const std::string& path) noexcept;

// Convert ErrorCode to human-readable string
std::string error_to_string(ErrorCode ec) noexcept;

// Convert FileType to human-readable string
std::string file_type_to_string(FileType type) noexcept;

// Format file mode (permissions) as "rwxr-xr-x" string
std::string format_permissions(uint32_t mode) noexcept;

// Format timestamp (tbb64 nanoseconds) to ISO 8601 string
// Returns "N/A" if timestamp is TBB64_ERR
std::string format_timestamp(tbb64 nanos) noexcept;

// Format file size with human-readable units (KB, MB, GB, etc.)
// Handles TBB64_ERR gracefully
std::string format_size(tbb64 bytes) noexcept;

// ============================================================================
// Six-Stream I/O Utilities
// ============================================================================

// Write FileInfo to stddato (FD 5) in binary format
// Uses ATP (Aria Test Protocol) binary serialization
// Returns bytes written or negative error code
int64_t write_binary_to_stddato(const FileInfo& info, const std::string& path) noexcept;

// Write human-readable summary to stdout (FD 1)
// Uses ANSI color codes for file type highlighting
void write_ui_to_stdout(const FileInfo& info, const std::string& path) noexcept;

// Write diagnostic message to stddbg (FD 3)
// Format: JSON structured log
void write_log_to_stddbg(const std::string& message, const std::string& level = "INFO") noexcept;

// ============================================================================
// FFI C API (For Aria Integration)
// ============================================================================
extern "C" {
    // Get file statistics
    // Returns 0 on success, error code on failure
    // out_info must be pre-allocated
    int32_t aria_file_stat(const char* path, FileInfo* out_info);
    
    // Get link statistics (no follow)
    int32_t aria_link_stat(const char* path, FileInfo* out_info);
    
    // Format timestamp to buffer
    // buf_size should be at least 32 bytes
    // Returns bytes written (excluding null terminator)
    int32_t aria_format_timestamp(tbb64 nanos, char* buf, size_t buf_size);
    
    // Format file size to buffer
    // buf_size should be at least 16 bytes
    int32_t aria_format_size(tbb64 bytes, char* buf, size_t buf_size);
    
    // Get error string
    // Returns pointer to static string (do not free)
    const char* aria_stat_error_string(int32_t error_code);
}

} // namespace astat
} // namespace utils
} // namespace aria

#endif // ARIA_UTILS_ASTAT_FILE_STAT_HPP
