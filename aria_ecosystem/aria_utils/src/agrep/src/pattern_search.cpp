#include "agrep/pattern_search.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <unistd.h>
#include <chrono>

namespace aria {
namespace utils {
namespace agrep {

// ============================================================================
// Pattern Matching Implementation
// ============================================================================

int32_t find_pattern(
    const char* text,
    size_t text_len,
    const char* pattern,
    size_t pattern_len
) noexcept {
    if (pattern_len == 0 || pattern_len > text_len) {
        return -1;
    }
    
    // Simple substring search (could be optimized with Boyer-Moore)
    for (size_t i = 0; i <= text_len - pattern_len; ++i) {
        if (std::memcmp(text + i, pattern, pattern_len) == 0) {
            return static_cast<int32_t>(i);
        }
    }
    
    return -1;
}

int32_t find_pattern_icase(
    const char* text,
    size_t text_len,
    const char* pattern,
    size_t pattern_len
) noexcept {
    if (pattern_len == 0 || pattern_len > text_len) {
        return -1;
    }
    
    // Case-insensitive search
    for (size_t i = 0; i <= text_len - pattern_len; ++i) {
        bool match = true;
        for (size_t j = 0; j < pattern_len; ++j) {
            if (std::tolower(static_cast<unsigned char>(text[i + j])) !=
                std::tolower(static_cast<unsigned char>(pattern[j]))) {
                match = false;
                break;
            }
        }
        if (match) {
            return static_cast<int32_t>(i);
        }
    }
    
    return -1;
}

bool is_word_boundary(
    const char* text,
    size_t text_len,
    size_t position
) noexcept {
    auto is_word_char = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };
    
    // Position should NOT be in the middle of a word
    // Before should be boundary (start or non-word)
    // After should be boundary (end or non-word)
    
    bool before_ok = (position == 0) || !is_word_char(text[position - 1]);
    bool after_ok = (position >= text_len) || !is_word_char(text[position]);
    
    return before_ok && after_ok;
}

// Check if a match at [start, start+len) is at word boundaries
bool is_whole_word_match(
    const char* text,
    size_t text_len,
    size_t match_start,
    size_t match_len
) noexcept {
    auto is_word_char = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };
    
    // Check before match
    bool before_ok = (match_start == 0) || 
                     !is_word_char(text[match_start - 1]);
    
    // Check after match
    size_t match_end = match_start + match_len;
    bool after_ok = (match_end >= text_len) || 
                    !is_word_char(text[match_end]);
    
    return before_ok && after_ok;
}

// ============================================================================
// Binary File Detection
// ============================================================================

bool is_binary_file(const std::string& file_path) noexcept {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return false;  // Can't determine, assume text
    }
    
    // Read first 8KB
    char buffer[8192];
    file.read(buffer, sizeof(buffer));
    std::streamsize bytes_read = file.gcount();
    
