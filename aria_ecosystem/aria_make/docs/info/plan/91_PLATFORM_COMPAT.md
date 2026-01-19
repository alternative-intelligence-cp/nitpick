# Platform Compatibility Patterns

This document provides cross-platform code patterns for POSIX (Linux, macOS) and Windows compatibility, covering file system operations, process spawning, threading, and path handling.

---

## Overview

**Supported Platforms:**
- **Linux** (primary development target)
- **macOS** (secondary target)
- **Windows 10+** (with MSVC or MinGW-w64)

**Strategy:** Use C++17 standard library where possible, falling back to platform-specific APIs only when necessary.

### Abstraction Layers

| Feature | Cross-Platform API | Platform-Specific |
|---------|-------------------|-------------------|
| File system | `std::filesystem` | ✅ Fully portable |
| Threads | `std::thread`, `std::mutex` | ✅ Fully portable |
| Process spawning | ❌ Not in standard | `fork()/exec()` vs `CreateProcess()` |
| Pipes | ❌ Not in standard | `pipe()` vs `CreatePipe()` |
| Signals | ❌ Not in standard | `signal()` vs Windows events |
| Environment vars | `std::getenv()` | ⚠️ Not thread-safe |

---

## File System Operations

### Path Handling

**✅ DO: Use `std::filesystem::path`**

```cpp
#include <filesystem>

namespace fs = std::filesystem;

fs::path project_root = "/home/user/project";
fs::path source_file = project_root / "src" / "main.aria";

// Portable across platforms:
// Linux:   /home/user/project/src/main.aria
// Windows: C:\home\user\project\src\main.aria (with backslashes)
```

**❌ DON'T: String concatenation with hard-coded separators**

```cpp
// BAD: Breaks on Windows
std::string path = base_dir + "/" + filename;

// GOOD: Use operator/
fs::path path = fs::path(base_dir) / filename;
```

### Path Normalization

```cpp
fs::path normalizePath(const fs::path& path) {
    // 1. Convert to absolute
    fs::path abs = fs::absolute(path);
    
    // 2. Resolve symlinks, . and .. 
    fs::path canonical = fs::canonical(abs);
    
    // 3. Make lexically normal (remove redundant components)
    return canonical.lexically_normal();
}

// Example:
// Input:  src/../include/./utils.aria
// Output: /home/user/project/include/utils.aria
```

### Hidden File Detection

```cpp
bool isHidden(const fs::path& path) {
#ifdef _WIN32
    // Windows: Check FILE_ATTRIBUTE_HIDDEN
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    return (attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    // POSIX: Leading dot in filename
    std::string filename = path.filename().string();
    return !filename.empty() && filename[0] == '.';
#endif
}
```

### File Timestamps

**✅ DO: Use `std::filesystem::last_write_time`**

```cpp
auto time1 = fs::last_write_time("src/main.aria");
auto time2 = fs::last_write_time("build/main.o");

if (time1 > time2) {
    // Source is newer → rebuild
}
```

**Platform differences (handled by std::filesystem):**

| Platform | Precision | Range |
|----------|-----------|-------|
| Linux | Nanoseconds | 1970-2038+ (64-bit) |
| macOS | Nanoseconds | 1970-2038+ |
| Windows | 100ns intervals | 1601-30828 |

**⚠️ Gotcha: Precision loss**

```cpp
// Windows timestamp (100ns precision):
//   2024-01-01 12:00:00.123456700

// Linux timestamp (1ns precision):
//   2024-01-01 12:00:00.123456789

// Comparison may fail if files synced across platforms!
// Solution: Use > (not >=) and accept 100ns tolerance
```

### Directory Traversal

```cpp
void traverseDirectory(const fs::path& dir) {
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            std::cout << "[DIR] " << entry.path() << "\n";
        } else if (entry.is_regular_file()) {
            std::cout << "[FILE] " << entry.path() << "\n";
        }
    }
}
```

**Recursive traversal:**

```cpp
void traverseRecursive(const fs::path& dir) {
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        std::cout << entry.path() << "\n";
    }
}
```

**⚠️ Handle permissions errors:**

