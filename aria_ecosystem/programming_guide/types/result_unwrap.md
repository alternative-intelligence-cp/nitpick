# Result Unwrap

**Category**: Types → Result  
**Purpose**: Extracting values from Result

---

## Basic Unwrap

```aria
// Unsafe - panics if Err
Result: Result<i32> = Ok(42);
value: i32 = result.unwrap();  // 42

// Panics!
error: Result<i32> = Err("fail");
value: i32 = error.unwrap();  // ❌ Runtime panic!
```

---

## Unwrap with Default

```aria
// Safe - provides default on error
Result: Result<i32> = Err("fail");
value: i32 = result.unwrap_or(0);  // 0
```

---

## Unwrap with Function

```aria
Result: Result<i32> = Err("fail");

value: i32 = result.unwrap_or_else(fn() -> i32 {
    return calculate_fallback();
});
```

---

## Expect (With Message)

```aria
// Like unwrap but with custom panic message
value: i32 = result.expect("Failed to get value");
```

---

## Unwrap Error

```aria
// Get error value (panics if Ok)
Result: Result<i32> = Err("failure");
error: string = result.unwrap_err();  // "failure"
```

---

## Best Practices

### ✅ DO: Use unwrap_or for Defaults

```aria
value: i32 = result.unwrap_or(DEFAULT_VALUE);
```

### ✅ DO: Use Pattern Matching

```aria
when result is Ok(v) then
    process(v);
elsif result is Err(e) then
    handle_error(e);
end
```

### ❌ DON'T: Unwrap Without Checking

```aria
value: i32 = result.unwrap();  // ❌ Might panic!
```

---

## Related

- [Result](result.md)
- [Result Err/Val](result_err_val.md)

---

**Remember**: `unwrap()` **panics on error** - use carefully!
