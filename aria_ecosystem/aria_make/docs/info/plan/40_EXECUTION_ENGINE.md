# Execution Engine Components

This document provides detailed specifications for the parallel build execution subsystem, including the thread pool, process spawning, build scheduler, and toolchain orchestration.

---

## ThreadPool - Fixed-Size Worker Pool

### Purpose
Manage a fixed number of worker threads that execute build tasks in parallel without oversubscribing the CPU.

### Design Rationale

**Why NOT `std::async` or per-task threads?**

| Approach | Problem |
|----------|---------|
| Per-task threads | 1000 tasks → 1000 threads → system overload, context switching storm |
| `std::async` | Implementation-defined thread management, may spawn unlimited threads |
| Fixed thread pool | Predictable resource usage, optimal CPU utilization |

**Optimal pool size:** `std::thread::hardware_concurrency()` (number of logical cores)

### Class Structure

```cpp
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();
    
    // Delete copy/move (pool manages unique resources)
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    
    // Submit task for execution
    template<typename F>
    void enqueue(F&& task);
    
    // Wait for all pending tasks to complete
    void waitAll();
    
    // Graceful shutdown
    void shutdown();
    
    // Status
    size_t pendingTasks() const;
    size_t activeThreads() const { return workers_.size(); }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_;
    std::atomic<size_t> active_tasks_;
    
    void workerLoop();
};
```

### Implementation

```cpp
ThreadPool::ThreadPool(size_t num_threads)
    : stop_flag_(false)
    , active_tasks_(0)
{
    workers_.reserve(num_threads);
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

template<typename F>
void ThreadPool::enqueue(F&& task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        if (stop_flag_) {
            throw std::runtime_error("Cannot enqueue task: pool is shutting down");
        }
        
        tasks_.emplace(std::forward<F>(task));
    }
    
    condition_.notify_one();  // Wake up one worker
}

void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // Wait for task or stop signal
            condition_.wait(lock, [this]() {
                return stop_flag_ || !tasks_.empty();
            });
            
            if (stop_flag_ && tasks_.empty()) {
                return;  // Shutdown with no pending tasks
            }
            
            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }
        
        // Execute task (outside lock!)
        if (task) {
            active_tasks_++;
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "[ThreadPool] Task threw exception: " << e.what() << "\n";
            }
            active_tasks_--;
        }
    }
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_flag_ = true;
    }
    
    condition_.notify_all();  // Wake all workers
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

void ThreadPool::waitAll() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (tasks_.empty() && active_tasks_ == 0) {
            break;
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
```

### Key Design Points

**1. Lock Scope Minimization**
```cpp
// ❌ BAD: Execute task while holding lock
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task = tasks_.front();
    task();  // ← Blocks all other threads!
}

// ✅ GOOD: Release lock before executing
{
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task = tasks_.front();
}
task();  // Execute outside lock
```

**2. Condition Variable Efficiency**
```cpp
// Wait efficiently (no busy-wait)
condition_.wait(lock, [this]() {
    return stop_flag_ || !tasks_.empty();
});

// Alternative (busy-wait, wasteful):
while (!stop_flag_ && tasks_.empty()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}
```

**3. Exception Safety**
```cpp
try {
    task();
} catch (const std::exception& e) {
    // Don't let exceptions kill worker thread
    std::cerr << "[ThreadPool] Task threw exception: " << e.what() << "\n";
}
```

---

## Process Execution (PAL - Platform Abstraction Layer)

### Purpose
Spawn child processes (compiler, linker) with stdout/stderr capture, cross-platform.

### Critical Problem: Pipe Deadlock

**Scenario:**
1. Parent spawns child process with piped stdout/stderr
2. Child writes 1MB of output to stderr
3. Pipe buffer fills (typically 64KB on Linux)
4. Child blocks waiting for parent to drain pipe
5. Parent blocks in `waitpid()` waiting for child to exit
6. **DEADLOCK** - both processes sleeping forever

**Solution:** Always spawn drain threads

### Class Structure

```cpp
struct ExecOptions {
    std::optional<std::filesystem::path> working_dir;
    std::optional<unsigned> timeout_seconds;
    std::map<std::string, std::string> env_vars;
    bool capture_stdout = true;
    bool capture_stderr = true;
};

struct ExecResult {
    int exit_code;
    std::string stdout_output;
    std::string stderr_output;
    bool timed_out = false;
    
    bool success() const { return exit_code == 0 && !timed_out; }
};

class ProcessExecutor {
public:
    static ExecResult execute(
        const std::string& binary,
        const std::vector<std::string>& args,
        const ExecOptions& opts = {}
    );
    
private:
#ifdef _WIN32
    static ExecResult executeWindows(const std::string& binary,
                                     const std::vector<std::string>& args,
                                     const ExecOptions& opts);
#else
    static ExecResult executePosix(const std::string& binary,
                                   const std::vector<std::string>& args,
                                   const ExecOptions& opts);
#endif
    
    static void drainPipe(int fd, std::string& output);
    static std::string buildCommandLine(const std::string& binary,
                                       const std::vector<std::string>& args);
};
```

### POSIX Implementation

