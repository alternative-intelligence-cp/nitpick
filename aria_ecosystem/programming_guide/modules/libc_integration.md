# libc Integration

**Category**: Modules → FFI  
**Purpose**: Using the C standard library from Aria

---

## Overview

libc (C standard library) provides **fundamental** system operations - memory, strings, I/O, math, and more.

---

## Commonly Used libc Functions

### Memory Management

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn calloc(num: usize, size: usize) -> *void;
    fn realloc(ptr: *void, new_size: usize) -> *void;
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
    fn memset(ptr: *void, value: i32, n: usize) -> *void;
    fn memmove(dest: *void, src: *void, n: usize) -> *void;
}
```

---

### String Operations

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
    fn strcpy(dest: *u8, src: *u8) -> *u8;
    fn strcat(dest: *u8, src: *u8) -> *u8;
    fn strncpy(dest: *u8, src: *u8, n: usize) -> *u8;
    fn strncmp(s1: *u8, s2: *u8, n: usize) -> i32;
}
```

---

### File I/O

```aria
extern "C" {
    fn fopen(path: *u8, mode: *u8) -> *void;
    fn fclose(file: *void) -> i32;
    fn fread(ptr: *void, size: usize, count: usize, file: *void) -> usize;
    fn fwrite(ptr: *void, size: usize, count: usize, file: *void) -> usize;
    fn fseek(file: *void, offset: i64, whence: i32) -> i32;
    fn ftell(file: *void) -> i64;
}
```

---

### Standard I/O

```aria
extern "C" {
    fn printf(format: *u8, ...) -> i32;
    fn scanf(format: *u8, ...) -> i32;
    fn puts(s: *u8) -> i32;
    fn putchar(c: i32) -> i32;
    fn getchar() -> i32;
}
```

---

### Math Functions

```aria
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn tan(x: f64) -> f64;
    fn log(x: f64) -> f64;
    fn log10(x: f64) -> f64;
    fn exp(x: f64) -> f64;
    fn floor(x: f64) -> f64;
    fn ceil(x: f64) -> f64;
    fn fabs(x: f64) -> f64;
}
```

---

## Memory Examples

### malloc/free

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

// Allocate
ptr: *void = extern.malloc(1024);
if ptr == NULL {
    return Err("Allocation failed");
}

// Use ptr...

// Free
extern.free(ptr);
```

---

### calloc (Zero-initialized)

```aria
extern "C" {
    fn calloc(num: usize, size: usize) -> *void;
    fn free(ptr: *void);
}

// Allocate array of 10 integers, zero-initialized
arr: *i32 = extern.calloc(10, sizeof(i32)) as *i32;

// Use arr[0] through arr[9]

extern.free(arr as *void);
```

---

### memcpy

```aria
extern "C" {
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
}

src: [u8; 100];
dest: [u8; 100];

// Copy 100 bytes from src to dest
extern.memcpy(&dest[0], &src[0], 100);
```

---

## String Examples

### strlen

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
}

str: *u8 = "Hello, World!\0";
len: usize = extern.strlen(str);  // 13
```

---

### strcmp

```aria
extern "C" {
    fn strcmp(s1: *u8, s2: *u8) -> i32;
}

Result: i32 = extern.strcmp("apple\0", "banana\0");
if result < 0 {
    stdout << "apple comes first";
} else if result > 0 {
    stdout << "banana comes first";
} else {
    stdout << "strings are equal";
}
```

---

### strcpy

```aria
extern "C" {
    fn strcpy(dest: *u8, src: *u8) -> *u8;
}

src: *u8 = "Hello\0";
dest: [u8; 100];

extern.strcpy(&dest[0], src);
// dest now contains "Hello\0"
```

---

## File I/O Examples

### Reading a File

```aria
extern "C" {
    fn fopen(path: *u8, mode: *u8) -> *void;
    fn fclose(file: *void) -> i32;
    fn fread(ptr: *void, size: usize, count: usize, file: *void) -> usize;
}

file: *void = extern.fopen("data.txt\0", "r\0");
if file == NULL {
    return Err("Failed to open file");
}

buffer: [u8; 1024];
bytes_read: usize = extern.fread(&buffer[0], 1, 1024, file);

stdout << "Read $bytes_read bytes";

extern.fclose(file);
```

