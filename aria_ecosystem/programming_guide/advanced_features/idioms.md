# Idioms

**Category**: Advanced Features → Best Practices  
**Purpose**: Idiomatic Aria programming style

---

## Overview

Idioms are language-specific patterns that make code more **expressive** and **readable**.

---

## Use `?` for Error Propagation

```aria
// ✅ Idiomatic
fn load_config() -> Result<Config> {
    content: string = readFile("config.toml")?;
    config: Config = parse(content)?;
    return Ok(config);
}

// ❌ Not idiomatic
fn load_config_verbose() -> Result<Config> {
    match readFile("config.toml") {
        Ok(content) => {
            match parse(content) {
                Ok(config) => return Ok(config),
                Err(e) => return Err(e),
            }
        }
        Err(e) => return Err(e),
    }
}
```

---

## Prefer `if let` for Single Pattern Match

```aria
// ✅ Idiomatic
if let Some(user) = find_user(id) {
    stdout << user.name;
}

// ❌ Verbose
match find_user(id) {
    Some(user) => stdout << user.name,
    None => {},
}
```

---

## Use Early Returns

```aria
// ✅ Idiomatic
fn validate(user: User) -> Result<void> {
    if user.name == "" {
        return Err("Name required");
    }
    
    if user.age < 0 {
        return Err("Invalid age");
    }
    
    return Ok();
}

// ❌ Nested
fn validate_nested(user: User) -> Result<void> {
    if user.name != "" {
        if user.age >= 0 {
            return Ok();
        } else {
            return Err("Invalid age");
        }
    } else {
        return Err("Name required");
    }
}
```

---

## Destructure in Function Parameters

```aria
// ✅ Idiomatic
fn greet(User { name, age, .. }: User) {
    stdout << "Hello $name, age $age";
}

// ❌ Verbose
fn greet_verbose(user: User) {
    stdout << "Hello ${user.name}, age ${user.age}";
}
```

---

## Use Iterator Chains

```aria
// ✅ Idiomatic
total: i32 = numbers
    .filter(|n| n > 0)
    .map(|n| n * 2)
    .sum();

// ❌ Imperative
total: i32 = 0;
till(numbers.length - 1, 1) {
    if numbers[$] > 0 {
        total += numbers[$] * 2;
    }
}
```

---

## Prefer `unwrap_or` Over `match`

```aria
// ✅ Idiomatic
value: i32 = maybe_value.unwrap_or(0);

// ❌ Verbose
value: i32 = match maybe_value {
    Some(v) => v,
    None => 0,
};
```

---

## Use Type Inference

```aria
// ✅ Idiomatic
items := vec![1, 2, 3];
result := calculate_sum(items);

// ❌ Overly explicit (when obvious)
items: Vec<i32> = vec![1, 2, 3];
Result: i32 = calculate_sum(items);
```

---

## Prefer Methods Over Functions

```aria
// ✅ Idiomatic
struct User {
    name: string,
}

impl User {
    fn greet() {
        stdout << "Hello, $self.name";
    }
}

user.greet();

// ❌ Less idiomatic
fn greet_user(user: User) {
    stdout << "Hello, ${user.name}";
}

greet_user(user);
```

---

## Use `defer` for Cleanup

```aria
// ✅ Idiomatic
fn process() -> Result<void> {
    file: File = open("data.txt")?;
    defer file.close();
    
    lock: Mutex = acquire_lock()?;
    defer lock.unlock();
    
    // Work with resources
    return Ok();
}

// ❌ Manual cleanup
fn process_manual() -> Result<void> {
    file: File = open("data.txt")?;
    lock: Mutex = acquire_lock()?;
    
    // Work with resources
    
    lock.unlock();
    file.close();
    return Ok();
}
```

---

## Tuple Returns for Multiple Values

