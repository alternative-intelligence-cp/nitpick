# aria_make

**Build System for Aria Language**

Fast, incremental build system with intelligent dependency tracking and full FFI support.

## Features

- ✅ **Incremental Builds** - SHA-256 content hashing, only rebuild what changed
- ✅ **Parallel Compilation** - Multi-threaded builds for maximum performance
- ✅ **Dependency Tracking** - Automatic cycle detection and topological sorting
- ✅ **FFI Support** - Compile C/C++ libraries and link with Aria code
- ✅ **Glob Patterns** - Source file wildcards via aglob engine
- ✅ **Clean API** - Simple ABC format configuration

## Quick Start

### Build Configuration (`build.abc`)

```ini
[project]
name = "myproject"
version = "0.1.0"

[compiler]
path = "/path/to/ariac"

# Aria binary target
[target.myapp]
type = "binary"
sources = ["src/*.aria"]
flags = ["-O2"]
deps = []

# C library for FFI
[target.mylib]
type = "c_library"
sources = ["ffi/wrapper.c"]
compiler = "gcc"
flags = ["-fPIC", "-O2"]
output = "libmylib.a"

# Aria binary using FFI
[target.app_with_ffi]
type = "binary"
sources = ["src/main.aria"]
deps = ["mylib"]
link_libraries = ["mylib", "m"]
link_paths = [".aria_make/build"]
flags = ["-O2"]
```

### Build Commands

```bash
# Build all targets
aria_make

# Build specific target
aria_make myapp

# Verbose output
aria_make -v

# Force rebuild
aria_make --force

# Clean build artifacts
aria_make --clean
```

## FFI Example

### C Library (`ffi/math_wrapper.c`)

```c
#include <math.h>

double aria_sqrt(double x) {
    return sqrt(x);
}
```

### Aria Code (`src/main.aria`)

```aria
extern "math_wrapper" {
    func:aria_sqrt = float64(float64:x);
}

func:main = int32() {
    float64:result = aria_sqrt(16.0);  // result = 4.0
    return 0;
};
```

### Build Configuration

```ini
[target.math_wrapper]
type = "c_library"
sources = ["ffi/math_wrapper.c"]
compiler = "gcc"
flags = ["-fPIC", "-O2"]

[target.main]
type = "binary"
sources = ["src/main.aria"]
deps = ["math_wrapper"]
link_libraries = ["math_wrapper", "m"]
link_paths = [".aria_make/build"]
```

Run `aria_make` and it will:
1. Compile `math_wrapper.c` → `libmath_wrapper.a` (gcc + ar)
2. Compile `main.aria` → `main` (ariac with linking)
3. Binary is ready at `.aria_make/build/main`

## Build Configuration Reference

### Project Section

```ini
[project]
name = "project_name"      # Project identifier
version = "0.1.0"          # Semantic version
```

### Compiler Section

```ini
[compiler]
path = "/path/to/ariac"    # Aria compiler path
```

### Target Types

#### Binary Target (Aria executable)

```ini
[target.myapp]
type = "binary"
sources = ["src/*.aria", "lib/utils.aria"]  # Supports globs
deps = ["libname"]                           # Other targets to build first
flags = ["-O2", "-g"]                       # Compiler flags
link_libraries = ["m", "pthread"]           # Libraries to link (-l)
link_paths = ["/usr/local/lib"]             # Library search paths (-L)
```

#### C Library Target (FFI)

```ini
[target.myclib]
type = "c_library"
sources = ["ffi/*.c"]
compiler = "gcc"           # gcc, g++, clang, clang++
flags = ["-fPIC", "-O2"]
output = "libmyclib.a"     # Explicit output filename
```

#### Aria Library Target (future)

```ini
[target.mylib]
type = "library"
sources = ["lib/*.aria"]
# Requires ariac -c support (not yet implemented)
```

## Architecture

### Components

- **StateManager** - Content-addressable build state (SHA-256 hashing)
- **ABC Parser** - INI-style build configuration parser
- **BuildOrchestrator** - Dependency graph, parallel execution
- **CompilerInterface** - Fork/exec wrapper for ariac
- **CCompilerInterface** - Fork/exec wrapper for gcc/g++/clang
- **GlobBridge** - Pattern expansion via aglob

### Build Flow

```
1. Parse build.abc → BuildFileNode AST
2. Load previous build state (.aria_make/state.json)
3. Expand glob patterns in source lists
4. Build dependency graph with cycle detection
5. Determine dirty targets (content changed?)
6. Topological sort for build order
7. Parallel compilation via thread pool
8. Update and save build state
```

### Incremental Builds

aria_make tracks:
- Source file content hashes (SHA-256)
- Dependency changes
- Compiler flags changes

Rebuild triggered only when:
- Source content changes
- Dependencies rebuilt
- Compiler flags modified

## Performance

Typical build times:
- **Full build** (C lib + Aria binary with FFI): ~330ms
- **Incremental** (up-to-date check): ~21ms
- **Parallel builds**: N threads (auto-detected or configurable)

## Development Status

**Current Version:** 0.1.0-dev

### Completed (Phase 1 + 1.5)
- ✅ ABC configuration parser
- ✅ StateManager with content hashing
- ✅ Dependency graph and cycle detection
- ✅ Parallel build execution
- ✅ Aria compiler integration
- ✅ C/C++ compiler integration
- ✅ FFI support (extern + linking)
- ✅ Glob pattern expansion
- ✅ Incremental builds

### Planned (Phase 2+)
- ⏳ Enhanced error reporting with colors
- ⏳ Structured diagnostics
- ⏳ Parallel testing support
- ⏳ Optimization flags analysis
- ⏳ Package manager integration

## Testing

Run the test suite:

```bash
cd build
./test_state_manager
```

Example projects:
- `test_project/` - Basic Aria compilation
- `test_project_ffi/` - Full FFI example with C library

## Building aria_make

### Requirements
- CMake 3.16+
- C++17 compiler (GCC 7+, Clang 5+)
- Aria compiler (ariac) for testing

### Build Steps

```bash
mkdir build && cd build
cmake ..
make

# Run tests
./test_state_manager

# Try example project
cd ../test_project
../build/aria_make
```

## Technical Notes

### SIGPIPE Fix (Exit 141)

Early versions had a deadlock when ariac wrote large amounts of debug output:
- **Problem**: 64KB pipe buffer fills, child blocks on write
- **Symptom**: Exit code 141 (128 + SIGPIPE)
- **Solution**: Use `select()` to read stdout/stderr while child runs

### Thread Safety

StateManager uses mutexes for:
- State file I/O
- Hash cache access
- Build record updates

Safe for parallel compilation.

## License

Part of the Aria Language Project
Copyright © 2025-2026

## Related Projects

- **ariac** - Aria compiler
- **aria_utils** - Shared utilities (aglob, etc.)
- **aria-pkg** - Package manager

---

**Alternative Intelligence Liberation Platform (AILP)**  
*Building tools for genuine AI development*

