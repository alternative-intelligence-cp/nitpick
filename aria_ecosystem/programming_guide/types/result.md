# The `result` Type

**Category**: Types → Core Types  
**Syntax**: `Result<T>`  
**Purpose**: Unified return type for all functions, enabling explicit error handling

---

## Overview

The `result` type is the foundation of Aria's error handling system. **Every function in Aria returns a `result`**, which contains two fields:

- **`err`** - Error code (NULL if successful, non-NULL if error occurred)
- **`val`** - Return value (contains the value if successful, NULL if error occurred)

This design makes error handling explicit, type-safe, and impossible to ignore.

---

## Structure

```aria
Result<T> {
    err: int8 | NULL,  // NULL = success, non-NULL = error code
    val: T | NULL      // The actual return value, or NULL on error
}
```

### Field Behavior

| State | `err` | `val` | Meaning |
|-------|-------|-------|---------|
| Success | `NULL` | `<value>` | Operation succeeded, value available |
| Error | `<code>` | `NULL` | Operation failed with error code |

---

## Basic Usage

### Returning a Result
```aria
func:add = Result<int8>(int8:a, int8:b) {
    pass(a + b);  // Creates: { err: NULL, val: a + b }
};

func:divide = Result<int8>(int8:a, int8:b) {
    if (b == 0) {
        fail(1);  // Creates: { err: 1, val: NULL }
    }
    pass(a / b);  // Creates: { err: NULL, val: a / b }
};
```

### Unwrapping a Result
```aria
// Using the ? operator (most common)
int8:sum = add(5, 3) ? 0;        // sum = 8
int8:quotient = divide(10, 0) ? 0;  // quotient = 0 (error, uses default)

// Explicit field access
Result:r = divide(10, 2);
if (r.err == NULL) {
    print(`Success: &{r.val}`);  // Prints: Success: 5
} else {
    print(`Error: &{r.err}`);
}
```

---

## Generic Type Parameter

The `result` type is generic over `T`, which represents the type of the successful return value:

```aria
func:getString = Result<string>() {
    pass("Hello");
};

func:getNumber = Result<int8>() {
    pass(42);
};

func:getArray = Result<array>() {
    pass([1, 2, 3]);
};

// Unwrap with appropriate defaults
string:str = getString() ? "";
int8:num = getNumber() ? 0;
array:arr = getArray() ? [];
```

---

## Error Checking Patterns

### Pattern 1: Unwrap with Default (Most Common)
```aria
int8:value = someFunction() ? 0;
// If success: value = result.val
// If error: value = 0
```

### Pattern 2: Explicit Error Check
```aria
Result:r = someFunction();
if (r.err != NULL) {
    stderr.write(`Error: &{r.err}`);
    // Handle error
} else {
    int8:value = r.val;
    // Use value
}
```

### Pattern 3: Early Return on Error
```aria
func:processData = Result<int8>() {
    int8:step1 = doStep1() ? fail(1);
    int8:step2 = doStep2(step1) ? fail(2);
    pass(step2);
};
```

### Pattern 4: Check Success
```aria
Result:r = someFunction();
bool:success = r.err == NULL;
if (success) {
    // Use r.val
}
```

---

## Real-World Examples

### File I/O
```aria
// readFile returns Result<string>
Result:file_result = readFile("config.txt");

// Pattern 1: Unwrap with default
string:content = readFile("config.txt") ? "";

// Pattern 2: Explicit handling
if (file_result.err != NULL) {
    stderr.write("Could not read config file");
} else {
    string:content = file_result.val;
    print(content);
}
```

### JSON Parsing
```aria
// readJSON returns Result<obj>
obj:config = readJSON("settings.json") ? { theme: "dark" };

// Or explicit:
Result:json_result = readJSON("settings.json");
if (json_result.err == NULL) {
    obj:config = json_result.val;
} else {
    stderr.write(`JSON parse error: &{json_result.err}`);
}
```

### Process Management
```aria
// spawn returns Result<int32> (PID)
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

## Chaining Operations

Results can be chained using the unwrap operator:

```aria
// Each operation returns a result, unwrapped inline
obj:data = readJSON(readFile("data.json") ? "") ? {};

