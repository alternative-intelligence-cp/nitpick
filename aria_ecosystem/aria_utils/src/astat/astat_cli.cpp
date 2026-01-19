// ============================================================================
// ASTAT - File Statistics CLI with Six-Stream Topology
// ============================================================================
// Purpose: Display detailed file metadata using the astat library
//
// Six-Stream Architecture:
//   FD 0 (stdin):   Not used
//   FD 1 (stdout):  Human-readable file statistics
//   FD 2 (stderr):  Error messages
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used  
//   FD 5 (stddato): Binary FileInfo structures (future)
//
// Usage:
//   astat [OPTIONS] FILE...
//
// Options:
//   -L, --dereference  Follow symlinks (default: stat)
//   -l, --no-dereference  Do not follow symlinks (use lstat)
//   -f, --file-system  Display filesystem information (not implemented yet)
//   -c, --format=FORMAT  Custom output format
//   -t, --terse  Terse output (single line per file)
//   --debug  Enable debug telemetry on FD 3
//   --help  Show this help message
// ============================================================================

#include "file_stat.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <chrono>

// FD constants
#ifndef STDDBG_FILENO
#define STDDBG_FILENO 3
#endif

using namespace aria::utils::astat;

// ============================================================================
// Options
// ============================================================================
struct Options {
    bool dereference = true;   // Follow symlinks by default
    bool terse = false;        // Terse output
    bool debug = false;        // Debug telemetry
    std::string format;        // Custom format
    std::vector<std::string> files;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string get_username(uint32_t uid) {
    if (uid == 0xFFFFFFFF) return "UNKNOWN";
    struct passwd* pw = getpwuid(uid);
    if (!pw) return std::to_string(uid);
    return pw->pw_name;
}

std::string get_groupname(uint32_t gid) {
    if (gid == 0xFFFFFFFF) return "UNKNOWN";
    struct group* gr = getgrgid(gid);
    if (!gr) return std::to_string(gid);
    return gr->gr_name;
}

// Use library's file_type_to_string() - already defined

std::string mode_to_string(uint32_t mode) {
    if (mode == 0xFFFFFFFF) return "?---------";
    
    std::string result;
    
    // File type
    if (S_ISREG(mode))  result += '-';
    else if (S_ISDIR(mode))  result += 'd';
    else if (S_ISLNK(mode))  result += 'l';
    else if (S_ISBLK(mode))  result += 'b';
    else if (S_ISCHR(mode))  result += 'c';
    else if (S_ISFIFO(mode)) result += 'p';
    else if (S_ISSOCK(mode)) result += 's';
    else result += '?';
    
    // User permissions
    result += (mode & S_IRUSR) ? 'r' : '-';
    result += (mode & S_IWUSR) ? 'w' : '-';
    result += (mode & S_IXUSR) ? 'x' : '-';
    
    // Group permissions
    result += (mode & S_IRGRP) ? 'r' : '-';
    result += (mode & S_IWGRP) ? 'w' : '-';
    result += (mode & S_IXGRP) ? 'x' : '-';
    
    // Other permissions
    result += (mode & S_IROTH) ? 'r' : '-';
    result += (mode & S_IWOTH) ? 'w' : '-';
    result += (mode & S_IXOTH) ? 'x' : '-';
    
    return result;
}

// ============================================================================
// Display Functions
// ============================================================================

void display_stat_verbose(const std::string& path, const FileInfo& info) {
    std::cout << "  File: " << path << "\n";
    std::cout << "  Size: " << (info.has_valid_size() ? std::to_string(info.size) : "N/A")
              << "\tType: " << file_type_to_string(info.type) << "\n";
    std::cout << "Access: (" << std::oct << std::setw(4) << std::setfill('0') << (info.mode & 0777) << std::dec
              << "/" << mode_to_string(info.mode) << ")";
    std::cout << "  Uid: ( " << std::setw(5) << info.uid << "/" << std::setw(8) << std::left 
              << get_username(info.uid) << ")"
              << "   Gid: ( " << std::setw(5) << info.gid << "/" << std::setw(8) << std::left
              << get_groupname(info.gid) << ")\n" << std::right;
    
    std::cout << "Access: " << format_timestamp(info.accessed) << "\n";
    std::cout << "Modify: " << format_timestamp(info.modified) << "\n";
    std::cout << "Change: " << format_timestamp(info.created) << "\n";
    
    std::cout << " Inode: " << info.inode << "\t";
    std::cout << "Links: " << info.nlinks << "\n";
}

void display_stat_terse(const std::string& path, const FileInfo& info) {
    std::cout << path << " "
              << (info.has_valid_size() ? std::to_string(info.size) : "?") << " "
              << info.inode << " "
              << std::oct << (info.mode & 0777) << std::dec << " "
              << info.uid << " " << info.gid << " "
              << static_cast<int>(info.type) << " "
              << info.nlinks << "\n";
}

// ============================================================================
// Main Processing
// ============================================================================

int process_files(const Options& opts) {
    int errors = 0;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (const auto& file : opts.files) {
        Result<FileInfo> result = opts.dereference 
            ? get_file_stat(file)
            : get_link_stat(file);
        
        if (result.err()) {
            std::cerr << "astat: cannot stat '" << file << "': "
                      << error_to_string(result.error) << "\n";
            errors++;
            continue;
        }
        
        if (opts.terse) {
            display_stat_terse(file, result.value);
        } else {
            display_stat_verbose(file, result.value);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Telemetry output to FD 3
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"astat\","
            "\"files_processed\":%d,\"files_failed\":%d,"
            "\"elapsed_ms\":%.6f}\n",
            static_cast<int>(opts.files.size()) - errors,
            errors,
            elapsed_ms
        );
    }
    
    return errors > 0 ? 1 : 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] FILE...\n\n"
              << "Display detailed file statistics\n\n"
              << "Options:\n"
              << "  -L, --dereference      Follow symlinks (default)\n"
              << "  -l, --no-dereference   Do not follow symlinks\n"
              << "  -t, --terse            Terse output (single line)\n"
              << "  --debug                Enable debug telemetry on FD 3\n"
              << "  --help                 Show this help message\n\n"
              << "Examples:\n"
              << "  astat file.txt\n"
              << "  astat -l /tmp/symlink\n"
              << "  astat -t /etc/passwd\n"
              << "  astat --debug file1 file2 3> telemetry.log\n";
}

int main(int argc, char* argv[]) {
    Options opts;
    
    // Filter --debug and --help before getopt
    std::vector<char*> filtered_argv;
    filtered_argv.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            opts.debug = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else {
            filtered_argv.push_back(argv[i]);
        }
    }
    
    // Reset argc/argv for getopt
    argc = filtered_argv.size();
    argv = filtered_argv.data();
    
    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "Lltf:c:")) != -1) {
        switch (opt) {
            case 'L':
                opts.dereference = true;
                break;
            case 'l':
                opts.dereference = false;
                break;
            case 't':
                opts.terse = true;
                break;
            case 'f':
                // File system option - not implemented yet
                std::cerr << "astat: -f/--file-system not implemented yet\n";
                return 1;
            case 'c':
                opts.format = optarg;
                std::cerr << "astat: -c/--format not implemented yet\n";
                return 1;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    // Collect files
    for (int i = optind; i < argc; i++) {
        opts.files.push_back(argv[i]);
    }
    
    if (opts.files.empty()) {
        std::cerr << "astat: missing file operand\n";
        print_help(filtered_argv[0]);
        return 1;
    }
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"astat\","
            "\"message\":\"astat starting with %zu file(s)\"}\n",
            opts.files.size()
        );
    }
    
    return process_files(opts);
}