```cpp
ExecResult ProcessExecutor::executePosix(
    const std::string& binary,
    const std::vector<std::string>& args,
    const ExecOptions& opts
) {
    ExecResult result;
    
    // 1. Create pipes
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        throw std::runtime_error("Failed to create pipes");
    }
    
    // 2. Fork process
    pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error("Failed to fork");
    }
    
    if (pid == 0) {
        // === CHILD PROCESS ===
        
        // Redirect stdout/stderr
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        // Close all pipe ends in child
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        
        // Change working directory
        if (opts.working_dir) {
            if (chdir(opts.working_dir->c_str()) != 0) {
                std::cerr << "Failed to change directory\n";
                exit(127);
            }
        }
        
        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        // Execute
        execvp(binary.c_str(), argv.data());
        
        // If we get here, exec failed
        std::cerr << "Failed to execute: " << binary << "\n";
        exit(127);
    }
    
    // === PARENT PROCESS ===
    
    // ⚠️ CRITICAL: Close write ends in parent
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // ✅ Spawn drain threads (prevents deadlock)
    std::thread stdout_thread([&]() {
        drainPipe(stdout_pipe[0], result.stdout_output);
        close(stdout_pipe[0]);
    });
    
    std::thread stderr_thread([&]() {
        drainPipe(stderr_pipe[0], result.stderr_output);
        close(stderr_pipe[0]);
    });
    
    // Wait for child with timeout
    if (opts.timeout_seconds) {
        auto deadline = std::chrono::steady_clock::now() + 
                       std::chrono::seconds(*opts.timeout_seconds);
        
        while (std::chrono::steady_clock::now() < deadline) {
            int status;
            pid_t wait_result = waitpid(pid, &status, WNOHANG);
            
            if (wait_result == pid) {
                // Child exited
                result.exit_code = WEXITSTATUS(status);
                break;
            } else if (wait_result < 0) {
                throw std::runtime_error("waitpid failed");
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Check if timed out
        if (std::chrono::steady_clock::now() >= deadline) {
            kill(pid, SIGTERM);  // Try graceful
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            kill(pid, SIGKILL);  // Force kill
            result.timed_out = true;
        }
    } else {
        // No timeout: wait indefinitely
        int status;
        waitpid(pid, &status, 0);
        result.exit_code = WEXITSTATUS(status);
    }
    
    // Join drain threads
    stdout_thread.join();
    stderr_thread.join();
    
    return result;
}

void ProcessExecutor::drainPipe(int fd, std::string& output) {
    char buffer[4096];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        output.append(buffer, bytes_read);
    }
}
```

### Windows Implementation

```cpp
ExecResult ProcessExecutor::executeWindows(
    const std::string& binary,
    const std::vector<std::string>& args,
    const ExecOptions& opts
) {
    ExecResult result;
    
    // 1. Create pipes
    HANDLE stdout_read, stdout_write;
    HANDLE stderr_read, stderr_write;
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    CreatePipe(&stdout_read, &stdout_write, &sa, 0);
    CreatePipe(&stderr_read, &stderr_write, &sa, 0);
    
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(stderr_read, HANDLE_FLAG_INHERIT, 0);
    
    // 2. Build command line (Windows requires single string)
    std::string cmd_line = buildCommandLine(binary, args);
    std::wstring cmd_line_wide(cmd_line.begin(), cmd_line.end());
    
    // 3. Setup process
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    
    PROCESS_INFORMATION pi = {};
    
    // 4. Create process
    if (!CreateProcessW(
        NULL,
        &cmd_line_wide[0],
        NULL, NULL,
        TRUE,  // Inherit handles
        0,
        NULL,
        opts.working_dir ? opts.working_dir->wstring().c_str() : NULL,
        &si,
        &pi
    )) {
        throw std::runtime_error("CreateProcess failed");
    }
    
    // ⚠️ CRITICAL: Close write ends in parent
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
    
    // ✅ Spawn drain threads
    std::thread stdout_thread([&]() {
        char buffer[4096];
        DWORD bytes_read;
        while (ReadFile(stdout_read, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            result.stdout_output.append(buffer, bytes_read);
        }
        CloseHandle(stdout_read);
    });
    
    std::thread stderr_thread([&]() {
        char buffer[4096];
        DWORD bytes_read;
        while (ReadFile(stderr_read, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
            result.stderr_output.append(buffer, bytes_read);
        }
        CloseHandle(stderr_read);
    });
    
    // 5. Wait for process
    DWORD timeout_ms = opts.timeout_seconds 
        ? (*opts.timeout_seconds * 1000)
        : INFINITE;
    
    DWORD wait_result = WaitForSingleObject(pi.hProcess, timeout_ms);
    
    if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        result.timed_out = true;
    } else {
        DWORD exit_code;
        GetExitCodeProcess(pi.hProcess, &exit_code);
        result.exit_code = static_cast<int>(exit_code);
    }
    
    // Cleanup
    stdout_thread.join();
    stderr_thread.join();
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return result;
}

std::string ProcessExecutor::buildCommandLine(
    const std::string& binary,
    const std::vector<std::string>& args
) {
    std::ostringstream oss;
    oss << "\"" << binary << "\"";
    
    for (const auto& arg : args) {
        oss << " ";
        if (arg.find(' ') != std::string::npos) {
            oss << "\"" << arg << "\"";
        } else {
            oss << arg;
        }
    }
    
    return oss.str();
}
```

---

## ToolchainOrchestrator - Command Synthesis

### Purpose
Translate build targets into compiler/linker command invocations.

### Class Structure

```cpp
class ToolchainOrchestrator {
public:
    explicit ToolchainOrchestrator();
    
    // Configure toolchain paths
    void setCompilerPath(const std::filesystem::path& path);
    void setLinkerPath(const std::filesystem::path& path);
    
    // Command generation
    std::pair<std::string, std::vector<std::string>>
    buildCompileCommand(
        Node* node,
        const std::filesystem::path& source_file,
        const std::filesystem::path& output_file
    );
    
    std::pair<std::string, std::vector<std::string>>
    buildLinkCommand(
        Node* node,
        const std::vector<std::filesystem::path>& object_files
    );
    
private:
    std::filesystem::path compiler_path_ = "ariac";
    std::filesystem::path linker_path_ = "lld";
    
    std::vector<std::string> buildIncludePaths(Node* node);
    std::vector<std::string> buildLibraryPaths(Node* node);
    std::string getPlatformLinkerFlags();
};
```

### Compile Command Generation

```cpp
std::pair<std::string, std::vector<std::string>>
ToolchainOrchestrator::buildCompileCommand(
    Node* node,
    const fs::path& source_file,
    const fs::path& output_file
) {
    std::vector<std::string> args;
    
    // Input file
    args.push_back(source_file.string());
    
    // Output file
    args.push_back("-o");
    args.push_back(output_file.string());
    
    // Direct object emission (skip assembly)
    args.push_back("--emit-obj");
    
    // Include paths from dependencies
    for (const auto& inc : buildIncludePaths(node)) {
        args.push_back("-I");
        args.push_back(inc);
    }
    
    // User-specified flags
    for (const auto& flag : node->compile_flags) {
        args.push_back(flag);
    }
    
    // Generate dependency file (.d)
    args.push_back("-MD");
    args.push_back("-MF");
    args.push_back(output_file.string() + ".d");
    
    return {compiler_path_.string(), args};
}

std::vector<std::string> ToolchainOrchestrator::buildIncludePaths(Node* node) {
    std::vector<std::string> paths;
    
    for (Node* dep : node->dependencies()) {
        // Add dependency's output directory to include path
        fs::path dep_dir = fs::path(dep->output_file).parent_path();
        paths.push_back(dep_dir.string());
    }
    
    return paths;
}
```

