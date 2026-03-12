# The `pass` Keyword

**Category**: Control Flow → Function Return  
**Syntax**: `pass(value)`  
**Purpose**: Return success from a function with a value

---

## Overview

The `pass` keyword is Aria's way to return successfully from a function. It creates a `result` type with:
- `err` set to `NULL` (indicating success)
- `val` set to the provided value

Every function in Aria returns a `result` type, making `pass` the standard way to return values.

---

## Syntax

```aria
func:functionName = returnType(parameters) {
    // ... function body ...
    pass(value);  // Return success with value
};
```

---

## Basic Examples

### Simple Return
```aria
func:add = Result<int8>(int8:a, int8:b) {
    pass(a + b);  // Returns { err: NULL, val: a + b }
};

int8:sum = add(5, 3) ? 0;  // sum = 8
```

### String Return
```aria
func:greet = Result<string>(string:name) {
    pass(`Hello, &{name}!`);
};

string:msg = greet("World") ? "";  // msg = "Hello, World!"
```

### Boolean Return
```aria
func:isPositive = Result<bool>(int8:num) {
    pass(num > 0);
};

bool:result = isPositive(42) ? false;  // result = true
```

---

## Conditional Returns

### With if/else
```aria
func:absolute = Result<int8>(int8:num) {
    if (num < 0) {
        pass(-num);
    } else {
        pass(num);
    }
};
```

### Early Return on Success
```aria
func:findValue = Result<int8>(array:arr, int8:target) {
    for (int8:i = 0; i < arr.length; i += 1) {
        if (arr[i] == target) {
            pass(i);  // Found it, return immediately
        }
    }
    fail(1);  // Not found, return error
};
```

---

## Generic Functions

The `pass` keyword works with any return type, including generics:

```aria
func<T>:identity = *T(*T:value) {
    pass(value);  // Returns value of type T
};

int8:num = identity<int8>(42) ? 0;        // num = 42
string:str = identity<string>("test") ? ""; // str = "test"
```

---

## Comparison with `fail`

| Aspect | `pass(value)` | `fail(errorCode)` |
|--------|---------------|-------------------|
| Purpose | Success return | Error return |
| `result.err` | `NULL` | Error code |
| `result.is_error` | `false` | `true` |
| Value access | `raw(result)` | N/A (error state) |
| Unwrap behavior | Returns value | Returns default or panics |

### Example Showing Both
```aria
func:divide = Result<int8>(int8:a, int8:b) {
    if (b == 0) {
        fail(1);  // Error: division by zero
    }
    pass(a / b);  // Success: return result
};

int8:result1 = divide(10, 2) ? 0;  // result1 = 5 (success)
int8:result2 = divide(10, 0) ? 0;  // result2 = 0 (error, uses default)
```

---

## The `result` Type Structure

When you call `pass(value)`, Aria creates a `result` with:

```aria
result {
    err: NULL,     // No error occurred
    val: value     // The value you passed
}
```

This is why unwrapping works:
```aria
func:getValue = Result<int8>() {
    pass(42);
};

result<int8>:r = getValue();
// r.is_error == false
int8:x = r ? 0;  // x = 42 (unwraps raw(r))
```

---

## Complex Return Values

### Returning Arrays
```aria
func:getNumbers = Result<array>(int8:count) {
    array:nums = [];
    for (int8:i = 0; i < count; i += 1) {
        nums[i] = i;
    }
    pass(nums);
};

array:result = getNumbers(5) ? [];  // result = [0, 1, 2, 3, 4]
```

### Returning Objects
```aria
func:createUser = Result<obj>(string:name, int8:age) {
    pass({ name: name, age: age });
};

obj:user = createUser("Alice", 30) ? { name: "", age: 0 };
```

### Returning Structs
```aria
struct:Point = {
    int8:x,
    int8:y
};

func:makePoint = Result<Point>(int8:x, int8:y) {
    Point:p = { x: x, y: y };
    pass(p);
};
```

---

## Multiple Return Points

Functions can have multiple `pass` statements:

```aria
func:categorize = Result<string>(int8:num) {
    if (num < 0) {
        pass("negative");
    } else if (num == 0) {
        pass("zero");
    } else {
        pass("positive");
    }
};

string:cat = categorize(-5) ? "unknown";  // cat = "negative"
```

---

## With Lambdas

Lambda functions also use `pass`:

```aria
// Lambda with pass
func:square = int8(int8:x) {
    pass(x * x);
};

int8:result = square(5) ? 0;  // result = 25

// Immediate execution
int8:value = int8(int8:x) { pass(x + 10); }(5) ? 0;  // value = 15
```

---

## Error Handling Pattern

Common pattern: validate inputs, then `pass` on success:

```aria
func:processData = Result<int8>(string:data) {
    if (data == "") {
        fail(1);  // Empty input
    }
    
    obj:parsed = readJSON(data) ? fail(2);  // Parse error
    
    if (parsed.value == NULL) {
        fail(3);  // Missing field
    }
    
    pass(parsed.value);  // Success!
};
```

---

## Best Practices

1. **Always use `pass` for successful returns**: Even if void-like, Aria functions return `result`
2. **Be consistent with return types**: All `pass` calls in a function must return the same type
3. **Validate before `pass`**: Check preconditions and `fail` early, `pass` at the end
4. **Keep return types explicit**: Use type annotations for clarity
5. **Document what success means**: Comment when success has specific meanings

---

## Common Patterns

### Validation → Pass
```aria
func:validateAge = Result<int8>(int8:age) {
    if (age < 0 || age > 150) {
        fail(1);
    }
    pass(age);  // Valid age
};
```

### Transform → Pass
```aria
func:double = Result<int8>(int8:x) {
    pass(x * 2);
};
```

### Lookup → Pass or Fail
```aria
func:findIndex = Result<int8>(array:arr, int8:target) {
    for (int8:i = 0; i < arr.length; i += 1) {
        if (arr[i] == target) {
            pass(i);  // Found
        }
    }
    fail(1);  // Not found
};
```

---

## Performance

`pass` is a **zero-cost abstraction**:
- No runtime overhead beyond returning a value
- Compiler optimizes `result` access patterns
- Equivalent to direct return in languages without explicit error handling

---

## Related Topics

- [fail Keyword](./fail.md) - Return errors from functions
- [result Type](../types/result.md) - The return type structure
- [Unwrap Operator](../operators/unwrap.md) - Extract values from results
- [Error Handling](../advanced_features/error_handling.md) - Complete error handling guide
- [Function Declaration](../functions/function_declaration.md) - Function syntax

---

**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Complete specification
