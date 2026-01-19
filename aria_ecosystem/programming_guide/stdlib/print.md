# The `print()` Function

**Category**: Standard Library → I/O  
**Syntax**: `print(value1, value2, ...)` or `print(template)`  
**Purpose**: Output text to stdout with automatic formatting

---

## Overview

The `print()` function outputs text to the **stdout** stream (standard output). It:

- Accepts **multiple arguments** of any type
- **Automatically converts** values to strings
- Supports **string interpolation** with `&{}`
- Adds **newline** at the end by default

**Philosophy**: Simple, intuitive text output for debugging and user interaction.

---

## Basic Syntax

```aria
// Single value
print("Hello, World!");

// Multiple values (space-separated)
print("Value:", 42);

// String interpolation
int64:x = 100;
print(`x = &{x}`);
```

---

## Example 1: Simple Output

```aria
print("Hello, Aria!");
// Output: Hello, Aria!

print(42);
// Output: 42

print(3.14159);
// Output: 3.14159

print(true);
// Output: true
```

---

## Example 2: Multiple Arguments

```aria
int64:age = 30;
string:name = "Alice";

print("Name:", name, "Age:", age);
// Output: Name: Alice Age: 30

print("Values:", 1, 2, 3, 4, 5);
// Output: Values: 1 2 3 4 5
```

---

## String Interpolation

### Template Literals with `&{}`

```aria
int64:x = 10;
int64:y = 20;
int64:sum = x + y;

print(`&{x} + &{y} = &{sum}`);
// Output: 10 + 20 = 30
```

### Complex Expressions

```aria
int64:a = 5, b = 3;

print(`The product of &{a} and &{b} is &{a * b}`);
// Output: The product of 5 and 3 is 15

print(`Is &{a} > &{b}? &{a > b}`);
// Output: Is 5 > 3? true
```

### Nested Interpolation

```aria
string:name = "Bob";
int64:score = 95;

print(`Player &{name} scored &{score} points (&{score * 100 / 100}%)`);
// Output: Player Bob scored 95 points (95%)
```

---

## Type Conversion

### Automatic String Conversion

```aria
// Integers
print(42);           // "42"
print(-100);         // "-100"

// Floats
print(3.14);         // "3.14"
print(-2.5);         // "-2.5"

// Booleans
print(true);         // "true"
print(false);        // "false"

// TBB types
tbb8:val = 127;
print(val);          // "127"
```

### Arrays

```aria
int64[]:array = [1, 2, 3, 4, 5];
print(array);
// Output: [1, 2, 3, 4, 5]
```

### Structs

```aria
%STRUCT Point {
    int64:x,
    int64:y
}

Point:p = { 10, 20 };
print(p);
// Output: Point { x: 10, y: 20 }
```

---

## Output Streams

### Default: stdout

```aria
// Goes to standard output
print("This goes to stdout");
```

### Redirect to stderr

```aria
// Use stderr stream directly
stderr.write("Error message\n");
```

### Redirect to stddbg

```aria
// Debug output
stddbg.write("Debug info\n");
```

---

## Common Patterns

### Pattern 1: Debug Output

```aria
func:calculateSum = int64(int64:a, int64:b) {
    print(`DEBUG: a = &{a}, b = &{b}`);
    int64:result = a + b;
    print(`DEBUG: result = &{result}`);
    pass(result);
};
```

### Pattern 2: Progress Indicators

```aria
till(100, 1) {
    if ($ % 10 == 0) {
        print(`Progress: &{$}%`);
    }
    processItem($);
}
print("Done!");
```

### Pattern 3: Table Output

```aria
print("Name\tAge\tCity");
print("----\t---\t----");
print(`Alice\t30\tNYC`);
print(`Bob\t25\tSF`);
print(`Charlie\t35\tLA`);
```

### Pattern 4: JSON-like Output

```aria
%STRUCT User {
    string:name,
    int64:age,
    string:email
}

User:u = { "Alice", 30, "alice@example.com" };
print(`{`);
print(`  "name": "&{u.name}",`);
print(`  "age": &{u.age},`);
print(`  "email": "&{u.email}"`);
print(`}`);
```

---

## Formatting Control

### No Newline (Future)

**Status**: Syntax TBD

Possible future syntax:

```aria
// Print without newline (tentative)
printNoNewline("Loading");
printNoNewline(".");
printNoNewline(".");
printNoNewline(".");
print("");  // Newline only
// Output: Loading...
```

