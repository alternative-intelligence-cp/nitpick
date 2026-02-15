# Colons

**Category**: Advanced Features → Syntax  
**Purpose**: Type annotations and labels

---

## Overview

Colons `:` separate **names from types** and define **labels**.

---

## Type Annotations

```aria
// Variable declarations
x: i32 = 42;
name: string = "Alice";
data: []u8 = [1, 2, 3];

// Function parameters
fn greet(name: string, age: i32) {
    stdout << "Hello $name, age $age";
}

// Return types
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Struct Fields

```aria
struct User {
    id: i32,
    name: string,
    email: string,
    created_at: i64,
}
```

---

## Enum Variants

```aria
enum Result<T, E> {
    Ok(value: T),
    Err(error: E),
}

enum Message {
    Quit,
    Move { x: i32, y: i32 },
    Write(text: string),
}
```

---

## Loop Labels

```aria
outer: till(9, 1) {
    i = $;
    inner: till(9, 1) {
        j = $;
        if i * j > 50 {
            break outer;  // Break outer loop
        }
    }
}
```

---

## Block Labels

```aria
Result: i32 = block: {
    x: i32 = compute_value();
    if x < 0 {
        break block 0;
    }
    break block x * 2;
};
```

---

## Match Arms

```aria
value: string = match number {
    0 => "zero",
    1 => "one",
    _ => "many",
};
```

---

## Common Patterns

### Multiple Variable Declarations

```aria
x: i32 = 0;
y: i32 = 0;
z: i32 = 0;

// Or with type inference
a := 1;
b := 2;
c := 3;
```

---

### Complex Types

```aria
// Array of integers
numbers: []i32 = [1, 2, 3, 4, 5];

// HashMap
users: HashMap<i32, User> = HashMap.new();

// Optional
maybe_value: ?i32 = Some(42);

// Result
Result: Result<Data> = fetch_data();

// Function pointer
callback: fn(i32) -> string;
```

---

### Generic Constraints

```aria
fn process<T: Display + Clone>(value: T) {
    stdout << value.to_string();
}

struct Container<T> where T: Clone {
    items: []T,
}
```

---

### Tuple Types

```aria
point: (i32, i32) = (10, 20);
person: (string, i32, string) = ("Alice", 30, "alice@example.com");

// Named tuple fields
fn get_user() -> (name: string, age: i32) {
    return ("Bob", 25);
}
```

---

## Nested Loop Labels

```aria
search: till(99, 1) {
    x = $;
    check: till(99, 1) {
        y = $;
        if found(x, y) {
            stdout << "Found at ($x, $y)";
            break search;
        }
    }
}
```

---

## Best Practices

### ✅ DO: Always Annotate Function Parameters

```aria
fn calculate(amount: f64, rate: f64) -> f64 {
    return amount * rate;
}
```

### ✅ DO: Use Labels for Complex Loops

```aria
find_pair: till(data.len() - 1, 1) {
    i = $;
    till(data.len() - 1, 1) {
        j = $;
        if j <= i { continue; }  // Skip if j <= i
        if data[i] + data[j] == target {
            break find_pair;
        }
    }
}
```

### ⚠️ WARNING: Type Inference vs Explicit Types

```aria
// Type inference - OK for obvious types
x := 42;
name := "Alice";

// Explicit types - better for clarity
timeout: Duration = Duration.seconds(30);
config: Config = load_config();
```

### ❌ DON'T: Omit Return Types

```aria
// ❌ Hard to understand what this returns
fn process(data) {
    return transform(data);
}

// ✅ Clear return type
fn process(data: Data) -> Result<Processed> {
    return transform(data);
}
```

---

## Style Guidelines

```aria
// Spacing: space after colon
x: i32 = 42;          // ✅ Correct
x:i32 = 42;           // ❌ Too cramped

// Alignment in structs
struct User {
    id:         i32,      // ✅ Can align
    name:       string,   // ✅ For readability
    email:      string,
    created_at: i64,
}

// Or not
struct User {
    id: i32,              // ✅ Also fine
    name: string,
    email: string,
    created_at: i64,
}
```

---

## Related

- [semicolons](semicolons.md) - Statement terminators
- [brace_delimited](brace_delimited.md) - Brace usage

---

**Remember**: Colons make types **explicit** and enable **label-based control flow**!
