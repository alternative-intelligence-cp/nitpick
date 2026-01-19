# C Interoperability

**Category**: Modules → FFI  
**Purpose**: Detailed guide to working with C code

---

## Overview

C interoperability allows Aria to seamlessly **call C libraries** and be **called from C**.

---

## Type Compatibility

### Integer Types

```aria
// Aria -> C mapping
i8  -> int8_t  / signed char
i16 -> int16_t / short
i32 -> int32_t / int
i64 -> int64_t / long long

u8  -> uint8_t  / unsigned char
u16 -> uint16_t / unsigned short
u32 -> uint32_t / unsigned int
u64 -> uint64_t / unsigned long long
```

---

### Floating Point

```aria
f32 -> float
f64 -> double
```

---

### Pointers

```aria
*void      -> void*
*u8        -> char* / uint8_t*
*i32       -> int*
*const T   -> const T*
*mut T     -> T*
```

---

### Booleans

```aria
// C has no bool (pre-C99), use int
extern "C" {
    fn c_bool_func() -> i32;  // 0 = false, 1 = true
}
```

---

## Calling C Libraries

### Standard C Library (libc)

```aria
extern "C" {
    fn printf(format: *u8, ...) -> i32;
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
}

// Use them
extern.printf("Hello, %s!\n", "World");

ptr: *void = extern.malloc(1024);
// ... use ptr ...
extern.free(ptr);

len: usize = extern.strlen("Hello");
```

---

### Math Library (libm)

```aria
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
    fn floor(x: f64) -> f64;
    fn ceil(x: f64) -> f64;
}

root: f64 = extern.sqrt(16.0);     // 4.0
power: f64 = extern.pow(2.0, 10.0); // 1024.0
```

---

## C Structs

### Simple Struct

```c
// C struct
struct Point {
    int x;
    int y;
};
```

```aria
// Aria equivalent
#[repr(C)]
struct Point {
    x: i32,
    y: i32,
}

extern "C" {
    fn create_point(x: i32, y: i32) -> Point;
    fn distance(p1: Point, p2: Point) -> f64;
}
```

---

### Struct with Padding

```c
struct Data {
    char flag;
    int value;
};
```

```aria
#[repr(C)]
struct Data {
    flag: u8,
    // Compiler adds padding here
    value: i32,
}
```

`#[repr(C)]` ensures C-compatible layout.

---

## Passing Data to C

### By Value

```aria
extern "C" {
    fn process(value: i32) -> i32;
}

Result: i32 = extern.process(42);
```

---

### By Pointer

```aria
extern "C" {
    fn modify(ptr: *mut i32);
}

value: i32 = 10;
extern.modify(&value);  // C modifies value
```

---

### Arrays

```aria
extern "C" {
    fn process_array(arr: *i32, len: usize) -> i32;
}

arr: [i32; 10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
Result: i32 = extern.process_array(&arr[0], 10);
```

---

### Strings

```aria
extern "C" {
    fn process_string(s: *u8) -> i32;
}

// Must be null-terminated for C!
c_str: *u8 = "Hello\0";
Result: i32 = extern.process_string(c_str);
```

---

## Receiving Data from C

### Return Values

```aria
extern "C" {
    fn get_value() -> i32;
    fn allocate_data() -> *void;
}

value: i32 = extern.get_value();
ptr: *void = extern.allocate_data();
```

---

### Output Parameters

```aria
extern "C" {
    fn get_dimensions(width: *mut i32, height: *mut i32);
}

width: i32;
height: i32;
extern.get_dimensions(&width, &height);
stdout << "Size: $width x $height";
```

---

## Memory Management

### C Allocates, C Frees

```aria
extern "C" {
    fn c_create_object() -> *void;
    fn c_destroy_object(obj: *void);
}

obj: *void = extern.c_create_object();
// ... use obj ...
extern.c_destroy_object(obj);
```

---

### Aria Allocates for C

```aria
#[no_mangle]
pub extern "C" fn aria_allocate(size: usize) -> *void {
    // Use C malloc so C can free it
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

## Callbacks

### C Library Expects Callback

```c
// C header
typedef int (*callback_t)(int);
void register_callback(callback_t cb);
```

```aria
// Aria callback
#[no_mangle]
extern "C" fn my_callback(value: i32) -> i32 {
    return value * 2;
}

// Register it
extern "C" {
    fn register_callback(cb: extern "C" fn(i32) -> i32);
}

extern.register_callback(my_callback);
```

---

## Error Handling

### C Error Codes

```aria
extern "C" {
    fn c_operation() -> i32;  // Returns 0 on success
}

Result: i32 = extern.c_operation();
if result != 0 {
    return Err("Operation failed with code: $result");
}
```

---

### errno

```aria
extern "C" {
    static errno: i32;
    fn some_operation() -> i32;
}

Result: i32 = extern.some_operation();
if result == -1 {
    error_code: i32 = extern.errno;
    return Err("Error code: $error_code");
}
```

---

## Platform-Specific Code

```aria
#[cfg(target_os = "linux")]
extern "C" {
    fn linux_function();
}

#[cfg(target_os = "windows")]
extern "system" {  // Note: "system" ABI for Windows
    fn windows_function();
}

#[cfg(target_os = "macos")]
extern "C" {
    fn macos_function();
}
```

---

## Common Patterns

### File I/O

```aria
extern "C" {
    fn fopen(path: *u8, mode: *u8) -> *void;
    fn fclose(file: *void) -> i32;
    fn fread(ptr: *void, size: usize, count: usize, file: *void) -> usize;
    fn fwrite(ptr: *void, size: usize, count: usize, file: *void) -> usize;
}

file: *void = extern.fopen("data.txt\0", "r\0");
if file == NULL {
    return Err("Failed to open file");
}

buffer: [u8; 1024];
bytes_read: usize = extern.fread(&buffer[0], 1, 1024, file);
extern.fclose(file);
```

---

### Dynamic Libraries

```aria
extern "C" {
    fn dlopen(filename: *u8, flag: i32) -> *void;
    fn dlsym(handle: *void, symbol: *u8) -> *void;
    fn dlclose(handle: *void) -> i32;
}

// Load library
handle: *void = extern.dlopen("libmylib.so\0", 1);  // RTLD_LAZY

// Get function pointer
func: *void = extern.dlsym(handle, "my_function\0");

// ... use function ...

// Close library
extern.dlclose(handle);
```

---

## Best Practices

### ✅ DO: Use #[repr(C)] for Structs

```aria
#[repr(C)]
struct CCompatible {
    a: i32,
    b: f64,
}
```

### ✅ DO: Null-Terminate Strings for C

```aria
c_string: *u8 = "Hello\0";  // ✅ Null terminator
```

### ✅ DO: Validate Pointers

```aria
if ptr == NULL {
    return Err("Null pointer from C");
}
```

### ✅ DO: Match C's Memory Model

```aria
// If C allocates, C must free
ptr: *void = extern.c_allocate();
extern.c_free(ptr);
```

### ⚠️ WARNING: No Panic Across FFI

```aria
#[no_mangle]
pub extern "C" fn aria_func() {
    // ❌ Never panic! Return error code instead
    if error {
        return -1;  // ✅ Error code
    }
}
```

---

## Related

- [ffi](ffi.md) - FFI overview
- [c_pointers](c_pointers.md) - C pointer handling
- [libc_integration](libc_integration.md) - libc integration
- [extern](extern.md) - Extern keyword

---

**Remember**: Match C's **types**, **calling conventions**, and **memory model** exactly!