```cpp
void safeTraversal(const fs::path& dir) {
    std::error_code ec;
    
    for (const auto& entry : fs::recursive_directory_iterator(dir, ec)) {
        if (ec) {
            std::cerr << "[WARNING] Cannot access: " << dir << " - " 
                      << ec.message() << "\n";
            ec.clear();
            continue;
        }
        
        // Process entry...
    }
}
```

---

## Process Spawning

### Platform-Specific Implementations

**POSIX (Linux, macOS):**

```cpp
#ifndef _WIN32

#include <unistd.h>
#include <sys/wait.h>

ExecResult executePosix(const std::string& binary,
                        const std::vector<std::string>& args) {
    int stdout_pipe[2], stderr_pipe[2];
    pipe(stdout_pipe);
    pipe(stderr_pipe);
    
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        
        // Build argv
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(binary.c_str()));
        for (const auto& arg : args) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);
        
        execvp(binary.c_str(), argv.data());
        
        // exec failed
        exit(127);
    }
    
    // Parent process
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
    
    // Drain pipes (see 40_EXECUTION_ENGINE.md for full implementation)
    // ...
    
    int status;
    waitpid(pid, &status, 0);
    
    ExecResult result;
    result.exit_code = WEXITSTATUS(status);
    return result;
}

#endif
```

**Windows:**

```cpp
#ifdef _WIN32

#include <windows.h>

ExecResult executeWindows(const std::string& binary,
                          const std::vector<std::string>& args) {
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
    
    // Build command line (Windows requires single string)
    std::wstring cmd_line = buildCommandLineWide(binary, args);
    
    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = stdout_write;
    si.hStdError = stderr_write;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(
        NULL,
        &cmd_line[0],
        NULL, NULL,
        TRUE,  // Inherit handles
        0, NULL, NULL,
        &si, &pi
    )) {
        throw std::runtime_error("CreateProcess failed");
    }
    
    CloseHandle(stdout_write);
    CloseHandle(stderr_write);
    
    // Drain pipes (see 40_EXECUTION_ENGINE.md)
    // ...
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exit_code;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    ExecResult result;
    result.exit_code = static_cast<int>(exit_code);
    return result;
}

#endif
```

### Command Line Building

**POSIX: Array of strings**

```cpp
std::vector<char*> buildArgvPosix(const std::string& binary,
                                  const std::vector<std::string>& args) {
    std::vector<char*> argv;
    
    argv.push_back(const_cast<char*>(binary.c_str()));
    
    for (const auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    
    argv.push_back(nullptr);  // NULL terminator required
    
    return argv;
}
```

**Windows: Single string with escaping**

```cpp
std::wstring buildCommandLineWide(const std::string& binary,
                                  const std::vector<std::string>& args) {
    std::ostringstream oss;
    
    // Quote binary if it contains spaces
    if (binary.find(' ') != std::string::npos) {
        oss << "\"" << binary << "\"";
    } else {
        oss << binary;
    }
    
    for (const auto& arg : args) {
        oss << " ";
        
        // Quote args with spaces
        if (arg.find(' ') != std::string::npos) {
            oss << "\"" << arg << "\"";
        } else {
            oss << arg;
        }
    }
    
    std::string utf8_str = oss.str();
    
    // Convert UTF-8 to UTF-16 (Windows wide strings)
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, 
                                       utf8_str.c_str(), -1, 
                                       NULL, 0);
    std::wstring wide_str(wide_len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, 
                        utf8_str.c_str(), -1, 
                        &wide_str[0], wide_len);
    
    return wide_str;
}
```

---

## Threading

### ThreadPool (Fully Portable)

```cpp
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
        // ... (see 40_EXECUTION_ENGINE.md)
    }
    
private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_flag_;
};
```

**Platform support:**
- ✅ Linux: pthreads (automatically linked)
- ✅ macOS: pthreads (automatically linked)
- ✅ Windows: Native threads (no extra libs needed)

### Atomic Operations (Fully Portable)

```cpp
#include <atomic>

class Node {
private:
    std::atomic<int> in_degree_;
    
public:
    int get_in_degree() const {
        return in_degree_.load(std::memory_order_acquire);
    }
    
    int decrement_in_degree() {
        return in_degree_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    }
};
```

