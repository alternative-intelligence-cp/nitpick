# Lambda Syntax Reference

**Category**: Functions → Lambda  
**Basic Form**: `|params| body`  
**Full Form**: `|params| -> return_type { body }`

---

## Basic Syntax

### Simplest Form

```aria
// Expression lambda
double = |x| x * 2;

// Type: fn(i32) -> i32 (inferred)
Result: i32 = double(5);  // 10
```

### With Type Annotations

```aria
// Parameter types
add = |a: i32, b: i32| a + b;

// Return type
multiply = |a: i32, b: i32| -> i32 { return a * b; };

// Both
divide = |a: i32, b: i32| -> i32 {
    when b == 0 then return 0; end
    return a / b;
};
```

---

## Parameter Syntax

### No Parameters

```aria
// Empty pipes
get_value = || 42;

// Returns: i32
value: i32 = get_value();  // 42
```

### Single Parameter

```aria
// No parentheses needed
square = |x| x * x;

// With type
square = |x: i32| x * x;
```

### Multiple Parameters

```aria
// Comma-separated
add = |a, b| a + b;

// With types
add = |a: i32, b: i32| a + b;

// Many parameters
format_user = |id: i32, name: string, age: i32| {
    return format("User {} ({}): {}", id, name, age);
};
```

---

## Body Syntax

### Expression Body (No Braces)

```aria
// Single expression - implicit return
double = |x| x * 2;
is_positive = |x| x > 0;
format = |x| "Value: " + format("{}", x);
```

### Block Body (With Braces)

```aria
// Multiple statements
process = |x| {
    temp: i32 = x * 2;
    temp = temp + 1;
    return temp;
};

// Control flow
absolute = |x: i32| -> i32 {
    when x < 0 then
        return -x;
    else
        return x;
    end
};
```

---

## Return Type Syntax

### Inferred

```aria
// Return type inferred from body
add = |a, b| a + b;  // Returns i32 if a and b are i32

// Inferred from usage
callback: fn(i32) -> i32 = |x| x * 2;
```

### Explicit

```aria
// Arrow syntax
parse = |s: string| -> i32 {
    return s.parse::<i32>();
};

// Optional return
find = |array: []i32, target: i32| -> i32? {
    till(array.length - 1, 1) {
        item: i32 = array[$];
        when item == target then return item; end
    }
    return nil;
};
```

---

## Closure Capture Syntax

### Immutable Capture (Automatic)

```aria
prefix: string = "Hello";

// prefix captured by value
greet = |name: string| prefix + ", " + name;

Result: string = greet("Alice");  // "Hello, Alice"
```

### Mutable Capture (Use $)

```aria
count: i32 = 0;

// $count captured by reference
increment = || {
    $count = $count + 1;
    return $count;
};

increment();  // 1
increment();  // 2
```

---

## Full Type Annotation

```aria
// Variable type includes full lambda signature
callback: fn(i32, i32) -> i32 = |a: i32, b: i32| -> i32 {
    return a + b;
};

// Can omit types in lambda (inferred from variable type)
callback: fn(i32, i32) -> i32 = |a, b| a + b;
```

---

## Async Lambda Syntax

```aria
// Async lambda
fetch = async |url: string| -> Data {
    return await http_get(url);
};

// Call with await
data: Data = await fetch("https://api.example.com");
```

---

## Generic Lambda (Not Directly Supported)

```aria
// ❌ Wrong: Can't have generic lambda directly
// identity = |x: T| -> T { return x; };

// ✅ Right: Use regular generic function
fn identity<T>(x: T) -> T {
    return x;
}

// Or use type inference
identity = |x| x;  // Type inferred at call site
```

---

## Special Cases

### Trailing Comma

```aria
// Allowed in multi-line
callback = |
    a: i32,
    b: i32,  // Trailing comma OK
| {
    return a + b;
};
```

### Underscore for Unused Parameters

```aria
// Ignore parameter
process = |_, value| value * 2;

// Usage
Result: i32 = process(999, 5);  // 10 (first arg ignored)
```

---

## Common Patterns

### As Callback

```aria
button.on_click(|| {
    stdout << "Button clicked!\n";
});
```

### In Array Methods

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// map
doubled: []i32 = numbers.map(|x| x * 2);

// filter
evens: []i32 = numbers.filter(|x| x % 2 == 0);

// reduce
sum: i32 = numbers.reduce(0, |acc, x| acc + x);
```

### As Function Argument

```aria
fn apply<T>(value: T, f: fn(T) -> T) -> T {
    return f(value);
}

// Pass lambda directly
Result: i32 = apply(5, |x| x * 2);  // 10
```

---

## Comparison with Named Functions

```aria
// Named function
fn double(x: i32) -> i32 {
    return x * 2;
}

// Equivalent lambda
double = |x: i32| -> i32 { return x * 2; };

// Or shorter
double = |x| x * 2;
```

---

## Best Practices

### ✅ DO: Use Expression Form for Simple Cases

```aria
// Good: Concise and clear
numbers.map(|x| x * 2)
```

### ✅ DO: Add Types for Clarity

```aria
// Good: Clear parameter types
callback: fn(User) -> bool = |user: User| {
    return user.age >= 18;
};
```

### ✅ DO: Use Block for Complex Logic

```aria
// Good: Complex logic in block
process = |data: Data| {
    validated: Data = validate(data);
    when validated == ERR then
        return ERR;
    end
    return transform(validated);
};
```

### ❌ DON'T: Make Lambdas Too Complex

```aria
// Wrong: Too complex for lambda
complicated = |x| {
    // 50 lines of code...
};

// Right: Use named function
fn complicated_process(x: i32) -> Result {
    // 50 lines of code...
}
```

### ❌ DON'T: Nest Lambdas Deeply

```aria
// Wrong: Hard to read
result = array.map(|x| {
    items.filter(|y| {
        y > x
    }).map(|z| z * 2)
});

// Right: Break it up
filtered = items.filter(|y| y > x);
doubled = filtered.map(|z| z * 2);
```

---

## Complete Examples

### Simple Callback

```aria
on_success: fn(string) = |msg| {
    stdout << "Success: " << msg << "\n";
};

on_success("Operation complete");
```

### Closure with State

```aria
counter: i32 = 0;
increment: fn() -> i32 = || {
    $counter = $counter + 1;
    return $counter;
};

stdout << increment();  // 1
stdout << increment();  // 2
```

### Data Pipeline

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

Result: i32 = numbers
    .filter(|x| x > 2)
    .map(|x| x * x)
    .reduce(0, |acc, x| acc + x);
// ((3*3) + (4*4) + (5*5)) = 9 + 16 + 25 = 50
```

---

## Related Topics

- [Lambda Expressions](lambda.md) - Lambda concept and usage
- [Closures](closures.md) - Closure semantics
- [Closure Capture](closure_capture.md) - Variable capture details
- [Function Syntax](function_syntax.md) - Regular function syntax

---

**Remember**: Lambda syntax is **pipes → arrow → body**. Keep it simple - if it gets complex, use a named function!
