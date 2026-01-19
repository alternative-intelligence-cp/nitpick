# The `void` Type

**Category**: Types → Special  
**Syntax**: `void`  
**Purpose**: Absence of a return value

---

## Overview

`void` represents the **absence of a value**. Used for functions that don't return anything.

---

## Function Returns

```aria
// Function that returns nothing
fn print_message(msg: string) -> void {
    stdout << msg;
    // No return statement needed
}

// Explicit return
fn log_error(err: string) -> void {
    stderr << err;
    return;  // Optional
}
```

---

## Can be Omitted

```aria
// void can be omitted
fn greet(name: string) {
    stdout << "Hello, $name!";
}

// Equivalent to
fn greet(name: string) -> void {
    stdout << "Hello, $name!";
}
```

---

## Use Cases

### Side Effects Only

```aria
// Modify state, no return
fn increment_counter() -> void {
    counter += 1;
}
```

### I/O Operations

```aria
fn write_file(path: string, data: string) -> void {
    file: File = File.open(path, "w");
    file.write(data);
    file.close();
}
```

---

## Best Practices

### ✅ DO: Omit for Clarity

```aria
// Clear without -> void
fn process() {
    ...
}
```

### ✅ DO: Use for Side Effects

```aria
// Function changes state
fn update_display() -> void {
    screen.refresh();
}
```

### ❌ DON'T: Try to Use the Value

```aria
fn do_something() -> void {
    ...
}

result := do_something();  // ❌ Error - void has no value
```

---

## Difference from nil

| Type | Purpose | Example |
|------|---------|---------|
| `void` | No return value | `fn print() -> void` |
| `nil` | Null pointer/optional | `ptr: *i32 = nil;` |

---

## Related

- [nil](nil_null_void.md) - Null value
- [Result](result.md) - For error handling
- [Functions](../functions/README.md)

---

**Remember**: `void` means **no return value**, `nil` means **null**!
