# Arrow Operator (->)

**Category**: Operators → Type Declaration  
**Operator**: `->`  
**Purpose**: Specify return types

---

## Syntax

```aria
fn <name>(<params>) -> <return_type>
```

---

## Description

The arrow operator `->` declares a function's return type.

---

## Basic Usage

```aria
// Returns i32
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Returns string
fn greet(name: string) -> string {
    return "Hello, " + name;
}
```

---

## Void Functions

```aria
// No return type needed for void
fn print_message(msg: string) {
    stdout << msg;
}

// Or explicit void
fn log(text: string) -> void {
    stdout << text;
}
```

---

## Complex Return Types

```aria
// Array
fn get_numbers() -> []i32 {
    return [1, 2, 3, 4, 5];
}

// Optional
fn find_user(id: i32) -> User? {
    when exists(id) then
        return get_user(id);
    else
        return nil;
    end
}

// Tuple
fn divide(a: i32, b: i32) -> (i32, i32) {
    return (a / b, a % b);
}
```

---

## Generic Returns

```aria
fn create<T>() -> T {
    return T::new();
}

fn wrap<T>(value: T) -> Option<T> {
    return Option::Some(value);
}
```

---

## Best Practices

### ✅ DO: Be Explicit

```aria
fn calculate() -> f64 {
    return 42.0;
}
```

### ✅ DO: Use for Clarity

```aria
// Clear contract
fn validate(input: string) -> bool {
    return input.length() > 0;
}
```

### ❌ DON'T: Omit for Non-Void

```aria
// Wrong: Missing return type
fn compute(x: i32) {  // Should specify -> i32
    return x * 2;
}

// Right
fn compute(x: i32) -> i32 {
    return x * 2;
}
```

---

## Related

- [Colon (:)](colon.md) - Type specification
- [Functions](../functions/functions.md)
- [Return Types](../functions/return_types.md)

---

**Remember**: `->` declares what the function **returns**!
