/**
 * file_cat.hpp
 * Hex-Stream File Concatenation Library
 *
 * Implements the acat utility's core functionality:
 * - Zero-copy file reading with splice()
 * - Binary output to stddato (FD 5)
 * - Streaming with configurable buffer sizes
 * - Line numbering and byte counting
 *
 * Architecture follows the Aria Six-Stream Topology:
 * - FD 1 (stdout): Human-readable text output
 * - FD 3 (stddbg): Structured JSON telemetry
 * - FD 5 (stddato): Raw binary data output
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ACAT_FILE_CAT_HPP
#define ARIA_ACAT_FILE_CAT_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace aria {
namespace acat {

// ============================================================================
// TBB Sentinel (Sticky Error)
// ============================================================================

constexpr int64_t TBB64_ERR = INT64_MIN;

inline bool is_tbb_err(int64_t v) { return v == TBB64_ERR; }

// Safe TBB addition
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
constexpr int FD_STDDATI = 4;
constexpr int FD_STDDATO = 5;

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    OK = 0,
    FILE_NOT_FOUND = 1,
    PERMISSION_DENIED = 2,
    IS_DIRECTORY = 3,
    READ_ERROR = 4,
    WRITE_ERROR = 5,
    MEMORY_ERROR = 6,
    INVALID_ARGUMENT = 7,
    PIPE_ERROR = 8,
    SYSTEM_ERROR = 99
};

// Get error description
const char* error_to_string(ErrorCode code);

// ============================================================================
// Cat Options
// ============================================================================

struct CatOptions {
    bool show_line_numbers = false;      // -n: Number all output lines
    bool show_nonblank_numbers = false;  // -b: Number non-blank lines only
    bool show_ends = false;              // -E: Display $ at end of lines
    bool show_tabs = false;              // -T: Display TAB as ^I
    bool show_nonprinting = false;       // -v: Display non-printing chars
    bool squeeze_blank = false;          // -s: Squeeze multiple blank lines

    int64_t byte_offset = 0;             // Start reading from offset
    int64_t byte_limit = -1;             // Max bytes to read (-1 = all)
    int64_t line_offset = 0;             // Skip first N lines
    int64_t line_limit = -1;             // Max lines to output (-1 = all)

    size_t buffer_size = 65536;          // I/O buffer size (64KB default)
    bool use_zero_copy = true;           // Use splice() when possible
    bool binary_mode = false;            // Output to stddato (FD 5)

    CatOptions() = default;
};

// ============================================================================
// Cat Result
// ============================================================================

struct CatResult {
    ErrorCode error = ErrorCode::OK;
    std::string error_message;

    int64_t bytes_read = 0;              // Total bytes read (TBB-safe)
    int64_t bytes_written = 0;           // Total bytes written
    int64_t lines_read = 0;              // Total lines processed
    int64_t files_processed = 0;         // Number of files processed

    double elapsed_ms = 0.0;             // Total time in milliseconds
    double throughput_mbps = 0.0;        // Throughput in MB/s

    bool is_ok() const { return error == ErrorCode::OK; }
};

// ============================================================================
// Data Callback
// ============================================================================

// Called for each chunk of data read
// Return false to abort, true to continue
using DataCallback = std::function<bool(const uint8_t* data, size_t size)>;

// Called for each line (if line processing enabled)
// Return false to abort, true to continue
using LineCallback = std::function<bool(const std::string& line, int64_t line_num)>;

// ============================================================================
// Telemetry Writer
// ============================================================================

class TelemetryWriter {
public:
    explicit TelemetryWriter(int fd = FD_STDDBG);

    void log_start(const std::string& path, const CatOptions& opts);
    void log_progress(const std::string& path, int64_t bytes, int64_t total);
    void log_complete(const std::string& path, int64_t bytes, double elapsed_ms);
    void log_error(const std::string& path, ErrorCode code, const std::string& message);

private:
    int fd_;
    void write_json(const std::string& json);
};

// ============================================================================
// Main API
// ============================================================================

/**
 * Read a single file and write to output
 *
 * @param path File path (or "-" for stdin)
 * @param out_fd Output file descriptor
 * @param opts Cat options
 * @param telemetry Optional telemetry writer
 * @return Result with stats
 */