### Link Command Generation

```cpp
std::pair<std::string, std::vector<std::string>>
ToolchainOrchestrator::buildLinkCommand(
    Node* node,
    const std::vector<fs::path>& object_files
) {
    std::vector<std::string> args;
    
    // Output file
    args.push_back("-o");
    args.push_back(node->output_file.string());
    
    // Input object files
    for (const auto& obj : object_files) {
        args.push_back(obj.string());
    }
    
    // Library paths from dependencies
    for (const auto& lib_path : buildLibraryPaths(node)) {
        args.push_back("-L");
        args.push_back(lib_path);
    }
    
    // Link against dependencies
    for (Node* dep : node->dependencies()) {
        if (dep->type() == Node::TargetType::STATIC_LIBRARY ||
            dep->type() == Node::TargetType::SHARED_LIBRARY) {
            args.push_back(dep->output_file.string());
        }
    }
    
    // User-specified link flags
    for (const auto& flag : node->link_flags) {
        args.push_back(flag);
    }
    
    // Platform-specific flags
    std::string platform_flags = getPlatformLinkerFlags();
    if (!platform_flags.empty()) {
        args.push_back(platform_flags);
    }
    
    return {linker_path_.string(), args};
}

std::string ToolchainOrchestrator::getPlatformLinkerFlags() {
#ifdef __linux__
    return "-fuse-ld=lld";  // Use LLD on Linux
#elif defined(_WIN32)
    return "/INCREMENTAL:NO";  // Disable incremental linking
#elif defined(__APPLE__)
    return "-fuse-ld=lld";  // Use LLD on macOS
#else
    return "";
#endif
}
```

---

## BuildScheduler - Kahn's Algorithm Integration

### Purpose
Orchestrate parallel compilation using Kahn's topological sort algorithm.

### Class Structure

```cpp
class BuildScheduler {
public:
    BuildScheduler(
        DependencyGraph& graph,
        ThreadPool& pool,
        ToolchainOrchestrator& toolchain
    );
    
    struct BuildOptions {
        bool incremental = true;
        bool verbose = false;
        bool stop_on_error = true;
        unsigned max_jobs = std::thread::hardware_concurrency();
    };
    
    bool build(const BuildOptions& opts = {});
    
    // Statistics
    struct Stats {
        size_t tasks_total = 0;
        size_t tasks_completed = 0;
        size_t tasks_failed = 0;
        size_t tasks_skipped = 0;  // Up-to-date
    };
    
    const Stats& getStats() const { return stats_; }
    
private:
    DependencyGraph& graph_;
    ThreadPool& pool_;
    ToolchainOrchestrator& toolchain_;
    
    Stats stats_;
    std::atomic<bool> build_failed_;
    std::mutex completion_mutex_;
    std::condition_variable completion_cv_;
    std::atomic<size_t> pending_tasks_;
    
    void executeNode(Node* node, const BuildOptions& opts);
    void onTaskComplete(Node* node, const ExecResult& result, const BuildOptions& opts);
    void propagateFailure(Node* node);
};
```

### Build Algorithm

```cpp
bool BuildScheduler::build(const BuildOptions& opts) {
    // Reset state
    graph_.resetState();
    stats_ = Stats{};
    build_failed_ = false;
    pending_tasks_ = 0;
    
    // Count total tasks
    for (Node* node : graph_.getAllNodes()) {
        if (node->is_dirty()) {
            stats_.tasks_total++;
        }
    }
    
    // Find initial ready nodes (in-degree == 0)
    std::vector<Node*> ready_nodes;
    for (Node* node : graph_.getAllNodes()) {
        if (node->get_in_degree() == 0) {
            ready_nodes.push_back(node);
        }
    }
    
    // Submit initial batch
    for (Node* node : ready_nodes) {
        pending_tasks_++;
        pool_.enqueue([this, node, opts]() {
            executeNode(node, opts);
        });
    }
    
    // Wait for completion
    {
        std::unique_lock<std::mutex> lock(completion_mutex_);
        completion_cv_.wait(lock, [this]() {
            return pending_tasks_ == 0;
        });
    }
    
    return !build_failed_;
}

void BuildScheduler::executeNode(Node* node, const BuildOptions& opts) {
    if (build_failed_ && opts.stop_on_error) {
        onTaskComplete(node, {1, "", "Build stopped", false}, opts);
        return;
    }
    
    node->set_status(Node::Status::BUILDING);
    
    // Check if dirty
    if (!node->is_dirty()) {
        if (opts.verbose) {
            std::cout << "[SKIP] " << node->name() << " (up-to-date)\n";
        }
        stats_.tasks_skipped++;
        onTaskComplete(node, {0, "", "", true}, opts);
        return;
    }
    
    // Compile sources
    std::vector<fs::path> object_files;
    for (const auto& src : node->source_files) {
        fs::path obj_file = fs::path(src).replace_extension(".o");
        
        auto [binary, args] = toolchain_.buildCompileCommand(node, src, obj_file);
        
        if (opts.verbose) {
            std::cout << "[COMPILE] " << src << "\n";
        }
        
        ExecResult result = ProcessExecutor::execute(binary, args);
        
        if (!result.success()) {
            std::cerr << "[ERROR] Compilation failed: " << src << "\n";
            std::cerr << result.stderr_output << "\n";
            onTaskComplete(node, result, opts);
            return;
        }
        
        object_files.push_back(obj_file);
    }
    
    // Link (if executable or library)
    if (node->type() == Node::TargetType::EXECUTABLE ||
        node->type() == Node::TargetType::STATIC_LIBRARY) {
        
        auto [binary, args] = toolchain_.buildLinkCommand(node, object_files);
        
        if (opts.verbose) {
            std::cout << "[LINK] " << node->output_file << "\n";
        }
        
        ExecResult result = ProcessExecutor::execute(binary, args);
        
        if (!result.success()) {
            std::cerr << "[ERROR] Link failed: " << node->name() << "\n";
            std::cerr << result.stderr_output << "\n";
        }
        
        onTaskComplete(node, result, opts);
    } else {
        onTaskComplete(node, {0, "", "", true}, opts);
    }
}

void BuildScheduler::onTaskComplete(Node* node, const ExecResult& result, const BuildOptions& opts) {
    if (result.success()) {
        node->set_status(Node::Status::COMPLETED);
        stats_.tasks_completed++;
        
        // Unlock dependents (ATOMIC!)
        for (Node* dependent : node->dependents()) {
            int new_in_degree = dependent->decrement_in_degree();
            
            if (new_in_degree == 0) {
                // This dependent is now ready
                pending_tasks_++;
                pool_.enqueue([this, dependent, opts]() {
                    executeNode(dependent, opts);
                });
            }
        }
    } else {
        node->set_status(Node::Status::FAILED);
        stats_.tasks_failed++;
        build_failed_ = true;
        
        if (opts.stop_on_error) {
            propagateFailure(node);
        }
    }
    
    // Notify completion
    pending_tasks_--;
    if (pending_tasks_ == 0) {
        completion_cv_.notify_all();
    }
}

void BuildScheduler::propagateFailure(Node* node) {
    for (Node* dependent : node->dependents()) {
        dependent->set_status(Node::Status::FAILED);
        stats_.tasks_failed++;
        propagateFailure(dependent);
    }
}
```