```aria
// ✅ Idiomatic
fn divide(a: i32, b: i32) -> (i32, i32) {
    return (a / b, a % b);
}

(quotient, remainder) = divide(17, 5);

// ❌ Out parameters (C-style)
fn divide_out(a: i32, b: i32, quotient: *i32, remainder: *i32) {
    *quotient = a / b;
    *remainder = a % b;
}
```

---

## Use Guards in Match

```aria
// ✅ Idiomatic
match value {
    n if n < 0 => stdout << "negative",
    n if n == 0 => stdout << "zero",
    n if n % 2 == 0 => stdout << "positive even",
    _ => stdout << "positive odd",
}

// ❌ Nested if
match value {
    n => {
        if n < 0 {
            stdout << "negative";
        } else if n == 0 {
            stdout << "zero";
        } else if n % 2 == 0 {
            stdout << "positive even";
        } else {
            stdout << "positive odd";
        }
    }
}
```

---

## Prefer `while let` for Iteration

```aria
// ✅ Idiomatic
while let Some(item) = iterator.next() {
    process(item);
}

// ❌ Verbose
loop {
    match iterator.next() {
        Some(item) => process(item),
        None => break,
    }
}
```

---

## Use Closures for Short Functions

```aria
// ✅ Idiomatic
numbers.map(|x| x * 2);
numbers.filter(|x| x > 0);

// ❌ Verbose (when function is simple)
fn double(x: i32) -> i32 {
    return x * 2;
}

fn is_positive(x: i32) -> bool {
    return x > 0;
}

numbers.map(double);
numbers.filter(is_positive);
```

---

## String Interpolation

```aria
// ✅ Idiomatic
name: string = "Alice";
age: i32 = 30;
message: string = "Hello, $name! You are $age years old.";

// ❌ Concatenation
message: string = "Hello, " ++ name ++ "! You are " ++ age.to_string() ++ " years old.";
```

---

## Explicit Error Types

```aria
// ✅ Idiomatic
enum AppError {
    NotFound,
    PermissionDenied,
    InvalidInput(string),
}

fn fetch_user(id: i32) -> Result<User, AppError> {
    // Implementation
}

// ❌ String errors
fn fetch_user_bad(id: i32) -> Result<User, string> {
    // Less type-safe
}
```

---

## Use Ranges

```aria
// ✅ Idiomatic
till(9, 1) {
    stdout << $;
}

// ❌ C-style
for i: i32 = 0; i < 10; i += 1 {
    stdout << i;
}
```

---

## Default Values with `unwrap_or`

```aria
// ✅ Idiomatic
timeout: i32 = config.timeout.unwrap_or(30);
retries: i32 = config.retries.unwrap_or(3);

// ❌ Verbose
timeout: i32 = if config.timeout is Some {
    config.timeout?
} else {
    30
};
```

---

## Slice Patterns

```aria
// ✅ Idiomatic
match items {
    [] => stdout << "empty",
    [x] => stdout << "one: $x",
    [x, y] => stdout << "two: $x, $y",
    [first, ...rest] => stdout << "many, first: $first",
}

// ❌ Manual indexing
if items.len() == 0 {
    stdout << "empty";
} else if items.len() == 1 {
    stdout << "one: ${items[0]}";
} else if items.len() == 2 {
    stdout << "two: ${items[0]}, ${items[1]}";
} else {
    stdout << "many, first: ${items[0]}";
}
```

---

## Named Loop Labels

```aria
// ✅ Idiomatic (for complex loops)
outer: till(9, 1) {
    i = $;
    inner: till(9, 1) {
        j = $;
        if found(i, j) {
            break outer;
        }
    }
}

// Avoid when simple
// ❌ Over-labeled
simple: till(9, 1) {
    stdout << $;
}
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [common_patterns](common_patterns.md) - Common patterns
- [code_examples](code_examples.md) - Code examples

---

**Remember**: Idiomatic code is **easier to read** and **maintain** for developers familiar with Aria!
