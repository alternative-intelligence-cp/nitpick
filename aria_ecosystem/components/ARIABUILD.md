# AriaBuild - Aria Build System

**Component Type**: Build System / Build Orchestrator  
**Language**: C++20 (planned)  
**Repository**: `/home/randy/._____RANDY_____/REPOS/aria_make` (design phase)  
**Configuration Format**: ABC (Aria Build Configuration) - JSON-based  
**Status**: Design Phase  
**Version**: v0.0.1-concept

---

## Table of Contents

1. [Overview](#overview)
2. [Design Philosophy](#design-philosophy)
3. [ABC Configuration Format](#abc-configuration-format)
4. [Build Pipeline](#build-pipeline)
5. [Dependency Management](#dependency-management)
6. [Incremental Compilation](#incremental-compilation)
7. [Parallel Execution](#parallel-execution)
8. [Integration Points](#integration-points)

---

## Overview

AriaBuild is the official build system for Aria projects. It orchestrates compilation, linking, testing, and packaging through a declarative configuration file (build.abc) and intelligent dependency analysis.

### Key Features

- **Declarative Configuration**: JSON-based build.abc files describe project structure
- **Automatic Dependency Tracking**: Analyzes imports to build dependency graphs
- **Incremental Builds**: Only recompiles changed files and their dependents
- **Parallel Compilation**: Exploits multi-core CPUs for faster builds
- **Smart Linking**: Automatically links with libaria_runtime.a
- **Cross-Platform**: Linux, Windows, macOS support
- **Package Integration**: Works with AriaX package manager

---

## Design Philosophy

### 1. Convention Over Configuration

**Minimal Configuration**:
```json
{
  "project": {
    "name": "my_app",
    "version": "1.0.0"
  },
  "targets": {
    "binary": {
      "type": "executable"
    }
  }
}
```

**Defaults**:
- Source files: `src/**/*.aria`
- Output: `bin/<project_name>`
- Runtime: Automatically linked

### 2. Build as a DAG (Directed Acyclic Graph)

Every build is a dependency graph:
```
helpers.aria  (no deps)
   ↓
utils.aria    (depends on helpers)
   ↓
parser.aria   (depends on utils, helpers)
   ↓
main.aria     (depends on parser, utils)
   ↓
main.o → link → executable
```

**Topological Sort**: Build order determined automatically

### 3. Reproducible Builds

- Deterministic output (same source → same binary)
- Lock file for dependency versions (build.lock)
- Hermetic builds (no external state leakage)

---

## ABC Configuration Format

### Minimal Example

```json
{
  "project": {
    "name": "hello",
    "version": "1.0.0"
  },
  "targets": {
    "binary": {
      "type": "executable",
      "output": "bin/hello",
      "sources": ["src/main.aria"]
    }
  }
}
```

### Full Example

```json
{
  "project": {
    "name": "my_app",
    "version": "1.0.0",
    "authors": ["Alice <alice@example.com>"],
    "license": "MIT"
  },
  
  "dependencies": {
    "std.collections": "1.0.0",
    "std.io": "1.0.0"
  },
  
  "targets": {
    "binary": {
      "type": "executable",
      "output": "bin/my_app",
      "sources": [
        "src/main.aria",
        "src/parser.aria",
        "src/utils.aria"
      ],
      "compile_flags": ["-O2", "--debug-info"]
    },
    
    "lib": {
      "type": "library",
      "output": "lib/libmyapp.a",
      "sources": ["lib/*.aria"],
      "public_headers": ["lib/api.aria"]
    },
    
    "tests": {
      "type": "test",
      "sources": ["tests/**/*.aria"],
      "depends_on": ["binary"]
    }
  },
  
  "build": {
    "parallel_jobs": 8,
    "incremental": true,
    "cache_dir": ".aria_cache"
  }
}
```

### Configuration Schema

| Field | Type | Description |
|-------|------|-------------|
| `project.name` | string | Project name (required) |
| `project.version` | string | Semantic version (required) |
| `project.authors` | string[] | Author list |
| `project.license` | string | SPDX license identifier |
| `dependencies` | object | Package dependencies |
| `targets` | object | Build targets (executable, library, test) |
| `targets.*.type` | enum | "executable", "library", "test" |
| `targets.*.output` | string | Output file path |
| `targets.*.sources` | string[] | Source file globs |
| `targets.*.compile_flags` | string[] | Compiler flags |
| `build.parallel_jobs` | int | Max parallel compilation tasks |
| `build.incremental` | bool | Enable incremental builds |
| `build.cache_dir` | string | Build cache directory |

---

## Build Pipeline

### Phase 1: Configuration Parsing

```
Read build.abc
   ↓
Validate schema
   ↓
Extract targets, dependencies, sources
   ↓
Resolve glob patterns (src/**/*.aria)
   ↓
Configuration object ready
```

### Phase 2: Dependency Resolution

```
For each source file:
   Parse imports (use std.io, use parser, etc.)
   ↓
Build dependency graph
   ↓
Topological sort
   ↓
Compilation order: [helpers, utils, parser, main]
```

**Import Parsing**:
```aria
use std.io;           // External package
use parser;           // Local module
use lib.helpers;      // Library module
```

**Graph Construction**:
```
main.aria:
  imports: [parser, std.io]
  
parser.aria:
  imports: [utils, lib.helpers]
  
utils.aria:
  imports: [lib.helpers]
  
lib.helpers:
  imports: []

Dependency Graph:
  lib.helpers → utils → parser → main
  std.io → main
```

### Phase 3: Incremental Build Check

For each source file:
1. Check if `.o` file exists
2. Compare timestamps:
   - Source file modified?
   - Any dependency modified?
   - Compiler flags changed?
3. Mark for recompilation if needed

**Example**:
```
helpers.aria: modified yesterday
  helpers.o: exists, timestamp yesterday
  → UP TO DATE, skip

utils.aria: modified today
  utils.o: exists, timestamp yesterday
  → OUT OF DATE, recompile

parser.aria: not modified
  parser.o: exists, timestamp yesterday
  BUT depends on utils.aria (changed today)
  → NEEDS RECOMPILATION
```

### Phase 4: Parallel Compilation

**Task Queue**:
```
Queue: [helpers, utils, parser, main]
Workers: 4 threads

t=0s: Thread 1 → compile helpers (no wait)
      Thread 2 → wait (utils needs helpers)
      Thread 3 → wait (parser needs utils, helpers)
      Thread 4 → wait (main needs parser)

t=2s: helpers.o ready
      Thread 2 → compile utils

t=3s: utils.o ready
      Thread 3 → compile parser

t=4s: parser.o ready
      Thread 4 → compile main

t=5s: All .o files ready
```

**Compilation Command**:
```bash
ariac src/utils.aria -o src/utils.o -I/usr/local/include/aria -O2
```

### Phase 5: Linking

```
Collect all .o files
   ↓
Locate runtime library: /usr/local/lib/libaria_runtime.a
   ↓
Invoke linker:
  clang helpers.o utils.o parser.o main.o \
        /usr/local/lib/libaria_runtime.a \
        -o bin/my_app
   ↓
Executable ready
```

---

## Dependency Management

### Dependency Resolution

**build.abc**:
```json
{
  "dependencies": {
    "std.collections": "1.0.0",
    "mylib": "^2.3.0"
  }
}
```

**Resolution Process**:
1. Query AriaX package index for latest compatible versions
2. Check if already installed in `/usr/local/aria/packages/`
3. Download if needed (via AriaX)
4. Generate `build.lock` with exact versions

**build.lock** (generated):
```json
{
  "dependencies": {
    "std.collections": {
      "version": "1.0.0",
      "checksum": "sha256:abc123..."
    },
    "mylib": {
      "version": "2.3.5",
      "checksum": "sha256:def456..."
    },
    "std.mem": {
      "version": "1.0.0",
      "checksum": "sha256:789ghi...",
      "note": "transitive dependency of std.collections"
    }
  }
}
```

### Import Resolution

When compiling `main.aria`:
```aria
use std.collections;
use mylib.widgets;
```

**Search Path**:
1. Project source: `src/`, `lib/`
2. Dependencies: `/usr/local/aria/packages/std.collections-1.0.0/src/`
3. System packages: `/usr/local/aria/packages/`

**Compiler Flags**:
```bash
ariac main.aria \
  -I/usr/local/aria/packages/std.collections-1.0.0/include \
  -I/usr/local/aria/packages/mylib-2.3.5/include \
  -o main.o
```

---

## Incremental Compilation

### Timestamp Tracking

**Build Cache** (.aria_cache/):
```
.aria_cache/
├── timestamps.json     # Source file modification times
├── deps.json           # Dependency graph cache
└── hashes.json         # Content hashes for deep checking
```

**timestamps.json**:
```json
{
  "src/main.aria": 1703267890,
  "src/parser.aria": 1703267800,
  "src/utils.aria": 1703267850,
  "src/main.o": 1703267895,
  "src/parser.o": 1703267805,
  "src/utils.o": 1703267855
}
```

### Change Detection

**Algorithm**:
```python
def needs_recompilation(source_file):
    obj_file = source_file.replace('.aria', '.o')
    
    # Check if object file exists
    if not exists(obj_file):
        return True
    
    # Check source file timestamp
    if mtime(source_file) > mtime(obj_file):
        return True
    
    # Check dependency timestamps (recursive)
    for dep in get_dependencies(source_file):
        if needs_recompilation(dep):
            return True
    
    # Check compiler flags
    if compile_flags_changed(source_file):
        return True
    
    return False
```

### Deep Dependency Checking

For robust builds, also hash file contents:

**hashes.json**:
```json
{
  "src/main.aria": "sha256:abc123...",
  "src/parser.aria": "sha256:def456..."
}
```

If timestamp unchanged but hash changed → recompile

---

## Parallel Execution

### Task Scheduling

**DAG Execution**:
```
Dependency Graph:
  A → C → E
  B → D → E
  
Execution Plan:
  Level 0: [A, B] (can run in parallel)
  Level 1: [C, D] (wait for A, B)
  Level 2: [E]    (wait for C, D)

With 4 threads:
  t=0: Thread 1 → A, Thread 2 → B
  t=2: Thread 1 → C, Thread 2 → D
  t=4: Thread 1 → E
```

### Job Limiter

**Respect System Resources**:
```json
{
  "build": {
    "parallel_jobs": 8  // Or auto-detect: CPU cores
  }
}
```

**Implementation**:
```cpp
ThreadPool pool(num_jobs);

for (auto& task : tasks_level_0) {
    pool.enqueue([task]() {
        compile(task);
    });
}
pool.wait_all();

for (auto& task : tasks_level_1) {
    pool.enqueue([task]() {
        compile(task);
    });
}
pool.wait_all();
// ...
```

---

## Integration Points

### With Aria Compiler

**Invokes Compiler**:
```bash
ariab build
  ↓
Invokes: ariac src/main.aria -o main.o
```

**Compiler Integration**:
- AriaBuild passes flags: `-O2`, `--debug-info`, etc.
- Compiler returns exit code (0 = success)
- AriaBuild captures stdout/stderr for error reporting

### With AriaX Package Manager

**Dependency Installation**:
```bash
ariab build
  ↓
Check build.abc dependencies
  ↓
Query AriaX: ariax install std.collections
  ↓
AriaX downloads and installs packages
  ↓
AriaBuild continues compilation
```

**Package Paths**:
- AriaX installs to: `/usr/local/aria/packages/`
- AriaBuild adds to include path: `-I/usr/local/aria/packages/*/include`

### With Aria Runtime

**Automatic Linking**:
```bash
ariab build
  ↓
Link phase: clang *.o /usr/local/lib/libaria_runtime.a -o bin/app
```

No manual linking required - AriaBuild knows to link runtime library.

### With aria_shell

**Shell Integration**:
```bash
aria_shell> ariab build
[Building my_app...]
Compiling src/main.aria... Done
Compiling src/parser.aria... Done
Linking bin/my_app... Done
Build succeeded in 3.2s

aria_shell> ./bin/my_app
Hello, World!
```

---

## Build Commands

### Common Commands

| Command | Description |
|---------|-------------|
| `ariab build` | Build all targets |
| `ariab build binary` | Build specific target |
| `ariab clean` | Remove build artifacts |
| `ariab rebuild` | Clean + build |
| `ariab test` | Run tests |
| `ariab install` | Install to system |
| `ariab package` | Create distributable package |

### Build Workflow

```bash
# Initial build
$ ariab build
[Building my_app v1.0.0]
Compiling 4 files...
  src/helpers.aria... Done (0.5s)
  src/utils.aria... Done (0.6s)
  src/parser.aria... Done (0.7s)
  src/main.aria... Done (0.6s)
Linking bin/my_app... Done (0.3s)
Build succeeded in 2.7s

# Change one file
$ echo "// comment" >> src/utils.aria

# Incremental build
$ ariab build
[Building my_app v1.0.0]
Compiling 3 files... (1 cached)
  src/utils.aria... Done (0.6s)
  src/parser.aria... Done (0.7s)  [depends on utils]
  src/main.aria... Done (0.6s)    [depends on parser]
Linking bin/my_app... Done (0.3s)
Build succeeded in 2.2s
```

---

## Future Enhancements

### Watch Mode

```bash
ariab watch
# Rebuilds automatically when files change
```

### Remote Caching

```json
{
  "build": {
    "cache_server": "https://cache.aria.example.com"
  }
}
```

Share build artifacts across team via remote cache.

### Distributed Builds

```json
{
  "build": {
    "distributed": true,
    "build_farm": "https://farm.aria.example.com"
  }
}
```

Offload compilation to remote servers.

---

## Related Components

- **[Aria Compiler](ARIA_COMPILER.md)**: Invoked by build system
- **[AriaX](ARIAX.md)**: Provides dependency resolution
- **[Aria Runtime](ARIA_RUNTIME.md)**: Linked automatically
- **[aria_shell](ARIA_SHELL.md)**: Invokes build system

---

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design specification (implementation pending)