---

## FileWatcher Subsystem - Continuous Build Automation

### Purpose

Implement a cross-platform filesystem observer that monitors source files and triggers automatic incremental rebuilds, enabling "watch mode" for rapid inner-loop development.

**Developer Workflow Enhancement:**
- Traditional: Edit → Save → Switch to terminal → Run `aria_make` → Switch back → Debug
- Watch Mode: Edit → Save → (automatic rebuild) → See results immediately

**Key Requirements:**
- **Cross-Platform:** Unified interface over Linux `inotify`, macOS `FSEvents`, Windows `ReadDirectoryChangesW`
- **Incremental:** Only rebuild affected targets based on dependency graph
- **Resilient:** Handle buffer overflows, directory deletion, permission errors gracefully
- **Integrated:** Works with BuildScheduler, DependencyGraph, and AriaLS (Language Server)

### Theoretical Foundation: OS Observation Models

The challenge of filesystem watching stems from fundamentally different kernel primitives:

| OS | API | Unit of Observation | Recursion | Event Delivery |
|----|-----|---------------------|-----------|----------------|
| **Linux** | `inotify` | Inode (per-directory) | Non-recursive | Blocking read on fd |
| **macOS** | `FSEvents` | Path hierarchy | Native recursive | CFRunLoop callback |
| **Windows** | `ReadDirectoryChangesW` | Directory handle | Native recursive | Overlapped I/O |

**Key Differences:**

1. **Linux inotify:**
   - Watches individual directory inodes
   - Must explicitly register watch descriptor (wd) for each subdirectory
   - Events delivered via file descriptor read
   - Non-recursive: requires manual tree walking and dynamic watch management

2. **macOS FSEvents:**
   - Watches entire path hierarchies
   - Native recursive monitoring
   - Log-based: can replay historical events from `.fseventsd`
   - Callback scheduled on CFRunLoop (requires thread affinity)

3. **Windows ReadDirectoryChangesW:**
   - Watches directory handle with `bWatchSubtree = TRUE`
   - Native recursive monitoring
   - Asynchronous overlapped I/O
   - Fixed-size buffer: overflow = silent data loss (returns 0 bytes)

### Architecture: Pimpl + Observer Pattern

**Class Structure:**

```cpp
namespace aria::build {

// Event types for fine-grained handling
enum class EventType {
    Created,    // New file/directory appeared
    Modified,   // File content changed
    Deleted,    // File/directory removed
    Renamed,    // File moved (may be 2 events: From + To)
    Overflow    // Buffer overflow, state indeterminate
};

struct FileEvent {
    EventType type;
    std::filesystem::path path;
    std::optional<std::filesystem::path> old_path;  // For renames
    
    bool is_directory() const;
};

// Callback signature: batch processing for efficiency
using WatchCallback = std::function<void(const std::vector<FileEvent>&)>;

class FileWatcher {
public:
    explicit FileWatcher(WatchCallback callback);
    ~FileWatcher();
    
    // Delete copy/move (unique resource ownership)
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;
    
    // Add directory to watch (recursive by default)
    void addWatch(const std::filesystem::path& path, bool recursive = true);
    
    // Remove watch
    void removeWatch(const std::filesystem::path& path);
    
    // Start/stop watching (spawns background thread)
    void start();
    void stop();
    
    bool isRunning() const { return running_; }
    
private:
    class Impl;  // Platform-specific implementation (Pimpl idiom)
    std::unique_ptr<Impl> impl_;
    
    WatchCallback callback_;
    std::atomic<bool> running_;
    std::thread watcher_thread_;
    
    void watcherLoop();  // OS-specific event loop
};

} // namespace aria::build
```

**Design Rationale:**

- **Pimpl Pattern:** Hides platform-specific headers (`<sys/inotify.h>`, `<CoreServices/CoreServices.h>`, `<windows.h>`) from public interface
- **Observer Pattern:** Decouples filesystem events (Subject) from build actions (Observer)
- **Asynchronous Dispatch:** Watcher runs on dedicated thread, callbacks pushed to BuildScheduler queue

### Linux Implementation: inotify Backend

**Challenges:**

1. **Non-Recursive Nature:** Must manually walk tree and register watches
2. **mkdir -p Race Condition:** Subdirectories created between inotify event and watch registration are invisible
3. **IN_Q_OVERFLOW:** Kernel buffer overflow requires fallback to full scan
4. **Resource Limits:** `/proc/sys/fs/inotify/max_user_watches` (often 8192)

