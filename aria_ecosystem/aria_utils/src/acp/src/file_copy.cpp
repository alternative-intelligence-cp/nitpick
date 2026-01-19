/**
 * file_copy.cpp
 * Implementation of file copy library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "acp/file_copy.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <utime.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <vector>

#ifdef __linux__
#include <sys/sendfile.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#endif

namespace aria {
namespace acp {

// ============================================================================
// Error Handling
// ============================================================================

const char* error_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::SOURCE_NOT_FOUND: return "Source not found";
        case ErrorCode::SOURCE_NOT_READABLE: return "Source not readable";
        case ErrorCode::DEST_EXISTS: return "Destination exists";
        case ErrorCode::DEST_NOT_WRITABLE: return "Destination not writable";
        case ErrorCode::IS_DIRECTORY: return "Is a directory";
        case ErrorCode::NOT_DIRECTORY: return "Not a directory";
        case ErrorCode::SAME_FILE: return "Same file";
        case ErrorCode::COPY_INTO_SELF: return "Cannot copy into itself";
        case ErrorCode::DISK_FULL: return "Disk full";
        case ErrorCode::READ_ERROR: return "Read error";
        case ErrorCode::WRITE_ERROR: return "Write error";
        case ErrorCode::PERMISSION_ERROR: return "Permission denied";
        case ErrorCode::ATTRIBUTE_ERROR: return "Failed to set attributes";
        case ErrorCode::SYMLINK_ERROR: return "Symlink error";
        case ErrorCode::SYSTEM_ERROR: return "System error";
        default: return "Unknown error";
    }
}

static ErrorCode errno_to_error(int err) {
    switch (err) {
        case ENOENT: return ErrorCode::SOURCE_NOT_FOUND;
        case EACCES:
        case EPERM: return ErrorCode::PERMISSION_ERROR;
        case EISDIR: return ErrorCode::IS_DIRECTORY;
        case ENOTDIR: return ErrorCode::NOT_DIRECTORY;
        case EEXIST: return ErrorCode::DEST_EXISTS;
        case ENOSPC: return ErrorCode::DISK_FULL;
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

void TelemetryWriter::log_start(const std::string& src, const std::string& dst) {
    std::ostringstream oss;
    oss << "{\"event\":\"copy_start\",\"src\":\"" << src
        << "\",\"dst\":\"" << dst << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_file_start(const std::string& path, int64_t size) {
    std::ostringstream oss;
    oss << "{\"event\":\"file_start\",\"path\":\"" << path
        << "\",\"size\":" << size << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_file_complete(const std::string& path, int64_t bytes, double ms) {
    std::ostringstream oss;
    oss << "{\"event\":\"file_complete\",\"path\":\"" << path
        << "\",\"bytes\":" << bytes
        << ",\"elapsed_ms\":" << ms << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_dir_create(const std::string& path) {
    std::ostringstream oss;
    oss << "{\"event\":\"dir_create\",\"path\":\"" << path << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_symlink(const std::string& path, const std::string& target) {
    std::ostringstream oss;
    oss << "{\"event\":\"symlink\",\"path\":\"" << path
        << "\",\"target\":\"" << target << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_skip(const std::string& path, const std::string& reason) {
    std::ostringstream oss;
    oss << "{\"event\":\"skip\",\"path\":\"" << path
        << "\",\"reason\":\"" << reason << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_error(const std::string& path, ErrorCode code, const std::string& message) {
    std::ostringstream oss;
    oss << "{\"event\":\"error\",\"path\":\"" << path
        << "\",\"code\":" << static_cast<int>(code)
        << ",\"message\":\"" << message << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_complete(int64_t files, int64_t bytes, double ms) {
    std::ostringstream oss;
    oss << "{\"event\":\"complete\",\"files\":" << files
        << ",\"bytes\":" << bytes
        << ",\"elapsed_ms\":" << ms << "}";
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

bool exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

bool is_symlink(const std::string& path) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0) return false;
    return S_ISLNK(st.st_mode);
}

std::optional<std::string> read_symlink(const std::string& path) {
    char buf[4096];
    ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
    if (len < 0) return std::nullopt;
    buf[len] = '\0';
    return std::string(buf);
}

std::optional<std::string> canonical_path(const std::string& path) {
    char* resolved = realpath(path.c_str(), nullptr);
    if (!resolved) return std::nullopt;
    std::string result(resolved);
    free(resolved);
    return result;
}

std::string parent_path(const std::string& path) {
    size_t pos = path.rfind('/');
    if (pos == std::string::npos) return ".";
    if (pos == 0) return "/";
    return path.substr(0, pos);
}

std::string filename(const std::string& path) {
    size_t pos = path.rfind('/');
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

bool same_file(const std::string& path1, const std::string& path2) {
    struct stat st1, st2;
    if (stat(path1.c_str(), &st1) != 0) return false;
    if (stat(path2.c_str(), &st2) != 0) return false;
    return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
}

bool is_subpath(const std::string& parent, const std::string& child) {
    auto p = canonical_path(parent);
    auto c = canonical_path(child);
    if (!p || !c) return false;
    if (c->size() <= p->size()) return false;
    return c->substr(0, p->size()) == *p && (*c)[p->size()] == '/';
}

bool is_newer(const std::string& src, const std::string& dst) {
    struct stat st_src, st_dst;
    if (stat(src.c_str(), &st_src) != 0) return false;
    if (stat(dst.c_str(), &st_dst) != 0) return true;  // Dst doesn't exist
    return st_src.st_mtime > st_dst.st_mtime;
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
        oss.precision(2);
        oss << std::fixed << size << " " << units[unit];
    }
    return oss.str();
}

// ============================================================================
// Low-Level Copy
// ============================================================================

int64_t copy_file_data(int src_fd, int dst_fd, int64_t size, size_t buffer_size) {
    int64_t total = 0;

#ifdef __linux__
    // Try sendfile first (zero-copy)
    if (size == 0) {
        struct stat st;
        if (fstat(src_fd, &st) == 0) {
            size = st.st_size;
        }
    }

    if (size > 0) {
        off_t offset = 0;
        while (offset < size) {
            ssize_t sent = sendfile(dst_fd, src_fd, &offset, size - offset);
            if (sent < 0) {
                if (errno == EINTR) continue;
                if (errno == EINVAL || errno == ENOSYS) {
                    // sendfile not supported, fall through to read/write
                    lseek(src_fd, 0, SEEK_SET);
                    break;
                }
                return total > 0 ? total : TBB64_ERR;
            }
            if (sent == 0) break;
            total = tbb_add(total, sent);
            if (is_tbb_err(total)) return TBB64_ERR;
        }
        if (total > 0) return total;
    }
#endif

    // Fallback to read/write
    std::vector<uint8_t> buffer(buffer_size);

    while (true) {
        ssize_t n = read(src_fd, buffer.data(), buffer.size());
        if (n < 0) {
            if (errno == EINTR) continue;
            return total > 0 ? total : TBB64_ERR;
        }
        if (n == 0) break;

        size_t written = 0;
        while (written < static_cast<size_t>(n)) {
            ssize_t w = write(dst_fd, buffer.data() + written, n - written);
            if (w < 0) {
                if (errno == EINTR) continue;
                return total > 0 ? total : TBB64_ERR;
            }
            written += w;
        }

        total = tbb_add(total, n);
        if (is_tbb_err(total)) return TBB64_ERR;
    }

    return total;
}

ErrorCode copy_symlink(const std::string& src, const std::string& dst) {
    auto target = read_symlink(src);
    if (!target) {
        return ErrorCode::SYMLINK_ERROR;
    }

    if (symlink(target->c_str(), dst.c_str()) != 0) {
        return errno_to_error(errno);
    }

    return ErrorCode::OK;
}

ErrorCode create_directory_like(const std::string& src, const std::string& dst) {
    struct stat st;
    if (stat(src.c_str(), &st) != 0) {
        return errno_to_error(errno);
    }

    if (mkdir(dst.c_str(), st.st_mode) != 0) {
        if (errno != EEXIST) {
            return errno_to_error(errno);
        }
    }

    return ErrorCode::OK;
}

ErrorCode copy_file_with_attrs(const std::string& src,
                                const std::string& dst,
                                const CopyOptions& opts) {
    struct stat st;
    if (stat(src.c_str(), &st) != 0) {
        return errno_to_error(errno);
    }

    int src_fd = open(src.c_str(), O_RDONLY);
    if (src_fd < 0) {
        return errno_to_error(errno);
    }

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
    int dst_fd = open(dst.c_str(), flags, st.st_mode);
    if (dst_fd < 0) {
        close(src_fd);
        return errno_to_error(errno);
    }

    int64_t copied = copy_file_data(src_fd, dst_fd, st.st_size, opts.buffer_size);
    close(src_fd);
    close(dst_fd);

    if (is_tbb_err(copied)) {
        return ErrorCode::WRITE_ERROR;
    }

    // Preserve attributes
    if (opts.preserve_mode) {
        if (chmod(dst.c_str(), st.st_mode) != 0) {
            // Non-fatal, continue
        }
    }

    if (opts.preserve_timestamps) {
        struct utimbuf times;
        times.actime = st.st_atime;
        times.modtime = st.st_mtime;
        utime(dst.c_str(), &times);
    }

    if (opts.preserve_ownership) {
        chown(dst.c_str(), st.st_uid, st.st_gid);
    }

    return ErrorCode::OK;
}

// ============================================================================
// Main API
// ============================================================================

CopyResult copy_file(const std::string& src,
                     const std::string& dst,
                     const CopyOptions& opts,
                     ProgressCallback progress,
                     TelemetryWriter* telemetry) {
    CopyResult result;
    auto start = std::chrono::steady_clock::now();

    if (telemetry) {
        telemetry->log_start(src, dst);
    }

    // Check source
    if (!exists(src)) {
        result.error = ErrorCode::SOURCE_NOT_FOUND;
        result.error_message = src + ": No such file or directory";
        result.error_path = src;
        if (telemetry) {
            telemetry->log_error(src, result.error, result.error_message);
        }
        return result;
    }

    // Handle symlinks
    if (is_symlink(src) && opts.preserve_links && !opts.dereference) {
        std::string actual_dst = dst;
        if (is_directory(dst)) {
            actual_dst = join_path(dst, filename(src));
        }

        if (exists(actual_dst)) {
            if (opts.no_clobber) {
                result.files_skipped = 1;
                if (telemetry) {
                    telemetry->log_skip(src, "no-clobber");
                }
                return result;
            }
            if (!opts.force) {
                result.error = ErrorCode::DEST_EXISTS;
                result.error_message = actual_dst + ": File exists";
                result.error_path = actual_dst;
                return result;
            }
            unlink(actual_dst.c_str());
        }

        ErrorCode err = copy_symlink(src, actual_dst);
        if (err != ErrorCode::OK) {
            result.error = err;
            result.error_message = error_to_string(err);
            result.error_path = actual_dst;
        } else {
            result.symlinks_copied = 1;
            if (telemetry) {
                auto target = read_symlink(src);
                telemetry->log_symlink(actual_dst, target.value_or(""));
            }
        }
        return result;
    }

    // Handle directories
    if (is_directory(src)) {
        if (!opts.recursive) {
            result.error = ErrorCode::IS_DIRECTORY;
            result.error_message = src + ": Is a directory";
            result.error_path = src;
            return result;
        }
        return copy_directory(src, dst, opts, progress, telemetry);
    }

    // Determine actual destination
    std::string actual_dst = dst;
    if (is_directory(dst)) {
        actual_dst = join_path(dst, filename(src));
    }

    // Check for same file
    if (exists(actual_dst) && same_file(src, actual_dst)) {
        result.error = ErrorCode::SAME_FILE;
        result.error_message = src + " and " + actual_dst + " are the same file";
        result.error_path = src;
        return result;
    }

    // Handle existing destination
    if (exists(actual_dst)) {
        if (opts.no_clobber) {
            result.files_skipped = 1;
            if (telemetry) {
                telemetry->log_skip(src, "no-clobber");
            }
            return result;
        }
        if (opts.update && !is_newer(src, actual_dst)) {
            result.files_skipped = 1;
            if (telemetry) {
                telemetry->log_skip(src, "not-newer");
            }
            return result;
        }
        if (!opts.force) {
            result.error = ErrorCode::DEST_EXISTS;
            result.error_message = actual_dst + ": File exists";
            result.error_path = actual_dst;
            return result;
        }
    }

    // Get file size for progress
    int64_t file_size = get_file_size(src);

    if (telemetry) {
        telemetry->log_file_start(src, file_size);
    }

    if (progress) {
        CopyProgress prog;
        prog.current_file = src;
        prog.file_bytes_copied = 0;
        prog.file_bytes_total = file_size;
        if (!progress(prog)) {
            result.error = ErrorCode::SYSTEM_ERROR;
            result.error_message = "Cancelled";
            return result;
        }
    }

    // Perform copy
    ErrorCode err = copy_file_with_attrs(src, actual_dst, opts);
    if (err != ErrorCode::OK) {
        result.error = err;
        result.error_message = error_to_string(err);
        result.error_path = actual_dst;
        if (telemetry) {
            telemetry->log_error(actual_dst, err, result.error_message);
        }
        return result;
    }

    result.files_copied = 1;
    result.bytes_copied = file_size > 0 ? file_size : 0;

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    if (result.elapsed_ms > 0 && result.bytes_copied > 0) {
        result.throughput_mbps = (result.bytes_copied / 1024.0 / 1024.0) /
                                  (result.elapsed_ms / 1000.0);
    }

    if (telemetry) {
        telemetry->log_file_complete(src, result.bytes_copied, result.elapsed_ms);
        telemetry->log_complete(1, result.bytes_copied, result.elapsed_ms);
    }

    return result;
}

CopyResult copy_files(const std::vector<std::string>& sources,
                      const std::string& dst_dir,
                      const CopyOptions& opts,
                      ProgressCallback progress,
                      TelemetryWriter* telemetry) {
    CopyResult aggregate;
    auto start = std::chrono::steady_clock::now();

    if (!is_directory(dst_dir)) {
        aggregate.error = ErrorCode::NOT_DIRECTORY;
        aggregate.error_message = dst_dir + ": Not a directory";
        aggregate.error_path = dst_dir;
        return aggregate;
    }

    for (const auto& src : sources) {
        CopyResult file_result = copy_file(src, dst_dir, opts, progress, telemetry);

        aggregate.bytes_copied = tbb_add(aggregate.bytes_copied, file_result.bytes_copied);
        aggregate.files_copied = tbb_add(aggregate.files_copied, file_result.files_copied);
        aggregate.dirs_created = tbb_add(aggregate.dirs_created, file_result.dirs_created);
        aggregate.symlinks_copied = tbb_add(aggregate.symlinks_copied, file_result.symlinks_copied);
        aggregate.files_skipped = tbb_add(aggregate.files_skipped, file_result.files_skipped);

        if (file_result.error != ErrorCode::OK) {
            aggregate.error = file_result.error;
            aggregate.error_message = file_result.error_message;
            aggregate.error_path = file_result.error_path;
        }
    }

    auto end = std::chrono::steady_clock::now();
    aggregate.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    return aggregate;
}

CopyResult copy_directory(const std::string& src,
                          const std::string& dst,
                          const CopyOptions& opts,
                          ProgressCallback progress,
                          TelemetryWriter* telemetry) {
    CopyResult result;
    auto start = std::chrono::steady_clock::now();

    if (!is_directory(src)) {
        result.error = ErrorCode::NOT_DIRECTORY;
        result.error_message = src + ": Not a directory";
        result.error_path = src;
        return result;
    }

    // Check for copy into self
    if (is_subpath(src, dst)) {
        result.error = ErrorCode::COPY_INTO_SELF;
        result.error_message = "Cannot copy directory into itself";
        result.error_path = src;
        return result;
    }

    // Create destination directory
    std::string actual_dst = dst;
    if (exists(dst) && is_directory(dst)) {
        actual_dst = join_path(dst, filename(src));
    }

    ErrorCode err = create_directory_like(src, actual_dst);
    if (err != ErrorCode::OK && err != ErrorCode::DEST_EXISTS) {
        result.error = err;
        result.error_message = error_to_string(err);
        result.error_path = actual_dst;
        return result;
    }
    result.dirs_created++;

    if (telemetry) {
        telemetry->log_dir_create(actual_dst);
    }

    // Iterate directory contents
    DIR* dir = opendir(src.c_str());
    if (!dir) {
        result.error = errno_to_error(errno);
        result.error_message = strerror(errno);
        result.error_path = src;
        return result;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string src_path = join_path(src, name);
        std::string dst_path = join_path(actual_dst, name);

        CopyResult item_result;

        if (is_symlink(src_path) && opts.preserve_links && !opts.dereference) {
            err = copy_symlink(src_path, dst_path);
            if (err == ErrorCode::OK) {
                result.symlinks_copied++;
                if (telemetry) {
                    auto target = read_symlink(src_path);
                    telemetry->log_symlink(dst_path, target.value_or(""));
                }
            }
        } else if (is_directory(src_path)) {
            item_result = copy_directory(src_path, dst_path, opts, progress, telemetry);
        } else {
            item_result = copy_file(src_path, dst_path, opts, progress, telemetry);
        }

        result.bytes_copied = tbb_add(result.bytes_copied, item_result.bytes_copied);
        result.files_copied = tbb_add(result.files_copied, item_result.files_copied);
        result.dirs_created = tbb_add(result.dirs_created, item_result.dirs_created);
        result.symlinks_copied = tbb_add(result.symlinks_copied, item_result.symlinks_copied);
        result.files_skipped = tbb_add(result.files_skipped, item_result.files_skipped);

        if (item_result.error != ErrorCode::OK) {
            result.error = item_result.error;
            result.error_message = item_result.error_message;
            result.error_path = item_result.error_path;
        }
    }

    closedir(dir);

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    if (result.elapsed_ms > 0 && result.bytes_copied > 0) {
        result.throughput_mbps = (result.bytes_copied / 1024.0 / 1024.0) /
                                  (result.elapsed_ms / 1000.0);
    }

    return result;
}

}  // namespace acp
}  // namespace aria
