/**
 * word_count.hpp
 * Hex-Stream Word Count Library
 *
 * Implements the awc utility's core functionality:
 * - Count lines, words, characters, and bytes
 * - Multiple file support
 * - UTF-8 aware character counting
 * - Six-stream telemetry
 *
 * Architecture follows the Aria Six-Stream Topology:
 * - FD 1 (stdout): Count results
 * - FD 3 (stddbg): Structured JSON telemetry
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_AWC_WORD_COUNT_HPP
#define ARIA_AWC_WORD_COUNT_HPP

#include <string>
#include <vector>
#include <cstdint>

namespace aria {
namespace awc {

// ==============================================================================
// TBB Sentinel (Sticky Error)
// ==============================================================================

constexpr int64_t TBB64_ERR = INT64_MIN;

inline bool is_tbb_err(int64_t v) { return v == TBB64_ERR; }

inline int64_t tbb_add(int64_t a, int64_t b) {
    if (is_tbb_err(a) || is_tbb_err(b)) return TBB64_ERR;
    if (b > 0 && a > INT64_MAX - b) return TBB64_ERR;
    if (b < 0 && a < INT64_MIN - b) return TBB64_ERR;
    return a + b;
}

// ==============================================================================
// Hex-Stream File Descriptors
// ==============================================================================

constexpr int FD_STDIN   = 0;
constexpr int FD_STDOUT  = 1;
constexpr int FD_STDERR  = 2;
constexpr int FD_STDDBG  = 3;

// ==============================================================================
// Error Codes
// ==============================================================================

enum class ErrorCode {
    OK = 0,
    FILE_NOT_FOUND = 1,
    FILE_NOT_READABLE = 2,
    READ_ERROR = 3,
    INVALID_UTF8 = 4,
    SYSTEM_ERROR = 99
};

const char* error_to_string(ErrorCode code);

// ==============================================================================
// Count Options
// ==============================================================================

struct CountOptions {
    bool count_lines = true;       // -l: Count lines
    bool count_words = true;       // -w: Count words
    bool count_chars = false;      // -m: Count characters (UTF-8 aware)
    bool count_bytes = true;       // -c: Count bytes
    bool count_max_line_length = false;  // -L: Max line length
};

// ==============================================================================
// Count Result
// ==============================================================================

struct CountResult {
    ErrorCode error = ErrorCode::OK;
    std::string error_message;
    std::string filename;

    int64_t lines = 0;
    int64_t words = 0;
    int64_t chars = 0;
    int64_t bytes = 0;
    int64_t max_line_length = 0;

    double elapsed_ms = 0.0;
    double throughput_mbps = 0.0;

    bool is_ok() const { return error == ErrorCode::OK; }
};

// ==============================================================================
// Total Result (for multiple files)
// ==============================================================================

struct TotalResult {
    std::vector<CountResult> file_results;
    
    int64_t total_lines = 0;
    int64_t total_words = 0;
    int64_t total_chars = 0;
    int64_t total_bytes = 0;
    int64_t total_max_line_length = 0;
    
    int64_t files_processed = 0;
    int64_t files_failed = 0;
    
    double total_elapsed_ms = 0.0;
};

// ==============================================================================
// Core Functions
// ==============================================================================

/**
 * Count lines, words, characters, and bytes in a file
 */
CountResult count_file(const std::string& filename,
                       const CountOptions& opts = CountOptions());

/**
 * Count from stdin
 */
CountResult count_stdin(const CountOptions& opts = CountOptions());

/**
 * Count multiple files
 */
TotalResult count_files(const std::vector<std::string>& filenames,
                        const CountOptions& opts = CountOptions());

// ==============================================================================
// Utility Functions
// ==============================================================================

/**
 * Count UTF-8 characters in a buffer
 */
int64_t count_utf8_chars(const char* data, size_t size);

/**
 * Check if a byte is whitespace
 */
inline bool is_whitespace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

} // namespace awc
} // namespace aria

#endif // ARIA_AWC_WORD_COUNT_HPP
