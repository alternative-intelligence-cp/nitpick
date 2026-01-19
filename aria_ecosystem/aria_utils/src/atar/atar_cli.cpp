// ============================================================================
// ATAR - Archive Utility CLI with Six-Stream Topology
// ============================================================================
// Purpose: TAR-compatible archive creation and extraction
//
// Six-Stream Architecture:
//   FD 0 (stdin):   Archive data for extraction
//   FD 1 (stdout):  Archive data for creation, verbose output
//   FD 2 (stderr):  Error messages
//   FD 3 (stddbg):  Debug telemetry (NDJSON)
//   FD 4 (stddati): Not used
//   FD 5 (stddato): Binary archive metadata (future)
//
// Usage:
//   atar -c [-f ARCHIVE] [FILES...]   Create archive
//   atar -x [-f ARCHIVE]              Extract archive
//   atar -t [-f ARCHIVE]              List contents
//   atar -r [-f ARCHIVE] [FILES...]   Append files
//
// Options:
//   -c, --create   Create a new archive
//   -x, --extract  Extract files from archive
//   -t, --list     List archive contents
//   -f FILE        Use archive FILE (default: stdout/stdin)
//   -v, --verbose  Verbose output
//   -z, --gzip     Filter through gzip (not implemented)
//   -C DIR         Change to DIR before operation
//   --debug        Enable debug telemetry on FD 3
//   --help         Show this help message
// ============================================================================

#include "archive.hpp"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <unistd.h>
#include <chrono>

// FD constants
#ifndef STDDBG_FILENO
#define STDDBG_FILENO 3
#endif

using namespace aria::atar;

// ============================================================================
// Options
// ============================================================================
struct Options {
    enum class Mode {
        NONE,
        CREATE,
        EXTRACT,
        LIST,
        APPEND
    };
    
    Mode mode = Mode::NONE;
    std::string archive_file;
    std::string change_dir;
    bool verbose = false;
    bool debug = false;
    bool gzip = false;
    std::vector<std::string> files;
};

// ============================================================================
// Helper Functions
// ============================================================================

std::string format_permissions(tbb64 mode) {
    std::string result;
    
    // File type
    char type = '-';
    if (S_ISDIR(mode))  type = 'd';
    else if (S_ISLNK(mode))  type = 'l';
    else if (S_ISBLK(mode))  type = 'b';
    else if (S_ISCHR(mode))  type = 'c';
    else if (S_ISFIFO(mode)) type = 'p';
    else if (S_ISSOCK(mode)) type = 's';
    result += type;
    
    // Permissions
    result += (mode & S_IRUSR) ? 'r' : '-';
    result += (mode & S_IWUSR) ? 'w' : '-';
    result += (mode & S_IXUSR) ? 'x' : '-';
    result += (mode & S_IRGRP) ? 'r' : '-';
    result += (mode & S_IWGRP) ? 'w' : '-';
    result += (mode & S_IXGRP) ? 'x' : '-';
    result += (mode & S_IROTH) ? 'r' : '-';
    result += (mode & S_IWOTH) ? 'w' : '-';
    result += (mode & S_IXOTH) ? 'x' : '-';
    
    return result;
}

std::string format_size(tbb64 size) {
    if (is_tbb_error(size)) return "?";
    return std::to_string(size);
}

std::string format_time(tbb64 mtime) {
    if (is_tbb_error(mtime)) return "UNKNOWN-TIME";
    
    time_t t = static_cast<time_t>(mtime);
    struct tm* tm_info = localtime(&t);
    if (!tm_info) return "INVALID";
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// ============================================================================
// Operations
// ============================================================================

int create_archive(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"atar\","
            "\"message\":\"Creating archive: %s\"}\n",
            opts.archive_file.empty() ? "stdout" : opts.archive_file.c_str()
        );
    }
    
    ArchiveWriter writer(opts.archive_file.empty() ? "-" : opts.archive_file);
    int errors = 0;
    
    for (const auto& file : opts.files) {
        if (opts.verbose) {
            std::cerr << file << "\n";
        }
        
        auto result = writer.add_file(file);
        if (!result.ok) {
            std::cerr << "atar: " << file << ": " << result.error_msg << "\n";
            errors++;
        }
    }
    
    auto finalize_result = writer.finalize();
    if (!finalize_result.ok) {
        std::cerr << "atar: error finalizing archive: " << finalize_result.error_msg << "\n";
        return 1;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    const auto& stats = writer.get_stats();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"atar\","
            "\"operation\":\"create\","
            "\"files_processed\":%ld,\"files_skipped\":%ld,"
            "\"bytes_written\":%ld,\"errors\":%d,"
            "\"elapsed_ms\":%.6f}\n",
            stats.files_processed, stats.files_skipped,
            stats.bytes_written, errors,
            elapsed_ms
        );
    }
    
    return errors > 0 ? 1 : 0;
}

