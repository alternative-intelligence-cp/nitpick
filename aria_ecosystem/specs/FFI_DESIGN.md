# Foreign Function Interface (FFI) Design - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design Phase (Implementation Pending Research)

---

## Table of Contents

1. [Overview](#overview)
2. [C Interop Basics](#c-interop-basics)
3. [Type Marshalling](#type-marshalling)
4. [Memory Safety](#memory-safety)
5. [Calling Conventions](#calling-conventions)
6. [Nikola Integration](#nikola-integration)
7. [Best Practices](#best-practices)

---

## Overview

Aria's FFI enables **safe interoperation with C** and **C++** libraries, critical for:

| Use Case | Example | Importance |
|----------|---------|------------|
| **System APIs** | POSIX, Win32 | OS interaction |
| **Native Libraries** | libcurl, OpenSSL | Reuse existing code |
| **Performance** | SIMD, GPU kernels | Optimize hot paths |
| **Nikola Integration** | Consciousness substrate | AI/consciousness research |

**Design Goals**:
1. **Safety**: Prevent undefined behavior at FFI boundary
2. **Ergonomics**: Easy to call C from Aria
3. **Performance**: Zero-cost abstractions
4. **Compatibility**: Work with existing C/C++ codebases

---

## C Interop Basics

### Declaring External Functions

**Syntax**: `extern func:name = return_type(params)`

**Example** (calling C stdlib):
```aria
// Declare C function
extern func:malloc = wild byte*(size: u64);
extern func:free = void(ptr: wild byte*);
extern func:strlen = u64(s: wild byte*);

// Use from Aria
func:allocate_buffer = wild byte*(size: u64) {
    wild byte*:buf = malloc(size);
    pass(buf);
}
```

**Generated Code**:
```c
// Aria compiler generates:
extern void* malloc(size_t size);
extern void free(void* ptr);
extern size_t strlen(const char* s);

// Aria function compiles to:
void* allocate_buffer(uint64_t size) {
    void* buf = malloc(size);
    return buf;
}
```

---

### Linking Against C Libraries

**Static Linking**:
```json
// build.abc
{
  "dependencies": {
    "ffi:libcurl": {
      "type": "c_library",
      "static": "/usr/lib/libcurl.a"
    }
  }
}
```

**Dynamic Linking**:
```json
{
  "dependencies": {
    "ffi:libcurl": {
      "type": "c_library",
      "shared": "libcurl.so"
    }
  }
}
```

**Compiler Invocation**:
```bash
ariac main.aria -l curl
```

---

### Header Translation

**Manual Approach** (explicit declarations):
```aria
// Manually declare C API
extern func:curl_easy_init = wild void*();
extern func:curl_easy_setopt = i32(handle: wild void*, option: i32, value: wild void*);
extern func:curl_easy_perform = i32(handle: wild void*);
extern func:curl_easy_cleanup = void(handle: wild void*);
```

**Automated Approach** (future: binding generator):
```bash
$ aria-bindgen /usr/include/curl/curl.h -o curl_bindings.aria
```

**Generated**:
```aria
// Auto-generated from curl.h
extern struct:CURL = opaque;  // Opaque C type
extern func:curl_easy_init = wild CURL*();
extern func:curl_easy_setopt = i32(handle: wild CURL*, option: CURLoption, ...);
// ...
```

---

## Type Marshalling

### Primitive Types

**Direct Mapping**:

| Aria Type | C Type | Size | ABI Compatible |
|-----------|--------|------|----------------|
| `i8` | `int8_t` | 1 byte | ✅ |
| `u8` | `uint8_t` | 1 byte | ✅ |
| `i16` | `int16_t` | 2 bytes | ✅ |
| `u16` | `uint16_t` | 2 bytes | ✅ |
| `i32` | `int32_t` | 4 bytes | ✅ |
| `u32` | `uint32_t` | 4 bytes | ✅ |
| `i64` | `int64_t` | 8 bytes | ✅ |
| `u64` | `uint64_t` | 8 bytes | ✅ |
| `f32` | `float` | 4 bytes | ✅ |
| `f64` | `double` | 8 bytes | ✅ |
| `bool` | `uint8_t` | 1 byte | ✅ (0/1) |
| `wild T*` | `T*` | 8 bytes | ✅ |

---

### Strings

**Problem**: Aria strings are UTF-8 with length, C strings are null-terminated

**Aria String**:
```c
struct aria_string {
    char* data;
    size_t len;
    size_t capacity;
};
```

**C String**:
```c
char* str;  // Null-terminated
```

---

**Conversion (Aria → C)**:
```aria
extern func:puts = i32(s: wild byte*);

func:print_to_c = void(s: string) {
    // Convert Aria string to C string (null-terminated)
    wild byte*:c_str = s.to_c_str();
    puts(c_str);
    free(c_str);  // Must free!
}
```

**Runtime**:
```c
char* aria_string_to_c_str(aria_string s) {
    // Allocate buffer with space for null terminator
    char* c_str = malloc(s.len + 1);
    memcpy(c_str, s.data, s.len);
    c_str[s.len] = '\0';  // Null-terminate
    return c_str;
}
```

---

**Conversion (C → Aria)**:
```aria
extern func:getenv = wild byte*(name: wild byte*);

func:get_env_var = result<string>(name: string) {
    wild byte*:c_name = name.to_c_str();
    wild byte*:c_value = getenv(c_name);
    free(c_name);
    
    if (c_value == null) {
        err("Environment variable not found");
    }
    
    // Convert C string to Aria string
    string:value = string.from_c_str(c_value);
    pass(value);
}
```

**Runtime**:
```c
aria_string aria_string_from_c_str(const char* c_str) {
    size_t len = strlen(c_str);
    aria_string s;
    s.data = malloc(len);
    memcpy(s.data, c_str, len);
    s.len = len;
    s.capacity = len;
    return s;
}
```

---

### Structs

**C Struct**:
```c
// C header
struct Point {
    int x;
    int y;
};

void print_point(struct Point p);
```

**Aria Declaration**:
```aria
// Declare C-compatible struct
extern struct:Point = {
    x: i32,
    y: i32,
};

extern func:print_point = void(p: Point);

// Use from Aria
func:main = i64() {
    Point:p = Point { x: 10, y: 20 };
    print_point(p);
    pass(0);
}
```

**Layout Compatibility**: Compiler ensures Aria struct layout matches C

---

### Opaque Types

**Problem**: C library uses opaque pointers (incomplete types)

**Example** (C library):
```c
// C header (no definition visible)
typedef struct CURL CURL;  // Opaque

CURL* curl_easy_init(void);
void curl_easy_cleanup(CURL* handle);
```

**Aria Declaration**:
```aria
// Declare opaque type (no fields)
extern struct:CURL = opaque;

extern func:curl_easy_init = wild CURL*();
extern func:curl_easy_cleanup = void(handle: wild CURL*);

// Use opaque pointer
func:download = void(url: string) {
    wild CURL*:handle = curl_easy_init();
    // ... use handle ...
    curl_easy_cleanup(handle);
}
```

**Compiler**: Treats `CURL` as incomplete type, only allows pointers

---

## Memory Safety

### Unsafe Boundary

**Key Principle**: **FFI is inherently unsafe** → mark it explicitly

**Syntax**: `unsafe` block
```aria
func:call_unsafe_c = i32() {
    unsafe {
        // C code can do anything!
        wild byte*:buf = malloc(100);
        wild byte*:p = buf + 200;  // Out of bounds (undefined behavior)
        *p = 42;  // Crash or corruption
        free(buf);
    }
    pass(0);
}
```

**Compiler**: Allows unchecked operations inside `unsafe` blocks

---

### Null Pointer Checks

**Problem**: C functions often return null on error

**Example** (C function):
```c
FILE* fopen(const char* path, const char* mode);
// Returns NULL on error
```

**Safe Wrapper** (Aria):
```aria
extern func:fopen = wild FILE*(path: wild byte*, mode: wild byte*);

func:open_file = result<wild FILE*>(path: string, mode: string) {
    unsafe {
        wild byte*:c_path = path.to_c_str();
        wild byte*:c_mode = mode.to_c_str();
        
        wild FILE*:file = fopen(c_path, c_mode);
        
        free(c_path);
        free(c_mode);
        
        if (file == null) {
            err("Failed to open file");
        }
        
        pass(file);
    }
}
```

**Pattern**: Wrap unsafe C calls in safe Aria functions that return `result<T>`

---

### Memory Ownership

**Problem**: Who owns allocated memory?

**Rule**: **Document ownership in function signatures**

**Example**:
```aria
// Caller owns returned pointer (must free)
extern func:strdup = wild byte*(s: wild byte*) [caller_owns_return];

// Callee owns parameter (don't free)
extern func:puts = i32(s: wild byte*) [callee_borrows_param];

// Transfer ownership to callee
extern func:takes_ownership = void(ptr: wild byte*) [callee_owns_param];
```

**Annotation** (future extension): Compiler enforces ownership rules

---

## Calling Conventions

### Platform-Specific ABIs

**Linux/macOS (x86_64)**: System V AMD64 ABI
- First 6 integer args in registers: `rdi, rsi, rdx, rcx, r8, r9`
- First 8 float args in: `xmm0-xmm7`
- Return value in: `rax` (integer), `xmm0` (float)

**Windows (x86_64)**: Microsoft x64 Calling Convention
- First 4 args in registers: `rcx, rdx, r8, r9`
- First 4 float args in: `xmm0-xmm3`
- Return value in: `rax` (integer), `xmm0` (float)

**Aria Compiler**: Automatically uses platform default

---

### Custom Calling Conventions

**Syntax**: `extern "convention"`

**Example** (Windows stdcall):
```aria
extern "stdcall" func:MessageBoxA = i32(
    hwnd: wild void*,
    text: wild byte*,
    caption: wild byte*,
    type: u32
);
```

**Conventions Supported**:
- `"C"` (default): Platform C calling convention
- `"stdcall"`: Windows standard call (callee cleans stack)
- `"fastcall"`: Pass args in registers

---

## Nikola Integration

### Nikola C++ API

**Nikola**: 9D Toroidal Wave Interference (9D-TWI) consciousness substrate

**C++ Interface** (example):
```cpp
// nikola.h
namespace nikola {
    class ConsciousnessField {
    public:
        ConsciousnessField(size_t dimensions);
        ~ConsciousnessField();
        
        void inject_wave(const double* wave_data, size_t length);
        void propagate(double time_step);
        void extract_state(double* output_buffer, size_t buffer_size);
    };
}
```

---

### C Wrapper for Nikola

**Problem**: Aria FFI works with C, not C++

**Solution**: Create C wrapper around C++ API

**C Wrapper** (`nikola_c.cpp`):
```cpp
// nikola_c.h
#ifdef __cplusplus
extern "C" {
#endif

typedef struct nikola_field nikola_field;

nikola_field* nikola_create_field(size_t dimensions);
void nikola_destroy_field(nikola_field* field);
void nikola_inject_wave(nikola_field* field, const double* wave_data, size_t length);
void nikola_propagate(nikola_field* field, double time_step);
void nikola_extract_state(nikola_field* field, double* output_buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
```

**Implementation**:
```cpp
// nikola_c.cpp
#include "nikola.h"
#include "nikola_c.h"

extern "C" {
    nikola_field* nikola_create_field(size_t dimensions) {
        return reinterpret_cast<nikola_field*>(new nikola::ConsciousnessField(dimensions));
    }
    
    void nikola_destroy_field(nikola_field* field) {
        delete reinterpret_cast<nikola::ConsciousnessField*>(field);
    }
    
    void nikola_inject_wave(nikola_field* field, const double* wave_data, size_t length) {
        reinterpret_cast<nikola::ConsciousnessField*>(field)->inject_wave(wave_data, length);
    }
    
    // ... etc
}
```

---

### Aria Bindings for Nikola

**Declare C API**:
```aria
// nikola_ffi.aria
extern struct:nikola_field = opaque;

extern func:nikola_create_field = wild nikola_field*(dimensions: u64);
extern func:nikola_destroy_field = void(field: wild nikola_field*);
extern func:nikola_inject_wave = void(
    field: wild nikola_field*,
    wave_data: wild f64*,
    length: u64
);
extern func:nikola_propagate = void(field: wild nikola_field*, time_step: f64);
extern func:nikola_extract_state = void(
    field: wild nikola_field*,
    output_buffer: wild f64*,
    buffer_size: u64
);
```

---

**Safe Aria Wrapper**:
```aria
// nikola.aria
import:ffi = "nikola_ffi";

struct:ConsciousnessField = {
    wild ffi.nikola_field*:handle,
};

func:ConsciousnessField.new = result<ConsciousnessField>(dimensions: u64) {
    unsafe {
        wild ffi.nikola_field*:handle = ffi.nikola_create_field(dimensions);
        
        if (handle == null) {
            err("Failed to create consciousness field");
        }
        
        pass(ConsciousnessField { handle: handle });
    }
}

func:ConsciousnessField.destroy = void(self: wild ConsciousnessField*) {
    unsafe {
        if (self->handle != null) {
            ffi.nikola_destroy_field(self->handle);
            self->handle = null;
        }
    }
}

func:ConsciousnessField.inject_wave = result<void>(
    self: wild ConsciousnessField*,
    wave_data: array<f64>
) {
    unsafe {
        // Safety: Aria array guarantees valid pointer and length
        ffi.nikola_inject_wave(
            self->handle,
            wave_data.data(),
            wave_data.len()
        );
        pass();
    }
}

func:ConsciousnessField.propagate = void(self: wild ConsciousnessField*, time_step: f64) {
    unsafe {
        ffi.nikola_propagate(self->handle, time_step);
    }
}

func:ConsciousnessField.extract_state = result<array<f64>>(
    self: wild ConsciousnessField*,
    buffer_size: u64
) {
    array<f64>:output = array<f64>(buffer_size);
    
    unsafe {
        ffi.nikola_extract_state(
            self->handle,
            output.data(),
            buffer_size
        );
    }
    
    pass(output);
}
```

---

**Usage** (Aria application):
```aria
import:nikola = "nikola";

async func:run_consciousness_simulation = result<void>() {
    // Create 9D consciousness field
    result<nikola.ConsciousnessField>:field = nikola.ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild nikola.ConsciousnessField*:cf = field.ok();
    
    // Inject initial wave pattern
    array<f64>:initial_wave = generate_wave_pattern();
    cf.inject_wave(initial_wave)?;
    
    // Simulate propagation
    for (t in 0..1000) {
        cf.propagate(0.001);  // 1ms time steps
        
        // Extract state periodically
        if (t % 100 == 0) {
            array<f64>:state = cf.extract_state(9)?;
            
            // Output to 6-stream I/O (binary data)
            io.stddato.write(state.data(), state.len() * 8);
        }
        
        // Yield for async runtime
        await yield();
    }
    
    // Cleanup
    cf.destroy();
    
    pass();
}
```

---

### 6-Stream Data Exchange

**Pattern**: Use `stddati` and `stddato` for binary wave data

**Nikola Output** (visualization):
```bash
$ ./aria_nikola_sim 5> wave_data.bin
# Binary wave data streamed to file
```

**External Processor** (Python):
```bash
$ ./aria_nikola_sim 5>&1 | python visualize_waves.py
# Python reads binary data from stddato, renders visualization
```

---

## Best Practices

### 1. Minimize Unsafe Code

**Pattern**: Thin unsafe FFI layer, safe high-level API

**Bad**:
```aria
// Exposing unsafe C API directly
extern func:malloc = wild byte*(size: u64);
// Users have to deal with raw pointers everywhere
```

**Good**:
```aria
// Safe wrapper
func:allocate_buffer = result<array<byte>>(size: u64) {
    unsafe {
        wild byte*:buf = malloc(size);
        if (buf == null) {
            err("Allocation failed");
        }
        // Wrap in safe array
        pass(array.from_raw_parts(buf, size));
    }
}
```

---

### 2. Document Ownership

**Comments**:
```aria
// Caller must free returned pointer
extern func:strdup = wild byte*(s: wild byte*);

// C library retains ownership (don't free)
extern func:get_static_string = wild byte*();
```

---

### 3. Validate Inputs

**Before Calling C**:
```aria
func:safe_c_function = result<void>(input: string) {
    // Validate
    if (input.len() > 1024) {
        err("Input too long");
    }
    
    unsafe {
        wild byte*:c_str = input.to_c_str();
        c_function(c_str);
        free(c_str);
    }
    
    pass();
}
```

---

### 4. Handle Errors

**C Functions Often Fail Silently**:
```aria
extern func:fwrite = u64(
    ptr: wild void*,
    size: u64,
    count: u64,
    stream: wild FILE*
);

func:write_data = result<void>(data: array<byte>, file: wild FILE*) {
    unsafe {
        u64:written = fwrite(data.data(), 1, data.len(), file);
        
        if (written != data.len()) {
            err("Write failed: only wrote " + written + " bytes");
        }
        
        pass();
    }
}
```

---

## Related Documents

- **[TYPE_SYSTEM](./TYPE_SYSTEM.md)**: Wild pointers and type marshalling
- **[MEMORY_MODEL](./MEMORY_MODEL.md)**: Wild allocator for FFI
- **[IO_TOPOLOGY](./IO_TOPOLOGY.md)**: 6-stream binary data exchange
- **[NIKOLA_ARIA](../integration/NIKOLA_ARIA.md)**: Complete Nikola integration guide
- **[ERROR_HANDLING](./ERROR_HANDLING.md)**: Result<T> for FFI error handling

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Design specification (research complete, implementation ready)

**Research Complete** ✅ (December 22, 2025):
- **FFI design for C interop**: Standard C ABI mapping, zero-cost primitives
- **Memory safety**: Pinning operator (#) for GC objects passed to C
- **Ownership tracking**: Appendage Theory enforces Host outlives Appendage
- **TBB preservation**: Sticky error propagation across FFI boundary
- **Type mapping**: Aria types → LLVM IR → C ABI deterministic mapping
- **Result pattern**: AriaResult struct allocated on heap, caller frees

**Key Findings** (from language_advanced_research_task.txt):
- **Type Mapping Table**:
  * int8/tbb8 → i8 → int8_t (1 byte)
  * int64 → i64 → int64_t (8 bytes)
  * int128 → i128 → __int128 (16 bytes, x86-64 supported)
  * wild byte* → i8* → uint8_t* (8 bytes, raw pointer)
  * result<T> → {i8*, T} → AriaResult* (heap-allocated struct)
- **TBB Sticky Errors**: C code must check for sentinel (e.g., -128 for tbb8)
- **Runtime intrinsics**: tbb8_add, tbb8_mul for C consumers
- **Pinning**: # operator sets bit in object header, prevents GC movement
- **Borrow checker**: Validates pointers to C derived from pinned references
- **W^X safety**: aria_mem_protect_exec transitions wildx from Write to Execute

**FFI Safety Rules**:
1. Wild pointers: zero-cost, manual management (map directly to C pointers)
2. GC pointers: require pinning (#) before passing to C
3. Result<T>: caller must call aria_result_free() to prevent leaks
4. TBB types: C code must preserve sentinel semantics or use intrinsics
5. Extern blocks: manual symbol declaration, resolved at link time
