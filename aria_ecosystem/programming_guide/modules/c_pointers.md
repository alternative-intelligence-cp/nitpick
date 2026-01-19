# C Pointers

**Category**: Modules → FFI  
**Purpose**: Working with C-style pointers in FFI

---

## Overview

C pointers are **raw memory addresses** - unsafe but necessary for C interop.

---

## Pointer Types

### Raw Pointers

```aria
*void      // void* - pointer to anything
*u8        // char* / uint8_t*
*i32       // int*
*f64       // double*
```

---

### Const vs Mutable

```aria
*const T   // const T* - read-only
*mut T     // T* - mutable
```

---

## Creating Pointers

### Address-of Operator

```aria
value: i32 = 42;
ptr: *i32 = &value;  // Get address of value
```

---

### From Array

```aria
arr: [i32; 10];
ptr: *i32 = &arr[0];  // Pointer to first element
```

---

### From C

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
}

ptr: *void = extern.malloc(1024);
```

---

## Dereferencing Pointers

```aria
value: i32 = 42;
ptr: *i32 = &value;

// Read through pointer
read: i32 = *ptr;  // 42

// Write through pointer
*ptr = 100;
// Now value == 100
```

---

## Null Pointers

```aria
ptr: *void = NULL;

if ptr == NULL {
    stdout << "Null pointer!";
}
```

⚠️ **Always check for NULL before dereferencing!**

---

## Pointer Arithmetic

```aria
arr: [i32; 10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
ptr: *i32 = &arr[0];

// Access elements
first: i32 = *ptr;           // arr[0] = 1
second: i32 = *(ptr + 1);    // arr[1] = 2
third: i32 = *(ptr + 2);     // arr[2] = 3

// Move pointer
ptr = ptr + 5;
value: i32 = *ptr;  // arr[5] = 6
```

---

## Passing Pointers to C

### Simple Pointer

```aria
extern "C" {
    fn process(ptr: *i32);
}

value: i32 = 42;
extern.process(&value);
```

---

### Array Pointer

```aria
extern "C" {
    fn process_array(arr: *i32, len: usize);
}

arr: [i32; 10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
extern.process_array(&arr[0], 10);
```

---

### Buffer Pointer

```aria
extern "C" {
    fn read_data(buffer: *mut u8, size: usize) -> i32;
}

buffer: [u8; 1024];
bytes_read: i32 = extern.read_data(&buffer[0], 1024);
```

---

## Receiving Pointers from C

### Return Value

```aria
extern "C" {
    fn get_data() -> *i32;
}

ptr: *i32 = extern.get_data();
if ptr != NULL {
    value: i32 = *ptr;
}
```

---

### Output Parameter

```aria
extern "C" {
    fn get_value(out: *mut i32) -> i32;
}

value: i32;
Result: i32 = extern.get_value(&value);
if result == 0 {
    stdout << "Value: $value";
}
```

---

## String Pointers

### C String (*u8)

```aria
extern "C" {
    fn strlen(s: *u8) -> usize;
    fn strcmp(s1: *u8, s2: *u8) -> i32;
}

// Null-terminated string
c_str: *u8 = "Hello\0";
len: usize = extern.strlen(c_str);  // 5
```

---

### String Buffers

```aria
extern "C" {
    fn strcpy(dest: *mut u8, src: *u8) -> *u8;
}

src: *u8 = "Hello\0";
dest: [u8; 100];
extern.strcpy(&dest[0], src);
```

---

## Memory Allocation

### C malloc/free

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

### Typed Allocation

```aria
extern "C" {
    fn malloc(size: usize) -> *void;
    fn free(ptr: *void);
}

// Allocate array of 10 i32s
ptr: *i32 = extern.malloc(10 * sizeof(i32)) as *i32;

// Use as array
ptr[0] = 1;
ptr[1] = 2;
ptr[2] = 3;

// Free
extern.free(ptr as *void);
```

---

## Pointer Casts

```aria
// void* to typed pointer
void_ptr: *void = extern.malloc(100);
int_ptr: *i32 = void_ptr as *i32;

// Typed pointer to void*
int_ptr: *i32 = &value;
void_ptr: *void = int_ptr as *void;

// Const to mut (unsafe!)
const_ptr: *const i32 = &value;
mut_ptr: *mut i32 = const_ptr as *mut i32;
```

---

## Pointer Safety

### ✅ DO: Check for NULL

```aria
ptr: *void = extern.c_function();
if ptr == NULL {
    return Err("Null pointer");  // ✅
}
```

---

### ✅ DO: Validate Before Dereferencing

```aria
if ptr != NULL && is_valid_address(ptr) {
    value: i32 = *ptr;  // ✅ Safe
}
```

---

### ⚠️ WARNING: Uninitialized Pointers

```aria
ptr: *i32;  // ⚠️ Uninitialized!
value: i32 = *ptr;  // ❌ CRASH

// Initialize first
ptr: *i32 = &some_value;  // ✅
```

---

### ⚠️ WARNING: Dangling Pointers

```aria
ptr: *i32 = extern.malloc(sizeof(i32));
extern.free(ptr);
value: i32 = *ptr;  // ❌ Use after free!
```

---

### ⚠️ WARNING: Buffer Overruns

```aria
arr: [i32; 10];
ptr: *i32 = &arr[0];
value: i32 = *(ptr + 100);  // ❌ Out of bounds!
```

---

## Common Patterns

### Iterating with Pointers

```aria
arr: [i32; 10] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
ptr: *i32 = &arr[0];
end: *i32 = &arr[10];

while ptr < end {
    stdout << *ptr;
    ptr = ptr + 1;
}
```

---

### Double Pointers

```aria
extern "C" {
    fn get_buffer(out: **u8, size: *mut usize) -> i32;
}

buffer: *u8;
size: usize;
Result: i32 = extern.get_buffer(&buffer, &size);

if result == 0 && buffer != NULL {
    // Use buffer...
}
```

---

### Function Pointers

```aria
// C callback type
callback_t = extern "C" fn(i32) -> i32;

extern "C" {
    fn register_callback(cb: callback_t);
}

#[no_mangle]
extern "C" fn my_callback(x: i32) -> i32 {
    return x * 2;
}

extern.register_callback(my_callback);
```

---

## Best Practices

### ✅ DO: Wrap Unsafe Pointer Operations

```aria
pub fn safe_read(ptr: *i32) -> Result<i32> {
    if ptr == NULL {
        return Err("Null pointer");
    }
    
    // Additional validation...
    
    return Ok(*ptr);
}
```

### ✅ DO: Track Pointer Ownership

```aria
// Document who owns the pointer
// Caller must free this pointer
fn allocate() -> *void {
    return extern.malloc(1024);
}
```

### ⚠️ WARNING: Raw Pointers are Unsafe

```aria
// No bounds checking
// No null checking
// No lifetime tracking
// YOU are responsible for safety!
```

---

## Related

- [c_interop](c_interop.md) - C interoperability
- [ffi](ffi.md) - FFI overview
- [libc_integration](libc_integration.md) - libc integration

---

**Remember**: Pointers are **powerful** but **dangerous** - validate everything!
