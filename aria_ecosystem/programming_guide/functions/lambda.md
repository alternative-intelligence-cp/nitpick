# Lambda Expressions

**Category**: Functions → Anonymous Functions  
**Syntax**: `|params| -> return_type { body }`  
**Philosophy**: Functions as values, concise and expressive

---

## What is a Lambda?

A **lambda** is an anonymous function - a function without a name that can be passed as a value, stored in variables, or used inline.

---

## Basic Syntax

```aria
// Simple lambda
add: fn(i32, i32) -> i32 = |a: i32, b: i32| -> i32 {
    return a + b;
};

Result: i32 = add(5, 3);  // 8
```

---

## Short Form (Single Expression)

```aria
// When body is single expression, braces and return optional
square: fn(i32) -> i32 = |x: i32| -> i32 x * x;

// Even shorter: Infer return type
square: fn(i32) -> i32 = |x: i32| x * x;
```

---

## Lambda as Function Parameter

```aria
// Higher-order function
fn apply_twice(f: fn(i32) -> i32, x: i32) -> i32 {
    return f(f(x));
}

// Use with lambda
Result: i32 = apply_twice(|n: i32| n * 2, 5);
// First: 5 * 2 = 10
// Second: 10 * 2 = 20
```

---

## Common Patterns

### Array Operations

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// Map: transform each element
doubled: []i32 = numbers.map(|x: i32| x * 2);
// [2, 4, 6, 8, 10]

// Filter: keep elements matching predicate
evens: []i32 = numbers.filter(|x: i32| x % 2 == 0);
// [2, 4]

// Reduce: combine elements
sum: i32 = numbers.reduce(0, |acc: i32, x: i32| acc + x);
// 15
```

### Inline Event Handlers

```aria
button.on_click(|event: Event| {
    stdout << "Button clicked at " << event.x << ", " << event.y << "\n";
});
```

### Sorting with Custom Comparator

```aria
users: []User = get_users();

// Sort by age
users.sort_by(|a: User, b: User| a.age <=> b.age);

// Sort by name length
users.sort_by(|a: User, b: User| a.name.len() <=> b.name.len());
```

---

## Closures (Capturing Variables)

Lambdas can **capture** variables from their surrounding scope:

```aria
fn make_adder(x: i32) -> fn(i32) -> i32 {
    // Lambda captures 'x' from outer scope
    return |y: i32| x + y;
}

add_5: fn(i32) -> i32 = make_adder(5);
Result: i32 = add_5(3);  // 8 (5 + 3)
```

### Capture by Value vs Reference

```aria
// Capture by value (copy)
counter: i32 = 0;
increment: fn() -> i32 = || {
    counter = counter + 1;  // Captures copy of counter
    return counter;
};

// Capture by reference (with $)
counter: i32 = 0;
increment: fn() -> i32 = || {
    $counter = $counter + 1;  // Modifies original counter
    return $counter;
};
```

---

## Type Annotations

### Explicit Types (Verbose)

```aria
processor: fn(string, i32) -> bool = |s: string, n: i32| -> bool {
    return s.len() > n;
};
```

### Type Inference (Concise)

```aria
// Compiler infers types from context
numbers.map(|x| x * 2);  // x inferred as i32

// But explicit is clearer
numbers.map(|x: i32| -> i32 x * 2);
```

---

## Multi-Line Lambdas

```aria
complex_operation: fn(Data) -> Result = |data: Data| -> Result {
    stddbg << "Processing data: " << data.id;
    
    when data.size() > MAX_SIZE then
        stderr << "ERROR: Data too large\n";
        return ERR;
    end
    
    processed: Data = transform(data);
    validated: Data = pass validate(processed);
    
    return validated;
};
```

---

## Lambdas in Data Structures

```aria
// Array of functions
operations: []fn(i32) -> i32 = [
    |x| x + 1,
    |x| x * 2,
    |x| x * x,
];

// Apply all operations
value: i32 = 5;
for op in operations {
    value = op(value);
}
// 5 → 6 → 12 → 144
```

---

## Immediate Invocation

```aria
// Define and call lambda immediately
Result: i32 = (|x: i32| x * x)(5);  // 25

// Useful for complex initialization
config: Config = (|() -> Config {
    c: Config = Config::new();
    c.timeout = 30;
    c.retries = 3;
    return c;
})();
```

---

## Best Practices

### ✅ DO: Use for Short, Inline Logic

```aria
// Good: Simple transformation
doubled: []i32 = numbers.map(|x| x * 2);
```

### ✅ DO: Capture Explicitly

```aria
// Good: Clear what's captured
multiplier: i32 = 10;
scale: fn(i32) -> i32 = |x: i32| x * multiplier;
```

### ✅ DO: Add Types for Clarity

```aria
// Good: Explicit types when non-obvious
users.sort_by(|a: User, b: User| -> i32 {
    return a.priority <=> b.priority;
});
```

### ❌ DON'T: Make Lambdas Too Complex

```aria
// Wrong: Lambda is too long
Result: []Data = items.map(|item| {
    // 50 lines of complex logic...
    // This should be a named function!
});

// Right: Extract to named function
fn process_item(item: Item) -> Data {
    // Complex logic here
}

Result: []Data = items.map(process_item);
```

### ❌ DON'T: Capture Mutable State Carelessly

```aria
// Wrong: Side effects in lambda
counter: i32 = 0;
numbers.map(|x| {
    $counter = $counter + 1;  // Side effect!
    return x * 2;
});

// Right: Use reduce or for loop
counter: i32 = numbers.len();
```

---

## Lambda vs Named Function

**Use Lambda when**:
- Function is used once (inline)
- Logic is short and simple
- Passed as callback/handler

**Use Named Function when**:
- Function is reused multiple times
- Logic is complex (>5 lines)
- Function needs a descriptive name

---

## Real-World Examples

### Event Processing

```aria
fn setup_handlers() {
    app.on_start(|| {
        stddbg << "Application started";
        load_config();
    });
    
    app.on_error(|err: Error| {
        stderr << "ERROR: " << err.message << "\n";
        log_error(err);
    });
    
    app.on_shutdown(|| {
        stddbg << "Shutting down...";
        cleanup_resources();
    });
}
```

### Data Pipeline

```aria
fn process_logs(logs: []LogEntry) -> []ErrorEntry {
    return logs
        .filter(|log| log.level == "ERROR")
        .map(|log| ErrorEntry{
            timestamp: log.timestamp,
            message: log.message,
            severity: calculate_severity(log),
        })
        .filter(|entry| entry.severity > 5);
}
```

### Configuration Builder

```aria
fn create_server_config() -> ServerConfig {
    return ServerConfig::builder()
        .port(8080)
        .on_request(|req: Request| -> Response {
            stddbg << "Received: " << req.method << " " << req.path;
            return handle_request(req);
        })
        .on_error(|err: Error| {
            stderr << "Server error: " << err << "\n";
        })
        .build();
}
```

---

## Related Topics

- [Closures](closures.md) - Variable capture details
- [Higher-Order Functions](higher_order_functions.md) - Functions taking functions
- [Function Declaration](function_declaration.md) - Named functions
- [Anonymous Functions](anonymous_functions.md) - General concept

---

**Remember**: Lambdas are **functions as values**. Use them for concise, inline logic. For anything complex, use a named function instead.
