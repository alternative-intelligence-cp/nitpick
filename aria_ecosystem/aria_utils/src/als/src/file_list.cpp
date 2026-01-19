/**
 * file_list.cpp
 * Hex-Stream Filesystem Enumerator Implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "als/file_list.hpp"

#include <algorithm>
#include <cstring>
#include <queue>
#include <set>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <fnmatch.h>

#ifdef __linux__
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace aria {
namespace als {

// ============================================================================
// Constructors
// ============================================================================

FileEntry::FileEntry()
    : inode(0), size(0), mode(0), uid(0), gid(0), nlink(0),
      file_type(FileType::UNKNOWN),
      atime(0), mtime(0), ctime(0),
      blocks(0), blksize(0),
      stat_failed(false) {}

FilterOptions::FilterOptions()
    : include_hidden(false),
      follow_symlinks(false),
      include_special(false) {}

SortOptions::SortOptions()
    : field(SortField::NAME),
      descending(false),
      directories_first(true) {}

TraversalOptions::TraversalOptions()
    : recursive(false),
      max_depth(-1),
      cross_filesystems(true),
      detect_cycles(true) {}

ListResult::ListResult()
    : error(ErrorCode::OK),
      total_size(0),
      file_count(0),
      dir_count(0),
      symlink_count(0),
      error_count(0) {}

// ============================================================================
// File Type Utilities
// ============================================================================

FileType mode_to_type(uint32_t mode) {
#ifdef _WIN32
    // Windows doesn't have the same mode flags
    return FileType::UNKNOWN;
#else
    switch (mode & S_IFMT) {
        case S_IFREG:  return FileType::REGULAR;
        case S_IFDIR:  return FileType::DIRECTORY;
        case S_IFLNK:  return FileType::SYMLINK;
        case S_IFIFO:  return FileType::FIFO;
        case S_IFSOCK: return FileType::SOCKET;
        case S_IFCHR:  return FileType::CHAR_DEVICE;
        case S_IFBLK:  return FileType::BLOCK_DEVICE;
        default:       return FileType::UNKNOWN;
    }
#endif
}

char type_to_char(FileType type) {
    switch (type) {
        case FileType::REGULAR:      return '-';
        case FileType::DIRECTORY:    return 'd';
        case FileType::SYMLINK:      return 'l';
        case FileType::FIFO:         return 'p';
        case FileType::SOCKET:       return 's';
        case FileType::CHAR_DEVICE:  return 'c';
        case FileType::BLOCK_DEVICE: return 'b';
        default:                     return '?';
    }
}

std::string format_size(int64_t size) {
    if (is_tbb_err(size)) {
        return "ERR";
    }

    const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit_idx = 0;
    double dsize = static_cast<double>(size);

    while (dsize >= 1024.0 && unit_idx < 5) {
        dsize /= 1024.0;
        unit_idx++;
    }

    std::ostringstream oss;
    if (unit_idx == 0) {
        oss << size << " " << units[unit_idx];
    } else {
        oss << std::fixed << std::setprecision(1) << dsize << " " << units[unit_idx];
    }
    return oss.str();
}

std::string format_mode(uint32_t mode) {
#ifdef _WIN32
    return "?????????";
#else
    char buf[10];
    buf[0] = (mode & S_IRUSR) ? 'r' : '-';
    buf[1] = (mode & S_IWUSR) ? 'w' : '-';
    buf[2] = (mode & S_IXUSR) ? ((mode & S_ISUID) ? 's' : 'x') : ((mode & S_ISUID) ? 'S' : '-');
    buf[3] = (mode & S_IRGRP) ? 'r' : '-';
    buf[4] = (mode & S_IWGRP) ? 'w' : '-';
    buf[5] = (mode & S_IXGRP) ? ((mode & S_ISGID) ? 's' : 'x') : ((mode & S_ISGID) ? 'S' : '-');
    buf[6] = (mode & S_IROTH) ? 'r' : '-';
    buf[7] = (mode & S_IWOTH) ? 'w' : '-';
    buf[8] = (mode & S_IXOTH) ? ((mode & S_ISVTX) ? 't' : 'x') : ((mode & S_ISVTX) ? 'T' : '-');
    buf[9] = '\0';
    return std::string(buf);
#endif
}

std::string format_time(int64_t time) {
    if (time == 0) return "N/A";

    std::time_t t = static_cast<std::time_t>(time);
    std::tm* tm = std::localtime(&t);
    if (!tm) return "N/A";

    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm);
    return std::string(buf);
}

std::string uid_to_name(uint32_t uid) {
#ifdef _WIN32
    return std::to_string(uid);
#else
    struct passwd* pw = getpwuid(uid);
    return pw ? pw->pw_name : std::to_string(uid);
#endif
}

std::string gid_to_name(uint32_t gid) {
#ifdef _WIN32
    return std::to_string(gid);
#else
    struct group* gr = getgrgid(gid);
    return gr ? gr->gr_name : std::to_string(gid);
#endif
}

bool glob_match(const std::string& pattern, const std::string& name) {
#ifdef _WIN32
    // Simple implementation for Windows
    // TODO: Proper Windows globbing
    return name.find(pattern) != std::string::npos;
#else
    return fnmatch(pattern.c_str(), name.c_str(), FNM_PATHNAME) == 0;
#endif
}

// ============================================================================
// Get File Info
// ============================================================================

#ifndef _WIN32

std::tuple<FileEntry, ErrorCode> get_file_info(const std::string& path,
                                                 bool follow_symlinks) {
    FileEntry entry;
    entry.path = path;

    // Extract filename
    size_t pos = path.rfind('/');
    entry.name = (pos != std::string::npos) ? path.substr(pos + 1) : path;

    struct stat st;
    int result;

    if (follow_symlinks) {
        result = stat(path.c_str(), &st);
    } else {
        result = lstat(path.c_str(), &st);
    }

    if (result != 0) {
        entry.stat_failed = true;
        entry.size = TBB64_ERR;

        if (errno == ENOENT) {
            return {entry, ErrorCode::NOT_FOUND};
        } else if (errno == EACCES) {
            return {entry, ErrorCode::PERMISSION_DENIED};
        }
        return {entry, ErrorCode::SYSTEM_ERROR};
    }

    entry.inode = st.st_ino;
    entry.size = st.st_size;
    entry.mode = st.st_mode;
    entry.uid = st.st_uid;
    entry.gid = st.st_gid;
    entry.nlink = st.st_nlink;
    entry.file_type = mode_to_type(st.st_mode);
    entry.atime = st.st_atime;
    entry.mtime = st.st_mtime;
    entry.ctime = st.st_ctime;
    entry.blocks = st.st_blocks;
    entry.blksize = st.st_blksize;

    // Read symlink target
    if (entry.file_type == FileType::SYMLINK) {
        char buf[4096];
        ssize_t len = readlink(path.c_str(), buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            entry.link_target = buf;
        }
    }

    return {entry, ErrorCode::OK};
}

#endif

// ============================================================================
// List Directory
// ============================================================================

#ifndef _WIN32

static bool apply_filter(const FileEntry& entry, const FilterOptions& filter) {
    // Hidden files
    if (!filter.include_hidden && !entry.name.empty() && entry.name[0] == '.') {
        return false;
    }

    // Type filter
    if (filter.type.has_value() && entry.file_type != filter.type.value()) {
        return false;
    }

    // Name glob
    if (filter.name_glob.has_value()) {
        if (!glob_match(filter.name_glob.value(), entry.name)) {
            return false;
        }
    }

    // Owner filter
    if (filter.uid.has_value() && entry.uid != filter.uid.value()) {
        return false;
    }

    // Size filters (skip if stat failed)
    if (!entry.stat_failed && !is_tbb_err(entry.size)) {
        if (filter.min_size.has_value() && entry.size < filter.min_size.value()) {
            return false;
        }
        if (filter.max_size.has_value() && entry.size > filter.max_size.value()) {
            return false;
        }
    }

    // Time filters
    if (filter.newer_than.has_value() && entry.mtime < filter.newer_than.value()) {
        return false;
    }
    if (filter.older_than.has_value() && entry.mtime > filter.older_than.value()) {
        return false;
    }

    // Special files
    if (!filter.include_special) {
        if (entry.file_type == FileType::CHAR_DEVICE ||
            entry.file_type == FileType::BLOCK_DEVICE ||
            entry.file_type == FileType::SOCKET ||
            entry.file_type == FileType::FIFO) {
            return false;
        }
    }

    return true;
}

static void sort_entries(std::vector<FileEntry>& entries, const SortOptions& opts) {
    std::sort(entries.begin(), entries.end(),
        [&opts](const FileEntry& a, const FileEntry& b) {
            // Directories first (if enabled)
            if (opts.directories_first) {
                bool a_dir = (a.file_type == FileType::DIRECTORY);
                bool b_dir = (b.file_type == FileType::DIRECTORY);
                if (a_dir != b_dir) {
                    return a_dir;
                }
            }

            int cmp = 0;
            switch (opts.field) {
                case SortField::NAME:
                    cmp = a.name.compare(b.name);
                    break;
                case SortField::SIZE:
                    // Handle TBB errors
                    if (is_tbb_err(a.size) && is_tbb_err(b.size)) cmp = 0;
                    else if (is_tbb_err(a.size)) cmp = 1;
                    else if (is_tbb_err(b.size)) cmp = -1;
                    else cmp = (a.size < b.size) ? -1 : (a.size > b.size) ? 1 : 0;
                    break;
                case SortField::MTIME:
                    cmp = (a.mtime < b.mtime) ? -1 : (a.mtime > b.mtime) ? 1 : 0;
                    break;
                case SortField::CTIME:
                    cmp = (a.ctime < b.ctime) ? -1 : (a.ctime > b.ctime) ? 1 : 0;
                    break;
                case SortField::INODE:
                    cmp = (a.inode < b.inode) ? -1 : (a.inode > b.inode) ? 1 : 0;
                    break;
                case SortField::EXTENSION: {
                    size_t a_dot = a.name.rfind('.');
                    size_t b_dot = b.name.rfind('.');
                    std::string a_ext = (a_dot != std::string::npos) ? a.name.substr(a_dot) : "";
                    std::string b_ext = (b_dot != std::string::npos) ? b.name.substr(b_dot) : "";
                    cmp = a_ext.compare(b_ext);
                    break;
                }
            }

            return opts.descending ? (cmp > 0) : (cmp < 0);
        });
}

ListResult list_directory(const std::string& path, const TraversalOptions& opts) {
    ListResult result;

    DIR* dir = opendir(path.c_str());
    if (!dir) {
        if (errno == ENOENT) {
            result.error = ErrorCode::NOT_FOUND;
        } else if (errno == EACCES) {
            result.error = ErrorCode::PERMISSION_DENIED;
        } else if (errno == ENOTDIR) {
            result.error = ErrorCode::NOT_A_DIRECTORY;
        } else {
            result.error = ErrorCode::SYSTEM_ERROR;
        }
        result.error_message = strerror(errno);
        return result;
    }

    struct dirent* de;
    while ((de = readdir(dir)) != nullptr) {
        // Skip . and ..
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        std::string full_path = path + "/" + de->d_name;
        auto [entry, err] = get_file_info(full_path, opts.filter.follow_symlinks);

        if (err != ErrorCode::OK) {
            result.error_count++;
            continue;
        }

        // Apply filters
        if (!apply_filter(entry, opts.filter)) {
            continue;
        }

        // Update statistics
        if (entry.file_type == FileType::DIRECTORY) {
            result.dir_count++;
        } else if (entry.file_type == FileType::SYMLINK) {
            result.symlink_count++;
        } else {
            result.file_count++;
        }

        // Accumulate size with TBB safety
        if (!is_tbb_err(entry.size) && !is_tbb_err(result.total_size)) {
            result.total_size += entry.size;
        } else {
            result.total_size = TBB64_ERR;  // Propagate error
        }

        result.entries.push_back(std::move(entry));
    }

    closedir(dir);

    // Sort results
    sort_entries(result.entries, opts.sort);

    return result;
}

// ============================================================================
// Walk Directory (Recursive)
// ============================================================================

std::tuple<uint64_t, ErrorCode> walk_directory(const std::string& root,
                                                 WalkCallback callback,
                                                 const TraversalOptions& opts) {
    uint64_t count = 0;
    std::set<uint64_t> visited_inodes;  // For cycle detection

    struct WorkItem {
        std::string path;
        int depth;
    };

    std::queue<WorkItem> queue;
    queue.push({root, 0});

    while (!queue.empty()) {
        WorkItem item = queue.front();
        queue.pop();

        // Depth limit
        if (opts.max_depth >= 0 && item.depth > opts.max_depth) {
            continue;
        }

        DIR* dir = opendir(item.path.c_str());
        if (!dir) continue;

        struct dirent* de;
        while ((de = readdir(dir)) != nullptr) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                continue;
            }

            std::string full_path = item.path + "/" + de->d_name;
            auto [entry, err] = get_file_info(full_path, opts.filter.follow_symlinks);

            if (err != ErrorCode::OK) continue;

            // Cycle detection
            if (opts.detect_cycles && entry.file_type == FileType::DIRECTORY) {
                if (visited_inodes.count(entry.inode)) {
                    continue;  // Skip cycle
                }
                visited_inodes.insert(entry.inode);
            }

            // Apply filter
            if (!apply_filter(entry, opts.filter)) continue;

            // Invoke callback
            if (!callback(entry, item.depth)) {
                closedir(dir);
                return {count, ErrorCode::OK};  // Callback requested stop
            }

            count++;

            // Queue subdirectory
            if (opts.recursive && entry.file_type == FileType::DIRECTORY) {
                queue.push({full_path, item.depth + 1});
            }
        }

        closedir(dir);
    }

    return {count, ErrorCode::OK};
}

#endif

// ============================================================================
// Binary Writer (stddato protocol)
// ============================================================================

BinaryWriter::BinaryWriter(int fd) : fd_(fd), bytes_written_(0) {
    buffer_.reserve(8192);
}

BinaryWriter::~BinaryWriter() {
    flush();
}

void BinaryWriter::write_u8(uint8_t v) {
    buffer_.push_back(v);
}

void BinaryWriter::write_u16_be(uint16_t v) {
    buffer_.push_back((v >> 8) & 0xFF);
    buffer_.push_back(v & 0xFF);
}

void BinaryWriter::write_u32_be(uint32_t v) {
    buffer_.push_back((v >> 24) & 0xFF);
    buffer_.push_back((v >> 16) & 0xFF);
    buffer_.push_back((v >> 8) & 0xFF);
    buffer_.push_back(v & 0xFF);
}

void BinaryWriter::write_u64_be(uint64_t v) {
    for (int i = 7; i >= 0; --i) {
        buffer_.push_back((v >> (i * 8)) & 0xFF);
    }
}

void BinaryWriter::write_i64_be(int64_t v) {
    write_u64_be(static_cast<uint64_t>(v));
}

void BinaryWriter::write_string_prefixed(const std::string& s) {
    uint16_t len = static_cast<uint16_t>(std::min(s.size(), size_t(65535)));
    write_u16_be(len);
    buffer_.insert(buffer_.end(), s.begin(), s.begin() + len);
}

void BinaryWriter::flush() {
    if (!buffer_.empty() && fd_ >= 0) {
        ssize_t written = write(fd_, buffer_.data(), buffer_.size());
        if (written > 0) {
            bytes_written_ += written;
        }
        buffer_.clear();
    }
}

void BinaryWriter::write_header() {
    write_u32_be(PROTOCOL_MAGIC);
    write_u8(PROTOCOL_VERSION);
    write_u8(static_cast<uint8_t>(RecordType::HEADER));
    flush();
}

void BinaryWriter::write_footer(uint64_t file_count, uint64_t total_size) {
    write_u8(static_cast<uint8_t>(RecordType::FOOTER));
    write_u64_be(file_count);
    write_u64_be(total_size);
    flush();
}

void BinaryWriter::write_entry(const FileEntry& entry) {
    write_u8(static_cast<uint8_t>(RecordType::FILE));
    write_string_prefixed(entry.path);
    write_u64_be(entry.inode);

    // Handle TBB error for size
    if (is_tbb_err(entry.size)) {
        write_i64_be(-1);  // Sentinel in protocol
    } else {
        write_i64_be(entry.size);
    }

    write_u32_be(entry.mode);
    write_u32_be(entry.uid);
    write_u32_be(entry.gid);
    write_i64_be(entry.mtime);

    if (buffer_.size() >= 4096) {
        flush();
    }
}

void BinaryWriter::write_dir_start(const std::string& path) {
    write_u8(static_cast<uint8_t>(RecordType::DIR_START));
    write_string_prefixed(path);
}

void BinaryWriter::write_dir_end(const std::string& path) {
    write_u8(static_cast<uint8_t>(RecordType::DIR_END));
    write_string_prefixed(path);
}

void BinaryWriter::write_error(const std::string& path, ErrorCode code, const std::string& message) {
    write_u8(static_cast<uint8_t>(RecordType::ERROR));
    write_string_prefixed(path);
    write_u8(static_cast<uint8_t>(code));
    write_string_prefixed(message);
}

// ============================================================================
// Telemetry Writer (stddbg)
// ============================================================================

TelemetryWriter::TelemetryWriter(int fd) : fd_(fd) {}

void TelemetryWriter::write_json(const std::string& json) {
    if (fd_ >= 0) {
        std::string line = json + "\n";
        write(fd_, line.c_str(), line.size());
    }
}

void TelemetryWriter::log_start(const std::string& root_path) {
    std::ostringstream oss;
    oss << R"({"event":"start","path":")" << root_path << R"("})";
    write_json(oss.str());
}

void TelemetryWriter::log_dir_enter(const std::string& path) {
    std::ostringstream oss;
    oss << R"({"event":"dir_enter","path":")" << path << R"("})";
    write_json(oss.str());
}

void TelemetryWriter::log_dir_exit(const std::string& path, uint64_t entry_count, double elapsed_ms) {
    std::ostringstream oss;
    oss << R"({"event":"dir_exit","path":")" << path
        << R"(","entries":)" << entry_count
        << R"(,"ms":)" << std::fixed << std::setprecision(2) << elapsed_ms << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_error(const std::string& path, const std::string& op, const std::string& message) {
    std::ostringstream oss;
    oss << R"({"event":"error","op":")" << op
        << R"(","path":")" << path
        << R"(","message":")" << message << R"("})";
    write_json(oss.str());
}

void TelemetryWriter::log_cycle(const std::string& path, uint64_t inode) {
    std::ostringstream oss;
    oss << R"({"event":"cycle","path":")" << path
        << R"(","inode":)" << inode << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_complete(uint64_t files, uint64_t dirs, uint64_t errors, double elapsed_ms) {
    std::ostringstream oss;
    oss << R"({"event":"complete","files":)" << files
        << R"(,"dirs":)" << dirs
        << R"(,"errors":)" << errors
        << R"(,"ms":)" << std::fixed << std::setprecision(2) << elapsed_ms << "}";
    write_json(oss.str());
}

void TelemetryWriter::log_perf(const std::string& dir, uint64_t entries, double elapsed_ms) {
    std::ostringstream oss;
    oss << R"({"event":"perf","dir":")" << dir
        << R"(","entries":)" << entries
        << R"(,"ms":)" << std::fixed << std::setprecision(2) << elapsed_ms << "}";
    write_json(oss.str());
}

// ============================================================================
// Stream Directory (Full Pipeline)
// ============================================================================

#ifndef _WIN32

ErrorCode stream_directory(const std::string& root,
                            BinaryWriter& writer,
                            TelemetryWriter* telemetry,
                            const TraversalOptions& opts) {
    auto start_time = std::chrono::steady_clock::now();

    if (telemetry) {
        telemetry->log_start(root);
    }

    writer.write_header();

    uint64_t file_count = 0;
    uint64_t dir_count = 0;
    uint64_t error_count = 0;
    int64_t total_size = 0;

    std::set<uint64_t> visited_inodes;

    struct WorkItem {
        std::string path;
        int depth;
    };

    std::queue<WorkItem> queue;
    queue.push({root, 0});

    while (!queue.empty()) {
        WorkItem item = queue.front();
        queue.pop();

        if (opts.max_depth >= 0 && item.depth > opts.max_depth) {
            continue;
        }

        auto dir_start = std::chrono::steady_clock::now();

        if (telemetry) {
            telemetry->log_dir_enter(item.path);
        }

        writer.write_dir_start(item.path);

        DIR* dir = opendir(item.path.c_str());
        if (!dir) {
            if (telemetry) {
                telemetry->log_error(item.path, "opendir", strerror(errno));
            }
            writer.write_error(item.path, ErrorCode::PERMISSION_DENIED, strerror(errno));
            error_count++;
            continue;
        }

        uint64_t dir_entries = 0;
        struct dirent* de;

        while ((de = readdir(dir)) != nullptr) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                continue;
            }

            std::string full_path = item.path + "/" + de->d_name;
            auto [entry, err] = get_file_info(full_path, opts.filter.follow_symlinks);

            if (err != ErrorCode::OK) {
                if (telemetry) {
                    telemetry->log_error(full_path, "stat", "Failed to stat");
                }
                error_count++;
                continue;
            }

            // Cycle detection
            if (opts.detect_cycles && entry.file_type == FileType::DIRECTORY) {
                if (visited_inodes.count(entry.inode)) {
                    if (telemetry) {
                        telemetry->log_cycle(full_path, entry.inode);
                    }
                    continue;
                }
                visited_inodes.insert(entry.inode);
            }

            // Apply filter
            if (!apply_filter(entry, opts.filter)) {
                continue;
            }

            // Write entry
            writer.write_entry(entry);
            dir_entries++;

            // Update stats
            if (entry.file_type == FileType::DIRECTORY) {
                dir_count++;
                if (opts.recursive) {
                    queue.push({full_path, item.depth + 1});
                }
            } else {
                file_count++;
                if (!is_tbb_err(entry.size) && !is_tbb_err(total_size)) {
                    total_size += entry.size;
                } else {
                    total_size = TBB64_ERR;
                }
            }
        }

        closedir(dir);

        writer.write_dir_end(item.path);

        if (telemetry) {
            auto dir_end = std::chrono::steady_clock::now();
            double elapsed_ms = std::chrono::duration<double, std::milli>(dir_end - dir_start).count();
            telemetry->log_dir_exit(item.path, dir_entries, elapsed_ms);
        }
    }

    writer.write_footer(file_count, is_tbb_err(total_size) ? 0 : total_size);

    if (telemetry) {
        auto end_time = std::chrono::steady_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        telemetry->log_complete(file_count, dir_count, error_count, elapsed_ms);
    }

    return ErrorCode::OK;
}

#endif

} // namespace als
} // namespace aria
