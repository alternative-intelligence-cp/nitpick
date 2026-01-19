# 6-Stream I/O Topology - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Core Design (Implementation in Progress)

---

## Table of Contents

1. [Overview](#overview)
2. [Design Rationale](#design-rationale)
3. [Stream Definitions](#stream-definitions)
4. [File Descriptor Mappings](#file-descriptor-mappings)
5. [Platform Implementation](#platform-implementation)
6. [Process Spawning Protocol](#process-spawning-protocol)
7. [Use Cases & Patterns](#use-cases--patterns)
8. [Performance Considerations](#performance-considerations)

---

## Overview

The **6-Stream I/O Topology** is a fundamental design principle of the Aria ecosystem. Where traditional Unix programs have 3 standard streams (stdin/stdout/stderr), Aria programs have **6 streams**:

| FD | Stream | Direction | Type | Purpose |
|----|--------|-----------|------|---------|
| 0 | **stdin** | Input | Text | Standard input |
| 1 | **stdout** | Output | Text | Standard output |
| 2 | **stderr** | Output | Text | Error messages |
| 3 | **stddbg** | Output | Text | Debug/diagnostic logs |
| 4 | **stddati** | Input | Binary | Binary data input |
| 5 | **stddato** | Output | Binary | Binary data output |

---

## Design Rationale

### Problem: Stream Pollution

**Traditional Unix (3 streams)**:
```bash
$ ./data_processor < input.bin > output.txt 2> errors.log
```

**Issues**:
1. **Mixing concerns**: stdout contains both user output AND debug logs
2. **Binary data conflicts**: Can't mix binary and text on same stream safely
3. **No structured logging**: Debug output pollutes stdout
4. **Shell limitations**: Can only redirect 3 streams

**Example Problem**:
```c
// C program
printf("Processing...\n");        // stdout (for user)
fprintf(stderr, "Debug: x=%d\n", x);  // stderr (for debug)
fwrite(binary_data, 1, 1024, stdout); // stdout (binary!) ← CONFLICT
```

Result: stdout now has text + binary mixed → unparseable

---

### Solution: Separate Streams by Purpose

**Aria (6 streams)**:
```bash
$ ./data_processor < input.bin 4< binary_input.dat > output.txt 2> errors.log 3> debug.log 5> binary_output.dat
```

**Benefits**:
1. **Clean separation**: User output (stdout), errors (stderr), debug (stddbg)
2. **Binary safety**: Binary streams (stddati/stddato) never mix with text
3. **Structured logging**: Debug logs go to stddbg, not stdout
4. **Composability**: Shell can route each stream independently

**Example Solution**:
```aria
// Aria program
io.stdout.write("Processing...\n");      // FD 1 (user output)
io.stddbg.write("Debug: x=" + x + "\n"); // FD 3 (debug log)
io.stddato.write(binary_data);           // FD 5 (binary output)
```

Result: Clean separation, no mixing

---

## Stream Definitions

### stdin (FD 0)

**Type**: Text input  
**Direction**: Input  
**Purpose**: User input, piped text data  
**API**: `io.stdin.read_line()`, `io.stdin.read_all()`

**Example**:
```aria
result<string>:line = io.stdin.read_line();
if (line.is_ok()) {
    io.stdout.write("You entered: " + line.ok());
}
```

**Shell Usage**:
```bash
$ echo "Hello" | ./my_app
You entered: Hello
```

---

### stdout (FD 1)

**Type**: Text output  
**Direction**: Output  
**Purpose**: Primary program output for users  
**API**: `io.stdout.write(text: string)`

**Example**:
```aria
io.stdout.write("Result: 42\n");
```

**Shell Usage**:
```bash
$ ./my_app > output.txt
$ cat output.txt
Result: 42
```

**Convention**: Use for final results, not intermediate logs

---

### stderr (FD 2)

**Type**: Text output  
**Direction**: Output  
**Purpose**: Error messages, warnings  
**API**: `io.stderr.write(text: string)`

**Example**:
```aria
if (file.is_err()) {
    io.stderr.write("Error: " + file.err() + "\n");
}
```

**Shell Usage**:
```bash
$ ./my_app 2> errors.log
$ cat errors.log
Error: File not found
```

**Convention**: Use for errors that users need to see

---

### stddbg (FD 3) - **NEW IN ARIA**

**Type**: Text output  
**Direction**: Output  
**Purpose**: Debug logs, diagnostic information  
**API**: `io.stddbg.write(text: string)`

**Rationale**: Separate debug output from user output

**Example**:
```aria
io.stddbg.write("Processing 1024 bytes\n");
io.stddbg.write("Memory allocated: " + mem_size + "\n");
io.stddbg.write("Checkpoint: function X entered\n");
```

**Shell Usage**:
```bash
# Normal run: Ignore debug output
$ ./my_app > output.txt 3> /dev/null

# Debug run: Capture debug logs
$ ./my_app > output.txt 3> debug.log
$ cat debug.log
Processing 1024 bytes
Memory allocated: 4096
Checkpoint: function X entered
```

**Convention**: Use for verbose logging that developers need, not end users

---

### stddati (FD 4) - **NEW IN ARIA**

**Type**: Binary input  
**Direction**: Input  
**Purpose**: Binary data input (images, compressed files, protocol buffers)  
**API**: `io.stddati.read(buffer: wild byte*, count: size)`

**Rationale**: Keep binary data separate from text streams

**Example**:
```aria
wild byte*:buffer = alloc(1024);
result<size>:bytes_read = io.stddati.read(buffer, 1024);
if (bytes_read.is_ok()) {
    // Process binary data
    process_binary(buffer, bytes_read.ok());
}
```

**Shell Usage**:
```bash
$ ./image_processor 4< input.png > output.txt
```

**Convention**: Use when input is binary (not UTF-8 text)

---

### stddato (FD 5) - **NEW IN ARIA**

**Type**: Binary output  
**Direction**: Output  
**Purpose**: Binary data output (images, compressed files, protocol buffers)  
**API**: `io.stddato.write(data: wild byte*, count: size)`

**Rationale**: Keep binary output separate from text

**Example**:
```aria
wild byte*:compressed_data = compress(input);
io.stddato.write(compressed_data, compressed_size);
```

**Shell Usage**:
```bash
$ ./compressor < input.txt 5> output.gz
```

**Convention**: Use when output is binary (not UTF-8 text)

---

## File Descriptor Mappings

### Standard Process Layout

```
Process Memory:
┌─────────────────────────────────────┐
│ Code Segment                        │
├─────────────────────────────────────┤
│ Data Segment                        │
├─────────────────────────────────────┤
│ Heap                                │
├─────────────────────────────────────┤
│ Stack                               │
└─────────────────────────────────────┘

File Descriptor Table:
┌────┬──────────┬──────────────────────┐
│ FD │ Stream   │ Points To            │
├────┼──────────┼──────────────────────┤
│ 0  │ stdin    │ Terminal / Pipe      │
│ 1  │ stdout   │ Terminal / Pipe      │
│ 2  │ stderr   │ Terminal / Pipe      │
│ 3  │ stddbg   │ Pipe / File / /dev/null │
│ 4  │ stddati  │ Pipe / File / /dev/null │
│ 5  │ stddato  │ Pipe / File / /dev/null │
│ 6+ │ (user)   │ Opened files         │
└────┴──────────┴──────────────────────┘
```

### Global Handles (Runtime)

**Aria Runtime** (`libaria_runtime.a`):
```c
// Global FD storage (internal to runtime)
static int io_stdin_fd = 0;
static int io_stdout_fd = 1;
static int io_stderr_fd = 2;
static int io_stddbg_fd = 3;
static int io_stddati_fd = 4;
static int io_stddato_fd = 5;

// Initialization (before main)
void aria_runtime_init(void) __attribute__((constructor));

void aria_runtime_init(void) {
    // Verify FDs 0-5 are open
    // If not, open /dev/null as fallback
    if (fcntl(io_stddbg_fd, F_GETFD) == -1) {
        io_stddbg_fd = open("/dev/null", O_WRONLY);
    }
    // Similar for stddati, stddato
}
```

---

## Platform Implementation

### Linux (Native Support)

**Kernel Behavior**:
- FDs 0-2: Standard (always present)
- FDs 3-5: Must be explicitly set up by parent process

**Process Spawning** (via aria_shell):
```c
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
    
    close_all_pipe_ends();
    execve(executable, argv, envp);
} else {
    // Parent: Close child ends, keep parent ends for I/O
    close(stdin_pipe[0]);  // ...etc
}
```

**Result**: Child process has FDs 0-5 set up correctly

---

### Windows (Bootstrap Protocol)

**Challenge**: Windows doesn't use Unix-style file descriptors

**Solution**: Custom bootstrap protocol

**Parent (aria_shell on Windows)**:
```cpp
// Create 6 pipes (Win32 HANDLEs)
HANDLE stdin_read, stdin_write, stdout_read, stdout_write, ...;
CreatePipe(&stdin_read, &stdin_write, &sa, 0);
CreatePipe(&stdout_read, &stdout_write, &sa, 0);
// ... create all 6 pipes ...

// Make child-side handles inheritable
SetHandleInformation(stdin_read, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stdout_write, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stderr_write, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stddbg_write, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stddati_read, HANDLE_FLAG_INHERIT, 1);
SetHandleInformation(stddato_write, HANDLE_FLAG_INHERIT, 1);

// Pass HANDLEs via environment variables
char env_vars[512];
sprintf(env_vars,
    "ARIA_STDIN_HANDLE=%lld;"
    "ARIA_STDOUT_HANDLE=%lld;"
    "ARIA_STDERR_HANDLE=%lld;"
    "ARIA_STDDBG_HANDLE=%lld;"
    "ARIA_STDDATI_HANDLE=%lld;"
    "ARIA_STDDATO_HANDLE=%lld",
    (long long)stdin_read,
    (long long)stdout_write,
    (long long)stderr_write,
    (long long)stddbg_write,
    (long long)stddati_read,
    (long long)stddato_write);

// Spawn child process
STARTUPINFOEX si = { ... };
CreateProcess(executable, args, NULL, NULL, TRUE, ..., env_vars, &si, ...);
```

**Child (Aria runtime on Windows)**:
```c
void aria_runtime_init(void) {
    // Parse environment variables
    const char* stdin_str = getenv("ARIA_STDIN_HANDLE");
    const char* stdout_str = getenv("ARIA_STDOUT_HANDLE");
    // ...
    
    if (stdin_str) {
        HANDLE h = (HANDLE)atoll(stdin_str);
        io_stdin_fd = _open_osfhandle((intptr_t)h, _O_RDONLY);
    }
    
    if (stddbg_str) {
        HANDLE h = (HANDLE)atoll(stddbg_str);
        io_stddbg_fd = _open_osfhandle((intptr_t)h, _O_WRONLY);
    }
    
    // ...similar for other streams
}
```

**Result**: Child process has FDs 0-5 mapped to Windows handles

---

### macOS (Similar to Linux)

**Implementation**: Same as Linux (POSIX fork/exec)

**Differences**: None (macOS supports FDs 0-5 natively)

---

## Process Spawning Protocol

### From aria_shell (Parent)

**Spawning a Child with 6 Streams**:

1. **Create 6 Pipes**:
   - Each pipe has read/write ends
   - 12 file descriptors total (6 pipes × 2 ends)

2. **Fork Process** (Linux/macOS):
   - Parent keeps: stdin write, stdout read, stderr read, stddbg read, stddati write, stddato read
   - Child keeps: stdin read, stdout write, stderr write, stddbg write, stddati read, stddato write

3. **Remap FDs in Child**:
   - `dup2(stdin_pipe[0], 0)`
   - `dup2(stdout_pipe[1], 1)`
   - `dup2(stderr_pipe[1], 2)`
   - `dup2(stddbg_pipe[1], 3)`
   - `dup2(stddati_pipe[0], 4)`
   - `dup2(stddato_pipe[1], 5)`

4. **Execute Program**: `execve(executable, argv, envp)`

5. **Parent Drains Streams**:
   - Spawn drain threads for stdout, stderr, stddbg, stddato
   - Feed stdin, stddati from files/variables

---

### Drain Workers (Shell Side)

**Purpose**: Read from child's output streams without blocking

**Implementation** (one thread per output stream):
```cpp
void drain_stdout(int fd) {
    char buffer[4096];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) break;  // EOF or error
        terminal_write(buffer, n);  // Display to terminal
    }
}

void drain_stderr(int fd) {
    char buffer[4096];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) break;
        terminal_write_red(buffer, n);  // Display in red
    }
}

void drain_stddbg(int fd) {
    char buffer[4096];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) break;
        ring_buffer_append(debug_log, buffer, n);  // Store for later
    }
}

void drain_stddato(int fd) {
    uint8_t buffer[4096];
    while (true) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) break;
        wild_buffer_append(binary_output, buffer, n);  // Accumulate
    }
}
```

**Threading**:
```cpp
std::thread t1(drain_stdout, stdout_fd);
std::thread t2(drain_stderr, stderr_fd);
std::thread t3(drain_stddbg, stddbg_fd);
std::thread t4(drain_stddato, stddato_fd);
```

---

## Use Cases & Patterns

### Use Case 1: Data Processing Pipeline

**Scenario**: Transform binary data, output results to text

**Command**:
```bash
$ ./process_data 4< input.bin > results.txt 5> processed.bin 3> debug.log
```

**Program**:
```aria
func:main = int64() {
    wild byte*:buffer = alloc(4096);
    
    while (true) {
        result<size>:n = io.stddati.read(buffer, 4096);
        if (n.is_err() || n.ok() == 0) break;  // EOF
        
        io.stddbg.write("Read " + n.ok() + " bytes\n");
        
        wild byte*:processed = transform(buffer, n.ok());
        io.stddato.write(processed, n.ok());
        
        io.stdout.write("Processed chunk\n");
    }
    
    pass(0);
}
```

**Streams**:
- **stdin**: Unused
- **stdout**: "Processed chunk\n" × N (user feedback)
- **stderr**: Unused (no errors)
- **stddbg**: "Read 4096 bytes\n" × N (debug log)
- **stddati**: Raw binary input (FD 4 < input.bin)
- **stddato**: Processed binary output (FD 5 > processed.bin)

---

### Use Case 2: Logging & Debugging

**Scenario**: Web server with structured logging

**Command**:
```bash
$ ./web_server 3> debug.log &
$ tail -f debug.log  # Watch debug logs in real-time
```

**Program**:
```aria
func:handle_request = void(req: request) {
    io.stddbg.write("[DEBUG] Handling request: " + req.url + "\n");
    
    result<response>:resp = process(req);
    
    if (resp.is_err()) {
        io.stderr.write("[ERROR] " + resp.err() + "\n");
        io.stddbg.write("[DEBUG] Stack trace: ...\n");
    } else {
        io.stdout.write("[INFO] Request completed\n");
        io.stddbg.write("[DEBUG] Response size: " + resp.ok().size + "\n");
    }
}
```

**Streams**:
- **stdout**: High-level info logs
- **stderr**: Errors (for monitoring)
- **stddbg**: Detailed debug logs (for developers)

---

### Use Case 3: Multi-Stage Pipeline

**Scenario**: Generator → Processor → Consumer

**Command**:
```bash
$ ./generator |[5→4]| ./processor |[5→4]| ./consumer > final.txt
```

(Where `|[5→4]|` means "pipe stddato of left to stddati of right")

**generator**:
```aria
func:main = int64() {
    wild byte*:data = generate();
    io.stddato.write(data, size);
    pass(0);
}
```

**processor**:
```aria
func:main = int64() {
    wild byte*:buffer = alloc(4096);
    result<size>:n = io.stddati.read(buffer, 4096);
    wild byte*:processed = process(buffer, n.ok());
    io.stddato.write(processed, n.ok());
    pass(0);
}
```

**consumer**:
```aria
func:main = int64() {
    wild byte*:buffer = alloc(4096);
    result<size>:n = io.stddati.read(buffer, 4096);
    io.stdout.write(format(buffer, n.ok()));
    pass(0);
}
```

**Result**: Binary data flows through pipeline, text output at end

---

## Performance Considerations

### Buffering

**Text Streams** (stdout, stderr, stddbg):
- Line-buffered by default
- Flush on `\n` or explicit flush

**Binary Streams** (stddati, stddato):
- No buffering (raw bytes)
- Direct read/write to FD

### Async I/O

**Future Enhancement**: Non-blocking I/O on all 6 streams

**Implementation**: `epoll` (Linux), `kqueue` (macOS), IOCP (Windows)

**Benefit**: Single-threaded async server can handle multiple streams efficiently

---

## Related Documents

- **[ECOSYSTEM_OVERVIEW](../ECOSYSTEM_OVERVIEW.md)**: System-wide design decisions
- **[INTERFACES](../INTERFACES.md)**: Shell ↔ Runtime spawning protocol
- **[DATA_FLOW](../DATA_FLOW.md)**: Execution traces showing 6-stream usage
- **[ARIA_SHELL](../components/ARIA_SHELL.md)**: Shell implementation details
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime I/O API

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Core specification (reference implementation in progress)
