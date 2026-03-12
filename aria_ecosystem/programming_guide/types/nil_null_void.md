# NIL vs NULL vs void - Critical Distinctions

**Category**: Types → Special Values  
**Keywords**: `NIL`, `NULL`, `void`  
**Purpose**: Three distinct concepts with different meanings and usage contexts

---

## Quick Reference

| Concept | Type | Usage | Example |
|---------|------|-------|---------|
| **NIL** | Unit type (absence of value) | Aria native optional types | `obj?:user = NIL;` |
| **NULL** | Pointer sentinel (address 0x0) | Pointer initialization/comparison | `int64@:ptr = NULL;` |
| **void** | FFI marker (no return value) | C function signatures only | `extern { func:exit = void(int32:code); }` |

---

## NIL - Aria's Native "Absence of Value"

### Definition

**NIL** represents "no value present" in Aria's type system. It is **not an error**, just the absence of a value.

- **Type**: Special unit type, distinct from all other types
- **Purpose**: Represents optional/maybe types in Aria native code
- **Semantics**: "No value" (not failure, just absence)
- **Different From**: NULL (which is specifically for pointers)

### Usage

```aria
// Optional types (? suffix)
obj?:user = getUser() ?? NIL;  // May or may not have a user
string?:name = user?.name ?? NIL;  // Safe navigation returns NIL if chain breaks
int64?:value = NIL;  // Explicitly no value

// Function returns with NIL
func:findConfig = obj?(string:key) {
    obj?:config = database.lookup(key);
    if (config == NIL) {
        pass(NIL);  // No config found, but not an error
    }
    pass(config);
};

// NIL coalescing with ??
string:name = getName() ?? "Unknown";  // Default if NIL
```

### When to Use NIL

- ✅ Optional return values (may or may not exist)
- ✅ Safe navigation chains (`obj?.field`)
- ✅ Representing "no value" without being an error
- ✅ Null coalescing with `??` operator
- ❌ **NOT** for pointer initialization (use NULL)
- ❌ **NOT** for error states (use ERR or result type)

---

## NULL - C-Style Null Pointer Constant

### Definition

**NULL** represents an invalid or uninitialized pointer (address `0x0`).

- **Type**: Pointer sentinel value
- **Purpose**: Pointer comparisons and initialization
- **Semantics**: "Invalid/no pointer address"
- **Compatible With**: `@` pointer types in Aria

### Usage

```aria
// Pointer initialization
int64@:ptr = NULL;  // Null pointer
string@:str = NULL;

// Pointer comparison
wild int64@:allocation = aria.alloc<int64>(100);
if (allocation == NULL) {
    stderr.write("Allocation failed");
    fail(1);
}

// Dereferencing check
func:processPointer = result(int64@:ptr) {
    if (ptr == NULL) {
        fail(1);  // Can't process null pointer
    }
    
    // Safe to use ptr here
    pass(ptr);
};
```

### When to Use NULL

- ✅ Initializing pointers that don't point anywhere yet
- ✅ Checking if pointer allocation succeeded
- ✅ Comparing pointers for validity
- ✅ Indicating "no pointer" as opposed to "valid address"
- ❌ **NOT** for non-pointer types (use NIL)
- ❌ **NOT** in optional types (use NIL)

---

## void - C FFI "No Return Value" Type

### Definition

**void** represents a C function that doesn't return a value. It is **ONLY** valid in `extern` blocks for FFI compatibility.

- **Type**: Not actually a type in Aria - only an FFI signature marker
- **Purpose**: Compatibility with C libraries that use void returns
- **Restriction**: **CANNOT** be used in Aria native code
- **Scope**: `extern` blocks only

### Usage (Extern Blocks ONLY)

```aria
// VALID: C FFI with void returns
extern "libc" {
    func:free = void(void*:ptr);         // C free() returns void
    func:exit = void(int32:code);        // C exit() returns void
    func:printf = int32(string:fmt, ...); // C printf() returns int (not void)
}

// Calling C void functions
// (internally translated to result type with NIL)
result<NIL>:r = exit(0);  // r.is_error == false if successful

// C-style pointers in extern blocks
extern "mylib" {
    func:malloc = void*(uint64:size);    // C-style void*
    func:process = void(int32*:data);    // C-style int32*
}
```

### ❌ INVALID Usage (Aria Native Code)

```aria
// WRONG: void cannot be used in Aria function signatures
func:ariaFunc = void() {  // ❌ SYNTAX ERROR!
    // ...
};

// WRONG: void cannot be a variable type
void:x = 10;  // ❌ SYNTAX ERROR!

// CORRECT: Aria functions always return result type
func:ariaFunc = result() {
    pass(NIL);  // Aria way: return NIL for "no value"
};
```

### When to Use void

- ✅ **ONLY** in `extern` block function signatures
- ✅ Matching C library function prototypes exactly
- ✅ C void* for generic pointers in FFI
- ❌ **NEVER** in Aria native function signatures
- ❌ **NEVER** as a variable type
- ❌ **NEVER** in regular Aria code

---

## Interplay and Rules

### 1. Aria Native Code

```aria
// Use NIL for "absence of value" semantics
func:getOptional = obj?() {
    bool:found = checkCondition();
    if (!found) {
        pass(NIL);  // No value, not an error
    }
    pass(someValue);
};

// Use NULL for pointer comparisons/initialization
wild int64@:ptr = aria.alloc<int64>(1024);
if (ptr == NULL) {
    fail(1);  // Allocation failed
}
defer aria.free(ptr);

// NEVER use void (syntax error)
// func:invalid = void() { };  // ❌ ERROR!
```

### 2. C FFI (extern blocks)

