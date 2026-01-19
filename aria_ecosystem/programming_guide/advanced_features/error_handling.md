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
fn read_config(path: string) -> Result<Config> {
    content: string = readFile(path)?;
    config: Config = parse_config(content)?;
    return Ok(config);
}

// Caller handles error
match read_config("config.toml") {
    Ok(config) => use_config(config),
    Err(e) => stderr << "Failed to load config: $e",
}
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
    fn message() -> string {
        match self {
            AppError.IO(msg) => return "IO error: $msg",
            AppError.Parse(msg) => return "Parse error: $msg",
            AppError.Network(msg) => return "Network error: $msg",
            AppError.NotFound => return "Resource not found",
            AppError.PermissionDenied => return "Permission denied",
        }
    }
}

fn fetch_user(id: i32) -> Result<User, AppError> {
    user: ?User = database.find(id);
    
    match user {
        Some(u) => return Ok(u),
        None => return Err(AppError.NotFound),
    }
}
```

---

## Error Context

```aria
fn load_user_data(user_id: i32) -> Result<UserData> {
    user: User = fetch_user(user_id)
        .context("Failed to fetch user $user_id")?;
    
    profile: Profile = fetch_profile(user_id)
        .context("Failed to fetch profile for user $user_id")?;
    
    posts: []Post = fetch_posts(user_id)
        .context("Failed to fetch posts for user $user_id")?;
    
    return Ok(UserData { user, profile, posts });
}

// Error output:
// Failed to fetch posts for user 42
// Caused by: Network error: Connection timeout
```

---

## Early Return Pattern

```aria
fn validate_user(user: User) -> Result<void> {
    if user.name == "" {
        return Err("Name cannot be empty");
    }
    
    if user.age < 0 {
        return Err("Age cannot be negative");
    }
    
    if user.age > 150 {
        return Err("Age too high");
    }
    
    if !user.email.contains("@") {
        return Err("Invalid email format");
    }
    
    return Ok();
}
```

---

## Recoverable Errors

```aria
fn fetch_with_fallback(url: string) -> Result<Data> {
    // Try primary source
    match fetch_from_primary(url) {
        Ok(data) => return Ok(data),
        Err(e) => {
            stdout << "Primary failed: $e, trying backup...";
            
            // Try backup source
            match fetch_from_backup(url) {
                Ok(data) => return Ok(data),
                Err(e) => {
                    stderr << "Backup also failed: $e";
                    
                    // Return cached data if available
                    match get_from_cache(url) {
                        Some(data) => return Ok(data),
                        None => return Err("All sources failed"),
                    }
                }
            }
        }
    }
}
```

---

## Retry Logic

```aria
fn fetch_with_retry(url: string, max_retries: i32) -> Result<Data> {
    retries: i32 = 0;
    
    loop {
        match fetch(url) {
            Ok(data) => return Ok(data),
            Err(e) => {
                retries += 1;
                
                if retries >= max_retries {
                    return Err("Max retries exceeded: $e");
                }
                
                // Exponential backoff
                delay: i32 = 1000 * (2 ** (retries - 1));
                stdout << "Retry $retries/$max_retries after ${delay}ms";
                sleep(delay);
            }
        }
    }
}
```

---

## Error Conversion

```aria
impl From<IoError> for AppError {
    fn from(e: IoError) -> AppError {
        return AppError.IO(e.to_string());
    }
}

impl From<ParseError> for AppError {
    fn from(e: ParseError) -> AppError {
        return AppError.Parse(e.to_string());
    }
}

fn process() -> Result<Data, AppError> {
    // Automatic conversion
    content: string = readFile("data.txt")?;  // IoError → AppError
    data: Data = parse(content)?;             // ParseError → AppError
    return Ok(data);
}
```

---

## Panic vs Result

```aria
// ✅ Use Result for expected errors
fn read_user_input() -> Result<i32> {
    input: string = stdin.read_line()?;
    number: i32 = input.parse()
        .map_err(|_| "Invalid number")?;
    return Ok(number);
}

// ✅ Use panic for programming errors
fn calculate(divisor: i32) -> f64 {
    if divisor == 0 {
        panic("Division by zero - this should never happen!");
    }
    return 100.0 / divisor;
}

// ✅ Use assertions in debug builds
fn process_array(arr: []i32, index: i32) {
    debug_assert(index < arr.len(), "Index out of bounds");
    // Process arr[index]
}
```

---

## Error Logging

```aria
fn process_with_logging() -> Result<void> {
    match risky_operation() {
        Ok(result) => {
            log_info("Operation succeeded: $result");
            return Ok();
        }
        Err(e) => {
            log_error("Operation failed: $e");
            log_debug("Stack trace: ${e.backtrace}");
            return Err(e);
        }
    }
}
```

---

## Validation Chain

```aria
fn register_user(data: UserData) -> Result<User> {
    validate_name(data.name)?;
    validate_email(data.email)?;
    validate_password(data.password)?;
    validate_age(data.age)?;
    
    check_email_unique(data.email)?;
    check_username_available(data.username)?;
    
    user: User = create_user(data)?;
    send_confirmation_email(user.email)?;
    
    return Ok(user);
}

fn validate_email(email: string) -> Result<void> {
    if !email.contains("@") {
        return Err("Invalid email format");
    }
    
    if email.len() < 3 {
        return Err("Email too short");
    }
    
    return Ok();
}
```

---

## Best Practices

### ✅ DO: Use Result for Fallible Operations

```aria
fn parse_number(s: string) -> Result<i32> {
    match s.parse() {
        Ok(n) => return Ok(n),
        Err(_) => return Err("Invalid number format"),
    }
}
```

### ✅ DO: Provide Context

```aria
fn load_data(id: i32) -> Result<Data> {
    data: Data = fetch(id)
        .context("Failed to load data for ID $id")?;
    return Ok(data);
}
```

### ✅ DO: Handle Errors at Appropriate Level

```aria
// Low-level: propagate
fn read_file(path: string) -> Result<string> {
    return readFile(path);  // Propagate error
}

// High-level: handle
fn main() {
    match read_file("config.toml") {
        Ok(content) => process(content),
        Err(e) => {
            stderr << "Fatal error: $e";
            exit(1);
        }
    }
}
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
async fn resilient_fetch(url: string) -> Result<Data> {
    for attempt in 1..=3 {
        match await fetch(url) {
            Ok(data) => return Ok(data),
            Err(e) if attempt < 3 => {
                await sleep(1000 * attempt);
                continue;
            }
            Err(e) => return Err(e),
        }
    }
}
```

### Circuit Breaker

```aria
struct CircuitBreaker {
    failures: i32,
    threshold: i32,
    open: bool,
}

impl CircuitBreaker {
    fn call<T>(f: fn() -> Result<T>) -> Result<T> {
        if self.open {
            return Err("Circuit breaker open");
        }
        
        match f() {
            Ok(result) => {
                self.failures = 0;
                return Ok(result);
            }
            Err(e) => {
                self.failures += 1;
                if self.failures >= self.threshold {
                    self.open = true;
                }
                return Err(e);
            }
        }
    }
}
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [error_propagation](error_propagation.md) - Error propagation

---

**Remember**: Good error handling makes code **reliable** and **user-friendly**!
