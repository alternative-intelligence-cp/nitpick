/**
 * aps_demo.cpp
 * Interactive demonstration of the aps (Aria Process Status) utility
 *
 * Shows process enumeration with filtering and sorting
 */

#include "aps/process_list.hpp"
#include <iostream>
#include <iomanip>

using namespace aria::aps;

// Helper: Print section header
void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Helper: Format memory size
std::string format_mem(uint64_t bytes) {
    if (bytes < 1024) return std::to_string(bytes) + " B";
    if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
    return std::to_string(bytes / (1024 * 1024)) + " MB";
}

// Demo 1: List all processes
void demo_list_all() {
    print_section("Demo 1: List All Processes (top 10)");

    FilterOptions filter;
    filter.include_kernel = false;  // Skip kernel threads

    SortOptions sort;
    sort.field = SortField::PID;
    sort.descending = false;

    auto result = get_process_list(filter, sort);

    if (result.error != ErrorCode::OK) {
        std::cout << "Error listing processes\n";
        return;
    }

    std::cout << "Total processes: " << result.processes.size() << "\n\n";
    std::cout << std::left
              << std::setw(8) << "PID"
              << std::setw(8) << "PPID"
              << std::setw(6) << "State"
              << std::setw(25) << "Name"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    size_t count = std::min(result.processes.size(), size_t(10));
    for (size_t i = 0; i < count; ++i) {
        const auto& proc = result.processes[i];
        std::cout << std::setw(8) << proc.pid
                  << std::setw(8) << proc.ppid
                  << std::setw(6) << state_to_char(proc.state)
                  << std::setw(25) << proc.name
                  << "\n";
    }
}

// Demo 2: Find processes by name
void demo_filter_by_name() {
    print_section("Demo 2: Find Processes by Name (bash)");

    FilterOptions filter;
    filter.name = "bash";
    filter.include_kernel = false;

    auto result = get_process_list(filter);

    std::cout << "Found " << result.processes.size() << " bash processes\n\n";

    for (const auto& proc : result.processes) {
        std::cout << "  PID " << proc.pid 
                  << " - User " << proc.uid
                  << " - " << proc.cmdline << "\n";
    }
}

// Demo 3: Sort by memory usage
void demo_sort_by_memory() {
    print_section("Demo 3: Top 10 Memory Consumers");

    FilterOptions filter;
    filter.include_kernel = false;

    SortOptions sort;
    sort.field = SortField::MEMORY;
    sort.descending = true;

    auto result = get_process_list(filter, sort);

    std::cout << std::left
              << std::setw(8) << "PID"
              << std::setw(12) << "RSS"
              << std::setw(10) << "VSize"
              << std::setw(25) << "Name"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    size_t count = std::min(result.processes.size(), size_t(10));
    for (size_t i = 0; i < count; ++i) {
        const auto& proc = result.processes[i];
        std::cout << std::setw(8) << proc.pid
                  << std::setw(12) << format_mem(proc.rss * 4096)  // Pages to bytes
                  << std::setw(10) << format_mem(proc.vsize)
                  << std::setw(25) << proc.name
                  << "\n";
    }
}

// Demo 4: Sort by CPU usage
void demo_sort_by_cpu() {
    print_section("Demo 4: Top 10 CPU Consumers");

    FilterOptions filter;
    filter.include_kernel = false;

    SortOptions sort;
    sort.field = SortField::CPU;
    sort.descending = true;

    auto result = get_process_list(filter, sort);

    std::cout << std::left
              << std::setw(8) << "PID"
              << std::setw(8) << "CPU %"
              << std::setw(30) << "Name"
              << "\n";
    std::cout << std::string(50, '-') << "\n";

    size_t count = std::min(result.processes.size(), size_t(10));
    for (size_t i = 0; i < count; ++i) {
        const auto& proc = result.processes[i];
        std::cout << std::setw(8) << proc.pid
                  << std::setw(8) << std::fixed << std::setprecision(1) << proc.cpu_percent
                  << std::setw(30) << proc.name
                  << "\n";
    }
}

// Demo 5: Get specific process info
void demo_get_process_info() {
    print_section("Demo 5: Detailed Process Information");

    // Get info for current process
    int32_t pid = get_current_pid();

    auto [info, error] = get_process_info(pid);

    if (error != ErrorCode::OK) {
        std::cout << "Error getting process info\n";
        return;
    }

    std::cout << "Process " << info.pid << " Details:\n\n";
    std::cout << "  Name:       " << info.name << "\n";
    std::cout << "  Cmdline:    " << info.cmdline << "\n";
    std::cout << "  Executable: " << info.exe << "\n";
    std::cout << "  CWD:        " << info.cwd << "\n";
    std::cout << "  State:      " << state_to_char(info.state) << "\n";
    std::cout << "  PPID:       " << info.ppid << "\n";
    std::cout << "  UID:        " << info.uid << "\n";
    std::cout << "  Threads:    " << info.threads << "\n";
    std::cout << "  Nice:       " << info.nice << "\n";
    std::cout << "  RSS:        " << format_mem(info.rss * 4096) << "\n";
    std::cout << "  VSize:      " << format_mem(info.vsize) << "\n";
}

// Main program
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║    Aria Process Status (aps) - Interactive Demonstration  ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  Cross-platform process enumeration with filtering        ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_list_all();
        demo_filter_by_name();
        demo_sort_by_memory();
        demo_sort_by_cpu();
        demo_get_process_info();

        std::cout << "\n✓ All 5 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
