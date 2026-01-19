# Build System ↔ Compiler Integration Guide

**Document Type**: Integration Guide  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Reference Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Build Configuration (ABC)](#build-configuration-abc)
3. [Compiler Invocation](#compiler-invocation)
4. [Dependency Resolution](#dependency-resolution)
5. [Incremental Compilation](#incremental-compilation)
6. [Parallel Builds](#parallel-builds)
7. [Error Handling](#error-handling)

---

## Overview

**AriaBuild** (`ariab`) orchestrates the compilation of Aria projects by:

1. **Parsing build.abc** (project configuration)
2. **Resolving dependencies** (local & external)
3. **Invoking ariac** (compiler) with correct flags
4. **Managing incremental builds** (only recompile changed files)
5. **Parallelizing compilation** (use all CPU cores)

---

## Build Configuration (ABC)

### ABC File Format

**Aria Build Configuration** (ABC): JSON-based project manifest

**Example**: `build.abc`
```json
{
  "name": "my_project",
  "version": "1.0.0",
  "type": "executable",
  "entry": "src/main.aria",
  
  "sources": [
    "src/**/*.aria"
  ],
  
  "dependencies": {
    "http_client": "^2.1.0",
    "json_parser": "^1.3.0"
  },
  
  "compiler": {
    "optimization": "O2",
    "warnings": "all",
    "debug_symbols": true
  },
  
  "output": {
    "directory": "build/",
    "name": "my_app"
  }
}
```

---

### Key Fields

**Project Metadata**:
- `name`: Project name (used for output binary)
- `version`: Semantic version (1.0.0)
- `type`: `"executable"`, `"library"`, or `"test"`
- `entry`: Main entry point file (for executables)

**Source Files**:
- `sources`: Glob patterns for source files
- `exclude`: Files to exclude (optional)

**Dependencies**:
- `dependencies`: External packages (from AriaX registry)
- `dev_dependencies`: Only needed for development/testing

**Compiler Options**:
- `optimization`: `"O0"` (none), `"O1"`, `"O2"`, `"O3"` (aggressive)
- `warnings`: `"none"`, `"all"`, `"error"` (warnings as errors)
- `debug_symbols`: `true`/`false` (DWARF generation)

---

## Compiler Invocation

### Basic Invocation

**Command**: `ariab build`

**Internal Process**:
```bash
# AriaBuild expands to:
ariac src/main.aria \
  --output build/my_app \
  --optimization O2 \
  --warnings all \
  --debug-symbols \
  --link-dependencies http_client json_parser
```

---

### Step-by-Step

**1. Parse build.abc**:
```cpp
// AriaBuild code (simplified)
json config = parse_json("build.abc");

std::string name = config["name"];
std::string optimization = config["compiler"]["optimization"];
bool debug = config["compiler"]["debug_symbols"];
```

---

**2. Resolve Source Files**:
```cpp
std::vector<std::string> sources = glob(config["sources"]);
// sources = ["src/main.aria", "src/utils.aria", "src/parser.aria"]
```

---

**3. Build Compiler Command**:
```cpp
std::vector<std::string> args;
args.push_back("ariac");

// Add source files
for (const auto& src : sources) {
    args.push_back(src);
}

// Add flags
args.push_back("--output");
args.push_back(config["output"]["directory"] + config["output"]["name"]);

args.push_back("--optimization");
args.push_back(optimization);

if (debug) {
    args.push_back("--debug-symbols");
}

// Add dependencies
for (const auto& [dep_name, dep_version] : config["dependencies"]) {
    args.push_back("--link-dependency");
    args.push_back(dep_name);
}
```

---

**4. Execute Compiler**:
```cpp
// Spawn ariac process
pid_t pid = fork();
if (pid == 0) {
    // Child: Execute compiler
    execvp("ariac", args.data());
    _exit(127);  // Failed to exec
} else {
    // Parent: Wait for compilation
    int status;
    waitpid(pid, &status, 0);
    
    if (WEXITSTATUS(status) != 0) {
        std::cerr << "Compilation failed with code " << WEXITSTATUS(status) << std::endl;
        exit(1);
    }
}
```

---

### Compiler Flags Reference

| Flag | Purpose | Example |
|------|---------|---------|
| `--output <path>` | Output executable path | `--output build/my_app` |
| `--optimization <level>` | Optimization level | `--optimization O2` |
| `--warnings <mode>` | Warning level | `--warnings all` |
| `--debug-symbols` | Generate DWARF debug info | `--debug-symbols` |
| `--link-dependency <name>` | Link external dependency | `--link-dependency http_client` |
| `--runtime-path <path>` | Custom runtime location | `--runtime-path /opt/aria/runtime` |
| `--no-runtime` | Skip runtime linking | `--no-runtime` |
| `--emit-llvm-ir` | Emit LLVM IR (.ll file) | `--emit-llvm-ir` |
| `--emit-asm` | Emit assembly (.s file) | `--emit-asm` |
| `--verbose` | Verbose output | `--verbose` |

---

## Dependency Resolution

### Dependency Types

**1. External Dependencies** (from AriaX):
```json
"dependencies": {
  "http_client": "^2.1.0"
}
```

**Resolution**:
```bash
# AriaBuild queries AriaX
ariax install http_client@^2.1.0
# Downloads to ~/.aria/packages/http_client/2.1.3/
```

**Compiler Link**:
```bash
ariac src/main.aria \
  --link-dependency http_client \
  --package-path ~/.aria/packages/http_client/2.1.3/lib/libhttp_client.a
```

---

**2. Local Dependencies** (subprojects):
```json
"dependencies": {
  "local:../shared_utils": "*"
}
```

**Resolution**:
```bash
# AriaBuild recursively builds local dependency
cd ../shared_utils
ariab build
cd ../my_project

# Link against built artifact
ariac src/main.aria \
  --link-dependency shared_utils \
  --package-path ../shared_utils/build/libshared_utils.a
```

---

### Dependency Graph

**Example Project Structure**:
```
my_project/
  build.abc  (depends on: http_client, local:../shared_utils)
    ↓
http_client/  (external, from AriaX)
    ↓
  json_parser  (transitive dependency)

shared_utils/  (local)
  build.abc  (depends on: string_utils)
    ↓
  string_utils  (external)
```

**Build Order** (topological sort):
1. `string_utils` (external, from AriaX)
2. `shared_utils` (local, depends on string_utils)
3. `json_parser` (external, from AriaX)
4. `http_client` (external, depends on json_parser)
5. `my_project` (root, depends on http_client & shared_utils)

---

### build.lock File

**Purpose**: Lock exact versions for reproducible builds

**Generated**: First time `ariab build` runs

**Example**: `build.lock`
```json
{
  "dependencies": {
    "http_client": {
      "version": "2.1.3",
      "resolved": "https://aria-packages.org/http_client-2.1.3.tar.gz",
      "checksum": "sha256:abc123...",
      "dependencies": {
        "json_parser": "1.3.2"
      }
    },
    "json_parser": {
      "version": "1.3.2",
      "resolved": "https://aria-packages.org/json_parser-1.3.2.tar.gz",
      "checksum": "sha256:def456..."
    }
  }
}
```

**Usage**: Ensure same versions across all machines

---

## Incremental Compilation

### Tracking Changes

**Problem**: Recompiling entire project on every build is slow

**Solution**: Track file changes, only recompile modified files

---

### Change Detection

**Method 1: Timestamp Comparison**

```cpp
// Check if source file is newer than object file
struct stat src_stat, obj_stat;
stat("src/main.aria", &src_stat);
stat("build/main.o", &obj_stat);

if (src_stat.st_mtime > obj_stat.st_mtime) {
    // Source is newer, recompile
    compile("src/main.aria", "build/main.o");
}
```

**Limitation**: Doesn't detect content changes (e.g., `touch` updates timestamp without changing content)

---

**Method 2: Content Hash**

```cpp
// Compute SHA-256 of source file
std::string current_hash = sha256_file("src/main.aria");

// Load previous hash from cache
std::string cached_hash = load_hash_from_cache("src/main.aria");

if (current_hash != cached_hash) {
    // Content changed, recompile
    compile("src/main.aria", "build/main.o");
    save_hash_to_cache("src/main.aria", current_hash);
}
```

**Benefit**: Only recompiles when content actually changes

---

### Dependency Tracking

**Problem**: If `utils.aria` changes, `main.aria` (which imports it) must be recompiled

**Solution**: Track import dependencies

**Dependency Graph**:
```
main.aria
  ├── imports utils.aria
  └── imports parser.aria
      └── imports string_utils.aria
```

**Change Propagation**:
- If `string_utils.aria` changes → recompile `parser.aria`, `main.aria`
- If `utils.aria` changes → recompile `main.aria`
- If `main.aria` changes → recompile only `main.aria`

---

**Implementation**:
```cpp
// Build dependency map
std::map<std::string, std::set<std::string>> imports;
imports["main.aria"] = {"utils.aria", "parser.aria"};
imports["parser.aria"] = {"string_utils.aria"};

// Find files that need recompilation
std::set<std::string> changed = {"string_utils.aria"};
std::set<std::string> to_recompile = transitive_closure(imports, changed);
// to_recompile = {"string_utils.aria", "parser.aria", "main.aria"}
```

---

### Cache Storage

**Location**: `.aria_build_cache/`

**Structure**:
```
.aria_build_cache/
  hashes.json       # File content hashes
  dependencies.json # Import graph
  timestamps.json   # Last build times
```

**hashes.json**:
```json
{
  "src/main.aria": "sha256:abc123...",
  "src/utils.aria": "sha256:def456...",
  "src/parser.aria": "sha256:ghi789..."
}
```

**dependencies.json**:
```json
{
  "src/main.aria": ["src/utils.aria", "src/parser.aria"],
  "src/parser.aria": ["src/string_utils.aria"]
}
```

---

## Parallel Builds

### Parallelization Strategy

**Goal**: Use all CPU cores to compile multiple files simultaneously

**Constraint**: Respect dependency order

---

### DAG-Based Scheduling

**Dependency Graph** (Directed Acyclic Graph):
```
       main.aria
       /       \
    utils.aria  parser.aria
                    |
                string_utils.aria
```

**Topological Levels**:
- **Level 0**: `string_utils.aria` (no dependencies)
- **Level 1**: `utils.aria`, `parser.aria` (depend on level 0)
- **Level 2**: `main.aria` (depends on level 1)

**Parallel Execution**:
1. Compile level 0 files (all in parallel)
2. Wait for all level 0 to complete
3. Compile level 1 files (all in parallel)
4. Wait for all level 1 to complete
5. Compile level 2 files

---

### Thread Pool Implementation

```cpp
#include <thread>
#include <queue>
#include <mutex>

class ThreadPool {
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop = false;

public:
    ThreadPool(size_t num_threads) {
        for (size_t i = 0; i < num_threads; i++) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
                        if (stop && tasks.empty()) return;
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    task();  // Execute compilation task
                }
            });
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        condition.notify_all();
        for (std::thread& worker : workers) {
            worker.join();
        }
    }
};
```

---

**Usage**:
```cpp
ThreadPool pool(std::thread::hardware_concurrency());

// Level 0 (parallel)
pool.enqueue([&] { compile("src/string_utils.aria", "build/string_utils.o"); });
// Wait for level 0 to complete...

// Level 1 (parallel)
pool.enqueue([&] { compile("src/utils.aria", "build/utils.o"); });
pool.enqueue([&] { compile("src/parser.aria", "build/parser.o"); });
// Wait for level 1 to complete...

// Level 2
pool.enqueue([&] { compile("src/main.aria", "build/main.o"); });
```

---

### Progress Reporting

**Terminal Output**:
```
[1/5] Compiling src/string_utils.aria...
[2/5] Compiling src/utils.aria...
[3/5] Compiling src/parser.aria...
[4/5] Compiling src/main.aria...
[5/5] Linking build/my_app...
Build complete in 3.2s
```

**Implementation**:
```cpp
std::atomic<int> completed(0);
int total_files = sources.size();

auto compile_with_progress = [&](const std::string& src, const std::string& obj) {
    int current = ++completed;
    std::cout << "[" << current << "/" << total_files << "] Compiling " << src << "..." << std::endl;
    compile(src, obj);
};
```

---

## Error Handling

### Compilation Errors

**Scenario**: Source file has syntax error

**Compiler Output** (stderr):
```
error: Expected ';' after statement
  --> src/main.aria:10:25
   |
10 | var:x = i32(42)
   |                 ^
   | help: Add ';' here
```

**AriaBuild Behavior**:
1. Capture compiler stderr
2. Display to user (with color coding)
3. Stop build immediately (fail-fast)
4. Exit with non-zero code

---

**Implementation**:
```cpp
int compile(const std::string& src, const std::string& obj) {
    // Create pipes for stderr
    int stderr_pipe[2];
    pipe(stderr_pipe);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child: Redirect stderr
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        
        execl("ariac", "ariac", src.c_str(), "--output", obj.c_str(), nullptr);
        _exit(127);
    } else {
        // Parent: Capture stderr
        close(stderr_pipe[1]);
        
        char buffer[4096];
        ssize_t n;
        while ((n = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
            // Display errors in red
            std::cerr << "\033[31m";
            write(STDERR_FILENO, buffer, n);
            std::cerr << "\033[0m";
        }
        
        int status;
        waitpid(pid, &status, 0);
        
        return WEXITSTATUS(status);
    }
}
```

---

### Dependency Resolution Errors

**Scenario**: Dependency not found

**Error**:
```
error: Package 'http_client@^2.1.0' not found in AriaX registry
  --> build.abc:8:5
   |
8  |   "http_client": "^2.1.0"
   |   ^^^^^^^^^^^^^^
   | help: Check package name and version
   | note: Available versions: 1.0.0, 1.5.2, 2.0.1
```

**AriaBuild Behavior**:
1. Query AriaX registry
2. If package doesn't exist, report error with suggestions
3. Exit with code 1

---

### Linking Errors

**Scenario**: Undefined symbol (missing function)

**Linker Output**:
```
undefined reference to `http_request'
```

**AriaBuild Behavior**:
1. Capture linker output
2. Suggest possible causes:
   - Missing dependency
   - Typo in function name
   - Incorrect import
3. Exit with code 1

---

## Related Documents

- **[ARIABUILD](../components/ARIABUILD.md)**: Build system architecture
- **[ARIA_COMPILER](../components/ARIA_COMPILER.md)**: Compiler architecture
- **[ARIAX](../components/ARIAX.md)**: Package manager integration
- **[COMPILER_RUNTIME](./COMPILER_RUNTIME.md)**: Compiler ↔ Runtime linking
- **[DATA_FLOW](../DATA_FLOW.md)**: Build system execution trace

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Reference guide (implementation planned)
