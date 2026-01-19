# Glob Pattern Cookbook
**For aria::glob::GlobEngine**

A practical guide to glob patterns for file discovery in build systems.

---

## Basic Patterns

### Match All Files with Extension

```cpp
// Match all .aria files in directory
engine.match("/project/src", "*.aria");
// Results: main.aria, utils.aria, test.aria
```

### Match All Files Recursively

```cpp
// Match all .aria files in directory tree
engine.match("/project", "**/*.aria");
// Results: src/main.aria, src/lib/utils.aria, tests/test.aria
```

### Match Single Character

```cpp
// Match file1.txt, file2.txt, filea.txt
engine.match("/data", "file?.txt");
```

---

## Character Classes

### Match Specific Characters

```cpp
// Match files starting with a, b, or c
engine.match("/src", "[abc]*.cpp");
// Results: allocator.cpp, buffer.cpp, cache.cpp
```

### Match Character Ranges

```cpp
// Match files starting with any digit
engine.match("/logs", "[0-9]*.log");
// Results: 2024.log, 01.log, 9_debug.log

// Match files starting with lowercase letter
engine.match("/src", "[a-z]*.cpp");
```

### Negated Character Classes

```cpp
// Match files NOT starting with test_
engine.match("/src", "[!t]*.cpp");
// Excludes: test_main.cpp, test_utils.cpp
// Includes: main.cpp, utils.cpp, allocator.cpp
```

### Combined Ranges

```cpp
// Match alphanumeric starting files
engine.match("/src", "[a-zA-Z0-9]*.cpp");
```

---

## Recursive Patterns

### Deep Directory Search

```cpp
// Find all .aria files anywhere in tree
engine.match("/project", "**/*.aria");

// Find test files in any tests/ directory
engine.match("/project", "**/tests/*.aria");

// Find files in any src/ directory at any depth
engine.match("/project", "**/src/**/*.cpp");
```

### Limited Depth

```cpp
GlobOptions opts;
opts.max_depth = 2;  // Only search 2 levels deep
GlobEngine engine(opts);

engine.match("/project", "**/*.aria");
// Only searches /project and /project/*/
```

---

## Build System Patterns

### Source File Discovery

```cpp
// All Aria source files
std::vector<std::string> patterns = {
    "src/**/*.aria",
    "lib/**/*.aria"
};
auto [sources, err] = engine.match_all("/project", patterns);
```

### Header Files

```cpp
// All C++ headers
engine.match("/include", "**/*.{h,hpp}");  // Not supported yet

// Workaround - use multiple patterns:
std::vector<std::string> headers = {"**/*.h", "**/*.hpp"};
auto [files, err] = engine.match_all("/include", headers);
```

### Test Files

```cpp
// All test files (by naming convention)
engine.match("/project", "**/test_*.aria");
engine.match("/project", "**/*_test.aria");
```

### Exclude Build Artifacts

```cpp
GlobOptions opts;
opts.files_only = true;  // Skip directories

GlobEngine engine(opts);
engine.match("/project", "**/*.aria");
// Won't match build/, bin/, obj/ directories
```

---

## Advanced Patterns

### Multiple Extensions

```cpp
std::vector<std::string> patterns = {
    "**/*.cpp",
    "**/*.c",
    "**/*.cc"
};
auto [sources, err] = engine.match_all("/src", patterns);
```

### Specific Subdirectories

```cpp
// Only files in core/ subdirectories
engine.match("/project", "**/core/*.aria");

// Only files directly in src/, not nested
engine.match("/project", "src/*.aria");  // No **
```

### Hidden Files

```cpp
// By default, hidden files are excluded
GlobOptions opts;
opts.include_hidden = true;

GlobEngine engine(opts);
engine.match("/project", "**/*");  // Includes .gitignore, .config, etc.
```

---

## Common Use Cases

### 1. Compile All Sources

```cpp
GlobEngine engine;

// Get all source files
auto [sources, err] = engine.match("/project/src", "**/*.aria");

if (err == GlobError::OK) {
    for (const auto& source : sources) {
        compile(source);  // Your compilation function
    }
}
```

### 2. Build System Source Discovery

```cpp
// AriaBuild-style source collection
std::vector<std::string> source_patterns = {
    "src/**/*.aria",       // Main sources
    "vendor/**/*.aria"     // Third-party
};

std::vector<std::string> exclude_patterns = {
    "**/tests/**",         // Skip tests
    "**/examples/**"       // Skip examples
};

auto [all_sources, err] = engine.match_all("/project", source_patterns);

// Filter excludes manually (exclude patterns not yet supported)
std::vector<std::string> sources;
for (const auto& src : all_sources) {
    bool excluded = false;
    for (const auto& excl : exclude_patterns) {
        if (engine.path_matches(src, excl)) {
            excluded = true;
            break;
        }
    }
    if (!excluded) {
        sources.push_back(src);
    }
}
```

### 3. Find Dependencies

```cpp
// Find all module files that might be dependencies
auto [modules, err] = engine.match("/project", "**/*.aria");

// Parse each to extract 'use' statements
for (const auto& module : modules) {
    extract_dependencies(module);
}
```

### 4. Clean Build Artifacts

