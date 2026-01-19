# `extern` Syntax

**Category**: Modules → FFI  
**Purpose**: Detailed extern declaration syntax

---

## Overview

`extern` has several syntax forms for interfacing with foreign code.

---

## Basic Syntax

```aria
extern "<abi>" {
    fn <function_name>(<params>) -> <return_type>;
}
```

---

## ABI Specifications

### C ABI

```aria
extern "C" {
    fn function_name();
}
```

### System ABI

```aria
extern "system" {
    fn system_function();
}
```

### Platform-Specific

```aria
extern "stdcall" {  // Windows
    fn windows_function();
}

extern "fastcall" {  // Windows
    fn fast_function();
}
```

---

## Function Declarations

### Simple Function

```aria
extern "C" {
    fn simple_func() -> i32;
}
```

### With Parameters

```aria
extern "C" {
    fn add(a: i32, b: i32) -> i32;
    fn multiply(x: f64, y: f64) -> f64;
}
```

### With Pointers

```aria
extern "C" {
    fn process_data(data: *void, size: usize) -> i32;
    fn get_string() -> *u8;
}
```

---

## Variadic Functions

```aria
extern "C" {
    fn printf(format: *u8, ...) -> i32;
    fn sprintf(buffer: *u8, format: *u8, ...) -> i32;
}
```

---

## Multiple Declarations

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn realloc(ptr: *void, new_size: usize) -> *void;
    fn calloc(num: usize, size: usize) -> *void;
}
```

---

## Linking Attributes

```aria
#[link(name = "m")]
extern "C" {
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
}

#[link(name = "crypto")]
extern "C" {
    fn encrypt(data: *void) -> i32;
    fn decrypt(data: *void) -> i32;
}
```

---

## Static Variables

```aria
extern "C" {
    static errno: i32;
    static stdout: *void;
}
```

---

## Exporting Aria Functions

### Basic Export

```aria
#[no_mangle]
pub extern "C" fn aria_function(x: i32) -> i32 {
    return x * 2;
}
```

### With Attributes

```aria
#[no_mangle]
#[export_name = "custom_name"]
pub extern "C" fn aria_func() -> i32 {
    return 42;
}
```

---

## Type Mapping

### Integers

```aria
extern "C" {
    fn int8_func(x: i8) -> i8;
    fn int16_func(x: i16) -> i16;
    fn int32_func(x: i32) -> i32;
    fn int64_func(x: i64) -> i64;
}
```

### Floats

```aria
extern "C" {
    fn float_func(x: f32) -> f32;
    fn double_func(x: f64) -> f64;
}
```

### Pointers

```aria
extern "C" {
    fn void_ptr_func(ptr: *void) -> *void;
    fn u8_ptr_func(ptr: *u8) -> *u8;
    fn const_ptr_func(ptr: *const void) -> *const void;
    fn mut_ptr_func(ptr: *mut void) -> *mut void;
}
```

---

## Platform-Specific Syntax

### Conditional Extern

```aria
#[cfg(target_os = "linux")]
extern "C" {
    fn linux_specific();
}

#[cfg(target_os = "windows")]
extern "system" {
    fn windows_specific();
}

#[cfg(target_os = "macos")]
extern "C" {
    fn macos_specific();
}
```

---

## Common Patterns

### C Standard Library

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
    fn printf(format: *u8, ...) -> i32;
}
```

### POSIX Functions

```aria
#[link(name = "c")]
extern "C" {
    fn open(path: *u8, flags: i32) -> i32;
    fn close(fd: i32) -> i32;
    fn read(fd: i32, buf: *void, count: usize) -> isize;
    fn write(fd: i32, buf: *void, count: usize) -> isize;
}
```

### Math Library

```aria
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn log(x: f64) -> f64;
}
```

---

## Best Practices

### ✅ DO: Specify ABI Explicitly

```aria
extern "C" {  // ✅ Clear ABI
    fn c_function();
}
```

### ✅ DO: Document Foreign Functions

```aria
// Allocates memory using C malloc
// Caller must free using free()
extern "C" {
    fn malloc(size: usize) -> *void;
}
```

### ✅ DO: Group Related Functions

```aria
// String functions
extern "C" {
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
    fn strcpy(dest: *u8, src: *u8) -> *u8;
}

// Memory functions
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
}
```

### ⚠️ WARNING: No Safety Checks

```aria
extern "C" {
    fn dangerous(ptr: *void);  // No null checks!
}
```

---

## Syntax Reference

| Syntax | Purpose |
|--------|---------|
| `extern "C" { }` | C calling convention |
| `extern "system" { }` | System calling convention |
| `#[link(name = "lib")]` | Link external library |
| `#[no_mangle]` | Prevent name mangling |
| `pub extern "C" fn` | Export function to C |
| `static var: type;` | Extern static variable |

---

## Related

- [extern](extern.md) - Extern keyword overview
- [extern_blocks](extern_blocks.md) - Extern blocks
- [extern_functions](extern_functions.md) - External functions
- [ffi](ffi.md) - FFI overview

---

**Remember**: Get the **ABI** and **signatures** exactly right!
