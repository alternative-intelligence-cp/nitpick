# `extern` Keyword

**Category**: Modules → FFI  
**Syntax**: `extern "<abi>" { ... }`  
**Purpose**: Interface with foreign code (C, system libraries)

---

## Overview

`extern` declares **foreign functions** and **external interfaces**.

---

## Basic Extern

```aria
extern "C" {
    fn printf(format: *u8, ...) -> i32;
}

// Use it
extern.printf("Hello from C!\n");
```

---

## Extern Block

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn strlen(s: *u8) -> usize;
}
```

---

## ABI (Application Binary Interface)

```aria
extern "C" { }      // C calling convention
extern "system" { } // System calling convention
extern "stdcall" { } // Windows stdcall
```

Most common: `"C"`

---

## Calling C Functions

```aria
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn abs(x: i32) -> i32;
}

Result: f64 = extern.sqrt(16.0);  // 4.0
value: i32 = extern.abs(-5);      // 5
```

---

## Linking Libraries

```aria
#[link(name = "m")]  // Link against libm (math library)
extern "C" {
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
}
```

---

## Common C Functions

### Standard C Library

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn memcpy(dest: *void, src: *void, n: usize) -> *void;
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
}
```

---

## Extern Functions from Aria

Make Aria functions callable from C:

```aria
#[no_mangle]  // Prevent name mangling
pub extern "C" fn aria_add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Best Practices

### ✅ DO: Wrap Unsafe Extern Calls

```aria
extern "C" {
    fn unsafe_c_function(ptr: *void) -> i32;
}

// Safe wrapper
pub fn safe_function(data: *Data) -> Result<i32> {
    if data == NULL {
        return Err("Null pointer");
    }
    
    Result: i32 = extern.unsafe_c_function(data);
    if result < 0 {
        return Err("C function failed");
    }
    
    return Ok(result);
}
```

### ✅ DO: Document ABI

```aria
// Calls C standard library malloc
extern "C" {
    fn malloc(size: usize) -> *void;
}
```

### ⚠️ WARNING: Extern is Unsafe

```aria
// No safety checks - you must validate!
extern "C" {
    fn dangerous_c_function(ptr: *void);
}

// Can crash, corrupt memory, etc.
extern.dangerous_c_function(invalid_ptr);  // ⚠️ DANGER
```

---

## Related

- [extern_syntax](extern_syntax.md) - Extern syntax details
- [extern_blocks](extern_blocks.md) - Extern blocks
- [extern_functions](extern_functions.md) - External functions
- [ffi](ffi.md) - Foreign Function Interface
- [c_interop](c_interop.md) - C interoperability

---

**Remember**: `extern` is **powerful** but **unsafe** - wrap it carefully!
