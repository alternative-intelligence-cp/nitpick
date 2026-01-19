# NIL, NULL, and void

**Category**: Types → Special Values  
**Purpose**: Understanding null/absent values in Aria

---

## Overview

Aria has three related but distinct concepts for representing absence:

---

## `nil` - Null Pointer/Optional

**Purpose**: Represents a null pointer or absent optional value

```aria
// Null pointer
ptr: *i32 = nil;

// Optional value
maybe_value: ?i32 = nil;

// Check for nil
when ptr != nil then
    use_pointer(ptr);
end
```

### Use Cases

```aria
// Pointer initialization
node: *TreeNode = nil;

// Optional return
fn find_user(id: i32) -> ?User {
    // Not found
    return nil;
}

// Nullable struct field
struct Config {
    cache: ?Cache  // Can be nil
}
```

---

## `NULL` - Constant Alias

**Purpose**: Alternative name for `nil` (compatibility/clarity)

```aria
// NULL is just nil
ptr: *Data = NULL;  // Same as nil

// Prefer nil in new code
ptr: *Data = nil;  // ✅ Preferred
```

---

## `void` - No Return Value

**Purpose**: Function returns nothing

```aria
// Function with no return
fn print_hello() -> void {
    stdout << "Hello";
}

// Can omit -> void
fn print_hello() {
    stdout << "Hello";
}
```

---

## Key Differences

| Concept | Type | Purpose | Example |
|---------|------|---------|---------|
| `nil` | Value | Null pointer/optional | `ptr: *i32 = nil` |
| `NULL` | Alias | Same as `nil` | `ptr: *i32 = NULL` |
| `void` | Type | No return value | `fn f() -> void` |

---

## Examples

### nil vs void

```aria
// void - no return value
fn log(msg: string) -> void {
    stdout << msg;
}

// Returns optional - can be nil
fn find(id: i32) -> ?User {
    return nil;  // Not found
}

// Call void function
log("Hello");  // No value returned

// Call optional function
user: ?User = find(42);
when user != nil then
    process(user);
end
```

### Pointer Initialization

```aria
// Always initialize pointers
ptr1: *i32 = nil;  // ✅ Safe
ptr2: *i32;        // ❌ Uninitialized!

// Check before use
when ptr1 != nil then
    value: i32 = *ptr1;  // Safe dereference
end
```

### Optional Values

```aria
// Optional field
struct Person {
    name: string,
    middle_name: ?string  // Can be nil
}

person: Person = {
    name = "Alice",
    middle_name = nil
};

// Check optional
when person.middle_name != nil then
    stdout << person.middle_name;
end
```

---

## Best Practices

### ✅ DO: Use nil for Pointers

```aria
// Initialize to nil
head: *Node = nil;

// Check before dereference
when head != nil then
    process(head.data);
end
```

### ✅ DO: Use ? for Optionals

```aria
// Optional return type
fn get_config() -> ?Config {
    when not file_exists("config.toml") then
        return nil;
    end
    return load_config();
}
```

### ✅ DO: Prefer nil over NULL

```aria
// Modern Aria style
ptr: *Data = nil;  // ✅

// Old style (still works)
ptr: *Data = NULL;  // ✅ but less common
```

### ❌ DON'T: Confuse nil and void

```aria
// Wrong - void is not a value
fn process() -> void {
    return nil;  // ❌ Error!
}

// Right
fn find() -> ?Data {
    return nil;  // ✅ Optional
}
```

### ❌ DON'T: Dereference Without Checking

```aria
ptr: *i32 = get_pointer();
value: i32 = *ptr;  // ❌ Might crash if nil!

// Better
when ptr != nil then
    value: i32 = *ptr;  // ✅ Safe
else
    handle_null();
end
```

---

## nil Coalescing

```aria
// Use ?? operator for defaults
config: ?Config = load_config();
final_config: Config = config ?? default_config();

// Chaining
value: i32 = optional1 ?? optional2 ?? default_value;
```

---

## Safe Navigation

```aria
// Use ?. to safely access optional fields
user: ?User = find_user(id);
name: ?string = user?.name;  // nil if user is nil

// Chaining
email: ?string = user?.contact?.email;
```

---

## Common Patterns

### Linked List

```aria
struct Node {
    data: i32,
    next: *Node  // Can be nil
}

fn traverse(head: *Node) {
    current: *Node = head;
    
    while current != nil {
        process(current.data);
        current = current.next;
    }
}
```

### Cache Lookup

```aria
cache: map<string, Data>;

fn get_cached(key: string) -> ?Data {
    when cache.contains(key) then
        return cache[key];
    end
    return nil;
}

// Use with default
data: Data = get_cached("key") ?? load_from_disk("key");
```

---

## Related

- [Pointers (*)](pointers.md)
- [Optional (?)](../advanced_features/optional.md)
- [Null Coalescing (??)](../operators/null_coalescing.md)
- [Safe Navigation (?.)](../operators/safe_nav.md)

---

**Remember**: 
- **nil** = null value/pointer
- **NULL** = alias for nil  
- **void** = no return value
