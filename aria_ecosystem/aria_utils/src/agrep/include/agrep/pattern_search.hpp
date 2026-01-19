#ifndef ARIA_UTILS_AGREP_PATTERN_SEARCH_HPP
#define ARIA_UTILS_AGREP_PATTERN_SEARCH_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

// ============================================================================
// AGREP - Pattern Search with Six-Stream Topology
// ============================================================================
// Purpose: Search for text patterns in files and emit both human-readable
//          highlighted matches (stdout) and machine-readable binary Match
//          structures (stddato FD 5) for pipeline composition.
//
// Architecture:
//   - Control Plane (FD 0-2): stdin, stdout, stderr for UI/errors
//   - Observability Plane (FD 3): stddbg for telemetry (JSON logs)
//   - Data Plane (FD 4-5): stddati/stddato for binary Match objects
//
// Search Algorithm:
//   - Simple substring search (Boyer-Moore-Horspool for future optimization)
//   - Case-sensitive and case-insensitive modes
//   - Line-based matching with context lines (before/after)
//   - Binary file detection and skipping
//
// TBB Integration:
//   - File offsets use tbb64 with sticky ERR sentinel
//   - Line numbers use tbb32 to handle massive files
//   - Match counts use tbb64 for overflow safety
// ============================================================================

namespace aria {
namespace utils {
namespace agrep {

// ============================================================================
// Twisted Balanced Binary (TBB) Type Aliases
// ============================================================================
using tbb64 = int64_t;
using tbb32 = int32_t;

// TBB Error Sentinels
constexpr tbb64 TBB64_ERR = INT64_MIN;
constexpr tbb32 TBB32_ERR = INT32_MIN;

// ============================================================================
// Error Codes
// ============================================================================
enum class ErrorCode : int32_t {
    SUCCESS            = 0,
    FILE_NOT_FOUND     = 1,
    PERMISSION_DENIED  = 2,
    IO_ERROR           = 3,
    INVALID_PATTERN    = 4,
    BINARY_FILE        = 5,
    PATTERN_TOO_LONG   = 6,
    UNKNOWN_ERROR      = 99
};

// ============================================================================
// Match Structure (Binary Protocol)
// ============================================================================
// This structure represents a single pattern match found in a file.
// It's designed for binary serialization to stddato (FD 5).
//
// Binary Protocol Layout:
//   Offset | Field        | Size | Type
//   -------|--------------|------|-----
//   0      | magic        | 1    | uint8 (0xA1)
//   1      | file_offset  | 8    | tbb64
//   9      | line_number  | 4    | tbb32
//   13     | column       | 4    | int32
//   17     | match_len    | 4    | int32
//   21     | context_len  | 4    | int32
//   25     | content      | N    | UTF-8 bytes (full line)
// ============================================================================
struct Match {
    // Magic byte for stream sync (0xA1 = 161)
    static constexpr uint8_t MAGIC = 0xA1;
    
    // Absolute byte offset in file (TBB64 - sticky error on overflow)
    tbb64 file_offset;
    
    // Line number where match occurs (1-based)
    tbb32 line_number;
    
    // Column where match starts (0-based)
    int32_t column;
    
    // Length of matched pattern in bytes
    int32_t match_len;
    
    // Length of content (full line)
    int32_t content_len;
    
    // The actual matched line content (not serialized in struct)
    std::string content;
    
    // Helpers for TBB validation
    [[nodiscard]] bool has_valid_offset() const noexcept {
        return file_offset != TBB64_ERR;
    }
    
    [[nodiscard]] bool has_valid_line() const noexcept {
        return line_number != TBB32_ERR;
    }
};

// ============================================================================
// Search Options
// ============================================================================
struct SearchOptions {
    bool case_insensitive = false;   // Ignore case when matching
    bool line_numbers = true;        // Include line numbers in output
    bool color_output = true;        // Use ANSI colors for highlighting
    bool invert_match = false;       // Select non-matching lines
    bool count_only = false;         // Only output count of matches
    bool whole_word = false;         // Match whole words only
    bool binary_skip = true;         // Skip binary files automatically
    size_t context_before = 0;       // Lines of context before match
    size_t context_after = 0;        // Lines of context after match
    size_t max_matches = 0;          // Stop after N matches (0 = unlimited)
    
