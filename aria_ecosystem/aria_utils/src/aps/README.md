# aps - Aria Process Listing

Cross-platform process listing library for the Aria ecosystem.

## Purpose

`aps` provides a high-level, type-safe API for enumerating and inspecting system processes. It serves as the foundation for the Aria `aps` command-line utility.

## Features

- **Cross-Platform**: Works on Linux (via /proc), macOS (via sysctl), and Windows (via WinAPI)
- **Process Enumeration**: List all running processes with filtering and sorting
- **Detailed Information**: PID, PPID, user, state, CPU/memory usage, command line
- **Process Trees**: Build parent-child hierarchy visualization
- **FFI Support**: C-compatible API for Aria runtime integration
- **TBB Safety**: All counters use TBB-safe types

## Quick Start

```cpp
#include "aps/process_list.hpp"
using namespace aria::aps;

// Get all processes
auto result = get_process_list();
for (const auto& proc : result.processes) {
    std::cout << proc.pid << " " << proc.name << "\n";
}

// Get info for specific process
auto [info, err] = get_process_info(12345);
if (err == ErrorCode::OK) {
    std::cout << "Process " << info.name << " is " << state_to_char(info.state) << "\n";
}

// Filter by user
FilterOptions filter;
filter.user = "root";
auto root_procs = get_process_list(filter);
```

## Building

```bash
mkdir build && cd build
cmake ..
make
make test
```

## FFI API (C)

```c
// Get process list
AriaProcessListResult result = aria_aps_list(NULL);
for (size_t i = 0; i < result.count; i++) {
    printf("%d %s\n", result.processes[i].pid, result.processes[i].name);
}
aria_aps_free(&result);

// Get single process
int error;
AriaProcessInfo info = aria_aps_get(1234, &error);
if (error == 0) {
    printf("%s\n", info.name);
}
aria_aps_free_info(&info);
```

## Process States

| State | Char | Description |
|-------|------|-------------|
| RUNNING | R | Running or runnable |
| SLEEPING | S | Interruptible sleep |
| DISK_SLEEP | D | Uninterruptible disk sleep |
| STOPPED | T | Stopped by signal |
| ZOMBIE | Z | Terminated but not reaped |
| DEAD | X | Dead |
| IDLE | I | Idle kernel thread |

## Platform Notes

### Linux
- Uses `/proc` filesystem for all information
- Requires read access to `/proc/[pid]/*` files
- Some fields (I/O stats) may require elevated privileges

### macOS
- Uses `sysctl` and `libproc` APIs
- Some process information may be restricted by SIP

### Windows
- Uses `CreateToolhelp32Snapshot` and related APIs
- May require elevated privileges for some processes

## License

Part of the Aria Language Project.
