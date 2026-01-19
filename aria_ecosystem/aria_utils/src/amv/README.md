# amv - Aria File Move/Rename Library

A high-performance C++17 library for moving and renaming files with
atomic operations when possible.

## Features

- **Atomic Rename**: Uses rename() when on same filesystem
- **Cross-Device Move**: Copy + delete for different filesystems
- **Backup Support**: Create backups before overwrite
- **TBB-Safe Arithmetic**: Overflow-safe byte counting
- **Progress Callbacks**: Monitor move progress
- **C FFI**: Call from Aria runtime or other languages

## Usage

### C++ API

```cpp
#include <amv/file_move.hpp>

using namespace aria::amv;

// Simple move
MoveResult result = move_file("/src/file.txt", "/dst/file.txt");
if (result.used_rename) {
    // Atomic rename was used
}

// Move with options
MoveOptions opts;
opts.force = true;           // Overwrite existing
opts.backup = true;          // Create backup
result = move_file("/src/file.txt", "/dst/file.txt", opts);

// Move multiple files to directory
std::vector<std::string> sources = {"file1.txt", "file2.txt"};
result = move_files(sources, "/dst/dir");

// Simple rename
result = rename_file("/old/path.txt", "/new/path.txt");
```

### C FFI

```c
#include <amv/file_move.hpp>

// Simple move
AriaError err = aria_mv_file("/src/file.txt", "/dst/file.txt");

// Rename
err = aria_mv_rename("/old.txt", "/new.txt");

// Check error
if (err != 0) {
    printf("Error: %s\n", aria_mv_last_error());
}
```

## Options

| Option | Flag | Description |
|--------|------|-------------|
| force | -f | Overwrite existing files |
| no_clobber | -n | Don't overwrite existing |
| update | -u | Only move if source is newer |
| backup | -b | Create backup before overwrite |
| backup_suffix | | Suffix for backups (default "~") |

## Behavior

1. **Same Filesystem**: Uses atomic `rename()` syscall
2. **Cross Filesystem**: Copies data then deletes source
3. **Directories**: Recursively moves contents

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
./test_amv
```

## License

Copyright (c) 2025 Aria Language Project
