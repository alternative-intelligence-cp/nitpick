# Context Stack

**Category**: Advanced Features → Syntax  
**Purpose**: Track execution context and error information

---

## Overview

The context stack tracks **function calls**, **error locations**, and **debugging information** at runtime.

---

## Stack Traces

```aria
fn level3() {
    panic("Something went wrong!");
}

fn level2() {
    level3();
}

fn level1() {
    level2();
}

fn main() {
    level1();
}

/*
Output:
panic: Something went wrong!
    at level3 (example.aria:2)
    at level2 (example.aria:6)
    at level1 (example.aria:10)
    at main (example.aria:14)
*/
```

---

## Error Context

```aria
fn process_file(path: string) -> Result<Data> {
    content: string = readFile(path)
        .context("Failed to read file: $path")?;
    
    data: Data = parse(content)
        .context("Failed to parse content from $path")?;
    
    return Ok(data);
}

/*
If parsing fails:
Error: Failed to parse content from config.txt
Caused by: Invalid JSON at line 5
*/
```

---

## Call Stack Information

```aria
fn debug_stack() {
    // Get current stack trace
    stack: StackTrace = StackTrace.capture();
    
    for frame in stack.frames() {
        stdout << "Function: ${frame.function}";
        stdout << "File: ${frame.file}:${frame.line}";
    }
}
```

---

## Unwinding

```aria
fn level3() -> Result<void> {
    return Err("Error at level3");
}

fn level2() -> Result<void> {
    level3()?;  // Unwinds on error
    return Ok();
}

fn level1() -> Result<void> {
    level2()?;  // Unwinds on error
    return Ok();
}

fn main() {
    match level1() {
        Ok(_) => stdout << "Success",
        Err(e) => {
            stderr << "Error: $e";
            // Stack automatically unwound
        }
    }
}
```

---

## Panic Unwinding

```aria
struct Resource {
    name: string,
}

impl Resource {
    fn destroy() {
        stdout << "Cleaning up $self.name";
    }
}

impl Drop for Resource {
    fn drop() {
        self.destroy();
    }
}

fn use_resources() {
    r1: Resource = Resource { name: "A" };
    r2: Resource = Resource { name: "B" };
    r3: Resource = Resource { name: "C" };
    
    panic("Something failed!");
    
    // Stack unwinds, drop() called for r3, r2, r1 in reverse order
}

/*
Output:
Cleaning up C
Cleaning up B
Cleaning up A
panic: Something failed!
*/
```

---

## Common Patterns

### Error Propagation with Context

```aria
fn load_config() -> Result<Config> {
    path: string = get_config_path()
        .context("Failed to determine config path")?;
    
    content: string = readFile(path)
        .context("Failed to read config from $path")?;
    
    config: Config = parse_config(content)
        .context("Failed to parse config")?;
    
    validate_config(config)
        .context("Config validation failed")?;
    
    return Ok(config);
}

/*
Error chain:
Error: Config validation failed
Caused by: Failed to parse config
Caused by: Failed to read config from /etc/app/config.toml
Caused by: Permission denied
*/
```

---

### Backtrace Capture

```aria
struct Error {
    message: string,
    backtrace: StackTrace,
}

impl Error {
    fn new(message: string) -> Error {
        return Error {
            message: message,
            backtrace: StackTrace.capture(),
        };
    }
    
    fn print() {
        stderr << "Error: $self.message\n";
        stderr << "Backtrace:\n";
        for frame in self.backtrace.frames() {
            stderr << "  ${frame.file}:${frame.line} - ${frame.function}\n";
        }
    }
}
```

---

### Scope Guards

```aria
struct Guard {
    name: string,
}

impl Guard {
    fn new(name: string) -> Guard {
        stdout << "Entering $name";
        return Guard { name: name };
    }
}

impl Drop for Guard {
    fn drop() {
        stdout << "Leaving $self.name";
    }
}

fn example() {
    _g1: Guard = Guard.new("outer");
    
    {
        _g2: Guard = Guard.new("inner");
        do_work();
    }  // _g2 dropped
    
    more_work();
}  // _g1 dropped

/*
Output:
Entering outer
Entering inner
Leaving inner
Leaving outer
*/
```

---

### Defer Cleanup

```aria
fn process() -> Result<void> {
    file: File = open("data.txt")?;
    defer file.close();
    
    lock: Mutex = acquire_lock()?;
    defer lock.unlock();
    
    // Work with file and lock
    data: string = file.read_all()?;
    process_data(data)?;
    
    return Ok();
    // Deferred cleanup runs in reverse order:
    // 1. lock.unlock()
    // 2. file.close()
}
```

---

## Debug Information

```aria
fn trace_execution() {
    stdout << "__FUNCTION__: ${__FUNCTION__}";  // Current function
    stdout << "__FILE__: ${__FILE__}";          // Current file
    stdout << "__LINE__: ${__LINE__}";          // Current line
    stdout << "__MODULE__: ${__MODULE__}";      // Current module
}
```

---

## Best Practices

### ✅ DO: Add Context to Errors

```aria
fn load_user(id: i32) -> Result<User> {
    user: User = database.find(id)
        .context("Failed to load user $id")?;
    
    return Ok(user);
}
```

### ✅ DO: Use RAII for Cleanup

```aria
fn safe_resource_use() -> Result<void> {
    resource: Resource = acquire()?;
    defer resource.release();
    
    // Use resource
    resource.do_work()?;
    
    return Ok();
    // resource.release() called automatically
}
```

### ✅ DO: Capture Backtraces for Debugging

```aria
fn create_error(msg: string) -> Error {
    return Error {
        message: msg,
        backtrace: StackTrace.capture(),
        timestamp: now(),
    };
}
```

### ⚠️ WARNING: Stack Overflow

```aria
fn infinite_recursion(n: i32) {
    stdout << n;
    infinite_recursion(n + 1);  // ⚠️ Will overflow stack
}

// ✅ Better - use iteration or tail recursion
fn safe_iteration(max: i32) {
    for i in 0..max {
        stdout << i;
    }
}
```

### ❌ DON'T: Ignore Unwinding

```aria
// ❌ Bad - resource leak on panic
fn bad() {
    resource: *Resource = allocate();
    risky_operation();  // Might panic
    free(resource);     // Never reached if panic
}

// ✅ Good - RAII ensures cleanup
fn good() {
    resource: Resource = Resource.new();
    defer resource.cleanup();
    risky_operation();  // Cleanup happens even on panic
}
```

---

## Stack Depth Control

```aria
// Get current stack depth
fn check_depth() -> i32 {
    stack: StackTrace = StackTrace.capture();
    return stack.depth();
}

fn recursive(n: i32, max_depth: i32) -> Result<void> {
    if check_depth() > max_depth {
        return Err("Stack too deep");
    }
    
    if n > 0 {
        return recursive(n - 1, max_depth);
    }
    
    return Ok();
}
```

---

## Related

- [error_propagation](error_propagation.md) - Error handling
- [pattern_matching](pattern_matching.md) - Match expressions

---

**Remember**: The context stack provides **debugging information** and ensures **proper cleanup** during unwinding!
