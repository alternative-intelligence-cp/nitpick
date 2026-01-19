#include "file_stat.hpp"
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#else
#include <pwd.h>
#include <grp.h>
#endif

namespace aria {
namespace utils {
namespace astat {

// ============================================================================
// Platform-Specific Helpers
// ============================================================================

#ifdef _WIN32
// Windows implementation
static ErrorCode errno_to_error_code() {
    int err = errno;
    switch (err) {
        case ENOENT: return ErrorCode::FILE_NOT_FOUND;
        case EACCES: return ErrorCode::PERMISSION_DENIED;
        case EIO:    return ErrorCode::IO_ERROR;
        case EINVAL: return ErrorCode::INVALID_PATH;
        case ELOOP:  return ErrorCode::SYMLINK_LOOP;
        case ENAMETOOLONG: return ErrorCode::NAME_TOO_LONG;
        case EOVERFLOW: return ErrorCode::OVERFLOW;
        default:     return ErrorCode::UNKNOWN_ERROR;
    }
}

static FileType mode_to_file_type(unsigned short mode) {
    if (_S_ISREG(mode))  return FileType::REGULAR;
    if (_S_ISDIR(mode))  return FileType::DIRECTORY;
    // Windows doesn't have direct support for other types
    return FileType::UNKNOWN;
}
#else
// POSIX implementation
static ErrorCode errno_to_error_code() {
    switch (errno) {
        case ENOENT: return ErrorCode::FILE_NOT_FOUND;
        case EACCES: return ErrorCode::PERMISSION_DENIED;
        case EIO:    return ErrorCode::IO_ERROR;
        case EINVAL: return ErrorCode::INVALID_PATH;
        case ELOOP:  return ErrorCode::SYMLINK_LOOP;
        case ENAMETOOLONG: return ErrorCode::NAME_TOO_LONG;
        case EOVERFLOW: return ErrorCode::OVERFLOW;
        default:     return ErrorCode::UNKNOWN_ERROR;
    }
}

static FileType mode_to_file_type(mode_t mode) {
    if (S_ISREG(mode))  return FileType::REGULAR;
    if (S_ISDIR(mode))  return FileType::DIRECTORY;
    if (S_ISLNK(mode))  return FileType::SYMLINK;
    if (S_ISBLK(mode))  return FileType::BLOCK_DEV;
    if (S_ISCHR(mode))  return FileType::CHAR_DEV;
    if (S_ISFIFO(mode)) return FileType::FIFO;
    if (S_ISSOCK(mode)) return FileType::SOCKET;
    return FileType::UNKNOWN;
}
#endif

// Convert timespec to tbb64 nanoseconds
static tbb64 timespec_to_nanos(const struct timespec& ts) {
    // Check for overflow
    if (ts.tv_sec > (INT64_MAX / 1000000000LL)) {
        return TBB64_ERR;  // Would overflow
    }
    
    int64_t seconds_nanos = static_cast<int64_t>(ts.tv_sec) * 1000000000LL;
    int64_t total = seconds_nanos + ts.tv_nsec;
    
    // Check for TBB sentinel collision
    if (total == TBB64_ERR) {
        return TBB64_ERR - 1;  // Avoid sentinel
    }
    
    return static_cast<tbb64>(total);
}

// ============================================================================
// Core Implementation
// ============================================================================

Result<FileInfo> get_file_stat(const std::string& path) noexcept {
    FileInfo info{};
    
#ifdef _WIN32
    struct _stati64 st;
    if (_stati64(path.c_str(), &st) != 0) {
        return Result<FileInfo>::failure(errno_to_error_code());
    }
#else
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return Result<FileInfo>::failure(errno_to_error_code());
    }
#endif
    