**Platform support:**
- ✅ All platforms: `std::atomic` uses CPU-specific instructions (lock-free on modern CPUs)

---

## Environment Variables

### Reading Environment Variables

```cpp
#include <cstdlib>

std::optional<std::string> getEnvVar(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    
    if (value == nullptr) {
        return std::nullopt;
    }
    
    return std::string(value);
}

// Example:
auto home = getEnvVar("HOME");  // Linux/macOS
auto userprofile = getEnvVar("USERPROFILE");  // Windows
```

**⚠️ Thread Safety:**

```cpp
// ❌ NOT thread-safe (std::getenv returns pointer to static buffer)
std::string getEnvVarUnsafe(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
}

// ✅ Thread-safe (copy immediately)
std::string getEnvVarSafe(const std::string& name) {
    // Lock mutex if multiple threads reading env vars
    static std::mutex env_mutex;
    std::lock_guard<std::mutex> lock(env_mutex);
    
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : "";
}
```

### Setting Environment Variables (Platform-Specific)

**POSIX:**

```cpp
#ifndef _WIN32

bool setEnvVar(const std::string& name, const std::string& value) {
    return setenv(name.c_str(), value.c_str(), 1) == 0;
}

bool unsetEnvVar(const std::string& name) {
    return unsetenv(name.c_str()) == 0;
}

#endif
```

**Windows:**

```cpp
#ifdef _WIN32

bool setEnvVar(const std::string& name, const std::string& value) {
    return _putenv_s(name.c_str(), value.c_str()) == 0;
}

bool unsetEnvVar(const std::string& name) {
    return _putenv_s(name.c_str(), "") == 0;
}

#endif
```

---

## Compiler Detection

### Preprocessor Macros

```cpp
// Compiler detection
#if defined(_MSC_VER)
    #define COMPILER_MSVC
#elif defined(__GNUC__)
    #define COMPILER_GCC
#elif defined(__clang__)
    #define COMPILER_CLANG
#endif

// OS detection
#if defined(_WIN32)
    #define OS_WINDOWS
#elif defined(__linux__)
    #define OS_LINUX
#elif defined(__APPLE__)
    #define OS_MACOS
#endif

// Architecture detection
#if defined(__x86_64__) || defined(_M_X64)
    #define ARCH_X64
#elif defined(__i386__) || defined(_M_IX86)
    #define ARCH_X86
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define ARCH_ARM64
#endif
```

### Platform-Specific Optimizations

```cpp
// Likely/unlikely hints (branch prediction)
#ifdef COMPILER_GCC
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

// Example:
if (UNLIKELY(error_occurred)) {
    handleError();
}
```

```cpp
// Force inline
#ifdef COMPILER_MSVC
    #define FORCE_INLINE __forceinline
#else
    #define FORCE_INLINE __attribute__((always_inline)) inline
#endif

FORCE_INLINE int fastHash(int x) {
    return x * 2654435761;
}
```

---

## Build System Configuration

### CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.17)

