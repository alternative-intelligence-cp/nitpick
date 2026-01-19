/**
 * aglob_cli.cpp
 * Aria Glob - Six-Stream File Glob Utility
 *
 * A glob pattern matching utility with six-stream architecture:
 * - FD 0 (stdin):   Standard input
 * - FD 1 (stdout):  Matched file paths
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Binary path structures
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include <aglob/glob_engine.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace aria::glob;

static bool debug_mode = false;
constexpr int FD_STDDBG = 3;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]... PATTERN [DIR]\n";
    std::cerr << "Match files using glob patterns.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout): Matched file paths\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n\n";
    std::cerr << "Pattern Syntax:\n";
    std::cerr << "  *         match any characters\n";
    std::cerr << "  ?         match single character\n";
    std::cerr << "  [abc]     match characters a, b, or c\n";
    std::cerr << "  [a-z]     match range a to z\n";
    std::cerr << "  **/       recursive directory match\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -d DIR    search in specific directory (default: .)\n";
    std::cerr << "  -0        separate output with null bytes\n";
    std::cerr << "  --debug   output debug telemetry to FD 3\n";
    std::cerr << "  --help    display this help and exit\n\n";
    std::cerr << "Examples:\n";
    std::cerr << "  aglob '*.txt'           # All .txt files in current dir\n";
    std::cerr << "  aglob '**/*.cpp'        # All .cpp files recursively\n";
    std::cerr << "  aglob 'src/**/test_*.aria'  # Test files under src/\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"aglob\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(size_t matches, GlobError error) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"aglob\""
                      ",\"matches\":" + std::to_string(matches) +
                      ",\"error_code\":" + std::to_string(static_cast<int>(error)) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

int main(int argc, char* argv[]) {
    GlobOptions opts;
    std::string pattern;
    std::string base_dir = ".";
    bool null_separator = false;
    
    // Check for long options
    std::vector<char*> filtered_args;
    filtered_args.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            filtered_args.push_back(argv[i]);
        }
    }
    
    int filtered_argc = filtered_args.size();
    
    // Parse options
    int opt;
    optind = 1;
    while ((opt = getopt(filtered_argc, filtered_args.data(), "d:0")) != -1) {
        switch (opt) {
            case 'd':
                base_dir = optarg;
                break;
            case '0':
                null_separator = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Get pattern
    if (optind >= filtered_argc) {
        std::cerr << "aglob: missing pattern\n";
        print_usage(argv[0]);
        return 1;
    }
    pattern = filtered_args[optind++];
    
    // Override base_dir if provided as positional argument
    if (optind < filtered_argc) {
        base_dir = filtered_args[optind];
    }
    
    write_debug("aglob starting: pattern='" + pattern + "', base_dir='" + base_dir + "'");
    
    // Create glob engine and match
    GlobEngine engine(opts);
    auto [matches, error] = engine.match(base_dir, pattern);
    
    if (error != GlobError::OK) {
        std::cerr << "aglob: " << glob_error_string(error) << "\n";
        write_telemetry(0, error);
        return static_cast<int>(error);
    }
    
    // Print matches
    for (const auto& path : matches) {
        std::cout << path.string();
        if (null_separator) {
            std::cout << '\0';
        } else {
            std::cout << '\n';
        }
    }
    
    write_telemetry(matches.size(), GlobError::OK);
    
    return matches.empty() ? 1 : 0;
}
