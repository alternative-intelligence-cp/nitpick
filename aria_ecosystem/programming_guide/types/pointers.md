# Pointer Syntax - @ vs * Distinction

**Category**: Types → Pointers  
**Operators**: `@`, `*`, `->`  
**Purpose**: Understanding Aria native pointers vs C FFI pointers

---

## Quick Reference

| Context | Pointer Declaration | Address-Of | Dereference | Member Access |
|---------|-------------------|------------|-------------|---------------|
| **Aria Native** | `int64@:ptr` | `@variable` | TBD | `ptr->member` or `.` |
| **Extern Blocks** | `int64*:ptr` | N/A | N/A | C-style |

**CRITICAL RULE**: `@` is for Aria native code, `*` is ONLY for extern blocks (C FFI).

---

## Aria Native Pointer Syntax

### Declaration with `@`

In Aria native code, the `@` operator is used to declare pointer types:

```aria
// Basic pointer declarations
int64@:ptr;           // Pointer to int64
string@:str_ptr;      // Pointer to string
obj@:obj_ptr;         // Pointer to obj
tbb8@:tbb_ptr;        // Pointer to tbb8

// Arrays of pointers
int64@:ptr_array[10]; // Array of 10 int64 pointers

// Pointers to pointers
int64@@:ptr_to_ptr;   // Pointer to pointer to int64
```

### Getting Address with `@`

The `@` operator is also used to get the address of a variable:

```aria
// Get address of a variable
int64:value = 42;
int64@:ptr = @value;   // ptr now points to value

// Get address of array element
int64:arr[5] = [1, 2, 3, 4, 5];
int64@:elem_ptr = @arr[2];  // Points to arr[2]

// Get address of struct field
struct:Point = {
    int64:x;
    int64:y;
};

Point:p = {10, 20};
int64@:x_ptr = @p.x;  // Points to p.x field
```

### Dereference Syntax

**Current Status**: Dereference syntax is TBD (To Be Determined) in the type system design.

Possible future syntax (not yet implemented):
```aria
// Potential future syntax (not final!)
int64@:ptr = @value;
int64:deref = ptr^;   // or ptr*, or ^ptr - TBD
```

For now, use direct member access or pass pointers to functions.

### Member Access

Two operators for accessing members through pointers:

#### 1. `->` Operator (Pointer Member Access)

Dereferences the pointer AND accesses the member (C-style):

```aria
struct:User = {
    string:name;
    int64:age;
};

wild User@:user_ptr = aria.alloc<User>(1);
user_ptr->name = "Alice";  // Dereference and access name
user_ptr->age = 30;        // Dereference and access age

print(user_ptr->name);     // Prints: Alice
```

#### 2. `.` Operator (Direct Member Access)

For objects (not pointers), use direct `.` access:

```aria
User:user = {name: "Bob", age: 25};
print(user.name);  // Direct access (no pointer)
```

#### Unified Access (Advanced)

In the future, Aria may unify `.` and `->` like Rust/Go, auto-dereferencing pointers when using `.`. Currently, follow the C-style distinction:

- Use `->` for pointer member access
- Use `.` for direct object member access

---

## Extern Block Pointer Syntax (C FFI)

### C-Style `*` for FFI Compatibility

When interfacing with C libraries in `extern` blocks, use C-style `*` for pointers:

```aria
extern "libc" {
    // C-style pointer types
    func:malloc = void*(uint64:size);      // Returns void*
    func:free = void(void*:ptr);           // Takes void*
    func:memcpy = void*(void*:dest, void*:src, uint64:n);
    
    // Specific C pointer types
    func:strlen = uint64(char*:str);       // char* for C strings
    func:read = int64(int32:fd, void*:buf, uint64:count);
}
```

### Generic `void*` Pointers

`void*` is a special C pointer type for generic memory:

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
}

// Calling C functions with void*
void*:raw_memory = malloc(1024);
if (raw_memory == NULL) {
    fail(1);
}
defer free(raw_memory);
```

### Converting Between Aria and C Pointers

```aria
// Aria native allocation
wild int64@:aria_ptr = aria.alloc<int64>(100);