```aria
extern "libc" {
    // Use void for C functions with no return
    func:exit = void(int32:code);
    
    // Use C-style pointers (void*, int*)
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
}

// NULL works for C pointer parameters
extern "mylib" {
    func:process = int32(void*:data);
}

void*:data = NULL;  // Valid in FFI context
int32:status = process(data);
```

### 3. Type System Summary

| Concept | Has Type? | Usable in Variables? | Usable in Returns? |
|---------|-----------|---------------------|-------------------|
| **NIL** | Yes (unit type) | ✅ Yes (`obj?:x = NIL;`) | ✅ Yes (`pass(NIL);`) |
| **NULL** | Yes (pointer sentinel) | ✅ Yes (`int64@:p = NULL;`) | ✅ Yes (as pointer value) |
| **void** | ❌ No (FFI marker only) | ❌ No (syntax error) | ❌ No (extern only) |

---

## Common Patterns

### Pattern 1: Optional Return with NIL

```aria
func:findUser = obj?(string:username) {
    obj?:user = database.lookup(username);
    
    if (user == NIL) {
        pass(NIL);  // Not found, return NIL
    }
    
    pass(user);
};

// Usage with null coalescing
obj?:user = findUser("alice") ?? createDefaultUser();
```

### Pattern 2: Pointer Validation with NULL

```aria
func:allocateBuffer = Result<int64@>(uint64:size) {
    wild int64@:buffer = aria.alloc<int64>(size);
    
    if (buffer == NULL) {
        fail(1);  // Allocation failed
    }
    
    pass(buffer);
};

// Usage
int64@:buf = allocateBuffer(1024) ? NULL;
if (buf != NULL) {
    defer aria.free(buf);
    // Use buffer...
}
```

### Pattern 3: C FFI with void Returns

```aria
extern "libc" {
    func:free = void(void*:ptr);
}

// Aria wraps C void return as result with NIL
wild int64@:ptr = aria.alloc<int64>(100);
if (ptr != NULL) {
    // Use ptr...
    result<NIL>:r = free(ptr);  // r is a NIL result
}
```

---

## Examples Comparing All Three

### Aria Native - Using NIL for Optional Values

```aria
func:findUser = obj?(string:username) {
    obj?:user = database.lookup(username);
    
    if (user == NIL) {
        pass(NIL);  // No user found, return NIL
    }
    
    pass(user);
};

obj?:res = findUser("bob") ?? NIL;

if (res == NIL) {
    print("User not found");
} else {
    print(`Found user: &{res.name}`);
}
```

### Aria Native - Using NULL for Pointers

```aria
wild int64@:ptr = aria.alloc<int64>(100);

if (ptr == NULL) {
    stderr.write("Allocation failed");
    fail(-1);
}

defer aria.free(ptr);

// Use ptr...
*ptr = 42;
```

### C FFI - Using void for C Function Returns

```aria
extern "libc" {
    func:printf = int32(string:format, ...);  // C returns int
    func:exit = void(int32:code);             // C void return
    func:free = void(void*:ptr);              // C void return, void* param
}

// When calling C void functions, Aria internally returns result with NIL
result<NIL>:r = exit(0);  // r.is_error == false if successful
```

---

## Decision Tree: Which One to Use?

```
Are you writing Aria native code or extern block?
│
├─ Aria Native Code
│  │
│  ├─ Working with pointers (@)?
│  │  └─> Use NULL (int64@:ptr = NULL;)
│  │
│  └─ Working with optional values (?)?
│     └─> Use NIL (obj?:x = NIL;)
│
└─ Extern Block (C FFI)
   │
   ├─ C function returns nothing?
   │  └─> Use void (func:exit = void(int32:code);)
   │
   └─ C function uses generic pointers?
      └─> Use void* (func:malloc = void*(uint64:size);)
```

---

## Gotchas and Common Mistakes

### ❌ Mistake 1: Using void in Aria Code

```aria
// WRONG!
func:myFunc = void() {  // ❌ SYNTAX ERROR
    print("Hello");
}

// CORRECT:
func:myFunc = result() {
    print("Hello");
    pass(NIL);  // Return NIL for "no value"
}
```

### ❌ Mistake 2: Using NULL for Non-Pointers

```aria
// WRONG!
int64:value = NULL;  // ❌ NULL is for pointers only

// CORRECT:
obj?:value = NIL;  // Use NIL for optional non-pointer types
```

### ❌ Mistake 3: Using NIL for Pointer Checks

```aria
// WRONG!
int64@:ptr = aria.alloc<int64>(100);
if (ptr == NIL) {  // ❌ Should use NULL
    fail(1);
}

// CORRECT:
if (ptr == NULL) {  // ✅ NULL for pointers
    fail(1);
}
```

### ✅ Correct Pattern: Each in Its Place

```aria
// Optional value: NIL
obj?:config = loadConfig() ?? NIL;

// Pointer check: NULL
wild int64@:buffer = aria.alloc<int64>(1024);
if (buffer == NULL) { fail(1); }

// C FFI: void
extern "libc" {
    func:exit = void(int32:code);
}
```

---

## Related Topics

- [result Type](result.md) - Aria's primary error-handling mechanism
- [Optional Types](../types/optional.md) - Using `?` suffix for maybe types
- [Pointers](../types/pointers.md) - `@` operator and pointer semantics
- [Safe Navigation](../operators/safe_navigation.md) - `?.` operator
- [Null Coalescing](../operators/null_coalesce.md) - `??` operator
- [FFI System](../modules/extern.md) - C interop and extern blocks

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 47-142  
**Critical Importance**: Mixing these up causes compile errors or semantic bugs