### Formatted Numbers (Future)

**Status**: Formatting functions TBD

Possible:

```aria
// Number formatting (tentative)
float64:pi = 3.14159265;
print(format(pi, ".2f"));  // "3.14"
print(format(1000000, ",d"));  // "1,000,000"
```

---

## Performance Considerations

### Buffering

`print()` is **buffered** for efficiency:

```aria
// All buffered until newline
print("Line 1");
print("Line 2");
print("Line 3");
// Flushed to stdout
```

### Explicit Flush (if needed)

```aria
// Ensure immediate output
print("Important message");
stdout.flush();  // Force output now
```

---

## Comparison with Other Languages

### Aria

```aria
print("Hello, World!");
print(`Value: &{x}`);
```

### Python

```python
print("Hello, World!")
print(f"Value: {x}")
```

### JavaScript

```javascript
console.log("Hello, World!");
console.log(`Value: ${x}`);
```

### C

```c
printf("Hello, World!\n");
printf("Value: %d\n", x);
```

### Rust

```rust
println!("Hello, World!");
println!("Value: {}", x);
```

---

## Best Practices

### ✅ Use Interpolation for Clarity

```aria
// GOOD: Clear what's being printed
int64:count = 42;
print(`Found &{count} items`);
```

### ✅ Use for Quick Debugging

```aria
// GOOD: Debug intermediate values
print(`DEBUG: x = &{x}, y = &{y}`);
```

### ✅ Use Multiple Arguments for Simplicity

```aria
// GOOD: Simple multi-value output
print("Name:", name, "Score:", score);
```

### ❌ Don't Overuse in Production

```aria
// BAD: Too many print statements in production code
func:process = void(data) {
    print("Starting process");
    print(`Processing: &{data}`);
    print("Step 1 complete");
    print("Step 2 complete");
    print("Done");
}

// GOOD: Use structured logging
func:process = void(data) {
    logger.info("Processing data", { data });
    // ... work ...
    logger.info("Process complete");
}
```

### ❌ Don't Print Sensitive Data

```aria
// BAD: Exposing credentials
string:password = getPassword();
print(`Password: &{password}`);  // ⚠️ Security risk!

// GOOD: Mask or omit sensitive data
print("Password set successfully");
```

---

## Advanced Examples

### Example 1: Multi-line Output

```aria
func:displayMenu = void() {
    print("=== Main Menu ===");
    print("1. New Game");
    print("2. Load Game");
    print("3. Settings");
    print("4. Quit");
    print("=================");
    print("Choose option: ");
};
```

### Example 2: Data Visualization

```aria
func:displayHistogram = void(int64[]:data) {
    till(data.length - 1, 1) {
        print(`Item &{$}: `);
        loop(1, data[$], 1) {
            printNoNewline("█");  // Tentative
        }
        print("");
    }
};
```

### Example 3: Structured Output

```aria
func:displayUser = void(User:u) {
    print("┌─────────────────────┐");
    print(`│ Name:  &{u.name.padRight(12)}│`);
    print(`│ Age:   &{u.age.toString().padRight(12)}│`);
    print(`│ Email: &{u.email.padRight(12)}│`);
    print("└─────────────────────┘");
};
```

### Example 4: Color Output (Terminal)

```aria
// ANSI color codes (if terminal supports)
string:RED = "\x1b[31m";
string:GREEN = "\x1b[32m";
string:RESET = "\x1b[0m";

print(`&{RED}Error: Something went wrong&{RESET}`);
print(`&{GREEN}Success!&{RESET}`);
```

---

## Error Handling

### Print Cannot Fail

`print()` typically **does not fail** for stdout:

```aria
print("This always works");
// No Result type needed
```

### File/Stream Writing Can Fail

```aria
// Writing to files returns Result
Result[void]:result = writeFile("output.txt", data);

result?
    .onError(err => print(`Failed to write: &{err}`))
    .onSuccess(_ => print("File written successfully"));
```

---

## Related Topics

- [String Interpolation](../types/strings.md#interpolation) - `&{}` syntax
- [stdout Stream](stdout.md) - Standard output stream
- [stderr Stream](stderr.md) - Error output stream
- [stddbg Stream](stddbg.md) - Debug output stream
- [Logging](logging.md) - Structured logging for production
- [writeFile()](writeFile.md) - File output

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 222  
**Unique Feature**: Automatic type conversion and clean interpolation syntax
