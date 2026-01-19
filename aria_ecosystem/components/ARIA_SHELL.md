# Aria Shell (aria_shell / AriaSH)

**Component Type**: Interactive Shell / Command Interpreter  
**Language**: C++20  
**Repository**: `/home/randy/._____RANDY_____/REPOS/aria_shell`  
**Status**: Pre-Alpha (Research & Planning Phase)  
**Version**: v0.0.1-dev

---

## Table of Contents

1. [Overview](#overview)
2. [Research Progress](#research-progress)
3. [Architecture](#architecture)
4. [6-Stream Topology](#6-stream-topology)
5. [Key Features](#key-features)
6. [Implementation Status](#implementation-status)
7. [Platform Support](#platform-support)
8. [Integration Points](#integration-points)

---

## Overview

AriaSH (Aria Shell) is a modern interactive shell designed for the Aria ecosystem with native support for **6-stream I/O topology**. Unlike traditional shells (bash, zsh) that only manage 3 streams (stdin/stdout/stderr), AriaSH handles all 6 streams defined by the Aria runtime.

### Design Goals

1. **Native 6-stream support**: stdin/stdout/stderr + stddbg/stddati/stddato
2. **Modern UX**: Multi-line input, Ctrl+Enter submission, intelligent editing
3. **Type-aware**: Understands Aria types and result<T> error handling
4. **Cross-platform**: Linux, Windows, macOS
5. **Async-ready**: Non-blocking I/O, concurrent process management
6. **TBB integration**: First-class support for Type-Based Bounded integers

---

## Research Progress

### Completed Research Topics (15/15) ✅

Comprehensive research via Gemini-based analysis completed for all 15 topics:

| # | Topic | Status | Key Findings |
|---|-------|--------|--------------|
| 1 | **Hex-stream I/O** | ✅ Complete | 6 pipes per process, FD 0-5 mapping |
| 2 | **Windows Bootstrap** | ✅ Complete | Environment variable protocol for FD handles |
| 3 | **Multi-line Input** | ✅ Complete | Ctrl+Enter, bracket balancing, syntax highlighting |
| 4 | **Stream Draining** | ✅ Complete | Async readers, ring buffers, multiplexing |
| 5 | **Parser Design** | ✅ Complete | Tokenizer + recursive descent, error recovery |
| 6 | **TBB Integration** | ✅ Complete | Variable type tracking, overflow detection |
| 7 | **Process Abstraction** | ✅ Complete | Platform-agnostic process handle, unified API |
| 8 | **Event Loop** | ✅ Complete | epoll/kqueue/IOCP, task queue, signal handling |
| 9 | **Variable Interpolation** | ✅ Complete | ${var}, escaping rules, type preservation |
| 10 | **Configuration** | ✅ Complete | .ariashrc, JSON format, theme system |
| 11 | **History Management** | ✅ Complete | Persistent history, search, deduplication |
| 12 | **Tab Completion** | ✅ Complete | Command completion, path completion, TBB-aware |
| 13 | **Job Control** | ✅ Complete | Background jobs, fg/bg, suspend/resume |
| 14 | **Pipelines** | ✅ Complete | Multi-stream pipelines, fan-out/fan-in |
| 15 | **Terminal Rendering** | ✅ Complete | ANSI escape codes, cursor control, colors |

### Implementation Plan

**Current Status**: v1.0 Implementation Plan created (9/15 topics synthesized)

**Document**: [`/home/randy/._____RANDY_____/REPOS/aria_shell/docs/research/plan/IMPLEMENTATION_PLAN_V1.md`](../aria_shell/docs/research/plan/IMPLEMENTATION_PLAN_V1.md)

**Next Steps**:
1. Gap analysis on all 15 research topics
2. Iterative refinement with Gemini
3. Hierarchical TODO structure creation
4. Phased implementation

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         ARIA SHELL                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Input Handler                                              ││
│  │  - Multi-line editing                                      ││
│  │  - Ctrl+Enter submission                                   ││
│  │  - Syntax highlighting                                     ││
│  │  - Bracket balancing                                       ││
│  │  - History integration                                     ││
│  │  - Tab completion                                          ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Parser                                                      ││
│  │  - Tokenizer (lexical analysis)                            ││
│  │  - Command parser (recursive descent)                      ││
│  │  - Variable interpolation                                  ││
│  │  - Pipeline construction                                   ││
│  │  - Error recovery                                          ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Executor                                                    ││
│  │  - Process spawning (6-stream topology)                    ││
│  │  - Job control (background, suspend, resume)               ││
│  │  - Pipeline orchestration                                  ││
│  │  - Built-in commands (cd, exit, set, etc.)                 ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ 6-Stream Manager                                           ││
│  │  - Create 6 pipes per process                              ││
│  │  - Spawn drain workers (async threads)                     ││
│  │  - Multiplex stdout/stderr to terminal                     ││
│  │  - Buffer stddbg in ring buffer                            ││
│  │  - Accumulate stddato in wild buffer                       ││
│  │  - Feed stddati from file/variable                         ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Event Loop (Async Runtime)                                 ││
│  │  - epoll/kqueue/IOCP for FD multiplexing                   ││
│  │  - Task queue for async operations                         ││
│  │  - Signal handling (SIGCHLD, SIGINT, SIGTSTP)              ││
│  │  - Process wait/reaping                                    ││
│  └────────────────────────────────────────────────────────────┘│
│                           ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐│
│  │ Platform Abstraction                                        ││
│  │  - Linux: fork/exec, pidfd, epoll                          ││
│  │  - Windows: CreateProcess, IOCP, FD bootstrap protocol     ││
│  │  - macOS: fork/exec, kqueue                                ││
│  └────────────────────────────────────────────────────────────┘│
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6-Stream Topology

### Stream Roles

| FD | Stream | Direction | Purpose | aria_shell Handling |
|----|--------|-----------|---------|---------------------|
| 0 | **stdin** | → Child | Standard input | Pipe from file or user input |
| 1 | **stdout** | ← Child | Standard output | Display to terminal (white text) |
| 2 | **stderr** | ← Child | Error messages | Display to terminal (red text) |
| 3 | **stddbg** | ← Child | Debug logs | Store in ring buffer (optional display) |
| 4 | **stddati** | → Child | Binary data in | Feed from file or variable |
| 5 | **stddato** | ← Child | Binary data out | Accumulate in wild buffer |

### Process Spawning (Linux)

```cpp
// Create 6 pipes
int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
int stddbg_pipe[2], stddati_pipe[2], stddato_pipe[2];

pipe2(stdin_pipe, O_CLOEXEC);
pipe2(stdout_pipe, O_CLOEXEC);
pipe2(stderr_pipe, O_CLOEXEC);
pipe2(stddbg_pipe, O_CLOEXEC);
pipe2(stddati_pipe, O_CLOEXEC);
pipe2(stddato_pipe, O_CLOEXEC);

pid_t pid = fork();
if (pid == 0) {
    // Child: Remap pipes to FDs 0-5
    dup2(stdin_pipe[0], 0);
    dup2(stdout_pipe[1], 1);
    dup2(stderr_pipe[1], 2);
    dup2(stddbg_pipe[1], 3);
    dup2(stddati_pipe[0], 4);
    dup2(stddato_pipe[1], 5);
    
    // Close all pipe ends (now duplicated)
    close_all_pipes();
    
    execve(executable, argv, envp);
    _exit(127);
} else {
    // Parent: Close child ends, spawn drain workers
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    close(stddbg_pipe[1]);
    close(stddati_pipe[0]);
    close(stddato_pipe[1]);
    
    // Start async drain workers
    spawn_drain_worker(stdout_pipe[0], STREAM_STDOUT);
    spawn_drain_worker(stderr_pipe[0], STREAM_STDERR);
    spawn_drain_worker(stddbg_pipe[0], STREAM_STDDBG);
    spawn_drain_worker(stddato_pipe[0], STREAM_STDDATO);
    
    return ProcessHandle{pid, stdin_pipe[1], ...};
}
```

### Windows Bootstrap Protocol

**Problem**: Windows doesn't support FDs 3-5 natively.

**Solution**: Pass handle values via environment variables.

**Shell Side**:
```cpp
// Create 6 pipes, get Win32 HANDLEs
HANDLE stdin_read, stdin_write, ...;
CreatePipe(&stdin_read, &stdin_write, &sa, 0);
// ... create all 6 pipes ...

// Convert HANDLEs to inheritable form
SetHandleInformation(stddbg_write, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stddati_read, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stddato_write, HANDLE_FLAG_INHERIT, 1);

// Pass as environment variables
char env_vars[256];
sprintf(env_vars, "ARIA_STDDBG_HANDLE=%lld;ARIA_STDDATI_HANDLE=%lld;ARIA_STDDATO_HANDLE=%lld",
        (long long)stddbg_write, (long long)stddati_read, (long long)stddato_write);

// Spawn process with custom environment
STARTUPINFOEX si = { ... };
CreateProcess(..., env_vars, &si, ...);
```

**Child Side** (Aria runtime on Windows):
```c
void aria_runtime_init(void) {
    // Parse environment variables
    const char* stddbg_str = getenv("ARIA_STDDBG_HANDLE");
    const char* stddati_str = getenv("ARIA_STDDATI_HANDLE");
    const char* stddato_str = getenv("ARIA_STDDATO_HANDLE");
    
    if (stddbg_str) {
        HANDLE h = (HANDLE)atoll(stddbg_str);
        io_stddbg_fd = _open_osfhandle((intptr_t)h, _O_WRONLY);
    } else {
        io_stddbg_fd = open("NUL", O_WRONLY);  // Fallback
    }
    
    // Similarly for stddati, stddato
}
```

---

## Key Features

### 1. Multi-line Input

**UX**: User can edit commands across multiple lines, submit with Ctrl+Enter

**Implementation**:
- Track cursor position (line, column)
- Handle Up/Down/Left/Right arrows
- Syntax highlighting (keywords, strings, operators)
- Bracket balancing: Auto-insert closing `}`, `]`, `)`
- Ctrl+Enter: Submit command (even mid-line)

**Example**:
```
aria_shell> func:compute = int64(x: int64) {
            ╎   if (x > 0) {
            ╎       pass(x * 2);
            ╎   } else {
            ╎       pass(0);
            ╎   }
            ╎}    <-- Ctrl+Enter here submits
```

---

### 2. Stream Draining (Async Workers)

**Purpose**: Read from child process streams without blocking

**Workers**:
- **stdout drain**: `read()` from FD, write to terminal
- **stderr drain**: `read()` from FD, write to terminal (in red)
- **stddbg drain**: `read()` from FD, append to ring buffer
- **stddato drain**: `read()` from FD, append to wild buffer

**Concurrency**:
- Each worker runs in a separate thread (or async task)
- Uses `select()`/`poll()`/`epoll()` to wait for data
- Terminates when child closes stream (read returns 0)

---

### 3. TBB Integration

**Type Tracking**: Shell maintains variable types

**Example**:
```bash
aria_shell> tbb8:age = 25
aria_shell> age = age + 1     # OK: 25 + 1 = 26
aria_shell> age = age + 200   # ERROR: Overflow! 26 + 200 = 226 > 127
```

**Implementation**:
- Symbol table maps variable names → types
- Arithmetic operations check TBB bounds
- Overflow → Set to ERR sentinel, display error message

---

### 4. Variable Interpolation

**Syntax**: `${var}` or `$var`

**Example**:
```bash
aria_shell> name = "World"
aria_shell> echo "Hello, ${name}!"
Hello, World!
```

**Type Preservation**:
```bash
aria_shell> tbb8:count = 10
aria_shell> ./process --count ${count}   # Passes "10" as string
```

---

### 5. Job Control

**Background Jobs**: `&` at end of command

**Example**:
```bash
aria_shell> ./long_running_task &
[1] 12345
aria_shell> jobs
[1]+  Running                 ./long_running_task &
aria_shell> fg 1              # Bring to foreground
aria_shell> Ctrl+Z            # Suspend
[1]+  Stopped                 ./long_running_task
aria_shell> bg 1              # Resume in background
[1]+ ./long_running_task &
```

**Implementation**:
- Maintain job table (ID → PID, state, command string)
- Handle SIGCHLD to reap terminated jobs
- `fg`/`bg` send SIGCONT, change foreground process group

---

### 6. Pipelines

**Standard Pipeline** (stdout → stdin):
```bash
aria_shell> cat file.txt | grep "error" | wc -l
```

**Multi-stream Pipeline** (stddato → stddati):
```bash
aria_shell> ./generator |> ./processor |> ./consumer
```
(Where `|>` pipes stddato of left to stddati of right)

**Fan-out**:
```bash
aria_shell> ./data_source |& tee stddbg.log |> ./analyzer
```
(Copy stddbg to file, pass stddato to next stage)

---

## Implementation Status

### Phase 1: Core Shell (In Progress)

**Goals**:
- ✅ Research complete (all 15 topics)
- ⏳ Implementation Plan v1.0 created (9/15 topics synthesized)
- ⏳ Gap analysis pending (all 15 topics)
- ⏳ TODO hierarchy generation
- ⏳ Basic REPL (read-eval-print loop)
- ⏳ Simple command execution (no pipelines)
- ⏳ 6-stream spawning on Linux

**Estimated Timeline**: 2-4 weeks (no rush per user philosophy)

---

### Phase 2: Advanced Features

**Goals**:
- Multi-line input + Ctrl+Enter
- Syntax highlighting
- Stream draining (async workers)
- TBB integration
- Variable interpolation

**Estimated Timeline**: 4-6 weeks

---

### Phase 3: Cross-Platform

**Goals**:
- Windows support (FD bootstrap protocol)
- macOS support
- Comprehensive testing on all platforms

**Estimated Timeline**: 2-3 weeks

---

### Phase 4: Polish

**Goals**:
- Tab completion
- History management
- Configuration (.ariashrc)
- Job control
- Pipeline orchestration

**Estimated Timeline**: 4-6 weeks

---

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| **Linux** | ⏳ Primary target | Native FD 0-5, fork/exec, epoll |
| **Windows** | 🔴 Planned | FD bootstrap protocol, CreateProcess, IOCP |
| **macOS** | 🔴 Planned | Similar to Linux, kqueue instead of epoll |

---

## Integration Points

### With Aria Compiler

- Executes compiled Aria programs
- Handles 6-stream I/O from Aria processes
- Type-aware (understands result<T>, TBB, etc.)

### With Aria Runtime

- Relies on `libaria_runtime.a` for child processes
- Expects runtime to handle FD 0-5 initialization
- Drains streams produced by runtime I/O calls

### With AriaBuild

- Can invoke `ariab build` to compile projects
- Displays build output via stdout/stderr
- Captures build artifacts for execution

### With AriaX

- May be distributed as system package via AriaX
- Depends on Aria runtime package
- Integrates with system PATH and shell profiles

---

## Related Components

- **[Aria Runtime](ARIA_RUNTIME.md)**: Provides 6-stream I/O for child processes
- **[Aria Compiler](ARIA_COMPILER.md)**: Compiles Aria programs that shell executes
- **[AriaBuild](ARIABUILD.md)**: Build system invoked by shell
- **[AriaX](ARIAX.md)**: Package manager for distributing shell

---

## Research Documents

Located in `/home/randy/._____RANDY_____/REPOS/aria_shell/docs/research/gemini/responses/`:

1. `shell_01_hexstream_io.txt` - 6-stream topology design
2. `shell_02_windows_bootstrap.txt` - Windows FD handle protocol
3. `shell_03_multiline_input.txt` - Multi-line editing UX
4. `shell_04_stream_draining.txt` - Async drain workers
5. `shell_05_parser.txt` - Command parser design
6. `shell_06_tbb_integration.txt` - TBB variable tracking
7. `shell_07_process_abstraction.txt` - Cross-platform process API
8. `shell_08_event_loop.txt` - Async event loop design
9. `shell_09_variable_interpolation.txt` - ${var} expansion
10. `shell_10_configuration.txt` - .ariashrc design
11. `shell_11_history.txt` - Persistent history management
12. `shell_12_tab_completion.txt` - Intelligent completion
13. `shell_13_job_control.txt` - Background jobs, fg/bg
14. `shell_14_pipelines.txt` - Multi-stream pipeline orchestration
15. `shell_15_terminal_rendering.txt` - ANSI codes, cursor control

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025
