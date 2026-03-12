# Ternary Operator (`is`)

**Category**: Operators → Control Flow  
**Keyword**: `is`  
**Purpose**: Inline conditional expression (ternary operator)

---

## Overview

The `is` keyword provides ternary conditional logic in expression form. It evaluates a condition and returns one of two values based on the result.

**Philosophy**: Express simple conditionals inline without verbose if/else blocks.

---

## Basic Syntax

```aria
is condition : trueValue : falseValue
```

- If `condition` is true, returns `trueValue`
- If `condition` is false, returns `falseValue`

---

## Simple Examples

### Example 1: Basic Conditional

```aria
int64:age = 25;

// Traditional if/else
string:status;
if (age >= 18) {
    status = "adult";
} else {
    status = "minor";
}

// With is operator (concise)
string:status = is age >= 18 : "adult" : "minor";
```

### Example 2: Assignment

```aria
int64:score = 85;

// Assign grade based on score
string:grade = is score >= 90 : "A" : "B";

// Sign of a number
int64:sign = is value >= 0 : 1 : -1;

// Boolean to int
int64:flag = is enabled : 1 : 0;
```

---

## Nested Ternaries

Chain multiple conditions:

```aria
int64:score = 87;

// Nested ternary for multiple conditions
string:grade = is score >= 90 : "A" :
               is score >= 80 : "B" :
               is score >= 70 : "C" :
               is score >= 60 : "D" :
               "F";

// Equivalent if/else chain
string:grade;
if (score >= 90) {
    grade = "A";
} else if (score >= 80) {
    grade = "B";
} else if (score >= 70) {
    grade = "C";
} else if (score >= 60) {
    grade = "D";
} else {
    grade = "F";
}
```

---

## Type Consistency

Both branches must return compatible types:

```aria
// GOOD: Both return int64
int64:value = is condition : 42 : 0;

// GOOD: Both return string
string:msg = is error : "Failed" : "Success";

// BAD: Incompatible types (compile error)
mixed:bad = is condition : 42 : "string";  // ❌
```

---

## With Null Coalescing (`??`)

Combine for powerful optional handling:

```aria
// Get value from result, or use ternary for default based on error
result<int64>:r = compute();
int64:value = !r.is_error ? raw(r) : (r.err == 1 ? 0 : -1);

// Simpler with ? operator
int64:value = r ? 0;  // Unwrap with default
```

---

## Common Patterns

### Pattern 1: Min/Max

```aria
// Minimum of two values
int64:min = is a < b : a : b;

// Maximum of two values
int64:max = is a > b : a : b;
```

### Pattern 2: Clamping

```aria
// Clamp value to range [0, 100]
int64:clamped = is value < 0 : 0 : is value > 100 : 100 : value;
```

### Pattern 3: Sign Function

```aria
// Return -1, 0, or 1 based on sign
int64:sign = is value > 0 : 1 : is value < 0 : -1 : 0;
```

### Pattern 4: Conditional String Building

```aria
string:message = is success : 
    `Operation completed in &{duration}ms` :
    `Operation failed: &{errorMsg}`;

print(message);
```

### Pattern 5: Toggle Boolean

```aria
// Toggle: true becomes false, false becomes true
bool:toggled = is current : false : true;

// Or simpler with NOT
bool:toggled = !current;
```

---

## With result Type

```aria
Result<int64>:r = compute();

// Extract value or default based on error
int64:value = r ? -1;

// More complex error handling  
string:message = !r.is_error ?
    `Success: &{raw(r)}` :
    `Error &{r.err}: &{getErrorMessage(r.err)}`;
```

---

## Short-Circuit Evaluation

Only the taken branch is evaluated:

```aria
// If condition is true, expensiveB() is NEVER called
result = is condition : expensiveA() : expensiveB();

// Example: Avoid division by zero
int64:safeDivide = is b != 0 : a / b : 0;
```

---

## Complex Conditions

```aria
// Multiple conditions with AND
status = is age >= 18 && hasLicense : "can drive" : "cannot drive";

// Multiple conditions with OR
access = is isAdmin || isModerator : "granted" : "denied";

// Parentheses for clarity
value = is (score > 90) && (attendance > 80) : "excellent" : "good";
```

---

## Edge Cases

### Empty Strings and Zero

```aria
// Careful with "falsy" values
string:name = "";
string:display = is name != "" : name : "Anonymous";  // Explicit check needed

// Zero is not treated as false
int64:count = 0;
string:status = is count > 0 : "items" : "no items";  // Explicit comparison
```

### NIL Checks

```aria
obj?:user = getUser();

// Check for NIL explicitly
string:name = is user != NIL : user.name : "Guest";

// Or use ?? (clearer for NIL checks)
string:name = user?.name ?? "Guest";
```

---

## Comparison with if/else

### Use `is` When:

- ✅ Simple two-branch condition
- ✅ Assigning based on condition
- ✅ Inline expression needed
- ✅ Short, readable branches