CatResult cat_file(const std::string& path,
                   int out_fd = FD_STDOUT,
                   const CatOptions& opts = CatOptions(),
                   TelemetryWriter* telemetry = nullptr);

/**
 * Read multiple files and concatenate to output
 *
 * @param paths List of file paths
 * @param out_fd Output file descriptor
 * @param opts Cat options
 * @param telemetry Optional telemetry writer
 * @return Result with aggregate stats
 */
CatResult cat_files(const std::vector<std::string>& paths,
                    int out_fd = FD_STDOUT,
                    const CatOptions& opts = CatOptions(),
                    TelemetryWriter* telemetry = nullptr);

/**
 * Read file with data callback
 *
 * @param path File path
 * @param callback Called for each chunk
 * @param opts Cat options
 * @return Result with stats
 */
CatResult cat_file_callback(const std::string& path,
                             DataCallback callback,
                             const CatOptions& opts = CatOptions());

/**
 * Read file with line callback
 *
 * @param path File path
 * @param callback Called for each line
 * @param opts Cat options
 * @return Result with stats
 */
CatResult cat_file_lines(const std::string& path,
                          LineCallback callback,
                          const CatOptions& opts = CatOptions());

/**
 * Read file to string (for small files)
 *
 * @param path File path
 * @param max_size Maximum size to read (default 16MB)
 * @return File contents or empty on error
 */
std::optional<std::string> read_file_string(const std::string& path,
                                             size_t max_size = 16 * 1024 * 1024);

/**
 * Read file to byte vector
 *
 * @param path File path
 * @param max_size Maximum size to read
 * @return File contents or empty on error
 */
std::optional<std::vector<uint8_t>> read_file_bytes(const std::string& path,
                                                     size_t max_size = 0);

// ============================================================================
// Zero-Copy Operations
// ============================================================================

/**
 * Zero-copy transfer using splice()
 *
 * @param in_fd Input file descriptor
 * @param out_fd Output file descriptor (must be pipe or socket)
 * @param count Bytes to transfer (0 = until EOF)
 * @return Bytes transferred or TBB64_ERR on error
 */
int64_t splice_transfer(int in_fd, int out_fd, size_t count = 0);

/**
 * Check if zero-copy is available for the given FDs
 */
bool can_zero_copy(int in_fd, int out_fd);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get file size
 *
 * @param path File path
 * @return Size in bytes or TBB64_ERR on error
 */
int64_t get_file_size(const std::string& path);

/**
 * Check if path is a regular file
 */
bool is_regular_file(const std::string& path);

/**
 * Check if path is readable
 */
bool is_readable(const std::string& path);

/**
 * Format bytes for display
 */
std::string format_bytes(int64_t bytes);

/**
 * Make character printable (for -v option)
 */
std::string make_printable(char c);

// ============================================================================
// FFI (C API)
// ============================================================================

extern "C" {

typedef int32_t AriaError;

// Simple cat to stdout
AriaError aria_cat_file(const char* path);

// Cat to specific FD
AriaError aria_cat_file_fd(const char* path, int out_fd);

// Cat multiple files
AriaError aria_cat_files(const char** paths, size_t count, int out_fd);

// Cat with options (JSON string)
AriaError aria_cat_file_opts(const char* path, int out_fd, const char* opts_json);

// Read file to buffer (caller must free with aria_free)
AriaError aria_read_file(const char* path, uint8_t** out_data, size_t* out_size);

// Free buffer allocated by aria_read_file
void aria_cat_free(void* ptr);

// Get last error message
const char* aria_cat_last_error(void);

}  // extern "C"

}  // namespace acat
}  // namespace aria

#endif // ARIA_ACAT_FILE_CAT_HPP
