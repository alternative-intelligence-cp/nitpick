# Aria Ecosystem - Interface Specifications

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Purpose**: Define API contracts and interfaces between all ecosystem components

---

## Table of Contents

1. [Compiler ↔ Runtime Interface](#1-compiler--runtime-interface)
2. [Application ↔ Runtime Interface](#2-application--runtime-interface)
3. [Shell ↔ Runtime Interface](#3-shell--runtime-interface)
4. [Build System ↔ Compiler Interface](#4-build-system--compiler-interface)
5. [Package Manager ↔ System Interface](#5-package-manager--system-interface)
6. [LSP/DAP ↔ Compiler Interface](#6-lspdap--compiler-interface)
7. [Nikola ↔ Aria Interface (Future)](#7-nikola--aria-interface-future)

---

## 1. Compiler ↔ Runtime Interface

### 1.1 Linking Contract

**Responsibility**: Compiler generates LLVM IR that calls runtime functions; linker resolves symbols.

**Compiler Obligations**:
```cpp
// IR Generator emits calls to runtime functions
llvm::Function* aria_alloc_fn = get_runtime_function("aria_alloc");
builder.CreateCall(aria_alloc_fn, {size_value});
```

**Runtime Obligations**:
```c
// Provide C linkage for all runtime functions
#ifdef __cplusplus
extern "C" {
#endif

void* aria_alloc(size_t size);
void aria_free(void* ptr);
// ... etc

#ifdef __cplusplus
}
#endif
```

**Linking Process**:
```bash
# Object file contains undefined symbols
$ nm source.o | grep aria_alloc
         U aria_alloc

# Static runtime library provides definitions
$ nm libaria_runtime.a | grep aria_alloc
0000000000001a40 T aria_alloc

# Linker resolves symbols
$ clang source.o libaria_runtime.a -o executable
```

---

### 1.2 Builtin Intrinsics

**Memory Allocation Builtins**:

| Aria Code | IR Intrinsic | Runtime Function | Return Type |
|-----------|--------------|------------------|-------------|
| `arena:a = arena_create(8192)` | `call @arena_create` | `aria_arena* arena_create(size_t)` | Opaque int64 handle |
| `wild byte*:p = arena_alloc(a, 64)` | `call @arena_alloc` | `void* arena_alloc(arena, size)` | Pointer |
| `arena_reset(a)` | `call @arena_reset` | `void arena_reset(arena)` | void |
| `arena_destroy(a)` | `call @arena_destroy` | `void arena_destroy(arena)` | void |

**I/O Builtins**:

| Aria Code | IR Intrinsic | Runtime Function | Signature |
|-----------|--------------|------------------|-----------|
| `io.stdin.read_line()` | `call @aria_stdin_read_line` | `AriaResult* aria_stdin_read_line()` | Result<String> |
| `io.stdout.write(s)` | `call @aria_stdout_write` | `AriaResult* aria_stdout_write(char*, size_t)` | Result<void> |
| `io.stddbg.log(msg)` | `call @aria_stddbg_write` | `AriaResult* aria_stddbg_write(char*)` | Result<void> |

**TBB Arithmetic Helpers**:

| Operation | IR Intrinsic | Runtime Function | Purpose |
|-----------|--------------|------------------|---------|
| `tbb8:x + tbb8:y` | `call @tbb8_add` | `int8_t tbb8_add(int8_t, int8_t)` | Add with ERR propagation |
| `tbb8:x * tbb8:y` | `call @tbb8_mul` | `int8_t tbb8_mul(int8_t, int8_t)` | Multiply with overflow check |
| `x == ERR` | `icmp eq %x, -128` | (inline, no call) | Sentinel check |

---

### 1.3 Type ABI (Application Binary Interface)

**Primitive Type Mapping**:

| Aria Type | LLVM IR Type | C ABI Type | Size | Alignment |
|-----------|--------------|------------|------|-----------|
| `int8` | `i8` | `int8_t` | 1 byte | 1 byte |
| `int64` | `i64` | `int64_t` | 8 bytes | 8 bytes |
| `int128` | `i128` | `__int128` | 16 bytes | 16 bytes |
| `tbb8` | `i8` | `int8_t` | 1 byte | 1 byte |
| `float32` | `float` | `float` | 4 bytes | 4 bytes |
| `bool` | `i1` | `bool` | 1 byte | 1 byte |
| `char` | `i8` | `char` | 1 byte | 1 byte |

**Composite Type Mapping**:

| Aria Type | LLVM IR Type | C ABI Type |
|-----------|--------------|------------|
| `struct Point { x: int32, y: int32 }` | `{i32, i32}` | `struct { int32_t x; int32_t y; }` |
| `[int32; 10]` | `[10 x i32]` | `int32_t[10]` |
| `wild byte*` | `i8*` | `uint8_t*` |
| `result<int32>` | `{i8*, i32}` | `AriaResult*` (heap allocated) |

**Memory Qualifier Mapping**:

| Aria Qualifier | LLVM IR Metadata | Runtime Behavior |
|----------------|------------------|------------------|
| Stack (default) | `alloca` | Automatic lifetime, no runtime call |
| `wild` | Heap pointer | `aria_alloc()` / `aria_free()` calls |
| `wildx` | Heap pointer + `executable` attribute | JIT memory, restricted operations |

---

### 1.4 Calling Convention

**Standard C ABI**:
- Arguments passed in registers (x86-64: rdi, rsi, rdx, rcx, r8, r9)
- Return values in rax (integers) or xmm0 (floats)
- Caller-saved vs callee-saved register preservation
- Stack alignment (16-byte on x86-64)

**Result<T> Return Convention**:
```c
// Runtime allocates AriaResult on heap, returns pointer
AriaResult* aria_read_file(const char* path) {
    AriaResult* result = malloc(sizeof(AriaResult));
    // ... populate result
    return result;
}

// Caller must free after use
AriaResult* r = aria_read_file("data.txt");
if (r->err == NULL) {
    // Use r->val
}
aria_result_free(r);  // Frees both err and val
```

---

## 2. Application ↔ Runtime Interface

### 2.1 Core Runtime API

**Memory Management**:

```c
// Wild allocator (general-purpose heap)
void* aria_alloc(size_t size);
void aria_free(void* ptr);
void* aria_realloc(void* ptr, size_t new_size);

// Arena allocator (region-based, lifetime-scoped)
typedef struct aria_arena aria_arena;
AriaArenaResult aria_arena_new(size_t initial_capacity);
void* aria_arena_alloc(aria_arena* arena, size_t size);
void aria_arena_reset(aria_arena* arena);  // Reclaim all memory
void aria_arena_destroy(aria_arena* arena);

// Pool allocator (fixed-size blocks)
typedef struct aria_pool aria_pool;
AriaPoolResult aria_pool_create(size_t block_size, size_t block_count);
void* aria_pool_alloc(aria_pool* pool);
void aria_pool_free(aria_pool* pool, void* ptr);
void aria_pool_destroy(aria_pool* pool);

// Slab allocator (kernel-style, typed)
typedef struct aria_slab aria_slab;
AriaSlabResult aria_slab_create(size_t object_size);
void* aria_slab_alloc(aria_slab* slab);
void aria_slab_free(aria_slab* slab, void* ptr);
void aria_slab_destroy(aria_slab* slab);
```

**I/O Functions (6-Stream Model)**:

```c
// Control Plane (Text I/O)
AriaResult* aria_stdin_read_line(void);                    // Read line from stdin
AriaResult* aria_stdin_read(char* buffer, size_t size);    // Read bytes from stdin
AriaResult* aria_stdout_write(const char* str, size_t len); // Write to stdout
AriaResult* aria_stdout_write_line(const char* str);       // Write line to stdout
AriaResult* aria_stderr_write(const char* str, size_t len); // Write to stderr
AriaResult* aria_stderr_write_line(const char* str);       // Write line to stderr

// Observability Plane (Debug/Telemetry)
AriaResult* aria_stddbg_write(const char* log_entry);      // Write to debug stream
AriaResult* aria_stddbg_flush(void);                       // Flush async buffer

// Data Plane (Binary I/O)
AriaResult* aria_stddati_read(uint8_t* buffer, size_t size); // Read binary from stddati
AriaResult* aria_stddato_write(const uint8_t* buffer, size_t size); // Write binary to stddato

// File I/O
AriaResult* aria_file_open(const char* path, const char* mode); // Returns FileHandle*
AriaResult* aria_file_read(void* handle, uint8_t* buffer, size_t size);
AriaResult* aria_file_write(void* handle, const uint8_t* buffer, size_t size);
AriaResult* aria_file_close(void* handle);
```

**Process Management**:

```c
// Process spawning
AriaResult* aria_spawn(const char* command, const char** args, AriaSpawnOptions* opts);
// Returns AriaProcessInfo* containing pid and handle

// Process control
int aria_process_wait(AriaProcess* handle);               // Block until exit
bool aria_process_is_running(AriaProcess* handle);
AriaResult* aria_process_kill(AriaProcess* handle, int signal);

// Process info
int64_t aria_get_current_pid(void);
int64_t aria_get_parent_pid(void);
```

**Result<T> Utilities**:

```c
// Result construction
AriaResult* aria_result_ok(void* value, size_t size);
AriaResult* aria_result_err(const char* error);

// Result inspection
bool aria_result_is_ok(AriaResult* r);   // r->err == NULL
bool aria_result_is_err(AriaResult* r);  // r->err != NULL

// Result cleanup
void aria_result_free(AriaResult* r);    // Frees err, val, and result itself
```

---

### 2.2 Stream Handles

**Global Stream Initialization**:

```c
// Runtime initializes these during startup (before main)
// Maps to file descriptors 0-5 on POSIX, handles on Windows

AriaTextStream* aria_get_stdin(void);      // FD 0
AriaTextStream* aria_get_stdout(void);     // FD 1
AriaTextStream* aria_get_stderr(void);     // FD 2
AriaTextStream* aria_get_stddbg(void);     // FD 3
AriaBinaryStream* aria_get_stddati(void);  // FD 4
AriaBinaryStream* aria_get_stddato(void);  // FD 5
```

**File Descriptor Mapping**:

| Stream | FD (POSIX) | Handle (Windows) | Type | Purpose |
|--------|------------|------------------|------|---------|
| stdin | 0 | STD_INPUT_HANDLE | Text | User input |
| stdout | 1 | STD_OUTPUT_HANDLE | Text | Program output |
| stderr | 2 | STD_ERROR_HANDLE | Text | Error messages |
| stddbg | 3 | Custom handle | Text | Debug logs/telemetry |
| stddati | 4 | Custom handle | Binary | Data input channel |
| stddato | 5 | Custom handle | Binary | Data output channel |

---

### 2.3 Error Handling Contract

**Result<T> Pattern**:

```c
// Application code must check err before using val
AriaResult* r = aria_read_file("config.txt");

// REQUIRED: Check for error
if (r->err != NULL) {
    // Handle error
    fprintf(stderr, "Error: %s\n", r->err);
    aria_result_free(r);
    return 1;
}

// SAFE: Use value
char* content = (char*)r->val;
process(content);

// REQUIRED: Free result when done
aria_result_free(r);
```

**TBB Sticky Error Pattern**:

```c
// ERR sentinel propagates through arithmetic
tbb8 x = 120;
tbb8 y = 10;
tbb8 result = tbb8_add(x, y);  // Overflow! result = ERR (-128)

// Check for ERR before using
if (result == -128) {
    // Handle overflow
    return ARIA_ERROR_OVERFLOW;
}

// SAFE: Use result
process(result);
```

---

## 3. Shell ↔ Runtime Interface

### 3.1 Process Spawning with 6 Streams

**Shell Responsibility**:
```cpp
// aria_shell sets up 6 pipes before spawning
class ProcessController {
    void spawn(const char* command) {
        // Create 6 pipes
        int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
        int stddbg_pipe[2], stddati_pipe[2], stddato_pipe[2];
        
        pipe2(stdin_pipe, O_CLOEXEC);
        pipe2(stdout_pipe, O_CLOEXEC);
        pipe2(stderr_pipe, O_CLOEXEC);
        pipe2(stddbg_pipe, O_CLOEXEC);
        pipe2(stddati_pipe, O_CLOEXEC);
        pipe2(stddato_pipe, O_CLOEXEC);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process: map pipes to FDs 0-5
            dup2(stdin_pipe[0], 0);
            dup2(stdout_pipe[1], 1);
            dup2(stderr_pipe[1], 2);
            dup2(stddbg_pipe[1], 3);
            dup2(stddati_pipe[0], 4);
            dup2(stddato_pipe[1], 5);
            
            // Close unused ends
            close(stdin_pipe[1]);
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            close(stddbg_pipe[0]);
            close(stddati_pipe[1]);
            close(stddato_pipe[0]);
            
            execve(command, argv, envp);
        }
        
        // Parent: close child ends, start draining threads
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        close(stddbg_pipe[1]);
        close(stddati_pipe[0]);
        close(stddato_pipe[1]);
        
        start_drain_workers();
    }
};
```

**Runtime Reception**:
```c
// Runtime expects FDs 0-5 to be pre-configured
// No special initialization needed - standard POSIX FDs

// Application code just calls runtime functions
AriaResult* r = aria_stdout_write("Hello", 5);
// Runtime writes to FD 1 (which shell connected to pipe)
```

---

### 3.2 Stream Draining Protocol

**Problem**: Child writes to pipe → kernel buffer fills (64KB) → child blocks → deadlock

**Solution**: Shell runs drain workers that continuously read pipes

```cpp
class StreamDrainer {
    void drain_worker(int fd, StreamType type) {
        char buffer[CHUNK_SIZE];
        while (running) {
            ssize_t n = read(fd, buffer, sizeof(buffer));
            if (n > 0) {
                // Forward to appropriate handler
                if (type == STDOUT || type == STDERR) {
                    terminal.write(buffer, n);
                } else if (type == STDDBG) {
                    ring_buffer.push(buffer, n);  // Async, drop-on-full
                } else if (type == STDDATO) {
                    wild_buffer.append(buffer, n);  // Collect binary data
                }
            } else if (n == 0) {
                break;  // EOF
            }
        }
    }
};
```

---

### 3.3 Windows Bootstrap Protocol

**Problem**: Windows handles are not fixed indices (like FDs 0-5)

**Solution**: Pass handle mapping via environment variable

```cpp
// Shell side (parent process)
void spawn_windows(const char* command) {
    // Create 6 pipe handles
    HANDLE stdin_r, stdin_w;
    HANDLE stdout_r, stdout_w;
    HANDLE stderr_r, stderr_w;
    HANDLE stddbg_r, stddbg_w;
    HANDLE stddati_r, stddati_w;
    HANDLE stddato_r, stddato_w;
    
    CreatePipe(&stdin_r, &stdin_w, NULL, 0);
    CreatePipe(&stdout_r, &stdout_w, NULL, 0);
    CreatePipe(&stderr_r, &stderr_w, NULL, 0);
    CreatePipe(&stddbg_r, &stddbg_w, NULL, 0);
    CreatePipe(&stddati_r, &stddati_w, NULL, 0);
    CreatePipe(&stddato_r, &stddato_w, NULL, 0);
    
    // Create handle list for inheritance
    HANDLE inherit_handles[6] = {
        stdin_r, stdout_w, stderr_w,
        stddbg_w, stddati_r, stddato_w
    };
    
    STARTUPINFOEX si_ex = {};
    InitializeProcThreadAttributeList(...);
    UpdateProcThreadAttribute(
        si_ex.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inherit_handles,
        sizeof(inherit_handles),
        NULL, NULL
    );
    
    // Encode handle mapping in environment
    char fd_map[256];
    snprintf(fd_map, sizeof(fd_map),
        "__ARIA_FD_MAP=3:%lld;4:%lld;5:%lld",
        (int64_t)stddbg_w,
        (int64_t)stddati_r,
        (int64_t)stddato_w
    );
    
    // Add to environment block
    env_block = create_env_with(fd_map);
    
    CreateProcessW(..., &si_ex, ...);
}
```

**Runtime side (child process)**:
```c
// Static initializer runs before main()
__attribute__((constructor))
void aria_runtime_init() {
    // Parse __ARIA_FD_MAP environment variable
    const char* fd_map = getenv("__ARIA_FD_MAP");
    if (fd_map) {
        // Parse "3:1234;4:5678;5:9012"
        parse_and_assign_handles(fd_map);
    }
    
    // For FDs 0-2, use standard handles
    io.stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
    io.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    io.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    
    // Wrap Windows handles with C runtime FDs for compatibility
    io.stdin_fd = _open_osfhandle((intptr_t)io.stdin_handle, _O_RDONLY);
    io.stdout_fd = _open_osfhandle((intptr_t)io.stdout_handle, _O_WRONLY);
    io.stderr_fd = _open_osfhandle((intptr_t)io.stderr_handle, _O_WRONLY);
}
```

---

## 4. Build System ↔ Compiler Interface

### 4.1 Compilation Command Interface

**AriaBuild invokes compiler**:
```bash
# Basic compilation
ariac source.aria -o output.o

# With options
ariac source.aria \
    --output output.o \
    --emit-llvm-ir \
    --optimize=2 \
    --target=x86_64-linux-gnu
```

**Compiler Exit Codes**:

| Exit Code | Meaning | AriaBuild Action |
|-----------|---------|------------------|
| 0 | Success | Continue to next task |
| 1 | Syntax/semantic error | Abort build, show errors |
| 2 | I/O error (file not found) | Abort build, show error |
| 3 | Internal compiler error | Abort build, report bug |

---

### 4.2 Dependency Graph Communication

**ABC File (Build Config)**:
```json
{
  "project": {
    "name": "my_app",
    "version": "1.0.0"
  },
  "targets": {
    "executable": {
      "type": "binary",
      "output": "bin/my_app",
      "sources": ["src/main.aria", "src/utils.aria"],
      "dependencies": ["std.io", "std.collections"]
    }
  }
}
```

**AriaBuild Workflow**:
1. Parse ABC file
2. Resolve dependencies (query AriaX for missing packages)
3. Build dependency graph (topological sort)
4. For each source file (in parallel):
   - Check if object file is up-to-date (timestamp/hash)
   - If not, invoke `ariac source.aria -o source.o`
   - Collect errors/warnings
5. Link phase:
   - Collect all object files
   - Invoke `ariac link` or `clang` with libaria_runtime.a
   - Generate executable

---

### 4.3 Linking Interface

**Static Linking (default)**:
```bash
clang main.o utils.o /usr/local/lib/libaria_runtime.a -o my_app
```

**Dynamic Linking (future)**:
```bash
clang main.o utils.o -laria_runtime -L/usr/local/lib -o my_app
# Requires: export LD_LIBRARY_PATH=/usr/local/lib
```

**Automatic Runtime Injection**:
- AriaBuild automatically appends libaria_runtime.a to link command
- No need for explicit `-laria_runtime` in ABC config
- Build system detects runtime location:
  - Development: `build/libaria_runtime.a`
  - System: `/usr/local/lib/libaria_runtime.a`

---

## 5. Package Manager ↔ System Interface

### 5.1 Package Repository Protocol

**Package Index (JSON)**:
```json
{
  "packages": [
    {
      "name": "std.io",
      "version": "1.0.0",
      "description": "Standard I/O library",
      "dependencies": [],
      "tarball_url": "https://ariax.ai-liberation-platform.org/packages/std.io-1.0.0.tar.gz",
      "signature_url": "https://ariax.ai-liberation-platform.org/packages/std.io-1.0.0.tar.gz.sig",
      "checksum_sha256": "a1b2c3d4..."
    }
  ]
}
```

**Download & Verify**:
```bash
# AriaX workflow
ariax install std.io
  ↓
1. Fetch index.json
2. Resolve dependencies (recursive)
3. Download tarball
4. Verify signature (GPG)
5. Verify checksum (SHA-256)
6. Extract to /usr/local/aria/packages/std.io/
7. Update system ABC config
```

---

### 5.2 File System Layout

**Package Installation Structure**:
```
/usr/local/aria/
├── packages/
│   ├── std.io-1.0.0/
│   │   ├── src/                 # Source files
│   │   ├── lib/                 # Precompiled libraries
│   │   ├── include/             # Header files
│   │   └── package.json         # Metadata
│   └── std.collections-1.0.0/
│       └── ...
│
├── lib/
│   └── libaria_runtime.a        # System-wide runtime
│
└── bin/
    ├── ariac                    # Compiler
    ├── ariab                    # Build system
    └── ariax                    # Package manager
```

---

### 5.3 AriaX ↔ AriaBuild Integration

**Automatic Dependency Fetching**:
```bash
# User runs build
ariab build

# AriaBuild reads ABC, sees missing dependency
{
  "dependencies": ["std.collections"]
}

# AriaBuild queries AriaX
ariax query std.collections
  → Not installed

# AriaBuild triggers installation
ariax install std.collections --auto

# Installation updates ABC search paths
# Build continues with dependency available
```

---

## 6. LSP/DAP ↔ Compiler Interface

### 6.1 Language Server Protocol

**LSP Lifecycle**:
```
1. VSCode launches: aria-lsp
2. Initialize request (capabilities exchange)
3. VSCode notifies: textDocument/didOpen
4. aria-lsp parses file, runs diagnostics
5. aria-lsp sends: textDocument/publishDiagnostics
6. User types → textDocument/didChange
7. aria-lsp incrementally reparses
8. User requests completion → textDocument/completion
9. aria-lsp uses compiler's AST/symbol table
10. Returns completion items
```

**Shared Compiler Components**:
```cpp
// aria-lsp uses compiler's lexer/parser
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "semantic/symbol_table.h"

// Parse source file
Lexer lexer(source_code);
Parser parser(lexer.tokenize());
AST* ast = parser.parse();

// Run semantic analysis
SymbolTable symbols;
TypeChecker checker(symbols);
checker.check(ast);

// Extract diagnostics
for (auto& error : checker.errors()) {
    lsp_server.publish_diagnostic(error);
}
```

---

### 6.2 Debug Adapter Protocol

**DAP Lifecycle**:
```
1. VSCode launches: aria-dap
2. Initialize request
3. User sets breakpoint → setBreakpoints request
4. User starts debugging → launch request
5. aria-dap invokes LLDB with executable
6. Breakpoint hit → stopped event
7. VSCode requests stack trace → stackTrace request
8. aria-dap queries LLDB, formats response
9. User inspects variable → variables request
10. aria-dap uses custom formatters for TBB, Result<T>
```

**Custom Type Formatters**:
```cpp
// aria-dap implements custom formatters for Aria types
class AriaTypeFormatter {
    std::string format_tbb8(int8_t value) {
        if (value == -128) {
            return "ERR (overflow)";
        }
        return std::to_string(value);
    }
    
    std::string format_result(void* ptr) {
        AriaResult* r = (AriaResult*)ptr;
        if (r->err) {
            return "Err(\"" + std::string(r->err) + "\")";
        } else {
            return "Ok(" + format_value(r->val) + ")";
        }
    }
};
```

---

## 7. Nikola ↔ Aria Interface (Future)

### 7.1 FFI Binding Layer

**C++ Core (Nikola)**:
```cpp
// nikola/src/core/brain.hpp
class Brain {
    Manifold9D manifold;
    UFIE solver;
public:
    void inject_stimulus(const Complex* data, size_t len);
    Complex* read_output(size_t* len);
    void step(double dt);
};

// FFI wrapper (extern "C")
extern "C" {
    void* nikola_brain_create(size_t grid_size);
    void nikola_brain_inject(void* handle, const double* data, size_t len);
    double* nikola_brain_read(void* handle, size_t* len);
    void nikola_brain_step(void* handle, double dt);
    void nikola_brain_destroy(void* handle);
}
```

**Aria Wrapper Library**:
```aria
// aria_nikola/src/nikola.aria
mod nikola {
    // Foreign function declarations
    extern "C" func:nikola_brain_create = wild void*(uint64:grid_size);
    extern "C" func:nikola_brain_inject = void(wild void*:handle, wild float64*:data, uint64:len);
    extern "C" func:nikola_brain_read = wild float64*(wild void*:handle, wild uint64*:len);
    extern "C" func:nikola_brain_step = void(wild void*:handle, float64:dt);
    extern "C" func:nikola_brain_destroy = void(wild void*:handle);
    
    // Safe Aria wrapper
    pub struct Brain {
        handle: wild void*,
    }
    
    impl Brain {
        pub func:new = result<Brain>(uint64:grid_size) {
            wild void*:handle = nikola_brain_create(grid_size);
            if (handle == null) {
                fail("Failed to create Nikola brain");
            }
            pass(Brain { handle: handle });
        }
        
        pub func:inject = void(wild float64[]:data) {
            nikola_brain_inject(self.handle, data.ptr(), data.len());
        }
        
        pub func:read = wild float64[]() {
            wild uint64:len = 0;
            wild float64*:ptr = nikola_brain_read(self.handle, &len);
            return slice_from_raw_parts(ptr, len);
        }
    }
}
```

---

### 7.2 Data Exchange via 6-Stream I/O

**Training Pipeline**:
```aria
// Aria orchestrator sends training data via stddato
use nikola;
use io;

func:main = int64() {
    // Spawn Nikola process
    result<Process>:proc = spawn("nikola_train", ["--mode", "supervised"]);
    if (proc.is_err()) {
        fail("Failed to spawn Nikola");
    }
    
    // Send training examples via stddato (binary data channel)
    wild float64[]:training_data = load_dataset("mnist.bin");
    io.stddato.write(training_data);
    
    // Receive loss metrics via stddbg (observability channel)
    string:metrics = io.stddbg.read_line();
    print("Loss: " + metrics);
    
    // Control commands via stdin (control channel)
    io.stdout.write("CHECKPOINT\n");
    
    pass(0);
}
```

**Nikola Process**:
```cpp
// nikola_train.cpp
int main() {
    Brain brain = Brain::create(grid_size);
    
    // Read training data from stddati (FD 4)
    float64* data = read_from_stddati();
    brain.inject(data);
    
    // Train
    for (int epoch = 0; epoch < 100; epoch++) {
        brain.step(dt);
        
        // Send metrics to stddbg (FD 3)
        fprintf(aria_get_stddbg(), "epoch=%d loss=%.4f\n", epoch, loss);
        
        // Check for control commands on stdin (FD 0)
        char cmd[256];
        if (fgets(cmd, sizeof(cmd), stdin)) {
            if (strcmp(cmd, "CHECKPOINT\n") == 0) {
                brain.save_checkpoint();
            }
        }
    }
    
    // Send final output via stddato (FD 5)
    float64* output = brain.read_output();
    write_to_stddato(output, output_len);
    
    return 0;
}
```

---

## Interface Stability Guarantees

### Stable Interfaces (v1.0+)

- **Runtime C API**: All functions in `include/runtime/*.h` are stable
- **Linker ABI**: Type sizes, alignments, calling convention stable
- **Result<T> Structure**: `{err, val}` layout guaranteed

### Unstable Interfaces (Pre-v1.0)

- **Compiler IR Intrinsics**: May change as compiler evolves
- **LSP/DAP Protocol Details**: May add new capabilities
- **Nikola FFI**: Not yet finalized

### Versioning Policy

- **Major version** (1.0 → 2.0): Breaking ABI changes allowed
- **Minor version** (1.0 → 1.1): Additive changes only
- **Patch version** (1.0.0 → 1.0.1): Bug fixes, no interface changes

---

## Error Handling Across Interfaces

### Runtime → Application

- **Result<T>**: Heap-allocated, caller frees
- **TBB**: Sticky errors propagate through arithmetic
- **errno**: Not used (Result<T> preferred)

### Compiler → Build System

- **Exit codes**: 0=success, 1=compile error, 2=I/O error, 3=ICE
- **stderr**: Human-readable error messages
- **stdout**: JSON diagnostics (optional `--json-errors` flag)

### Shell → Process

- **Exit codes**: Process's exit code passed through
- **Signals**: SIGTERM, SIGKILL, SIGSTOP forwarded
- **Streams**: EOF signals process termination

---

**Document Status**: v1.0 - Comprehensive interface specifications  
**Next**: Create DATA_FLOW.md showing execution traces and data movement
