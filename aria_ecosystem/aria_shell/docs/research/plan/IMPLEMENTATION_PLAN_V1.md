# AriaSH Implementation Plan v1.0
**Date**: December 22, 2025  
**Status**: Research Phase Complete (15/15 topics complete)  
**Next Steps**: Begin implementation - Stage 0 foundation work

---

## Executive Summary

AriaSH is a native shell for the Aria programming language ecosystem, implementing two radical innovations:

1. **Hex-Stream I/O Topology**: Expands from 3 to 6 file descriptors (stdin/stdout/stderr + stddbg/stddati/stddato)
2. **Modal Multi-Line Input**: Raw terminal mode with Ctrl+Enter submission, eliminating heredocs

This document consolidates research from **15 completed Gemini Deep Research cycles** into a cohesive implementation roadmap, representing a comprehensive foundation ready for implementation.

---

## Research Synthesis Summary

### Completed Research Topics (15/15)

1. ✅ **Hex-Stream I/O** (shell_01): Core topology, zero-copy splice, kernel integration
2. ✅ **Windows Bootstrap** (shell_02): STARTUPINFOEX, handle inheritance, bootstrap protocol
3. ✅ **Multi-Line Input** (shell_03): Raw mode state machine, Ctrl+Enter detection, brace-aware indentation
4. ✅ **Stream Draining** (shell_04): Threaded drain workers, deadlock prevention, backpressure handling
5. ✅ **Parser** (shell_05): Whitespace-insensitive tokenizer, Aria syntax subset, type-safe variables
6. ✅ **TBB Integration** (shell_06): Twisted Balanced Binary arithmetic, sticky errors, overflow handling
7. ✅ **Process Abstraction** (shell_07): Process lifecycle, pidfd management, result<int> wrapper
8. ✅ **Event Loop** (shell_08): epoll/IOCP integration, async I/O, process termination events
9. ✅ **Variable Interpolation** (shell_09): &{VAR} engine, scope resolution, injection safety
10. ✅ **Config System** (shell_10): .aria_shrc configuration, modal input parameters, submission trigger negotiation
11. ✅ **History** (shell_11): Atomic Block Text Protocol, fcntl locking, std::deque storage, incremental search
12. ✅ **Tab Completion** (shell_12): Context-aware engine, Adaptive Radix Tree, Bloom filter, fuzzy matching
13. ✅ **Job Control** (shell_13): POSIX process groups, Windows Job Objects, fg/bg/jobs commands, terminal arbitration
14. ✅ **Pipelines** (shell_14): Multi-stage hex-stream routing, zero-copy splice, error propagation, data/control plane separation
15. ✅ **Terminal Rendering** (shell_15): VT100/ANSI sequences, raw mode management, VirtualScreenBuffer, optimal escape generation

---

## Core Architecture

### Component Hierarchy

```
aria::shell namespace (C++20)
│
├── Shell (Main Controller)
│   ├── Terminal (Raw mode, key decoding)
│   ├── StreamController (6-stream I/O management)
│   ├── Environment (Variable scope, type tracking)
│   ├── Parser (Tokenizer, AST builder)
│   └── EventLoop (epoll/IOCP orchestration)
│
├── Process (Subprocess abstraction)
│   ├── spawn() → result<Process>
│   ├── wait() → result<int>
│   └── Stream routing (FD 0-5 setup)
│
└── Input (REPL state machine)
    ├── RawMode management
    ├── Chord detection (Ctrl+Enter)
    └── Brace-aware indentation
```

---

## Phase 0: Foundation (Research Synthesis Complete - 9/15)

### 0.1 Hex-Stream I/O Core

**Research**: shell_01_hexstream_io.txt (27K)

**Components**:
- Stream definitions (FD 0-5 semantic roles)
- Async ring buffer for stddbg
- Wild buffer integration for stddati/stddato
- Zero-copy splice on Linux
- Handle duplication strategy on Windows

**Key Implementation Details**:
```cpp
// Three planes of communication
Control Plane:   stdin (0), stdout (1), stderr (2)    // UTF-8 text, line-buffered
Observability:   stddbg (3)                           // Structured logs, async ring buffer
Data Plane:      stddati (4), stddato (5)             // Wild buffers, zero-copy

// Linux zero-copy optimization
splice(pipe_out_A, NULL, pipe_in_B, NULL, len, SPLICE_F_MOVE);

// Windows direct piping (no intermediate proxy)
CreatePipe(&read_handle, &write_handle, ...);
// Pass write_handle to Process A's FD 5
// Pass read_handle to Process B's FD 4
```

**Dependencies**:
- Aria runtime integration (io.stddbg, io.stddati, io.stddato globals)
- Wild memory allocator (aria_alloc/aria_free)
- TBB type system (for typed stream errors)

---

### 0.2 Platform-Specific Process Spawning

**Research**: shell_02_windows_bootstrap.txt (21K)

