# Return Statement

**Category**: Control Flow → Function Control  
**Keyword**: `return`  
**Philosophy**: Exit function with value

---

## What is Return?

**Return** exits a function and optionally provides a value back to the caller.

---

## Basic Syntax

```aria
fn function_name() -> ReturnType {
    return value;
}
```

---

## Return a Value

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

Result: i32 = add(5, 3);  // 8
```

---

## Return Without Value

For functions returning `()` (unit/void):

```aria
fn print_message(msg: string) {
    stdout << msg << "\n";
    return;  // Optional - exits function
}
```

---

## Early Return

```aria
fn divide(a: i32, b: i32) -> i32? {
    if b == 0 {
        return nil;  // Early exit
    }
    
    return a / b;
}
```

---

## Multiple Return Points

```aria
fn classify_number(n: i32) -> string {
    if n < 0 {
        return "negative";
    }
    
    if n == 0 {
        return "zero";
    }
    
    return "positive";
}
```

---

## Guard Clauses

```aria
fn process_user(user: User?) -> bool {
    // Guard: Check nil
    if user == nil {
        return false;
    }
    
    // Guard: Check valid
    if !user.is_valid() {
        return false;
    }
    
    // Main logic
    user.update();
    return true;
}
```

---

## Return from Loops

```aria
fn find_item(items: []Item, id: i32) -> Item? {
    for item in items {
        if item.id == id {
            return item;  // Exit function immediately
        }
    }
    
    return nil;  // Not found
}
```

---

## Return Complex Types

```aria
fn create_user(name: string) -> User {
    user: User = User{
        name: name,
        id: generate_id(),
        created_at: now()
    };
    
    return user;
}
```

---

## Return Tuples

```aria
fn min_max(numbers: []i32) -> (i32, i32) {
    min: i32 = numbers[0];
    max: i32 = numbers[0];
    
    for num in numbers {
        if num < min { min = num; }
        if num > max { max = num; }
    }
    
    return (min, max);
}

// Usage
min_val, max_val: (i32, i32) = min_max([3, 7, 1, 9, 2]);
```

---

## Return Optional

```aria
fn find_user(id: i32) -> User? {
    user: User? = database.get(id);
    
    if user == nil {
        return nil;
    }
    
    return user;
}
```

---

## Return with TBB

```aria
fn load_config() -> Config {
    file: File = pass open("config.toml");
    content: string = pass file.read();
    config: Config = pass parse(content);
    
    return config;
}

// Errors propagate automatically via pass
```

---

## Implicit Return

Aria requires **explicit** return statements:

```aria
// ❌ Wrong: No implicit return
fn add(a: i32, b: i32) -> i32 {
    a + b  // Error!
}

// ✅ Correct: Explicit return
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Best Practices

### ✅ DO: Use Guard Clauses

```aria
// Good: Handle errors first
fn process(data: Data?) -> bool {
    if data == nil {
        return false;
    }
    
    if !data.is_valid() {
        return false;
    }
    
    // Main logic here
    data.process();
    return true;
}
```

### ✅ DO: Return Early on Error

```aria
// Good: Fail fast
fn validate(user: User) -> bool {
    if user.name.length() == 0 {
        return false;
    }
    
    if user.age < 0 {
        return false;
    }
    
    return true;
}
```

### ✅ DO: Single Return for Complex Logic

```aria
// Good: Clear flow
fn calculate_score(user: User) -> i32 {
    score: i32 = 0;
    
    score = score + user.points;
    score = score + user.bonus;
    
    if user.is_premium {
        score = score * 2;
    }
    
    return score;
}
```

### ❌ DON'T: Return in Finally/Defer

```aria
// ❌ Wrong: Don't return from defer
fn example() -> i32 {
    defer {
        return 42;  // Error!
    }
    
    return 0;
}
```

### ❌ DON'T: Have Unreachable Code

```aria
// Wrong: Code after return never runs
fn example() -> i32 {
    return 42;
    stdout << "Never prints\n";  // Unreachable
}
```

---

## Return vs Pass vs Fail

### Return - Normal Exit

```aria
fn get_value() -> i32 {
    return 42;  // Normal return
}
```

### Pass - Error Propagation

```aria
fn load_data() -> Data {
    file: File = pass open("data.txt");  // Propagate error
    return pass parse(file);
}
```

### Fail - Explicit Error

```aria
fn validate(x: i32) -> i32 {
    if x < 0 {
        fail "Negative value";  // Trigger error
    }
    
    return x;
}
```

---

## Real-World Examples

### Validation Function

```aria
fn validate_email(email: string) -> bool {
    if email.length() == 0 {
        return false;
    }
    
    if !email.contains("@") {
        return false;
    }
    
    if !email.contains(".") {
        return false;
    }
    
    return true;
}
```

### Search Function

```aria
fn binary_search(arr: []i32, target: i32) -> i32? {
    left: i32 = 0;
    right: i32 = arr.length() - 1;
    
    while left <= right {
        mid: i32 = (left + right) / 2;
        
        if arr[mid] == target {
            return mid;  // Found!
        }
        
        if arr[mid] < target {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return nil;  // Not found
}
```

### Factory Function

```aria
fn create_connection(config: Config) -> Connection {
    conn: Connection = Connection::new();
    
    if config.use_ssl {
        conn.enable_ssl();
    }
    
    if config.timeout > 0 {
        conn.set_timeout(config.timeout);
    }
    
    return conn;
}
```

---

## Related Topics

- [Function Declaration](../functions/function_declaration.md) - Declaring functions
- [Pass Keyword](../functions/pass_keyword.md) - Error propagation
- [Fail Keyword](../functions/fail_keyword.md) - Error triggering
- [Break](break.md) - Exit loops
- [Continue](continue.md) - Skip iterations

---

**Remember**: Return **exits function** with optional value - use guard clauses for clarity, return early on errors!
