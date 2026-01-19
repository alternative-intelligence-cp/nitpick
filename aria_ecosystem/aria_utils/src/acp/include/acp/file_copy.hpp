/**
 * file_copy.hpp
 * Hex-Stream File Copy Library
 *
 * Implements the acp utility's core functionality:
 * - Zero-copy file copying with copy_file_range() / sendfile()
 * - Recursive directory copying
 * - Attribute preservation (permissions, timestamps, ownership)
 * - Progress reporting and telemetry
 *
 * Architecture follows the Aria Six-Stream Topology:
 * - FD 1 (stdout): Progress and status output
 * - FD 3 (stddbg): Structured JSON telemetry
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ACP_FILE_COPY_HPP
#define ARIA_ACP_FILE_COPY_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace aria {
namespace acp {

// ============================================================================
// TBB Sentinel (Sticky Error)
// ============================================================================

constexpr int64_t TBB64_ERR = INT64_MIN;

inline bool is_tbb_err(int64_t v) { return v == TBB64_ERR; }

inline int64_t tbb_add(int64_t a, int64_t b) {
    if (is_tbb_err(a) || is_tbb_err(b)) return TBB64_ERR;
    if (b > 0 && a > INT64_MAX - b) return TBB64_ERR;
    if (b < 0 && a < INT64_MIN - b) return TBB64_ERR;
    return a + b;
}

// ============================================================================
// Hex-Stream File Descriptors
// ============================================================================

constexpr int FD_STDIN   = 0;
constexpr int FD_STDOUT  = 1;
constexpr int FD_STDERR  = 2;
constexpr int FD_STDDBG  = 3;

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    OK = 0,
    SOURCE_NOT_FOUND = 1,
    SOURCE_NOT_READABLE = 2,
    DEST_EXISTS = 3,
    DEST_NOT_WRITABLE = 4,
    IS_DIRECTORY = 5,
    NOT_DIRECTORY = 6,
    SAME_FILE = 7,
    COPY_INTO_SELF = 8,
    DISK_FULL = 9,
    READ_ERROR = 10,
    WRITE_ERROR = 11,
    PERMISSION_ERROR = 12,
    ATTRIBUTE_ERROR = 13,
    SYMLINK_ERROR = 14,
    SYSTEM_ERROR = 99
};

const char* error_to_string(ErrorCode code);

// ============================================================================
// Copy Options
// ============================================================================

struct CopyOptions {
    bool recursive = false;              // -r: Copy directories recursively
    bool force = false;                  // -f: Force overwrite
    bool interactive = false;            // -i: Prompt before overwrite
    bool no_clobber = false;             // -n: Don't overwrite existing
    bool update = false;                 // -u: Only copy if src is newer
    bool verbose = false;                // -v: Verbose output

    bool preserve_mode = true;           // -p: Preserve file mode
    bool preserve_ownership = false;     // Preserve owner/group (needs root)
    bool preserve_timestamps = true;     // Preserve access/modification times
    bool preserve_links = true;          // Copy symlinks as links
    bool dereference = false;            // -L: Follow symlinks

    bool one_filesystem = false;         // -x: Don't cross filesystem boundaries
    bool sparse = true;                  // Handle sparse files efficiently
    bool reflink = true;                 // Use reflink (copy-on-write) if available

    size_t buffer_size = 1024 * 1024;    // 1MB copy buffer
    bool use_zero_copy = true;           // Use sendfile/copy_file_range

    CopyOptions() = default;
};

// ============================================================================
// Copy Result
// ============================================================================

struct CopyResult {
    ErrorCode error = ErrorCode::OK;
    std::string error_message;
    std::string error_path;              // Path where error occurred

    int64_t bytes_copied = 0;            // Total bytes copied (TBB-safe)
    int64_t files_copied = 0;            // Number of files copied
    int64_t dirs_created = 0;            // Number of directories created
    int64_t symlinks_copied = 0;         // Number of symlinks copied
    int64_t files_skipped = 0;           // Files skipped (no-clobber, update, etc.)

    double elapsed_ms = 0.0;
    double throughput_mbps = 0.0;

    bool is_ok() const { return error == ErrorCode::OK; }
};

// ============================================================================
// Progress Callback
// ============================================================================

struct CopyProgress {
    std::string current_file;            // Currently copying file
    int64_t file_bytes_copied;           // Bytes copied of current file
    int64_t file_bytes_total;            // Total bytes of current file
    int64_t total_bytes_copied;          // Total bytes copied overall
    int64_t total_bytes;                 // Total bytes to copy (if known)
    int64_t files_completed;             // Files completed
    int64_t files_total;                 // Total files (if known)
    double percent;                      // Overall progress percentage
};

// Return false to cancel copy
using ProgressCallback = std::function<bool(const CopyProgress& progress)>;

// ============================================================================
// Telemetry Writer
// ============================================================================

class TelemetryWriter {
public:
    explicit TelemetryWriter(int fd = FD_STDDBG);

    void log_start(const std::string& src, const std::string& dst);
    void log_file_start(const std::string& path, int64_t size);
    void log_file_complete(const std::string& path, int64_t bytes, double ms);
    void log_dir_create(const std::string& path);
    void log_symlink(const std::string& path, const std::string& target);
    void log_skip(const std::string& path, const std::string& reason);
    void log_error(const std::string& path, ErrorCode code, const std::string& message);
    void log_complete(int64_t files, int64_t bytes, double ms);

private:
    int fd_;
    void write_json(const std::string& json);
};

// ============================================================================
// Main API
// ============================================================================

/**
 * Copy a single file
 *
 * @param src Source file path
 * @param dst Destination path (file or directory)
 * @param opts Copy options
 * @param progress Optional progress callback
 * @param telemetry Optional telemetry writer
 * @return Copy result
 */
