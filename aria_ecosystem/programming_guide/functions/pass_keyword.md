# pass Keyword

**Category**: Functions → Result Handling  
**Purpose**: Propagate success values from TBB results  
**Philosophy**: Errors flow automatically, success needs explicit handling

---

## What is `pass`?

`pass` is Aria's keyword for **extracting success values** from TBB (Twisted Balanced Binary) result types while allowing errors to propagate automatically.

**Think of it as**: "If this succeeded, pass the value forward. If it failed (ERR), abort and return ERR immediately."

---

## Basic Syntax

```aria
value: Type = pass function_that_might_fail();
```

If the function returns a success value, `value` gets that value.  
If the function returns `ERR`, the current function immediately returns `ERR`.

---

## How It Works

### Without `pass` (Manual Result Handling)

```aria
func:process_file = result<int32>(string:filename) {
    // Manual error checking (tedious!)
    result<File>:file_result = open_file(filename);
    
    if file_result.is_error then
        fail(file_result.err);
    end
    
    File:file = raw(file_result);
    
    result<string>:content_result = file.read();
    
    if content_result.is_error then
        fail(content_result.err);
    end
    
    string:content = raw(content_result);
    
    // Actually process content...
    pass(content.len());
};
```

### With `pass` + `?` Propagation (Clean)

```aria
func:process_file = result<int32>(string:filename) {
    File:file = open_file(filename)?;      // ? propagates error to caller
    string:content = file.read()?;
    
    pass(content.len());  // Explicit success return
};
```

**Result**: If either `open_file` or `read` fails, the function immediately returns `ERR`. Otherwise, we get the success values.

---

## TBB Result Types

`pass` works with **TBB types** that use the ERR sentinel:

### TBB Integers
```aria
fn divide(a: tbb32, b: tbb32) -> tbb32 {
    when b == 0 then
        return ERR;  // Division by zero
    end
    
    return a / b;
}

// Using pass
fn calculate() -> tbb32 {
    Result: tbb32 = pass divide(10, 2);  // Gets 5
    return result * 2;  // Returns 10
}

// Error propagation
fn calculate_fail() -> tbb32 {
    Result: tbb32 = pass divide(10, 0);  // divide returns ERR
    // Function exits here, returns ERR
    return result * 2;  // Never executed
}
```

---

## Common Patterns

### Chaining Operations

```aria
fn process_data(input: string) -> tbb32 {
    // Each step might fail, pass propagates errors
    parsed: Data = pass parse(input);
    validated: Data = pass validate(parsed);
    processed: Data = pass transform(validated);
    Result: tbb32 = pass save(processed);
    
    return result;  // Success
}
```

**Benefit**: Clean, linear code. Errors propagate automatically.

### File I/O Chain

```aria
fn load_config(path: string) -> Config? {
    file: File = pass open_file(path);
    content: string = pass file.read();
    config: Config = pass parse_json(content);
    validated: Config = pass validate_config(config);
    
    return validated;
}
```

### Multiple Fallible Operations

```aria
fn calculate_average(numbers: []tbb32) -> tbb32 {
    when numbers.len() == 0 then
        return ERR;  // Can't average empty array
    end
    
    sum: tbb32 = 0;
    till(numbers.length - 1, 1) {
        num: tbb32 = numbers[$];
        sum = pass add_checked(sum, num);  // Overflow-safe addition
    }
    
    return pass divide(sum, numbers.len());
}
```

---

## Error Context

### Adding Context to Errors

```aria
fn load_user(id: i32) -> User? {
    user: User = pass fetch_user(id);  // Returns ERR if not found
    
    stddbg << "Loaded user: " << user.name;
    
    return user;
}

// Caller can check for ERR
user: User? = load_user(123);

when user == ERR then
    stderr << "ERROR: User not found\n";
end
```

---

## `pass` vs `?` Operator

In some languages, `?` is used for error propagation. Aria uses `pass` for clarity:

### Other Languages (Rust example)
```rust
fn process() -> Result<i32, Error> {
    let value = some_operation()?;  // ? propagates errors
    Ok(value * 2)
}
```

### Aria
```aria
fn process() -> tbb32 {
    value: tbb32 = pass some_operation();
    return value * 2;
}
```

**Aria's approach**: `pass` is a keyword, not punctuation. More explicit, easier to search for in code.

---

## When `pass` Returns ERR

```aria
fn outer() -> tbb32 {
    Result: tbb32 = pass failing_function();
    // If failing_function() returns ERR:
    // 1. outer() immediately returns ERR
    // 2. Lines below never execute
    
    stdout << "This won't print if error occurred\n";
    return result;
}

fn failing_function() -> tbb32 {
    return ERR;  // Simulate failure
}

// Usage
value: tbb32 = outer();
// value is ERR (propagated from failing_function)
```

