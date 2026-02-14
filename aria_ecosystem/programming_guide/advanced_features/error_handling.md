# Error Handling

**Category**: Advanced Features → Best Practices  
**Purpose**: Comprehensive error handling strategies

---

## Overview

Effective error handling makes code **robust** and **maintainable**.

---

## Result Type

```aria
// Use Result for fallible operations
func:read_config = Result<Config, string>(string:path) {
    string:content = readFile(path)?;
    Config:config = parse_config(content)?;
    pass(config);
};

// Caller handles error
Result<Config, string>:result = read_config("config.toml");
if result.err != NULL then
    stderr.write("Failed to load config: {}\n", result.err);
else
    use_config(result.val);
end
```

---

## Custom Error Types

```aria
enum AppError {
    IO(string),
    Parse(string),
    Network(string),
    NotFound,
    PermissionDenied,
}

impl AppError {
    func:message = string() {
        pick self {
            AppError.IO(msg) => pass("IO error: " + msg);
            AppError.Parse(msg) => pass("Parse error: " + msg);
            AppError.Network(msg) => pass("Network error: " + msg);
            AppError.NotFound => pass("Resource not found");
            AppError.PermissionDenied => pass("Permission denied");
        }
    };
}

func:fetch_user = Result<User, AppError>(int32:id) {
    User | NULL:user = database.find(id);
    
    if user != NULL then
        pass(user);
    else
        fail(AppError.NotFound);
    end
};
```

---

## Error Context

```aria
func:load_user_data = Result<UserData, string>(int32:user_id) {
    Result<User, AppError>:user_result = fetch_user(user_id);
    if user_result.err != NULL then
        fail("Failed to fetch user " + string(user_id));
    end
    User:user = user_result.val;
    
    Result<Profile, AppError>:profile_result = fetch_profile(user_id);
    if profile_result.err != NULL then
        fail("Failed to fetch profile for user " + string(user_id));
    end
    Profile:profile = profile_result.val;
    
    Result<Post[], AppError>:posts_result = fetch_posts(user_id);
    if posts_result.err != NULL then
        fail("Failed to fetch posts for user " + string(user_id));
    end
    Post[]:posts = posts_result.val;
    
    pass(UserData{ user, profile, posts });
};

// Error output:
// Failed to fetch posts for user 42
// Caused by: Network error: Connection timeout
```

---

## Early Return Pattern

```aria
func:validate_user = Result<nil, string>(User:user) {
    if user.name == "" then
        fail("Name cannot be empty");
    end
    
    if user.age < 0 then
        fail("Age cannot be negative");
    end
    
    if user.age > 150 then
        fail("Age too high");
    end
    
    if !user.email.contains("@") then
        fail("Invalid email format");
    end
    
    pass(nil);
};
```

---

## Recoverable Errors

```aria
func:fetch_with_fallback = Result<Data, string>(string:url) {
    // Try primary source
    Result<Data, string>:primary = fetch_from_primary(url);
    if primary.err == NULL then
        pass(primary.val);
    end
    
    stdout.write("Primary failed: {}, trying backup...\n", primary.err);
    
    // Try backup source
    Result<Data, string>:backup = fetch_from_backup(url);
    if backup.err == NULL then
        pass(backup.val);
    end
    
    stderr.write("Backup also failed: {}\n", backup.err);
    
    // Return cached data if available
    Data | NULL:cached = get_from_cache(url);
    if cached != NULL then
        pass(cached);
    else
        fail("All sources failed");
    end
};
```

---

## Retry Logic

```aria
func:fetch_with_retry = Result<Data, string>(string:url, int32:max_retries) {
    int32:retries = 0;
    
    loop
        Result<Data, string>:result = fetch(url);
        if result.err == NULL then
            pass(result.val);
        end
        
        retries = retries + 1;
        
        if retries >= max_retries then
            fail("Max retries exceeded: " + result.err);
        end
        
        // Exponential backoff
        int32:delay = 1000 * (2 ** (retries - 1));
        stdout.write("Retry {}/{} after {}ms\n", retries, max_retries, delay);
        sleep(delay);
    end
};
```

---

## Error Conversion

```aria
impl From<IoError> for AppError {
    func:from = AppError(IoError:e) {
        pass(AppError.IO(e.to_string()));
    };
}

impl From<ParseError> for AppError {
    func:from = AppError(ParseError:e) {
        pass(AppError.Parse(e.to_string()));
    };
}

func:process = Result<Data, AppError>() {
    // Explicit error handling
    Result<string, IoError>:file_result = readFile("data.txt");
    if file_result.err != NULL then
        fail(AppError.IO(file_result.err.to_string()));
    end
    string:content = file_result.val;
    
    Result<Data, ParseError>:parse_result = parse(content);
    if parse_result.err != NULL then
        fail(AppError.Parse(parse_result.err.to_string()));
    end
    
    pass(parse_result.val);
};
```