    // Check for null bytes (strong indicator of binary)
    for (std::streamsize i = 0; i < bytes_read; ++i) {
        if (buffer[i] == '\0') {
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// Error Handling
// ============================================================================

std::string error_to_string(ErrorCode ec) noexcept {
    switch (ec) {
        case ErrorCode::SUCCESS:           return "Success";
        case ErrorCode::FILE_NOT_FOUND:    return "File not found";
        case ErrorCode::PERMISSION_DENIED: return "Permission denied";
        case ErrorCode::IO_ERROR:          return "I/O error";
        case ErrorCode::INVALID_PATTERN:   return "Invalid pattern";
        case ErrorCode::BINARY_FILE:       return "Binary file skipped";
        case ErrorCode::PATTERN_TOO_LONG:  return "Pattern too long";
        case ErrorCode::UNKNOWN_ERROR:     return "Unknown error";
        default:                           return "Unrecognized error";
    }
}

// ============================================================================
// Core Search Implementation
// ============================================================================

Result<tbb64> search_file(
    const std::string& file_path,
    const std::string& pattern,
    const SearchOptions& options,
    std::function<void(const Match&)> match_callback
) noexcept {
    // Validate pattern
    if (pattern.empty()) {
        return Result<tbb64>::failure(ErrorCode::INVALID_PATTERN);
    }
    
    if (pattern.length() > 1024) {
        return Result<tbb64>::failure(ErrorCode::PATTERN_TOO_LONG);
    }
    
    // Check for binary file
    if (options.binary_skip && is_binary_file(file_path)) {
        write_telemetry("skip", "Binary file detected: " + file_path);
        return Result<tbb64>::failure(ErrorCode::BINARY_FILE);
    }
    
    // Open file
    std::ifstream file(file_path);
    if (!file) {
        return Result<tbb64>::failure(ErrorCode::FILE_NOT_FOUND);
    }
    
    tbb64 match_count = 0;
    tbb64 byte_offset = 0;
    tbb32 line_number = 1;
    std::string line;
    
    // Line-by-line search
    while (std::getline(file, line)) {
        size_t line_len = line.length();
        
        // Search for pattern in line
        int32_t pos;
        if (options.case_insensitive) {
            pos = find_pattern_icase(line.c_str(), line_len,
                                    pattern.c_str(), pattern.length());
        } else {
            pos = find_pattern(line.c_str(), line_len,
                              pattern.c_str(), pattern.length());
        }
        
        bool found = (pos >= 0);
        
        // Check whole-word constraint
        if (found && options.whole_word) {
            if (!is_whole_word_match(line.c_str(), line_len, pos, pattern.length())) {
                found = false;
            }
        }
        
        // Apply invert-match logic
        if (options.invert_match) {
            found = !found;
        }
        
        if (found) {
            // Create Match structure
            Match match;
            match.file_offset = byte_offset;
            match.line_number = line_number;
            match.column = (pos >= 0) ? pos : 0;
            match.match_len = static_cast<int32_t>(pattern.length());
            match.content_len = static_cast<int32_t>(line_len);
            match.content = line;
            
            // Callback to process match
            if (match_callback) {
                match_callback(match);
            }
            
            match_count++;
            
            // Check max matches limit
            if (options.max_matches > 0 && 
                match_count >= static_cast<tbb64>(options.max_matches)) {
                break;
            }
        }
        
        // Update position tracking
        byte_offset += static_cast<tbb64>(line_len + 1);  // +1 for newline
        line_number++;
        
        // Check for TBB overflow
        if (byte_offset == TBB64_ERR || line_number == TBB32_ERR) {
            write_telemetry("error", "Offset overflow in file: " + file_path);
            break;
        }
    }
    
    return Result<tbb64>::success(match_count);
}

Result<SearchStats> search_files(
    const std::vector<std::string>& file_paths,
    const std::string& pattern,
    const SearchOptions& options,
    std::function<void(const Match&, const std::string& file)> match_callback
) noexcept {
    SearchStats stats;
    auto start_time = std::chrono::steady_clock::now();
    
    for (const auto& file_path : file_paths) {
        auto result = search_file(
            file_path,
            pattern,
            options,
            [&](const Match& match) {
                if (match_callback) {
                    match_callback(match, file_path);
                }
            }
        );
        
        if (result.ok()) {
            stats.files_searched++;
            stats.matches_found += result.value;
        } else {
            stats.files_skipped++;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    stats.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time
    ).count();
    
    return Result<SearchStats>::success(stats);
}

// ============================================================================
// Formatting Functions
// ============================================================================

std::string format_match_ui(
    const Match& match,
    const std::string& pattern,
    const std::string& file_path,
    const SearchOptions& options
) noexcept {
    std::ostringstream oss;
    
    // ANSI color codes
    const char* color_file = "\033[35m";     // Magenta for filename
    const char* color_line = "\033[32m";     // Green for line number
    const char* color_match = "\033[1;31m";  // Bold red for match
    const char* color_reset = "\033[0m";
    
    if (!options.color_output) {
        color_file = color_line = color_match = color_reset = "";
    }
    
    // Format: filename:line:content with highlighted match
    if (!file_path.empty()) {
        oss << color_file << file_path << color_reset << ":";
    }
    
    if (options.line_numbers) {
        oss << color_line << match.line_number << color_reset << ":";
    }
    
    // Highlight the match in the content
    std::string content = match.content;
    if (options.color_output && match.column >= 0) {
        content.insert(match.column + match.match_len, color_reset);
        content.insert(match.column, color_match);
    }
    
    oss << content;
    
    return oss.str();
}

std::string format_stats(const SearchStats& stats) noexcept {
    std::ostringstream oss;
    oss << "Files searched: " << stats.files_searched << "\n"
        << "Files skipped: " << stats.files_skipped << "\n"
        << "Matches found: " << stats.matches_found << "\n"
        << "Duration: " << stats.duration_ms << " ms";
    return oss.str();
}

// ============================================================================
// Six-Stream I/O
// ============================================================================

int64_t write_match_to_stddato(const Match& match) noexcept {
    constexpr int FD_STDDATO = 5;
    
    int64_t total = 0;
    
    // Write magic byte
    uint8_t magic = Match::MAGIC;
    total += write(FD_STDDATO, &magic, sizeof(magic));
    
    // Write fixed fields
    total += write(FD_STDDATO, &match.file_offset, sizeof(match.file_offset));
    total += write(FD_STDDATO, &match.line_number, sizeof(match.line_number));
    total += write(FD_STDDATO, &match.column, sizeof(match.column));
    total += write(FD_STDDATO, &match.match_len, sizeof(match.match_len));
    total += write(FD_STDDATO, &match.content_len, sizeof(match.content_len));
    
    // Write content
    total += write(FD_STDDATO, match.content.c_str(), match.content.length());
    
    return total;
}

void write_match_to_stdout(
    const Match& match,
    const std::string& pattern,
    const std::string& file_path,
    const SearchOptions& options
) noexcept {
    std::string formatted = format_match_ui(match, pattern, file_path, options);
    std::cout << formatted << "\n";
}

void write_telemetry(
    const std::string& event_type,
    const std::string& message
) noexcept {
    constexpr int FD_STDDBG = 3;
    
    // Format as JSON
    std::ostringstream oss;
    oss << "{\"event\":\"" << event_type << "\","
        << "\"tool\":\"agrep\","
        << "\"message\":\"" << message << "\","
        << "\"timestamp\":" << std::time(nullptr) << "}\n";
    
    std::string log = oss.str();
    write(FD_STDDBG, log.c_str(), log.length());
}

void write_stats_telemetry(const SearchStats& stats) noexcept {
    constexpr int FD_STDDBG = 3;
    
    std::ostringstream oss;
    oss << "{\"event\":\"summary\","
        << "\"files_searched\":" << stats.files_searched << ","
        << "\"files_skipped\":" << stats.files_skipped << ","
        << "\"matches\":" << stats.matches_found << ","
        << "\"bytes_scanned\":" << stats.bytes_scanned << ","
        << "\"duration_ms\":" << stats.duration_ms << "}\n";
    
    std::string log = oss.str();
    write(FD_STDDBG, log.c_str(), log.length());
}

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {

int64_t aria_grep_file(
    const char* file_path,
    const char* pattern,
    bool case_insensitive,
    void (*match_callback)(const Match*)
) {
    if (!file_path || !pattern) {
        return -static_cast<int64_t>(ErrorCode::INVALID_PATTERN);
    }
    
    SearchOptions options;
    options.case_insensitive = case_insensitive;
    
    auto result = search_file(
        file_path,
        pattern,
        options,
        [match_callback](const Match& match) {
            if (match_callback) {
                match_callback(&match);
            }
        }
    );
    
    if (result.err()) {
        return -static_cast<int64_t>(result.error);
    }
    
    return result.value;
}

int32_t aria_is_binary_file(const char* file_path) {
    if (!file_path) {
        return -1;
    }
    
    return is_binary_file(file_path) ? 1 : 0;
}

const char* aria_grep_error_string(int32_t error_code) {
    static std::string error_str;
    error_str = error_to_string(static_cast<ErrorCode>(-error_code));
    return error_str.c_str();
}

int32_t aria_format_match(
    const Match* match,
    const char* pattern,
    char* buf,
    size_t buf_size
) {
    if (!match || !pattern || !buf || buf_size == 0) {
        return -1;
    }
    
    SearchOptions options;
    std::string formatted = format_match_ui(*match, pattern, "", options);
    
    size_t len = std::min(formatted.length(), buf_size - 1);
    std::memcpy(buf, formatted.c_str(), len);
    buf[len] = '\0';
    
    return static_cast<int32_t>(len);
}

} // extern "C"

} // namespace agrep
} // namespace utils
} // namespace aria
