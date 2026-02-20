# External Functions

**Category**: Modules → FFI  
**Purpose**: Calling and exposing functions across language boundaries

---

## Overview

External functions are **foreign functions** called from Aria or **Aria functions** exposed to other languages.

---

## Calling External Functions

### Basic C Function

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
}

// Call it
length: usize = extern.strlen("Hello");
```

---

### With Parameters

```aria
extern "C" {
    fn strcmp(s1: *u8, s2: *u8) -> i32;
}

Result: i32 = extern.strcmp("abc", "abd");
if result < 0 {
    stdout << "First string is less";
}
```

---

## Return Values

```aria
extern "C" {
    fn abs(x: i32) -> i32;
    fn sqrt(x: f64) -> f64;
    fn malloc(size: usize) -> *void;
}

value: i32 = extern.abs(-42);        // 42
root: f64 = extern.sqrt(16.0);       // 4.0
ptr: *void = extern.malloc(1024);    // Memory pointer
```

---

## Pointer Parameters

```aria
extern "C" {
    fn process_data(data: *void, size: usize) -> i32;
}

data: [u8; 100];
Result: i32 = extern.process_data(&data, 100);
```

---

## Variadic Functions

```aria
extern "C" {
    fn printf(format: *u8, ...) -> i32;
}

extern.printf("Number: %d, String: %s\n", 42, "hello");
```

---

## Exposing Aria Functions to C

### Basic Export

```aria
#[no_mangle]
pub extern "C" fn aria_add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

The `#[no_mangle]` attribute prevents name mangling so C can find the function.

---

### More Complex Export

```aria
#[no_mangle]
pub extern "C" fn aria_process_array(arr: *i32, len: usize) -> i32 {
    if arr == NULL {
        return -1;
    }
    
    sum: i32 = 0;
    till(len - 1, 1) {
        sum += arr[$];
    }
    
    return sum;
}
```

---

## Custom Export Name

```aria
#[no_mangle]
#[export_name = "custom_function_name"]
pub extern "C" fn internal_name() -> i32 {
    return 42;
}
```

C code can call it as `custom_function_name()`.

---

## Error Handling

### C Functions Return Error Codes

```aria
extern "C" {
    fn c_function() -> i32;  // Returns 0 on success, -1 on error
}

Result: i32 = extern.c_function();
if result != 0 {
    stdout << "Error occurred!";
}
```

---

### Aria Functions for C

```aria
#[no_mangle]
pub extern "C" fn aria_divide(a: i32, b: i32, Result: *mut i32) -> i32 {
    if b == 0 {
        return -1;  // Error code
    }
    
    if result == NULL {
        return -2;  // Null pointer error
    }
    
    *result = a / b;
    return 0;  // Success
}
```

---

## String Handling

### Receiving C Strings

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
}

c_str: *u8 = "Hello from C";
length: usize = extern.strlen(c_str);
```

---

### Returning Strings to C

```aria
#[no_mangle]
pub extern "C" fn aria_get_string() -> *u8 {
    // Must be static or heap-allocated
    return "Hello from Aria\0";
}
```

⚠️ **Warning**: String must outlive the call!

---

## Memory Management

### C Allocates, Aria Frees

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

ptr: *void = extern.malloc(1024);
// Use ptr...
extern.free(ptr);
```

---

### Aria Allocates, C Frees

```aria
#[no_mangle]
pub extern "C" fn aria_allocate(size: usize) -> *void {
    // Allocate using C-compatible allocator
    extern "C" {
        fn malloc(size: usize) -> *void;
    }
    return extern.malloc(size);
}

#[no_mangle]
pub extern "C" fn aria_free(ptr: *void) {
    extern "C" {
        fn free(ptr: *void);
    }
    extern.free(ptr);
}
```

---

## Common Patterns

### Safe Wrapper

```aria
// Unsafe extern declaration
extern "C" {
    fn unsafe_c_function(data: *void, size: usize) -> i32;
}

// Safe Aria wrapper
pub fn safe_process(data: []u8) -> Result<i32> {
    if data.len() == 0 {
        return Err("Empty data");
    }
    
    Result: i32 = extern.unsafe_c_function(&data[0], data.len());
    
    if result < 0 {
        return Err("C function failed");
    }
    
    return Ok(result);
}
```

---

### Callback Functions

```aria
// C expects a callback
extern "C" {
    fn register_callback(cb: extern "C" fn(i32) -> i32);
}

// Define callback in Aria
#[no_mangle]
extern "C" fn my_callback(value: i32) -> i32 {
    return value * 2;
}

// Register it
extern.register_callback(my_callback);
```

---

## Best Practices

### ✅ DO: Validate Pointers

```aria
#[no_mangle]
pub extern "C" fn aria_func(ptr: *void) -> i32 {
    if ptr == NULL {
        return -1;  // ✅ Check for null
    }
    // Process...
}
```

### ✅ DO: Use C-Compatible Types

```aria
#[no_mangle]
pub extern "C" fn aria_func(
    x: i32,      // ✅ C int
    y: f64,      // ✅ C double
    ptr: *u8     // ✅ C char*
) -> i32 {
    // Implementation
}
```

### ✅ DO: Document Ownership

```aria
// Caller must free returned pointer using free()
#[no_mangle]
pub extern "C" fn aria_allocate_string() -> *u8 {
    extern "C" {
        fn malloc(size: usize) -> *void;
    }
    return extern.malloc(100);
}
```

### ⚠️ WARNING: No Panic Across FFI

```aria
#[no_mangle]
pub extern "C" fn aria_func() -> i32 {
    // ❌ Don't panic! C can't handle it
    // Use error codes instead
    
    if error_condition {
        return -1;  // ✅ Return error code
    }
    
    return 0;
}
```

---

## Related

- [extern](extern.md) - Extern keyword
- [extern_blocks](extern_blocks.md) - Extern blocks
- [ffi](ffi.md) - FFI overview
- [c_interop](c_interop.md) - C interoperability

---

**Remember**: External functions are **unsafe** - validate everything!