**Linux Strategy**:
```cpp
// pidfd-based race-free process management
int pidfd = pidfd_open(child_pid, 0);
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pidfd, &event);

// FD setup sequence in child
dup2(pipe_stdin_read, 0);    // stdin
dup2(pipe_stdout_write, 1);  // stdout
dup2(pipe_stderr_write, 2);  // stderr
dup2(pipe_stddbg_write, 3);  // stddbg
dup2(pipe_stddati_read, 4);  // stddati
dup2(pipe_stddato_write, 5); // stddato
execve(...);
```

**Windows Strategy**:
```cpp
// STARTUPINFOEX with attribute list
STARTUPINFOEX si_ex = {};
InitializeProcThreadAttributeList(...);
UpdateProcThreadAttribute(
    si_ex.lpAttributeList,
    0,
    PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
    handle_array,  // 6 handles
    sizeof(handle_array),
    ...
);

// Bootstrap protocol via environment variable
__ARIA_FD_MAP=3:0x1234;4:0x5678;5:0x9ABC

// Aria Runtime crt0 parses and wraps handles before main()
```

**Critical Sections**:
- O_CLOEXEC hygiene (prevent FD leaks)
- PID recycling prevention (pidfd)
- Handle whitelist security (no bInheritHandles=TRUE)

---

### 0.3 Modal Multi-Line Input Model

**Research**: shell_03_multiline_input.txt (35K)

**State Machine**:
```
States:
  Idle → Waiting for input
  Chord Analysis → Modifier key held (Ctrl/Alt)
  Buffer Manipulation → Standard key appends to buffer
  Submission → Ctrl+Enter triggers parse/execute

Key Behaviors:
  Enter → Insert literal \n (not submit)
  Ctrl+Enter → Submit command
  Brace detection → Auto-indent on { }
```

**Terminal Setup (POSIX)**:
```cpp
termios raw_mode;
tcgetattr(STDIN_FILENO, &raw_mode);
raw_mode.c_lflag &= ~(ICANON | ECHO | ISIG);  // Disable canonical, echo, signals
tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_mode);
```

**Terminal Setup (Windows)**:
```cpp
HANDLE h_stdin = GetStdHandle(STD_INPUT_HANDLE);
DWORD mode;
GetConsoleMode(h_stdin, &mode);
mode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
SetConsoleMode(h_stdin, mode);
```

**Ctrl+Enter Detection**:
- Protocol Negotiation: Query for Kitty Keyboard Protocol support
- Legacy Fallback: Time-based heuristics or Alt+Enter alternative
- Configurable: User can define submission chord in .aria_shrc

**Brace-Aware Indentation**:
```cpp
// Lightweight lexical analyzer tracks brace depth
if (key == '\n' && brace_depth > 0) {
    buffer.append('\n');
    buffer.append(indent_string(brace_depth * 4));  // 4 spaces per level
    prompt = ".... ";  // Continuation indicator
}
```

---

### 0.4 Stream Draining Architecture

**Research**: shell_04_stream_draining.txt (27K)

**Problem**: Pipe deadlock when child writes > kernel buffer (typically 64KB)

**Solution**: Threaded drain workers with lockless queues

```cpp
class StreamController {
    std::thread drain_workers[6];  // One per stream
    LockFreeQueue<DrainTask> task_queue;
    
    void drain_stream(int fd) {
        char buffer[DRAIN_CHUNK_SIZE];
        while (running) {
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0) {
                // Forward to appropriate handler
                // stdout/stderr → terminal
                // stddbg → async ring buffer
                // stddati/stddato → wild buffer pool
            }
        }
    }
};
```

**Backpressure Handling**:
- **Control Plane** (stdout/stderr): Block if terminal can't keep up
- **Observability** (stddbg): Drop-on-full policy (preserves performance)
- **Data Plane** (stddato): Block or buffer to disk (configurable)

**Performance Target**: Sustain 1 GB/s throughput on modern SSD-backed systems

---

### 0.5 Parser and Syntax

**Research**: shell_05_parser.txt (21K)

**Design Principles**:
- Whitespace-insensitive (unlike Bash)
- Brace-delimited blocks (like Aria)
- Type-safe variable declarations
- Subset of full Aria grammar

**Tokenizer**:
```cpp
enum TokenType {
    IDENTIFIER,    // Variable or command name
    OPERATOR,      // ==, !=, &&, ||, |>, etc.
    LITERAL,       // String "...", number, tbb literal
    BRACE_OPEN,    // {
    BRACE_CLOSE,   // }
    SEMICOLON,     // ; (statement terminator)
    INTERPOLATION  // &{VAR}
};
```

**Syntax Examples**:
```aria
// Variable declaration (type-safe)
int8:status = 0;
string:filename = "output.txt";

// Control flow (brace-delimited)
if (status == 0) {
    echo("Success");
} else {
    echo("Failed");
}

// Process spawning
spawn("ariac", ["--version"]);
```

