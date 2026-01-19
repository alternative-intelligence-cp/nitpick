/**
 * ffi.cpp
 * C FFI bridge for aps (process listing) library
 *
 * Exposes C-compatible API for linking with:
 * - Aria runtime/compiler
 * - External tools
 * - Test harnesses
 *
 * Memory Management: All returned memory owned by caller
 * Thread-safe: Each call creates fresh instance
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "aps/process_list.hpp"
#include <cstdlib>
#include <cstring>
#include <new>

using namespace aria::aps;

extern "C" {

// ============================================================================
// Result Structures (Aria-compatible)
// ============================================================================

typedef struct {
    int32_t pid;
    int32_t ppid;
    uint32_t uid;
    char* name;         // Caller must free
    char* cmdline;      // Caller must free
    char state;
    int32_t threads;
    uint64_t vsize;
    uint64_t rss;
    double cpu_percent;
} AriaProcessInfo;

typedef struct {
    AriaProcessInfo* processes;  // Array of processes
    size_t count;                // Number of processes
    int error_code;              // 0 = OK
    char* error_message;         // Error message (caller must free)
} AriaProcessListResult;

typedef struct {
    int32_t pid;                // Filter by PID (-1 for all)
    int32_t ppid;               // Filter by parent PID (-1 for all)
    int32_t uid;                // Filter by UID (-1 for all)
    const char* name;           // Filter by name (NULL for all)
    int include_kernel;         // Include kernel threads (0/1)
} AriaProcessFilter;

// ============================================================================
// Helper Functions
// ============================================================================

static char* safe_strdup(const std::string& s) {
    if (s.empty()) return nullptr;
    char* result = (char*)malloc(s.size() + 1);
    if (result) {
        memcpy(result, s.c_str(), s.size() + 1);
    }
    return result;
}

static AriaProcessInfo convert_process_info(const ProcessInfo& info) {
    AriaProcessInfo result;
    result.pid = info.pid;
    result.ppid = info.ppid;
    result.uid = info.uid;
    result.name = safe_strdup(info.name);
    result.cmdline = safe_strdup(info.cmdline);
    result.state = state_to_char(info.state);
    result.threads = info.threads;
    result.vsize = info.vsize;
    result.rss = info.rss;
    result.cpu_percent = info.cpu_percent;
    return result;
}

// ============================================================================
// Main API
// ============================================================================

/**
 * Get list of all processes with optional filtering
 */
AriaProcessListResult aria_aps_list(const AriaProcessFilter* filter) {
    AriaProcessListResult result = {nullptr, 0, 0, nullptr};

    try {
        FilterOptions opts;

        if (filter) {
            if (filter->pid >= 0) {
                opts.pid = filter->pid;
            }
            if (filter->ppid >= 0) {
                opts.ppid = filter->ppid;
            }
            if (filter->uid >= 0) {
                opts.uid = static_cast<uint32_t>(filter->uid);
            }
            if (filter->name && filter->name[0] != '\0') {
                opts.name = filter->name;
            }
            opts.include_kernel = (filter->include_kernel != 0);
        }

        auto list_result = get_process_list(opts, SortOptions());

        if (list_result.error != ErrorCode::OK) {
            result.error_code = static_cast<int>(list_result.error);
            result.error_message = safe_strdup(list_result.error_message);
            return result;
        }

        // Allocate process array
        result.count = list_result.processes.size();
        if (result.count > 0) {
            result.processes = (AriaProcessInfo*)malloc(
                result.count * sizeof(AriaProcessInfo));

            if (!result.processes) {
                result.count = 0;
                result.error_code = static_cast<int>(ErrorCode::SYSTEM_ERROR);
                result.error_message = safe_strdup("Memory allocation failed");
                return result;
            }

            for (size_t i = 0; i < result.count; ++i) {
                result.processes[i] = convert_process_info(list_result.processes[i]);
            }
        }

    } catch (const std::exception& e) {
        result.error_code = static_cast<int>(ErrorCode::UNKNOWN_ERROR);
        result.error_message = safe_strdup(e.what());
    } catch (...) {
        result.error_code = static_cast<int>(ErrorCode::UNKNOWN_ERROR);
        result.error_message = safe_strdup("Unknown error");
    }

    return result;
}

/**
 * Free a process list result
 */
void aria_aps_free(AriaProcessListResult* result) {
    if (!result) return;

    if (result->processes) {
        for (size_t i = 0; i < result->count; ++i) {
            free(result->processes[i].name);
            free(result->processes[i].cmdline);
        }
        free(result->processes);
        result->processes = nullptr;
    }

    if (result->error_message) {
        free(result->error_message);
        result->error_message = nullptr;
    }

    result->count = 0;
}

/**
 * Get info for a single process
 */
AriaProcessInfo aria_aps_get(int32_t pid, int* error_code) {
    AriaProcessInfo result = {0};

    try {
        auto [info, err] = get_process_info(pid);

        if (err != ErrorCode::OK) {
            if (error_code) *error_code = static_cast<int>(err);
            return result;
        }

        result = convert_process_info(info);
        if (error_code) *error_code = 0;

    } catch (...) {
        if (error_code) *error_code = static_cast<int>(ErrorCode::UNKNOWN_ERROR);
    }

    return result;
}

/**
 * Free a single process info
 */
void aria_aps_free_info(AriaProcessInfo* info) {
    if (!info) return;
    free(info->name);
    free(info->cmdline);
    info->name = nullptr;
    info->cmdline = nullptr;
}

/**
 * Check if process exists
 */
int aria_aps_exists(int32_t pid) {
    return process_exists(pid) ? 1 : 0;
}

/**
 * Get current PID
 */
int32_t aria_aps_current_pid(void) {
    return get_current_pid();
}

/**
 * Get parent PID
 */
int32_t aria_aps_parent_pid(void) {
    return get_parent_pid();
}

/**
 * Get username from UID
 */
char* aria_aps_uid_to_name(uint32_t uid) {
    return safe_strdup(uid_to_name(uid));
}

/**
 * Get CPU count
 */
int aria_aps_cpu_count(void) {
    return get_cpu_count();
}

/**
 * Get system uptime in seconds
 */
double aria_aps_uptime(void) {
    return get_uptime();
}

/**
 * Get total system memory in bytes
 */
uint64_t aria_aps_total_memory(void) {
    return get_total_memory();
}

} // extern "C"
