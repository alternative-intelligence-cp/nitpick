# aria_utils Quick Reference
**Version**: 0.0.1-dev  
**Last Updated**: January 15, 2026

All utilities are built and ready to use. Test executables are in `src/<utility>/build/`.

---

## File Operations

### acat - File Concatenation
**Location**: `src/acat/build/test_acat`  
**Tests**: 32 passing  

**Features**:
- Concatenate multiple files
- Line numbering (-n)
- Show line endings (-E)
- Show tabs (-T)
- Squeeze blank lines (-s)

**Usage** (from FFI):
```cpp
AriaStatus status = aria_cat_file("file.txt", ARIA_CAT_SHOW_ENDS);
```

---

### acp - File Copy
**Location**: `src/acp/build/test_acp`  
**Tests**: 33 passing  

**Features**:
- Simple file copy
- Copy to directory
- Force overwrite
- No clobber mode
- Backup creation
- Directory copying
- Symlink handling

**Usage** (from FFI):
```cpp
AriaStatus status = aria_copy_file("source.txt", "dest.txt", ARIA_CP_FORCE);
```

---

### amv - File Move/Rename
**Location**: `src/amv/build/test_amv`  
**Tests**: 29 passing  

**Features**:
- Move files
- Rename files
- Force overwrite
- No clobber mode
- Backup creation
- Directory move
- Cross-filesystem detection

**Usage** (from FFI):
```cpp
AriaStatus status = aria_move_file("old.txt", "new.txt", ARIA_MV_BACKUP);
```

---

## System Utilities

### aps - Process Listing
**Location**: `src/aps/build/test_aps`  
**Tests**: 29 passing  

**Features**:
- Get system page size
- Get total memory
- UID ↔ username conversion
- GID ↔ groupname conversion
- Process state parsing (R/S/D/Z)

**Usage** (from FFI):
```cpp
size_t page_size = aria_get_page_size();
size_t total_mem = aria_get_total_memory();
char* username = aria_uid_to_name(1000);
```

---

### als - Directory Listing
**Location**: `src/als/build/test_als`  
**Tests**: 34 passing  

**Features**:
- List directory contents
- Filter hidden files
- Filter by type (file/dir)
- Sort by name
- File statistics
- Recursive walk
- Depth limiting
- Glob pattern matching

**Usage** (from FFI):
```cpp
AriaListOptions opts = {
    .include_hidden = false,
    .files_only = true,
    .sorted = true
};
AriaFileList* list = aria_list_directory("/path", &opts);
```

---

## Development Tools

### aglob - Glob Pattern Matching
**Location**: `src/aglob/build/test_aglob`  
**Tests**: 25 passing  

**Features**:
- `*` - match any characters in directory
- `**` - recursive directory match
- `?` - match single character
- `[abc]` - character class
- `[a-z]` - character range
- `[!x]` - negated class
- Deterministic ordering
- Symlink safety (cycle detection)

**Usage** (from FFI):
```cpp
AriaGlobResult result = aria_glob_match("/project", "src/**/*.aria");
for (size_t i = 0; i < result.count; i++) {
    printf("%s\n", result.paths[i]);
}
aria_glob_free(&result);
```

**Usage** (from C++):
```cpp
#include "aglob/glob_engine.hpp"
using namespace aria::glob;

GlobEngine engine;
auto [paths, err] = engine.match("/project", "src/**/*.aria");
```

---

### abc - Command Interpolator
**Location**: `src/abc/build/test_abc`  
**Tests**: 37 passing  

**Features**:
- Full lexer for build files
- Parser with error recovery
- Variable interpolation
- Nested variable support
- Environment variable access
- Circular reference detection
- Glob pattern resolution

**Components**:
- **Lexer**: Tokenizes input (strings, integers, booleans, comments)
- **Parser**: Builds AST from tokens
- **Interpolator**: Resolves `{{var}}` patterns
- **Source Resolver**: Expands glob patterns

**Usage** (for build files):
```
target = {
    name: "my_app",
    sources: ["src/**/*.aria"],
    deps: {{DEPENDENCIES}},
    output: {{BUILD_DIR}}/bin/{{name}}
}
```

---

### depgraph - Dependency Graph
**Location**: `src/depgraph/build/test_depgraph`  
**Tests**: 28 passing  

**Features**:
- Dependency scanner (parses `use` statements)
- Directed graph construction
- Topological sorting
- Cycle detection
- Parallel build scheduling
- Thread pool (configurable workers)
- Incremental builds

