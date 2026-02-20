# fail Keyword

**Category**: Functions → Result Handling  
**Purpose**: Extract and handle error states from TBB results  
**Philosophy**: Make error handling explicit and first-class

---

## What is `fail`?

`fail` is Aria's keyword for **extracting error information** when a TBB result is ERR. It's the opposite of `pass`:

- **pass**: Extracts success value, propagates ERR
- **fail**: Handles ERR case, allows success to continue

---

## Basic Syntax

```aria
when fail operation() then
    // This block executes if operation() returned ERR
    // Handle the error
end
```

---

## How It Works

### Simple Error Handling

```aria
fn divide_safe(a: tbb32, b: tbb32) -> tbb32 {
    Result: tbb32 = divide(a, b);
    
    when fail result then
        stderr << "ERROR: Division failed (divide by zero?)\n";
        return 0;  // Fallback value
    end
    
    return result;
}
```

**If `divide` returns ERR**:
- `fail result` is true
- Error handling block executes
- Function returns fallback value

**If `divide` succeeds**:
- `fail result` is false
- Error block is skipped
- Function returns actual result

---

## Inline Error Checking

```aria
fn process_file(filename: string) -> bool {
    when fail open_file(filename) then
        stderr << "ERROR: Cannot open file: " << filename << "\n";
        return false;
    end
    
    // File opened successfully, continue processing
    return true;
}
```

---

## Common Patterns

### With Fallback Values

```aria
fn parse_number_safe(s: string) -> tbb32 {
    num: tbb32 = parse(s);
    
    when fail num then
        stddbg << "Parse failed, returning default";
        return 0;  // Default value
    end
    
    return num;
}
```

### With Error Messages

```aria
fn load_config(path: string) -> Config? {
    file: File? = open_file(path);
    
    when fail file then
        stderr << "ERROR: Cannot load config from " << path << "\n";
        stderr << "  Using default configuration\n";
        return default_config();
    end
    
    content: string = pass file.read();
    config: Config = pass parse_json(content);
    
    return config;
}
```

### Chaining Error Checks

```aria
fn process_pipeline(input: string) -> Data? {
    step1: Data? = parse(input);
    when fail step1 then
        stderr << "ERROR: Parse failed\n";
        return ERR;
    end
    
    step2: Data? = validate(step1);
    when fail step2 then
        stderr << "ERROR: Validation failed\n";
        return ERR;
    end
    
    step3: Data? = transform(step2);
    when fail step3 then
        stderr << "ERROR: Transform failed\n";
        return ERR;
    end
    
    return step3;
}
```

---

## `fail` with `else`

```aria
fn get_value(key: string) -> tbb32 {
    value: tbb32 = lookup(key);
    
    when fail value then
        stderr << "Key not found: " << key << "\n";
        return 0;
    else
        stddbg << "Found value: " << value;
        return value;
    end
}
```

---

## Combining `fail` and `pass`

```aria
fn robust_process(data: string) -> Result {
    // Try primary method
    Result: Result? = try_primary(data);
    
    when fail result then
        stddbg << "Primary method failed, trying fallback";
        
        // Try fallback method
        result = try_fallback(data);
        
        when fail result then
            stderr << "ERROR: Both methods failed\n";
            return ERR;
        end
    end
    
    // At this point, result is guaranteed to be valid
    return pass finalize(result);
}
```

---

## Error Recovery Patterns

### Retry Logic

```aria
fn fetch_with_retry(url: string, max_attempts: i32) -> Data? {
    till(max_attempts - 1, 1) {
        attempt: i32 = $ + 1;
        stddbg << "Attempt " << attempt << "/" << max_attempts;
        
        data: Data? = fetch(url);
        
        when fail data then
            when attempt < max_attempts then
                stddbg << "Retrying...";
                continue;
            else
                stderr << "ERROR: All retry attempts failed\n";
                return ERR;
            end
        end
        
        // Success!
        return data;
    }
    
    return ERR;  // Shouldn't reach here
}
```

### Graceful Degradation

```aria
fn get_user_preference(user_id: i32, key: string) -> string {
    // Try user-specific preference
    pref: string? = db.get_user_pref(user_id, key);
    
    when fail pref then
        // Fallback to default preference
        pref = db.get_default_pref(key);
        
        when fail pref then
            // Ultimate fallback: hardcoded default
            stddbg << "Using hardcoded default for " << key;
            return "default_value";
        end
    end
    
    return pref;
}
```

---

## Error Logging

```aria
fn process_with_logging(item: Item) -> Result? {
    Result: Result? = process(item);
    
    when fail result then
        // Log error details
        stderr << "ERROR: Processing failed\n";
        stderr << "  Item ID: " << item.id << "\n";
        stderr << "  Item type: " << item.type << "\n";
        stderr << "  Timestamp: " << Time::now() << "\n";
        
        // Also log to debug stream
        stddbg << "Full item data: " << item.serialize();
        
        return ERR;
    end
    
    return result;
}
```

