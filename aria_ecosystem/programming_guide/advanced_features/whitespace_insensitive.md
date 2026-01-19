# Whitespace Insensitive

**Category**: Advanced Features → Syntax  
**Purpose**: Flexible formatting for readability

---

## Overview

Aria is **whitespace insensitive** - extra spaces, tabs, and newlines don't affect program meaning.

---

## Flexible Spacing

```aria
// All equivalent
x:i32=42;
x: i32 = 42;
x  :  i32  =  42;

// Function calls
add(1,2)
add(1, 2)
add( 1, 2 )
add(  1  ,  2  )
```

---

## Flexible Line Breaks

```aria
// Single line
fn add(a: i32, b: i32) -> i32 { return a + b; }

// Multiple lines
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Spread out
fn add(
    a: i32,
    b: i32
) -> i32 {
    return a + b;
}
```

---

## Array Formatting

```aria
// Compact
numbers: []i32 = [1,2,3,4,5];

// Spaced
numbers: []i32 = [1, 2, 3, 4, 5];

// Multi-line
numbers: []i32 = [
    1,
    2,
    3,
    4,
    5,
];

// Aligned
numbers: []i32 = [
    1,  2,  3,
    4,  5,  6,
    7,  8,  9,
];
```

---

## Struct Formatting

```aria
// Compact
user: User = User{name:"Alice",age:30,email:"alice@example.com"};

// Readable
user: User = User {
    name: "Alice",
    age: 30,
    email: "alice@example.com",
};

// Aligned
user: User = User {
    name:  "Alice",
    age:   30,
    email: "alice@example.com",
};
```

---

## Function Calls

```aria
// Single line
Result: i32 = calculate(x, y, z);

// Multi-line
Result: i32 = calculate(
    x,
    y,
    z,
);

// Grouped
Result: i32 = calculate(
    first_param,
    second_param,
    third_param,
    fourth_param,
    fifth_param,
);
```

---

## Chained Calls

```aria
// Single line
Result: string = data.filter(|x| x > 0).map(|x| x * 2).collect();

// Multi-line
Result: string = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .collect();

// Indented
Result: string = data
    .filter(|x| x > 0)
    .map(|x| x * 2)
    .reduce(0, |acc, x| acc + x);
```

---

## Common Patterns

### Vertical Alignment

```aria
// Align values
x:      i32 = 10;
y:      i32 = 20;
Result: i32 = x + y;

// Align struct fields
struct Config {
    host:    string,
    port:    i32,
    timeout: i32,
    retries: i32,
}
```

---

### Long Expressions

```aria
// Break long expressions
total: i32 = 
    first_value + 
    second_value + 
    third_value + 
    fourth_value;

// Or use parentheses
total: i32 = (
    first_value +
    second_value +
    third_value +
    fourth_value
);
```

---

### Function Definitions

```aria
// Many parameters
fn complex_function(
    param1: i32,
    param2: string,
    param3: f64,
    param4: bool,
    param5: []u8,
) -> Result<Data> {
    // Implementation
}

// Generic constraints
fn process<T, U>(
    value: T,
    converter: fn(T) -> U,
) -> U 
where 
    T: Clone,
    U: Display,
{
    // Implementation
}
```

---

### Match Expressions

```aria
// Compact
match x { 0 => "zero", 1 => "one", _ => "many" }

// Readable
match x {
    0 => "zero",
    1 => "one",
    _ => "many",
}

// Complex patterns
match request {
    Request::Get { path, params } => {
        handle_get(path, params)
    }
    Request::Post { path, body } => {
        handle_post(path, body)
    }
    _ => {
        handle_other()
    }
}
```

---

## Best Practices

### ✅ DO: Format for Readability

```aria
// ✅ Good - easy to read
fn calculate_total(
    base_price: f64,
    tax_rate: f64,
    discount: f64,
) -> f64 {
    subtotal: f64 = base_price * (1.0 - discount);
    tax: f64 = subtotal * tax_rate;
    return subtotal + tax;
}
```

### ✅ DO: Use Whitespace for Grouping

```aria
// ✅ Visual grouping
x: i32 = 10;
y: i32 = 20;

Result: i32 = x + y;  // Separated by blank line

output: string = "Result: $result";
```

### ✅ DO: Align Related Items

```aria
// ✅ Aligned for easy scanning
ERROR_NOT_FOUND:     i32 = 404;
ERROR_UNAUTHORIZED:  i32 = 401;
ERROR_SERVER_ERROR:  i32 = 500;
```

### ⚠️ WARNING: Be Consistent

```aria
// ⚠️ Inconsistent spacing
x:i32=10;
y: i32 = 20;
z  :  i32  =  30;

// ✅ Consistent
x: i32 = 10;
y: i32 = 20;
z: i32 = 30;
```

### ❌ DON'T: Overdo Whitespace

```aria
// ❌ Too much whitespace
fn     add    (    a   :   i32   ,    b   :   i32   )    ->    i32    {
    return     a     +     b     ;
}

// ✅ Reasonable spacing
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Indentation Styles

```aria
// K&R style (opening brace same line)
fn example() {
    if condition {
        do_work();
    }
}

// Allman style (opening brace next line)
fn example()
{
    if condition
    {
        do_work();
    }
}

// Recommended: K&R for Aria
fn recommended() {
    while condition {
        process();
    }
}
```

---

## Line Length

```aria
// Short - all on one line
fn add(a: i32, b: i32) -> i32 { return a + b; }

// Long - break into multiple lines
fn complex_calculation(
    base: f64,
    multiplier: f64,
    offset: f64,
    scale: f64,
) -> f64 {
    return (base * multiplier + offset) * scale;
}
```

---

## Related

- [brace_delimited](brace_delimited.md) - Brace usage
- [semicolons](semicolons.md) - Statement terminators

---

**Remember**: Whitespace flexibility enables **readable**, **well-formatted** code - use it wisely!