```aria
// GOOD: Simple assignment
int64:abs = is x >= 0 : x : -x;
```

### Use if/else When:

- ❌ Complex logic in branches
- ❌ Multiple statements per branch
- ❌ Side effects (mutations, I/O)
- ❌ Readability suffers

```aria
// BAD: Too complex for is
value = is condition :
    {
        print("Branch 1");
        computeA();
        logResult();
        pass(result);
    } :
    {
        print("Branch 2");
        computeB();
        logResult();
        pass(other);
    };

// GOOD: Use if/else instead
int64:value;
if (condition) {
    print("Branch 1");
    value = computeA();
    logResult();
} else {
    print("Branch 2");
    value = computeB();
    logResult();
}
```

---

## Performance

### Zero Overhead

The `is` operator compiles to the same conditional branch as if/else:

```aria
// This is operator...
value = is x > 0 : 1 : -1;

// ...compiles identically to:
int64:value;
if (x > 0) {
    value = 1;
} else {
    value = -1;
}
```

**No runtime cost difference.**

---

## Best Practices

### ✅ Use for Simple Conditionals

```aria
// GOOD: Clear and concise
status = is ready : "GO" : "WAIT";
sign = is value >= 0 : "+" : "-";
```

### ✅ Format Multi-Line for Readability

```aria
// GOOD: Easy to read
grade = is score >= 90 : "A" :
        is score >= 80 : "B" :
        is score >= 70 : "C" :
        "F";
```

### ✅ Use ?? for NIL Checks

```aria
// LESS CLEAR: Using is for NIL
name = is user != NIL : user.name : "Guest";

// MORE CLEAR: Using ??
name = user?.name ?? "Guest";
```

### ❌ Don't Nest Too Deeply

```aria
// BAD: Hard to read
value = is a : is b : is c : 1 : 2 : is d : 3 : 4 : 5;

// GOOD: Use if/else or match
value = match(a, b, c, d) {
    case (true, true, true, _) => 1,
    case (true, true, false, _) => 2,
    case (true, false, _, true) => 3,
    case (true, false, _, false) => 4,
    default => 5
};
```

### ❌ Don't Use for Side Effects

```aria
// BAD: Side effects in ternary
_ = is condition : print("True") : print("False");

// GOOD: Use if/else for side effects
if (condition) {
    print("True");
} else {
    print("False");
}
```

---

## Advanced Examples

### Example 1: Bitwise Operations

```aria
// Get bit at position
int64:bit = is (value & (1 << pos)) != 0 : 1 : 0;

// Set or clear bit
int64:modified = is setBit : value | (1 << pos) : value & ~(1 << pos);
```

### Example 2: Array Bounds

```aria
// Safe array access with bounds check
int64:safe = is index >= 0 && index < array.length : array[index] : -1;
```

### Example 3: Configuration Selection

```aria
// Select config based on environment
obj:config = is isProduction :
    productionConfig :
    is isDevelopment :
        devConfig :
        testConfig;
```

### Example 4: Mathematical Functions

```aria
// Absolute value
int64:abs = is x >= 0 : x : -x;

// Clamp to range
int64:clamped = is x < min : min : is x > max : max : x;

// Step function
int64:step = is x >= threshold : 1 : 0;
```

---

## Comparison with Other Languages

### Aria

```aria
value = is condition : trueVal : falseVal;
```

### C / C++ / Java

```c
value = condition ? trueVal : falseVal;
```

### Python

```python
value = trueVal if condition else falseVal
```

### Ruby

```ruby
value = condition ? trueVal : falseVal
```

### Rust

```rust
let value = if condition { trueVal } else { falseVal };
```

---

## With Other Operators

### With Unwrap (`?`)

```aria
// Unwrap result with ternary fallback — use ? operator instead
int64:value = computeValue() ? defaultValue;

// Or with explicit check:
result<int64>:r = computeValue();
int64:value = !r.is_error ? raw(r) : defaultValue;
```

### With Pipeline (`|>`)

```aria
// Conditional pipeline
result = data
    |> (is validateFirst : validate : identity)
    |> transform
    |> (is debugMode : logAndReturn : identity);
```

### With Safe Navigation (`?.`)

```aria
// Ternary with safe navigation
name = is user != NIL :
    user?.profile?.name ?? "Unknown" :
    "Not logged in";
```

---

## Related Topics

- [Null Coalescing](null_coalesce.md) - `??` for NIL handling (often clearer)
- [if/else](../control_flow/if_else.md) - Block-based conditionals
- [pick](../control_flow/pick.md) - Pattern matching (for complex conditions)
- [Unwrap Operator](unwrap.md) - `?` for result unwrapping
- [Boolean Logic](../operators/logical.md) - `&&`, `||`, `!`

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 192  
**Use Case**: Inline conditionals, simple branching, functional-style code
