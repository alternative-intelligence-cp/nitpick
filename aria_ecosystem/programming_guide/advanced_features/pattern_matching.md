# Pattern Matching

**Category**: Advanced Features → Patterns  
**Purpose**: Match values against patterns and destructure data

---

## Overview

Pattern matching provides powerful **structural comparison** and **data extraction**.

---

## Basic Match

```aria
enum Color {
    Red,
    Green,
    Blue,
}

fn describe(color: Color) -> string {
    match color {
        Color.Red => return "red",
        Color.Green => return "green",
        Color.Blue => return "blue",
    }
}
```

---

## Match with Values

```aria
fn describe_number(n: i32) -> string {
    match n {
        0 => return "zero",
        1 => return "one",
        2 => return "two",
        _ => return "many",  // Default case
    }
}
```

---

## Range Patterns

```aria
fn categorize_age(age: i32) -> string {
    match age {
        0..=12 => return "child",
        13..=19 => return "teenager",
        20..=64 => return "adult",
        65.. => return "senior",
        _ => return "invalid",
    }
}
```

---

## Destructuring Tuples

```aria
fn process_point(point: (i32, i32)) -> string {
    match point {
        (0, 0) => return "origin",
        (x, 0) => return "on x-axis at $x",
        (0, y) => return "on y-axis at $y",
        (x, y) if x == y => return "on diagonal",
        (x, y) => return "at ($x, $y)",
    }
}
```

---

## Destructuring Structs

```aria
struct Point {
    x: i32,
    y: i32,
}

fn describe_point(p: Point) -> string {
    match p {
        Point { x: 0, y: 0 } => return "origin",
        Point { x, y: 0 } => return "on x-axis",
        Point { x: 0, y } => return "on y-axis",
        Point { x, y } => return "point at ($x, $y)",
    }
}
```

---

## Nested Patterns

```aria
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
    Point(i32, i32),
}

fn area(shape: Shape) -> f64 {
    match shape {
        Shape.Circle(r) => return 3.14159 * r * r,
        Shape.Rectangle(w, h) => return w * h,
        Shape.Point(_, _) => return 0.0,
    }
}
```

---

## Guards

```aria
fn classify_number(n: i32) -> string {
    match n {
        x if x < 0 => return "negative",
        x if x == 0 => return "zero",
        x if x % 2 == 0 => return "positive even",
        x if x % 2 == 1 => return "positive odd",
        _ => return "unknown",
    }
}
```

---

## Option Matching

```aria
fn get_default(opt: ?i32, default: i32) -> i32 {
    match opt {
        Some(value) => return value,
        None => return default,
    }
}

fn process_optional(opt: ?string) {
    match opt {
        Some(s) if s.len() > 0 => stdout << s,
        Some(_) => stdout << "empty string",
        None => stdout << "no value",
    }
}
```

---

## Result Matching

```aria
fn handle_result(Result: Result<i32>) {
    match result {
        Ok(value) => stdout << "Success: $value",
        Err(error) => stderr << "Error: $error",
    }
}

fn process_with_recovery(Result: Result<Data>) -> Data {
    match result {
        Ok(data) => return data,
        Err(NetworkError) => return fetch_from_cache(),
        Err(ParseError) => return Data.default(),
        Err(e) => panic("Unexpected error: $e"),
    }
}
```

---

## Array/Slice Patterns

```aria
fn process_list(items: []i32) {
    match items {
        [] => stdout << "empty",
        [x] => stdout << "one item: $x",
        [x, y] => stdout << "two items: $x, $y",
        [first, ...rest] => {
            stdout << "first: $first";
            stdout << "rest: $rest";
        }
        _ => stdout << "many items",
    }
}
```

---

## Wildcard Patterns

```aria
fn process_tuple(t: (i32, string, bool)) {
    match t {
        (0, _, _) => stdout << "first is zero",
        (_, "hello", _) => stdout << "second is hello",
        (_, _, true) => stdout << "third is true",
        _ => stdout << "no match",
    }
}
```

---

## Common Patterns

### Error Handling

```aria
fn safe_divide(a: f64, b: f64) -> Result<f64> {
    match b {
        0.0 => return Err("Division by zero"),
        b if b.is_nan() => return Err("Invalid divisor"),
        _ => return Ok(a / b),
    }
}
```