CopyResult copy_file(const std::string& src,
                     const std::string& dst,
                     const CopyOptions& opts = CopyOptions(),
                     ProgressCallback progress = nullptr,
                     TelemetryWriter* telemetry = nullptr);

/**
 * Copy multiple files to a directory
 *
 * @param sources List of source paths
 * @param dst_dir Destination directory
 * @param opts Copy options
 * @param progress Optional progress callback
 * @param telemetry Optional telemetry writer
 * @return Copy result
 */
CopyResult copy_files(const std::vector<std::string>& sources,
                      const std::string& dst_dir,
                      const CopyOptions& opts = CopyOptions(),
                      ProgressCallback progress = nullptr,
                      TelemetryWriter* telemetry = nullptr);

/**
 * Copy a directory recursively
 *
 * @param src Source directory
 * @param dst Destination path
 * @param opts Copy options (recursive will be forced true)
 * @param progress Optional progress callback
 * @param telemetry Optional telemetry writer
 * @return Copy result
 */
CopyResult copy_directory(const std::string& src,
                          const std::string& dst,
                          const CopyOptions& opts = CopyOptions(),
                          ProgressCallback progress = nullptr,
                          TelemetryWriter* telemetry = nullptr);

// ============================================================================
// Low-Level API
// ============================================================================

/**
 * Copy file data using zero-copy when available
 *
 * @param src_fd Source file descriptor
 * @param dst_fd Destination file descriptor
 * @param size Number of bytes to copy (0 = until EOF)
 * @param buffer_size Fallback buffer size
 * @return Bytes copied or TBB64_ERR on error
 */
int64_t copy_file_data(int src_fd, int dst_fd,
                       int64_t size = 0,
                       size_t buffer_size = 1024 * 1024);

/**
 * Copy file with attribute preservation
 */
ErrorCode copy_file_with_attrs(const std::string& src,
                                const std::string& dst,
                                const CopyOptions& opts);

/**
 * Create a symlink copy
 */
ErrorCode copy_symlink(const std::string& src, const std::string& dst);

/**
 * Copy directory structure (without contents)
 */
ErrorCode create_directory_like(const std::string& src, const std::string& dst);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Check if source is newer than destination
 */
bool is_newer(const std::string& src, const std::string& dst);

/**
 * Check if two paths refer to the same file
 */
bool same_file(const std::string& path1, const std::string& path2);

/**
 * Check if dst is inside src (would cause recursive copy into self)
 */
bool is_subpath(const std::string& parent, const std::string& child);

/**
 * Get file size
 */
int64_t get_file_size(const std::string& path);

/**
 * Check if path exists
 */
bool exists(const std::string& path);

/**
 * Check if path is a directory
 */
bool is_directory(const std::string& path);

/**
 * Check if path is a symlink
 */
bool is_symlink(const std::string& path);

/**
 * Get symlink target
 */
std::optional<std::string> read_symlink(const std::string& path);

/**
 * Get canonical (absolute, resolved) path
 */
std::optional<std::string> canonical_path(const std::string& path);

/**
 * Get parent directory
 */
std::string parent_path(const std::string& path);

/**
 * Get filename from path
 */
std::string filename(const std::string& path);

/**
 * Join paths
 */
std::string join_path(const std::string& a, const std::string& b);

/**
 * Format bytes for display
 */
std::string format_bytes(int64_t bytes);

// ============================================================================
// FFI (C API)
// ============================================================================

extern "C" {

typedef int32_t AriaError;

// Copy single file
AriaError aria_cp_file(const char* src, const char* dst);

// Copy with options (JSON string)
AriaError aria_cp_file_opts(const char* src, const char* dst, const char* opts_json);

// Copy multiple files to directory
AriaError aria_cp_files(const char** sources, size_t count, const char* dst_dir);

// Copy directory recursively
AriaError aria_cp_dir(const char* src, const char* dst);

// Get last error message
const char* aria_cp_last_error(void);

}  // extern "C"

}  // namespace acp
}  // namespace aria

#endif // ARIA_ACP_FILE_COPY_HPP
