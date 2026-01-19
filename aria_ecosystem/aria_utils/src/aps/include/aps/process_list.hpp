/**
 * process_list.hpp
 * Cross-platform process listing library
 *
 * Provides process enumeration and information retrieval for
 * the Aria `aps` utility. Uses /proc on Linux, sysctl on macOS/BSD,
 * and Windows APIs on Windows.
 *
 * Features:
 * - List all running processes
 * - Filter by name, user, state
 * - Get detailed process information
 * - Tree view (parent-child relationships)
 * - Resource usage (CPU, memory)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_APS_PROCESS_LIST_HPP
#define ARIA_APS_PROCESS_LIST_HPP

#include <string>
#include <vector>
#include <cstdint>
#include <optional>
#include <functional>

namespace aria {
namespace aps {

// ============================================================================
// Error Codes
// ============================================================================

enum class ErrorCode {
    OK = 0,
    PERMISSION_DENIED = 1,
    PROCESS_NOT_FOUND = 2,
    INVALID_PID = 3,
    READ_ERROR = 4,
    SYSTEM_ERROR = 5,
    UNKNOWN_ERROR = 99
};

// ============================================================================
// Process State
// ============================================================================

enum class ProcessState {
    RUNNING,        // R - Running or runnable
    SLEEPING,       // S - Interruptible sleep
    DISK_SLEEP,     // D - Uninterruptible disk sleep
    STOPPED,        // T - Stopped (signal or tracing)
    ZOMBIE,         // Z - Zombie (terminated but not reaped)
    DEAD,           // X - Dead
    IDLE,           // I - Idle kernel thread
    UNKNOWN
};

/**
 * Convert process state to single character (like ps)
 */
char state_to_char(ProcessState state);

/**
 * Parse state from character
 */
ProcessState char_to_state(char c);

// ============================================================================
// Process Information
// ============================================================================

/**
 * Basic process information
 */
struct ProcessInfo {
    int32_t pid;                    // Process ID
    int32_t ppid;                   // Parent process ID
    int32_t pgid;                   // Process group ID
    int32_t sid;                    // Session ID

    uint32_t uid;                   // User ID
    uint32_t gid;                   // Group ID
    uint32_t euid;                  // Effective user ID
    uint32_t egid;                  // Effective group ID

    std::string name;               // Process name (comm)
    std::string cmdline;            // Full command line
    std::string exe;                // Path to executable
    std::string cwd;                // Current working directory

    ProcessState state;             // Current state
    int32_t nice;                   // Nice value (-20 to 19)
    int32_t priority;               // Scheduling priority
    int32_t threads;                // Number of threads

    // Timing (all in clock ticks, use sysconf(_SC_CLK_TCK) to convert)
    uint64_t utime;                 // User mode time (ticks)
    uint64_t stime;                 // Kernel mode time (ticks)
    uint64_t start_time;            // Process start time (since boot)

    // Memory (in bytes)
    uint64_t vsize;                 // Virtual memory size
    uint64_t rss;                   // Resident set size (pages)
    uint64_t shared;                // Shared memory
    uint64_t text;                  // Code size
    uint64_t data;                  // Data + stack

    // CPU usage (calculated, 0-100 per CPU)
    double cpu_percent;

    // I/O (Linux specific)
    uint64_t read_bytes;            // Bytes read
    uint64_t write_bytes;           // Bytes written

    ProcessInfo();
};

// ============================================================================
// Filter Options
// ============================================================================

/**
 * Options for filtering process list
 */
struct FilterOptions {
    std::optional<int32_t> pid;         // Specific PID
    std::optional<int32_t> ppid;        // Filter by parent PID
    std::optional<uint32_t> uid;        // Filter by user ID
    std::optional<std::string> user;    // Filter by username
    std::optional<std::string> name;    // Filter by process name (partial match)
    std::optional<std::string> cmdline; // Filter by cmdline (partial match)
    std::optional<ProcessState> state;  // Filter by state

    bool include_threads;               // Include threads (Linux: /proc/[pid]/task)
    bool include_kernel;                // Include kernel threads

    FilterOptions();
};

// ============================================================================
// Sort Options
// ============================================================================

enum class SortField {
    PID,
    PPID,
    NAME,
    USER,
    STATE,
    CPU,
    MEMORY,
    START_TIME,
    THREADS
};

struct SortOptions {
    SortField field;
    bool descending;

    SortOptions() : field(SortField::PID), descending(false) {}
};

// ============================================================================
// Process List Result
// ============================================================================

struct ProcessListResult {
    std::vector<ProcessInfo> processes;
    ErrorCode error;
    std::string error_message;

    ProcessListResult() : error(ErrorCode::OK) {}
};

// ============================================================================
// Process Tree Node
// ============================================================================

struct ProcessTreeNode {
    ProcessInfo info;
    std::vector<ProcessTreeNode> children;

    ProcessTreeNode() = default;
    explicit ProcessTreeNode(const ProcessInfo& info) : info(info) {}
};

// ============================================================================
// Main API
// ============================================================================

/**
 * Get list of all processes
 *
 * @param filter Optional filter criteria
 * @param sort Optional sort options
 * @return List of processes or error
 */
ProcessListResult get_process_list(const FilterOptions& filter = FilterOptions(),
                                    const SortOptions& sort = SortOptions());

/**
 * Get information for a specific process
 *
 * @param pid Process ID
 * @return Process information or error
 */
std::tuple<ProcessInfo, ErrorCode> get_process_info(int32_t pid);

/**
 * Build process tree (parent-child relationships)
 *
 * @param filter Optional filter criteria
 * @return Root nodes of process tree (typically init/systemd)
 */
std::vector<ProcessTreeNode> get_process_tree(const FilterOptions& filter = FilterOptions());

/**
 * Get children of a specific process
 *
 * @param pid Parent process ID
 * @param recursive Include grandchildren, etc.
 * @return List of child processes
 */
std::vector<ProcessInfo> get_children(int32_t pid, bool recursive = false);

/**
 * Check if a process exists
 *
 * @param pid Process ID
 * @return true if process exists
 */
bool process_exists(int32_t pid);

/**
 * Get current process ID
 */
int32_t get_current_pid();

/**
 * Get parent process ID
 */
int32_t get_parent_pid();

// ============================================================================
// User/Group Utilities
// ============================================================================

/**
 * Get username from UID
 */
std::string uid_to_name(uint32_t uid);

/**
 * Get UID from username
 */
std::optional<uint32_t> name_to_uid(const std::string& name);

/**
 * Get group name from GID
 */
std::string gid_to_name(uint32_t gid);

// ============================================================================
// System Information
// ============================================================================

/**
 * Get number of CPUs
 */
int get_cpu_count();

/**
 * Get system uptime in seconds
 */
double get_uptime();

/**
 * Get clock ticks per second
 */
long get_clock_ticks();

/**
 * Get page size in bytes
 */
long get_page_size();

/**
 * Get total system memory in bytes
 */
uint64_t get_total_memory();

} // namespace aps
} // namespace aria

#endif // ARIA_APS_PROCESS_LIST_HPP
