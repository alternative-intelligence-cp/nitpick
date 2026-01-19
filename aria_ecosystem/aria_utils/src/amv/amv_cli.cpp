/**
 * amv_cli.cpp
 * Aria Move - Six-Stream File Move/Rename Utility
 *
 * A mv replacement that fully utilizes the Aria six-stream architecture:
 * - FD 0 (stdin):   Standard input
 * - FD 1 (stdout):  Normal output (file paths, status)
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Output metadata stream (future use)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include <amv/file_move.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>

using namespace aria::amv;

static bool debug_mode = false;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]... SOURCE... DEST\n";
    std::cerr << "Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout): Progress and status messages\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n";
    std::cerr << "  FD 5 (stddato): Binary metadata stream (future)\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -f            force overwrite of existing files\n";
    std::cerr << "  -i            prompt before overwrite (interactive)\n";
    std::cerr << "  -n            do not overwrite existing files\n";
    std::cerr << "  -v            verbose output\n";
    std::cerr << "  -b            create backup of each existing destination file\n";
    std::cerr << "  --debug       output debug telemetry to FD 3\n";
    std::cerr << "  --help        display this help and exit\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"amv\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(const MoveResult& result) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"amv\""
                      ",\"files_moved\":" + std::to_string(result.files_moved) +
                      ",\"dirs_moved\":" + std::to_string(result.dirs_moved) +
                      ",\"bytes_moved\":" + std::to_string(result.bytes_moved) +
                      ",\"files_skipped\":" + std::to_string(result.files_skipped) +
                      ",\"used_rename\":" + (result.used_rename ? "true" : "false") +
                      ",\"used_copy\":" + (result.used_copy ? "true" : "false") +
                      ",\"elapsed_ms\":" + std::to_string(result.elapsed_ms) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

int main(int argc, char* argv[]) {
    MoveOptions opts;
    std::vector<std::string> sources;
    
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
    while ((opt = getopt(filtered_argc, filtered_args.data(), "finvb")) != -1) {
        switch (opt) {
            case 'f':
                opts.force = true;
                opts.interactive = false;
                opts.no_clobber = false;
                break;
            case 'i':
                opts.interactive = true;
                opts.force = false;
                opts.no_clobber = false;
                break;
            case 'n':
                opts.no_clobber = true;
                opts.force = false;
                opts.interactive = false;
                break;
            case 'v':
                opts.verbose = true;
                break;
            case 'b':
                opts.backup = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Collect source files
    for (int i = optind; i < filtered_argc - 1; i++) {
        sources.push_back(filtered_args[i]);
    }
    
    // Get destination
    if (optind >= filtered_argc || sources.empty()) {
        std::cerr << "amv: missing file operand\n";
        print_usage(argv[0]);
        return 1;
    }
    
    std::string dest = filtered_args[filtered_argc - 1];
    
    write_debug("amv starting with " + std::to_string(sources.size()) + " source(s)");
    
    MoveResult result;
    
    // Single source to destination (rename or move)
    if (sources.size() == 1) {
        result = move_file(sources[0], dest, opts);
        
        if (opts.verbose && result.is_ok()) {
            std::cout << "'" << sources[0] << "' -> '" << dest << "'\n";
        }
    } 
    // Multiple sources to directory
    else {
        struct stat st;
        if (stat(dest.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            std::cerr << "amv: target '" << dest << "' is not a directory\n";
            return 1;
        }
        
        result = move_files(sources, dest, opts);
        
        if (opts.verbose && result.is_ok()) {
            for (const auto& src : sources) {
                std::cout << "'" << src << "' -> '" << dest << "/'\n";
            }
        }
    }
    
    // Write telemetry
    write_telemetry(result);
    
    // Handle errors
    if (!result.is_ok()) {
        std::string err_msg = "amv: " + std::string(error_to_string(result.error));
        if (!result.error_path.empty()) {
            err_msg += ": " + result.error_path;
        }
        if (!result.error_message.empty()) {
            err_msg += ": " + result.error_message;
        }
        std::cerr << err_msg << "\n";
        
        if (debug_mode) {
            std::string json = "{\"level\":\"error\",\"component\":\"amv\",\"error_code\":" + 
                              std::to_string(static_cast<int>(result.error)) +
                              ",\"error_message\":\"" + result.error_message + "\"}\n";
            [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
        }
        
        return static_cast<int>(result.error);
    }
    
    return 0;
}