**Implementation:**

```cpp
class FileWatcher::Impl {
public:
    Impl(WatchCallback callback) : callback_(callback), inotify_fd_(-1) {
        inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (inotify_fd_ < 0) {
            throw std::runtime_error("inotify_init failed");
        }
        
        // Create eventfd for clean shutdown
        stop_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    }
    
    void addWatch(const fs::path& path, bool recursive) {
        if (!recursive) {
            addSingleWatch(path);
            return;
        }
        
        // Recursive: walk tree and register all directories
        for (auto it = fs::recursive_directory_iterator(path); 
             it != fs::recursive_directory_iterator(); ++it) {
            if (it->is_directory()) {
                addSingleWatch(it->path());
            }
        }
    }
    
    void addSingleWatch(const fs::path& path) {
        int wd = inotify_add_watch(
            inotify_fd_,
            path.c_str(),
            IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE | 
            IN_DELETE_SELF | IN_MOVE_SELF | IN_ONLYDIR
        );
        
        if (wd < 0) {
            if (errno == ENOSPC) {
                std::cerr << "[ERROR] inotify watch limit exceeded. "
                          << "Increase /proc/sys/fs/inotify/max_user_watches\n";
            } else if (errno != EACCES) {  // Ignore permission denied
                std::cerr << "[WARN] Failed to watch: " << path << "\n";
            }
            return;
        }
        
        // Bidirectional mapping: wd ↔ path
        wd_to_path_[wd] = path.string();
        path_to_wd_[path.string()] = wd;
    }
    
    void run() {
        std::array<char, 16384> buffer;  // 16KB event buffer
        
        // epoll for multiplexing inotify_fd + stop_fd
        int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
        
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = inotify_fd_;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, inotify_fd_, &ev);
        
        ev.data.fd = stop_fd_;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, stop_fd_, &ev);
        
        std::vector<FileEvent> batch;
        
        while (running_) {
            struct epoll_event events[2];
            int n = epoll_wait(epoll_fd, events, 2, -1);
            
            for (int i = 0; i < n; ++i) {
                if (events[i].data.fd == stop_fd_) {
                    return;  // Shutdown signal
                }
            }
            
            // Read inotify events
            ssize_t len = read(inotify_fd_, buffer.data(), buffer.size());
            if (len <= 0) continue;
            
            const inotify_event* event = nullptr;
            for (char* ptr = buffer.data(); ptr < buffer.data() + len; 
                 ptr += sizeof(inotify_event) + event->len) {
                
                event = reinterpret_cast<const inotify_event*>(ptr);
                
                // Handle overflow
                if (event->mask & IN_Q_OVERFLOW) {
                    batch.push_back({EventType::Overflow, "", {}});
                    callback_(batch);
                    batch.clear();
                    continue;
                }
                
                // Resolve path from watch descriptor
                auto it = wd_to_path_.find(event->wd);
                if (it == wd_to_path_.end()) continue;
                
                fs::path full_path = fs::path(it->second) / event->name;
                
                // Determine event type
                EventType type;
                if (event->mask & IN_CREATE) type = EventType::Created;
                else if (event->mask & IN_MODIFY) type = EventType::Modified;
                else if (event->mask & IN_DELETE) type = EventType::Deleted;
                else if (event->mask & IN_MOVED_FROM) type = EventType::Renamed;
                else continue;
                
                batch.push_back({type, full_path, {}});
                
                // NEW DIRECTORY CREATED: Recursively add watches (close mkdir -p gap)
                if ((event->mask & IN_CREATE) && (event->mask & IN_ISDIR)) {
                    addWatch(full_path, true);
                    
                    // Rescan to catch missed children (race condition mitigation)
                    for (auto it = fs::directory_iterator(full_path); 
                         it != fs::directory_iterator(); ++it) {
                        if (it->is_directory() && path_to_wd_.count(it->path().string()) == 0) {
                            // Missed directory, add synthetic event
                            batch.push_back({EventType::Created, it->path(), {}});
                            addWatch(it->path(), true);
                        }
                    }
                }
            }
            
            if (!batch.empty()) {
                callback_(batch);
                batch.clear();
            }
        }
        
        close(epoll_fd);
    }
    
    void stop() {
        uint64_t val = 1;
        write(stop_fd_, &val, sizeof(val));  // Signal stop
    }
    
private:
    WatchCallback callback_;
    std::atomic<bool> running_;
    
    int inotify_fd_;
    int stop_fd_;  // eventfd for shutdown
    
    std::map<int, std::string> wd_to_path_;     // Watch descriptor → path
    std::map<std::string, int> path_to_wd_;     // Path → watch descriptor
};
```

**mkdir -p Race Mitigation:**

When `IN_CREATE | IN_ISDIR` detected:
1. Immediately add watch on new directory
2. Rescan directory contents
3. For any subdirectories not in `path_to_wd_`, emit synthetic `Created` events
4. Recursively watch missed subdirectories

This ensures eventual consistency even if kernel creates `a/b/c` faster than userspace can register watches.

### macOS Implementation: FSEvents Backend

**Challenges:**

1. **CFRunLoop Requirement:** Callbacks must be scheduled on CFRunLoop
2. **Thread Affinity:** Stream must be created and run on same thread
3. **Latency Coalescing:** Kernel batches events (configurable delay)

**Implementation:**