**AST Structure**:
```cpp
struct ASTNode {
    enum Type { STMT, EXPR, BLOCK };
    Type type;
    std::vector<std::unique_ptr<ASTNode>> children;
    std::variant<int64_t, double, std::string> value;
};
```

---

### 0.6 TBB Integration and Sticky Errors

**Research**: shell_06_tbb_integration.txt (23K)

**Twisted Balanced Binary Arithmetic**:
- Base-3 signed representation: -1, 0, +1 (trits)
- Packed into 8-bit types: tbb8 range [-121, +121]
- ERR sentinel: -128 (overflow/error marker)

**Sticky Error Propagation**:
```cpp
tbb8:x = 121;
tbb8:y = 10;
tbb8:result = x + y;  // Overflow! result = ERR (-128)

if (result < 0) {
    // This triggers sticky error halt
    shell_panic("TBB overflow detected");
}
```

**Exit Code Mapping**:
```cpp
result<int> Process::wait() {
    int exit_code = waitpid(...);
    
    if (exit_code == 0) return ok(0);
    if (exit_code == -128) return err(TBBError::STICKY);
    return ok(exit_code);
}
```

**Integration Points**:
- Shell arithmetic uses TBB types by default
- Exit codes validated against ERR sentinel
- Prevents silent corruption in scripts

---

### 0.7 Process Abstraction Layer

**Research**: shell_07_process_abstraction.txt (25K)

**Class Design**:
```cpp
class Process {
public:
    // Factory method (can fail)
    static result<Process> spawn(
        string:command,
        vector<string>:args,
        StreamConfig:streams = default_streams()
    );
    
    // Lifecycle management
    result<int> wait();              // Block until exit
    result<void> kill(int signal);   // Send signal (SIGTERM, SIGKILL)
    bool is_running();               // Poll status
    
    // Stream access
    FileDescriptor stdin();
    FileDescriptor stdout();
    // ... etc for all 6 streams
    
private:
    #ifdef __linux__
        int pidfd_;  // Linux pidfd handle
    #elif _WIN32
        HANDLE process_handle_;
        HANDLE thread_handle_;
    #endif
    
    std::array<FileDescriptor, 6> streams_;
};
```

**Result Type Integration**:
```cpp
// Aria-native error handling
auto proc = Process::spawn("invalid_cmd", {});
if (proc.is_err()) {
    stderr.writeln("Failed to spawn: " + proc.error_msg());
    return 1;
}

auto exit_code = proc.unwrap().wait();
// exit_code is result<int>, can propagate errors
```

---

### 0.8 Event Loop Architecture

**Research**: shell_08_event_loop.txt (29K)

**Linux (epoll)**:
```cpp
class EventLoop {
    int epoll_fd_;
    
    void run() {
        epoll_event events[MAX_EVENTS];
        while (running_) {
            int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, timeout);
            
            for (int i = 0; i < n; i++) {
                if (events[i].data.fd == stdin_fd) {
                    handle_user_input();
                } else if (is_pidfd(events[i].data.fd)) {
                    handle_process_exit();
                } else {
                    handle_stream_data(events[i].data.fd);
                }
            }
        }
    }
};
```

**Windows (IOCP)**:
```cpp
class EventLoop {
    HANDLE iocp_;
    
    void run() {
        OVERLAPPED_ENTRY entries[MAX_EVENTS];
        ULONG num_entries;
        
        while (running_) {
            GetQueuedCompletionStatusEx(iocp_, entries, MAX_EVENTS, &num_entries, timeout, FALSE);
            
            for (ULONG i = 0; i < num_entries; i++) {
                auto* ctx = (IOContext*)entries[i].lpCompletionKey;
                ctx->handle_completion(entries[i].dwNumberOfBytesTransferred);
            }
        }
    }
};
```

**Registered Events**:
- stdin ready (user input)
- Process termination (pidfd/process handle)
- Stream data available (stdout, stderr, stddbg pipes)
- Timer expiration (for prompts, status updates)

---

### 0.9 Variable Interpolation Engine

**Research**: shell_09_var_interpolation.txt (23K)

**Pattern Syntax**: `&{VARIABLE_NAME}`

**Scope Resolution**:
```cpp
string resolve_variable(const string& name) {
    // 1. Check local shell variables
    if (local_vars.contains(name)) {
        return local_vars[name].to_string();
    }
    
    // 2. Check environment variables
    const char* env_val = getenv(name.c_str());
    if (env_val) return env_val;
    
    // 3. Error: undefined variable
    throw UndefinedVariableError(name);
}
```

**Type-Safe Injection**:
```cpp
// Unsafe (Bash-style): filename could contain "; rm -rf /"
system("cat " + filename);  // DANGEROUS!

// Safe (AriaSH): filename is single argument regardless of content
spawn("cat", ["&{filename}"]);
// Even if filename = "; rm -rf /", it's passed as literal string
```

**Interpolation in Strings**:
```aria
string:greeting = "Hello, &{USER}!";
// Resolves to: "Hello, randy!"

string:path = "/home/&{USER}/projects";
// Resolves to: "/home/randy/projects"
```

