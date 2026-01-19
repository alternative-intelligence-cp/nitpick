# acp - Aria File Copy Library

A high-performance C++17 library for copying files and directories with
zero-copy optimization and attribute preservation.

## Features

- **Zero-Copy I/O**: Uses sendfile()/copy_file_range() on Linux
- **Recursive Directory Copy**: Full directory tree copying
- **Attribute Preservation**: Permissions, timestamps, ownership
- **Symlink Handling**: Copy as links or dereference
- **Progress Callbacks**: Monitor copy progress
- **TBB-Safe Arithmetic**: Overflow-safe byte counting
- **C FFI**: Call from Aria runtime or other languages

## Usage

### C++ API

```cpp
#include <acp/file_copy.hpp>

using namespace aria::acp;

// Simple file copy
CopyResult result = copy_file("/src/file.txt", "/dst/file.txt");

// Copy with options
CopyOptions opts;
opts.force = true;           // Overwrite existing
opts.preserve_mode = true;   // Preserve permissions
opts.preserve_timestamps = true;
result = copy_file("/src/file.txt", "/dst/file.txt", opts);

// Copy directory recursively
result = copy_directory("/src/dir", "/dst/dir");

// Copy multiple files to directory
std::vector<std::string> sources = {"file1.txt", "file2.txt"};
result = copy_files(sources, "/dst/dir");

// With progress callback
result = copy_file("/large/file", "/dst/file", opts,
    [](const CopyProgress& prog) {
        std::cout << prog.percent << "% complete" << std::endl;
        return true;  // Continue
    });
```

### C FFI

```c
#include <acp/file_copy.hpp>

// Simple copy
AriaError err = aria_cp_file("/src/file.txt", "/dst/file.txt");

// Copy directory
err = aria_cp_dir("/src/dir", "/dst/dir");

// Check error
if (err != 0) {
    printf("Error: %s\n", aria_cp_last_error());
}
```

## Options

| Option | Flag | Description |
|--------|------|-------------|
| recursive | -r | Copy directories recursively |
| force | -f | Overwrite existing files |
| no_clobber | -n | Don't overwrite existing |
| update | -u | Only copy if source is newer |
| preserve_mode | -p | Preserve file permissions |
| preserve_timestamps | | Preserve modification times |
| preserve_ownership | | Preserve owner/group (needs root) |
| preserve_links | | Copy symlinks as links |
| dereference | -L | Follow symlinks |

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
./test_acp
```

## License

Copyright (c) 2025 Aria Language Project