    // Populate FileInfo
    info.size = static_cast<tbb64>(st.st_size);
    info.mode = static_cast<uint32_t>(st.st_mode & 0777);
    info.type = mode_to_file_type(st.st_mode);
    info.inode = static_cast<uint64_t>(st.st_ino);
    info.nlinks = static_cast<uint64_t>(st.st_nlink);
    
#ifdef _WIN32
    // Windows: Use modification time for all timestamps
    info.created = static_cast<tbb64>(st.st_mtime) * 1000000000LL;
    info.modified = static_cast<tbb64>(st.st_mtime) * 1000000000LL;
    info.accessed = static_cast<tbb64>(st.st_atime) * 1000000000LL;
    info.uid = 0xFFFFFFFF;  // Not available on Windows
    info.gid = 0xFFFFFFFF;
#else
    // POSIX: Full timestamp support with nanosecond precision
    #if defined(__APPLE__)
        info.created = timespec_to_nanos(st.st_ctimespec);
        info.modified = timespec_to_nanos(st.st_mtimespec);
        info.accessed = timespec_to_nanos(st.st_atimespec);
    #elif defined(__linux__)
        info.created = timespec_to_nanos(st.st_ctim);
        info.modified = timespec_to_nanos(st.st_mtim);
        info.accessed = timespec_to_nanos(st.st_atim);
    #else
        // Fallback to second precision
        info.created = static_cast<tbb64>(st.st_ctime) * 1000000000LL;
        info.modified = static_cast<tbb64>(st.st_mtime) * 1000000000LL;
        info.accessed = static_cast<tbb64>(st.st_atime) * 1000000000LL;
    #endif
    
    info.uid = static_cast<uint32_t>(st.st_uid);
    info.gid = static_cast<uint32_t>(st.st_gid);
#endif
    
    info.path_len = static_cast<uint16_t>(path.length());
    std::memset(info.reserved, 0, sizeof(info.reserved));
    
