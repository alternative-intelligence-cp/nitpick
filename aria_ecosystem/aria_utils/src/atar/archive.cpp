/**
 * @file archive.cpp
 * @brief Archive Manager - TAR format implementation
 * 
 * Implements USTAR-format TAR archives with six-stream architecture.
 * Provides TBB-safe parsing and zero-copy streaming where possible.
 */

#include "archive.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <cstdio>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace aria {
namespace atar {

// ============================================================================
// Six-Stream Writers
// ============================================================================

namespace streams {

void write_telemetry(const std::string& event, const std::string& data) {
    static const int STDDBG_FD = 3;
    
    // Check if stddbg is active (not /dev/null)
    static bool checked = false;
    static bool active = false;
    
    if (!checked) {
        struct stat st;
        if (fstat(STDDBG_FD, &st) == 0) {
            active = !S_ISCHR(st.st_mode) || st.st_rdev != makedev(1, 3);
        }
        checked = true;
    }
    
    if (!active) return;
    
    // Format JSON telemetry
    std::ostringstream oss;
    oss << "{\"event\":\"" << event << "\",\"data\":" << data << "}\n";
    std::string msg = oss.str();
    
    // Write to FD 3
    write(STDDBG_FD, msg.c_str(), msg.length());
}

void write_entry_frame(const ArchiveEntry& entry, const std::vector<uint8_t>& payload) {
    static const int STDDATO_FD = 5;
    
    // Check if stddato is active
    struct stat st;
    if (fstat(STDDATO_FD, &st) != 0) return;
    
    // Binary protocol: [Magic][MetaLen][DataLen][JSON][Payload]
    uint32_t magic = 0x41544152; // "ATAR"
    
    // Create JSON metadata
    std::ostringstream json;
    json << "{"
         << "\"path\":\"" << entry.path << "\""
         << ",\"size\":" << entry.size
         << ",\"mtime\":" << entry.mtime
         << ",\"mode\":" << std::oct << entry.mode << std::dec
         << ",\"type\":\"" << entry.type_flag << "\""
         << "}";
    std::string meta = json.str();
    
    tbb64 meta_len = static_cast<tbb64>(meta.length());
    tbb64 data_len = static_cast<tbb64>(payload.size());
    
    // Write frame
    write(STDDATO_FD, &magic, sizeof(magic));
    write(STDDATO_FD, &meta_len, sizeof(meta_len));
    write(STDDATO_FD, &data_len, sizeof(data_len));
    write(STDDATO_FD, meta.c_str(), meta.length());
    if (!payload.empty()) {
        write(STDDATO_FD, payload.data(), payload.size());
    }
}

bool is_fd_active(int fd) {
    struct stat st;
    return fstat(fd, &st) == 0 && isatty(fd);
}

} // namespace streams

// ============================================================================
// TAR Header Parser
// ============================================================================

// USTAR header offsets
constexpr size_t TAR_NAME_OFFSET = 0;
constexpr size_t TAR_NAME_SIZE = 100;
constexpr size_t TAR_MODE_OFFSET = 100;
constexpr size_t TAR_UID_OFFSET = 108;
constexpr size_t TAR_GID_OFFSET = 116;
constexpr size_t TAR_SIZE_OFFSET = 124;
constexpr size_t TAR_MTIME_OFFSET = 136;
constexpr size_t TAR_CHECKSUM_OFFSET = 148;
constexpr size_t TAR_TYPE_OFFSET = 156;
constexpr size_t TAR_LINK_OFFSET = 157;
constexpr size_t TAR_USTAR_OFFSET = 257;
constexpr size_t TAR_UNAME_OFFSET = 265;
constexpr size_t TAR_GNAME_OFFSET = 297;
constexpr size_t TAR_PREFIX_OFFSET = 345;

constexpr size_t TAR_BLOCK_SIZE = 512;

tbb64 TarParser::parse_octal(const uint8_t* ptr, size_t len) {
    tbb64 result = 0;
    
    for (size_t i = 0; i < len; ++i) {
        uint8_t c = ptr[i];
        
        // Null or space terminates
        if (c == 0 || c == ' ') break;
        
        // Must be valid octal digit
        if (c < '0' || c > '7') {
            return TBB64_ERR;
        }
        
        // TBB-aware multiplication and addition
        tbb64 next_result = result * 8 + (c - '0');
        
        // Check for overflow
        if (next_result < result) {
            return TBB64_ERR;
        }
        
        result = next_result;
    }
    
    return result;
}

tbb64 TarParser::calculate_checksum(const uint8_t* block) {
    tbb64 sum = 0;
    
    // Sum all bytes, treating checksum field as spaces
    for (size_t i = 0; i < TAR_BLOCK_SIZE; ++i) {
        if (i >= TAR_CHECKSUM_OFFSET && i < TAR_CHECKSUM_OFFSET + 8) {
            sum += ' ';
        } else {
            sum += block[i];
        }
    }
    
    return sum;
}

void TarParser::format_octal(uint8_t* ptr, size_t len, tbb64 value) {
    // Format as octal with trailing null and space
    std::snprintf(reinterpret_cast<char*>(ptr), len, "%0*lo", 
                  static_cast<int>(len - 2), static_cast<unsigned long>(value));
}

Result<ArchiveEntry> TarParser::parse_header(const uint8_t* block, tbb64 offset) {
    ArchiveEntry entry;
    entry.header_offset = offset;
    
    // Check if this is an empty block (end of archive)
    bool all_zeros = true;
    for (size_t i = 0; i < TAR_BLOCK_SIZE && all_zeros; ++i) {
        if (block[i] != 0) all_zeros = false;
    }
    
    if (all_zeros) {
        return Result<ArchiveEntry>::success(entry); // Empty entry signals EOF
    }
    
    // Parse string fields
    entry.path = std::string(reinterpret_cast<const char*>(block + TAR_NAME_OFFSET), TAR_NAME_SIZE);
    // Trim null padding
    size_t null_pos = entry.path.find('\0');
    if (null_pos != std::string::npos) {
        entry.path.resize(null_pos);
    }
    
    // Check for USTAR prefix (for paths > 100 chars)
    std::string prefix(reinterpret_cast<const char*>(block + TAR_PREFIX_OFFSET), 155);
    null_pos = prefix.find('\0');
    if (null_pos != std::string::npos) {
        prefix.resize(null_pos);
    }
    if (!prefix.empty()) {
        entry.path = prefix + "/" + entry.path;
    }
    
    entry.link_name = std::string(reinterpret_cast<const char*>(block + TAR_LINK_OFFSET), 100);
    null_pos = entry.link_name.find('\0');
    if (null_pos != std::string::npos) {
        entry.link_name.resize(null_pos);
    }
    
    entry.uname = std::string(reinterpret_cast<const char*>(block + TAR_UNAME_OFFSET), 32);
    null_pos = entry.uname.find('\0');
    if (null_pos != std::string::npos) {
        entry.uname.resize(null_pos);
    }
    
    entry.gname = std::string(reinterpret_cast<const char*>(block + TAR_GNAME_OFFSET), 32);
    null_pos = entry.gname.find('\0');
    if (null_pos != std::string::npos) {
        entry.gname.resize(null_pos);
    }
    
    // Parse numeric fields with TBB safety
    entry.mode = parse_octal(block + TAR_MODE_OFFSET, 8);
    entry.uid = parse_octal(block + TAR_UID_OFFSET, 8);
    entry.gid = parse_octal(block + TAR_GID_OFFSET, 8);
    entry.size = parse_octal(block + TAR_SIZE_OFFSET, 12);
    entry.mtime = parse_octal(block + TAR_MTIME_OFFSET, 12);
    entry.checksum_read = parse_octal(block + TAR_CHECKSUM_OFFSET, 8);
    
    // Type flag
    entry.type_flag = block[TAR_TYPE_OFFSET];
    if (entry.type_flag == 0) entry.type_flag = '0'; // Regular file
    
    // Validate critical fields
    if (is_tbb_error(entry.size) || is_tbb_error(entry.checksum_read)) {
        return Result<ArchiveEntry>::error("Invalid TAR header: corrupt numeric fields");
    }
    
    // Verify checksum
    entry.checksum_calc = calculate_checksum(block);
    entry.is_valid = (entry.checksum_calc == entry.checksum_read);
    
    if (!entry.is_valid) {
        return Result<ArchiveEntry>::error("Invalid TAR header: checksum mismatch");
    }
    
    // Calculate payload offset (header + 512 bytes)
    entry.payload_offset = offset + TAR_BLOCK_SIZE;
    
    return Result<ArchiveEntry>::success(entry);
}

Result<std::vector<uint8_t>> TarParser::create_header(const ArchiveEntry& entry) {
    std::vector<uint8_t> block(TAR_BLOCK_SIZE, 0);
    
    // Name (truncate if needed)
    std::string name = entry.path;
    if (name.length() > TAR_NAME_SIZE) {
        name = name.substr(name.length() - TAR_NAME_SIZE);
    }
    std::memcpy(block.data() + TAR_NAME_OFFSET, name.c_str(), name.length());
    
    // Numeric fields
    format_octal(block.data() + TAR_MODE_OFFSET, 8, entry.mode);
    format_octal(block.data() + TAR_UID_OFFSET, 8, entry.uid);
    format_octal(block.data() + TAR_GID_OFFSET, 8, entry.gid);
    format_octal(block.data() + TAR_SIZE_OFFSET, 12, entry.size);
    format_octal(block.data() + TAR_MTIME_OFFSET, 12, entry.mtime);
    
    // Type flag
    block[TAR_TYPE_OFFSET] = entry.type_flag;
    
    // Link name
    if (!entry.link_name.empty()) {
        std::memcpy(block.data() + TAR_LINK_OFFSET, entry.link_name.c_str(),
                   std::min(entry.link_name.length(), size_t(100)));
    }
    
    // USTAR magic
    std::memcpy(block.data() + TAR_USTAR_OFFSET, "ustar  ", 8);
    
    // User/group names
    if (!entry.uname.empty()) {
        std::memcpy(block.data() + TAR_UNAME_OFFSET, entry.uname.c_str(),
                   std::min(entry.uname.length(), size_t(32)));
    }
    if (!entry.gname.empty()) {
        std::memcpy(block.data() + TAR_GNAME_OFFSET, entry.gname.c_str(),
                   std::min(entry.gname.length(), size_t(32)));
    }
    
    // Calculate and write checksum (initially fill with spaces)
    std::memset(block.data() + TAR_CHECKSUM_OFFSET, ' ', 8);
    tbb64 checksum = calculate_checksum(block.data());
    format_octal(block.data() + TAR_CHECKSUM_OFFSET, 8, checksum);
    
    return Result<std::vector<uint8_t>>::success(block);
}

// ============================================================================
// Archive Reader
// ============================================================================

ArchiveReader::ArchiveReader(const std::string& filename)
    : fd_(-1), position_(0), buffer_(64 * 1024) // 64KB buffer
{
    fd_ = open(filename.c_str(), O_RDONLY);
    if (fd_ < 0) {
        streams::write_telemetry("error", "{\"msg\":\"Failed to open archive\"}");
    }
}

ArchiveReader::~ArchiveReader() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

Result<ArchiveEntry> ArchiveReader::read_entry() {
    if (fd_ < 0) {
        return Result<ArchiveEntry>::error("Archive not open");
    }
    
    // Read 512-byte header block
    uint8_t header[TAR_BLOCK_SIZE];
    ssize_t bytes_read = read(fd_, header, TAR_BLOCK_SIZE);
    
    if (bytes_read < 0) {
        stats_.errors_count++;
        return Result<ArchiveEntry>::error("Read error");
    }
    
    if (bytes_read == 0 || bytes_read < static_cast<ssize_t>(TAR_BLOCK_SIZE)) {
        return Result<ArchiveEntry>::error("Unexpected EOF");
    }
    
    stats_.bytes_read += bytes_read;
    
    // Parse header
    auto result = TarParser::parse_header(header, position_);
    position_ += TAR_BLOCK_SIZE;
    
    if (!result.ok) {
        stats_.errors_count++;
    } else if (!result.value.path.empty()) {
        stats_.files_processed++;
        
        // Log to stddbg
        std::ostringstream data;
        data << "{\"file\":\"" << result.value.path << "\""
             << ",\"size\":" << result.value.size
             << ",\"type\":\"" << result.value.type_flag << "\"}";
        streams::write_telemetry("entry_read", data.str());
    }
    
    return result;
}

Result<bool> ArchiveReader::extract_entry(const ArchiveEntry& entry, const ArchiveOptions& opts) {
    // Skip directories, symlinks for now (simplified implementation)
    if (entry.type_flag == '5' || entry.type_flag == '2') {
        return skip_entry(entry);
    }
    
    // Build output path
    std::string output_path = opts.output_dir + "/" + entry.path;
    
    // Create parent directories if needed
    size_t slash_pos = output_path.rfind('/');
    if (slash_pos != std::string::npos) {
        std::string dir_path = output_path.substr(0, slash_pos);
        // Create directory recursively using system command (simple approach)
        std::string cmd = "mkdir -p \"" + dir_path + "\" 2>/dev/null";
        system(cmd.c_str());
    }
    
    // Open output file
    int out_fd = open(output_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, entry.mode);
    if (out_fd < 0) {
        stats_.errors_count++;
        return skip_entry(entry); // Skip and continue
    }
    
    // Extract file content
    tbb64 remaining = entry.size;
    while (remaining > 0) {
        size_t to_read = std::min(remaining, static_cast<tbb64>(buffer_.size()));
        ssize_t bytes = read(fd_, buffer_.data(), to_read);
        
        if (bytes <= 0) {
            close(out_fd);
            return Result<bool>::error("Read error during extraction");
        }
        
        ssize_t written = write(out_fd, buffer_.data(), bytes);
        if (written != bytes) {
            close(out_fd);
            return Result<bool>::error("Write error during extraction");
        }
        
        remaining -= bytes;
        position_ += bytes;
        stats_.bytes_read += bytes;
        stats_.bytes_written += bytes;
    }
    
    close(out_fd);
    
    // Handle TAR padding (round up to 512 bytes)
    tbb64 padding = (TAR_BLOCK_SIZE - (entry.size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    if (padding > 0) {
        lseek(fd_, padding, SEEK_CUR);
        position_ += padding;
    }
    
    if (opts.verbose) {
        std::cout << "Extracted: " << entry.path << " (" << entry.size << " bytes)\n";
    }
    
    return Result<bool>::success(true);
}

Result<bool> ArchiveReader::extract_entry_stream(
    const ArchiveEntry& entry,
    std::function<void(const uint8_t*, size_t)> callback)
{
    tbb64 remaining = entry.size;
    
    while (remaining > 0) {
        size_t to_read = std::min(remaining, static_cast<tbb64>(buffer_.size()));
        ssize_t bytes = read(fd_, buffer_.data(), to_read);
        
        if (bytes <= 0) {
            return Result<bool>::error("Read error during streaming");
        }
        
        callback(buffer_.data(), bytes);
        
        remaining -= bytes;
        position_ += bytes;
        stats_.bytes_read += bytes;
    }
    
    // Handle padding
    tbb64 padding = (TAR_BLOCK_SIZE - (entry.size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    if (padding > 0) {
        lseek(fd_, padding, SEEK_CUR);
        position_ += padding;
    }
    
    return Result<bool>::success(true);
}

Result<bool> ArchiveReader::skip_entry(const ArchiveEntry& entry) {
    // Calculate total bytes to skip (data + padding)
    tbb64 data_size = entry.size;
    tbb64 padding = (TAR_BLOCK_SIZE - (data_size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
    tbb64 total_skip = data_size + padding;
    
    if (lseek(fd_, total_skip, SEEK_CUR) < 0) {
        return Result<bool>::error("Seek error");
    }
    
    position_ += total_skip;
    stats_.files_skipped++;
    
    return Result<bool>::success(true);
}

bool ArchiveReader::is_eof() const {
    return fd_ < 0;
}

// ============================================================================
// Archive Writer
// ============================================================================

ArchiveWriter::ArchiveWriter(const std::string& filename)
    : fd_(-1), position_(0), buffer_(64 * 1024)
{
    fd_ = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_ < 0) {
        streams::write_telemetry("error", "{\"msg\":\"Failed to create archive\"}");
    }
}

ArchiveWriter::~ArchiveWriter() {
    if (fd_ >= 0) {
        close(fd_);
    }
}

Result<ArchiveEntry> ArchiveWriter::create_entry_from_stat(
    const std::string& path,
    const struct stat& st)
{
    ArchiveEntry entry;
    entry.path = path;
    entry.size = st.st_size;
    entry.mode = st.st_mode & 0777;
    entry.uid = st.st_uid;
    entry.gid = st.st_gid;
    entry.mtime = st.st_mtime;
    
    // Determine type
    if (S_ISREG(st.st_mode)) {
        entry.type_flag = '0';
    } else if (S_ISDIR(st.st_mode)) {
        entry.type_flag = '5';
    } else if (S_ISLNK(st.st_mode)) {
        entry.type_flag = '2';
    } else {
        return Result<ArchiveEntry>::error("Unsupported file type");
    }
    
    return Result<ArchiveEntry>::success(entry);
}

Result<bool> ArchiveWriter::add_file(const std::string& path) {
    // Get file stats
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        stats_.errors_count++;
        return Result<bool>::error("Cannot stat file");
    }
    
    // Create entry
    auto entry_result = create_entry_from_stat(path, st);
    if (!entry_result.ok) {
        return Result<bool>::error(entry_result.error_msg);
    }
    
    ArchiveEntry entry = entry_result.value;
    
    // Create header
    auto header_result = TarParser::create_header(entry);
    if (!header_result.ok) {
        return Result<bool>::error(header_result.error_msg);
    }
    
    // Write header
    if (write(fd_, header_result.value.data(), TAR_BLOCK_SIZE) != TAR_BLOCK_SIZE) {
        return Result<bool>::error("Write error (header)");
    }
    position_ += TAR_BLOCK_SIZE;
    
    // Write file content (if regular file)
    if (entry.type_flag == '0') {
        int in_fd = open(path.c_str(), O_RDONLY);
        if (in_fd < 0) {
            return Result<bool>::error("Cannot open input file");
        }
        
        tbb64 remaining = entry.size;
        while (remaining > 0) {
            ssize_t bytes = read(in_fd, buffer_.data(), 
                               std::min(remaining, static_cast<tbb64>(buffer_.size())));
            if (bytes <= 0) break;
            
            write(fd_, buffer_.data(), bytes);
            remaining -= bytes;
            position_ += bytes;
            stats_.bytes_read += bytes;
        }
        
        close(in_fd);
        
        // Write padding
        tbb64 padding = (TAR_BLOCK_SIZE - (entry.size % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
        if (padding > 0) {
            uint8_t zeros[TAR_BLOCK_SIZE] = {0};
            write(fd_, zeros, padding);
            position_ += padding;
        }
    }
    
    stats_.files_processed++;
    return Result<bool>::success(true);
}

Result<bool> ArchiveWriter::add_directory(const std::string& path, bool recursive) {
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return Result<bool>::error("Cannot open directory");
    }
    
    struct dirent* ent;
    while ((ent = readdir(dir)) != nullptr) {
        std::string name = ent->d_name;
        if (name == "." || name == "..") continue;
        
        std::string full_path = path + "/" + name;
        
        struct stat st;
        if (stat(full_path.c_str(), &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode) && recursive) {
            add_directory(full_path, true);
        } else if (S_ISREG(st.st_mode)) {
            add_file(full_path);
        }
    }
    
    closedir(dir);
    return Result<bool>::success(true);
}

Result<bool> ArchiveWriter::write_entry(const ArchiveEntry& entry, const uint8_t* data, size_t len) {
    auto header_result = TarParser::create_header(entry);
    if (!header_result.ok) {
        return Result<bool>::error(header_result.error_msg);
    }
    
    // Write header
    write(fd_, header_result.value.data(), TAR_BLOCK_SIZE);
    position_ += TAR_BLOCK_SIZE;
    
    // Write data
    if (data && len > 0) {
        write(fd_, data, len);
        position_ += len;
        
        // Padding
        tbb64 padding = (TAR_BLOCK_SIZE - (len % TAR_BLOCK_SIZE)) % TAR_BLOCK_SIZE;
        if (padding > 0) {
            uint8_t zeros[TAR_BLOCK_SIZE] = {0};
            write(fd_, zeros, padding);
            position_ += padding;
        }
    }
    
    return Result<bool>::success(true);
}

Result<bool> ArchiveWriter::finalize() {
    // Write two 512-byte zero blocks to mark end of archive
    uint8_t zeros[TAR_BLOCK_SIZE * 2] = {0};
    write(fd_, zeros, sizeof(zeros));
    
    streams::write_telemetry("finalize", "{\"total_files\":" + 
                           std::to_string(stats_.files_processed) + "}");
    
    return Result<bool>::success(true);
}

// ============================================================================
// FFI C API
// ============================================================================

extern "C" {

int aria_extract_archive(
    const char* archive_path,
    const char* output_dir,
    int verbose,
    void (*progress_callback)(const char* filename, int64_t bytes))
{
    ArchiveReader reader(archive_path);
    ArchiveOptions opts;
    opts.output_dir = output_dir ? output_dir : ".";
    opts.verbose = verbose != 0;
    
    while (true) {
        auto entry_result = reader.read_entry();
        if (!entry_result.ok) break;
        
        const ArchiveEntry& entry = entry_result.value;
        if (entry.path.empty()) break; // EOF
        
        if (progress_callback) {
            progress_callback(entry.path.c_str(), entry.size);
        }
        
        reader.extract_entry(entry, opts);
    }
    
    return 0;
}

int aria_create_archive(
    const char* archive_path,
    const char** file_list,
    int file_count)
{
    ArchiveWriter writer(archive_path);
    
    for (int i = 0; i < file_count; ++i) {
        writer.add_file(file_list[i]);
    }
    
    writer.finalize();
    return 0;
}

int aria_list_archive(
    const char* archive_path,
    void (*entry_callback)(const char* filename, int64_t size, int64_t mtime))
{
    ArchiveReader reader(archive_path);
    
    while (true) {
        auto entry_result = reader.read_entry();
        if (!entry_result.ok) break;
        
        const ArchiveEntry& entry = entry_result.value;
        if (entry.path.empty()) break;
        
        if (entry_callback) {
            entry_callback(entry.path.c_str(), entry.size, entry.mtime);
        }
        
        reader.skip_entry(entry);
    }
    
    return 0;
}

const char* aria_archive_error_string(int code) {
    switch (code) {
        case 0: return "Success";
        case 1: return "File not found";
        case 2: return "I/O error";
        case 3: return "Corrupt archive";
        default: return "Unknown error";
    }
}

} // extern "C"

} // namespace atar
} // namespace aria
