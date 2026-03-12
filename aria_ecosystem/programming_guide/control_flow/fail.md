# The `fail` Keyword

**Category**: Control Flow → Function Return  
**Syntax**: `fail(errorCode)`  
**Purpose**: Return an error from a function

---

## Overview

The `fail` keyword is Aria's way to return an error from a function. It creates a `result` type with:
- `err` set to the provided error code (non-NULL)
- `val` set to `NULL` (no value available)

Every function in Aria returns a `result` type, making `fail` the standard way to signal errors.

---

## Syntax

```aria
func:functionName = returnType(parameters) {
    // ... function body ...
    fail(errorCode);  // Return error with code
};
```

The error code is typically an integer representing the type of error that occurred.

---

## Basic Examples

### Division by Zero
```aria
func:divide = Result<int8>(int8:a, int8:b) {
    if (b == 0) {
        fail(1);  // Error code 1: division by zero
    }
    pass(a / b);
};

int8:result = divide(10, 0) ? -1;  // result = -1 (error, uses default)
```

### Validation Error
```aria
func:validateAge = Result<int8>(int8:age) {
    if (age < 0) {
        fail(1);  // Error code 1: negative age
    }
    if (age > 150) {
        fail(2);  // Error code 2: unrealistic age
    }
    pass(age);
};
```

### File Not Found
```aria
// Simulated file reading
func:readConfig = Result<string>(string:filename) {
    if (filename == "") {
        fail(1);  // Error code 1: empty filename
    }
    // Actual file reading would happen here
    // If file doesn't exist:
    fail(2);  // Error code 2: file not found
};
```

---

## Error Codes

Error codes are application-defined integers that represent different error conditions:

```aria
// Common pattern: define error code constants
int8:ERR_INVALID_INPUT = 1;
int8:ERR_NOT_FOUND = 2;
int8:ERR_PERMISSION_DENIED = 3;
int8:ERR_TIMEOUT = 4;

func:processRequest = Result<obj>(string:request) {
    if (request == "") {
        fail(ERR_INVALID_INPUT);
    }
    
    obj:data = fetchData(request) ? fail(ERR_NOT_FOUND);
    
    if (!hasPermission(data)) {
        fail(ERR_PERMISSION_DENIED);
    }
    
    pass(data);
};
```

---

## The `result` Type Structure

When you call `fail(code)`, Aria creates a `result` with:

```aria
result {
    err: errorCode,  // The error code you provided
    val: NULL        // No value available
}
```

This is why unwrapping with a default is safe:
```aria
func:getValue = Result<int8>() {
    fail(404);
};

result<int8>:r = getValue();
// r.is_error == true, r.err == 404
int8:x = r ? 0;  // x = 0 (error occurred, uses default)
```

---

## Checking for Errors

### Explicit Error Checking
```aria
result<T>:r = someFunction();
if (r.is_error) {
    stderr.write(`Error occurred: &{r.err}`);
} else {
    print(`Success: &{raw(r)}`);
}
```

### Using Unwrap with Default
```aria
int8:value = someFunction() ? 0;
// If error: value = 0 (default)
// If success: value = raw(r)
```

### Early Return Pattern
```aria
func:processMultipleSteps = Result<int8>() {
    int8:step1 = doStep1() ? fail(1);
    int8:step2 = doStep2(step1) ? fail(2);
    int8:step3 = doStep3(step2) ? fail(3);
    pass(step3);
};
```

---

## Propagating Errors

The `fail` keyword can propagate errors from nested function calls:

```aria
func:readAndParse = Result<obj>(string:filename) {
    // If readFile fails, propagate its error
    string:content = readFile(filename) ? fail(1);
    
    // If readJSON fails, propagate different error code
    obj:data = readJSON(content) ? fail(2);
    
    pass(data);
};
```

Or propagate the exact error:
```aria
func:wrapper = Result<string>(string:file) {
    result<string>:r = readFile(file);
    if (r.is_error) {
        fail(r.err);  // Propagate exact error code
    }
    pass(raw(r));
};
```

---

## Multiple Error Paths

Functions can have multiple `fail` statements for different error conditions:

```aria
func:validateUser = Result<obj>(string:username, string:password) {
    if (username == "") {
        fail(1);  // Empty username
    }
    if (password.length < 8) {
        fail(2);  // Password too short
    }
    if (!isAlphanumeric(username)) {
        fail(3);  // Invalid characters
    }
    
    obj:user = { username: username, password: password };
    pass(user);
};
```

---

## Generic Functions

The `fail` keyword works with any return type:

```aria
func<T>:maybeGet = *T(array:arr, int8:index) {
    if (index < 0 || index >= arr.length) {
        fail(1);  // Out of bounds
    }
    pass(arr[index]);
};

int8:value = maybeGet<int8>([1, 2, 3], 5) ? 0;  // value = 0 (error)
```

---

## Comparison with `pass`

| Aspect | `fail(errorCode)` | `pass(value)` |
|--------|-------------------|---------------|
| Purpose | Error return | Success return |
| `result.err` | Error code | `NULL` |
| `result.is_error` | `true` | `false` |
| Value access | N/A (error state) | `raw(result)` |
| Unwrap behavior | Returns default or panics | Returns value |
| When to use | Validation fails, operation impossible | Operation succeeded |

---

## Error Handling Strategies

### 1. Fail Fast
```aria
func:processData = Result<int8>(string:data) {
    if (data == "") fail(1);
    if (data.length > 1000) fail(2);
    // ... process data ...
    pass(result);
};
```

### 2. Try Multiple Approaches
```aria
func:loadConfig = Result<string>() {
    result<string>:primary = readFile("config.json");
    if (!primary.is_error) {
        pass(raw(primary));
    }
    
    result<string>:backup = readFile("config.default.json");
    if (!backup.is_error) {
        pass(raw(backup));
    }
    
    fail(1);  // Both failed
};
```

### 3. Collect Errors
```aria
func:validateAll = Result<array>(array:items) {
    array:errors = [];
    for (int8:i = 0; i < items.length; i += 1) {
        result<bool>:r = validate(items[i]);
        if (r.is_error) {
            errors[errors.length] = r.err;
        }
    }
    
    if (errors.length > 0) {
        fail(1);  // At least one validation failed
    }
    
    pass(items);
};
```

---

## Best Practices

1. **Use meaningful error codes**: Document what each code means
2. **Fail early**: Check preconditions at the start of functions
3. **Be consistent**: Use the same error codes for the same conditions across your codebase
4. **Document error codes**: Comment or use constants for error code meanings
5. **Don't swallow errors**: Propagate errors up the call stack when appropriate
6. **Validate inputs**: Use `fail` for invalid input rather than crashing

---

## Common Patterns

### Validation → Fail
```aria
func:checkPositive = Result<int8>(int8:num) {
    if (num <= 0) {
        fail(1);
    }
    pass(num);
};
```

### Resource Not Available → Fail
```aria
func:acquireResource = Result<obj>() {
    if (!resourceAvailable()) {
        fail(1);
    }
    pass(getResource());
};
```

### Timeout → Fail
```aria
func:waitForResponse = Result<string>(int8:timeoutMs) {
    // ... wait logic ...
    if (timedOut) {
        fail(1);
    }
    pass(response);
};
```

---

## Error Logging

Combine `fail` with logging for better debugging:

```aria
func:criticalOperation = Result<int8>(string:input) {
    if (input == "") {
        stderr.write("ERROR: Empty input to criticalOperation");
        fail(1);
    }
    
    result<int8>:r = dangerousOperation(input);
    if (r.is_error) {
        stderr.write(`ERROR: Operation failed with code &{r.err}`);
        fail(2);
    }
    
    pass(raw(r));
};
```

---

## Performance

`fail` is a **zero-cost abstraction**:
- No runtime overhead beyond returning an error code
- Compiler optimizes `result` structure
- Equivalent to direct return in languages with explicit error handling

---

## Related Topics

- [pass Keyword](./pass.md) - Return success from functions
- [result Type](../types/result.md) - The return type structure
- [Unwrap Operator](../operators/unwrap.md) - Extract values from results
- [Error Handling](../advanced_features/error_handling.md) - Complete error handling guide
- [Function Declaration](../functions/function_declaration.md) - Function syntax

---

**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Complete specification