    return Result<FileInfo>::success(info);
}

Result<FileInfo> get_link_stat(const std::string& path) noexcept {
#ifdef _WIN32
    // Windows doesn't distinguish - just use regular stat
    return get_file_stat(path);
#else
    FileInfo info{};
    struct stat st;
    
    if (lstat(path.c_str(), &st) != 0) {
        return Result<FileInfo>::failure(errno_to_error_code());
    }
    
    // Populate FileInfo (same as get_file_stat but with lstat)
    info.size = static_cast<tbb64>(st.st_size);
    info.mode = static_cast<uint32_t>(st.st_mode & 0777);
    info.type = mode_to_file_type(st.st_mode);
    info.inode = static_cast<uint64_t>(st.st_ino);
    info.nlinks = static_cast<uint64_t>(st.st_nlink);
    
    #if defined(__APPLE__)
        info.created = timespec_to_nanos(st.st_ctimespec);
        info.modified = timespec_to_nanos(st.st_mtimespec);
        info.accessed = timespec_to_nanos(st.st_atimespec);
    #elif defined(__linux__)
        info.created = timespec_to_nanos(st.st_ctim);
        info.modified = timespec_to_nanos(st.st_mtim);
        info.accessed = timespec_to_nanos(st.st_atim);
    #else
        info.created = static_cast<tbb64>(st.st_ctime) * 1000000000LL;
        info.modified = static_cast<tbb64>(st.st_mtime) * 1000000000LL;
        info.accessed = static_cast<tbb64>(st.st_atime) * 1000000000LL;
    #endif
    
    info.uid = static_cast<uint32_t>(st.st_uid);
    info.gid = static_cast<uint32_t>(st.st_gid);
    info.path_len = static_cast<uint16_t>(path.length());
    std::memset(info.reserved, 0, sizeof(info.reserved));
    
    return Result<FileInfo>::success(info);
#endif
}

// ============================================================================
// Formatting Functions
// ============================================================================

std::string error_to_string(ErrorCode ec) noexcept {
    switch (ec) {
        case ErrorCode::SUCCESS:           return "Success";
        case ErrorCode::FILE_NOT_FOUND:    return "File not found";
        case ErrorCode::PERMISSION_DENIED: return "Permission denied";
        case ErrorCode::IO_ERROR:          return "I/O error";
        case ErrorCode::INVALID_PATH:      return "Invalid path";
        case ErrorCode::SYMLINK_LOOP:      return "Symlink loop detected";
        case ErrorCode::NAME_TOO_LONG:     return "Path name too long";
        case ErrorCode::OVERFLOW:          return "Value overflow";
        case ErrorCode::UNKNOWN_ERROR:     return "Unknown error";
        default:                           return "Unrecognized error";
    }
}

std::string file_type_to_string(FileType type) noexcept {
    switch (type) {
        case FileType::REGULAR:    return "regular file";
        case FileType::DIRECTORY:  return "directory";
        case FileType::SYMLINK:    return "symbolic link";
        case FileType::BLOCK_DEV:  return "block device";
        case FileType::CHAR_DEV:   return "character device";
        case FileType::FIFO:       return "named pipe";
        case FileType::SOCKET:     return "socket";
        case FileType::UNKNOWN:    return "unknown";
        default:                   return "unknown";
    }
}

std::string format_permissions(uint32_t mode) noexcept {
    std::string result(9, '-');
    
    // Owner
    if (mode & 0400) result[0] = 'r';
    if (mode & 0200) result[1] = 'w';
    if (mode & 0100) result[2] = 'x';
    
    // Group
    if (mode & 0040) result[3] = 'r';
    if (mode & 0020) result[4] = 'w';
    if (mode & 0010) result[5] = 'x';
    
    // Others
    if (mode & 0004) result[6] = 'r';
    if (mode & 0002) result[7] = 'w';
    if (mode & 0001) result[8] = 'x';
    
    return result;
}

std::string format_timestamp(tbb64 nanos) noexcept {
    if (nanos == TBB64_ERR) {
        return "N/A";
    }
    
    // Convert nanoseconds to seconds
    time_t seconds = static_cast<time_t>(nanos / 1000000000LL);
    
    // Format as ISO 8601
    std::tm tm_buf;
    #ifdef _WIN32
        if (gmtime_s(&tm_buf, &seconds) != 0) {
            return "INVALID";
        }
    #else
        if (gmtime_r(&seconds, &tm_buf) == nullptr) {
            return "INVALID";
        }
    #endif
    
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_buf.tm_year + 1900) << '-'
        << std::setw(2) << (tm_buf.tm_mon + 1) << '-'
        << std::setw(2) << tm_buf.tm_mday << ' '
        << std::setw(2) << tm_buf.tm_hour << ':'
        << std::setw(2) << tm_buf.tm_min << ':'
        << std::setw(2) << tm_buf.tm_sec << " UTC";
    
    return oss.str();
}

std::string format_size(tbb64 bytes) noexcept {
    if (bytes == TBB64_ERR) {
        return "ERR";
    }
    
    if (bytes < 0) {
        return "NEGATIVE";
    }
    
    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    size_t unit_idx = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unit_idx < 6) {
        size /= 1024.0;
        unit_idx++;
    }
    
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << " " << units[unit_idx];
    return oss.str();
}

// ============================================================================
// Six-Stream I/O
// ============================================================================

int64_t write_binary_to_stddato(const FileInfo& info, const std::string& path) noexcept {
    constexpr int FD_STDDATO = 5;
    
    // Write fixed-size struct fields
    int64_t total = 0;
    
    total += write(FD_STDDATO, &info.size, sizeof(info.size));
    total += write(FD_STDDATO, &info.created, sizeof(info.created));
    total += write(FD_STDDATO, &info.modified, sizeof(info.modified));
    total += write(FD_STDDATO, &info.accessed, sizeof(info.accessed));
    total += write(FD_STDDATO, &info.mode, sizeof(info.mode));
    total += write(FD_STDDATO, &info.uid, sizeof(info.uid));
    total += write(FD_STDDATO, &info.gid, sizeof(info.gid));
    total += write(FD_STDDATO, &info.type, sizeof(info.type));
    total += write(FD_STDDATO, &info.reserved, sizeof(info.reserved));
    total += write(FD_STDDATO, &info.inode, sizeof(info.inode));
    total += write(FD_STDDATO, &info.nlinks, sizeof(info.nlinks));
    
    // Write path length and path
    uint16_t path_len = static_cast<uint16_t>(path.length());
    total += write(FD_STDDATO, &path_len, sizeof(path_len));
    total += write(FD_STDDATO, path.c_str(), path_len);
    
    return total;
}

