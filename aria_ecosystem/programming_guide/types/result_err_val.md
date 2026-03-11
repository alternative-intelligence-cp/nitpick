# Result Values in Aria

**Category**: Types → Result  
**Purpose**: Creating and checking `result<T>` values

---

## Overview

`result<T>` is Aria’s type for operations that can either succeed with a value or fail with an error.

| Field | Type | Meaning |
|-------|------|---------|
| `.is_error` | `bool` | `true` if the operation failed |
| `.err` | `string` | Error message (valid when `is_error == true`) |

Use `raw()` to extract the underlying value (checks are your responsibility).

---

## Producing Results from Functions

```aria
func:divide = result<int32>(int32:a, int32:b) {
    if (b == 0) {
        fail("Division by zero");  // Returns error result
    }
    pass(a / b);  // Returns success result
};
```

---

## Checking State

```aria
result<int32>:r = divide(10, 2);

if (r.is_error) {
    print(`Error: &{r.err}`);  // r.err is the error string
} else {
    int32:value = raw(r);      // raw() extracts the value after checking
    print(`Result: &{value}`);
}
```

---

## Accessing the Error

```aria
result<string>:result = readFile("config.txt");

if (result.is_error) {
    // .err is the error message string
    stderr.write(`File error: &{result.err}\n`);
    fail(result.err);  // propagate it
}
```

---

## Propagating Success

```aria
func:loadConfig = result<Config>() {
    string:content = readFile("config.txt") ? "{}";  // default on error
    pass(parseJSON(content)?);  // propagate parse error upward
};
```

---

## Common Patterns

### Default on error
```aria
string:config = readFile("config.txt") ? "default";
```

### Must succeed
```aria
Config:cfg = loadConfig()?!;  // triggers failsafe if it fails
```

### Check and use
```aria
result<string>:r = readFile("data.txt");
if (!r.is_error) {
    process(raw(r));
}
```

### Propagate in a result-returning function
```aria
func:process = result<NIL>(string:path) {
    string:content = readFile(path)?;  // propagate on failure
    transform(content)?;
    pass(NIL);
};
```

---

## Related

- [result<T>](result.md)
- [Result Extraction](result_unwrap.md)

---

**Remember**: `pass()` — success. `fail()` — error. Check `.is_error` before accessing `.err`.