**Reuse from AriaBuild**:
- Same engine used in build system
- Proven against injection attacks
- Handles complex nested interpolation

---

## Phase 1: User Experience & Advanced Features (Research Complete - 6/15)

### 1.1 Configuration System

**Research**: shell_10_config_system.txt (178 lines)

**Modal Multi-Line Input Architecture**:
- Raw mode initialization (disables ICANON, ECHO, ISIG on POSIX)
- Hierarchical state machine: Idle → Chord Analysis → Buffer Manipulation → Submission
- Enter key inserts literal `\n` (not submission)
- Brace-aware auto-indentation (4 spaces per depth level)

**Submission Trigger Configuration**:
```cpp
// Protocol negotiation for Ctrl+Enter
1. Extended Protocol: Kitty Keyboard Protocol (distinct sequences)
2. Time-based Heuristics: Infer modifiers from byte timing
3. Fallback Binding: Alt+Enter (unambiguous escape prefix)
```

**Type-Safe Variables**:
```aria
// Explicit typing in .aria_shrc
int8:verbosity = 2;
string:default_editor = "nvim";
tbb8:error_threshold = 10;
```

**Hex-Stream Configuration**:
- Stream redirection policies (FD 3-5)
- stddbg verbosity levels and filtering
- Binary stream handling (prevent terminal corruption)
- Status bar multiplexing for telemetry

**Key Features**:
- Process Orchestration Language (POL) syntax
- Whitespace-insensitive, brace-delimited
- Reuses Aria compiler frontend tokenizer
- Integration with TBB arithmetic for safe config values

---

### 1.2 Command History

**Research**: shell_11_history.txt (296 lines)

**Atomic Block Text Protocol**:
```
#BEGIN_ENTRY timestamp=1734839000 pid=4501
pick (x) {
1: print("One");
2: print("Two");
}
#END_ENTRY checksum=8f4a2c1
```

**Concurrency Strategy**:
```cpp
// fcntl-based locking (POSIX) / LockFileEx (Windows)
1. Acquire exclusive lock (F_WRLCK / LOCKFILE_EXCLUSIVE_LOCK)
2. Seek to end
3. Write atomic block
4. Release lock
5. Close file
```

**In-Memory Data Structure**:
```cpp
std::deque<HistoryEntry> entries;  // O(1) access both ends
struct HistoryEntry {
    std::string content;      // Multi-line preserving whitespace
    uint64_t timestamp;
    std::string working_dir;
    int exit_code;
    std::string session_id;
};
```

**Search Engine**:
- Incremental search (Ctrl+R)
- Inverted index for substring queries (O(log N) per word)
- Sub-millisecond retrieval target
- Session synchronization via file watchers (inotify/ReadDirectoryChangesW)

**Key Features**:
- Multi-line atomicity (20-line blocks stored as single entry)
- Navigation: Up/Down arrows with cursor-aware logic
- Human-readable persistence (no SQLite dependency)
- Deduplication support with configurable policies

---

### 1.3 Tab Completion

**Research**: shell_12_tab_completion.txt (211 lines)

**Context Classification System**:
```cpp
enum CompletionContext {
    COMMAND,           // Start of command line
    ARGUMENT,          // Command arguments/flags
    MODULE_PATH,       // use/import statements
    FILE_PATH,         // Filesystem paths
    VARIABLE,          // &{VAR} interpolation
    MEMBER_ACCESS      // struct.field or ptr->field
};
```

**Reverse Incremental Lexer**:
- Scans backward from cursor position
- Identifies token boundaries and delimiters
- Stops at synchronization points (`;`, `}`)
- Handles incomplete/dirty buffer state

**Data Structures**:
```cpp
// Adaptive Radix Tree (ART) for prefix queries
// - O(k) lookup (k = key length, independent of dataset size)
// - Prefix compression for memory efficiency
// - Ordered iteration (lexicographical)

// Bloom Filter for PATH cache
// - Probabilistic "command exists" check
// - Eliminates stat() calls for non-existent commands
// - Reduces I/O jitter during typing
```

**Fuzzy Matching Algorithm**:
```cpp
// Scoring factors:
1. Baseline Score: 0
2. Prefix Match Bonus: +50 points
3. Character Position Weight: Earlier = higher score
4. Consecutive Characters: +10 per streak
5. Case Match Bonus: +5
6. Frecency Weight: frequency * recency factor
```

**Render Budget**: 16ms per frame (60 Hz target)

**Key Features**:
- Context-aware completion (detects command vs argument vs module)
- Integration with Aria type system (type-safe suggestions)
- VT100 overlay menu rendering (dropdown UI)
- Async completion for remote filesystems
- TBB type awareness (prevents overflow suggestions)

---

### 1.4 Job Control

**Research**: shell_13_job_control.txt (315 lines)