// Passing to C FFI (requires explicit cast - TBD)
extern "libc" {
    func:free = void(void*:ptr);
}

// When calling C free, conversion handled by compiler
free(aria_ptr);  // Compiler converts int64@ to void*
```

---

## Memory Allocation Contexts

### Wild Pointers (`wild`)

Manual memory management with `@` pointers:

```aria
// Allocate wild memory
wild int64@:ptr = aria.alloc<int64>(100);

if (ptr == NULL) {
    stderr.write("Allocation failed");
    fail(1);
}

// Must manually free
defer aria.free(ptr);

// Use pointer
ptr->field = 42;  // If int64 were a struct
// or just use ptr directly for primitives
```

### GC Pointers (`gc`)

Garbage-collected memory:

```aria
// Allocate GC memory
gc obj@:obj_ptr = aria.gc_alloc<obj>();

// No manual free needed - GC handles it
obj_ptr->field = value;

// GC cleans up automatically when no references remain
```

### Stack Pointers

Taking address of stack-allocated variables:

```aria
int64:stack_value = 42;
int64@:stack_ptr = @stack_value;

// stack_ptr valid only while stack_value is in scope
print(*stack_ptr);  // Access through pointer (syntax TBD)
```

---

## Pointer Arithmetic (Wild Pointers)

### Offsetting Pointers

```aria
wild int64@:array_ptr = aria.alloc<int64>(10);
defer aria.free(array_ptr);

// Access elements (syntax may vary)
int64@:first = array_ptr;         // Points to array_ptr[0]
int64@:second = array_ptr + 1;    // Points to array_ptr[1] (maybe)
int64@:third = array_ptr + 2;     // Points to array_ptr[2] (maybe)

// Note: Pointer arithmetic semantics TBD in type system
```

---

## Safety Operators with Pointers

### Pinning (`#`)

Pin memory to prevent GC from moving it:

```aria
gc obj@:gc_ptr = aria.gc_alloc<obj>();

// Pin for FFI call
#gc_ptr;  // Pin prevents GC movement
callCFunction(gc_ptr);
// Automatically unpinned after scope
```

### Borrowing (`$`)

Safe reference (immutable or mutable):

```aria
int64:value = 100;

// Immutable borrow
$int64:ref = $value;  // Cannot modify value while ref exists

// Mutable borrow  
$mut int64:mut_ref = $mut value;  // Exclusive mutable access
```

---

## Common Patterns

### Pattern 1: Allocate, Check, Defer

```aria
func:processData = Result<int64>(uint64:size) {
    wild int64@:buffer = aria.alloc<int64>(size);
    
    if (buffer == NULL) {
        fail(1);  // Allocation failed
    }
    defer aria.free(buffer);
    
    // Use buffer...
    buffer->value = 42;
    
    pass(buffer->value);
};
```

### Pattern 2: Pointer to Struct Field

```aria
struct:Config = {
    int64:timeout;
    string:host;
};

Config:cfg = {timeout: 30, host: "localhost"};

// Get pointer to specific field
int64@:timeout_ptr = @cfg.timeout;

// Modify through pointer (syntax TBD)
*timeout_ptr = 60;  // cfg.timeout now 60
```

### Pattern 3: C FFI with Aria Pointers

```aria
extern "mylib" {
    func:process_data = int32(void*:data, uint64:size);
}

// Allocate in Aria
wild int64@:data = aria.alloc<int64>(100);
defer aria.free(data);

// Pass to C (automatic conversion)
int32:status = process_data(data, 100 * sizeof(int64));
```

---

## Comparison Table