int list_archive(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"atar\","
            "\"message\":\"Listing archive: %s\"}\n",
            opts.archive_file.empty() ? "stdin" : opts.archive_file.c_str()
        );
    }
    
    ArchiveReader reader(opts.archive_file.empty() ? "-" : opts.archive_file);
    int errors = 0;
    int entries = 0;
    
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) {
            // End of archive
            if (result.error_msg == "End of archive") {
                break;
            }
            std::cerr << "atar: error reading entry: " << result.error_msg << "\n";
            errors++;
            break;
        }
        
        const auto& entry = result.value;
        entries++;
        
        if (opts.verbose) {
            std::cout << format_permissions(entry.mode) << " "
                      << entry.uname << "/" << entry.gname << " "
                      << std::setw(10) << format_size(entry.size) << " "
                      << format_time(entry.mtime) << " "
                      << entry.path << "\n";
        } else {
            std::cout << entry.path << "\n";
        }
        
        // Skip entry data
        auto skip_result = reader.skip_entry(entry);
        if (!skip_result.ok) {
            std::cerr << "atar: error skipping entry: " << skip_result.error_msg << "\n";
            errors++;
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    const auto& stats = reader.get_stats();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"atar\","
            "\"operation\":\"list\","
            "\"entries_found\":%d,\"bytes_read\":%ld,"
            "\"errors\":%d,\"elapsed_ms\":%.6f}\n",
            entries, stats.bytes_read,
            errors, elapsed_ms
        );
    }
    
    return errors > 0 ? 1 : 0;
}

int extract_archive(const Options& opts) {
    auto start = std::chrono::high_resolution_clock::now();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"debug\",\"component\":\"atar\","
            "\"message\":\"Extracting archive: %s\"}\n",
            opts.archive_file.empty() ? "stdin" : opts.archive_file.c_str()
        );
    }
    
    ArchiveReader reader(opts.archive_file.empty() ? "-" : opts.archive_file);
    ArchiveOptions arc_opts;
    arc_opts.preserve_permissions = true;
    arc_opts.verbose = opts.verbose;
    
    int errors = 0;
    int extracted = 0;
    
    while (true) {
        auto result = reader.read_entry();
        if (!result.ok) {
            if (result.error_msg == "End of archive") {
                break;
            }
            std::cerr << "atar: error reading entry: " << result.error_msg << "\n";
            errors++;
            break;
        }
        
        const auto& entry = result.value;
        
        if (opts.verbose) {
            std::cerr << entry.path << "\n";
        }
        
        auto extract_result = reader.extract_entry(entry, arc_opts);
        if (!extract_result.ok) {
            std::cerr << "atar: error extracting " << entry.path << ": "
                      << extract_result.error_msg << "\n";
            errors++;
            continue;
        }
        
        extracted++;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    
    const auto& stats = reader.get_stats();
    
    if (opts.debug) {
        dprintf(STDDBG_FILENO,
            "{\"level\":\"telemetry\",\"component\":\"atar\","
            "\"operation\":\"extract\","
            "\"files_extracted\":%d,\"bytes_read\":%ld,"
            "\"errors\":%d,\"elapsed_ms\":%.6f}\n",
            extracted, stats.bytes_read,
            errors, elapsed_ms
        );
    }
    
    return errors > 0 ? 1 : 0;
}

// ============================================================================
// Help and Main
// ============================================================================

void print_help(const char* program) {
    std::cout << "Usage: " << program << " [OPTIONS] [FILES...]\n\n"
              << "Archive utility (TAR compatible)\n\n"
              << "Operation Modes (one required):\n"
              << "  -c, --create   Create a new archive\n"
              << "  -x, --extract  Extract files from archive\n"
              << "  -t, --list     List archive contents\n\n"
              << "Options:\n"
              << "  -f FILE        Use archive FILE (default: stdout/stdin)\n"
              << "  -v, --verbose  Verbose output\n"
              << "  -C DIR         Change to directory DIR\n"
              << "  --debug        Enable debug telemetry on FD 3\n"
              << "  --help         Show this help message\n\n"
              << "Examples:\n"
              << "  atar -cf archive.tar file1 file2\n"
              << "  atar -xvf archive.tar\n"
              << "  atar -tf archive.tar\n"
              << "  atar -cf - dir/ | atar -xf - -C /tmp\n";
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
    
    argc = filtered_argv.size();
    argv = filtered_argv.data();
    
    // Parse options
    int opt;
    while ((opt = getopt(argc, argv, "cxtf:vC:")) != -1) {
        switch (opt) {
            case 'c':
                opts.mode = Options::Mode::CREATE;
                break;
            case 'x':
                opts.mode = Options::Mode::EXTRACT;
                break;
            case 't':
                opts.mode = Options::Mode::LIST;
                break;
            case 'f':
                opts.archive_file = optarg;
                break;
            case 'v':
                opts.verbose = true;
                break;
            case 'C':
                opts.change_dir = optarg;
                break;
            default:
                print_help(filtered_argv[0]);
                return 1;
        }
    }
    
    // Collect files
    for (int i = optind; i < argc; i++) {
        opts.files.push_back(argv[i]);
    }
    
    // Validate mode
    if (opts.mode == Options::Mode::NONE) {
        std::cerr << "atar: must specify one of -c, -x, or -t\n";
        print_help(filtered_argv[0]);
        return 1;
    }
    
    // Change directory if requested
    if (!opts.change_dir.empty()) {
        if (chdir(opts.change_dir.c_str()) != 0) {
            std::cerr << "atar: cannot change directory to '" << opts.change_dir << "'\n";
            return 1;
        }
    }
    
    // Execute operation
    switch (opts.mode) {
        case Options::Mode::CREATE:
            if (opts.files.empty()) {
                std::cerr << "atar: cowardly refusing to create an empty archive\n";
                return 1;
            }
            return create_archive(opts);
            
        case Options::Mode::LIST:
            return list_archive(opts);
            
        case Options::Mode::EXTRACT:
            return extract_archive(opts);
            
        default:
            std::cerr << "atar: internal error: unknown mode\n";
            return 1;
    }
}
