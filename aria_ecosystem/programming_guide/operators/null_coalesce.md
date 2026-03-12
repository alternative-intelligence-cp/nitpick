# Null Coalescing Operator (`??`)

**Category**: Operators → Safety  
**Operator**: `??`  
**Purpose**: Provide default values for NIL expressions

---

## Overview

The null coalescing operator (`??`) returns the left operand if it's not NIL, otherwise returns the right operand. It's the elegant way to handle optional values with fallback defaults.

**Philosophy**: Express "use this value, or if it's NIL, use that default" concisely.

---

## Basic Syntax

```aria
value ?? default        // Returns value if not NIL, else default
expr1 ?? expr2 ?? expr3 // Chain multiple fallbacks (left-to-right)
```

---

## Simple Examples

### Example 1: Variable with Default

```aria
obj?:user = getUser();

// Get user name or default to "Guest"
string:name = user?.name ?? "Guest";

// Get user age or default to 0
int64:age = user?.age ?? 0;
```

### Example 2: Configuration with Fallback

```aria
obj?:config = loadConfig();

// Use config port, or default to 8080
int64:port = config?.port ?? 8080;

// Use config host, or default to localhost
string:host = config?.host ?? "localhost";
```

---

## How It Works

### Evaluation Rules

```aria
a ?? b
```

