# aria_utils Examples

Interactive demonstrations of the C++ utilities with complete working code.

## ✅ Implemented Examples (3/10)

### 1. aglob - Glob Pattern Matching
**File**: `aglob_demo.cpp` | **Build**: `./build.sh aglob`

Demonstrates:
- Simple wildcards (`*.cpp`, `test_*`)
- Recursive patterns (`**/*.hpp`)
- Character classes (`[abc]`, `[0-9]`)
- Multiple pattern matching
- Path matching (without filesystem)
- Error handling and validation
- Deterministic ordering

See `GLOB_PATTERNS.md` for comprehensive pattern cookbook.

### 2. als - Directory Listing
**File**: `als_demo.cpp` | **Build**: `./build.sh als`

Demonstrates:
- Basic and recursive directory listing
- Filtering by type (files, dirs, symlinks)
- Filtering by glob patterns
- Sorting (size, time, name)
- TBB error propagation
- Directory statistics

### 3. aps - Process Status
**File**: `aps_demo.cpp` | **Build**: `./build.sh aps`

Demonstrates:
- List all running processes
- Filter by process name
- Sort by memory (RSS, VSize)
- Sort by CPU usage
- Detailed process info

## Building

```bash
# Build all examples
./build.sh all

# Build specific example
./build.sh aglob
./build.sh als
./build.sh aps

# Clean
./build.sh clean
```

## Prerequisites

Ensure utilities are built first:
```bash
cd ../src/aglob/build && cmake .. && make
cd ../src/als/build && cmake .. && make
cd ../src/aps/build && cmake .. && make
```

## Architecture Notes

### Six-Stream I/O
- **FD 1** (stdout): Human-readable output
- **FD 3** (stddbg): JSON telemetry
- **FD 5** (stddato): Binary data output

### TBB Error Propagation
Uses sticky error sentinels (`TBB64_ERR`) for safety-critical error handling.

## Coming Soon

- abc (build configuration parser)
- acat (file concatenation)
- acp/amv (file operations)
- acurl (HTTP with six-stream)
- depgraph (dependency analysis)
- adap (Debug Adapter Protocol)

---

**Status**: 3/10 examples complete (updated Jan 15, 2026, 7:15 AM)