```cpp
class FileWatcher::Impl {
public:
    Impl(WatchCallback callback) 
        : callback_(callback), stream_(nullptr), run_loop_(nullptr) {}
    
    void addWatch(const fs::path& path, bool recursive) {
        watched_paths_.push_back(path.string());
    }
    
    void run() {
        // Convert std::vector<std::string> to CFArrayRef
        CFStringRef path_refs[watched_paths_.size()];
        for (size_t i = 0; i < watched_paths_.size(); ++i) {
            path_refs[i] = CFStringCreateWithCString(
                nullptr, watched_paths_[i].c_str(), kCFStringEncodingUTF8
            );
        }
        
        CFArrayRef paths_array = CFArrayCreate(
            nullptr, (const void**)path_refs, watched_paths_.size(), nullptr
        );
        
        // Context allows passing 'this' to C callback
        FSEventStreamContext context = {0, this, nullptr, nullptr, nullptr};
        
        // Create stream with file-level events
        stream_ = FSEventStreamCreate(
            nullptr,
            &fseventCallback,  // Static C function
            &context,
            paths_array,
            kFSEventStreamEventIdSinceNow,
            0.1,  // 100ms latency (debounce)
            kFSEventStreamCreateFlagFileEvents | 
            kFSEventStreamCreateFlagWatchRoot
        );
        
        // Schedule on CURRENT thread's run loop
        run_loop_ = CFRunLoopGetCurrent();
        FSEventStreamScheduleWithRunLoop(stream_, run_loop_, kCFRunLoopDefaultMode);
        FSEventStreamStart(stream_);
        
        // Block thread in run loop (event-driven)
        CFRunLoopRun();
        
        // Cleanup after stop()
        FSEventStreamStop(stream_);
        FSEventStreamInvalidate(stream_);
        FSEventStreamRelease(stream_);
        CFRelease(paths_array);
    }
    
    void stop() {
        if (run_loop_) {
            CFRunLoopStop(run_loop_);  // Break CFRunLoopRun()
        }
    }
    
    // Static callback (C ABI requirement)
    static void fseventCallback(
        ConstFSEventStreamRef stream,
        void* context,
        size_t num_events,
        void* event_paths,
        const FSEventStreamEventFlags event_flags[],
        const FSEventStreamEventId event_ids[]
    ) {
        auto* impl = static_cast<Impl*>(context);
        char** paths = (char**)event_paths;
        
        std::vector<FileEvent> batch;
        
        for (size_t i = 0; i < num_events; ++i) {
            fs::path path(paths[i]);
            
            EventType type;
            if (event_flags[i] & kFSEventStreamEventFlagItemCreated) {
                type = EventType::Created;
            } else if (event_flags[i] & kFSEventStreamEventFlagItemModified) {
                type = EventType::Modified;
            } else if (event_flags[i] & kFSEventStreamEventFlagItemRemoved) {
                type = EventType::Deleted;
            } else if (event_flags[i] & kFSEventStreamEventFlagItemRenamed) {
                type = EventType::Renamed;
            } else {
                continue;
            }
            
            batch.push_back({type, path, {}});
        }
        
        if (!batch.empty()) {
            impl->callback_(batch);
        }
    }
    
private:
    WatchCallback callback_;
    std::vector<std::string> watched_paths_;
    FSEventStreamRef stream_;
    CFRunLoopRef run_loop_;
};
```

**Thread Model:**
- Watcher thread created in `FileWatcher::start()`
- Thread enters `CFRunLoopRun()`, blocking indefinitely
- Events delivered via `fseventCallback` on same thread
- `stop()` called from main thread, invokes `CFRunLoopStop()` to break loop

### Windows Implementation: ReadDirectoryChangesW Backend

**Challenges:**

1. **Overlapped I/O:** Asynchronous model requires `OVERLAPPED` structure and event handles
2. **Buffer Overflow:** Returns success but `lpBytesReturned == 0` on overflow (silent data loss)
3. **Short Filenames:** NTFS reports 8.3 names (e.g., `PROGRA~1`), must normalize to long names
4. **Root Deletion:** Deleting watched directory invalidates handle, must detect and handle

**Implementation:**

```cpp
class FileWatcher::Impl {
public:
    Impl(WatchCallback callback) : callback_(callback), dir_handle_(INVALID_HANDLE_VALUE) {}
    
    void addWatch(const fs::path& path, bool recursive) {
        dir_handle_ = CreateFileW(
            path.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,  // CRITICAL for dirs + async
            nullptr
        );
        
        if (dir_handle_ == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to open directory: " + path.string());
        }
        
        root_path_ = path;
        recursive_ = recursive;
    }
    
    void run() {
        std::vector<BYTE> buffer(65536);  // 64KB buffer (aligned to DWORD)
        OVERLAPPED overlapped = {0};
        overlapped.hEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        
        HANDLE stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        stop_event_ = stop_event;
        
        HANDLE events[2] = {overlapped.hEvent, stop_event};
        
        while (running_) {
            BOOL success = ReadDirectoryChangesW(
                dir_handle_,
                buffer.data(),
                buffer.size(),
                recursive_ ? TRUE : FALSE,  // bWatchSubtree
                FILE_NOTIFY_CHANGE_FILE_NAME | 
                FILE_NOTIFY_CHANGE_DIR_NAME | 
                FILE_NOTIFY_CHANGE_LAST_WRITE,
                nullptr,
                &overlapped,
                nullptr  // Completion routine (unused, using event-driven)
            );
            
            if (!success && GetLastError() != ERROR_IO_PENDING) {
                std::cerr << "[ERROR] ReadDirectoryChangesW failed\n";
                break;
            }
            
            // Wait for event or stop signal
            DWORD wait_result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            
            if (wait_result == WAIT_OBJECT_0 + 1) {
                break;  // Stop event
            }
            
            DWORD bytes_returned = 0;
            if (!GetOverlappedResult(dir_handle_, &overlapped, &bytes_returned, FALSE)) {
                if (GetLastError() == ERROR_ACCESS_DENIED) {
                    // Root directory deleted, emit event and exit
                    std::vector<FileEvent> batch;
                    batch.push_back({EventType::Deleted, root_path_, {}});
                    callback_(batch);
                    break;
                }
                continue;
            }
            
            // CRITICAL: Check for buffer overflow
            if (bytes_returned == 0) {
                // Overflow: emit special event for BuildScheduler to handle
                std::vector<FileEvent> batch;
                batch.push_back({EventType::Overflow, root_path_, {}});
                callback_(batch);
                continue;
            }
            
            // Parse FILE_NOTIFY_INFORMATION records
            std::vector<FileEvent> batch;
            BYTE* ptr = buffer.data();
            
            while (true) {
                auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(ptr);
                
                std::wstring filename(info->FileName, info->FileNameLength / sizeof(WCHAR));
                fs::path full_path = root_path_ / filename;
                
                // Normalize to long path (avoid 8.3 short names)
                WCHAR long_path[MAX_PATH];
                GetLongPathNameW(full_path.c_str(), long_path, MAX_PATH);
                full_path = long_path;
                
                EventType type;
                switch (info->Action) {
                    case FILE_ACTION_ADDED:
                        type = EventType::Created; break;
                    case FILE_ACTION_MODIFIED:
                        type = EventType::Modified; break;
                    case FILE_ACTION_REMOVED:
                        type = EventType::Deleted; break;
                    case FILE_ACTION_RENAMED_OLD_NAME:
                    case FILE_ACTION_RENAMED_NEW_NAME:
                        type = EventType::Renamed; break;
                    default:
                        goto next_record;
                }
                
                batch.push_back({type, full_path, {}});
                
            next_record:
                if (info->NextEntryOffset == 0) break;
                ptr += info->NextEntryOffset;
            }
            
            if (!batch.empty()) {
                callback_(batch);
            }
        }
        
        CloseHandle(overlapped.hEvent);
        CloseHandle(stop_event);
    }
    
    void stop() {
        SetEvent(stop_event_);  // Signal stop
    }
    
private:
    WatchCallback callback_;
    std::atomic<bool> running_;
    
    HANDLE dir_handle_;
    HANDLE stop_event_;
    fs::path root_path_;
    bool recursive_;
};
```

