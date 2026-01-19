/**
 * word_count.cpp
 * Word Count Library Implementation
 */

#include "awc/word_count.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <unistd.h>

namespace aria {
namespace awc {

const char* error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::FILE_NOT_FOUND: return "File not found";
        case ErrorCode::FILE_NOT_READABLE: return "File not readable";
        case ErrorCode::READ_ERROR: return "Read error";
        case ErrorCode::INVALID_UTF8: return "Invalid UTF-8";
        case ErrorCode::SYSTEM_ERROR: return "System error";
        default: return "Unknown error";
    }
}

int64_t count_utf8_chars(const char* data, size_t size) {
    int64_t count = 0;
    for (size_t i = 0; i < size; ) {
        unsigned char c = data[i];
        
        // ASCII (0xxxxxxx)
        if (c < 0x80) {
            i += 1;
        }
        // 2-byte UTF-8 (110xxxxx 10xxxxxx)
        else if ((c & 0xE0) == 0xC0) {
            i += 2;
        }
        // 3-byte UTF-8 (1110xxxx 10xxxxxx 10xxxxxx)
        else if ((c & 0xF0) == 0xE0) {
            i += 3;
        }
        // 4-byte UTF-8 (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        else if ((c & 0xF8) == 0xF0) {
            i += 4;
        }
        // Invalid UTF-8, treat as single byte
        else {
            i += 1;
        }
        
        count++;
    }
    return count;
}

CountResult count_data(const char* data, size_t size, const std::string& filename,
                       const CountOptions& opts) {
    CountResult result;
    result.filename = filename;
    result.bytes = static_cast<int64_t>(size);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    bool in_word = false;
    int64_t current_line_length = 0;
    
    for (size_t i = 0; i < size; i++) {
        char c = data[i];
        
        // Count lines
        if (c == '\n') {
            if (opts.count_lines) {
                result.lines++;
            }
            if (opts.count_max_line_length) {
                if (current_line_length > result.max_line_length) {
                    result.max_line_length = current_line_length;
                }
                current_line_length = 0;
            }
            in_word = false;
        } else {
            current_line_length++;
        }
        
        // Count words
        if (opts.count_words) {
            if (is_whitespace(c)) {
                in_word = false;
            } else if (!in_word) {
                in_word = true;
                result.words++;
            }
        }
    }
    
    // Check final line length
    if (opts.count_max_line_length && current_line_length > result.max_line_length) {
        result.max_line_length = current_line_length;
    }
    
    // Count characters (UTF-8 aware)
    if (opts.count_chars) {
        result.chars = count_utf8_chars(data, size);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    if (result.elapsed_ms > 0.0) {
        result.throughput_mbps = (static_cast<double>(size) / (1024.0 * 1024.0)) / 
                                 (result.elapsed_ms / 1000.0);
    }
    
    return result;
}

CountResult count_file(const std::string& filename, const CountOptions& opts) {
    CountResult result;
    result.filename = filename;
    
    // Open file
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        result.error = ErrorCode::FILE_NOT_FOUND;
        result.error_message = "Cannot open file: " + filename;
        return result;
    }
    
    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Read entire file
    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        result.error = ErrorCode::READ_ERROR;
        result.error_message = "Error reading file: " + filename;
        return result;
    }
    
    return count_data(buffer.data(), size, filename, opts);
}

CountResult count_stdin(const CountOptions& opts) {
    CountResult result;
    result.filename = "";  // stdin has no filename
    
    // Read all data from stdin
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    std::string data = ss.str();
    
    return count_data(data.c_str(), data.size(), "-", opts);
}

TotalResult count_files(const std::vector<std::string>& filenames, 
                        const CountOptions& opts) {
    TotalResult total;
    
    for (const auto& filename : filenames) {
        CountResult result = count_file(filename, opts);
        
        if (result.is_ok()) {
            total.total_lines = tbb_add(total.total_lines, result.lines);
            total.total_words = tbb_add(total.total_words, result.words);
            total.total_chars = tbb_add(total.total_chars, result.chars);
            total.total_bytes = tbb_add(total.total_bytes, result.bytes);
            
            if (result.max_line_length > total.total_max_line_length) {
                total.total_max_line_length = result.max_line_length;
            }
            
            total.files_processed++;
            total.total_elapsed_ms += result.elapsed_ms;
        } else {
            total.files_failed++;
        }
        
        total.file_results.push_back(result);
    }
    
    return total;
}

} // namespace awc
} // namespace aria