// More readable with intermediate variables:
string:json = readFile("data.json") ? "";
obj:data = readJSON(json) ? {};
```

---

## Error Propagation

Functions can propagate errors from callees:

```aria
func:loadConfig = Result<obj>() {
    // If readFile fails, immediately return its error
    string:content = readFile("app.config") ? fail(1);
    
    // If readJSON fails, return different error
    obj:config = readJSON(content) ? fail(2);
    
    pass(config);
};
```

Or propagate exact errors:
```aria
func:wrapper = Result<string>(string:file) {
    Result:r = readFile(file);
    if (r.err != NULL) {
        fail(r.err);  // Propagate exact error code
    }
    pass(r.val);
};
```

---

## Comparison with Other Languages

### Aria (Explicit)
```aria
Result:r = divide(10, 2);
if (r.err != NULL) {
    // Handle error
}
int8:value = r.val;
```

### Go (Similar)
```go
value, err := divide(10, 2)
if err != nil {
    // Handle error
}
// Use value
```

### Rust (Similar)
```rust
let Result: Result<i8, i8> = divide(10, 2);
match result {
    Ok(value) => // Use value,
    Err(e) => // Handle error,
}
```

### C (No Explicit Error)
```c
int value = divide(10, 2);  // Error silently ignored or checked via errno
```

---

## Type Safety

The `result` type is fully type-safe:

```aria
func:getNumber = Result<int8>() { pass(42); };
func:getString = Result<string>() { pass("hello"); };

int8:num = getNumber() ? 0;     // ✅ Correct
string:str = getString() ? "";  // ✅ Correct

int8:x = getString() ? 0;       // ❌ ERROR: Type mismatch
string:y = getNumber() ? "";    // ❌ ERROR: Type mismatch
```

---

## Why Every Function Returns `result`

1. **Explicit Error Handling**: Errors cannot be silently ignored
2. **Type Safety**: Compiler enforces error checking
3. **Consistency**: All functions follow the same pattern
4. **Composability**: Easy to chain operations with `?`
5. **Performance**: Zero-cost abstraction when optimized

---

## Performance

The `result` type is a **zero-cost abstraction**:
- Compiles to simple value + flag representation
- No heap allocation
- Optimizes to direct value return when errors are impossible
- Modern CPUs predict error branches efficiently

---

## Advanced: Result with Complex Types

### Arrays
```aria
func:getNumbers = Result<array>(int8:count) {
    array:nums = [];
    for (int8:i = 0; i < count; i += 1) {
        nums[i] = i;
    }
    pass(nums);
};

array:result = getNumbers(5) ? [];
```

### Structs
```aria
struct:User = {
    string:name,
    int8:age
};

func:createUser = Result<User>(string:name, int8:age) {
    if (name == "") {
        fail(1);
    }
    if (age < 0) {
        fail(2);
    }
    pass({ name: name, age: age });
};

User:user = createUser("Alice", 30) ? { name: "", age: 0 };
```

### Functions
```aria
func:getHandler = Result<func>(string:type) {
    if (type == "add") {
        pass(int8(int8:a, int8:b) { pass(a + b); });
    } else {
        fail(1);
    }
};

func:handler = getHandler("add") ? int8(int8:a, int8:b) { pass(0); };
```

---

## Common Mistakes

### 1. Forgetting to Unwrap
```aria
Result:r = getValue();
int8:x = r;  // ❌ WRONG: r is Result<int8>, not int8

// ✅ CORRECT: Must unwrap
int8:x = r ? 0;
```

### 2. Wrong Default Type
```aria
// ❌ WRONG: Default type doesn't match
int8:value = getString() ? 0;

// ✅ CORRECT: Match the return type
string:value = getString() ? "";
```

### 3. Ignoring Errors Silently
```aria
// ⚠️ QUESTIONABLE: Error silently becomes default
int8:critical = criticalOperation() ? 0;

// ✅ BETTER: Log errors
Result:r = criticalOperation();
if (r.err != NULL) {
    stderr.write(`Critical error: &{r.err}`);
}
int8:critical = r ? 0;
```

---

## Best Practices

1. **Always handle errors**: Use `?` with appropriate defaults or explicit checks
2. **Document error codes**: Comment what each error code means
3. **Use meaningful defaults**: Choose defaults that make sense for your domain
4. **Propagate when appropriate**: Don't catch and ignore errors that should bubble up
5. **Log critical errors**: Use stderr for important failures
6. **Check before unwrap**: Use explicit checks for critical operations

---

## Related Topics

- [pass Keyword](../control_flow/pass.md) - Return success from functions
- [fail Keyword](../control_flow/fail.md) - Return errors from functions
- [Unwrap Operator](../operators/unwrap.md) - Extract values from results
- [Error Handling](../advanced_features/error_handling.md) - Complete error handling guide
- [Generics](../functions/generics.md) - Generic type system

---

**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Complete specification