**Buffer Overflow Handling:**

When `GetOverlappedResult` returns `bytes_returned == 0` with success:
- Kernel detected overflow (too many events for 64KB buffer)
- Emit `EventType::Overflow`
- BuildScheduler responds by invalidating entire graph and triggering full incremental scan

### Event Debouncing and Coalescing

**Problem:** Text editors emit rapid sequences of events on save:
```
CREATE .main.aria.swp
MODIFY .main.aria.swp (x5)
DELETE main.aria
RENAME .main.aria.swp → main.aria
```

Triggering 8 builds would be inefficient and create race conditions.

**Solution: Trailing Edge Debouncer**

```cpp
class BuildScheduler {
public:
    void onFileEvent(const std::vector<FileEvent>& events) {
        std::lock_guard<std::mutex> lock(debounce_mutex_);
        
        for (const auto& event : events) {
            if (event.type == EventType::Overflow) {
                // Handle overflow: invalidate entire graph
                dependency_graph_.invalidateAll();
                pending_full_scan_ = true;
                continue;
            }
            
            // Add to pending changes (deduplicates via set)
            pending_changes_.insert(event.path);
        }
        
        // Reset debounce timer
        last_event_time_ = std::chrono::steady_clock::now();
        debounce_cv_.notify_one();
    }
    
private:
    void debounceLoop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(debounce_mutex_);
            
            // Wait for events
            debounce_cv_.wait(lock, [this] { 
                return !pending_changes_.empty() || !running_; 
            });
            
            if (!running_) break;
            
            // Calculate time since last event
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_event_time_
            );
            
            // If quiet period exceeded (e.g., 200ms), trigger build
            if (elapsed >= std::chrono::milliseconds(200)) {
                std::set<fs::path> changes = std::move(pending_changes_);
                pending_changes_.clear();
                bool full_scan = pending_full_scan_;
                pending_full_scan_ = false;
                lock.unlock();
                
                // Trigger build on changes
                if (full_scan) {
                    performFullScan();
                } else {
                    performIncrementalBuild(changes);
                }
            } else {
                // Still receiving events, wait remaining time
                auto remaining = std::chrono::milliseconds(200) - elapsed;
                debounce_cv_.wait_for(lock, remaining);
            }
        }
    }
    
    std::mutex debounce_mutex_;
    std::condition_variable debounce_cv_;
    std::set<fs::path> pending_changes_;
    std::chrono::steady_clock::time_point last_event_time_;
    bool pending_full_scan_ = false;
};
```

**Algorithm:**
1. Events arrive, inserted into `pending_changes_` set (deduplicates)
2. Timer reset to `now()`
3. Debounce thread wakes, checks if 200ms have elapsed since last event
4. If yes: take snapshot of changes, trigger build
5. If no: wait remaining time, check again

**Result:** Rapid edits coalesced into single build trigger after 200ms of silence.

### Integration with DependencyGraph

```cpp
void BuildScheduler::performIncrementalBuild(const std::set<fs::path>& changed_files) {
    std::vector<Node*> dirty_nodes;
    
    for (const auto& file : changed_files) {
        // Find node in dependency graph
        Node* node = dependency_graph_.findNode(file);
        
        if (!node) {
            // New file created, check if matches any target glob
            for (auto& target : config_.targets) {
                if (glob_engine_.matches(file, target.sources_pattern)) {
                    // Add new node to graph
                    node = dependency_graph_.addNode(file, target.name);
                    break;
                }
            }
        }
        
        if (node) {
            // Mark node as dirty
            node->set_status(Node::Status::PENDING);
            dirty_nodes.push_back(node);
            
            // Propagate dirty status to all reverse dependencies
            for (Node* dependent : node->reverse_dependencies()) {
                dependent->set_status(Node::Status::PENDING);
                dirty_nodes.push_back(dependent);
            }
        }
    }
    
    // Build only dirty subgraph
    if (!dirty_nodes.empty()) {
        build(dirty_nodes);
    }
}
```

**Graph Invalidation Logic:**

1. **Modified File:** Mark node PENDING, propagate to reverse deps (things that depend on this file)
2. **Created File:** Check glob patterns, add new node if matches
3. **Deleted File:** Remove node, mark all reverse deps FAILED (missing dependency)
4. **Overflow Event:** Mark ALL nodes PENDING (safe but conservative)

### Integration with AriaLS (Language Server)

**Dual-Source Truth:**
- **LSP Client:** Editor sends `textDocument/didChange` for open files (in-memory buffer)
- **FileWatcher:** Detects external changes (git checkout, code generation, dependency updates)

**Reconciliation Strategy:**

