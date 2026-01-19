# AriaMake Build System - Overview

## Executive Summary

**aria_make** is a modern, native build automation tool designed specifically for the Aria programming language ecosystem. It replaces GNU Make with a declarative, "Build System as Code" approach that prioritizes:

- **Human readability** - JSON-like syntax with unquoted keys, comments, trailing commas
- **Determinism** - Reproducible builds across platforms with sorted outputs
- **Performance** - Sub-10ms parsing, parallel compilation, smart incrementalism
- **Developer experience** - Precise error messages, LSP integration, hermetic state

---

## System Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                      build.aria                              │
│  (JSON-superset config with interpolation)                   │
└────────────────────────┬─────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                  PARSING & CONFIGURATION                       │
├────────────────────────────────────────────────────────────────┤
│  ConfigParser  →  Variable Interpolator  →  BuildFileAST      │
│  (Reuses Aria    (Tri-color DFS for        (project, vars,    │
│   lexer)          cycle detection)          targets)          │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                    FILE DISCOVERY                              │
├────────────────────────────────────────────────────────────────┤
│  GlobEngine  →  FileSystemTraits  →  Source File List         │
│  (Anchor-point  (Path normalization, (Sorted,                 │
│   optimization,  hidden detection)    deterministic)          │
│   exclusion pruning)                                           │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                   DEPENDENCY GRAPH                             │
├────────────────────────────────────────────────────────────────┤
│  DependencyGraph  →  CycleDetector  →  Validated DAG           │
│  (Nodes with         (Tri-color DFS    (Topologically         │
│   atomic in-degree)   with path        sorted)                │
│                       reconstruction)                          │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                   INCREMENTAL ANALYSIS                         │
├────────────────────────────────────────────────────────────────┤
│  Timestamp Checker  +  Command Signature Hasher               │
│  (mtime propagation)   (Detects flag changes)                 │
│                                                                │
│  Output: Dirty node list                                      │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                    EXECUTION ENGINE                            │
├────────────────────────────────────────────────────────────────┤
│  BuildScheduler (Kahn's algorithm)                            │
│       │                                                        │
│       v                                                        │
│  ThreadPool ─> ToolchainOrchestrator ─> Process Execution    │
│  (Fixed-size   (ariac + lld command     (PAL: pipes,          │
│   workers)      synthesis)               thread draining)     │
└────────────────────────┬───────────────────────────────────────┘
                         │
                         v
┌────────────────────────────────────────────────────────────────┐
│                      ARTIFACTS                                 │
├────────────────────────────────────────────────────────────────┤
│  - Compiled binaries (.o, executables)                        │
│  - compile_commands.json (LSP integration)                    │
│  - .d dependency files (header tracking)                      │
│  - .aria_build_state.json (command hashes)                    │
└────────────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. Parser & Configuration
- **ConfigParser** - Recursive descent parser for ABC (Aria Build Configuration) format
- **Variable Interpolator** - Resolves `&{variable}` syntax with cycle detection
- **Input:** `build.aria` file
- **Output:** `BuildFileAST` with resolved values

### 2. File Discovery
- **GlobEngine** - Pattern matching with `*, ?, **, [...]` support
- **FileSystemTraits** - Cross-platform path normalization and metadata
- **Input:** Glob patterns from config
- **Output:** Sorted list of source files

### 3. Dependency Graph
- **DependencyGraph** - DAG representing build targets and dependencies
- **CycleDetector** - Validates graph, distinguishes diamonds from cycles
- **Input:** Target definitions from AST
- **Output:** Validated DAG with topological order

### 4. Execution Engine
- **BuildScheduler** - Kahn's algorithm for parallel task dispatch
- **ThreadPool** - Fixed-size worker pool (`std::thread::hardware_concurrency()`)
- **Process Execution** - Platform abstraction layer (PAL) for spawning compiler
- **ToolchainOrchestrator** - Translates targets into `ariac`/`lld` commands
- **Input:** DAG + dirty nodes
- **Output:** Compiled artifacts

### 5. Incremental Builds
- **Timestamp Checker** - Compares mtimes: source vs output vs dependencies
- **Command Signature Hasher** - Detects flag changes (`-O0` → `-O3`)
- **BuildState** - Persists hashes to `.aria_build_state.json`
- **Input:** Current graph + previous state
- **Output:** Set of dirty nodes requiring rebuild

### 6. Tooling Integration
- **CompileDBWriter** - Generates `compile_commands.json` for LSP
- **DependencyPrinter** - Emits `.d` files for header dependency tracking
- **Input:** Build graph + commands
- **Output:** LSP-compatible metadata

---

## Key Design Decisions

### Memory Management
- **Ownership:** `std::unique_ptr` for node storage (graph owns nodes)
- **References:** Raw pointers for edges (non-owning, graph-scoped)
- **Future:** Arena allocator for AST (not C++17 stdlib)

### Concurrency
- **Fixed thread pool** (not `std::async` chaos)
- **Atomic counters** for in-degree updates (lock-free)
- **Mutex-protected task queue** with condition variable signaling

### Determinism
- **Sorted glob results** (lexicographic order)
- **Normalized paths** (POSIX `/` separator)
- **Hermetic state** (command hashes prevent drift)
- **No timestamp races** (dependency propagation)

### Performance Targets
| Operation | Target | Strategy |
|-----------|--------|----------|
| Parse build.aria (1000 lines) | < 10ms | Reuse Aria lexer, predictive descent |
| Glob 10k files | < 50ms | Anchor-point optimization, exclusion pruning |
| Cycle detection | < 5ms | Tri-color DFS, O(V+E) |
| Scheduling overhead | < 1% | Atomic operations, cache locality |

---

## ABC Format Example

```javascript
// build.aria - Aria Build Configuration
project: {
    name: "AriaApp",
    version: "0.1.0"
}

variables: {
    src: "src",
    build: "build",
    flags: "-O2 -Wall"
}

targets: [
    {
        name: "utils",
        type: "library",
        sources: "&{src}/utils/**/*.aria",
        output: "&{build}/libutils.a",
        flags: "&{flags}"
    },
    {
        name: "app",
        type: "executable",
        sources: "&{src}/main.aria",
        output: "&{build}/app",
        dependencies: ["utils"],
        flags: "&{flags}"
    }
]
```

**Key Features:**
- Unquoted keys (`name:` not `"name":`)
- Comments (`//`)
- Variable interpolation (`&{src}`)
- Trailing commas allowed
- JSON-compatible (can parse standard JSON too)

---

## Data Flow

1. **Parse:** `build.aria` → `BuildFileAST`
2. **Interpolate:** Resolve `&{variables}` recursively
3. **Discover:** Expand glob patterns to file lists
4. **Build Graph:** Construct `DependencyGraph` from targets
5. **Validate:** Detect cycles, verify all deps exist
6. **Analyze:** Mark dirty nodes (incremental build logic)
7. **Schedule:** Kahn's algorithm finds ready-to-build nodes
8. **Execute:** ThreadPool dispatches compiler tasks
9. **Update:** Persist command hashes, update timestamps
10. **Report:** Output success/failure, generate metadata

---

## Error Handling Strategy

### Parse Errors
- **Panic mode recovery** - Skip to next sync point (`;`, `}`, `]`)
- **Line/column precision** - "Error at line 42, column 15"
- **Context-aware messages** - "Expected ':' after key 'name'"

### Cycle Detection
- **Full path reconstruction** - "Cycle: A → B → C → A"
- **Distinguish diamonds** - Shared dependencies OK, loops forbidden

### Build Failures
- **Capture stdout/stderr** - Per-task output isolation
- **Graceful shutdown** - Stop scheduling, let running tasks finish
- **Error propagation** - Mark dependent nodes as failed

---

## Platform Support

| Platform | Status | Notes |
|----------|--------|-------|
| Linux (x64) | Primary | POSIX pipes, fork/exec |
| macOS (x64/ARM) | Supported | BSD-compatible |
| Windows 10+ | Supported | CreateProcess, UTF-16 handling |

**C++ Standard:** C++17 (for `std::filesystem`, `std::optional`, `std::variant`)

---

## External Dependencies

- **Aria Compiler (`ariac`)** - Required for compilation
- **LLVM Linker (`lld`)** - For linking object files
- **LLVM Interpreter (`lli`)** - For test execution
- **nlohmann/json** (optional) - For BuildState persistence

**Aria Standard Library Gap:** No native globbing or filesystem APIs → C++17 implementation required

---

## Success Criteria

A successful aria_make implementation:
- ✅ Parses any valid ABC file in < 10ms
- ✅ Detects all cycles with full path reporting
- ✅ Correctly handles diamond dependencies
- ✅ Rebuilds only dirty nodes (timestamp + command hash)
- ✅ Saturates CPU with parallel compilation
- ✅ Generates correct `compile_commands.json`
- ✅ Cross-platform compatible (Linux, macOS, Windows)
- ✅ Zero crashes on malformed input (graceful error recovery)

---

**Next:** Read [02_DEPENDENCY_MATRIX.md](./02_DEPENDENCY_MATRIX.md) to understand component relationships
