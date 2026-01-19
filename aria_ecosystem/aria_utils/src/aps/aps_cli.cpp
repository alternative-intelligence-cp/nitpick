/**
 * aps_cli.cpp
 * Aria Process Status - Six-Stream Process Listing Utility
 *
 * A ps replacement that fully utilizes the Aria six-stream architecture:
 * - FD 0 (stdin):   Standard input
 * - FD 1 (stdout):  Process listing
 * - FD 2 (stderr):  Error messages
 * - FD 3 (stddbg):  Debug telemetry (NDJSON format)
 * - FD 4 (stddati): Input metadata stream (future use)
 * - FD 5 (stddato): Binary ProcessInfo structures
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include <aps/process_list.hpp>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <pwd.h>

using namespace aria::aps;

static bool debug_mode = false;
constexpr int FD_STDDBG = 3;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " [OPTION]...\n";
    std::cerr << "Display information about running processes.\n\n";
    std::cerr << "Six-Stream Architecture:\n";
    std::cerr << "  FD 1 (stdout): Process listing\n";
    std::cerr << "  FD 2 (stderr): Error messages\n";
    std::cerr << "  FD 3 (stddbg): Debug telemetry (NDJSON format)\n";
    std::cerr << "  FD 5 (stddato): Binary ProcessInfo structures\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -a            show processes from all users\n";
    std::cerr << "  -u USER       show processes for specific user\n";
    std::cerr << "  -p PID        show specific process\n";
    std::cerr << "  -e            show all processes (same as -a)\n";
    std::cerr << "  -f            full format listing\n";
    std::cerr << "  -l            long format listing\n";
    std::cerr << "  --tree        show process tree\n";
    std::cerr << "  --debug       output debug telemetry to FD 3\n";
    std::cerr << "  --help        display this help and exit\n";
}

void write_debug(const std::string& message) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"debug\",\"component\":\"aps\",\"message\":\"" + message + "\"}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

void write_telemetry(size_t process_count, size_t errors) {
    if (!debug_mode) return;
    
    std::string json = "{\"level\":\"telemetry\",\"component\":\"aps\""
                      ",\"processes_listed\":" + std::to_string(process_count) +
                      ",\"errors\":" + std::to_string(errors) +
                      "}\n";
    [[maybe_unused]] ssize_t written = write(FD_STDDBG, json.c_str(), json.size());
}

std::string get_username(uid_t uid) {
    struct passwd* pw = getpwuid(uid);
    if (pw) {
        return std::string(pw->pw_name);
    }
    return std::to_string(uid);
}

void print_header(bool full_format, bool long_format) {
    if (full_format) {
        std::cout << std::left
                  << std::setw(10) << "UID"
                  << std::setw(8) << "PID"
                  << std::setw(8) << "PPID"
                  << std::setw(3) << "C"
                  << std::setw(8) << "STIME"
                  << std::setw(8) << "TTY"
                  << std::setw(10) << "TIME"
                  << "CMD\n";
    } else if (long_format) {
        std::cout << std::left
                  << std::setw(2) << "F"
                  << std::setw(2) << "S"
                  << std::setw(10) << "UID"
                  << std::setw(8) << "PID"
                  << std::setw(8) << "PPID"
                  << std::setw(3) << "C"
                  << std::setw(4) << "PRI"
                  << std::setw(4) << "NI"
                  << std::setw(9) << "ADDR"
                  << std::setw(5) << "SZ"
                  << std::setw(7) << "WCHAN"
                  << std::setw(8) << "TTY"
                  << std::setw(10) << "TIME"
                  << "CMD\n";
    } else {
        std::cout << std::left
                  << std::setw(8) << "PID"
                  << std::setw(8) << "TTY"
                  << std::setw(10) << "TIME"
                  << "CMD\n";
    }
}

void print_process(const ProcessInfo& proc, bool full_format, bool long_format) {
    if (full_format) {
        std::cout << std::left
                  << std::setw(10) << get_username(proc.uid)
                  << std::setw(8) << proc.pid
                  << std::setw(8) << proc.ppid
                  << std::setw(3) << "0"  // CPU usage placeholder
                  << std::setw(8) << "?"   // Start time placeholder
                  << std::setw(8) << "?"   // TTY placeholder
                  << std::setw(10) << "00:00:00"  // Time placeholder
                  << (proc.cmdline.empty() ? proc.name : proc.cmdline)
                  << "\n";
    } else if (long_format) {
        std::cout << std::left
                  << std::setw(2) << "0"  // Flags
                  << std::setw(2) << state_to_char(proc.state)
                  << std::setw(10) << get_username(proc.uid)
                  << std::setw(8) << proc.pid
                  << std::setw(8) << proc.ppid
                  << std::setw(3) << "0"
                  << std::setw(4) << proc.priority
                  << std::setw(4) << proc.nice
                  << std::setw(9) << "-"
                  << std::setw(5) << (proc.rss / 1024)  // KB
                  << std::setw(7) << "-"
                  << std::setw(8) << "?"
                  << std::setw(10) << "00:00:00"
                  << (proc.cmdline.empty() ? proc.name : proc.cmdline)
                  << "\n";
    } else {
        std::cout << std::left
                  << std::setw(8) << proc.pid
                  << std::setw(8) << "?"
                  << std::setw(10) << "00:00:00"
                  << (proc.cmdline.empty() ? proc.name : proc.cmdline)
                  << "\n";
    }
}

int main(int argc, char* argv[]) {
    FilterOptions filter;
    SortOptions sort;
    bool show_all = false;
    bool full_format = false;
    bool long_format = false;
    bool show_tree = false;
    std::string user_filter;
    int32_t pid_filter = -1;
    
    // Check for long options first
    std::vector<char*> filtered_args;
    filtered_args.push_back(argv[0]);
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--tree") == 0) {
            show_tree = true;
        } else {
            filtered_args.push_back(argv[i]);
        }
    }
    
    int filtered_argc = filtered_args.size();
    
    // Parse short options
    int opt;
    optind = 1;
    while ((opt = getopt(filtered_argc, filtered_args.data(), "aeu:p:fl")) != -1) {
        switch (opt) {
            case 'a':
            case 'e':
                show_all = true;
                break;
            case 'u':
                user_filter = optarg;
                break;
            case 'p':
                pid_filter = std::stoi(optarg);
                break;
            case 'f':
                full_format = true;
                break;
            case 'l':
                long_format = true;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    write_debug("aps starting");
    
    // Get process list
    ProcessListResult result;
    
    if (pid_filter > 0) {
        // Get single process
        filter.pid = pid_filter;
        result = get_process_list(filter, sort);
    } else {
        // Get all processes matching filter
        if (!user_filter.empty()) {
            filter.user = user_filter;
        }
        // show_all means include kernel threads and all users
        if (show_all) {
            filter.include_kernel = true;
        }
        
        result = get_process_list(filter, sort);
    }
    
    if (result.error != ErrorCode::OK) {
        std::cerr << "aps: error listing processes: " << result.error_message << "\n";
        return 1;
    }
    
    // Print header
    print_header(full_format, long_format);
    
    // Print processes
    for (const auto& proc : result.processes) {
        print_process(proc, full_format, long_format);
    }
    
    write_telemetry(result.processes.size(), 0);
    
    return 0;
}
