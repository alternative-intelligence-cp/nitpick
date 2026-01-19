# acat - Aria File Concatenation Library

A high-performance C++17 library for reading and concatenating files with
Hex-Stream topology support.

## Features

- **Zero-Copy I/O**: Uses sendfile() on Linux for maximum throughput
- **Hex-Stream Compatible**: Binary output to stddato (FD 5)
- **Line Processing**: Line numbering, blank squeezing, non-printing chars
- **TBB-Safe Arithmetic**: Overflow-safe byte counting
- **Streaming API**: Callback-based processing for large files
- **C FFI**: Call from Aria runtime or other languages

## Usage

### C++ API

```cpp
#include <acat/file_cat.hpp>

using namespace aria::acat;

// Simple cat to stdout
CatResult result = cat_file("/path/to/file.txt");

// Cat with options
CatOptions opts;
opts.show_line_numbers = true;
opts.show_ends = true;
result = cat_file("/path/to/file.txt", FD_STDOUT, opts);

// Cat multiple files
std::vector<std::string> files = {"file1.txt", "file2.txt"};
result = cat_files(files, FD_STDOUT);

// Read file to string
auto content = read_file_string("/path/to/file.txt");
if (content) {
    std::cout << *content << std::endl;
}

// Streaming with callback
cat_file_callback("/path/to/large.bin", [](const uint8_t* data, size_t size) {
    // Process chunk
    return true;  // Continue
});

// Line-by-line processing
cat_file_lines("/path/to/file.txt", [](const std::string& line, int64_t num) {
    std::cout << num << ": " << line << std::endl;
    return true;
});
```

### C FFI

```c
#include <acat/file_cat.hpp>

// Simple cat
AriaError err = aria_cat_file("/path/to/file.txt");

// Read file to buffer
uint8_t* data;
size_t size;
err = aria_read_file("/path/to/file.txt", &data, &size);
// ... use data ...
aria_cat_free(data);
```

## Options

| Option | Flag | Description |
|--------|------|-------------|
| show_line_numbers | -n | Number all output lines |
| show_nonblank_numbers | -b | Number non-blank lines only |
| show_ends | -E | Display $ at end of lines |
| show_tabs | -T | Display TAB as ^I |
| show_nonprinting | -v | Display non-printing characters |
| squeeze_blank | -s | Squeeze multiple blank lines |
| binary_mode | | Output to stddato (FD 5) |
| use_zero_copy | | Use sendfile() when possible |

## Hex-Stream Topology

| FD | Name | Usage |
|----|------|-------|
| 0 | stdin | Input (with "-" path) |
| 1 | stdout | Text output |
| 3 | stddbg | Telemetry (NDJSON) |
| 5 | stddato | Binary output |

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
./test_acat
```

## License

Copyright (c) 2025 Aria Language Project
