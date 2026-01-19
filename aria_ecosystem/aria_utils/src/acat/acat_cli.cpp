/**
 * acat - Aria Concatenate (CLI)
 * 
 * A proper cat replacement that uses the six-stream I/O topology:
 * - FD 1 (stdout): File content output
 * - FD 2 (stderr): Error messages
 * - FD 3 (stddbg): Debug telemetry (NDJSON format)
 * - FD 5 (stddato): Binary/metadata output
 * 
 * Copyright (c) 2025 Aria Language Project
 */

#include <acat/file_cat.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace aria::acat;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " [OPTIONS] [FILE]...\n";
    std::cerr << "Concatenate FILE(s) to standard output.\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -n, --number           Number all output lines\n";
    std::cerr << "  -b, --number-nonblank  Number non-blank output lines\n";
    std::cerr << "  -s, --squeeze-blank    Squeeze multiple blank lines\n";
    std::cerr << "  -E, --show-ends        Display $ at end of lines\n";
    std::cerr << "  -T, --show-tabs        Display TAB as ^I\n";
    std::cerr << "  -v, --show-nonprinting Display non-printing characters\n";
    std::cerr << "  -A, --show-all         Equivalent to -vET\n";
    std::cerr << "  -h, --help             Display this help and exit\n";
    std::cerr << "  --debug                Enable debug telemetry to FD 3\n";
    std::cerr << "\nWith no FILE, or when FILE is -, read standard input.\n";
    std::cerr << "\nSix-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout)  - File content\n";
    std::cerr << "  FD 2 (stderr)  - Error messages\n";
    std::cerr << "  FD 3 (stddbg)  - Debug telemetry (NDJSON)\n";
    std::cerr << "  FD 5 (stddato) - Binary/metadata output\n";
}

void write_debug(const std::string& msg) {
    // Write debug telemetry to FD 3 (stddbg) in NDJSON format
    if (fcntl(FD_STDDBG, F_GETFD) != -1) {  // Check if FD 3 is open
        std::string json = "{\"level\":\"debug\",\"component\":\"acat\",\"message\":\"" + msg + "\"}\n";
        write(FD_STDDBG, json.c_str(), json.length());
    }
}

void write_telemetry(const CatResult& result) {
    // Write performance telemetry to FD 3 (stddbg)
    if (fcntl(FD_STDDBG, F_GETFD) != -1) {
        char buf[512];
        snprintf(buf, sizeof(buf),
                "{\"level\":\"telemetry\",\"component\":\"acat\","
                "\"bytes_read\":%ld,\"bytes_written\":%ld,"
                "\"lines_read\":%ld,\"files_processed\":%ld,"
                "\"elapsed_ms\":%.2f,\"throughput_mbps\":%.2f}\n",
                result.bytes_read, result.bytes_written,
                result.lines_read, result.files_processed,
                result.elapsed_ms, result.throughput_mbps);
        write(FD_STDDBG, buf, strlen(buf));
    }
}

int main(int argc, char* argv[]) {
    CatOptions opts;
    std::vector<std::string> files;
    bool enable_debug = false;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "-n" || arg == "--number") {
            opts.show_line_numbers = true;
        } else if (arg == "-b" || arg == "--number-nonblank") {
            opts.show_nonblank_numbers = true;
        } else if (arg == "-s" || arg == "--squeeze-blank") {
            opts.squeeze_blank = true;
        } else if (arg == "-E" || arg == "--show-ends") {
            opts.show_ends = true;
        } else if (arg == "-T" || arg == "--show-tabs") {
            opts.show_tabs = true;
        } else if (arg == "-v" || arg == "--show-nonprinting") {
            opts.show_nonprinting = true;
        } else if (arg == "-A" || arg == "--show-all") {
            opts.show_nonprinting = true;
            opts.show_ends = true;
            opts.show_tabs = true;
        } else if (arg == "--debug") {
            enable_debug = true;
        } else if (arg[0] == '-' && arg != "-") {
            std::cerr << "acat: invalid option -- '" << arg << "'\n";
            std::cerr << "Try 'acat --help' for more information.\n";
            return 1;
        } else {
            files.push_back(arg);
        }
    }

    // If no files specified, read from stdin
    if (files.empty()) {
        files.push_back("-");
    }

    if (enable_debug) {
        write_debug("acat starting with " + std::to_string(files.size()) + " file(s)");
    }

    // Process files using libacat
    CatResult result;
    
    if (files.size() == 1) {
        // Single file
        result = cat_file(files[0], FD_STDOUT, opts);
    } else {
        // Multiple files
        result = cat_files(files, FD_STDOUT, opts);
    }

    // Check for errors
    if (result.error != ErrorCode::OK) {
        std::cerr << "acat: " << result.error_message << "\n";
        if (enable_debug) {
            write_debug("error: " + result.error_message);
        }
        return static_cast<int>(result.error);
    }

    // Write performance telemetry
    if (enable_debug) {
        write_telemetry(result);
    }

    return 0;
}
