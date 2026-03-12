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
    use_config(raw(result));
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
    User:user = raw(user_result);
    
    Result<Profile, AppError>:profile_result = fetch_profile(user_id);
    if profile_result.err != NULL then
        fail("Failed to fetch profile for user " + string(user_id));
    end
    Profile:profile = raw(profile_result);
    
    Result<Post[], AppError>:posts_result = fetch_posts(user_id);
    if posts_result.err != NULL then
        fail("Failed to fetch posts for user " + string(user_id));
    end
    Post[]:posts = raw(posts_result);
    
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
    if (!primary.is_error) {
        pass(raw(primary));
    }
    
    stdout.write("Primary failed: {}, trying backup...\n", primary.err);
    
    // Try backup source
    Result<Data, string>:backup = fetch_from_backup(url);
    if (!backup.is_error) {
        pass(raw(backup));
    }
    
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
        if (!result.is_error) {
            pass(raw(result));
        }
        
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
    if (file_result.is_error) {
        fail(AppError.IO(file_result.err.to_string()));
    }
    string:content = raw(file_result);
    
    Result<Data, ParseError>:parse_result = parse(content);
    if (parse_result.is_error) {
        fail(AppError.Parse(parse_result.err.to_string()));
    }
    
    pass(raw(parse_result));
};
```

---

## Panic vs Result

```aria
// ✅ Use Result for expected errors
func:read_user_input = Result<int32, string>() {
    Result<string, string>:line_result = stdin.read_line();
    if (line_result.is_error) {
        fail(line_result.err);
    }
    string:input = raw(line_result);
    
    Result<int32, string>:parse_result = input.parse();
    if (parse_result.is_error) {
        fail("Invalid number");
    }
    
    pass(raw(parse_result));
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
    if (!result.is_error) {
        log_info("Operation succeeded: " + raw(result));
        pass(nil);
    } else {
        log_error("Operation failed: " + result.err);
        log_debug("Stack trace: " + result.err.backtrace);
        fail(result.err);
    }
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
    if (result.is_error) {
        fail("Invalid number format");
    }
    pass(raw(result));
};
```

### ✅ DO: Provide Context

```aria
func:load_data = Result<Data, string>(int32:id) {
    Result<Data, string>:result = fetch(id);
    if (result.is_error) {
        fail("Failed to load data for ID " + string(id) + ": " + result.err);
    }
    pass(raw(result));
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
    if (file_result.is_error) {
        stderr.write("Fatal error: {}\n", file_result.err);
        exit(1);
    }
    process(raw(file_result));
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
result<void>:op = risky_operation();
if op.is_error then
    stderr.write("Warning: {}\n", op.err);
end
```

### ❌ DON'T: Skip the Error Check

```aria
// ❌ Bad - bypasses error check entirely
int32:value = raw(maybe_value);

// ✅ Good - use fallback value
int32:value = maybe_value ? 0;

// ✅ Or propagate error to caller
int32:value = maybe_value?;
```

---

## Error Recovery Strategies

### Fallback Values

```aria
int32:timeout = config.timeout ? 30;
int32:retries = config.retries ? 3;
```

### Retry with Backoff

```aria
async func:resilient_fetch = Result<Data, string>(string:url) {
    till 3 loop
        int32:attempt = $ + 1;
        Result<Data, string>:result = await fetch(url);
        
        if (!result.is_error) {
            pass(raw(result));
        }
        
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
        if (!result.is_error) {
            self.failures = 0;
            pass(raw(result));
        } else {
            self.failures = self.failures + 1;
            if (self.failures >= self.threshold) {
                self.open = true;
            }
            fail(result.err);
        }
    };
}
```

---

## Related

- [best_practices](best_practices.md) - Best practices
- [error_propagation](error_propagation.md) - Error propagation

---

## TOS Safety Vocabulary

Aria is safe by default. The keywords and builtins below let you explicitly opt out of that safety.
Using them means **you accept responsibility** for correctness at that call site.

| Keyword / fn | What it does |
|---|---|
| `wild` | Raw heap allocation (untracked) |
| `wildx` | Executable memory allocation (JIT) |
| `unsafe {}` | Block of raw pointer ops / weak memory orderings |
| `ok(v)` | Acknowledge unknown value, strip taint, continue |
| `drop(expr)` | Evaluate expr for side effects, discard Result without checking |
| `raw(result)` | Extract `Result<T>` value without `is_error` check |
| `?` | Unwrap with default value (safe — provides fallback) |
| `??` | Null-coalescing for NIL optionals (safe — provides fallback) |
| `?!` | Emphatic unwrap — triggers `failsafe()` on error |
| `!!!` | Direct `failsafe()` call / panic |

---

## drop() — Discard Result

**Status**: ✅ Implemented (March 2026)

```aria
drop(expr);  // evaluate expr, discard Result — void return
```

`drop()` evaluates an expression fully (including side effects) and silently throws away
the `Result<T>`. No error check is performed.

Use it when you genuinely don't care whether an operation succeeded:

```aria
// Discard Result<nil> from println
drop(println("Starting process..."));

// Fire-and-forget log write
drop(writeFile("audit.log", entry));

// Best-effort notification; failure is acceptable
drop(notifyService(event));
```

### ⚠️ WARNING: You own the consequences

`drop()` is an explicit contract: *I know this can fail and I accept that.*
It bypasses Aria's "no silent failure" guarantee at that exact call site.
The compiler trusts you. If the operation fails and you haven't handled it, that's on you.

### ❌ Don't

```aria
// Don't drop() failures that actually matter:
drop(writeFile("critical_backup.bin", data));  // if this fails, data is lost
```

### ✅ Do

```aria
// Do drop() only when failure is genuinely acceptable:
drop(println("debug: entering loop"));  // cosmetic output
```

---

## raw() — Force Unwrap Result

**Status**: ✅ Implemented (March 2026)

```aria
T:x = raw(result_expr);  // extract value without is_error check
```

`raw()` extracts the value from a `Result<T>` unconditionally.
The `is_error` flag is **not** checked. If the Result is an error, you get back the
ERR sentinel for that type (e.g. `0x80000000` for `tbb32`, undefined for `int32`).

This is the **sharpest tool** in the TOS vocabulary. It maps directly to a single
LLVM `extractvalue` instruction — zero overhead, zero safety net.

Use it only when you can formally prove the Result cannot be an error:

```aria
// You've already checked above — raw() for clean extraction
Result<int32>:r = divide(a, b);
if r.err != NULL then
    fail("division failed");
end
int32:val = raw(r);  // safe: we already verified r is not error

// Performance-critical path where success is guaranteed by design
int32:x = raw(safe_add(10, 32));  // x = 42
```

### ❌ Don't replace `?` with `raw()` out of laziness

```aria
// ❌ BAD - raw() as a lazy alternative to proper handling
int32:x = raw(might_fail());  // if it fails: x = ERR or garbage

// ✅ GOOD - use ? with a sensible default
int32:x = might_fail() ? 0;

// ✅ ALSO GOOD - use ?! when failure is truly unrecoverable
int32:x = must_succeed()?!;
```

### ⚠️ When raw() is actually appropriate

- You previously checked `.err` and want to avoid a second branch
- Integration with code that is statically guaranteed to return success
- Benchmarking / micro-optimization with formal proof of correctness

---

**Remember**: Good error handling makes code **reliable** and **user-friendly**!