```cpp
class AriaLS {
public:
    void onFileWatcherEvent(const std::vector<FileEvent>& events) {
        for (const auto& event : events) {
            // Check if file is open in editor
            if (vfs_.hasDocument(event.path)) {
                // Open in editor: editor buffer is source of truth
                // Ignore filesystem event (user may have unsaved changes)
                continue;
            }
            
            // Not open: filesystem is source of truth
            switch (event.type) {
                case EventType::Modified:
                case EventType::Created:
                    // Reload file from disk
                    vfs_.reloadFromDisk(event.path);
                    
                    // Re-parse and update diagnostics
                    work_queue_.enqueue([this, path = event.path]() {
                        auto diagnostics = parser_.parse(path);
                        lsp_client_.publishDiagnostics(path, diagnostics);
                    });
                    break;
                    
                case EventType::Deleted:
                    // Remove from VFS
                    vfs_.removeDocument(event.path);
                    lsp_client_.publishDiagnostics(event.path, {});
                    break;
            }
        }
    }
    
private:
    VirtualFileSystem vfs_;
    WorkQueue work_queue_;
    FileWatcher watcher_;
};
```

**Priority Rules:**
- Open file in editor: `didChange` notification > filesystem event
- Closed file: filesystem event > stale VFS cache

### CLI Integration: --watch Flag

```cpp
int main(int argc, char* argv[]) {
    BuildOptions opts = parseArgs(argc, argv);
    
    if (opts.watch_mode) {
        std::cout << "[WATCH] Monitoring filesystem for changes...\n";
        
        FileWatcher watcher([&](const std::vector<FileEvent>& events) {
            scheduler.onFileEvent(events);
        });
        
        watcher.addWatch("src", true);
        watcher.addWatch("include", true);
        watcher.start();
        
        // Initial build
        scheduler.build();
        
        // Block indefinitely (Ctrl+C to exit)
        std::cout << "[WATCH] Press Ctrl+C to stop\n";
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } else {
        // One-shot build
        scheduler.build();
    }
}
```

**Usage:**
```bash
aria_make --watch    # Start watch mode
aria_make -w         # Shorthand
```

### Testing Strategy

**Unit Tests (Platform-Specific):**

```cpp
TEST(FileWatcher_Linux, DetectsFileCreation) {
    TempDir tmp;
    std::vector<FileEvent> captured;
    
    FileWatcher watcher([&](const std::vector<FileEvent>& events) {
        captured.insert(captured.end(), events.begin(), events.end());
    });
    
    watcher.addWatch(tmp.path());
    watcher.start();
    
    // Create file
    tmp.writeFile("test.aria", "// Hello");
    
    // Wait for event (debounced)
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    ASSERT_EQ(captured.size(), 1);
    EXPECT_EQ(captured[0].type, EventType::Created);
    EXPECT_EQ(captured[0].path.filename(), "test.aria");
}

TEST(FileWatcher_Linux, HandlesOverflow) {
    // Simulate overflow by creating 10,000 files rapidly
    TempDir tmp;
    bool overflow_detected = false;
    
    FileWatcher watcher([&](const std::vector<FileEvent>& events) {
        for (const auto& e : events) {
            if (e.type == EventType::Overflow) {
                overflow_detected = true;
            }
        }
    });
    
    watcher.addWatch(tmp.path());
    watcher.start();
    
    for (int i = 0; i < 10000; ++i) {
        tmp.writeFile("file_" + std::to_string(i) + ".txt", "x");
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(overflow_detected);  // Should trigger IN_Q_OVERFLOW
}
```

**Integration Tests:**

```cpp
TEST(BuildScheduler, WatchModeIncrementalRebuild) {
    TestProject proj;
    proj.writeFile("main.aria", "import lib; fn main() {}");
    proj.writeFile("lib.aria", "export fn helper() {}");
    
    BuildScheduler scheduler;
    scheduler.initialize(proj.config());
    scheduler.build();  // Initial build
    
    FileWatcher watcher([&](auto events) { scheduler.onFileEvent(events); });
    watcher.addWatch(proj.root());
    watcher.start();
    
    // Modify lib.aria
    proj.writeFile("lib.aria", "export fn helper() { return 42; }");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Should rebuild lib.aria + main.aria (reverse dependency)
    EXPECT_EQ(scheduler.stats().tasks_completed, 2);
}
```

### Performance Considerations

**Linux inotify:**
- Watch limit: 8192 (default), configurable via sysctl
- Memory: ~1KB per watch descriptor
- Recommendation: For large projects (>5000 dirs), increase limit or use polling fallback

**macOS FSEvents:**
- Kernel-side coalescing: 100ms latency reduces event flood
- Log-based: Very efficient for large trees
- Memory: ~1-2KB per watched path

**Windows ReadDirectoryChangesW:**
- Buffer size: 64KB (max ~1000 events before overflow)
- Recursive: Native, no per-directory overhead
- Recommendation: Increase buffer to 256KB for high-churn directories

### Implementation Checklist

- [ ] Create `FileWatcher` class with Pimpl pattern
- [ ] Implement Linux backend (`inotify` + epoll + eventfd)
- [ ] Implement macOS backend (`FSEvents` + CFRunLoop)
- [ ] Implement Windows backend (`ReadDirectoryChangesW` + overlapped I/O)
- [ ] Add debounce coordinator thread to BuildScheduler
- [ ] Integrate with DependencyGraph invalidation
- [ ] Add `--watch` CLI flag to aria_make
- [ ] Implement overflow handling (fallback to full scan)
- [ ] Add unit tests for each platform
- [ ] Add integration test for watch mode rebuild
- [ ] Document resource limits (inotify max_user_watches)
- [ ] Add to AriaLS WorkQueue integration

**Estimated Implementation Time:** Week 5-6 (7-10 days)
- Core FileWatcher class + Pimpl: 1 day
- Linux backend: 2-3 days (mkdir -p race, overflow handling)
- macOS backend: 1-2 days (CFRunLoop integration)
- Windows backend: 2-3 days (overlapped I/O, buffer overflow)
- BuildScheduler integration: 1 day
- Testing + documentation: 1-2 days

---

**Next:** [50_INCREMENTAL_BUILDS.md](./50_INCREMENTAL_BUILDS.md) - Timestamp checking and command hashing