**POSIX Architecture**:
```cpp
// Process Groups and Sessions
setpgid(child_pid, child_pid);        // New process group
tcsetpgrp(STDIN_FILENO, child_pgid);  // Terminal ownership

// Signal handling
SIGTSTP  // Ctrl+Z (suspend)
SIGCONT  // Resume
SIGCHLD  // Child state change notification

// pidfd monitoring (race-free)
int pidfd = pidfd_open(child_pid, 0);
epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pidfd, &event);
```

**Windows Architecture**:
```cpp
// Job Objects (kernel-level grouping)
HANDLE hJob = CreateJobObject(NULL, NULL);
AssignProcessToJobObject(hJob, hProcess);

// Lifecycle guarantee
JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE  // Auto-cleanup on crash

// Suspension (undocumented APIs)
NtSuspendProcess(hProcess);
NtResumeProcess(hProcess);
```

**Job Data Structure**:
```cpp
struct Job {
    uint32_t job_id;
    std::vector<Process> processes;  // Pipeline components
    JobState state;  // RUNNING, STOPPED, COMPLETED
    std::string command_line;
    
    #ifdef __linux__
        pid_t pgid;  // Process group ID
    #elif _WIN32
        HANDLE hJob;  // Job Object handle
    #endif
};
```

**Built-in Commands**:
```aria
command &         // Background execution
fg [%job_id]      // Bring to foreground
bg [%job_id]      // Resume in background
jobs              // List active jobs
```

**Terminal Arbitration**:
- Only foreground process group can read stdin
- Background reads trigger SIGTTIN (suspend)
- tcsetpgrp() switches terminal ownership
- Shell blocks SIGTTOU during ownership transfer

**Hex-Stream Integration**:
- Stream locking for stddato (prevent data corruption)
- Background jobs: stddati → /dev/null (prevent SIGTTIN)
- Output buffering for background stddbg telemetry

---

### 1.5 Pipeline Implementation

**Research**: shell_14_pipelines.txt (300 lines)

**Hex-Stream Topology Routing**:
```cpp
// Three planes of communication
Control Plane:   stdin(0), stdout(1), stderr(2)     // UTF-8 text
Observability:   stddbg(3)                          // Structured logs (JSON)
Data Plane:      stddati(4), stddato(5)             // Binary, zero-copy
```

**Zero-Copy Mechanisms**:
```cpp
// Linux splice() for binary streams
splice(pipe_out_A, NULL, pipe_in_B, NULL, len, SPLICE_F_MOVE);
// Sustains 10+ GB/s throughput (memory-bound, no user-space copy)

// Windows handle inheritance
CreatePipe(&read_h, &write_h, ...);
// Process A: write_h → FD 5 (stddato)
// Process B: read_h → FD 4 (stddati)
```

**Pipeline Syntax**:
```aria
// Standard text pipe (stdout → stdin)
cmd1 | cmd2 | cmd3

// Binary data pipe (stddato → stddati)
encoder |* decoder  // Specialized operator

// Explicit stream redirection
cmd 3> debug.log    // stddbg → file
cmd 5> output.bin   // stddato → file
```

**Error Propagation**:
```cpp
// TBB Sticky Errors
result<int> = proc_a | proc_b;
// Fails if EITHER process returns error
// No pipefail flag needed (default behavior)

// SIGPIPE handling
// Signal → SIG_IGN (ignore)
// write() returns EPIPE → TBB ERR value
// Graceful degradation, no silent crash
```

**File Descriptor Dance**:
```cpp
// Child process setup (between fork and execve)
1. Create 6 pipes with O_CLOEXEC
2. dup2() to map pipes to FD 0-5
3. Close unused descriptors
4. execve() replaces process image
```

**Key Features**:
- Multi-stage pipeline construction
- Stream purity (no log messages in binary data)
- Process group coordination
- Typed injection security (prevents argument injection)
- Backpressure handling per plane

---

### 1.6 Terminal Rendering

**Research**: shell_15_terminal_rendering.txt (218 lines)

**VirtualScreenBuffer Architecture**:
```cpp
class VirtualScreenBuffer {
    std::vector<std::vector<Cell>> grid;  // [row][col]
    struct Cell {
        char32_t codepoint;  // Unicode character
        uint8_t fg_color;    // Foreground (256-color or RGB)
        uint8_t bg_color;    // Background
        uint8_t attributes;  // Bold, underline, etc.
    };
};
```

**Optimal Escape Generation**:
```cpp
// Diffing algorithm (minimize bandwidth)
for (row, col in dirty_cells) {
    if (col == prev_col + 1 && row == prev_row) {
        // Sequential: just emit character
        output << cell.codepoint;
    } else {
        // Jump: emit cursor move + character
        output << "\x1B[" << row << ";" << col << "H" << cell.codepoint;
    }
}
```

**ANSI/VT100 Core Sequences**:
```cpp
ESC[A          // Cursor up
ESC[B          // Cursor down
ESC[C          // Cursor forward
ESC[D          // Cursor backward
ESC[H          // Cursor home
ESC[2J         // Clear screen
ESC[K          // Clear line from cursor
ESC[7          // Save cursor position
ESC[8          // Restore cursor position
ESC[38;2;R;G;Bm // 24-bit TrueColor foreground
```