    // Buffer size for file reading (64KB default)
    size_t buffer_size = 65536;
};

// ============================================================================
// Search Statistics (Telemetry)
// ============================================================================
struct SearchStats {
    tbb64 bytes_scanned = 0;      // Total bytes read
    tbb64 matches_found = 0;      // Total matches
    tbb32 files_searched = 0;     // Number of files searched
    tbb32 files_skipped = 0;      // Binary or error files skipped
    int64_t duration_ms = 0;      // Search duration in milliseconds
    
    void reset() {
        bytes_scanned = 0;
        matches_found = 0;
        files_searched = 0;
        files_skipped = 0;
        duration_ms = 0;
    }
};

// ============================================================================
// Result Type
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
// Core Search API
// ============================================================================

// Search a single file for pattern matches
// Returns number of matches found, or negative error code
// Calls match_callback for each match found
Result<tbb64> search_file(
    const std::string& file_path,
    const std::string& pattern,
    const SearchOptions& options,
    std::function<void(const Match&)> match_callback
) noexcept;

// Search multiple files for pattern
// Returns aggregate statistics
Result<SearchStats> search_files(
    const std::vector<std::string>& file_paths,
    const std::string& pattern,
    const SearchOptions& options,
    std::function<void(const Match&, const std::string& file)> match_callback
) noexcept;

// Detect if file appears to be binary (contains null bytes in first 8KB)
bool is_binary_file(const std::string& file_path) noexcept;

// Convert ErrorCode to human-readable string
std::string error_to_string(ErrorCode ec) noexcept;

// ============================================================================
// Output Formatting
// ============================================================================

// Format match for human-readable output (stdout)
// Includes ANSI color highlighting if options.color_output is true
std::string format_match_ui(
    const Match& match,
    const std::string& pattern,
    const std::string& file_path,
    const SearchOptions& options
) noexcept;

// Format match statistics for summary output
std::string format_stats(const SearchStats& stats) noexcept;

// ============================================================================
// Six-Stream I/O
// ============================================================================

// Write Match to stddato (FD 5) in binary format
// Returns bytes written or negative error code
int64_t write_match_to_stddato(const Match& match) noexcept;

// Write match to stdout (FD 1) with highlighting
void write_match_to_stdout(
    const Match& match,
    const std::string& pattern,
    const std::string& file_path,
    const SearchOptions& options
) noexcept;

// Write telemetry event to stddbg (FD 3)
// Format: JSON structured log
void write_telemetry(
    const std::string& event_type,
    const std::string& message
) noexcept;

// Write search statistics to stddbg
void write_stats_telemetry(const SearchStats& stats) noexcept;

// ============================================================================
// Pattern Matching Helpers
// ============================================================================

// Simple case-sensitive substring search
// Returns position of match or -1 if not found
int32_t find_pattern(
    const char* text,
    size_t text_len,
    const char* pattern,
    size_t pattern_len
) noexcept;

// Case-insensitive substring search
int32_t find_pattern_icase(
    const char* text,
    size_t text_len,
    const char* pattern,
    size_t pattern_len
) noexcept;

// Check if match is at word boundary (for --whole-word option)
bool is_word_boundary(
    const char* text,
    size_t text_len,
    size_t position
) noexcept;

// ============================================================================
// FFI C API (For Aria Integration)
// ============================================================================
extern "C" {
    // Search file for pattern
    // Returns match count or negative error code
    int64_t aria_grep_file(
        const char* file_path,
        const char* pattern,
        bool case_insensitive,
        void (*match_callback)(const Match*)
    );
    
    // Check if file is binary
    // Returns 1 if binary, 0 if text, -1 on error
    int32_t aria_is_binary_file(const char* file_path);
    
    // Get error string
    const char* aria_grep_error_string(int32_t error_code);
    
    // Format match for display
    // buf_size should be at least 256 bytes
    int32_t aria_format_match(
        const Match* match,
        const char* pattern,
        char* buf,
        size_t buf_size
    );
}

} // namespace agrep
} // namespace utils
} // namespace aria

#endif // ARIA_UTILS_AGREP_PATTERN_SEARCH_HPP