---

## Combining `pass` with Validation

```aria
fn create_user(name: string, age: tbb32) -> User? {
    // Validate age
    validated_age: tbb32 = pass check_age_valid(age);
    
    // Validate name
    when name.len() == 0 then
        return ERR;  // Empty name not allowed
    end
    
    // Both validations passed
    user: User = User{name: name, age: validated_age};
    return user;
}

fn check_age_valid(age: tbb32) -> tbb32 {
    when age < 0 or age > 150 then
        return ERR;
    end
    return age;
}
```

---

## Best Practices

### ✅ DO: Use `pass` for Error Propagation

```aria
// Good: Clean error handling
fn load_and_process(file: string) -> tbb32 {
    data: Data = pass load(file);
    Result: tbb32 = pass process(data);
    return result;
}
```

### ✅ DO: Chain Fallible Operations

```aria
// Good: Multiple steps, clear flow
fn pipeline(input: string) -> Result? {
    step1: Data = pass parse(input);
    step2: Data = pass validate(step1);
    step3: Data = pass transform(step2);
    return step3;
}
```

### ✅ DO: Add Debug Context

```aria
fn process(id: i32) -> Data? {
    stddbg << "Processing ID: " << id;
    
    data: Data = pass fetch(id);
    
    stddbg << "Fetched data: " << data.size() << " bytes";
    
    return data;
}
```

### ❌ DON'T: Use `pass` with Non-TBB Types

```aria
// Wrong: i32 doesn't have ERR sentinel
value: i32 = pass get_number();  // Compile error!

// Right: Use TBB types
value: tbb32 = pass get_number();
```

### ❌ DON'T: Ignore Errors Silently

```aria
// Wrong: Error is lost
value: tbb32 = some_operation();  // Might be ERR, no check!
use_value(value);  // Might propagate error unexpectedly

// Right: Use pass to propagate or check explicitly
value: tbb32 = pass some_operation();  // Propagates ERR
// OR
value: tbb32 = some_operation();
when value == ERR then
    // Handle error explicitly
end
```

---

## Sticky Error Propagation

`pass` leverages **TBB sticky errors**:

```aria
fn compute() -> tbb32 {
    a: tbb32 = pass get_value();  // Might return ERR
    b: tbb32 = pass get_value();
    
    // If either a or b is ERR, this returns ERR
    return a + b;  // TBB arithmetic propagates ERR
}
```

**Key insight**: Even if you don't use `pass`, TBB arithmetic will propagate ERR. But `pass` catches errors **immediately**, making control flow clearer.

---

## Comparison with Traditional Error Handling

### Traditional (Python-style)
```python
def process_file(filename):
    try:
        file = open(filename)
        content = file.read()
        data = parse(content)
        result = process(data)
        return result
    except Exception as e:
        return None  # Or raise
```

### Aria with `pass`
```aria
fn process_file(filename: string) -> Result? {
    file: File = pass open(filename);
    content: string = pass file.read();
    data: Data = pass parse(content);
    Result: Result = pass process(data);
    return result;
}
```

**Aria advantages**:
- No try/catch boilerplate
- Errors propagate at type level (ERR sentinel)
- Clear, linear flow
- Compiler tracks error states

---

## Real-World Example

```aria
fn process_transaction(txn_id: string) -> Transaction? {
    stddbg << "Processing transaction: " << txn_id;
    
    // Fetch transaction from database
    txn: Transaction = pass db.fetch_transaction(txn_id);
    
    // Validate transaction data
    validated: Transaction = pass validate_transaction(txn);
    
    // Check account balance
    account: Account = pass db.fetch_account(validated.account_id);
    balance: tbb64 = pass account.get_balance();
    
    when balance < validated.amount then
        stderr << "ERROR: Insufficient funds\n";
        return ERR;
    end
    
    // Apply transaction
    new_balance: tbb64 = pass subtract_checked(balance, validated.amount);
    pass db.update_balance(account.id, new_balance);
    
    stddbg << "Transaction complete, new balance: " << new_balance;
    
    return validated;
}
```

**If any step fails**:
- Returns `ERR` immediately
- No need for nested if/else
- Clean, readable code

---

## Related Topics

- [fail Keyword](fail_keyword.md) - Opposite of pass (extracts error)
- [TBB Types](../types/tbb8.md) - Twisted Balanced Binary with ERR
- [Function Return Types](function_return_type.md) - TBB return types
- [Sticky Errors](../types/sticky_errors.md) - How ERR propagates

---

**Remember**: `pass` means "pass the success value forward, or abort with ERR". It's Aria's clean solution to error handling without exceptions or Result<T, E> boilerplate.
