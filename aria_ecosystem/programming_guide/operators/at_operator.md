# The `@` Operator (Address-of and Pointers)

**Category**: Operators → Memory & Pointers  
**Syntax**: `@variable` (address-of), `type@` (pointer type)  
**Purpose**: Pointer operations in Aria native code

---

## Overview

The `@` operator is Aria's native pointer syntax, used for:

1. **Pointer type declarations**: `int64@`, `string@`
2. **Address-of operator**: `@variable` (get memory address)

**Philosophy**: Clean native syntax for pointers, distinct from C's `*` which is reserved for `extern` FFI blocks.

---

## Critical Distinction: `@` vs `*`

### Aria Native Code: Use `@`

```aria
// Pointer type: int64@
wild int64@:ptr = @value;

// Get address
int64:x = 42;
int64@:px = @x;
```

### Extern Blocks Only: Use `*`

```aria
// C FFI: Use * for C library compatibility
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
}
```

**Rule**: `@` for Aria code, `*` for C FFI compatibility in `extern` blocks.

---

## Pointer Type Declaration

### Syntax: `type@`

```aria
// Pointer to int64
int64@:ptr;

// Pointer to string
string@:strPtr;

// Pointer to custom type
MyStruct@:objPtr;
```

### Memory Allocation Strategies

Pointers typically use `wild` (manual memory management):

```aria
// Manual allocation
wild int64@:ptr = aria.alloc(sizeof(int64));
*ptr = 42;  // Dereference TBD
aria.free(ptr);
```

Can also use `gc` (garbage collected) or `wildx` (executable):

```aria
// Garbage collected pointer
gc int64@:ptr = aria.alloc(sizeof(int64));
// No manual free needed

// Executable memory (JIT)
wildx byte@:code = aria.allocx(1024);
```

---

## Address-Of Operator: `@variable`

### Get Address of Variable

```aria
int64:value = 100;
int64@:ptr = @value;  // ptr now points to value

// Can pass address to functions
processPointer(@value);
```

### Address of Array Elements

```aria
int64[10]:array;
int64@:firstElement = @array[0];
int64@:fifthElement = @array[4];
```

### Address of Struct Fields

```aria
%STRUCT Person {
    string:name,
    int64:age
}

Person:p = { "Alice", 30 };
string@:namePtr = @p.name;
int64@:agePtr = @p.age;
```

---

## Dereference Syntax (TBD)

**Note**: The dereference syntax is currently **under design** in Aria's type system.

Possible future syntax:

```aria
// Tentative (not finalized)
int64@:ptr = @value;
int64:x = *ptr;  // Dereference to read value
*ptr = 42;       // Dereference to write value
```

**Current Status**: Pointer creation works (`@` operator), dereference syntax not yet specified.

---

## Member Access with Pointers

### Direct Member Access: `.`

For non-pointer objects:

```aria
Person:p = { "Bob", 25 };
string:name = p.name;  // Direct access with .
```

### Pointer Member Access: `->`

For pointer objects (dereferences AND accesses):

```aria
Person@:ptr = @p;
string:name = ptr->name;  // Dereference and access in one step

// Equivalent to (if dereference syntax existed):
// string:name = (*ptr).name;
```

**Key**: `->` is syntactic sugar for dereference + member access.

---

## Common Patterns

### Pattern 1: Pass by Reference

```aria
func:increment = void(int64@:ptr) {
    *ptr += 1;  // Modify value at address
};

int64:x = 10;
increment(@x);  // Pass address
// x is now 11
```

### Pattern 2: Manual Memory Management

```aria
func:createBuffer = byte@(int64:size) {
    wild byte@:buffer = aria.alloc(size);
    
    // Initialize buffer
    till(size - 1, 1) {
        buffer[$] = 0;
    }
    
    pass(buffer);
};

// Use buffer
byte@:buf = createBuffer(1024);
defer aria.free(buf);  // Cleanup with defer
```

### Pattern 3: Linked List Node

```aria
%STRUCT Node {
    int64:data,
    Node@:next  // Pointer to next node
}

wild Node@:head = aria.alloc(sizeof(Node));
head->data = 100;
head->next = NIL;  // NULL pointer

wild Node@:second = aria.alloc(sizeof(Node));
second->data = 200;
second->next = NIL;

head->next = second;  // Link nodes
```

### Pattern 4: Array Manipulation

```aria
func:swap = void(int64@:a, int64@:b) {
    int64:temp = *a;
    *a = *b;
    *b = temp;
};

int64[]:array = [5, 2, 8, 1];
swap(@array[0], @array[3]);  // Swap first and last
// array is now [1, 2, 8, 5]
```

---

## Pointer Arithmetic (Future)

**Status**: Under consideration for future specification.

Possible syntax:

```aria
int64[5]:array = [10, 20, 30, 40, 50];
int64@:ptr = @array[0];

// Pointer arithmetic
ptr = ptr + 1;  // Points to array[1]
int64:value = *ptr;  // value = 20
```

**Current**: Not yet implemented in specification.

---

## NIL Pointers (NULL)

### Null Pointer Value

```aria
int64@:ptr = NIL;  // NULL pointer

if (ptr == NIL) {
    print("Pointer is null");
}
```

### Safe Navigation with Pointers

```aria
Person@:ptr = findPerson(id);

// Check before dereferencing
if (ptr != NIL) {
    print(ptr->name);
} else {
    print("Person not found");
}

// Or use safe navigation (if supported)
string:name = ptr?.name ?? "Unknown";
```

---

## Comparison with C Pointers

### Aria `@` Operator

```aria
// Pointer type: int@
int64@:ptr = @value;

// Address-of
int64:x = 42;
ptr = @x;

// Member access
ptr->member;
```

