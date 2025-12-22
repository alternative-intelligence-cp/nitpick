# Compiler Integration Requirements from Gemini Audit
**Source**: gemini_gap_todo3.txt (AriaBuild Architectural Audit)  
**Date**: December 19, 2025  
**Current Version**: v0.0.9

## Overview
Gemini's comprehensive audit of the AriaBuild system identified several compiler-side requirements needed for proper build system integration. While most gaps are build-system specific, the following features must be implemented in the Aria compiler (ariac) to enable production-ready toolchain functionality.

---

## High Priority Compiler Features

### 1. Dependency Generation Flags
**Status**: NOT IMPLEMENTED  
**Priority**: HIGH  
**Tracking**: Future Session (10 or 11)

**Required Flags**:
```bash
ariac -M src/main.aria                    # Generate dependencies to stdout
ariac -M -MF build/main.d src/main.aria  # Write dependencies to file
ariac --emit-deps=<file> src/main.aria   # Alternative syntax
```

**Output Format** (GCC-compatible .d file):
```makefile
build/main.o: src/main.aria src/utils.aria assets/model.json
```

**Implementation Points**:
- Add `-M` and `-MF` flags to compiler driver (src/main.cpp)
- Create `DependencyTracker` singleton class
- Hook into Lexer to capture `use` statements
- Hook into `comptime` execution to track file reads (embed_file, etc.)
- Hook into `cfg` conditional compilation to track platform-specific imports
- Emit dependency manifest after successful compilation

**Why Critical**:
- Enables proper incremental builds
- Detects when comptime-embedded resources change
- Handles conditional compilation dependencies correctly
- Prevents stale artifact bugs

**Example Use Case**:
```aria
// main.aria
use utils;

const config = comptime {
    return embed_file("config.json");  // Dynamic dependency!
};
```
Dependencies: `main.aria`, `utils.aria`, `config.json`

---

### 2. Native Object File Emission
**Status**: NOT IMPLEMENTED  
**Priority**: HIGH  
**Tracking**: Future Session (10 or 11)

**Current State**: Compiler only emits LLVM IR (.ll files), executed via `lli` interpreter

**Required Flags**:
```bash
ariac -c src/main.aria -o build/main.o          # Compile to object file
ariac --emit-obj src/main.aria -o build/main.o  # Explicit flag
ariac --emit=obj src/main.aria                  # Unified emission control
ariac --emit=llvm-ir src/main.aria              # Explicit IR (current behavior)
ariac --emit=llvm-bc src/main.aria              # LLVM bitcode
ariac --emit=asm src/main.aria                  # Assembly output
```

**Implementation Points**:
- Already have LLVM infrastructure in place
- Use `TargetMachine::emit()` pipeline to generate native code
- Support ELF (Linux), PE (Windows), Mach-O (macOS) formats
- Default to object files for production, IR for development/debugging

**Why Critical**:
- Required for systems language classification
- Enables standalone executable creation
- Proper C FFI integration (extern "libc")
- Native performance (no JIT overhead)
- Supports `wild` memory management properly

**Current Limitation**:
```bash
# Current workflow (interpreted):
ariac src/main.aria --emit-llvm -o build/main.ll
lli build/main.ll  # JIT execution, not distributable

# Needed workflow (native):
ariac -c src/main.aria -o build/main.o
ariac -c src/utils.aria -o build/utils.o
ld build/main.o build/utils.o -o myapp  # Standalone executable
./myapp  # Native execution
```

---

## Medium Priority Compiler Features

### 3. Linker Driver Integration
**Status**: NOT IMPLEMENTED  
**Priority**: MEDIUM  
**Tracking**: Future Session (after object emission)

**Concept**: Compiler driver should support full compilation pipeline, including linking.

**Proposed Flags**:
```bash
ariac src/main.aria src/utils.aria -o myapp  # Compile + Link (like gcc/clang)
ariac -c src/*.aria                           # Compile only, no linking
```

**Implementation Points**:
- Detect system linker: `lld` (preferred), `ld`, `link.exe`
- Construct linker command line with proper flags
- Handle library paths (`-L`) and libraries (`-l`)
- Link standard library automatically
- Support `-nostdlib` for bare metal

**Note**: This could be a separate `arialink` tool initially, then integrated into the main driver.

---

### 4. Dynamic Dependency Tracking (comptime)
**Status**: PARTIALLY IMPLEMENTED  
**Priority**: MEDIUM  
**Tracking**: Requires Session on comptime evaluation

**Challenge**: `comptime` blocks execute arbitrary code at compile time, which can:
- Read files via `embed_file()`
- Import modules dynamically
- Generate code based on external data

**Implementation Strategy**:
- Instrument file I/O in comptime runtime
- Track all opened files during compilation
- Emit these as dependencies in `-M` output
- Update build system dependency graph dynamically

**Example**:
```aria
const assets = comptime {
    // These files become dependencies:
    let models = embed_file("models/player.json");
    let textures = embed_file("textures/sprites.png");
    return process_assets(models, textures);
};
```

Build system must rebuild if any embedded file changes.

---

## Build System Gaps (Not Compiler Issues)

The following are **build system responsibilities**, documented here for completeness:

1. **Command Signature Hashing**: Track when compiler flags change
2. **Native Process Spawning**: Replace popen with proper CreateProcessW/fork+execve
3. **Cycle Detection**: Tri-color DFS for dependency graph validation
4. **Globbing Exclusion**: Prevent infinite recursion into build/ directories
5. **compile_commands.json**: LSP support (build tool generates this)
6. **Environment Variable Scoping**: Thread-safe per-target environments

These are addressed in the aria_make project, not the compiler.

---

## Implementation Roadmap

### Phase 1: Dependency Generation (Session 10/11)
- [ ] Add `-M` and `-MF` flags to compiler driver
- [ ] Implement DependencyTracker class
- [ ] Hook Lexer for `use` statements
- [ ] Hook Parser for `cfg` conditionals
- [ ] Emit .d file format
- [ ] Test with aria_make integration

### Phase 2: Object Emission (Session 10/11)
- [ ] Add `--emit-obj` and `-c` flags
- [ ] Implement TargetMachine pipeline
- [ ] Support ELF/PE/Mach-O output
- [ ] Add `-S` for assembly output
- [ ] Test native compilation end-to-end

### Phase 3: Linker Integration (Session 12/13)
- [ ] Detect system linker
- [ ] Construct linker command lines
- [ ] Handle library paths and dependencies
- [ ] Support `-nostdlib` for bare metal
- [ ] Test full compilation pipeline

### Phase 4: Dynamic Dependencies (Future)
- [ ] Instrument comptime file I/O
- [ ] Track dynamic dependencies
- [ ] Emit in .d file format
- [ ] Integration testing with aria_make

---

## References
- Gemini Audit: `/home/randy/._____RANDY_____/REPOS/aria_make/docs/research/gemini/tasks/gemini_gap_todo3.txt`
- GCC Dependency Generation: `gcc -M -MF` documentation
- LLVM Object Emission: TargetMachine::addPassesToEmitFile()
- Ninja dyndep: Dynamic dependency discovery model

---

## Version Notes
- **v0.0.9**: Session 8 complete (Result Types)
- **v0.1.0**: Target for object emission + dependency tracking
- **v0.2.0**: Target for full linker integration

**Note**: These features do NOT block current compiler development sessions. They are integration requirements for the build system and can be implemented in dedicated sessions after core language features are complete.
