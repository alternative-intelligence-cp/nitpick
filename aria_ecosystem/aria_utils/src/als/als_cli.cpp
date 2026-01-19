/**
 * als - Aria List (CLI)
 * 
 * A proper ls replacement that uses the six-stream I/O topology:
 * - FD 1 (stdout): Formatted directory listing
 * - FD 2 (stderr): Error messages
 * - FD 3 (stddbg): Debug telemetry (NDJSON format)
 * - FD 5 (stddato): Binary FileEntry stream
 * 
 * Copyright (c) 2025 Aria Language Project
 */

#include <als/file_list.hpp>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <pwd.h>
#include <grp.h>
#include <ctime>
#include <sys/stat.h>

using namespace aria::als;

// Six-stream file descriptors
constexpr int FD_STDIN   = 0;
constexpr int FD_STDOUT  = 1;
constexpr int FD_STDERR  = 2;
constexpr int FD_STDDBG  = 3;
constexpr int FD_STDDATI = 4;
constexpr int FD_STDDATO = 5;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [OPTIONS] [FILE]...\n";
    std::cerr << "List information about FILEs (the current directory by default).\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -l, --long             Use long listing format\n";
    std::cerr << "  -a, --all              Include hidden files (starting with .)\n";
    std::cerr << "  -h, --human-readable   Print sizes in human readable format\n";
    std::cerr << "  -r, --reverse          Reverse order while sorting\n";
    std::cerr << "  -t, --time             Sort by modification time, newest first\n";
    std::cerr << "  -S, --size             Sort by file size, largest first\n";
    std::cerr << "  -R, --recursive        List subdirectories recursively\n";
    std::cerr << "  -1                     List one file per line\n";
    std::cerr << "  --help                 Display this help and exit\n";
    std::cerr << "  --debug                Enable debug telemetry to FD 3\n";
    std::cerr << "\nSix-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout)  - Formatted listing\n";
    std::cerr << "  FD 2 (stderr)  - Error messages\n";
    std::cerr << "  FD 3 (stddbg)  - Debug telemetry (NDJSON)\n";
    std::cerr << "  FD 5 (stddato) - Binary FileEntry stream\n";
}

void write_debug(const std::string& msg) {
    if (fcntl(FD_STDDBG, F_GETFD) != -1) {
        std::string json = "{\"level\":\"debug\",\"component\":\"als\",\"message\":\"" + msg + "\"}\n";
        write(FD_STDDBG, json.c_str(), json.length());
    }
}

void write_telemetry(const ListResult& result) {
    if (fcntl(FD_STDDBG, F_GETFD) != -1) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                "{\"level\":\"telemetry\",\"component\":\"als\","
                "\"entries_found\":%zu,\"directories\":%lu,\"files\":%lu,"
                "\"symlinks\":%lu,\"total_size\":%ld,"
                "\"errors\":%lu,\"elapsed_ms\":0.0}\n",
                result.entries.size(), result.dir_count,
                result.file_count, result.symlink_count,
                result.total_size, result.error_count);
        write(FD_STDDBG, buf, strlen(buf));
    }
}

std::string format_size(int64_t size, bool human_readable) {
    if (is_tbb_err(size)) return "?";
    
    if (!human_readable) {
        return std::to_string(size);
    }
    
    const char* units[] = {"B", "K", "M", "G", "T"};
    int unit = 0;
    double dsize = size;
    
    while (dsize >= 1024.0 && unit < 4) {
        dsize /= 1024.0;
        unit++;
    }
    
    char buf[32];
    if (unit == 0) {
        snprintf(buf, sizeof(buf), "%ld", size);
    } else {
        snprintf(buf, sizeof(buf), "%.1f%s", dsize, units[unit]);
    }
    return buf;
}

