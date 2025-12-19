# Aria LLDB Data Formatters

## Overview

Custom LLDB formatters for Aria's unique type system, enabling proper debugging experience with:
- **TBB (Twisted Balanced Binary) Types**: Special display for ERR sentinels and overflow detection
- **GC Pointers**: Synthetic children exposing object header metadata
- **Result<T>**: Pretty printing of Ok/Error states

## Phase 7.4.2: TBB Type Formatters

Reference: `docs/gemini/responses/request_036_debugger.txt` (Section 4.1)

### Features

#### TBB Type Summary Provider
Displays TBB values with semantic meaning:
- **ERR Sentinel**: Shows "ERR" instead of raw sentinel value
  - `tbb8`: -128 → "ERR"
  - `tbb16`: -32768 → "ERR"
  - `tbb32`: -2147483648 → "ERR"
  - `tbb64`: INT64_MIN → "ERR"
- **Overflow Detection**: Values outside symmetric range show "(OVERFLOW)"
  - `tbb8` valid range: [-127, +127]
  - `tbb16` valid range: [-32767, +32767]
  - `tbb32` valid range: [-2147483647, +2147483647]
  - `tbb64` valid range: [INT64_MIN + 1, INT64_MAX]
- **Normal Values**: Standard decimal display within range

#### GC Pointer Synthetic Provider
Exposes object header fields as synthetic children:
```
(gc_ptr<MyStruct>) ptr = 0x7fff1234 {
  value = { ... }           # Dereferenced object
  mark_bit = 1              # GC mark status
  pinned_bit = 0            # Pin status
  forwarded_bit = 0         # Forwarding status
  is_nursery = 1            # Young generation
  size_class = 5            # Allocation size bucket
  type_id = 0x42            # Runtime type ID
}
```

#### Result<T> Summary Provider
Pretty prints Result<T> values:
- `Result<i32> r = 42` → "Ok(42)"
- `Result<i32> r = Error(5)` → "Error(5)"

## Building with LLDB Support

### Requirements
- LLVM 20.1+ (includes LLDB API headers)
- liblldb.so (LLDB shared library)

### Installation

**Ubuntu/Debian:**
```bash
sudo apt-get install lldb-20 liblldb-20-dev
```

**Arch Linux:**
```bash
sudo pacman -S lldb
```

**macOS:**
```bash
brew install llvm@20
```

### Build
```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

CMake will automatically detect LLDB:
- ✅ LLDB found: Formatters will be built
- ❌ LLDB not found: Formatters skipped (core functionality unaffected)

## Usage

### Automatic Loading (Recommended)

Add to `~/.lldbinit`:
```python
# Load Aria formatters
command script import /path/to/aria/tools/debugger/lldb_init.py
```

### Manual Registration

```cpp
#include "tools/debugger/aria_formatters.h"

lldb::SBDebugger debugger = lldb::SBDebugger::Create();
aria::debugger::RegisterAriaFormatters(debugger);
```

### Command Line

```bash
$ lldb ./build/my_aria_program
(lldb) command script import tools/debugger/lldb_init.py
(lldb) type category enable aria
(lldb) breakpoint set --name main
(lldb) run
```

## Examples

### TBB Variables
```aria
let x: tbb8 = ERR;          // Displays: "ERR"
let y: tbb8 = 127;          // Displays: "127"
let z: tbb8 = 200;          // Displays: "200 (OVERFLOW)"
```

### GC Pointers
```aria
let ptr: gc_ptr<MyStruct> = allocate<MyStruct>();
// In LLDB:
(lldb) p ptr
(gc_ptr<MyStruct>) ptr = 0x7fff1234 {
  value = { field1 = 42, field2 = "hello" }
  mark_bit = 0
  pinned_bit = 1
  type_id = 0x1A
}
```

### Result Types
```aria
let result: result<i32> = do_something();
// In LLDB:
(lldb) p result
(result<i32>) result = Ok(42)

let error: result<i32> = fail();
(lldb) p error
(result<i32>) error = Error(5)
```

## Architecture

### File Structure
```
include/tools/debugger/
  aria_formatters.h         # Public API and type declarations

src/tools/debugger/
  aria_formatters.cpp       # Formatter implementations

tests/unit/
  test_tbb_formatters.cpp   # Unit tests
```

### Type Mapping Strategy

DWARF typedefs enable formatter recognition:
```c
// Phase 7.4.1 emits:
typedef int8_t tbb8;

// Phase 7.4.2 registers formatter:
category.AddTypeSummary("tbb8", TBBTypeSummaryProvider::GetSummary);
```

This allows LLDB to match type names exactly, enabling semantic display of TBB values.

## Testing

### Unit Tests
```bash
./build/tests/test_runner 2>&1 | grep tbb_formatter
```

Tests verify:
- ✅ ERR sentinel values correct for all bit widths
- ✅ Symmetric range bounds correct
- ✅ Bit field extraction for GC headers
- ✅ Formatter registration (when LLDB available)

### End-to-End Testing
Full debugger testing requires:
1. Compile Aria program with debug info: `ariac -g program.aria`
2. Launch in LLDB: `lldb ./program`
3. Set breakpoint: `(lldb) b main`
4. Inspect TBB variables and verify formatters work

## Future Work (Phase 7.4.3-7.4.6)

- **Phase 7.4.3**: DAP (Debug Adapter Protocol) server
- **Phase 7.4.4**: Web UI with memory map visualization
- **Phase 7.4.5**: Async/await coroutine inspection
- **Phase 7.4.6**: Interactive memory map viewer

## References

- `docs/gemini/responses/request_036_debugger.txt` - Complete debugger specification
- `research_002` - GC object header layout
- `research_018` - Error handling with Result<T>
- LLDB C++ API: https://lldb.llvm.org/cpp_reference/

## License

Part of the Aria compiler project.