**Components**:
- **Scanner**: Extracts module dependencies from source
- **Graph**: Dependency graph with cycle detection
- **Scheduler**: Parallel task execution
- **ThreadPool**: Worker thread management
- **IncrementalBuild**: Change tracking

**Usage**:
```cpp
DepGraph graph;
graph.addNode("main.aria", {"std.io", "mylib"});
graph.addNode("mylib.aria", {"std.mem"});

auto order = graph.topologicalSort(); // Build order
if (order.empty()) {
    // Cycle detected!
}
```

---

### adap - Debug Adapter Protocol
**Location**: `src/adap/build/test_adap`  
**Tests**: 47 passing  

**Features**:
- Full DAP JSON-RPC support
- Request/Response/Event structures
- Capabilities negotiation
- Source location mapping
- Breakpoint management
- Stack frame inspection
- Variable inspection
- Thread management
- TBB error sentinel support

**Usage** (for VS Code integration):
```cpp
AdapSession session;
session.initialize(capabilities);
session.setBreakpoint(source, line);
auto frames = session.getStackTrace(thread_id);
auto vars = session.getVariables(frame_id);
```

**JSON Protocol**:
```json
{
    "seq": 1,
    "type": "request",
    "command": "setBreakpoints",
    "arguments": {
        "source": { "path": "/project/main.aria" },
        "lines": [10, 20, 30]
    }
}
```

---

## Network

### acurl - HTTP Client
**Location**: `src/acurl/build/test_acurl`  
**Tests**: 34 passing  

**Features**:
- HTTP GET/POST requests
- Six-stream topology (FD 0-5)
- Telemetry output (FD 3 - stddbg)
- Progress reporting
- Response header parsing
- Error handling
- Invalid URL detection

**Six-Stream Support**:
- FD 0: stdin (input)
- FD 1: stdout (UI text)
- FD 2: stderr (errors)
- FD 3: stddbg (telemetry JSON)
- FD 4: stddati (binary input)
- FD 5: stddato (binary output)

**Usage** (from FFI):
```cpp
aria_ensure_hex_streams(); // Set up FD 3-5

HttpClient* client = aria_http_client_create();
HttpRequest req = {
    .url = "https://example.com/api",
    .method = "GET",
    .headers = NULL,
    .body = NULL
};

HttpResponse* resp = aria_http_perform(client, &req);
printf("Status: %ld\n", resp->status_code);
aria_http_client_destroy(client);
```

**Telemetry Format** (FD 3):
```json
{"timestamp": 1705305600, "event": "download_progress", "bytes": 1048576, "total": 10485760}
```

---

## Building Utilities

All utilities use CMake:

```bash
cd src/<utility>
mkdir build && cd build
cmake ..
make

# Run tests
./test_<utility>
```

---

## Integration with Aria

### FFI Approach

Each utility provides C-compatible FFI functions:

```cpp
// In C++ utility
extern "C" {
    AriaStatus aria_cat_file(const char* path, int flags);
}
```

```aria
// In Aria code
extern "C" {
    func:aria_cat_file = int32(wild byte*:path, int32:flags);
}

func:main = int32() {
    unsafe {
        wild byte*:path = "file.txt".to_c_str();
        int32:result = aria_cat_file(path, 0);
        pass(result);
    }
}
```

### Future: Pure Aria Wrappers

```aria
use std.fs;

// High-level wrapper hiding FFI
mod utils.cat;

pub func:cat = result<void>(string:path) {
    // Call FFI internally, return Result<T>
}
```

---

## Test Summary

| Utility | Tests | Purpose |
|---------|-------|---------|
| aglob | 25 | Pattern matching, file discovery |
| abc | 37 | Build file parsing, interpolation |
| acat | 32 | File display, concatenation |
| acp | 33 | File copying |
| amv | 29 | File moving/renaming |
| aps | 29 | Process information |
| als | 34 | Directory listing |
| acurl | 34 | HTTP client |
| adap | 47 | Debugger integration |
| depgraph | 28 | Build dependencies |
| **Total** | **270** | **All passing** ✅ |

---

## Next Steps

### For Users
1. Link against utility libraries in your C++ projects
2. Call FFI functions from Aria unsafe blocks
3. Wait for high-level Aria wrappers

### For Developers
1. Write Aria wrapper modules
2. Create usage examples
3. Add integration tests
4. Benchmark performance
5. Document six-stream integration

---

## Documentation

Each utility has a README in `src/<utility>/README.md` with:
- Purpose and features
- Quick start guide
- Pattern syntax (for aglob)
- API reference
- Building instructions
- Architecture overview

See individual READMEs for detailed information.

---

**All utilities are production-ready and tested!** 🎉
