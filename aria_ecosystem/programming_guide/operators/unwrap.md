# The Unwrap Operator (`?`)

**Category**: Operators → Error Handling  
**Syntax**: `expression ?` or `expression ? default_value`  
**Purpose**: Extract the value from a `result` type or provide a default on error

---

## Overview

The unwrap operator (`?`) is Aria's primary mechanism for concise error handling. It operates on the `result` type, which is returned by all functions and contains two fields:
- `err` - Error code (NULL if successful)
- `val` - Return value (NULL if error occurred)

The `?` operator provides two modes:
1. **Panic mode**: `expr ?` - Extract value or panic if error exists
2. **Default mode**: `expr ? default` - Extract value or use default if error exists

---

## Syntax Forms

### Basic Unwrap (Panic on Error)
```aria
Result:r = someFunction();
type:value = r ?;  // Panics if r.err != NULL
```

### Unwrap with Default
```aria
Result:r = someFunction();
type:value = r ? defaultValue;  // Returns defaultValue if r.err != NULL
```

### Inline Unwrap
```aria
// Most common pattern - unwrap function call directly
int8:value = someFunction() ? 0;
string:text = readFile("config.txt") ? "default config";
```

---

## How It Works

The unwrap operator checks the `err` field of a `result`:

```aria
// Conceptually equivalent to:
Result:r = someFunction();
type:value = is r.err == NULL : r.val : defaultValue;
```

**With default value**:
- If `r.err == NULL`: Returns `r.val` (success case)
- If `r.err != NULL`: Returns the default value (error case)

**Without default value** (panic mode):
- If `r.err == NULL`: Returns `r.val`
- If `r.err != NULL`: Program panics with error information

---

## Common Use Cases

### 1. Function Call with Fallback
```aria
// Read file, use default if it fails
string:config = readFile("app.config") ? "default config";

// Parse JSON, use empty object on error
obj:settings = readJSON("settings.json") ? { theme: "dark" };

// Read CSV, use empty array on error
array:data = readCSV("data.csv") ? [];
```

### 2. Arithmetic Operations
```aria
func:divide = Result<int8>(int8:a, int8:b) {
    if (b == 0) {
        fail(1);  // Return error
    }
    pass(a / b);  // Return success
};

// Unwrap with default
int8:result = divide(10, 2) ? 0;  // result = 5
int8:safe = divide(10, 0) ? 0;    // safe = 0 (error, uses default)
```

### 3. Chained Operations
```aria
// Multiple unwraps in sequence
int8:final = someFunction() ? 0 + anotherFunction() ? 0;

// Nested function calls
int8:value = process(getData() ? 0) ? 0;
```

### 4. Generic Function Returns
```aria
func<T>:identity = *T(*T:value) {
    pass(value);
};

// Unwrap generic result
int8:num = identity<int8>(42) ? 0;        // num = 42
string:str = identity<string>("test") ? ""; // str = "test"
```

---

## Advanced Patterns

### Unwrap in Lambda with Immediate Execution
```aria
// Lambda returns result, execute immediately, unwrap result
func:test = int8(int8:a, int8:b) { pass(a + b); };
Result:r = test(
    int8(int8:x, int8:y) { pass(x * y); }(4, 5) ? 0,  // Execute & unwrap = 20
    12
);
int8:final = r ? 0;  // final = 32
```

### Conditional Unwrap
```aria
Result:file = readFile("optional.txt");
string:content = is file.err == NULL : file.val : "File not found";

// Equivalent to:
string:content = readFile("optional.txt") ? "File not found";
```

### Unwrap with Type Inference
```aria
// Type inferred from function return
int8:x = getValue() ? 0;  // Compiler infers getValue returns Result<int8>
```

---

## Comparison with Explicit Checking

**Verbose (explicit error checking)**:
```aria
Result:r = readFile("config.txt");
string:config;
if (r.err == NULL) {
    config = r.val;
} else {
    config = "default config";
}
```

**Concise (unwrap operator)**:
```aria
string:config = readFile("config.txt") ? "default config";
```

