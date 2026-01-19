/**
 * file_list.hpp
 * Hex-Stream Filesystem Enumerator Library
 *
 * Implements the als utility's core functionality:
 * - Recursive directory traversal
 * - Binary serialization protocol (stddato)
 * - JSON telemetry (stddbg)
 * - Rich UI rendering (stdout)
 *
 * Architecture follows the Aria Six-Stream Topology:
 * - FD 1 (stdout): Human-readable UI with colors/icons
 * - FD 3 (stddbg): Structured JSON telemetry
 * - FD 5 (stddato): Binary FileEntry stream
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ALS_FILE_LIST_HPP
#define ARIA_ALS_FILE_LIST_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>
#include <memory>

namespace aria {
namespace als {

// ============================================================================
// TBB Sentinel (Sticky Error)
// ============================================================================

// TBB64 error sentinel - minimum representable value
constexpr int64_t TBB64_ERR = INT64_MIN;

// Check if value is TBB error
inline bool is_tbb_err(int64_t v) { return v == TBB64_ERR; }

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    OK = 0,
    PERMISSION_DENIED = 1,
    NOT_FOUND = 2,
    NOT_A_DIRECTORY = 3,
    SYMLINK_LOOP = 4,
    READ_ERROR = 5,
    SYSTEM_ERROR = 6,
    UNKNOWN_ERROR = 99
};

// ============================================================================
// File Types (matches Linux d_type)
// ============================================================================

enum class FileType : uint8_t {
    UNKNOWN = 0,
    FIFO = 1,
    CHAR_DEVICE = 2,
    DIRECTORY = 4,
    BLOCK_DEVICE = 6,
    REGULAR = 8,
    SYMLINK = 10,
    SOCKET = 12,
    WHITEOUT = 14
};

// ============================================================================
// File Entry (Core Data Structure)
// ============================================================================

/**
 * Represents a single filesystem entry.
 *
 * Uses TBB-safe types for sizes:
 * - size = TBB64_ERR if stat failed
 * - All operations propagate the error sentinel
 */
struct FileEntry {
    std::string path;           // Full path relative to root
    std::string name;           // Filename only

    uint64_t inode;             // Inode number
    int64_t size;               // Size in bytes (TBB64: ERR if unknown)

    uint32_t mode;              // Permission bits (st_mode)
    uint32_t uid;               // Owner user ID
    uint32_t gid;               // Owner group ID
    uint32_t nlink;             // Number of hard links

    FileType file_type;         // Type (directory, file, symlink, etc.)

    int64_t atime;              // Access time (seconds since epoch)
    int64_t mtime;              // Modification time
    int64_t ctime;              // Change time

    uint64_t blocks;            // Allocated blocks (512-byte units)
    uint32_t blksize;           // Preferred I/O block size

    std::string link_target;    // Symlink target (if symlink)

    bool stat_failed;           // True if stat() failed

    FileEntry();
};

// ============================================================================
// Filter Options
// ============================================================================

struct FilterOptions {
    std::optional<FileType> type;           // Filter by file type
    std::optional<std::string> name_glob;   // Glob pattern for name
    std::optional<uint32_t> uid;            // Filter by owner
    std::optional<int64_t> min_size;        // Minimum size
    std::optional<int64_t> max_size;        // Maximum size
    std::optional<int64_t> newer_than;      // Modified after (epoch)
    std::optional<int64_t> older_than;      // Modified before

    bool include_hidden;        // Include dotfiles
    bool follow_symlinks;       // Follow symbolic links
    bool include_special;       // Include device files, sockets, etc.

    FilterOptions();
};

// ============================================================================
// Sort Options
// ============================================================================

enum class SortField {
    NAME,           // Alphabetical by name
    SIZE,           // By size
    MTIME,          // By modification time
    CTIME,          // By change time
    INODE,          // By inode number
    EXTENSION       // By file extension
};

struct SortOptions {
    SortField field;
    bool descending;
    bool directories_first;     // Group directories before files

    SortOptions();
};

// ============================================================================
// Traversal Options
// ============================================================================

struct TraversalOptions {
    bool recursive;             // Recurse into subdirectories
    int32_t max_depth;          // Max recursion depth (-1 = unlimited)
    bool cross_filesystems;     // Cross filesystem boundaries
    bool detect_cycles;         // Detect symlink cycles

    FilterOptions filter;
    SortOptions sort;

    TraversalOptions();
};

// ============================================================================
// Directory Listing Result
// ============================================================================

struct ListResult {
    std::vector<FileEntry> entries;
    ErrorCode error;
    std::string error_message;

    // Statistics
    int64_t total_size;         // Sum of all sizes (TBB: ERR if any unknown)
    uint64_t file_count;
    uint64_t dir_count;
    uint64_t symlink_count;
    uint64_t error_count;       // Files that couldn't be stat'd