---

### State Machine

```aria
enum State {
    Idle,
    Running(i32),
    Paused(i32),
    Done,
}

fn transition(current: State, event: Event) -> State {
    match (current, event) {
        (State.Idle, Event.Start) => return State.Running(0),
        (State.Running(n), Event.Pause) => return State.Paused(n),
        (State.Paused(n), Event.Resume) => return State.Running(n),
        (State.Running(n), Event.Complete) if n >= 100 => return State.Done,
        (State.Running(n), Event.Update) => return State.Running(n + 1),
        (state, _) => return state,  // No transition
    }
}
```

---

### Command Processing

```aria
enum Command {
    Quit,
    Help,
    Set(string, string),
    Get(string),
    Delete(string),
}

fn execute(cmd: Command, state: *State) {
    match cmd {
        Command.Quit => exit(0),
        Command.Help => print_help(),
        Command.Set(key, value) => state.insert(key, value),
        Command.Get(key) => {
            match state.get(key) {
                Some(value) => stdout << value,
                None => stderr << "Key not found",
            }
        }
        Command.Delete(key) => state.remove(key),
    }
}
```

---

### JSON Parsing

```aria
enum Json {
    Null,
    Bool(bool),
    Number(f64),
    String(string),
    Array([]Json),
    Object(HashMap<string, Json>),
}

fn stringify(json: Json) -> string {
    match json {
        Json.Null => return "null",
        Json.Bool(true) => return "true",
        Json.Bool(false) => return "false",
        Json.Number(n) => return "$n",
        Json.String(s) => return "\"$s\"",
        Json.Array(items) => {
            // Handle array
        }
        Json.Object(map) => {
            // Handle object
        }
    }
}
```

---

## Multi-Pattern Match

```aria
fn is_vowel(c: char) -> bool {
    match c {
        'a' | 'e' | 'i' | 'o' | 'u' => return true,
        'A' | 'E' | 'I' | 'O' | 'U' => return true,
        _ => return false,
    }
}

fn classify_char(c: char) -> string {
    match c {
        'a'..'z' | 'A'..'Z' => return "letter",
        '0'..'9' => return "digit",
        ' ' | '\t' | '\n' => return "whitespace",
        _ => return "other",
    }
}
```

---

## Best Practices

### ✅ DO: Handle All Cases

```aria
fn safe_match(opt: ?i32) -> i32 {
    match opt {
        Some(value) => return value,
        None => return 0,  // ✅ All cases handled
    }
}
```

### ✅ DO: Use Guards for Complex Conditions

```aria
fn validate_age(age: i32) -> Result<i32> {
    match age {
        n if n < 0 => return Err("Age cannot be negative"),
        n if n > 150 => return Err("Age too high"),
        n => return Ok(n),
    }
}
```

### ✅ DO: Destructure to Extract Data

```aria
struct User {
    name: string,
    age: i32,
    email: string,
}

fn greet(user: User) {
    match user {
        User { name, age, .. } if age < 18 => {
            stdout << "Hi $name! You're under 18.";
        }
        User { name, .. } => {
            stdout << "Hello $name!";
        }
    }
}
```

### ⚠️ WARNING: Exhaustive Matching Required

```aria
enum Status {
    Active,
    Inactive,
    Pending,
}

fn process(status: Status) {
    match status {
        Status.Active => handle_active(),
        Status.Inactive => handle_inactive(),
        // ⚠️ Compiler error: missing Status.Pending
    }
}

// ✅ Fix: handle all cases or use _
fn process_fixed(status: Status) {
    match status {
        Status.Active => handle_active(),
        Status.Inactive => handle_inactive(),
        Status.Pending => handle_pending(),  // ✅ All cases
    }
}
```

### ❌ DON'T: Ignore Unused Bindings Warnings

```aria
// ❌ Bad - unused binding
match value {
    Some(x) => return default,  // x not used
    None => return default,
}

// ✅ Good - use _ for unused
match value {
    Some(_) => return default,
    None => return default,
}
```

---

## Related

- [destructuring](destructuring.md) - Destructuring syntax
- [error_propagation](error_propagation.md) - Error handling

---

**Remember**: Pattern matching makes code **safer** and more **expressive** by ensuring all cases are handled!
