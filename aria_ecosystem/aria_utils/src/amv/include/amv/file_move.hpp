/**
 * file_move.hpp
 * Hex-Stream File Move/Rename Library
 *
 * Implements the amv utility's core functionality:
 * - Atomic rename when possible (same filesystem)
 * - Copy + delete for cross-filesystem moves
 * - Batch moves with progress reporting
 *
 * Architecture follows the Aria Six-Stream Topology:
 * - FD 1 (stdout): Progress and status output
 * - FD 3 (stddbg): Structured JSON telemetry
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_AMV_FILE_MOVE_HPP
#define ARIA_AMV_FILE_MOVE_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace aria {
namespace amv {

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
    MOVE_INTO_SELF = 8,
    CROSS_DEVICE = 9,
    RENAME_FAILED = 10,
    DELETE_FAILED = 11,
    PERMISSION_ERROR = 12,
    SYSTEM_ERROR = 99
};

const char* error_to_string(ErrorCode code);

// ============================================================================
// Move Options
// ============================================================================

struct MoveOptions {
    bool force = false;                  // -f: Force overwrite
    bool interactive = false;            // -i: Prompt before overwrite
    bool no_clobber = false;             // -n: Don't overwrite existing
    bool update = false;                 // -u: Only move if src is newer
    bool verbose = false;                // -v: Verbose output
    bool backup = false;                 // -b: Make backup before overwrite

    std::string backup_suffix = "~";     // Suffix for backup files

    MoveOptions() = default;
};

// ============================================================================
// Move Result
// ============================================================================

struct MoveResult {
    ErrorCode error = ErrorCode::OK;
    std::string error_message;
    std::string error_path;

    int64_t files_moved = 0;             // Number of files moved
    int64_t dirs_moved = 0;              // Number of directories moved
    int64_t bytes_moved = 0;             // Total bytes (for cross-device)
    int64_t files_skipped = 0;           // Files skipped

    bool used_rename = false;            // True if atomic rename was used
    bool used_copy = false;              // True if copy+delete was used

    double elapsed_ms = 0.0;

    bool is_ok() const { return error == ErrorCode::OK; }
};

// ============================================================================
// Progress Callback
// ============================================================================

struct MoveProgress {
    std::string current_file;
    int64_t bytes_moved;
    int64_t bytes_total;
    int64_t files_completed;
    int64_t files_total;
    double percent;
};

using ProgressCallback = std::function<bool(const MoveProgress& progress)>;

// ============================================================================
// Telemetry Writer
// ============================================================================

class TelemetryWriter {
public:
    explicit TelemetryWriter(int fd = FD_STDDBG);

    void log_start(const std::string& src, const std::string& dst);
    void log_rename(const std::string& src, const std::string& dst);
    void log_copy_start(const std::string& src, int64_t size);
    void log_copy_complete(const std::string& src, int64_t bytes, double ms);
    void log_delete(const std::string& path);
    void log_skip(const std::string& path, const std::string& reason);
    void log_backup(const std::string& original, const std::string& backup);
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
 * Move/rename a single file or directory
 *
 * @param src Source path
 * @param dst Destination path (file or directory)
 * @param opts Move options
 * @param progress Optional progress callback
 * @param telemetry Optional telemetry writer
 * @return Move result
 */
MoveResult move_file(const std::string& src,
                     const std::string& dst,
                     const MoveOptions& opts = MoveOptions(),
                     ProgressCallback progress = nullptr,
                     TelemetryWriter* telemetry = nullptr);

/**
 * Move multiple files to a directory
 *
 * @param sources List of source paths
 * @param dst_dir Destination directory
 * @param opts Move options
 * @param progress Optional progress callback
 * @param telemetry Optional telemetry writer
 * @return Move result
 */
MoveResult move_files(const std::vector<std::string>& sources,
                      const std::string& dst_dir,
                      const MoveOptions& opts = MoveOptions(),
                      ProgressCallback progress = nullptr,
                      TelemetryWriter* telemetry = nullptr);

/**
 * Rename a file or directory (same as move_file, explicit name)
 */
MoveResult rename_file(const std::string& old_path,
                       const std::string& new_path,
                       const MoveOptions& opts = MoveOptions());

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Check if two paths are on the same filesystem
 */
bool same_filesystem(const std::string& path1, const std::string& path2);

/**
 * Atomic rename (same filesystem only)
 */
ErrorCode atomic_rename(const std::string& src, const std::string& dst);

/**
 * Cross-device move (copy + delete)
 */
MoveResult cross_device_move(const std::string& src,
                              const std::string& dst,
                              const MoveOptions& opts,
                              ProgressCallback progress = nullptr,
                              TelemetryWriter* telemetry = nullptr);

/**
 * Create backup of existing file
 */
ErrorCode create_backup(const std::string& path, const std::string& suffix);

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
 * Check if src is newer than dst
 */
bool is_newer(const std::string& src, const std::string& dst);

/**
 * Check if two paths refer to the same file
 */
bool same_file(const std::string& path1, const std::string& path2);

/**
 * Check if child is inside parent directory
 */
bool is_subpath(const std::string& parent, const std::string& child);

/**
 * Get parent directory
 */
std::string parent_path(const std::string& path);

/**
 * Get filename
 */
std::string filename(const std::string& path);

/**
 * Join paths
 */
std::string join_path(const std::string& a, const std::string& b);

/**
 * Get file size
 */
int64_t get_file_size(const std::string& path);

/**
 * Delete file or empty directory
 */
ErrorCode delete_file(const std::string& path);

/**
 * Delete directory recursively
 */
ErrorCode delete_directory(const std::string& path);

// ============================================================================
// FFI (C API)
// ============================================================================

extern "C" {

typedef int32_t AriaError;

// Move/rename single file
AriaError aria_mv_file(const char* src, const char* dst);

// Move with options (JSON string)
AriaError aria_mv_file_opts(const char* src, const char* dst, const char* opts_json);

// Move multiple files to directory
AriaError aria_mv_files(const char** sources, size_t count, const char* dst_dir);

// Simple rename
AriaError aria_mv_rename(const char* old_path, const char* new_path);

// Get last error message
const char* aria_mv_last_error(void);

}  // extern "C"

}  // namespace amv
}  // namespace aria

#endif // ARIA_AMV_FILE_MOVE_HPP