**Raw Mode State Management**:
```cpp
// POSIX
termios raw;
raw.c_lflag &= ~(ICANON | ECHO | ISIG);  // Disable processing
raw.c_oflag &= ~OPOST;                    // No output translation
tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

// Windows
HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
SetConsoleMode(h, ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));
```

**Key Decoding State Machine**:
```cpp
// Distinguish ESC (single 0x1B) from ESC sequences
1. Receive 0x1B
2. Set timeout (50ms)
3. If next byte arrives quickly → escape sequence
4. If timeout → standalone Escape key
```

**SIGWINCH Handling**:
```cpp
// Terminal resize signal
volatile sig_atomic_t resize_pending = 1;

// Event loop checks flag
if (resize_pending) {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);  // Query new size
    reflow_buffer();                         // Recalculate wraps
    full_redraw();                           // Repaint screen
}
```

**Unicode Rendering**:
- wcwidth() for character width calculation
- East Asian wide characters (2 columns)
- Emoji support
- RTL (right-to-left) text handling

**Hex-Stream Multiplexing**:
- Status bar for stddbg telemetry
- Async updates (ring buffer)
- Save/Restore cursor for atomic updates
- Prevents corruption of input line

**TBB Safety**:
- Coordinate math uses TBB types
- Overflow → ERR sentinel (not wraparound)
- Prevents "cursor explosion" bugs
- Sticky error detection aborts invalid updates

---

## Implementation Roadmap

### Stage 0: Foundation (Est. 2-3 weeks)
**Status**: Ready to begin (all research complete)

- [ ] Project structure setup (CMake, C++20 config)
- [ ] Cross-platform abstraction layer (POSIX vs Windows)
- [ ] Basic terminal setup (raw mode on/off)
- [ ] Minimal REPL (read input, print back)

### Stage 1: Core I/O (Est. 3-4 weeks)
**Status**: Ready to begin (research complete)

- [ ] Hex-stream FD setup (Linux fork/exec + Windows STARTUPINFOEX)
- [ ] Stream draining threads
- [ ] Basic process spawning (single command, no pipes)
- [ ] Zero-copy splice on Linux
- [ ] stddbg async ring buffer

### Stage 2: Parser & Execution (Est. 2-3 weeks)
**Status**: Ready to begin (research complete)

- [ ] Tokenizer implementation
- [ ] AST builder
- [ ] Variable storage and scope
- [ ] TBB arithmetic integration
- [ ] Variable interpolation engine
- [ ] Basic control flow (if/else)

### Stage 3: Event Loop (Est. 2 weeks)
**Status**: Ready to begin (research complete)

- [ ] epoll implementation (Linux)
- [ ] IOCP implementation (Windows)
- [ ] Process termination handling
- [ ] Async I/O integration
- [ ] Signal handling

### Stage 4: User Experience (Est. 4-5 weeks)
**Status**: Ready to begin (research complete)

- [ ] Config system (.aria_shrc with POL parser)
- [ ] Command history (Atomic Block Text Protocol)
- [ ] fcntl/LockFileEx concurrent write locking
- [ ] Incremental search (Ctrl+R) with inverted index
- [ ] Tab completion engine
  - [ ] Reverse incremental lexer
  - [ ] Adaptive Radix Tree for keywords
  - [ ] Bloom filter for PATH cache
  - [ ] Context dispatcher (command/argument/module/path/variable)
  - [ ] Fuzzy matching scorer
- [ ] Multi-line input with brace detection
- [ ] VT100/ANSI rendering engine
  - [ ] VirtualScreenBuffer diffing
  - [ ] Optimal escape generation
  - [ ] SIGWINCH handling
- [ ] Syntax highlighting (optional)

### Stage 5: Advanced Features (Est. 3-4 weeks)
**Status**: Ready to begin (research complete)

- [ ] Job control architecture
  - [ ] Linux: Process groups, setpgid, tcsetpgrp
  - [ ] Windows: Job Objects, NtSuspendProcess
  - [ ] JobManager with pidfd/epoll monitoring
  - [ ] fg/bg/jobs commands
- [ ] Pipeline construction
  - [ ] Multi-stage pipe builder
  - [ ] Stream routing (control/data/observability planes)
  - [ ] Zero-copy splice for binary streams
  - [ ] SIGPIPE → EPIPE error handling
- [ ] Error propagation with TBB sticky errors
- [ ] Hex-stream I/O arbitration

### Stage 6: Polish (Est. 2 weeks)
**Status**: Ready to begin (research complete)

- [ ] Terminal rendering optimization
  - [ ] Unicode width calculation (wcwidth)
  - [ ] 24-bit TrueColor support
  - [ ] Completion menu overlay
- [ ] Theme system (color schemes)
- [ ] Protocol negotiation (Kitty Keyboard Protocol)
- [ ] Performance tuning (16ms render budget)
- [ ] Cross-platform testing
- [ ] Documentation