---

## Panic vs Result

```aria
// ✅ Use Result for expected errors
func:read_user_input = Result<int32, string>() {
    Result<string, string>:line_result = stdin.read_line();
    if line_result.err != NULL then
        fail(line_result.err);
    end
    string:input = line_result.val;
    
    Result<int32, string>:parse_result = input.parse();
    if parse_result.err != NULL then
        fail("Invalid number");
    end
    
    pass(parse_result.val);
};

// ✅ Use panic for programming errors
func:calculate = float64(int32:divisor) {
    if divisor == 0 then
        !!! "Division by zero - this should never happen!";
    end
    pass(100.0 / float64(divisor));
};

// ✅ Use assertions in debug builds
func:process_array = (int32[]:arr, int32:index) {
    dbug.check("validation", "Index {} out of bounds (len {})", index, arr.len, index < arr.len());
    // Process arr[index]
};
```

---

## Error Logging

```aria
func:process_with_logging = Result<nil, string>() {
    Result<string, string>:result = risky_operation();
    if result.err == NULL then
        log_info("Operation succeeded: " + result.val);
        pass(nil);
    else
        log_error("Operation failed: " + result.err);
        log_debug("Stack trace: " + result.err.backtrace);
        fail(result.err);
    end
};
```

---

## Validation Chain

```aria
func:register_user = Result<User, string>(UserData:data) {
    validate_name(data.name)?;
    validate_email(data.email)?;
    validate_password(data.password)?;
    validate_age(data.age)?;
    
    check_email_unique(data.email)?;
    check_username_available(data.username)?;
    
    User:user = create_user(data)?;
    send_confirmation_email(user.email)?;
    
    pass(user);
};

func:validate_email = Result<nil, string>(string:email) {
    if !email.contains("@") then
        fail("Invalid email format");
    end
    
    if email.len() < 3 then
        fail("Email too short");
    end
    
    pass(nil);
};
```

---

## Best Practices

### ✅ DO: Use Result for Fallible Operations

```aria
func:parse_number = Result<int32, string>(string:s) {
    Result<int32, string>:result = s.parse();
    if result.err != NULL then
        fail("Invalid number format");
    end
    pass(result.val);
};
```

### ✅ DO: Provide Context

```aria
func:load_data = Result<Data, string>(int32:id) {
    Result<Data, string>:result = fetch(id);
    if result.err != NULL then
        fail("Failed to load data for ID " + string(id) + ": " + result.err);
    end
    pass(result.val);
};
```

### ✅ DO: Handle Errors at Appropriate Level

```aria
// Low-level: propagate
func:read_file = Result<string, string>(string:path) {
    pass(readFile(path));  // Propagate error via Result
};

// High-level: handle
func:main = int32() {
    Result<string, string>:file_result = read_file("config.toml");
    if file_result.err != NULL then
        stderr.write("Fatal error: {}\n", file_result.err);
        exit(1);
    end
    process(file_result.val);
    pass(0);
};
```

### ⚠️ WARNING: Don't Ignore Errors

```aria
// ❌ Bad - silently ignores error
_ = risky_operation();

// ✅ Good - handle or propagate
risky_operation()?;

// ✅ Or explicitly handle
match risky_operation() {
    Ok(_) => {},
    Err(e) => stderr << "Warning: $e",
}
```

### ❌ DON'T: Use unwrap() in Production

```aria
// ❌ Bad - can panic
value: i32 = maybe_value.unwrap();

// ✅ Good - handle None case
value: i32 = maybe_value.unwrap_or(0);

// ✅ Or propagate
value: i32 = maybe_value.ok_or("Value not found")?;
```

---

## Error Recovery Strategies

### Fallback Values

```aria
timeout: i32 = config.timeout.unwrap_or(30);
retries: i32 = config.retries.unwrap_or(3);
```

### Retry with Backoff

```aria
async func:resilient_fetch = Result<Data, string>(string:url) {
    till 3 loop
        int32:attempt = $ + 1;
        Result<Data, string>:result = await fetch(url);
        
        if result.err == NULL then
            pass(result.val);
        end
        
        if attempt < 3 then
            await sleep(1000 * attempt);
        else
            fail(result.err);
        end
    end
};
```

### Circuit Breaker

```aria
struct CircuitBreaker {
    failures: i32,
    threshold: i32,
    open: bool,
}

impl CircuitBreaker {
    func:call = Result<T, string>(func:(Result<T, string>)():f) {
        if self.open then
            fail("Circuit breaker open");
        end
        
        Result<T, string>:result = f();
        if result.err == NULL then
            self.failures = 0;
            pass(result.val);
        else
            self.failures = self.failures + 1;
            if self.failures >= self.threshold then
                self.open = true;
            end
            fail(result.err);
        end
    };
}
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [error_propagation](error_propagation.md) - Error propagation

---

**Remember**: Good error handling makes code **reliable** and **user-friendly**!