| Feature | Aria Native (`@`) | C FFI (`*`) |
|---------|------------------|-------------|
| **Declaration** | `int64@:ptr` | `int32*:ptr` (extern only) |
| **Address-Of** | `@variable` | N/A (C handles) |
| **Dereference** | TBD | N/A (C handles) |
| **Member Access** | `ptr->member` | Varies by C library |
| **Generic Pointer** | `obj@` | `void*` |
| **Null Check** | `ptr == NULL` | `ptr == NULL` |
| **Allocation** | `aria.alloc<T>()` | `malloc()` (extern) |
| **Deallocation** | `aria.free(ptr)` | `free(ptr)` (extern) |

---

## Examples

### Aria Native Pointers

```aria
// Stack variable
int64:value = 100;

// Get address
int64@:ptr = @value;

// Check validity
if (ptr == NULL) {
    fail(1);
}

// Member access (for structs)
struct:Point = { int64:x; int64:y; };
Point:p = {10, 20};
Point@:p_ptr = @p;

print(p_ptr->x);  // 10
print(p_ptr->y);  // 20
```

### C FFI Pointers

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
    func:memset = void*(void*:s, int32:c, uint64:n);
}

// Allocate via C malloc
void*:buffer = malloc(1024);
if (buffer == NULL) {
    fail(1);
}
defer free(buffer);

// Use C memset
memset(buffer, 0, 1024);  // Zero out buffer
```

### Mixing Aria and C Pointers

```aria
// Aria allocation
wild int64@:aria_buf = aria.alloc<int64>(256);
defer aria.free(aria_buf);

// Pass to C library
extern "mylib" {
    func:process = int32(void*:data, uint64:size);
}

int32:result = process(aria_buf, 256 * sizeof(int64));
```

---

## Gotchas and Edge Cases

### ❌ Don't Use `*` in Aria Native Code

```aria
// WRONG: * is only for extern blocks
int64*:ptr = malloc(100);  // ❌ SYNTAX ERROR

// CORRECT: Use @ for Aria pointers
wild int64@:ptr = aria.alloc<int64>(100);
```

### ❌ Don't Use `@` in Extern Blocks

```aria
// WRONG: Extern blocks use C-style *
extern "libc" {
    func:malloc = obj@(uint64:size);  // ❌ SYNTAX ERROR
}

// CORRECT: Use * for C FFI
extern "libc" {
    func:malloc = void*(uint64:size);  // ✅
}
```

### ✅ Pointer Validity Checks

```aria
// Always check for NULL after allocation
wild int64@:ptr = aria.alloc<int64>(1000);

// WRONG: Use without checking
ptr->value = 42;  // May crash if allocation failed

// CORRECT: Check first
if (ptr == NULL) {
    fail(1);
}
ptr->value = 42;  // Safe
```

### ✅ Defer for Cleanup

```aria
// WRONG: Manual free (may leak on early return)
wild int64@:ptr = aria.alloc<int64>(100);
// ... code that might return early ...
aria.free(ptr);  // Might not be reached!

// CORRECT: defer ensures cleanup
wild int64@:ptr = aria.alloc<int64>(100);
defer aria.free(ptr);  // Always runs at scope exit
// ... code can return safely ...
```

---

## Future Evolution

### Unified Member Access

Future Aria versions may unify `.` and `->` operators:

```aria
// Future: auto-dereference with .
Point@:ptr = @point;
print(ptr.x);  // Same as ptr->x (auto-dereference)
```

### Dereference Syntax

Future Aria versions will define explicit dereference syntax:

```aria
// Possible future syntax
int64@:ptr = @value;
int64:deref = ptr^;  // or *ptr, or ^ptr - TBD
```

---

## Related Topics

- [Memory Allocation](../memory_model/allocation.md) - wild, gc, stack allocation
- [defer Keyword](../memory_model/defer.md) - Automatic cleanup with RAII
- [Null Pointers](nil_null_void.md) - NULL vs NIL distinction
- [FFI System](../modules/extern.md) - C interop and extern blocks
- [Pinning Operator](../operators/pin.md) - `#` for GC pinning
- [Borrowing Operator](../operators/borrow.md) - `$` for safe references

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 179-202  
**Critical Rule**: `@` for Aria native, `*` for extern blocks ONLY