1. Evaluate `a`
2. If `a` is **NOT NIL**: return `a` (don't evaluate `b`)
3. If `a` **IS NIL**: evaluate and return `b`

**Key**: Right side is only evaluated if left side is NIL (short-circuit).

---

## Chaining Multiple Defaults

```aria
// Try multiple sources, use first non-NIL
string:name = user?.name ?? user?.username ?? user?.email ?? "Anonymous";

// Configuration fallback chain
int64:timeout = 
    devConfig?.timeout ??
    prodConfig?.timeout ??
    defaultConfig?.timeout ??
    30;  // Final fallback
```

---

## With Safe Navigation (`?.`)

Perfect pairing - safe navigation produces NIL, `??` handles it:

```aria
// Safely access nested data with fallback
string:city = response?.data?.user?.address?.city ?? "Unknown";

// Get nested config value or default
bool:enabled = config?.features?.experimental?.enabled ?? false;

// Method call with fallback
obj:result = service?.process(data) ?? defaultResult;
```

---

## Type Coercion

Both sides should have compatible types:

```aria
// GOOD: Same types
string:name = user?.name ?? "Guest";      // string ?? string
int64:age = user?.age ?? 0;               // int64 ?? int64
bool:active = user?.active ?? false;      // bool ?? bool

// GOOD: Compatible types (compiler converts)
obj?:data = fetchData() ?? NIL;           // obj? ?? NIL

// BAD: Incompatible types (compile error)
string:bad = user?.age ?? "Unknown";      // ❌ int64 ?? string
```

---

## Common Patterns

### Pattern 1: Function Parameter Defaults

```aria
func:connect = result(string?:host, int64?:port) {
    // Use provided values or defaults
    string:actualHost = host ?? "localhost";
    int64:actualPort = port ?? 8080;
    
    Connection:conn = openConnection(actualHost, actualPort);
    pass(conn);
};

// Call with defaults
connect(NIL, NIL);             // Uses localhost:8080
connect("example.com", NIL);   // Uses example.com:8080
connect(NIL, 443);             // Uses localhost:443
```

### Pattern 2: Environment Variables

```aria
string:dbHost = getEnv("DB_HOST") ?? "localhost";
int64:dbPort = parseInt(getEnv("DB_PORT")) ?? 5432;
string:dbName = getEnv("DB_NAME") ?? "myapp";
```

### Pattern 3: Nested Object Defaults

```aria
obj?:settings = loadSettings();

// Deep defaults
string:theme = settings?.ui?.theme ?? "dark";
int64:fontSize = settings?.ui?.fontSize ?? 14;
bool:animations = settings?.ui?.animations ?? true;
```

### Pattern 4: API Response Fallbacks

```aria
obj?:response = apiCall();

// Provide sensible defaults for missing fields
obj:user = {
    name: response?.name ?? "Unknown User",
    email: response?.email ?? "no-email@example.com",
    age: response?.age ?? 0,
    active: response?.active ?? true
};
```

---

## With result Type

Combine with result unwrapping:

```aria
// Get result value or default
int64:value = computeValue() ? 0;  // Unwrap with default

// Explicitly
result<int64>:r = computeValue();
int64:value = !r.is_error ? raw(r) : 0;

// With ? operator (preferred)
int64:value = r ? 0;  // Unwrap with default
```

---

## Short-Circuit Evaluation

Right side is **only evaluated if left is NIL**:

```aria
// expensiveComputation() only runs if cheapCheck() returns NIL
result = cheapCheck() ?? expensiveComputation();

// Logging example
string:value = cache?.get(key) ?? {
    stderr.write("Cache miss, computing...");
    pass(computeExpensive());
};
```

---

## Nested ?? Expressions

```aria
// Parentheses for clarity
value = (a ?? b) ?? (c ?? d);

// Left-associative without parens
value = a ?? b ?? c ?? d;
// Same as: ((a ?? b) ?? c) ?? d
```

---

## Edge Cases

### NIL on Both Sides

```aria
obj?:a = NIL;
obj?:b = NIL;

obj?:result = a ?? b;  // NIL (both NIL)
```

### Non-NIL on Left

```aria
obj:a = {value: 42};
obj:b = {value: 100};

obj:result = a ?? b;  // a (left is not NIL, b never evaluated)
```

### Explicit NIL Assignment

```aria
obj?:value = NIL;
obj?:fallback = {default: true};

obj?:result = value ?? fallback;  // fallback
```

---

## Performance

### Zero Overhead

When left operand is not NIL, compiles to simple assignment:

```aria
// If value is not NIL:
result = value ?? default;

// Compiles to:
result = value;  // No check, no jump
```

### Short-Circuit Optimization

Right side is never evaluated if unnecessary:

```aria
// If user exists, getUserFromDB() never called
user = cachedUser ?? getUserFromDB();
```

---

## Comparison with Ternary

### Using ?? (Preferred for NIL checks)

```aria
// Clean and obvious
string:name = user?.name ?? "Guest";
```

### Using is (Ternary - More Verbose)

```aria
// More verbose for simple NIL check
string:name = is user?.name != NIL : user.name : "Guest";
```

### When to Use Each

```aria
// Use ?? for NIL coalescing
value = optional ?? default;

// Use 'is' for complex conditions
value = is score > 90 : "A" : is score > 80 : "B" : "C";
```

---

## Best Practices

### ✅ Use for Optional Values

```aria
// GOOD: Elegant optional handling
string:email = user?.email ?? "no-email@example.com";
```

### ✅ Provide Sensible Defaults

```aria
// GOOD: Reasonable fallbacks
int64:timeout = config?.timeout ?? 30;
int64:retries = config?.retries ?? 3;
bool:enabled = config?.enabled ?? true;
```

### ✅ Chain for Multiple Fallbacks

```aria
// GOOD: Try multiple sources
string:value = 
    primarySource?.get(key) ??
    secondarySource?.get(key) ??
    cache?.get(key) ??
    "default";
```

### ❌ Don't Use for Non-NIL Conditions

```aria
// BAD: ?? only checks NIL, not empty
string:name = user?.name ?? "Guest";  // Doesn't check if name is ""

// GOOD: Use is for complex conditions
string:name = is user?.name != NIL && user.name != "" : user.name : "Guest";
```

### ❌ Don't Ignore Type Safety

```aria
// BAD: Incompatible types
string:bad = user?.age ?? "Unknown";  // ❌ int64 ?? string

// GOOD: Convert types explicitly
string:age = toString(user?.age) ?? "Unknown";
```

---

## Error Handling Integration

### With result Type

```aria
Result<obj>:r = fetchData();

// Check error first, then use ??
if (r.err != NULL) {
    stderr.write(`Error: &{r.err}`);
    fail(1);
}

// Use ?? on success value
string:name = raw(r)?.name ?? "Unknown";
```

### Providing Error Defaults

```aria
// Get value or log error and use default
obj:data = fetchData() ? {
    stderr.write("Failed to fetch data, using default");
    pass(defaultData);
};
```

---

## Comparison with Other Languages

### Aria

```aria
value = optional ?? default;
```

### JavaScript / TypeScript

```javascript
const value = optional ?? default;
```

### C# (Null-Coalescing)

```csharp
var value = optional ?? default;
```

### PHP

```php
$value = $optional ?? $default;
```

### Python (walrus := with or)

```python
value = optional or default  # Similar but not identical
```

### Kotlin (Elvis Operator)

```kotlin
val value = optional ?: default
```

---

## Advanced Patterns

### Pattern: Lazy Evaluation

```aria
// Compute default only if needed
value = cache?.get(key) ?? computeExpensive(key);
```

### Pattern: Fallback Chain

```aria
// Try multiple approaches
connection = 
    tryHTTPS() ??
    tryHTTP() ??
    tryFallbackProtocol() ??
    offlineMode();
```

### Pattern: Nested Defaults

```aria
// Deeply nested with multiple fallbacks
display = user
    ?.profile
    ?.displayName 
    ?? user?.username 
    ?? user?.email 
    ?? "Anonymous";
```

---

## Related Topics

- [Safe Navigation Operator](safe_navigation.md) - `?.` for safe property access
- [NIL vs NULL](../types/nil_null_void.md) - Understanding NIL
- [Optional Types](../types/optional.md) - Using `?` suffix
- [Ternary Operator](is_ternary.md) - `is` for complex conditions
- [result Type](../types/result.md) - Error handling

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 187  
**Critical for**: Handling optional values elegantly, configuration defaults, API responses