void write_ui_to_stdout(const FileInfo& info, const std::string& path) noexcept {
    // ANSI color codes
    const char* color_reset = "\033[0m";
    const char* color_dir = "\033[1;34m";      // Blue
    const char* color_link = "\033[1;36m";     // Cyan
    const char* color_exec = "\033[1;32m";     // Green
    const char* color_dev = "\033[1;33m";      // Yellow
    const char* color_err = "\033[1;31m";      // Red
    
    // Choose color based on file type
    const char* color = color_reset;
    char type_char = '-';
    
    switch (info.type) {
        case FileType::DIRECTORY:
            color = color_dir;
            type_char = 'd';
            break;
        case FileType::SYMLINK:
            color = color_link;
            type_char = 'l';
            break;
        case FileType::BLOCK_DEV:
            color = color_dev;
            type_char = 'b';
            break;
        case FileType::CHAR_DEV:
            color = color_dev;
            type_char = 'c';
            break;
        case FileType::FIFO:
            type_char = 'p';
            break;
        case FileType::SOCKET:
            type_char = 's';
            break;
        case FileType::REGULAR:
            // Check if executable
            if (info.mode & 0111) {
                color = color_exec;
            }
            type_char = '-';
            break;
        default:
            color = color_err;
            type_char = '?';
            break;
    }
    
    // Format output similar to `stat` command
    std::string perms = format_permissions(info.mode);
    std::string size_str = format_size(info.size);
    std::string mtime_str = format_timestamp(info.modified);
    
    std::cout << type_char << perms << "  "
              << std::setw(10) << std::right << size_str << "  "
              << mtime_str << "  "
              << color << path << color_reset << "\n";
}

void write_log_to_stddbg(const std::string& message, const std::string& level) noexcept {
    constexpr int FD_STDDBG = 3;
    
    // Format as JSON structured log
    std::ostringstream oss;
    oss << "{\"level\":\"" << level << "\","
        << "\"tool\":\"astat\","
        << "\"message\":\"" << message << "\","
        << "\"timestamp\":" << std::time(nullptr) << "}\n";
    
    std::string log = oss.str();
    write(FD_STDDBG, log.c_str(), log.length());
}

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {

int32_t aria_file_stat(const char* path, FileInfo* out_info) {
    if (!path || !out_info) {
        return static_cast<int32_t>(ErrorCode::INVALID_PATH);
    }
    
    auto result = get_file_stat(path);
    if (result.err()) {
        return static_cast<int32_t>(result.error);
    }
    
    *out_info = result.value;
    return 0;
}

int32_t aria_link_stat(const char* path, FileInfo* out_info) {
    if (!path || !out_info) {
        return static_cast<int32_t>(ErrorCode::INVALID_PATH);
    }
    
    auto result = get_link_stat(path);
    if (result.err()) {
        return static_cast<int32_t>(result.error);
    }
    
    *out_info = result.value;
    return 0;
}

int32_t aria_format_timestamp(tbb64 nanos, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return -1;
    }
    
    std::string formatted = format_timestamp(nanos);
    size_t len = std::min(formatted.length(), buf_size - 1);
    std::memcpy(buf, formatted.c_str(), len);
    buf[len] = '\0';
    
    return static_cast<int32_t>(len);
}

int32_t aria_format_size(tbb64 bytes, char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return -1;
    }
    
    std::string formatted = format_size(bytes);
    size_t len = std::min(formatted.length(), buf_size - 1);
    std::memcpy(buf, formatted.c_str(), len);
    buf[len] = '\0';
    
    return static_cast<int32_t>(len);
}

const char* aria_stat_error_string(int32_t error_code) {
    return error_to_string(static_cast<ErrorCode>(error_code)).c_str();
}

} // extern "C"

} // namespace astat
} // namespace utils
} // namespace aria