### C `*` Operator

```c
// Pointer type: int*
int *ptr = &value;

// Address-of
int x = 42;
ptr = &x;

// Dereference
int y = *ptr;

// Member access
ptr->member;
```

**Key Difference**: Aria uses `@` for native code, `*` reserved for C FFI.

---

## Memory Allocation Strategies

### `wild` (Manual)

```aria
wild int64@:ptr = aria.alloc(sizeof(int64));
defer aria.free(ptr);  // Manual cleanup
```

### `gc` (Automatic)

```aria
gc int64@:ptr = aria.alloc(sizeof(int64));
// Automatically freed when no references
```

### `wildx` (Executable)

```aria
wildx byte@:code = aria.allocx(1024);
defer aria.freex(code);
// For JIT compilation, self-modifying code
```

---

## Best Practices

### ✅ Use `defer` for Manual Cleanup

```aria
// GOOD: Guaranteed cleanup
wild int64@:ptr = aria.alloc(sizeof(int64));
defer aria.free(ptr);
```

### ✅ Check for NIL Before Dereference

```aria
// GOOD: Safe dereference
if (ptr != NIL) {
    value = *ptr;
}
```

### ✅ Use `gc` for Complex Ownership

```aria
// GOOD: Let GC handle complex lifetimes
gc Node@:tree = buildTree();
// No manual free needed
```

### ❌ Don't Mix `@` and `*` in Native Code

```aria
// WRONG: Don't use C syntax in Aria code
int64*:ptr = &value;  // ❌ Error

// CORRECT: Use Aria syntax
int64@:ptr = @value;  // ✅
```

### ❌ Don't Forget to Free `wild` Memory

```aria
// BAD: Memory leak
wild int64@:ptr = aria.alloc(sizeof(int64));
// ... use ptr ...
// Forgot to free!

// GOOD: Use defer
wild int64@:ptr = aria.alloc(sizeof(int64));
defer aria.free(ptr);
```

---

## Safety Considerations

### Dangling Pointers

```aria
// DANGER: Dangling pointer
int64@:getDanglingPtr = func() {
    int64:local = 42;
    pass(@local);  // ⚠️ Returns address of stack variable!
}

int64@:ptr = getDanglingPtr();
// ptr points to invalid memory
```

**Solution**: Use `wild` or `gc` allocation for returned pointers.

### Double Free

```aria
// DANGER: Double free
wild int64@:ptr = aria.alloc(sizeof(int64));
aria.free(ptr);
aria.free(ptr);  // ⚠️ Undefined behavior
```

**Solution**: Set to NIL after freeing:

```aria
wild int64@:ptr = aria.alloc(sizeof(int64));
aria.free(ptr);
ptr = NIL;  // Prevent double free
```

---

## Use Cases

### Use Case 1: Data Structures

```aria
// Binary tree node
%STRUCT TreeNode {
    int64:value,
    TreeNode@:left,
    TreeNode@:right
}

wild TreeNode@:createNode = func(int64:val) {
    wild TreeNode@:node = aria.alloc(sizeof(TreeNode));
    node->value = val;
    node->left = NIL;
    node->right = NIL;
    pass(node);
};
```

### Use Case 2: Output Parameters

```aria
func:divmod = bool(int64:a, int64:b, int64@:quotient, int64@:remainder) {
    if (b == 0) {
        pass(false);
    }
    
    *quotient = a / b;
    *remainder = a % b;
    pass(true);
};

int64:q, r;
if (divmod(17, 5, @q, @r)) {
    print(`&{q} remainder &{r}`);  // 3 remainder 2
}
```

### Use Case 3: C Library Interop

```aria
extern "libc" {
    func:malloc = void*(uint64:size);
    func:free = void(void*:ptr);
    func:memcpy = void*(void*:dest, void*:src, uint64:n);
}

// Call C functions with C pointer syntax
wild void*:buffer = malloc(1024);
defer free(buffer);
```

---

## Advanced Examples

### Example 1: Generic Swap Function

```aria
func:swapBytes = void(byte@:a, byte@:b, int64:size) {
    till(size - 1, 1) {
        byte:temp = a[$];
        a[$] = b[$];
        b[$] = temp;
    }
};

// Use for any type
int64:x = 10, y = 20;
swapBytes(@x as byte@, @y as byte@, sizeof(int64));
```

### Example 2: Reference Counting (Manual)

```aria
%STRUCT RefCounted {
    int64:refCount,
    byte@:data
}

func:addRef = void(RefCounted@:obj) {
    obj->refCount++;
};

func:release = void(RefCounted@:obj) {
    obj->refCount--;
    if (obj->refCount == 0) {
        aria.free(obj->data);
        aria.free(obj);
    }
};
```

### Example 3: Pointer to Function (Future)

```aria
// Function pointer type (tentative)
func:(int64)(int64)@:operation;

func:double = int64(int64:x) { pass(x * 2); };
func:triple = int64(int64:x) { pass(x * 3); };

operation = @double;
int64:result = operation(5);  // 10

operation = @triple;
result = operation(5);  // 15
```

---

## Related Topics

- [Pointers Guide](../types/pointers.md) - Comprehensive pointer documentation
- [Memory Allocation](../memory_model/allocation.md) - wild, gc, stack, wildx
- [defer Statement](../memory_model/defer.md) - RAII cleanup
- [NIL vs NULL](../types/nil_null_void.md) - Null pointer semantics
- [extern Blocks](../modules/extern.md) - C FFI with `*` syntax
- [Member Access](member_access.md) - `.` vs `->` operators

---

**Status**: Comprehensive (dereference syntax TBD)  
**Specification**: aria_specs.txt Lines 179-202  
**Unique Feature**: `@` for native code, `*` for C FFI only