**Total Estimated Duration**: 16-21 weeks (4-5 months)

---

## Success Criteria

### Functional Requirements

1. ✅ **Hex-Stream I/O**: All 6 streams (0-5) working correctly
2. ✅ **Multi-Line Input**: Ctrl+Enter submission, Enter inserts newline
3. ✅ **Zero Binary Corruption**: stddato can transfer raw binary without log interference
4. ✅ **Type Safety**: Variables have Aria types, prevent silent type coercion
5. ✅ **TBB Integration**: Sticky errors prevent overflow corruption
6. ✅ **Pipeline Support**: Multi-stage pipes with stream routing
7. ✅ **Job Control**: Background jobs, fg/bg/jobs commands, terminal arbitration
8. ✅ **Tab Completion**: Context-aware, ART/Bloom filter optimized, fuzzy matching
9. ✅ **Command History**: Atomic multi-line blocks, fcntl locking, incremental search
10. ✅ **Config System**: .aria_shrc with POL parser, modal input configuration
11. ✅ **Terminal Rendering**: VT100/ANSI, VirtualScreenBuffer, optimal escape generation

### Performance Requirements

1. **Throughput**: Sustain 1 GB/s on data streams (stddati/stddato) via splice()
2. **Latency**: <10ms for command echo (keystroke → screen)
3. **Render Budget**: 16ms per frame (60 Hz) for completion menus and updates
4. **Memory**: <50 MB baseline footprint
5. **Zero-Copy**: splice() used for large transfers on Linux
6. **Search**: Sub-millisecond history search via inverted index

### Compatibility Requirements

1. **Linux**: Ubuntu 20.04+ (kernel 5.3+ for pidfd)
2. **Windows**: Windows 10+ (STARTUPINFOEX support)
3. **Aria**: Compatible with Aria compiler v0.1.0+
4. **Terminals**: VT100/ANSI-compatible (xterm, Windows Terminal, Alacritty)

---

## Research-Resolved Questions

### Configuration System
- ✅ **Format**: Process Orchestration Language (POL) - Aria syntax subset
- ✅ **Security**: Type-safe with explicit typing, reuses compiler frontend
- ✅ **Submission Trigger**: Protocol negotiation (Kitty → time heuristics → Alt+Enter)
- ✅ **Hot-reload**: Configurable via file watchers (inotify/ReadDirectoryChangesW)

### History System
- ✅ **Storage**: Atomic Block Text Protocol (custom format, no SQLite)
- ✅ **Synchronization**: fcntl locking + session deduplication
- ✅ **Multi-line**: Single atomic entry per submission (preserves formatting)
- ✅ **Search**: Inverted index for O(log N) substring queries

### Tab Completion
- ✅ **Matching**: Fuzzy scoring algorithm (prefix bonus, frecency weighting)
- ✅ **UI**: VT100 overlay menu with save/restore cursor
- ✅ **Performance**: Adaptive Radix Tree + Bloom filter for PATH
- ✅ **Context**: Reverse incremental lexer with 6 distinct contexts

### Job Control
- ✅ **Background Output**: Stream locking + ring buffer for stddbg
- ✅ **Job Numbering**: Auto-increment with % prefix (e.g., %1, %2)
- ✅ **Windows Signals**: NtSuspendProcess/NtResumeProcess wrappers
- ✅ **Notifications**: SIGCHLD → JobManager updates state

### Pipelines
- ✅ **Stream Syntax**: `|` (text), `|*` (binary), `3>` (explicit FD redirect)
- ✅ **Error Handling**: TBB sticky errors propagate (default pipefail)
- ✅ **Execution**: Spawn all stages simultaneously, let kernel schedule
- ✅ **SIGPIPE**: Ignored, write() → EPIPE → TBB ERR

### Terminal Rendering
- ✅ **Capability Detection**: Query via DA1, protocol negotiation
- ✅ **Fallback**: Basic ANSI subset for dumb terminals
- ✅ **Redraws**: Diffing algorithm minimizes escape sequences
- ✅ **Unicode**: wcwidth() for display width, full UTF-8 support

---

## Implementation Priorities

### Critical Path (Weeks 1-8)
1. Raw mode terminal setup (Stage 0-1)
2. Hex-stream process spawning (Stage 1)
3. Event loop with epoll/IOCP (Stage 3)
4. Basic parser and execution (Stage 2)

### High Priority (Weeks 9-14)
1. Command history with atomic blocks (Stage 4)
2. Tab completion engine with ART (Stage 4)
3. VT100 rendering with VirtualScreenBuffer (Stage 4)
4. Config system (.aria_shrc parser) (Stage 4)

### Medium Priority (Weeks 15-18)
1. Job control (fg/bg/jobs) (Stage 5)
2. Multi-stage pipelines (Stage 5)
3. Stream routing and hex-stream arbitration (Stage 5)