```cpp
std::vector<std::string> artifact_patterns = {
    "**/*.o",
    "**/*.obj",
    "**/*.a",
    "**/*.so",
    "**/build/**"
};

auto [artifacts, err] = engine.match_all("/project", artifact_patterns);

for (const auto& artifact : artifacts) {
    std::filesystem::remove(artifact);
}
```

### 5. Documentation Generation

```cpp
// Find all Aria files with doc comments
auto [sources, err] = engine.match("/project/src", "**/*.aria");

for (const auto& source : sources) {
    extract_documentation(source);
}
```

---

## Path Matching (No Filesystem Access)

Useful for filtering, validation, or testing:

```cpp
GlobEngine engine;

// Test if path matches pattern (no disk I/O)
bool matches = engine.path_matches("src/core/main.aria", "src/**/*.aria");
// Returns: true

// Batch testing
std::vector<std::string> paths = load_paths_from_cache();
std::vector<std::string> filtered;

for (const auto& path : paths) {
    if (engine.path_matches(path, "src/**/*.aria")) {
        filtered.push_back(path);
    }
}
```

---

## Options Reference

```cpp
GlobOptions opts;

// Case sensitivity (default: true on Linux, false on Windows)
opts.case_sensitive = true;

// Follow symlinks (default: false for safety)
opts.follow_symlinks = false;

// Maximum recursion depth (default: 64)
opts.max_depth = 32;

// Only return files, not directories (default: false)
opts.files_only = true;

// Include hidden files (default: false)
opts.include_hidden = false;

// Skip permission errors and continue (default: true)
opts.skip_permission_errors = true;

GlobEngine engine(opts);
```

---

## Error Handling

```cpp
auto [paths, err] = engine.match("/project", "**/*.aria");

switch (err) {
    case GlobError::OK:
        process_files(paths);
        break;
        
    case GlobError::INVALID_BASE_DIR:
        std::cerr << "Directory does not exist\n";
        break;
        
    case GlobError::PATTERN_SYNTAX_ERROR:
        std::cerr << "Invalid glob pattern\n";
        break;
        
    case GlobError::ACCESS_DENIED:
        std::cerr << "Permission denied\n";
        break;
        
    case GlobError::SYMLINK_CYCLE:
        std::cerr << "Circular symlink detected\n";
        break;
        
    default:
        std::cerr << "Error: " << glob_error_string(err) << "\n";
        break;
}
```

### Pattern Validation

```cpp
// Validate before using
GlobError err = engine.validate_pattern("src/**/*.aria");

if (err != GlobError::OK) {
    std::cerr << "Invalid pattern: " << glob_error_string(err) << "\n";
    return 1;
}
```

---

## Performance Tips

### 1. Use Anchoring

```cpp
// GOOD - Anchored pattern (starts at src/)
engine.match("/project", "src/**/*.aria");
// Skips other top-level directories

// LESS EFFICIENT - Unanchored
engine.match("/project", "**/*.aria");
// Must search entire tree
```

### 2. Batch Pattern Matching

```cpp
// GOOD - Single call with multiple patterns
std::vector<std::string> patterns = {"*.cpp", "*.h", "*.hpp"};
auto [files, err] = engine.match_all("/src", patterns);

// LESS EFFICIENT - Multiple calls
auto cpp = engine.match("/src", "*.cpp");
auto h = engine.match("/src", "*.h");
auto hpp = engine.match("/src", "*.hpp");
```

### 3. Limit Depth When Possible

```cpp
GlobOptions opts;
opts.max_depth = 3;  // If you know max nesting

GlobEngine engine(opts);
engine.match("/project", "**/*.aria");  // Stops at depth 3
```

### 4. Use Specific Patterns

```cpp
// GOOD - Specific
engine.match("/project", "src/core/*.cpp");

// LESS EFFICIENT - Overly broad
engine.match("/project", "**/*.cpp");  // Searches everything
```

---

## Pattern Syntax Summary

| Pattern | Matches | Example |
|---------|---------|---------|
| `*` | Any characters in single directory | `*.aria` → `main.aria` |
| `**` | Any directories (recursive) | `**/*.aria` → `src/main.aria` |
| `?` | Exactly one character | `file?.txt` → `file1.txt` |
| `[abc]` | One of: a, b, or c | `[abc].txt` → `a.txt` |
| `[a-z]` | Character range | `[0-9].log` → `5.log` |
| `[!x]` | Any character except x | `[!0-9]*` → `test.txt` |

---

## Integration with aria_make

Example build file usage:

```
sources = {{glob("src/**/*.aria")}}
headers = {{glob("include/**/*.h")}}
tests = {{glob("tests/test_*.aria")}}

target = {
    name: "myapp",
    sources: sources,
    deps: {{scan_deps(sources)}}
}
```

---

## Deterministic Output

All glob results are sorted in canonical order for reproducible builds:

```cpp
// Run 1
auto [paths1, _] = engine.match("/src", "**/*.cpp");

// Run 2 (on different day, different system)
auto [paths2, _] = engine.match("/src", "**/*.cpp");

// Guaranteed: paths1 == paths2 (identical order)
```

This is critical for:
- Reproducible builds
- Build system caching
- Version control diffs
- CI/CD consistency

---

**See also**: `aglob_demo.cpp` for working examples of all patterns.
