# Semicolons

**Category**: Advanced Features → Syntax  
**Purpose**: Statement termination

---

## Overview

Semicolons `;` **terminate statements** in Aria.

---

## Required Semicolons

```aria
// Variable declarations
x: i32 = 42;
name: string = "Alice";

// Expression statements
print("Hello");
do_work();

// Multiple statements
x: i32 = 10;
y: i32 = 20;
z: i32 = x + y;
```

---

## Optional Semicolons

```aria
// Last expression in block (return value)
fn get_value() -> i32 {
    return 42  // No semicolon needed (but allowed)
}

fn calculate() -> i32 {
    x: i32 = 10;
    y: i32 = 20;
    x + y  // No semicolon - this is the return value
}

// With semicolon returns void
fn no_return() {
    x: i32 = 42;  // Semicolon required
}
```

---

## Semicolons in Blocks

```aria
fn example() {
    x: i32 = 42;        // Semicolon required
    y: i32 = x * 2;     // Semicolon required
    
    // Last expression becomes return value
    if x > 10 {
        y                // No semicolon - returned
    } else {
        0                // No semicolon - returned
    }
}
```

---

## Multiple Statements on One Line

```aria
// Allowed but discouraged
x: i32 = 10; y: i32 = 20; z: i32 = x + y;

// Preferred - one per line
x: i32 = 10;
y: i32 = 20;
z: i32 = x + y;
```

---

## Semicolons in Loops

```aria
// For loop - semicolons separate parts
for i: i32 = 0; i < 10; i += 1 {
    stdout << i;  // Statement needs semicolon
}

// While loop
while condition {
    do_work();   // Statement needs semicolon
    check();     // Statement needs semicolon
}
```

---

## Return Statements

```aria
fn explicit_return() -> i32 {
    return 42;  // Semicolon required
}

fn implicit_return() -> i32 {
    42  // No semicolon - expression value returned
}

fn early_return() -> i32 {
    if error {
        return 0;  // Semicolon required
    }
    42  // No semicolon - final expression
}
```

---

## Common Patterns

### Variable Declaration and Initialization

```aria
x: i32 = 10;
y: i32 = 20;
Result: i32 = x + y;
```

---

### Function Calls

```aria
initialize();
process_data();
cleanup();
```

---

### Expression vs Statement

```aria
// Expression (no semicolon) - value used
value: i32 = if condition { 10 } else { 20 };

// Statement (semicolon) - value discarded
if condition { 10 } else { 20 };  // Result ignored
```

---

### Block Returns

```aria
fn calculate() -> i32 {
    x: i32 = 10;
    y: i32 = 20;
    
    // Last expression is return value (no semicolon)
    x + y
}

fn calculate_void() {
    x: i32 = 10;
    y: i32 = 20;
    
    // Semicolon makes it a statement (no return value)
    x + y;
}
```

---

## Best Practices

### ✅ DO: Terminate Statements

```aria
fn proper() {
    initialize();
    x: i32 = compute();
    process(x);
    cleanup();
}
```

### ✅ DO: Omit Trailing Semicolon for Returns

```aria
fn get_value() -> i32 {
    x: i32 = 42;
    x  // No semicolon - this is returned
}

fn get_conditional() -> i32 {
    if condition {
        10  // No semicolon
    } else {
        20  // No semicolon
    }
}
```

### ✅ DO: One Statement Per Line

```aria
// ✅ Good
x: i32 = 10;
y: i32 = 20;
z: i32 = x + y;

// ❌ Hard to read
x: i32 = 10; y: i32 = 20; z: i32 = x + y;
```

### ⚠️ WARNING: Return Value vs Statement

```aria
fn confusing() -> i32 {
    x: i32 = 42;
    x;  // ⚠️ Semicolon means no return! Returns void.
}

fn clear() -> i32 {
    x: i32 = 42;
    x  // ✅ No semicolon - returns x
}
```

### ❌ DON'T: Forget Semicolons on Statements

```aria
// ❌ Error - missing semicolons
fn bad() {
    x: i32 = 10  // Error!
    y: i32 = 20  // Error!
    print(x)     // Error!
}

// ✅ Correct
fn good() {
    x: i32 = 10;
    y: i32 = 20;
    print(x);
}
```

---

## Edge Cases

### Empty Statement

```aria
fn example() {
    ;  // Empty statement (valid but useless)
    
    if condition {
        ;  // No-op
    }
}
```

---

### Match Expressions

```aria
// No semicolon - match is an expression
Result: i32 = match value {
    0 => 10,
    1 => 20,
    _ => 30,
};  // Semicolon here ends the declaration

// As statement
match value {
    0 => process0(),
    1 => process1(),
    _ => process_default(),
};  // Semicolon terminates match statement
```

---

### Block Expressions

```aria
// Block as expression
x: i32 = {
    a: i32 = 10;
    b: i32 = 20;
    a + b  // No semicolon - return value
};  // Semicolon ends declaration

// Block as statement
{
    a: i32 = 10;
    b: i32 = 20;
    print(a + b);  // Semicolon in statement
};  // Optional semicolon after block
```

---

## Related

- [colons](colons.md) - Type annotations
- [brace_delimited](brace_delimited.md) - Block syntax

---

**Remember**: Semicolons **end statements** but **omit** them on final expressions to return values!
