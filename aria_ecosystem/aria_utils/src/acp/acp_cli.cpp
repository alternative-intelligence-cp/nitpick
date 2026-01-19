/**
 * acp_cli.cpp
 * Aria Copy - Six-Stream File Copy Utility
 *
 * A cp replacement that fully utilizes the Aria six-stream architecture:
 * - FD 0 (stdin):   Standard input
 * - FD 1 (stdout):  Normal output (file paths, status)
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Output metadata stream (future use)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include <acp/file_copy.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/stat.h>

using namespace aria::acp;

static bool debug_mode = false;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]... SOURCE... DEST\n";
    std::cerr << "Copy SOURCE to DEST, or multiple SOURCE(s) to directory DEST.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout): Progress and status messages\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n";
    std::cerr << "  FD 5 (stddato): Binary metadata stream (future)\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -r, -R        copy directories recursively\n";
    std::cerr << "  -f            force overwrite of existing files\n";
    std::cerr << "  -i            prompt before overwrite (interactive)\n";
    std::cerr << "  -n            do not overwrite existing files\n";
    std::cerr << "  -u            copy only when SOURCE is newer\n";
    std::cerr << "  -v            verbose output\n";
    std::cerr << "  -p            preserve mode, ownership, timestamps\n";
    std::cerr << "  -L            dereference symlinks (follow)\n";
    std::cerr << "  -x            do not cross filesystem boundaries\n";
    std::cerr << "  --debug       output debug telemetry to FD 3\n";
    std::cerr << "  --help        display this help and exit\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"acp\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(const CopyResult& result) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"acp\""
                      ",\"bytes_copied\":" + std::to_string(result.bytes_copied) +
                      ",\"files_copied\":" + std::to_string(result.files_copied) +
                      ",\"dirs_created\":" + std::to_string(result.dirs_created) +
                      ",\"symlinks_copied\":" + std::to_string(result.symlinks_copied) +
                      ",\"files_skipped\":" + std::to_string(result.files_skipped) +
                      ",\"elapsed_ms\":" + std::to_string(result.elapsed_ms) +
                      ",\"throughput_mbps\":" + std::to_string(result.throughput_mbps) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

int main(int argc, char* argv[]) {
    CopyOptions opts;
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
    while ((opt = getopt(filtered_argc, filtered_args.data(), "rRfinuvpLx")) != -1) {
        switch (opt) {
            case 'r':
            case 'R':
                opts.recursive = true;
                break;
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
            case 'u':
                opts.update = true;
                break;
            case 'v':
                opts.verbose = true;
                break;
            case 'p':
                opts.preserve_mode = true;
                opts.preserve_ownership = true;
                opts.preserve_timestamps = true;
                break;
            case 'L':
                opts.dereference = true;
                opts.preserve_links = false;
                break;
            case 'x':
                opts.one_filesystem = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Collect source files (from filtered args)
    for (int i = optind; i < filtered_argc - 1; i++) {
        sources.push_back(filtered_args[i]);
    }
    
    // Get destination
    if (optind >= filtered_argc || sources.empty()) {
        std::cerr << "acp: missing file operand\n";
        print_usage(argv[0]);
        return 1;
    }
    
    std::string dest = filtered_args[filtered_argc - 1];
    
    write_debug("acp starting with " + std::to_string(sources.size()) + " source(s)");
    
    CopyResult result;
    
    // Progress callback for verbose mode
    ProgressCallback progress_cb = nullptr;
    if (opts.verbose) {
        progress_cb = [](const CopyProgress& prog) -> bool {
            std::cout << prog.current_file << "\n";
            return true;  // Continue copying
        };
    }
    
    // Single source to destination
    if (sources.size() == 1) {
        struct stat st;
        bool dest_is_dir = (stat(dest.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
        
        if (opts.recursive && is_directory(sources[0])) {
            // Recursive directory copy
            result = copy_directory(sources[0], dest, opts, progress_cb);
        } else {
            // Single file copy
            result = copy_file(sources[0], dest, opts, progress_cb);
        }
    } 
    // Multiple sources to directory
    else {
        struct stat st;
        if (stat(dest.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
            std::cerr << "acp: target '" << dest << "' is not a directory\n";
            return 1;
        }
        
        result = copy_files(sources, dest, opts, progress_cb);
    }
    
    // Write telemetry
    write_telemetry(result);
    
    // Handle errors
    if (!result.is_ok()) {
        std::string err_msg = "acp: " + std::string(error_to_string(result.error));
        if (!result.error_path.empty()) {
            err_msg += ": " + result.error_path;
        }
        if (!result.error_message.empty()) {
            err_msg += ": " + result.error_message;
        }
        std::cerr << err_msg << "\n";
        
        if (debug_mode) {
            std::string json = "{\"level\":\"error\",\"component\":\"acp\",\"error_code\":" + 
                              std::to_string(static_cast<int>(result.error)) +
                              ",\"error_message\":\"" + result.error_message + "\"}\n";
            [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
        }
        
        return static_cast<int>(result.error);
    }
    
    if (opts.verbose) {
        std::cout << "Copied " << result.files_copied << " file(s), " 
                  << result.bytes_copied << " bytes\n";
    }
    
    return 0;
}