    ListResult();
};

// ============================================================================
// Binary Protocol (stddato)
// ============================================================================

// Protocol magic number: "ARIA" in big-endian
constexpr uint32_t PROTOCOL_MAGIC = 0x41524941;
constexpr uint8_t PROTOCOL_VERSION = 0x01;

// Record types
enum class RecordType : uint8_t {
    FILE = 0x01,
    DIR_START = 0x02,
    DIR_END = 0x03,
    SYMLINK = 0x04,
    ERROR = 0x05,
    HEADER = 0x10,
    FOOTER = 0x11
};

/**
 * Binary stream writer for stddato protocol
 */
class BinaryWriter {
public:
    explicit BinaryWriter(int fd = 5);  // Default to stddato
    ~BinaryWriter();

    // Write protocol header
    void write_header();

    // Write protocol footer with stats
    void write_footer(uint64_t file_count, uint64_t total_size);

    // Write a file entry
    void write_entry(const FileEntry& entry);

    // Write directory markers
    void write_dir_start(const std::string& path);
    void write_dir_end(const std::string& path);

    // Write error record
    void write_error(const std::string& path, ErrorCode code, const std::string& message);

    // Get bytes written
    uint64_t bytes_written() const { return bytes_written_; }

private:
    int fd_;
    uint64_t bytes_written_;
    std::vector<uint8_t> buffer_;

    void write_u8(uint8_t v);
    void write_u16_be(uint16_t v);
    void write_u32_be(uint32_t v);
    void write_u64_be(uint64_t v);
    void write_i64_be(int64_t v);
    void write_string_prefixed(const std::string& s);
    void flush();
};

// ============================================================================
// Telemetry (stddbg)
// ============================================================================

/**
 * JSON telemetry writer for stddbg
 */
class TelemetryWriter {
public:
    explicit TelemetryWriter(int fd = 3);  // Default to stddbg

    // Log events
    void log_start(const std::string& root_path);
    void log_dir_enter(const std::string& path);
    void log_dir_exit(const std::string& path, uint64_t entry_count, double elapsed_ms);
    void log_error(const std::string& path, const std::string& op, const std::string& message);
    void log_cycle(const std::string& path, uint64_t inode);
    void log_complete(uint64_t files, uint64_t dirs, uint64_t errors, double elapsed_ms);
    void log_perf(const std::string& dir, uint64_t entries, double elapsed_ms);

private:
    int fd_;
    void write_json(const std::string& json);
};

// ============================================================================
// Main API
// ============================================================================

/**
 * List contents of a directory
 *
 * @param path Directory path to list
 * @param opts Traversal options
 * @return List of entries or error
 */
ListResult list_directory(const std::string& path,
                           const TraversalOptions& opts = TraversalOptions());

/**
 * Get info for a single file
 *
 * @param path File path
 * @param follow_symlinks Resolve symlinks
 * @return File entry or error
 */
std::tuple<FileEntry, ErrorCode> get_file_info(const std::string& path,
                                                 bool follow_symlinks = false);

/**
 * Recursively walk a directory tree
 *
 * @param root Starting directory
 * @param callback Called for each entry
 * @param opts Traversal options
 * @return Total entries processed or error
 */
using WalkCallback = std::function<bool(const FileEntry&, int depth)>;

std::tuple<uint64_t, ErrorCode> walk_directory(const std::string& root,
                                                 WalkCallback callback,
                                                 const TraversalOptions& opts = TraversalOptions());

/**
 * Stream directory contents to binary protocol
 *
 * @param root Starting directory
 * @param writer Binary protocol writer
 * @param telemetry Optional telemetry writer
 * @param opts Traversal options
 */
ErrorCode stream_directory(const std::string& root,
                            BinaryWriter& writer,
                            TelemetryWriter* telemetry = nullptr,
                            const TraversalOptions& opts = TraversalOptions());

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get file type from mode bits
 */
FileType mode_to_type(uint32_t mode);

/**
 * Get file type character (like ls -l)
 */
char type_to_char(FileType type);

/**
 * Format size for human display (1.2 MB, 4.5 GB, etc.)
 */
std::string format_size(int64_t size);

/**
 * Format mode bits as permission string (rwxr-xr-x)
 */
std::string format_mode(uint32_t mode);

/**
 * Format timestamp
 */
std::string format_time(int64_t time);

/**
 * Get username from UID
 */
std::string uid_to_name(uint32_t uid);

/**
 * Get group name from GID
 */
std::string gid_to_name(uint32_t gid);

/**
 * Check if path matches glob pattern
 */
bool glob_match(const std::string& pattern, const std::string& name);

} // namespace als
} // namespace aria

#endif // ARIA_ALS_FILE_LIST_HPP