Both are functionally equivalent, but the unwrap operator is:
- More concise (1 line vs 6 lines)
- More readable (intent is clearer)
- Less error-prone (no forgotten error checks)

---

## Error Propagation Pattern

The unwrap operator is often used for early returns in error scenarios:

```aria
func:processData = Result<int8>() {
    // If any operation fails, return error immediately
    string:data = readFile("data.txt") ? fail(1);
    obj:parsed = parseJSON(data) ? fail(2);
    int8:value = extracted(parsed) ? fail(3);
    
    pass(value);  // All succeeded
};
```

**Note**: This pattern requires the default value to match the function's return type or use `fail()` to create an error result.

---

## Panic Mode (No Default)

Using `?` without a default value will panic if an error occurs:

```aria
// This will panic if divide returns an error
int8:value = divide(10, 0) ?;  // PANIC: division by zero
```

**Use sparingly**: Panic mode is appropriate when:
- The error should never happen (programming bug if it does)
- Recovery is impossible
- You want to fail-fast during development

**Prefer default values** for production code to handle errors gracefully.

---

## Type Safety

The unwrap operator is type-safe:

```aria
Result:r = someFunction();  // Returns Result<int8>
int8:value = r ? 0;          // ✅ Correct: default matches type
string:text = r ? "error";   // ❌ ERROR: Type mismatch
```

The default value must match the type contained in the result.

---

## Performance

The unwrap operator is a **zero-cost abstraction**:
- Compiles to a simple branch (if/else)
- No runtime overhead compared to explicit checking
- Optimizes well (branch predictor friendly)

---

## Common Mistakes

### 1. Wrong Default Type
```aria
// ❌ WRONG: Default type doesn't match result type
int8:value = getString() ? 0;  // getString returns Result<string>

// ✅ CORRECT: Matching types
string:value = getString() ? "";
```

### 2. Forgetting Unwrap
```aria
Result:r = getValue();
int8:x = r;  // ❌ WRONG: r is Result<int8>, not int8

// ✅ CORRECT: Must unwrap
int8:x = r ? 0;
```

### 3. Ignoring Errors Silently
```aria
// ⚠️ QUESTIONABLE: Errors are silently replaced with 0
int8:value = criticalOperation() ? 0;

// ✅ BETTER: Log or handle errors explicitly when critical
Result:r = criticalOperation();
if (r.err != NULL) {
    stderr.write("Critical error: &{r.err}");
    fail(1);
}
int8:value = r.val;
```

---

## Related Topics

- [result Type](../types/result.md) - The type that `?` operates on
- [pass Keyword](../control_flow/pass.md) - Return success from functions
- [fail Keyword](../control_flow/fail.md) - Return errors from functions
- [Error Handling](../advanced_features/error_handling.md) - Complete error handling guide
- [is Operator](./is_operator.md) - Ternary conditional for explicit error checking

---

## Examples from Real Code

### File I/O with Defaults
```aria
Result:file_result = readFile("config.txt");
string:content = is file_result.err == NULL : file_result.val : "";
if (file_result.err != NULL) {
    stderr.write("Could not read config file");
}

// Equivalent using unwrap:
string:content = readFile("config.txt") ? "";
```

### Configuration Loading
```aria
string:config = readFile("app.config") ? "default config";
obj:settings = readJSON("settings.json") ? { theme: "dark" };
array:data = readCSV("data.csv") ? [];
```

### Process Management
```aria
Result:child = spawn("./worker", ["--input", "data.txt"]);
if (child.err == NULL) {
    int32:pid = child.val;
    Result:exit = wait(pid);
    int8:exit_code = exit ? -1;
    print(`Worker finished with code: &{exit_code}`);
} else {
    stderr.write(`Failed to spawn worker: &{child.err}`);
}
```

---

## Best Practices

1. **Use defaults for expected errors**: File not found, network timeout, etc.
2. **Use panic mode for programming errors**: Null pointer dereference, array bounds
3. **Match default values semantically**: Empty string for text, 0 for counts, NULL for pointers
4. **Log critical errors**: Don't silently swallow important failures
5. **Chain carefully**: Too many `?` in one expression reduces readability

---

**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Complete specification
