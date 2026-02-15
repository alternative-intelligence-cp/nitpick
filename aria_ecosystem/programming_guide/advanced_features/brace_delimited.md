# Brace-Delimited Syntax

**Category**: Advanced Features → Syntax  
**Purpose**: Block delimitation using braces

---

## Overview

Braces `{ }` delimit **blocks** of code in Aria.

---

## Function Bodies

```aria
fn simple() {
    stdout << "Hello";
}

fn with_return() -> i32 {
    return 42;
}

fn multi_statement() {
    x: i32 = 10;
    y: i32 = 20;
    stdout << x + y;
}
```

---

## Control Flow

```aria
// If statements
if condition {
    do_something();
}

if x > 10 {
    stdout << "big";
} else {
    stdout << "small";
}

// While loops
while running {
    process();
}

// Till loops
till(9, 1) {
    stdout << $;
}
```

---

## Struct Initialization

```aria
struct Point {
    x: i32,
    y: i32,
}

point: Point = Point {
    x: 10,
    y: 20,
};

// Inline
point: Point = Point { x: 10, y: 20 };
```

---

## Blocks as Expressions

```aria
// Block returns a value
value: i32 = {
    x: i32 = 10;
    y: i32 = 20;
    x + y  // Return value (no semicolon)
};

// Conditional blocks
Result: i32 = if condition {
    42
} else {
    0
};
```

---

## Match Expressions

```aria
value: string = match number {
    0 => "zero",
    1 => "one",
    _ => "many",
};

// Multi-line arms
match request {
    Request::Get { path } => {
        stdout << "GET $path";
        handle_get(path)
    }
    Request::Post { path, body } => {
        stdout << "POST $path";
        handle_post(path, body)
    }
}
```

---

## Nested Blocks

```aria
fn nested() {
    {
        x: i32 = 10;
        stdout << x;
    }  // x goes out of scope here
    
    {
        y: i32 = 20;
        stdout << y;
    }  // y goes out of scope here
}
```

---

## Scope and Lifetime

```aria
fn scoping_example() {
    x: i32 = 10;
    
    {
        y: i32 = 20;
        stdout << x + y;  // Both x and y visible
    }  // y destroyed
    
    stdout << x;  // Only x visible
    // stdout << y;  // Error: y not in scope
}
```

---

## Common Patterns

### Early Return

```aria
fn validate(x: i32) -> Result<i32> {
    if x < 0 {
        return Err("Negative value");
    }
    
    if x > 100 {
        return Err("Value too large");
    }
    
    return Ok(x);
}
```

---

### Initialization Block

```aria
fn initialize() {
    // Setup block
    {
        load_config();
        init_database();
        start_services();
    }
    
    // Main logic
    run_application();
}
```

---

### Resource Management

```aria
fn process_file(path: string) -> Result<void> {
    {
        file: File = open(path)?;
        defer file.close();
        
        // Work with file
        data: string = file.read_all()?;
        process(data);
    }  // file.close() called here
    
    return Ok();
}
```

---

### Labeled Blocks

```aria
Result: i32 = outer: {
    till(9, 1) {
        if $ == 5 {
            break outer 42;  // Exit with value
        }
    }
    0  // Default value
};
```

---

### Closure Bodies

```aria
// Single expression (no braces)
square = |x| x * x;

// Multiple statements (braces required)
complex = |x| {
    temp: i32 = x * 2;
    Result: i32 = temp + 10;
    result
};
```

---

## Empty Blocks

```aria
fn empty() {
    // Empty function
}

fn placeholder() {
    if condition {
        // TODO: implement
    }
}
```

---

## Best Practices

### ✅ DO: Use Braces for Clarity

```aria
// ✅ Clear
if condition {
    do_something();
}

// ✅ Even for single statements
if error {
    return Err("Failed");
}
```

### ✅ DO: Indent Consistently

```aria
fn proper_indentation() {
    if condition {
        if nested {
            do_work();
        }
    }
}
```

### ✅ DO: Use Blocks for Scope Control

```aria
fn scoped_variables() {
    {
        temp: []u8 = allocate_large_buffer();
        process(temp);
    }  // temp freed here - good for memory
    
    continue_processing();
}
```

### ⚠️ WARNING: Match Brace Placement

```aria
// ⚠️ Unclosed brace
fn bad() {
    if condition {
        do_work();
    // Missing closing brace!

// ✅ Proper closing
fn good() {
    if condition {
        do_work();
    }
}
```

### ❌ DON'T: Unnecessary Nesting

```aria
// ❌ Too many nested braces
fn over_nested() {
    {
        {
            {
                do_work();
            }
        }
    }
}

// ✅ Simpler
fn better() {
    do_work();
}
```

---

## Brace Styles

### K&R Style (Recommended)

```aria
fn k_and_r() {
    if condition {
        do_work();
    } else {
        do_other();
    }
}
```

---

### Allman Style

```aria
fn allman()
{
    if condition
    {
        do_work();
    }
    else
    {
        do_other();
    }
}
```

---

### GNU Style

```aria
fn gnu()
  {
    if condition
      {
        do_work();
      }
    else
      {
        do_other();
      }
  }
```

---

## Single-Line Blocks

```aria
// Allowed but use sparingly
if x > 0 { stdout << "positive"; }

fn one_liner() { return 42; }

// Preferred - multi-line for readability
if x > 0 {
    stdout << "positive";
}

fn readable() {
    return 42;
}
```

---

## Related

- [colons](colons.md) - Type annotations
- [semicolons](semicolons.md) - Statement terminators
- [whitespace_insensitive](whitespace_insensitive.md) - Whitespace rules

---

**Remember**: Braces **delimit scope** and make code structure **clear** and **explicit**!
