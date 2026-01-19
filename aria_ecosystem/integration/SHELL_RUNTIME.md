# Shell ↔ Runtime Integration Guide

**Document Type**: Integration Guide  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Reference Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Process Spawning Deep-Dive](#process-spawning-deep-dive)
3. [Pipe Topology](#pipe-topology)
4. [Drain Workers](#drain-workers)
5. [Stream Inheritance](#stream-inheritance)
6. [Variable Interpolation](#variable-interpolation)
7. [Job Control](#job-control)

---

## Overview

The **aria_shell** (AriaSH) manages the execution of Aria programs by:

1. **Spawning processes** with 6 streams properly configured
2. **Draining output** without blocking
3. **Feeding input** from variables/files
4. **Managing job lifecycle** (foreground/background)

The **libaria_runtime.a** provides the I/O primitives that these spawned processes use.

---

## Process Spawning Deep-Dive

### Linux/macOS Implementation

**High-Level Flow**:
```
Shell                          Child Process
  |                                |
  | fork()  -------------------->  |
  | (creates copy)                 |
  |                                |
  | (parent continues)             | (child runs)
  |                                |
  | -- dup2() remapping -->        |
  |    (sets FDs 0-5)              |
  |                                |
  |                                | execve()
  |                                | (becomes Aria program)
  |                                |
  | -- drain threads -->           |
  | (read stdout, stderr, ...)    | (writes to FDs 1-5)
```

---

### Step-by-Step Process Creation

**1. Create 6 Pipes**:
```cpp
int stdin_pipe[2];   // [0] = read, [1] = write
int stdout_pipe[2];
int stderr_pipe[2];
int stddbg_pipe[2];
int stddati_pipe[2];
int stddato_pipe[2];

pipe2(stdin_pipe, O_CLOEXEC);   // Create pipe, close-on-exec
pipe2(stdout_pipe, O_CLOEXEC);
pipe2(stderr_pipe, O_CLOEXEC);
pipe2(stddbg_pipe, O_CLOEXEC);
pipe2(stddati_pipe, O_CLOEXEC);
pipe2(stddato_pipe, O_CLOEXEC);
```

**Result**: 12 file descriptors (6 pipes × 2 ends each)

---

**2. Fork Process**:
```cpp
pid_t pid = fork();

if (pid == -1) {
    // Error
    perror("fork");
    return -1;
}
```

**After fork()**:
- **Parent**: `pid` = child's PID
- **Child**: `pid` = 0

**Memory**: Child is a copy of parent (copy-on-write)

---

**3. Child: Remap File Descriptors**:
```cpp
if (pid == 0) {
    // Child process
    
    // Remap stdin
    dup2(stdin_pipe[0], STDIN_FILENO);   // FD 0
    close(stdin_pipe[0]);
    close(stdin_pipe[1]);
    
    // Remap stdout
    dup2(stdout_pipe[1], STDOUT_FILENO);  // FD 1
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    
    // Remap stderr
    dup2(stderr_pipe[1], STDERR_FILENO);  // FD 2
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    
    // Remap stddbg
    dup2(stddbg_pipe[1], 3);  // FD 3
    close(stddbg_pipe[0]);
    close(stddbg_pipe[1]);
    
    // Remap stddati
    dup2(stddati_pipe[0], 4);  // FD 4
    close(stddati_pipe[0]);
    close(stddati_pipe[1]);
    
    // Remap stddato
    dup2(stddato_pipe[1], 5);  // FD 5
    close(stddato_pipe[0]);
    close(stddato_pipe[1]);
    
    // Execute Aria program
    execve(executable_path, argv, envp);
    
    // Only reached if execve fails
    perror("execve");
    _exit(127);
}
```

**Result**: Child's FDs 0-5 point to pipe ends

---

**4. Parent: Close Child Ends, Keep Parent Ends**:
```cpp
// Parent process

// Close child ends
close(stdin_pipe[0]);   // Child reads stdin, we write
close(stdout_pipe[1]);  // Child writes stdout, we read
close(stderr_pipe[1]);
close(stddbg_pipe[1]);
close(stddati_pipe[0]);  // We write stddati, child reads
close(stddato_pipe[1]);  // Child writes stddato, we read

// Save parent ends for I/O
int stdin_write_fd = stdin_pipe[1];
int stdout_read_fd = stdout_pipe[0];
int stderr_read_fd = stderr_pipe[0];
int stddbg_read_fd = stddbg_pipe[0];
int stddati_write_fd = stddati_pipe[1];
int stddato_read_fd = stddato_pipe[0];
```

---

**5. Parent: Spawn Drain Threads**:
```cpp
std::thread t_stdout(drain_stream, stdout_read_fd, "stdout");
std::thread t_stderr(drain_stream, stderr_read_fd, "stderr");
std::thread t_stddbg(drain_stream, stddbg_read_fd, "stddbg");
std::thread t_stddato(drain_binary, stddato_read_fd, "stddato");

t_stdout.detach();
t_stderr.detach();
t_stddbg.detach();
t_stddato.detach();
```

**Purpose**: Read from child's output streams without blocking main thread

---

**6. Parent: Wait for Child**:
```cpp
int status;
waitpid(pid, &status, 0);

if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    std::cout << "Process exited with code " << exit_code << std::endl;
}
```

---

### Windows Implementation

**Challenge**: Windows doesn't have `fork()` or Unix-style FDs

**Solution**: Custom bootstrap protocol (see [IO_TOPOLOGY](../specs/IO_TOPOLOGY.md#windows-bootstrap-protocol))

**Summary**:
1. Create 6 Windows pipes (`HANDLE`)
2. Pass handles to child via environment variables
3. Child runtime reads env vars, maps handles to FDs

---

## Pipe Topology

### Visual Representation

```
Shell (Parent Process)
┌───────────────────────────────────────────────────────────┐
│                                                           │
│  stdin_write_fd ───────┐                                 │
│                        │                                 │
│  stdout_read_fd ◄──────┤                                 │
│  stderr_read_fd ◄──────┤                                 │
│  stddbg_read_fd ◄──────┤  [Drain Threads]               │
│  stddato_read_fd ◄─────┤                                 │
│                        │                                 │
│  stddati_write_fd ─────┤                                 │
│                        │                                 │
└────────────────────────┼──────────────────────────────────┘
                         │
                         │ (Pipes)
                         │
┌────────────────────────┼──────────────────────────────────┐
│                        │                                 │
│                        │  Child Process (Aria Program)   │
│                        │                                 │
│   FD 0 (stdin) ◄───────┤                                 │
│   FD 1 (stdout) ───────►                                 │
│   FD 2 (stderr) ───────►                                 │
│   FD 3 (stddbg) ───────►                                 │
│   FD 4 (stddati) ◄─────┤                                 │
│   FD 5 (stddato) ───────►                                │
│                                                           │
│  [libaria_runtime.a]                                     │
│    - aria_stdin_read_line() reads from FD 0              │
│    - aria_stdout_write() writes to FD 1                  │
│    - ...                                                 │
│                                                           │
└───────────────────────────────────────────────────────────┘
```

---

### Data Flow Example

**Command**: `$ echo "hello" | my_prog > output.txt 3> debug.log`

**Setup**:
```
Shell:
  - stdin_write_fd: Write "hello\n"
  - stdout_read_fd: Redirect to output.txt
  - stderr_read_fd: Display on terminal
  - stddbg_read_fd: Redirect to debug.log
  - stddati_write_fd: Closed (no binary input)
  - stddato_read_fd: Closed (no binary output)

Child (my_prog):
  - FD 0 (stdin): Read "hello\n"
  - FD 1 (stdout): Write to pipe → shell → output.txt
  - FD 2 (stderr): Write to pipe → shell → terminal
  - FD 3 (stddbg): Write to pipe → shell → debug.log
  - FD 4 (stddati): Closed
  - FD 5 (stddato): Closed
```

---

## Drain Workers

### Purpose

**Problem**: If child writes to stdout/stderr but parent doesn't read, pipe buffer fills → child blocks

**Solution**: Dedicated threads to continuously drain output streams

---

### Implementation

**Drain Thread (Text Streams)**:
```cpp
void drain_stream(int fd, const std::string& stream_name) {
    char buffer[4096];
    ssize_t n;
    
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        // Process data
        if (stream_name == "stdout") {
            // Write to terminal
            write(STDOUT_FILENO, buffer, n);
        } else if (stream_name == "stderr") {
            // Write to terminal in red
            std::cerr << "\033[31m";  // Red color
            write(STDERR_FILENO, buffer, n);
            std::cerr << "\033[0m";   // Reset color
        } else if (stream_name == "stddbg") {
            // Append to debug log buffer
            debug_log.append(buffer, n);
        }
    }
    
    // EOF or error
    close(fd);
}
```

---

**Drain Thread (Binary Streams)**:
```cpp
void drain_binary(int fd, const std::string& stream_name) {
    uint8_t buffer[4096];
    ssize_t n;
    
    std::vector<uint8_t> binary_output;
    
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        // Accumulate binary data
        binary_output.insert(binary_output.end(), buffer, buffer + n);
    }
    
    // Write to file
    if (stream_name == "stddato") {
        write_binary_file("stddato_output.bin", binary_output);
    }
    
    close(fd);
}
```

---

### Thread Lifecycle

**Startup**: Threads created after `fork()` but before `waitpid()`

**Shutdown**: Threads terminate when child closes output FDs (EOF)

**Detachment**: Threads are detached (run independently)

---

## Stream Inheritance

### Shell Variables to Child Streams

**Scenario**: Pass shell variable as input to child

**Example**:
```bash
$ var:input = "Hello, World!"
$ echo $input | my_prog
```

---

**Implementation**:
```cpp
// Shell has variable: input = "Hello, World!"
std::string input_data = variables["input"];

// Spawn child with stdin from variable
pid_t pid = fork();
if (pid == 0) {
    // Child
    dup2(stdin_pipe[0], STDIN_FILENO);
    execve("my_prog", ...);
} else {
    // Parent: Write variable to stdin pipe
    write(stdin_write_fd, input_data.c_str(), input_data.size());
    close(stdin_write_fd);  // EOF
}
```

**Result**: Child's `io.stdin.read_line()` reads "Hello, World!"

---

### Child Output to Shell Variables

**Scenario**: Capture child's stdout in shell variable

**Example**:
```bash
$ var:output = $(my_prog)
$ echo $output
```

**Implementation**:
```cpp
std::string captured_stdout;

void drain_to_variable(int fd) {
    char buffer[4096];
    ssize_t n;
    
    while ((n = read(fd, buffer, sizeof(buffer))) > 0) {
        captured_stdout.append(buffer, n);
    }
}

// Spawn child
std::thread t_stdout(drain_to_variable, stdout_read_fd);
waitpid(pid, &status, 0);
t_stdout.join();

// Store in variable
variables["output"] = captured_stdout;
```

---

## Variable Interpolation

### String Interpolation in Commands

**Syntax**: `$variable_name` or `${variable_name}`

**Example**:
```bash
$ var:name = "Alice"
$ echo "Hello, $name!"
# Output: Hello, Alice!
```

---

### Implementation in Shell

**Lexer**: Detect `$` tokens

**Parser**: Replace with variable value

**Example**:
```cpp
std::string interpolate(const std::string& input) {
    std::string result;
    size_t i = 0;
    
    while (i < input.size()) {
        if (input[i] == '$') {
            // Extract variable name
            size_t j = i + 1;
            while (j < input.size() && isalnum(input[j])) j++;
            
            std::string var_name = input.substr(i + 1, j - i - 1);
            
            // Look up variable
            if (variables.count(var_name)) {
                result += variables[var_name];
            } else {
                result += "$" + var_name;  // Keep literal
            }
            
            i = j;
        } else {
            result += input[i];
            i++;
        }
    }
    
    return result;
}
```

---

### Stream Variable Interpolation

**Syntax**: `<$variable` (input from variable)

**Example**:
```bash
$ var:data = "binary data here"
$ my_prog 4<$data
```

**Implementation**: Write variable contents to `stddati` pipe

---

## Job Control

### Foreground vs Background

**Foreground**: Shell waits for process to complete

**Background**: Shell continues immediately (process runs in background)

**Syntax**: `command &` (ampersand = background)

---

### Foreground Execution

```cpp
void run_foreground(const std::vector<std::string>& args) {
    pid_t pid = spawn_process(args);
    
    // Wait for child
    int status;
    waitpid(pid, &status, 0);
    
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            std::cerr << "Process exited with code " << exit_code << std::endl;
        }
    }
}
```

---

### Background Execution

```cpp
struct Job {
    pid_t pid;
    std::string command;
    bool running;
};

std::vector<Job> background_jobs;

void run_background(const std::vector<std::string>& args) {
    pid_t pid = spawn_process(args);
    
    // Don't wait, add to job list
    background_jobs.push_back({
        .pid = pid,
        .command = join(args),
        .running = true
    });
    
    std::cout << "[" << background_jobs.size() << "] " << pid << std::endl;
}
```

---

### Job Management Commands

**`jobs`**: List background jobs
```cpp
void list_jobs() {
    for (size_t i = 0; i < background_jobs.size(); i++) {
        Job& job = background_jobs[i];
        
        // Check if still running
        int status;
        pid_t result = waitpid(job.pid, &status, WNOHANG);
        
        if (result == 0) {
            // Still running
            std::cout << "[" << (i + 1) << "]  Running  " << job.command << std::endl;
        } else {
            // Finished
            job.running = false;
            std::cout << "[" << (i + 1) << "]  Done     " << job.command << std::endl;
        }
    }
}
```

**`fg <job_id>`**: Bring job to foreground
```cpp
void foreground_job(int job_id) {
    if (job_id < 1 || job_id > background_jobs.size()) {
        std::cerr << "Invalid job ID" << std::endl;
        return;
    }
    
    Job& job = background_jobs[job_id - 1];
    
    // Wait for job
    int status;
    waitpid(job.pid, &status, 0);
    job.running = false;
}
```

---

## Related Documents

- **[ARIA_SHELL](../components/ARIA_SHELL.md)**: Shell architecture
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime I/O implementation
- **[IO_TOPOLOGY](../specs/IO_TOPOLOGY.md)**: 6-stream design
- **[INTERFACES](../INTERFACES.md)**: Shell ↔ Runtime API
- **[DATA_FLOW](../DATA_FLOW.md)**: Process spawning execution trace

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Reference guide (implementation in progress, research complete 15/15)
