# Extern Blocks

**Category**: Modules → FFI  
**Purpose**: Organize foreign function declarations

---

## Overview

Extern blocks group **foreign function declarations** with a specific ABI.

---

## Basic Extern Block

```aria
extern "C" {
    fn function1() -> i32;
    fn function2(x: i32) -> i32;
    fn function3(x: i32, y: i32) -> i32;
}
```

---

## Multiple Blocks

```aria
// C standard library
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

// Math library
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn sin(x: f64) -> f64;
}

// Custom library
#[link(name = "mylib")]
extern "C" {
    fn custom_function() -> i32;
}
```

---

## Block Organization

### By Library

```aria
// libc
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn strlen(s: *u8) -> usize;
}

// libm (math)
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
}

// libcrypto
#[link(name = "crypto")]
extern "C" {
    fn encrypt(data: *void) -> i32;
    fn decrypt(data: *void) -> i32;
}
```

---

### By Functionality

```aria
// Memory management
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn realloc(ptr: *void, size: usize) -> *void;
}

// String operations
extern "C" {
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
    fn strcpy(dest: *u8, src: *u8) -> *u8;
}

// File I/O
extern "C" {
    fn fopen(path: *u8, mode: *u8) -> *void;
    fn fclose(file: *void) -> i32;
    fn fread(ptr: *void, size: usize, count: usize, file: *void) -> usize;
}
```

---

## Static Variables in Blocks

```aria
extern "C" {
    static errno: i32;
    static stdin: *void;
    static stdout: *void;
    static stderr: *void;
}
```

---

## Attributes on Blocks

```aria
#[link(name = "mylib")]
#[link(kind = "static")]
extern "C" {
    fn library_function();
}
```

---

## Platform-Specific Blocks

```aria
#[cfg(target_os = "linux")]
extern "C" {
    fn linux_function();
}

#[cfg(target_os = "windows")]
extern "system" {
    fn windows_function();
}

#[cfg(target_os = "macos")]
extern "C" {
    fn macos_function();
}
```

---

## Common Patterns

### C Standard Library Block

```aria
extern "C" {
    // Memory
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn calloc(num: usize, size: usize) -> *void;
    fn realloc(ptr: *void, size: usize) -> *void;
    
    // Strings
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
    fn strcpy(dest: *u8, src: *u8) -> *u8;
    fn strcat(dest: *u8, src: *u8) -> *u8;
    
    // I/O
    fn printf(format: *u8, ...) -> i32;
    fn scanf(format: *u8, ...) -> i32;
    fn puts(s: *u8) -> i32;
}
```

---

### Math Library Block

```aria
#[link(name = "m")]
extern "C" {
    // Trigonometry
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn tan(x: f64) -> f64;
    
    // Powers and roots
    fn sqrt(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
    
    // Logarithms
    fn log(x: f64) -> f64;
    fn log10(x: f64) -> f64;
    
    // Rounding
    fn floor(x: f64) -> f64;
    fn ceil(x: f64) -> f64;
    fn round(x: f64) -> f64;
}
```

---

### POSIX Block

```aria
#[link(name = "c")]
extern "C" {
    // File operations
    fn open(path: *u8, flags: i32) -> i32;
    fn close(fd: i32) -> i32;
    fn read(fd: i32, buf: *void, count: usize) -> isize;
    fn write(fd: i32, buf: *void, count: usize) -> isize;
    
    // Process operations
    fn fork() -> i32;
    fn exec(path: *u8, argv: **u8) -> i32;
    fn wait(status: *i32) -> i32;
}
```

---

## Best Practices

### ✅ DO: Group Related Functions

```aria
// Group memory functions together
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void;
    fn realloc(ptr: *void, size: usize) -> *void;
}
```

### ✅ DO: Document Block Purpose

```aria
// SQLite3 database functions
#[link(name = "sqlite3")]
extern "C" {
    fn sqlite3_open(filename: *u8, db: **void) -> i32;
    fn sqlite3_close(db: *void) -> i32;
    fn sqlite3_exec(db: *void, sql: *u8) -> i32;
}
```

### ✅ DO: Use Link Attributes

```aria
#[link(name = "ssl")]
#[link(name = "crypto")]
extern "C" {
    fn SSL_library_init() -> i32;
    fn SSL_load_error_strings();
}
```

### ❌ DON'T: Mix ABIs in Same Block

```aria
// Bad - different ABIs
extern "C" {
    fn c_function();
}
extern "system" {  // ✅ Separate block
    fn system_function();
}
```

---

## Modular Organization

```aria
// In ffi/libc.aria
pub mod libc {
    extern "C" {
        pub fn malloc(size: usize) -> *void;
        pub fn free(ptr: *void);
    }
}

// In ffi/math.aria
pub mod math {
    #[link(name = "m")]
    extern "C" {
        pub fn sqrt(x: f64) -> f64;
        pub fn sin(x: f64) -> f64;
    }
}

// Use them
use ffi.libc;
use ffi.math;

ptr: *void = libc.malloc(100);
Result: f64 = math.sqrt(16.0);
```

---

## Related

- [extern](extern.md) - Extern keyword
- [extern_syntax](extern_syntax.md) - Extern syntax
- [extern_functions](extern_functions.md) - External functions
- [ffi](ffi.md) - FFI overview

---

**Remember**: **Organize** extern blocks by library or functionality!
