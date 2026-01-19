/**
 * file_move.cpp
 * Implementation of file move/rename library
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "amv/file_move.hpp"

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <vector>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

namespace aria {
namespace amv {

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
        case ErrorCode::MOVE_INTO_SELF: return "Cannot move into itself";
        case ErrorCode::CROSS_DEVICE: return "Cross-device move";
        case ErrorCode::RENAME_FAILED: return "Rename failed";
        case ErrorCode::DELETE_FAILED: return "Delete failed";
        case ErrorCode::PERMISSION_ERROR: return "Permission denied";
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
        case EXDEV: return ErrorCode::CROSS_DEVICE;
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
    oss << "{\"event\":\"move_start\",\"src\":\"" << src
        << "\",\"dst\":\"" << dst << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_rename(const std::string& src, const std::string& dst) {
    std::ostringstream oss;
    oss << "{\"event\":\"rename\",\"src\":\"" << src
        << "\",\"dst\":\"" << dst << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_copy_start(const std::string& src, int64_t size) {
    std::ostringstream oss;
    oss << "{\"event\":\"copy_start\",\"path\":\"" << src
        << "\",\"size\":" << size << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_copy_complete(const std::string& src, int64_t bytes, double ms) {
    std::ostringstream oss;
    oss << "{\"event\":\"copy_complete\",\"path\":\"" << src
        << "\",\"bytes\":" << bytes
        << ",\"elapsed_ms\":" << ms << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_delete(const std::string& path) {
    std::ostringstream oss;
    oss << "{\"event\":\"delete\",\"path\":\"" << path << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_skip(const std::string& path, const std::string& reason) {
    std::ostringstream oss;
    oss << "{\"event\":\"skip\",\"path\":\"" << path
        << "\",\"reason\":\"" << reason << "\"}";
    write_json(oss.str());
}

void TelemetryWriter::log_backup(const std::string& original, const std::string& backup) {
    std::ostringstream oss;
    oss << "{\"event\":\"backup\",\"original\":\"" << original
        << "\",\"backup\":\"" << backup << "\"}";
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

int64_t get_file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return TBB64_ERR;
    return st.st_size;
}

bool is_newer(const std::string& src, const std::string& dst) {
    struct stat st_src, st_dst;
    if (stat(src.c_str(), &st_src) != 0) return false;
    if (stat(dst.c_str(), &st_dst) != 0) return true;
    return st_src.st_mtime > st_dst.st_mtime;
}

bool same_file(const std::string& path1, const std::string& path2) {
    struct stat st1, st2;
    if (stat(path1.c_str(), &st1) != 0) return false;
    if (stat(path2.c_str(), &st2) != 0) return false;
    return st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino;
}

bool same_filesystem(const std::string& path1, const std::string& path2) {
    struct stat st1, st2;

    // For destination that doesn't exist, check parent
    std::string check1 = exists(path1) ? path1 : parent_path(path1);
    std::string check2 = exists(path2) ? path2 : parent_path(path2);

    if (stat(check1.c_str(), &st1) != 0) return false;
    if (stat(check2.c_str(), &st2) != 0) return false;

    return st1.st_dev == st2.st_dev;
}

bool is_subpath(const std::string& parent, const std::string& child) {
    char* p_real = realpath(parent.c_str(), nullptr);
    char* c_real = realpath(child.c_str(), nullptr);

    if (!p_real || !c_real) {
        free(p_real);
        free(c_real);
        return false;
    }

    std::string p_str(p_real);
    std::string c_str(c_real);
    free(p_real);
    free(c_real);

    if (c_str.size() <= p_str.size()) return false;
    return c_str.substr(0, p_str.size()) == p_str && c_str[p_str.size()] == '/';
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

ErrorCode delete_file(const std::string& path) {
    if (unlink(path.c_str()) != 0) {
        if (rmdir(path.c_str()) != 0) {
            return errno_to_error(errno);
        }
    }
    return ErrorCode::OK;
}

ErrorCode delete_directory(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return errno_to_error(errno);
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;

        std::string full_path = join_path(path, name);

        if (is_directory(full_path) && !is_symlink(full_path)) {
            ErrorCode err = delete_directory(full_path);
            if (err != ErrorCode::OK) {
                closedir(dir);
                return err;
            }
        } else {
            if (unlink(full_path.c_str()) != 0) {
                closedir(dir);
                return errno_to_error(errno);
            }
        }
    }

    closedir(dir);

    if (rmdir(path.c_str()) != 0) {
        return errno_to_error(errno);
    }

    return ErrorCode::OK;
}

ErrorCode atomic_rename(const std::string& src, const std::string& dst) {
    if (rename(src.c_str(), dst.c_str()) != 0) {
        return errno_to_error(errno);
    }
    return ErrorCode::OK;
}

ErrorCode create_backup(const std::string& path, const std::string& suffix) {
    std::string backup = path + suffix;
    return atomic_rename(path, backup);
}

// ============================================================================
// Cross-Device Move
// ============================================================================

static int64_t copy_file_data(int src_fd, int dst_fd, int64_t size) {
    int64_t total = 0;

#ifdef __linux__
    if (size > 0) {
        off_t offset = 0;
        while (offset < size) {
            ssize_t sent = sendfile(dst_fd, src_fd, &offset, size - offset);
            if (sent < 0) {
                if (errno == EINTR) continue;
                if (errno == EINVAL || errno == ENOSYS) {
                    lseek(src_fd, 0, SEEK_SET);
                    break;
                }
                return total > 0 ? total : TBB64_ERR;
            }
            if (sent == 0) break;
            total = tbb_add(total, sent);
        }
        if (total > 0) return total;
    }
#endif

    std::vector<uint8_t> buffer(1024 * 1024);

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
    }

    return total;
}

MoveResult cross_device_move(const std::string& src,
                              const std::string& dst,
                              const MoveOptions& opts,
                              ProgressCallback progress,
                              TelemetryWriter* telemetry) {
    MoveResult result;
    result.used_copy = true;

    auto start = std::chrono::steady_clock::now();

    // Get source info
    struct stat st;
    if (lstat(src.c_str(), &st) != 0) {
        result.error = errno_to_error(errno);
        result.error_message = strerror(errno);
        result.error_path = src;
        return result;
    }

    // Handle symlinks
    if (S_ISLNK(st.st_mode)) {
        char target[4096];
        ssize_t len = readlink(src.c_str(), target, sizeof(target) - 1);
        if (len < 0) {
            result.error = ErrorCode::SYSTEM_ERROR;
            result.error_message = strerror(errno);
            result.error_path = src;
            return result;
        }
        target[len] = '\0';

        if (symlink(target, dst.c_str()) != 0) {
            result.error = errno_to_error(errno);
            result.error_message = strerror(errno);
            result.error_path = dst;
            return result;
        }

        if (unlink(src.c_str()) != 0) {
            result.error = ErrorCode::DELETE_FAILED;
            result.error_message = strerror(errno);
            result.error_path = src;
            return result;
        }

        result.files_moved = 1;
        return result;
    }

    // Handle directories recursively
    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst.c_str(), st.st_mode) != 0 && errno != EEXIST) {
            result.error = errno_to_error(errno);
            result.error_message = strerror(errno);
            result.error_path = dst;
            return result;
        }
        result.dirs_moved++;

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
            std::string dst_path = join_path(dst, name);

            MoveResult item_result = cross_device_move(src_path, dst_path, opts, progress, telemetry);

            result.bytes_moved = tbb_add(result.bytes_moved, item_result.bytes_moved);
            result.files_moved = tbb_add(result.files_moved, item_result.files_moved);
            result.dirs_moved = tbb_add(result.dirs_moved, item_result.dirs_moved);

            if (item_result.error != ErrorCode::OK) {
                result.error = item_result.error;
                result.error_message = item_result.error_message;
                result.error_path = item_result.error_path;
                closedir(dir);
                return result;
            }
        }

        closedir(dir);

        // Delete source directory
        if (rmdir(src.c_str()) != 0) {
            result.error = ErrorCode::DELETE_FAILED;
            result.error_message = strerror(errno);
            result.error_path = src;
            return result;
        }

        return result;
    }

    // Regular file copy
    if (telemetry) {
        telemetry->log_copy_start(src, st.st_size);
    }

    int src_fd = open(src.c_str(), O_RDONLY);
    if (src_fd < 0) {
        result.error = errno_to_error(errno);
        result.error_message = strerror(errno);
        result.error_path = src;
        return result;
    }

    int dst_fd = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dst_fd < 0) {
        close(src_fd);
        result.error = errno_to_error(errno);
        result.error_message = strerror(errno);
        result.error_path = dst;
        return result;
    }

    int64_t copied = copy_file_data(src_fd, dst_fd, st.st_size);
    close(src_fd);
    close(dst_fd);

    if (is_tbb_err(copied)) {
        result.error = ErrorCode::SYSTEM_ERROR;
        result.error_message = "Copy failed";
        result.error_path = src;
        return result;
    }

    result.bytes_moved = copied;

    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (telemetry) {
        telemetry->log_copy_complete(src, copied, ms);
    }

    // Delete source
    if (unlink(src.c_str()) != 0) {
        result.error = ErrorCode::DELETE_FAILED;
        result.error_message = strerror(errno);
        result.error_path = src;
        return result;
    }

    if (telemetry) {
        telemetry->log_delete(src);
    }

    result.files_moved = 1;
    result.elapsed_ms = ms;

    return result;
}

// ============================================================================
// Main API
// ============================================================================

MoveResult move_file(const std::string& src,
                     const std::string& dst,
                     const MoveOptions& opts,
                     ProgressCallback progress,
                     TelemetryWriter* telemetry) {
    MoveResult result;
    auto start = std::chrono::steady_clock::now();

    if (telemetry) {
        telemetry->log_start(src, dst);
    }

    // Check source exists
    if (!exists(src)) {
        result.error = ErrorCode::SOURCE_NOT_FOUND;
        result.error_message = src + ": No such file or directory";
        result.error_path = src;
        if (telemetry) {
            telemetry->log_error(src, result.error, result.error_message);
        }
        return result;
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

    // Check for move into self
    if (is_directory(src) && is_subpath(src, actual_dst)) {
        result.error = ErrorCode::MOVE_INTO_SELF;
        result.error_message = "Cannot move directory into itself";
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

        // Create backup if requested
        if (opts.backup) {
            ErrorCode err = create_backup(actual_dst, opts.backup_suffix);
            if (err != ErrorCode::OK) {
                result.error = err;
                result.error_message = "Failed to create backup";
                result.error_path = actual_dst;
                return result;
            }
            if (telemetry) {
                telemetry->log_backup(actual_dst, actual_dst + opts.backup_suffix);
            }
        } else {
            // Remove existing
            if (is_directory(actual_dst)) {
                ErrorCode err = delete_directory(actual_dst);
                if (err != ErrorCode::OK) {
                    result.error = err;
                    result.error_message = "Failed to remove existing directory";
                    result.error_path = actual_dst;
                    return result;
                }
            } else {
                if (unlink(actual_dst.c_str()) != 0) {
                    result.error = errno_to_error(errno);
                    result.error_message = strerror(errno);
                    result.error_path = actual_dst;
                    return result;
                }
            }
        }
    }

    // Try atomic rename first
    if (same_filesystem(src, actual_dst)) {
        ErrorCode err = atomic_rename(src, actual_dst);
        if (err == ErrorCode::OK) {
            result.used_rename = true;
            result.files_moved = is_directory(actual_dst) ? 0 : 1;
            result.dirs_moved = is_directory(actual_dst) ? 1 : 0;

            if (telemetry) {
                telemetry->log_rename(src, actual_dst);
            }

            auto end = std::chrono::steady_clock::now();
            result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

            if (telemetry) {
                telemetry->log_complete(result.files_moved + result.dirs_moved, 0, result.elapsed_ms);
            }

            return result;
        }
    }

    // Fall back to copy + delete
    result = cross_device_move(src, actual_dst, opts, progress, telemetry);

    auto end = std::chrono::steady_clock::now();
    result.elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    if (telemetry && result.error == ErrorCode::OK) {
        telemetry->log_complete(result.files_moved, result.bytes_moved, result.elapsed_ms);
    }

    return result;
}

MoveResult move_files(const std::vector<std::string>& sources,
                      const std::string& dst_dir,
                      const MoveOptions& opts,
                      ProgressCallback progress,
                      TelemetryWriter* telemetry) {
    MoveResult aggregate;
    auto start = std::chrono::steady_clock::now();

    if (!is_directory(dst_dir)) {
        aggregate.error = ErrorCode::NOT_DIRECTORY;
        aggregate.error_message = dst_dir + ": Not a directory";
        aggregate.error_path = dst_dir;
        return aggregate;
    }

    for (const auto& src : sources) {
        MoveResult file_result = move_file(src, dst_dir, opts, progress, telemetry);

        aggregate.files_moved = tbb_add(aggregate.files_moved, file_result.files_moved);
        aggregate.dirs_moved = tbb_add(aggregate.dirs_moved, file_result.dirs_moved);
        aggregate.bytes_moved = tbb_add(aggregate.bytes_moved, file_result.bytes_moved);
        aggregate.files_skipped = tbb_add(aggregate.files_skipped, file_result.files_skipped);

        if (file_result.used_rename) aggregate.used_rename = true;
        if (file_result.used_copy) aggregate.used_copy = true;

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

MoveResult rename_file(const std::string& old_path,
                       const std::string& new_path,
                       const MoveOptions& opts) {
    return move_file(old_path, new_path, opts);
}

}  // namespace amv
}  // namespace aria
