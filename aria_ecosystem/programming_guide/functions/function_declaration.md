# Function Declaration

**Category**: Functions → Basics  
**Keyword**: `fn`  
**Philosophy**: Functions are first-class, type-safe, and explicit

---

## Basic Syntax

```aria
fn function_name(param1: Type1, param2: Type2) -> ReturnType {
    // Function body
    return value;
}
```

---

## Components

### Function Keyword: `fn`

```aria
fn greet(name: string) -> string {
    return "Hello, " + name;
}
```

### Parameters with Types

```aria
// Parameters must have explicit types
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### Return Type

```aria
// Return type declared with ->
fn calculate() -> f64 {
    return 42.5;
}

// No return value (void)
fn print_message(msg: string) {
    stdout << msg << "\n";
    // No return statement needed
}
```

---

## Examples

### Simple Function

```aria
fn square(x: i32) -> i32 {
    return x * x;
}

// Usage
Result: i32 = square(5);  // 25
```

### Multiple Parameters

```aria
fn calculate_area(width: f64, height: f64) -> f64 {
    return width * height;
}

area: f64 = calculate_area(10.0, 20.0);  // 200.0
```

### No Return Value

```aria
fn log_message(level: string, msg: string) {
    stdout << "[" << level << "] " << msg << "\n";
}

log_message("INFO", "Application started");
```

### With TBB Return Type

```aria
fn divide(a: tbb32, b: tbb32) -> tbb32 {
    when b == 0 then
        return ERR;  // TBB error sentinel
    end
    
    return a / b;
}

Result: tbb32 = divide(10, 2);  // 5
error: tbb32 = divide(10, 0);   // ERR
```

---

## Early Return

```aria
fn check_valid(value: i32) -> bool {
    when value < 0 then
        return false;  // Early return
    end
    
    when value > 100 then
        return false;
    end
    
    return true;
}
```

---

## Function Naming Conventions

### Standard Style
```aria
// snake_case for functions
fn calculate_total() -> i32 { ... }
fn process_user_input() -> string { ... }
```

### Predicates (Boolean Returns)
```aria
// Common: is_, has_, can_ prefixes
fn is_valid(x: i32) -> bool { ... }
fn has_permission(user: User) -> bool { ... }
fn can_access(resource: Resource) -> bool { ... }
```

---

## Parameter Passing

### By Value (Default)
```aria
fn modify(x: i32) -> i32 {
    x = x + 1;  // Modifies local copy
    return x;
}

value: i32 = 10;
Result: i32 = modify(value);
// value is still 10, result is 11
```

### By Reference (with $)
```aria
fn modify_ref(x: $i32) {
    x = x + 1;  // Modifies original
}

value: i32 = 10;
modify_ref($value);
// value is now 11
```

---

## Visibility

### Public Functions
```aria
// Accessible from other modules
pub fn public_api() -> string {
    return "Available to all";
}
```

### Private Functions (Default)
```aria
// Only accessible within same module
fn internal_helper() -> i32 {
    return 42;
}
```

---

## Best Practices

### ✅ DO: Be Explicit with Types

```aria
// Good: Clear types
fn process(data: string) -> i32 {
    return data.len();
}
```

### ✅ DO: Use TBB for Error-Prone Operations

```aria
// Good: TBB handles errors
fn parse_number(s: string) -> tbb32 {
    // Returns ERR on parse failure
    return parse(s);
}
```

### ✅ DO: Keep Functions Focused

```aria
// Good: Single responsibility
fn validate_email(email: string) -> bool {
    return email.contains("@");
}

fn send_email(to: string, message: string) {
    // Separate concern
}
```

### ❌ DON'T: Omit Return Types

```aria
// Wrong: Unclear what this returns
fn calculate(x, y) {  // Missing types!
    return x + y;
}

// Right:
fn calculate(x: i32, y: i32) -> i32 {
    return x + y;
}
```

---

## Related Topics

- [Function Syntax](function_syntax.md) - Complete syntax reference
- [Function Parameters](function_params.md) - Parameter details
- [Function Return Types](function_return_type.md) - Return type options
- [pass/fail Keywords](pass_keyword.md) - TBB result handling
- [Generic Functions](generic_functions.md) - Parameterized functions

---

**Remember**: Aria functions are explicit, type-safe, and designed for clarity. No implicit conversions, no hidden behavior.