std::string format_permissions(uint32_t mode) {
    std::string perms;
    
    // File type
    if (S_ISDIR(mode)) perms += 'd';
    else if (S_ISLNK(mode)) perms += 'l';
    else if (S_ISCHR(mode)) perms += 'c';
    else if (S_ISBLK(mode)) perms += 'b';
    else if (S_ISFIFO(mode)) perms += 'p';
    else if (S_ISSOCK(mode)) perms += 's';
    else perms += '-';
    
    // User permissions
    perms += (mode & S_IRUSR) ? 'r' : '-';
    perms += (mode & S_IWUSR) ? 'w' : '-';
    perms += (mode & S_IXUSR) ? 'x' : '-';
    
    // Group permissions
    perms += (mode & S_IRGRP) ? 'r' : '-';
    perms += (mode & S_IWGRP) ? 'w' : '-';
    perms += (mode & S_IXGRP) ? 'x' : '-';
    
    // Other permissions
    perms += (mode & S_IROTH) ? 'r' : '-';
    perms += (mode & S_IWOTH) ? 'w' : '-';
    perms += (mode & S_IXOTH) ? 'x' : '-';
    
    return perms;
}

std::string get_username(uint32_t uid) {
    struct passwd* pw = getpwuid(uid);
    if (pw) return pw->pw_name;
    return std::to_string(uid);
}

std::string get_groupname(uint32_t gid) {
    struct group* gr = getgrgid(gid);
    if (gr) return gr->gr_name;
    return std::to_string(gid);
}

void print_long_format(const FileEntry& entry, bool human_readable) {
    std::cout << format_permissions(entry.mode) << " ";
    std::cout << std::setw(3) << entry.nlink << " ";
    std::cout << std::setw(8) << std::left << get_username(entry.uid) << " ";
    std::cout << std::setw(8) << std::left << get_groupname(entry.gid) << " ";
    std::cout << std::setw(8) << std::right << format_size(entry.size, human_readable) << " ";
    std::cout << format_time(entry.mtime) << " ";
    std::cout << entry.name;
    
    if (!entry.link_target.empty()) {
        std::cout << " -> " << entry.link_target;
    }
    
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    TraversalOptions opts;
    std::vector<std::string> paths;
    bool long_format = false;
    bool human_readable = false;
    bool one_per_line = false;
    bool enable_debug = false;
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-l" || arg == "--long") {
            long_format = true;
        } else if (arg == "-a" || arg == "--all") {
            opts.filter.include_hidden = true;
        } else if (arg == "-h" || arg == "--human-readable") {
            human_readable = true;
        } else if (arg == "-r" || arg == "--reverse") {
            opts.sort.descending = !opts.sort.descending;
        } else if (arg == "-t" || arg == "--time") {
            opts.sort.field = SortField::MTIME;
            opts.sort.descending = true;
        } else if (arg == "-S" || arg == "--size") {
            opts.sort.field = SortField::SIZE;
            opts.sort.descending = true;
        } else if (arg == "-R" || arg == "--recursive") {
            opts.recursive = true;
        } else if (arg == "-1") {
            one_per_line = true;
        } else if (arg == "--debug") {
            enable_debug = true;
        } else if (arg[0] == '-') {
            std::cerr << "als: invalid option -- '" << arg << "'\n";
            std::cerr << "Try 'als --help' for more information.\n";
            return 1;
        } else {
            paths.push_back(arg);
        }
    }
    
    // Default to current directory
    if (paths.empty()) {
        paths.push_back(".");
    }
    
    if (enable_debug) {
        write_debug("als starting with " + std::to_string(paths.size()) + " path(s)");
    }
    
    int exit_code = 0;
    
    // Process each path
    for (const auto& path : paths) {
        ListResult result = list_directory(path, opts);
        
        if (result.error != ErrorCode::OK) {
            std::cerr << "als: cannot access '" << path << "': " << result.error_message << "\n";
            exit_code = static_cast<int>(result.error);
            if (enable_debug) {
                write_debug("error: " + result.error_message);
            }
            continue;
        }
        
        // Print entries
        if (long_format) {
            for (const auto& entry : result.entries) {
                print_long_format(entry, human_readable);
            }
        } else if (one_per_line) {
            for (const auto& entry : result.entries) {
                std::cout << entry.name << "\n";
            }
        } else {
            // Simple multi-column format
            for (const auto& entry : result.entries) {
                std::cout << entry.name << "  ";
            }
            if (!result.entries.empty()) {
                std::cout << "\n";
            }
        }
        
        // Write telemetry
        if (enable_debug) {
            write_telemetry(result);
        }
    }
    
    return exit_code;
}