---

## Best Practices

### ✅ DO: Provide Context in Error Messages

```aria
when fail parse_json(data) then
    stderr << "ERROR: Failed to parse JSON\n";
    stderr << "  Data length: " << data.len() << " bytes\n";
    stderr << "  First 100 chars: " << data.substr(0, 100) << "\n";
    return ERR;
end
```

### ✅ DO: Use Fallback Values When Appropriate

```aria
// Configuration with sensible defaults
timeout: tbb32 = load_timeout_config();

when fail timeout then
    stddbg << "Using default timeout";
    timeout = 30;  // 30 seconds default
end
```

### ✅ DO: Log Before Propagating Errors

```aria
when fail critical_operation() then
    stderr << "CRITICAL: Operation failed, aborting\n";
    stddbg << "Stack trace: " << get_stack_trace();
    return ERR;  // Propagate error upward
end
```

### ❌ DON'T: Silently Ignore Errors

```aria
// Wrong: Error is ignored
Result: tbb32 = dangerous_operation();
when fail result then
    // Empty block - error is lost!
end

// Right: At least log it
when fail dangerous_operation() then
    stderr << "ERROR: Operation failed\n";
    return ERR;
end
```

### ❌ DON'T: Use `fail` with Non-TBB Types

```aria
// Wrong: i32 doesn't have ERR
value: i32 = get_number();
when fail value then  // Compile error!
    // ...
end

// Right: Use TBB types
value: tbb32 = get_number();
when fail value then
    // ...
end
```

---

## `fail` vs `pass` Decision Guide

**Use `pass` when**:
- You want errors to propagate automatically
- No special error handling needed
- Building a pipeline of operations

**Use `fail` when**:
- You want to handle the error case explicitly
- You need to log error details
- You have a fallback/default value
- You want to retry or recover

### Example Comparison

```aria
// Using pass: Simple propagation
fn simple_process(file: string) -> Data? {
    content: string = pass read_file(file);
    data: Data = pass parse(content);
    return data;
    // Errors propagate automatically
}

// Using fail: Explicit handling
fn robust_process(file: string) -> Data? {
    content: string? = read_file(file);
    
    when fail content then
        stderr << "ERROR: Cannot read " << file << "\n";
        content = try_backup_file(file + ".bak");
        
        when fail content then
            return ERR;  // Both attempts failed
        end
    end
    
    data: Data = pass parse(content);
    return data;
}
```

---

## Error State Introspection

```aria
fn diagnose_error(value: tbb32) {
    when fail value then
        stdout << "Value is in error state (ERR)\n";
        stddbg << "ERR value: " << value;  // Shows -128 for tbb8, etc.
    else
        stdout << "Value is valid: " << value << "\n";
    end
}
```

---

## Real-World Example: Database Query

```aria
fn fetch_user_safely(user_id: i32) -> User? {
    // Try to fetch from cache first
    user: User? = cache.get_user(user_id);
    
    when fail user then
        stddbg << "Cache miss for user " << user_id;
        
        // Fetch from database
        user = db.query_user(user_id);
        
        when fail user then
            stderr << "ERROR: User " << user_id << " not found in database\n";
            stddbg << "Database connection status: " << db.status();
            return ERR;
        end
        
        // Cache the result for next time
        cache.set_user(user_id, user);
        stddbg << "Cached user " << user_id;
    end
    
    return user;
}
```

---

## Error Context with Multiple Failures

```aria
fn complex_operation(data: Data) -> Result? {
    // Try operation A
    result_a: Result? = try_method_a(data);
    
    when fail result_a then
        stddbg << "Method A failed, trying B";
        
        // Try operation B
        result_b: Result? = try_method_b(data);
        
        when fail result_b then
            stddbg << "Method B failed, trying C";
            
            // Try operation C
            result_c: Result? = try_method_c(data);
            
            when fail result_c then
                stderr << "ERROR: All methods (A, B, C) failed\n";
                return ERR;
            end
            
            return result_c;
        end
        
        return result_b;
    end
    
    return result_a;
}
```

---

## Related Topics

- [pass Keyword](pass_keyword.md) - Opposite of fail (extracts success)
- [TBB Types](../types/tbb8.md) - Types that support ERR
- [when/then/end](../control_flow/when_then_end.md) - Conditional control flow
- [Sticky Errors](../types/sticky_errors.md) - Error propagation

---

**Remember**: `fail` makes error handling **explicit and first-class**. Use it when you need to do something specific with errors - log them, recover from them, or provide fallbacks.