### Polish (Weeks 19-21)
1. Protocol negotiation (Kitty Keyboard) (Stage 6)
2. Color themes and TrueColor (Stage 6)
3. Performance optimization (splice, bloom filter) (Stage 6)
4. Cross-platform testing (Stage 6)

---

## Architectural Decisions Finalized

1. ✅ **GC Integration**: Shell uses wild (manual) allocation for control structures, GC for user script objects
2. ✅ **stddbg Overflow**: Drop-on-full policy (preserves performance over completeness)
3. ✅ **Config Security**: .aria_shrc is parsed (not executed), type-safe variables only
4. ✅ **Portability**: Linux and Windows as primary targets (macOS/BSD stretch goal)
5. ✅ **History Format**: Human-readable text (grep-able, recoverable with standard tools)
6. ✅ **Completion Backend**: In-process (no external LSP for shell built-ins)
7. ✅ **Render Strategy**: Double-buffered VirtualScreenBuffer with escape minimization

---

## Next Steps

### Immediate Actions (This Week)
1. ✅ **Research Complete**: All 15 topics synthesized into implementation plan
2. → **Project Setup**: Initialize CMake build system with Linux/Windows targets
3. → **Repository Structure**: Create src/shell directory structure
4. → **Platform Abstraction**: Define PAL interfaces (Terminal, Process, FileDescriptor)

### Week 1-2: Stage 0 Foundation
1. Implement Terminal::enableRawMode() for POSIX and Windows
2. Create basic event loop skeleton (stdin polling)
3. Implement minimal echo REPL (proof of concept)
4. Test raw mode on multiple terminal emulators

### Week 3-4: Stage 1 Core I/O
1. Implement Process::spawn() with 6-stream setup
2. Linux: fork/dup2/execve with O_CLOEXEC hygiene
3. Windows: STARTUPINFOEX with attribute list
4. Stream draining thread pool (LockFreeQueue)

### Week 5+: Continue Through Stages 2-6
Follow implementation roadmap detailed above, with weekly checkpoints and integration testing at each stage boundary.

---

**Document Status**: v1.0 - Complete (15/15 research topics integrated)  
**Last Updated**: December 22, 2025  
**Ready For**: Implementation Phase - Stage 0 can begin immediately

---

## Dependencies

### External Libraries

- **C++20 Standard Library**: std::variant, std::deque, std::filesystem, std::jthread
- **POSIX APIs**: fork, execve, pidfd_open, epoll, splice, fcntl, termios
- **Windows APIs**: CreateProcess, STARTUPINFOEX, IOCP, NtSuspendProcess, SetConsoleMode, LockFileEx
- **Aria Runtime**: io.stddbg, TBB types (tbb8/tbb32), wild memory allocator

### Build System

- **CMake 3.20+**: Cross-platform build configuration
- **Compiler**: GCC 11+ or Clang 13+ (C++20 support required)
- **Platform Detection**: Separate build paths for Linux vs Windows

---

## Risk Assessment

### High-Risk Areas

1. **Windows Bootstrap Protocol**: Complex handle management via STARTUPINFOEX
   - *Mitigation*: Extensive testing with handle leak detection tools
2. **Stream Deadlocks**: Pipe buffer exhaustion with concurrent hex-streams
   - *Mitigation*: Threaded drain workers with backpressure handling
3. **Ctrl+Enter Detection**: Terminal compatibility varies (Kitty vs xterm vs PuTTY)
   - *Mitigation*: Protocol negotiation with fallback to Alt+Enter
4. **Zero-Copy Splice**: Requires kernel 5.3+ on Linux
   - *Mitigation*: Fallback to standard read/write on older kernels
5. **TBB Overflow**: Sticky errors in coordinate calculations
   - *Mitigation*: ERR detection aborts invalid screen updates

### Mitigation Strategies

1. **Unit Testing**: Comprehensive test suite for each stream configuration
2. **Fallback Paths**: Graceful degradation when advanced features unavailable
3. **Platform Matrices**: CI testing on Ubuntu 20.04/22.04, Windows 10/11
4. **Fuzzing**: Input fuzzing for parser and terminal decoder state machines
5. **Monitoring**: Integration tests with stddbg telemetry validation

---

## Glossary

- **ART**: Adaptive Radix Tree - O(k) prefix search data structure
- **IOCP**: I/O Completion Ports - Windows async I/O mechanism
- **PAL**: Platform Abstraction Layer
- **POL**: Process Orchestration Language - .aria_shrc syntax
- **TBB**: Twisted Balanced Binary - Aria's overflow-safe integer type
- **VT100**: DEC terminal standard, basis for ANSI escape sequences
- **pidfd**: Linux process file descriptor (kernel 5.3+)
- **splice()**: Linux zero-copy pipe-to-pipe transfer syscall
- **fcntl**: File control - POSIX locking mechanism
- **SIGTSTP**: POSIX signal for terminal stop (Ctrl+Z)
- **SIGWINCH**: POSIX signal for window size change
