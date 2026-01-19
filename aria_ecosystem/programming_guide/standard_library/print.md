# print()

**Category**: Standard Library → I/O  
**Syntax**: `print(value: any, ...)`  
**Purpose**: Print values to stdout

---

## Overview

`print()` outputs values to standard output, adding newline at the end.

---

## Syntax

```aria
print("Hello, World!");
```

---

## Examples

### Print String

```aria
print("Hello!");
// Output: Hello!
```

### Print Numbers

```aria
print(42);
// Output: 42

print(3.14);
// Output: 3.14
```

### Print Multiple Values

```aria
print("Name:", "Alice", "Age:", 30);
// Output: Name: Alice Age: 30
```

### Print Variables

```aria
name: string = "Bob";
age: i32 = 25;

print("Name:", name, "Age:", age);
// Output: Name: Bob Age: 25
```

### Print with Interpolation

```aria
name: string = "Alice";
age: i32 = 30;

print("$name is $age years old");
// Output: Alice is 30 years old
```

---

## vs stdout

```aria
// print() - adds newline
print("Hello");
print("World");
// Output:
// Hello
// World

// stdout << - no automatic newline
stdout << "Hello";
stdout << "World";
// Output:
// HelloWorld
```

---

## Formatting

### Print Array

```aria
numbers: []i32 = [1, 2, 3, 4, 5];
print(numbers);
// Output: [1, 2, 3, 4, 5]
```

### Print Struct

```aria
struct Point {
    x: i32,
    y: i32
}

p: Point = {x = 10, y = 20};
print(p);
// Output: Point { x: 10, y: 20 }
```

---

## Print without Newline

```aria
// Use stdout directly
stdout << "Hello";
stdout << " ";
stdout << "World";
// Output: Hello World (no newlines)
```

---

## Best Practices

### ✅ DO: Use for Quick Output

```aria
print("Debug: value =", value);
```

### ✅ DO: Use for Simple Logging

```aria
print("Processing file:", filename);
```

### ❌ DON'T: Use in Production Code

```aria
// Development
print("User logged in:", username);  // ⚠️ Temporary debug

// Production - use logger
logger.info("User logged in", { username = username });  // ✅
```

---

## Related

- [stdout](../io/stdout.md) - Standard output stream
- [createLogger()](createLogger.md) - Structured logging

---

**Remember**: `print()` adds **newline** - use `stdout <<` for no newline!
