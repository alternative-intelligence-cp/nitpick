# Foreign Function Interface (FFI)

**Category**: Modules → FFI  
**Purpose**: Interfacing Aria with C and other languages

---

## Overview

FFI allows Aria to **call foreign code** (C, C++, system libraries) and be **called by** foreign code.

---

## Why FFI?

- ✅ Use existing C libraries
- ✅ Call system APIs
- ✅ Interop with legacy code
- ✅ Access platform-specific features
- ✅ Integrate with databases, crypto libraries, etc.

---

## Basic FFI Pattern

### 1. Declare Foreign Function

```aria
extern "C" {
    fn c_function(x: i32) -> i32;
}
```

### 2. Link Library

```aria
#[link(name = "mylib")]
extern "C" {
    fn library_function();
}
```

### 3. Call It

```aria
Result: i32 = extern.c_function(42);
```

---

## Calling C from Aria

```aria
// Declare C functions
extern "C" {
    fn strlen(s: *u8) -> usize;
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

// Use them
length: usize = extern.strlen("Hello");
ptr: *void = extern.malloc(1024);
// ... use ptr ...
extern.free(ptr);
```

---

## Calling Aria from C

```aria
// Export Aria function to C
#[no_mangle]
pub extern "C" fn aria_add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

C code:
```c
// Declare in C header
extern int aria_add(int a, int b);

// Call it
int result = aria_add(5, 3);  // 8
```

---

## Type Mapping

### Integers

| Aria | C | Size |
|------|---|------|
| `i8` | `int8_t` / `char` | 8-bit |
| `i16` | `int16_t` / `short` | 16-bit |
| `i32` | `int32_t` / `int` | 32-bit |
| `i64` | `int64_t` / `long long` | 64-bit |
| `u8` | `uint8_t` / `unsigned char` | 8-bit |
| `u16` | `uint16_t` / `unsigned short` | 16-bit |
| `u32` | `uint32_t` / `unsigned int` | 32-bit |
| `u64` | `uint64_t` / `unsigned long long` | 64-bit |

### Floats

| Aria | C | Size |
|------|---|------|
| `f32` | `float` | 32-bit |
| `f64` | `double` | 64-bit |

### Pointers

| Aria | C |
|------|---|
| `*void` | `void*` |
| `*u8` | `char*` / `uint8_t*` |
| `*const T` | `const T*` |
| `*mut T` | `T*` |

---

## Common FFI Patterns

### Using libc

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
    fn strlen(s: *u8) -> usize;
}
```

---

### Using Math Library

```aria
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
}

Result: f64 = extern.sqrt(16.0);  // 4.0
```

---

### Safe Wrapper Pattern

```aria
// Unsafe extern
extern "C" {
    fn unsafe_c_function(data: *void, size: usize) -> i32;
}

// Safe Aria wrapper
pub fn safe_wrapper(data: []u8) -> Result<void> {
    if data.len() == 0 {
        return Err("Empty data");
    }
    
    Result: i32 = extern.unsafe_c_function(&data[0], data.len());
    
    if result != 0 {
        return Err("Operation failed");
    }
    
    return Ok();
}
```

---

## Memory Management

### Rule: **Allocator owns the memory**

```aria
// If C allocates, C must free
extern "C" {
    fn c_allocate() -> *void;
    fn c_free(ptr: *void);
}

ptr: *void = extern.c_allocate();
// ... use ptr ...
extern.c_free(ptr);  // ✅ C frees what C allocated
```

```aria
// If Aria allocates for C, C must know how to free it
#[no_mangle]
pub extern "C" fn aria_allocate() -> *void {
    // Use C allocator so C can free
    extern "C" {
        fn malloc(size: usize) -> *void;
    }
    return extern.malloc(1024);
}
```

---

## String Handling

### C String to Aria

```aria
extern "C" {
    fn get_c_string() -> *u8;  // Returns null-terminated string
}

c_str: *u8 = extern.get_c_string();

// Convert to Aria string (copy)
aria_str: string = string.from_c_str(c_str);
```

---

### Aria String to C

```aria
#[no_mangle]
pub extern "C" fn aria_get_string() -> *u8 {
    // Must be static or heap-allocated and null-terminated
    return "Hello from Aria\0";
}
```

---

## Error Handling

### C Functions

```aria
extern "C" {
    fn c_function() -> i32;  // 0 = success, -1 = error
}

Result: i32 = extern.c_function();
if result != 0 {
    return Err("C function failed");
}
```

---

### Aria Functions for C

```aria
#[no_mangle]
pub extern "C" fn aria_function(out: *mut i32) -> i32 {
    if out == NULL {
        return -1;  // Error: null pointer
    }
    
    *out = 42;
    return 0;  // Success
}
```

---

## Platform-Specific FFI

```aria
#[cfg(target_os = "linux")]
extern "C" {
    fn linux_specific_function();
}

#[cfg(target_os = "windows")]
extern "system" {
    fn windows_specific_function();
}

#[cfg(target_os = "macos")]
extern "C" {
    fn macos_specific_function();
}
```

---

## Callbacks

```aria
// C library expects a callback
extern "C" {
    fn register_callback(cb: extern "C" fn(i32) -> i32);
}

// Define Aria callback
#[no_mangle]
extern "C" fn my_callback(value: i32) -> i32 {
    return value * 2;
}

// Register
extern.register_callback(my_callback);
```

---

## Best Practices

### ✅ DO: Wrap Unsafe FFI

```aria
// Hide unsafe extern behind safe API
pub fn safe_api() -> Result<Data> {
    // Validate inputs
    // Call unsafe extern
    // Check results
    // Return safely
}
```

### ✅ DO: Check Pointers

```aria
if ptr == NULL {
    return Err("Null pointer");
}
```

### ✅ DO: Document Ownership

```aria
// Returns pointer owned by caller - must call free()
#[no_mangle]
pub extern "C" fn aria_allocate() -> *void {
    // ...
}
```

### ⚠️ WARNING: Don't Panic

```aria
#[no_mangle]
pub extern "C" fn aria_func() -> i32 {
    // ❌ Never panic across FFI boundary!
    // ✅ Return error codes instead
}
```

### ⚠️ WARNING: ABI Matters

```aria
extern "C" { }      // ✅ For C functions
extern "system" { } // ✅ For Windows system functions
// Wrong ABI = crashes!
```

---

## Common Libraries

### SQLite

```aria
#[link(name = "sqlite3")]
extern "C" {
    fn sqlite3_open(filename: *u8, db: **void) -> i32;
    fn sqlite3_close(db: *void) -> i32;
    fn sqlite3_exec(db: *void, sql: *u8) -> i32;
}
```

### OpenSSL

```aria
#[link(name = "ssl")]
#[link(name = "crypto")]
extern "C" {
    fn SSL_library_init() -> i32;
    fn SSL_CTX_new() -> *void;
}
```

---

## Related

- [extern](extern.md) - Extern keyword
- [c_interop](c_interop.md) - C interoperability
- [c_pointers](c_pointers.md) - C pointer handling
- [libc_integration](libc_integration.md) - libc integration

---

**Remember**: FFI is **powerful** but **dangerous** - always validate and wrap!
