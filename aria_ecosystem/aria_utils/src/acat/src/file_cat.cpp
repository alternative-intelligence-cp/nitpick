/**
 * file_cat.cpp
 * Implementation of file concatenation library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acat/file_cat.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

namespace aria {
namespace acat {

// ============================================================================
// Error Handling
// ============================================================================

const char* error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::FILE_NOT_FOUND: return "File not found";
        case ErrorCode::PERMISSION_DENIED: return "Permission denied";
        case ErrorCode::IS_DIRECTORY: return "Is a directory";
        case ErrorCode::READ_ERROR: return "Read error";
        case ErrorCode::WRITE_ERROR: return "Write error";
        case ErrorCode::MEMORY_ERROR: return "Memory allocation error";
        case ErrorCode::INVALID_ARGUMENT: return "Invalid argument";
        case ErrorCode::PIPE_ERROR: return "Pipe error";
        case ErrorCode::SYSTEM_ERROR: return "System error";
        default: return "Unknown error";
    }
}

static ErrorCode errno_to_error(int err) {
    switch (err) {
        case ENOENT: return ErrorCode::FILE_NOT_FOUND;
        case EACCES:
        case EPERM: return ErrorCode::PERMISSION_DENIED;
        case EISDIR: return ErrorCode::IS_DIRECTORY;
        case ENOMEM: return ErrorCode::MEMORY_ERROR;
        case EINVAL: return ErrorCode::INVALID_ARGUMENT;
        case EPIPE: return ErrorCode::PIPE_ERROR;
        default: return ErrorCode::SYSTEM_ERROR;
    }
}

// ============================================================================
// Telemetry Writer
// ============================================================================

TelemetryWriter::TelemetryWriter(int fd) : fd_(fd) {}

void TelemetryWriter::write_json(const std::string& json) {
    if (fd_ < 0) return;
    std::string line = json + "\n";
    ssize_t written = write(fd_, line.c_str(), line.size());
    (void)written;
}

void TelemetryWriter::log_start(const std::string& path, const CatOptions& opts) {
    std::ostringstream oss;
    oss << "{\"event\":\"cat_start\",\"path\":\"" << path << "\""
        << ",\"buffer_size\":" << opts.buffer_size
        << ",\"zero_copy\":" << (opts.use_zero_copy ? "true" : "false")
        << ",\"binary_mode\":" << (opts.binary_mode ? "true" : "false")
        << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_progress(const std::string& path, int64_t bytes, int64_t total) {
    std::ostringstream oss;
    oss << "{\"event\":\"progress\",\"path\":\"" << path << "\""
        << ",\"bytes\":" << bytes
        << ",\"total\":" << total
        << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_complete(const std::string& path, int64_t bytes, double elapsed_ms) {
    std::ostringstream oss;
    oss << "{\"event\":\"complete\",\"path\":\"" << path << "\""
        << ",\"bytes\":" << bytes
        << ",\"elapsed_ms\":" << elapsed_ms
        << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_error(const std::string& path, ErrorCode code, const std::string& message) {
    std::ostringstream oss;
    oss << "{\"event\":\"error\",\"path\":\"" << path << "\""
        << ",\"code\":" << static_cast<int>(code)
        << ",\"message\":\"" << message << "\""
        << "}";
    write_json(oss.str());
}

// ============================================================================
// Utility Functions
// ============================================================================

int64_t get_file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return TBB64_ERR;
    }
    return st.st_size;
}

bool is_regular_file(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

bool is_readable(const std::string& path) {
    return access(path.c_str(), R_OK) == 0;
}

std::string format_bytes(int64_t bytes) {
    if (is_tbb_err(bytes)) return "ERR";
    if (bytes < 0) return "???";

    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }

    std::ostringstream oss;
    if (unit == 0) {
        oss << bytes << " B";
    } else {
        oss << std::fixed;
        oss.precision(2);
        oss << size << " " << units[unit];
    }
    return oss.str();
}

std::string make_printable(char c) {
    if (c >= 32 && c < 127) {
        return std::string(1, c);
    } else if (c == '\t') {
        return "^I";
    } else if (c == '\n') {
        return std::string(1, c);
    } else if (c < 32) {
        return std::string("^") + static_cast<char>(c + 64);
    } else if (c == 127) {
        return "^?";
    } else {
        // Meta characters
        std::ostringstream oss;
        oss << "M-";
        if ((c & 0x7f) < 32) {
            oss << "^" << static_cast<char>((c & 0x7f) + 64);
        } else if ((c & 0x7f) == 127) {
            oss << "^?";
        } else {
            oss << static_cast<char>(c & 0x7f);
        }
        return oss.str();
    }
}

bool can_zero_copy(int in_fd, int out_fd) {
#ifdef __linux__
    struct stat in_st, out_st;
    if (fstat(in_fd, &in_st) != 0 || fstat(out_fd, &out_st) != 0) {
        return false;
    }
    // sendfile works with regular files as input
    // and regular files, pipes, or sockets as output
    return S_ISREG(in_st.st_mode);
#else
    (void)in_fd;
    (void)out_fd;
    return false;
#endif
}

int64_t splice_transfer(int in_fd, int out_fd, size_t count) {
#ifdef __linux__
    int64_t total = 0;
    size_t remaining = count == 0 ? SIZE_MAX : count;

    while (remaining > 0) {
        size_t chunk = std::min(remaining, static_cast<size_t>(1024 * 1024));
        ssize_t sent = sendfile(out_fd, in_fd, nullptr, chunk);

        if (sent < 0) {
            if (errno == EINTR) continue;
            return total > 0 ? total : TBB64_ERR;
        }
        if (sent == 0) break;  // EOF

        total = tbb_add(total, sent);
        if (is_tbb_err(total)) return TBB64_ERR;

        if (count > 0) {
            remaining -= sent;
        }
    }
    return total;
#else
    (void)in_fd;
    (void)out_fd;
    (void)count;
    return TBB64_ERR;
#endif
}

// ============================================================================
// Core Implementation
// ============================================================================

static CatResult cat_fd(int in_fd, int out_fd, const CatOptions& opts,
                        const std::string& path, TelemetryWriter* telemetry) {
    CatResult result;
    auto start = std::chrono::steady_clock::now();

    // Try zero-copy first
    if (opts.use_zero_copy && opts.byte_offset == 0 && opts.byte_limit < 0 &&
        !opts.show_line_numbers && !opts.show_nonblank_numbers &&
        !opts.show_ends && !opts.show_tabs && !opts.show_nonprinting &&
        !opts.squeeze_blank && can_zero_copy(in_fd, out_fd)) {

        int64_t transferred = splice_transfer(in_fd, out_fd, 0);
        if (!is_tbb_err(transferred)) {
            result.bytes_read = transferred;
            result.bytes_written = transferred;

            auto end = std::chrono::steady_clock::now();
            result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
            if (result.elapsed_ms > 0) {
                result.throughput_mbps = (transferred / 1024.0 / 1024.0) / (result.elapsed_ms / 1000.0);
            }

            if (telemetry) {
                telemetry->log_complete(path, transferred, result.elapsed_ms);
            }
            return result;
        }
        // Fall through to normal read if zero-copy fails
    }

    // Allocate buffer
    std::vector<uint8_t> buffer(opts.buffer_size);

    // Handle byte offset
    if (opts.byte_offset > 0) {
        if (lseek(in_fd, opts.byte_offset, SEEK_SET) < 0) {
            // If seek fails, read and discard
            int64_t to_skip = opts.byte_offset;
            while (to_skip > 0) {
                size_t chunk = std::min(static_cast<size_t>(to_skip), buffer.size());
                ssize_t n = read(in_fd, buffer.data(), chunk);
                if (n <= 0) break;
                to_skip -= n;
            }
        }
    }

    int64_t bytes_remaining = opts.byte_limit;
    int64_t lines_remaining = opts.line_limit;
    int64_t lines_to_skip = opts.line_offset;
    int64_t current_line = 1;
    bool at_line_start = true;
    int consecutive_blank = 0;

    // Line buffer for line processing
    std::string line_buffer;

    while (bytes_remaining != 0 && lines_remaining != 0) {
        size_t to_read = buffer.size();
        if (bytes_remaining > 0 && static_cast<size_t>(bytes_remaining) < to_read) {
            to_read = bytes_remaining;
        }

        ssize_t n = read(in_fd, buffer.data(), to_read);
        if (n < 0) {
            if (errno == EINTR) continue;
            result.error = errno_to_error(errno);
            result.error_message = strerror(errno);
            if (telemetry) {
                telemetry->log_error(path, result.error, result.error_message);
            }
            break;
        }
        if (n == 0) break;  // EOF

        result.bytes_read = tbb_add(result.bytes_read, n);
        if (bytes_remaining > 0) {
            bytes_remaining -= n;
        }

        // Process data
        const uint8_t* data = buffer.data();
        size_t size = n;

        if (opts.show_line_numbers || opts.show_nonblank_numbers ||
            opts.show_ends || opts.show_tabs || opts.show_nonprinting ||
            opts.squeeze_blank || lines_to_skip > 0) {

            // Line-by-line processing
            std::ostringstream output;

            for (size_t i = 0; i < size; ++i) {
                char c = static_cast<char>(data[i]);

                if (c == '\n') {
                    bool is_blank = line_buffer.empty();

                    // Handle squeeze_blank
                    if (opts.squeeze_blank && is_blank) {
                        consecutive_blank++;
                        if (consecutive_blank > 1) {
                            line_buffer.clear();
                            at_line_start = true;
                            continue;
                        }
                    } else {
                        consecutive_blank = 0;
                    }

                    // Handle line skip
                    if (lines_to_skip > 0) {
                        lines_to_skip--;
                        line_buffer.clear();
                        current_line++;
                        at_line_start = true;
                        continue;
                    }

                    // Output line number
                    if (opts.show_line_numbers ||
                        (opts.show_nonblank_numbers && !is_blank)) {
                        output << std::setw(6) << current_line << "\t";
                    }

                    // Output line content
                    for (char lc : line_buffer) {
                        if (opts.show_tabs && lc == '\t') {
                            output << "^I";
                        } else if (opts.show_nonprinting) {
                            output << make_printable(lc);
                        } else {
                            output << lc;
                        }
                    }

                    // Show end marker
                    if (opts.show_ends) {
                        output << "$";
                    }
                    output << "\n";

                    result.lines_read++;
                    current_line++;
                    line_buffer.clear();
                    at_line_start = true;

                    if (lines_remaining > 0) {
                        lines_remaining--;
                        if (lines_remaining == 0) break;
                    }
                } else {
                    line_buffer += c;
                    at_line_start = false;
                }
            }

            // Write processed output
            std::string out_str = output.str();
            if (!out_str.empty()) {
                size_t written = 0;
                while (written < out_str.size()) {
                    ssize_t w = write(out_fd, out_str.c_str() + written,
                                      out_str.size() - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        result.error = ErrorCode::WRITE_ERROR;
                        result.error_message = strerror(errno);
                        break;
                    }
                    written += w;
                }
                result.bytes_written = tbb_add(result.bytes_written, written);
            }
        } else {
            // Direct output
            size_t written = 0;
            while (written < size) {
                ssize_t w = write(out_fd, data + written, size - written);
                if (w < 0) {
                    if (errno == EINTR) continue;
                    result.error = ErrorCode::WRITE_ERROR;
                    result.error_message = strerror(errno);
                    break;
                }
                written += w;
            }
            result.bytes_written = tbb_add(result.bytes_written, written);
        }

        if (result.error != ErrorCode::OK) break;
    }

    // Handle incomplete last line
    if (!line_buffer.empty() && result.error == ErrorCode::OK) {
        std::ostringstream output;

        if (lines_to_skip == 0) {
            bool is_blank = line_buffer.empty();
            if (opts.show_line_numbers ||
                (opts.show_nonblank_numbers && !is_blank)) {
                output << std::setw(6) << current_line << "\t";
            }

            for (char lc : line_buffer) {
                if (opts.show_tabs && lc == '\t') {
                    output << "^I";
                } else if (opts.show_nonprinting) {
                    output << make_printable(lc);
                } else {
                    output << lc;
                }
            }

            std::string out_str = output.str();
            ssize_t w = write(out_fd, out_str.c_str(), out_str.size());
            if (w > 0) {
                result.bytes_written = tbb_add(result.bytes_written, w);
            }
            result.lines_read++;
        }
    }

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    if (result.elapsed_ms > 0 && !is_tbb_err(result.bytes_read)) {
        result.throughput_mbps = (result.bytes_read / 1024.0 / 1024.0) /
                                  (result.elapsed_ms / 1000.0);
    }

    if (telemetry && result.error == ErrorCode::OK) {
        telemetry->log_complete(path, result.bytes_read, result.elapsed_ms);
    }

    return result;
}

CatResult cat_file(const std::string& path, int out_fd,
                   const CatOptions& opts, TelemetryWriter* telemetry) {
    CatResult result;

    if (telemetry) {
        telemetry->log_start(path, opts);
    }

    int in_fd;
    bool close_fd = false;

    if (path == "-") {
        in_fd = FD_STDIN;
    } else {
        in_fd = open(path.c_str(), O_RDONLY);
        if (in_fd < 0) {
            result.error = errno_to_error(errno);
            result.error_message = path + ": " + strerror(errno);
            if (telemetry) {
                telemetry->log_error(path, result.error, result.error_message);
            }
            return result;
        }
        close_fd = true;

        // Check if it's a directory
        struct stat st;
        if (fstat(in_fd, &st) == 0 && S_ISDIR(st.st_mode)) {
            close(in_fd);
            result.error = ErrorCode::IS_DIRECTORY;
            result.error_message = path + ": Is a directory";
            if (telemetry) {
                telemetry->log_error(path, result.error, result.error_message);
            }
            return result;
        }
    }

    result = cat_fd(in_fd, out_fd, opts, path, telemetry);
    result.files_processed = 1;

    if (close_fd) {
        close(in_fd);
    }

    return result;
}

CatResult cat_files(const std::vector<std::string>& paths, int out_fd,
                    const CatOptions& opts, TelemetryWriter* telemetry) {
    CatResult aggregate;
    auto start = std::chrono::steady_clock::now();

    for (const auto& path : paths) {
        CatResult file_result = cat_file(path, out_fd, opts, telemetry);

        aggregate.bytes_read = tbb_add(aggregate.bytes_read, file_result.bytes_read);
        aggregate.bytes_written = tbb_add(aggregate.bytes_written, file_result.bytes_written);
        aggregate.lines_read = tbb_add(aggregate.lines_read, file_result.lines_read);
        aggregate.files_processed++;

        if (file_result.error != ErrorCode::OK) {
            aggregate.error = file_result.error;
            aggregate.error_message = file_result.error_message;
            // Continue processing remaining files
        }
    }

    auto end = std::chrono::steady_clock::now();
    aggregate.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    if (aggregate.elapsed_ms > 0 && !is_tbb_err(aggregate.bytes_read)) {
        aggregate.throughput_mbps = (aggregate.bytes_read / 1024.0 / 1024.0) /
                                     (aggregate.elapsed_ms / 1000.0);
    }

    return aggregate;
}

CatResult cat_file_callback(const std::string& path, DataCallback callback,
                             const CatOptions& opts) {
    CatResult result;
    auto start = std::chrono::steady_clock::now();

    int in_fd;
    bool close_fd = false;

    if (path == "-") {
        in_fd = FD_STDIN;
    } else {
        in_fd = open(path.c_str(), O_RDONLY);
        if (in_fd < 0) {
            result.error = errno_to_error(errno);
            result.error_message = path + ": " + strerror(errno);
            return result;
        }
        close_fd = true;
    }

    std::vector<uint8_t> buffer(opts.buffer_size);

    while (true) {
        ssize_t n = read(in_fd, buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            result.error = errno_to_error(errno);
            result.error_message = strerror(errno);
            break;
        }
        if (n == 0) break;

        result.bytes_read = tbb_add(result.bytes_read, n);

        if (!callback(buffer.data(), n)) {
            break;  // Callback requested abort
        }
    }

    if (close_fd) {
        close(in_fd);
    }

    result.files_processed = 1;

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

CatResult cat_file_lines(const std::string& path, LineCallback callback,
                          const CatOptions& opts) {
    CatResult result;
    std::string line_buffer;
    int64_t line_num = 1;

    auto line_handler = [&](const uint8_t* data, size_t size) -> bool {
        for (size_t i = 0; i < size; ++i) {
            char c = static_cast<char>(data[i]);
            if (c == '\n') {
                if (!callback(line_buffer, line_num)) {
                    return false;
                }
                result.lines_read++;
                line_num++;
                line_buffer.clear();
            } else {
                line_buffer += c;
            }
        }
        return true;
    };

    result = cat_file_callback(path, line_handler, opts);

    // Handle last line without newline
    if (!line_buffer.empty()) {
        callback(line_buffer, line_num);
        result.lines_read++;
    }

    return result;
}

std::optional<std::string> read_file_string(const std::string& path, size_t max_size) {
    int64_t file_size = get_file_size(path);
    if (is_tbb_err(file_size) || file_size < 0) {
        return std::nullopt;
    }

    if (max_size > 0 && static_cast<size_t>(file_size) > max_size) {
        return std::nullopt;
    }

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return std::nullopt;
    }

    std::string content;
    content.resize(file_size);

    size_t total = 0;
    while (total < static_cast<size_t>(file_size)) {
        ssize_t n = read(fd, &content[total], file_size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return std::nullopt;
        }
        if (n == 0) break;
        total += n;
    }

    close(fd);
    content.resize(total);
    return content;
}

std::optional<std::vector<uint8_t>> read_file_bytes(const std::string& path, size_t max_size) {
    int64_t file_size = get_file_size(path);
    if (is_tbb_err(file_size) || file_size < 0) {
        // Unknown size, read dynamically
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) {
            return std::nullopt;
        }

        std::vector<uint8_t> content;
        uint8_t buffer[65536];

        while (max_size == 0 || content.size() < max_size) {
            size_t to_read = sizeof(buffer);
            if (max_size > 0 && content.size() + to_read > max_size) {
                to_read = max_size - content.size();
            }

            ssize_t n = read(fd, buffer, to_read);
            if (n < 0) {
                if (errno == EINTR) continue;
                close(fd);
                return std::nullopt;
            }
            if (n == 0) break;

            content.insert(content.end(), buffer, buffer + n);
        }

        close(fd);
        return content;
    }

    if (max_size > 0 && static_cast<size_t>(file_size) > max_size) {
        return std::nullopt;
    }

    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return std::nullopt;
    }

    std::vector<uint8_t> content(file_size);

    size_t total = 0;
    while (total < static_cast<size_t>(file_size)) {
        ssize_t n = read(fd, content.data() + total, file_size - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            close(fd);
            return std::nullopt;
        }
        if (n == 0) break;
        total += n;
    }

    close(fd);
    content.resize(total);
    return content;
}

}  // namespace acat
}  // namespace aria