project(aria_make CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform-specific sources
set(COMMON_SOURCES
    src/config_parser.cpp
    src/dependency_graph.cpp
    src/glob_engine.cpp
    src/build_scheduler.cpp
    src/thread_pool.cpp
)

if(WIN32)
    list(APPEND SOURCES
        src/process_executor_windows.cpp
    )
else()
    list(APPEND SOURCES
        src/process_executor_posix.cpp
    )
endif()

# Executable
add_executable(aria_make ${COMMON_SOURCES} ${SOURCES})

# Link libraries
if(UNIX)
    target_link_libraries(aria_make pthread)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(aria_make PRIVATE /W4)
else()
    target_compile_options(aria_make PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

### Makefile (Alternative)

```makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

COMMON_SOURCES = \
    src/config_parser.cpp \
    src/dependency_graph.cpp \
    src/glob_engine.cpp \
    src/build_scheduler.cpp \
    src/thread_pool.cpp

ifeq ($(OS),Windows_NT)
    SOURCES = $(COMMON_SOURCES) src/process_executor_windows.cpp
    LDFLAGS =
else
    SOURCES = $(COMMON_SOURCES) src/process_executor_posix.cpp
    LDFLAGS = -lpthread
endif

OBJECTS = $(SOURCES:.cpp=.o)

aria_make: $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) aria_make
```

---

## Console Output

### ANSI Color Codes (POSIX)

```cpp
#ifndef _WIN32

void printError(const std::string& msg) {
    std::cerr << "\033[1;31m[ERROR]\033[0m " << msg << "\n";
}

void printSuccess(const std::string& msg) {
    std::cout << "\033[1;32m[SUCCESS]\033[0m " << msg << "\n";
}

#endif
```

### Windows Console API

```cpp
#ifdef _WIN32

#include <windows.h>

void printError(const std::string& msg) {
    HANDLE console = GetStdHandle(STD_ERROR_HANDLE);
    
    // Red text
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::cerr << "[ERROR] ";
    
    // Reset to default
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    std::cerr << msg << "\n";
}

void printSuccess(const std::string& msg) {
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    
    // Green text
    SetConsoleTextAttribute(console, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    std::cout << "[SUCCESS] ";
    
    // Reset
    SetConsoleTextAttribute(console, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
    std::cout << msg << "\n";
}

#endif
```

### Portable Alternative (No Colors)

```cpp
void printError(const std::string& msg) {
    std::cerr << "[ERROR] " << msg << "\n";
}

void printSuccess(const std::string& msg) {
    std::cout << "[SUCCESS] " << msg << "\n";
}
```

---

## Testing Across Platforms

### GitHub Actions CI

```yaml
name: CI

on: [push, pull_request]

jobs:
  build:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]
        compiler: [gcc, clang, msvc]
        exclude:
          - os: ubuntu-latest
            compiler: msvc
          - os: macos-latest
            compiler: msvc
          - os: windows-latest
            compiler: gcc
          - os: windows-latest
            compiler: clang
    
    runs-on: ${{ matrix.os }}
    
    steps:
      - uses: actions/checkout@v2
      
      - name: Install dependencies (Linux)
        if: matrix.os == 'ubuntu-latest'
        run: sudo apt-get install -y build-essential
      
      - name: Install dependencies (macOS)
        if: matrix.os == 'macos-latest'
        run: brew install gcc
      
      - name: Build
        run: |
          mkdir build
          cd build
          cmake ..
          cmake --build .
      
      - name: Test
        run: |
          cd build
          ctest --output-on-failure
```

### Platform-Specific Test Cases

```cpp
TEST(PlatformCompat, PathSeparators) {
    fs::path path = fs::path("src") / "main.aria";
    
#ifdef _WIN32
    // Windows uses backslash internally
    ASSERT_NE(path.string().find('\\'), std::string::npos);
#else
    // POSIX uses forward slash
    ASSERT_NE(path.string().find('/'), std::string::npos);
#endif
}

TEST(PlatformCompat, ProcessExecution) {
#ifdef _WIN32
    auto result = ProcessExecutor::execute("cmd.exe", {"/c", "echo", "Hello"});
#else
    auto result = ProcessExecutor::execute("echo", {"Hello"});
#endif
    
    ASSERT_EQ(result.exit_code, 0);
    ASSERT_NE(result.stdout_output.find("Hello"), std::string::npos);
}
```

---

## Summary: Portability Checklist

**✅ Use Standard Library When Possible:**
- `std::filesystem` for paths, directories, timestamps
- `std::thread`, `std::mutex`, `std::atomic` for threading
- `std::chrono` for timing

**⚠️ Platform-Specific When Necessary:**
- Process spawning (`fork()/exec()` vs `CreateProcess()`)
- Pipes (`pipe()` vs `CreatePipe()`)
- Environment variable modification (`setenv()` vs `_putenv_s()`)

**📋 Testing:**
- Test on all target platforms (Linux, macOS, Windows)
- Use CI/CD (GitHub Actions) for automated testing
- Write platform-specific test cases

**🔧 Build System:**
- Use CMake for cross-platform builds
- Separate platform-specific source files
- Link platform-specific libraries (`pthread` on POSIX)

---

**Return to:** [00_INDEX.md](./00_INDEX.md) for navigation