---

### Writing a File

```aria
extern "C" {
    fn fopen(path: *u8, mode: *u8) -> *void;
    fn fclose(file: *void) -> i32;
    fn fwrite(ptr: *void, size: usize, count: usize, file: *void) -> usize;
}

file: *void = extern.fopen("output.txt\0", "w\0");
if file == NULL {
    return Err("Failed to open file");
}

data: *u8 = "Hello, File!\0";
bytes_written: usize = extern.fwrite(data, 1, 12, file);

extern.fclose(file);
```

---

## Math Examples

```aria
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
    fn sin(x: f64) -> f64;
}

root: f64 = extern.sqrt(16.0);        // 4.0
power: f64 = extern.pow(2.0, 10.0);   // 1024.0
sine: f64 = extern.sin(0.0);          // 0.0
```

---

## Error Handling

### errno

```aria
extern "C" {
    static errno: i32;
    fn fopen(path: *u8, mode: *u8) -> *void;
}

file: *void = extern.fopen("nonexistent.txt\0", "r\0");
if file == NULL {
    error: i32 = extern.errno;
    return Err("fopen failed with errno: $error");
}
```

---

### Checking Return Values

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
}

ptr: *void = extern.malloc(1024);
if ptr == NULL {
    return Err("malloc failed - out of memory");
}
```

---

## Safe Wrappers

### Memory Allocation

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

pub fn safe_alloc(size: usize) -> Result<*void> {
    if size == 0 {
        return Err("Invalid size");
    }
    
    ptr: *void = extern.malloc(size);
    if ptr == NULL {
        return Err("Allocation failed");
    }
    
    return Ok(ptr);
}

pub fn safe_free(ptr: *void) {
    if ptr != NULL {
        extern.free(ptr);
    }
}
```

---

### String Operations

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
}

pub fn safe_strlen(s: *u8) -> Result<usize> {
    if s == NULL {
        return Err("Null string");
    }
    
    return Ok(extern.strlen(s));
}
```

---

## Platform-Specific libc

```aria
#[cfg(target_os = "linux")]
extern "C" {
    fn getpid() -> i32;
    fn getuid() -> i32;
}

#[cfg(target_os = "windows")]
extern "C" {
    fn GetCurrentProcessId() -> u32;
}
```

---

## Best Practices

### ✅ DO: Check Return Values

```aria
ptr: *void = extern.malloc(1024);
if ptr == NULL {  // ✅ Always check!
    return Err("Allocation failed");
}
```

### ✅ DO: Free Allocated Memory

```aria
ptr: *void = extern.malloc(1024);
// ... use ptr ...
extern.free(ptr);  // ✅ Don't leak!
```

### ✅ DO: Null-Terminate Strings

```aria
str: *u8 = "Hello\0";  // ✅ Null terminator for C
```

### ⚠️ WARNING: Buffer Overflows

```aria
extern "C" {
    fn strcpy(dest: *u8, src: *u8) -> *u8;
}

dest: [u8; 10];
src: *u8 = "This string is way too long\0";
extern.strcpy(&dest[0], src);  // ⚠️ Buffer overflow!

// Use strncpy instead
extern "C" {
    fn strncpy(dest: *u8, src: *u8, n: usize) -> *u8;
}
extern.strncpy(&dest[0], src, 9);  // ✅ Safe
```

---

## Common libc Constants

```aria
// File modes
const O_RDONLY: i32 = 0;
const O_WRONLY: i32 = 1;
const O_RDWR: i32 = 2;

// Seek whence
const SEEK_SET: i32 = 0;
const SEEK_CUR: i32 = 1;
const SEEK_END: i32 = 2;

// File stream pointers
extern "C" {
    static stdin: *void;
    static stdout: *void;
    static stderr: *void;
}
```

---

## Related

- [c_interop](c_interop.md) - C interoperability
- [c_pointers](c_pointers.md) - C pointer handling
- [ffi](ffi.md) - FFI overview

---

**Remember**: libc is **battle-tested** but **unsafe** - validate inputs and check returns!
