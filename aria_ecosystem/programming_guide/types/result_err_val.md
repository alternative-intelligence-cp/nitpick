# Result Err and Val

**Category**: Types → Result  
**Purpose**: Ok and Err constructors

---

## Ok Constructor

```aria
// Success with value
Result: Result<i32> = Ok(42);

// Success with void
Result: Result<void> = Ok();
```

---

## Err Constructor

```aria
// Error with string message
Result: Result<i32> = Err("Something went wrong");

// Error with custom type
Result: Result<i32, ErrorCode> = Err(ErrorCode.NotFound);
```

---

## Pattern Matching

```aria
when result is Ok(value) then
    stdout << "Success: $value";
elsif result is Err(error) then
    stderr << "Error: $error";
end
```

---

## Checking State

```aria
// Check if Ok
when result.is_ok() then
    value: i32 = result.unwrap();
end

// Check if Err
when result.is_err() then
    msg: string = result.unwrap_err();
end
```

---

## Related

- [Result](result.md)
- [Result Unwrap](result_unwrap.md)

---

**Remember**: `Ok` = success, `Err` = failure!
