/**
 * agrep_cli.cpp
 * Aria Grep - Six-Stream Pattern Search Utility
 *
 * A grep replacement that fully utilizes the Aria six-stream architecture:
 * - FD 0 (stdin):   Standard input (for searching stdin)
 * - FD 1 (stdout):  Matched lines with highlighting
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Binary Match structures for pipeline composition
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "agrep/pattern_search.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace aria::utils::agrep;

static bool debug_mode = false;
constexpr int FD_STDDBG = 3;
constexpr int FD_STDDATO = 5;

// ANSI color codes
constexpr const char* COLOR_RED = "\033[1;31m";
constexpr const char* COLOR_PURPLE = "\033[1;35m";
constexpr const char* COLOR_CYAN = "\033[1;36m";
constexpr const char* COLOR_RESET = "\033[0m";

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]... PATTERN [FILE]...\n";
    std::cerr << "Search for PATTERN in each FILE.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 0 (stdin):  Read input when no files specified\n";
    std::cerr << "  FD 1 (stdout): Matched lines (highlighted)\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n";
    std::cerr << "  FD 5 (stddato): Binary Match structures\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -i            ignore case distinctions\n";
    std::cerr << "  -v            select non-matching lines\n";
    std::cerr << "  -c            print only a count of matching lines\n";
    std::cerr << "  -n            print line number with output\n";
    std::cerr << "  -w            match whole words only\n";
    std::cerr << "  -A NUM        print NUM lines of trailing context\n";
    std::cerr << "  -B NUM        print NUM lines of leading context\n";
    std::cerr << "  -C NUM        print NUM lines of output context\n";
    std::cerr << "  --color=WHEN  use markers (always, never, auto)\n";
    std::cerr << "  --debug       output debug telemetry to FD 3\n";
    std::cerr << "  --help        display this help and exit\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"agrep\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(const SearchStats& stats) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"agrep\""
                      ",\"bytes_scanned\":" + std::to_string(stats.bytes_scanned) +
                      ",\"matches_found\":" + std::to_string(stats.matches_found) +
                      ",\"files_searched\":" + std::to_string(stats.files_searched) +
                      ",\"files_skipped\":" + std::to_string(stats.files_skipped) +
                      ",\"duration_ms\":" + std::to_string(stats.duration_ms) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void highlight_match(const std::string& line, size_t match_pos, size_t match_len, bool use_color) {
    if (!use_color || match_pos >= line.size()) {
        std::cout << line;
        return;
    }
    
    // Print before match
    std::cout << line.substr(0, match_pos);
    
    // Print match with color
    std::cout << COLOR_RED;
    std::cout << line.substr(match_pos, match_len);
    std::cout << COLOR_RESET;
    
    // Print after match
    if (match_pos + match_len < line.size()) {
        std::cout << line.substr(match_pos + match_len);
    }
}

int main(int argc, char* argv[]) {
    SearchOptions opts;
    std::vector<std::string> files;
    std::string pattern;
    
    // Check for --debug, --help, and --color first
    std::vector<char*> filtered_args;
    filtered_args.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strncmp(argv[i], "--color=", 8) == 0) {
            const char* when = argv[i] + 8;
            if (strcmp(when, "never") == 0) {
                opts.color_output = false;
            } else if (strcmp(when, "always") == 0) {
                opts.color_output = true;
            } else if (strcmp(when, "auto") == 0) {
                opts.color_output = isatty(STDOUT_FILENO);
            }
        } else {
            filtered_args.push_back(argv[i]);
        }
    }
    
    int filtered_argc = filtered_args.size();
    
    // Parse options
    int opt;
    optind = 1;
    while ((opt = getopt(filtered_argc, filtered_args.data(), "ivcnwA:B:C:")) != -1) {
        switch (opt) {
            case 'i':
                opts.case_insensitive = true;
                break;
            case 'v':
                opts.invert_match = true;
                break;
            case 'c':
                opts.count_only = true;
                break;
            case 'n':
                opts.line_numbers = true;
                break;
            case 'w':
                opts.whole_word = true;
                break;
            case 'A':
                opts.context_after = std::stoul(optarg);
                break;
            case 'B':
                opts.context_before = std::stoul(optarg);
                break;
            case 'C':
                opts.context_before = opts.context_after = std::stoul(optarg);
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Get pattern (first non-option argument)
    if (optind >= filtered_argc) {
        std::cerr << "agrep: missing pattern\n";
        print_usage(argv[0]);
        return 1;
    }
    pattern = filtered_args[optind++];
    
    // Get files (remaining arguments)
    for (int i = optind; i < filtered_argc; i++) {
        files.push_back(filtered_args[i]);
    }
    
    write_debug("agrep starting: pattern='" + pattern + "', files=" + std::to_string(files.size()));
    
    SearchStats total_stats;
    bool found_matches = false;
    
    // Match callback for printing results
    auto print_match = [&](const Match& match, const std::string& filename) {
        found_matches = true;
        
        if (opts.count_only) {
            return;  // Don't print individual matches in count mode
        }
        
        // Print filename if multiple files
        if (files.size() > 1 && !filename.empty()) {
            if (opts.color_output) std::cout << COLOR_PURPLE;
            std::cout << filename;
            if (opts.color_output) std::cout << COLOR_RESET;
            std::cout << ":";
        }
        
        // Print line number
        if (opts.line_numbers) {
            if (opts.color_output) std::cout << COLOR_CYAN;
            std::cout << match.line_number;
            if (opts.color_output) std::cout << COLOR_RESET;
            std::cout << ":";
        }
        
        // Print line with highlighting
        highlight_match(match.content, match.column, match.match_len, opts.color_output);
        std::cout << "\n";
    };
    
    if (files.empty()) {
        // Search stdin (not yet implemented in library - would need stdin support)
        std::cerr << "agrep: stdin search not yet implemented\n";
        return 1;
    } else {
        // Search files
        auto result = search_files(files, pattern, opts, 
                                   [&](const Match& m, const std::string& f) {
                                       print_match(m, f);
                                   });
        
        if (result.err()) {
            std::cerr << "agrep: search failed with error code " 
                     << static_cast<int>(result.error) << "\n";
            return static_cast<int>(result.error);
        }
        
        total_stats = result.value;
    }
    
    // Print count if requested
    if (opts.count_only) {
        std::cout << total_stats.matches_found << "\n";
    }
    
    write_telemetry(total_stats);
    
    // Return 0 if matches found, 1 if no matches (grep convention)
    return found_matches ? 0 : 1;
}
