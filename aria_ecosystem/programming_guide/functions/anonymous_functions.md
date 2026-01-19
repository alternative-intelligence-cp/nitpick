# Anonymous Functions

**Category**: Functions → Lambda  
**Concept**: Functions without names  
**Also Known As**: Lambda expressions, closures

---

## What Are Anonymous Functions?

**Anonymous functions** are functions **without names** - they're defined inline where you need them.

In Aria, anonymous functions are created with **lambda syntax**: `|params| body`

---

## Basic Anonymous Function

```aria
// Named function
fn double(x: i32) -> i32 {
    return x * 2;
}

// Anonymous function (lambda)
|x| x * 2
```

---

## Using Anonymous Functions

### Assigned to Variable

```aria
// Store anonymous function in variable
double: fn(i32) -> i32 = |x| x * 2;

// Use it
Result: i32 = double(5);  // 10
```

### Passed as Argument

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// Pass anonymous function to map
doubled: []i32 = numbers.map(|x| x * 2);
```

### Returned from Function

```aria
fn make_multiplier(factor: i32) -> fn(i32) -> i32 {
    // Return anonymous function
    return |x| x * factor;
}

times_5: fn(i32) -> i32 = make_multiplier(5);
Result: i32 = times_5(3);  // 15
```

---

## Syntax Variations

### Expression Body

```aria
// Simple expression - implicit return
add = |a, b| a + b;
```

### Block Body

```aria
// Multiple statements - explicit return
process = |x| {
    temp: i32 = x * 2;
    temp = temp + 1;
    return temp;
};
```

### With Type Annotations

```aria
// Parameter types
multiply = |a: i32, b: i32| a * b;

// Return type
divide = |a: i32, b: i32| -> i32 {
    when b == 0 then return 0; end
    return a / b;
};
```

---

## Anonymous vs Named Functions

### Named Function

```aria
fn greet(name: string) {
    stdout << "Hello, " << name << "\n";
}

// Can be called by name anywhere
greet("Alice");
```

### Anonymous Function

```aria
// Only exists in this scope
callback: fn(string) = |name| {
    stdout << "Hello, " << name << "\n";
};

// Must be called through variable
callback("Alice");
```

---

## Common Use Cases

### Event Handlers

```aria
button.on_click(|| {
    stdout << "Button clicked!\n";
});
```

### Array Operations

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// Filter with anonymous function
evens: []i32 = numbers.filter(|x| x % 2 == 0);

// Map with anonymous function
doubled: []i32 = numbers.map(|x| x * 2);

// Reduce with anonymous function
sum: i32 = numbers.reduce(0, |acc, x| acc + x);
```

### Sorting

```aria
users: []User = [...];

// Sort by age (anonymous comparator)
sorted: []User = users.sort(|a, b| a.age - b.age);
```

### Callbacks

```aria
async fn fetch_data(on_success: fn(Data), on_error: fn(Error)) {
    // Implementation
}

// Pass anonymous callbacks
await fetch_data(
    |data| {
        stdout << "Success: " << data << "\n";
    },
    |error| {
        stderr << "Error: " << error << "\n";
    }
);
```

---

## Closure Behavior

Anonymous functions can **capture** variables from surrounding scope:

```aria
multiplier: i32 = 10;

// Anonymous function captures 'multiplier'
scale = |x| x * multiplier;

Result: i32 = scale(5);  // 50
```

---

## Immediately Invoked Function Expression (IIFE)

```aria
// Define and call immediately
Result: i32 = (|| {
    temp: i32 = 42;
    temp = temp * 2;
    return temp;
})();  // Parentheses to invoke

stdout << result;  // 84
```

---

## Multiple Parameters

```aria
// Two parameters
add = |a, b| a + b;

// Three parameters
calculate = |x, y, z| (x + y) * z;

// Many parameters
format_user = |id: i32, name: string, age: i32| {
    return format("User {} ({}): {}", id, name, age);
};
```

---

## No Parameters

```aria
// Empty parameter list
get_random = || 42;

// With block
initialize = || {
    stdout << "Initializing...\n";
    setup_system();
    return true;
};
```

---

## Async Anonymous Functions

```aria
// Async lambda
fetch = async |url: string| {
    return await http_get(url);
};

// Usage
data: Data = await fetch("https://api.example.com");
```

---

## Capturing Variables

### Immutable Capture

```aria
prefix: string = "Hello";

// Captures 'prefix' by value
greet = |name| prefix + ", " + name;

stdout << greet("Alice");  // "Hello, Alice"
```

### Mutable Capture

```aria
count: i32 = 0;

// Captures 'count' by reference (using $)
increment = || {
    $count = $count + 1;
    return $count;
};

stdout << increment();  // 1
stdout << increment();  // 2
stdout << count;        // 2
```

---

## Best Practices

### ✅ DO: Use for Short, Inline Logic

```aria
// Good: Simple, clear
numbers.filter(|x| x > 0)
```

### ✅ DO: Use Meaningful Parameter Names

```aria
// Good: Clear parameter names
users.filter(|user| user.age >= 18)

// Avoid: Cryptic names
users.filter(|u| u.age >= 18)  // Less clear
```

### ✅ DO: Keep Anonymous Functions Short

```aria
// Good: 1-3 lines
process = |x| {
    validated: i32 = validate(x);
    return transform(validated);
};
```

### ❌ DON'T: Make Them Too Complex

```aria
// Wrong: Too complex for anonymous function
complicated = |data| {
    // 30 lines of complex logic...
};

// Right: Use named function
fn complicated_process(data: Data) -> Result {
    // 30 lines of complex logic...
}
```

### ❌ DON'T: Nest Too Deeply

```aria
// Wrong: Hard to read
result = array.map(|x| {
    other.filter(|y| {
        y.items.map(|z| z * x)
    })
});

// Right: Break it up
filtered = other.filter(|y| y.matches(criteria));
mapped = filtered.items.map(|z| z * factor);
```

---

## Comparison with Other Languages

### JavaScript

```javascript
// JavaScript
const double = x => x * 2;
const add = (a, b) => a + b;
```

### Aria

```aria
// Aria
double = |x| x * 2;
add = |a, b| a + b;
```

### Python

```python
# Python
double = lambda x: x * 2
add = lambda a, b: a + b
```

### Aria

```aria
// Aria
double = |x| x * 2;
add = |a, b| a + b;
```

---

## When to Use Anonymous vs Named

### Use Anonymous When:

- ✅ Function is used once
- ✅ Logic is simple (1-3 lines)
- ✅ Used as callback/argument
- ✅ In array operations (map/filter/reduce)

### Use Named Function When:

- ✅ Function is reused multiple times
- ✅ Logic is complex
- ✅ Needs documentation
- ✅ Part of public API

---

## Real-World Examples

### Data Pipeline

```aria
users: []User = load_users();

adults: []User = users
    .filter(|u| u.age >= 18)
    .filter(|u| u.active)
    .map(|u| User{...u, verified: true});
```

### Event System

```aria
event_bus.on("user_login", |user| {
    stdout << "User logged in: " << user.name << "\n";
    update_last_login(user);
});
```

### Retry Logic

```aria
Result: Data? = retry(3, || {
    return fetch_data();
});
```

---

## Related Topics

- [Lambda Expressions](lambda.md) - Detailed lambda guide
- [Lambda Syntax](lambda_syntax.md) - Syntax reference
- [Closures](closures.md) - Closure semantics
- [Higher-Order Functions](higher_order_functions.md) - Functions using lambdas

---

**Remember**: Anonymous functions are **nameless inline functions** - perfect for callbacks, array operations, and one-off logic!
