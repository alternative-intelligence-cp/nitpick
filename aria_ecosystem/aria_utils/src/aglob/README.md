# aglob - Aria Globbing Engine

High-performance, deterministic glob pattern matching engine for the Aria ecosystem.

## Purpose

Provides file pattern matching for:
- **aria_make** - Build system source file discovery
- **ariac** - Compiler driver file expansion
- **Future std.fs.glob** - Native Aria filesystem module

## Features

- **ABC Glob Compliance**: Full support for `*`, `**`, `?`, `[...]` patterns
- **Deterministic Output**: Canonical sorting ensures reproducible builds
- **Anchoring Optimization**: Skips irrelevant subtrees for performance
- **Cross-Platform**: Windows, Linux, macOS support
- **FFI Bridge**: C-compatible API for integration with Aria runtime
- **Symlink Safety**: Cycle detection prevents infinite loops

## Quick Start

```cpp
#include "aglob/glob_engine.hpp"

using namespace aria::glob;

int main() {
    GlobEngine engine;

    auto [paths, err] = engine.match("/project", "src/**/*.aria");

    if (err == GlobError::OK) {
        for (const auto& path : paths) {
            std::cout << path << "\n";
        }
    }

    return 0;
}
```

## Pattern Syntax

| Pattern | Description | Example |
|---------|-------------|---------|
| `*` | Match any characters in single directory | `*.aria` matches `main.aria` |
| `**` | Match across directories (recursive) | `**/*.aria` matches `src/core/file.aria` |
| `?` | Match exactly one character | `file?.txt` matches `file1.txt` |
| `[abc]` | Match one of the characters | `[abc].txt` matches `a.txt` |
| `[a-z]` | Match character range | `[0-9].txt` matches `5.txt` |
| `[!x]` | Match any character except x | `[!0-9].txt` matches `a.txt` |

## FFI Interface

For integration with Aria or C code:

```c
#include "aglob/glob_engine.hpp"

// Match pattern
AriaGlobResult result = aria_glob_match("/project", "src/**/*.aria");

// Process results
for (size_t i = 0; i < result.count; i++) {
    printf("%s\n", result.paths[i]);
}

// Free memory
aria_glob_free(&result);
```

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
make test
./test_aglob
```

## Configuration Options

```cpp
GlobOptions opts;
opts.case_sensitive = true;       // Case-sensitive matching
opts.follow_symlinks = false;     // Don't follow symlinks (safety)
opts.max_depth = 64;              // Maximum recursion depth
opts.files_only = true;           // Return only files
opts.include_hidden = false;      // Skip dotfiles
opts.skip_permission_errors = true; // Continue on permission denied

GlobEngine engine(opts);
```

## Architecture

```
aria::glob::GlobPattern   - Pattern parsing and analysis
aria::glob::GlobEngine    - Traversal and matching
aria::glob::GlobOptions   - Configuration
glob_ffi.cpp              - C-compatible interface
```

## Implementation Notes

- Uses C++17 `std::filesystem` for portable traversal
- Glob patterns are transpiled to regex for matching
- Results are sorted using `generic_string()` for cross-platform consistency
- Anchoring optimization starts traversal from longest literal prefix

## License

Part of the Aria Language Project. See LICENSE.md in repository root.
