/**
 * awc_cli.cpp
 * Aria Word Count - Six-Stream Word Count Utility
 *
 * A wc replacement that fully utilizes the Aria six-stream architecture:
 * - FD 0 (stdin):   Standard input (for reading when no files specified)
 * - FD 1 (stdout):  Count results
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Output metadata stream (future use)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include <awc/word_count.hpp>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

using namespace aria::awc;

static bool debug_mode = false;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]... [FILE]...\n";
    std::cerr << "Count lines, words, and bytes in files.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 0 (stdin):  Read input when no files specified\n";
    std::cerr << "  FD 1 (stdout): Count results\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -l            print the newline counts\n";
    std::cerr << "  -w            print the word counts\n";
    std::cerr << "  -m            print the character counts (UTF-8 aware)\n";
    std::cerr << "  -c            print the byte counts\n";
    std::cerr << "  -L            print the maximum display width\n";
    std::cerr << "  --debug       output debug telemetry to FD 3\n";
    std::cerr << "  --help        display this help and exit\n\n";
    std::cerr << "If no options are specified, -l -w -c are assumed.\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"awc\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(const TotalResult& total) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"awc\""
                      ",\"total_lines\":" + std::to_string(total.total_lines) +
                      ",\"total_words\":" + std::to_string(total.total_words) +
                      ",\"total_chars\":" + std::to_string(total.total_chars) +
                      ",\"total_bytes\":" + std::to_string(total.total_bytes) +
                      ",\"files_processed\":" + std::to_string(total.files_processed) +
                      ",\"files_failed\":" + std::to_string(total.files_failed) +
                      ",\"elapsed_ms\":" + std::to_string(total.total_elapsed_ms) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void print_result(const CountResult& result, const CountOptions& opts, int field_width) {
    if (opts.count_lines) {
        std::cout << std::setw(field_width) << result.lines;
    }
    if (opts.count_words) {
        std::cout << std::setw(field_width) << result.words;
    }
    if (opts.count_chars) {
        std::cout << std::setw(field_width) << result.chars;
    }
    if (opts.count_bytes) {
        std::cout << std::setw(field_width) << result.bytes;
    }
    if (opts.count_max_line_length) {
        std::cout << std::setw(field_width) << result.max_line_length;
    }
    
    if (!result.filename.empty()) {
        std::cout << " " << result.filename;
    }
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    CountOptions opts;
    bool options_specified = false;
    std::vector<std::string> filenames;
    
    // Check for --debug and --help first, filter them out for getopt
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
    optind = 1;  // Reset getopt
    while ((opt = getopt(filtered_argc, filtered_args.data(), "lwmcL")) != -1) {
        switch (opt) {
            case 'l':
                if (!options_specified) {
                    // First option specified, clear defaults
                    opts.count_lines = false;
                    opts.count_words = false;
                    opts.count_bytes = false;
                    options_specified = true;
                }
                opts.count_lines = true;
                break;
            case 'w':
                if (!options_specified) {
                    opts.count_lines = false;
                    opts.count_words = false;
                    opts.count_bytes = false;
                    options_specified = true;
                }
                opts.count_words = true;
                break;
            case 'm':
                if (!options_specified) {
                    opts.count_lines = false;
                    opts.count_words = false;
                    opts.count_bytes = false;
                    options_specified = true;
                }
                opts.count_chars = true;
                opts.count_bytes = false;  // -m and -c are mutually exclusive
                break;
            case 'c':
                if (!options_specified) {
                    opts.count_lines = false;
                    opts.count_words = false;
                    opts.count_bytes = false;
                    options_specified = true;
                }
                opts.count_bytes = true;
                opts.count_chars = false;  // -m and -c are mutually exclusive
                break;
            case 'L':
                if (!options_specified) {
                    opts.count_lines = false;
                    opts.count_words = false;
                    opts.count_bytes = false;
                    options_specified = true;
                }
                opts.count_max_line_length = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Collect filenames
    for (int i = optind; i < filtered_argc; i++) {
        filenames.push_back(filtered_args[i]);
    }
    
    write_debug("awc starting with " + std::to_string(filenames.size()) + " file(s)");
    
    // Calculate field width (7 digits should be enough for most files)
    int field_width = 7;
    
    // Process files or stdin
    if (filenames.empty()) {
        // Read from stdin
        CountResult result = count_stdin(opts);
        
        if (!result.is_ok()) {
            std::cerr << "awc: " << error_to_string(result.error) << ": " << result.error_message << "\n";
            return static_cast<int>(result.error);
        }
        
        print_result(result, opts, field_width);
        
        if (debug_mode) {
            TotalResult total;
            total.file_results.push_back(result);
            total.total_lines = result.lines;
            total.total_words = result.words;
            total.total_chars = result.chars;
            total.total_bytes = result.bytes;
            total.files_processed = 1;
            total.total_elapsed_ms = result.elapsed_ms;
            write_telemetry(total);
        }
    } else {
        // Process files
        TotalResult total = count_files(filenames, opts);
        
        // Print individual file results
        for (const auto& result : total.file_results) {
            if (result.is_ok()) {
                print_result(result, opts, field_width);
            } else {
                std::cerr << "awc: " << error_to_string(result.error) << ": " << result.error_message << "\n";
            }
        }
        
        // Print total if multiple files
        if (filenames.size() > 1) {
            CountResult total_result;
            total_result.filename = "total";
            total_result.lines = total.total_lines;
            total_result.words = total.total_words;
            total_result.chars = total.total_chars;
            total_result.bytes = total.total_bytes;
            total_result.max_line_length = total.total_max_line_length;
            print_result(total_result, opts, field_width);
        }
        
        write_telemetry(total);
        
        if (total.files_failed > 0) {
            return 1;
        }
    }
    
    return 0;
}
