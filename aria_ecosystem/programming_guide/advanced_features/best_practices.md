# Best Practices

**Category**: Advanced Features → Best Practices  
**Purpose**: Guidelines for writing idiomatic Aria code

---

## Overview

Follow these best practices to write **clean**, **maintainable**, and **efficient** Aria code.

---

## Code Organization

### File Structure

```aria
// File: user_service.aria

// 1. Imports
import std.io.{File, readFile, writeFile};
import std.collections.HashMap;

// 2. Constants
USER_MAX_AGE: i32 = 150;
DEFAULT_TIMEOUT: i32 = 30;

// 3. Types
struct User {
    id: i32,
    name: string,
    age: i32,
}

// 4. Functions
pub fn create_user(name: string, age: i32) -> Result<User> {
    // Implementation
}

// 5. Main (if applicable)
fn main() {
    // Entry point
}
```

---

### Module Organization

```aria
// Good module structure:
project/
  src/
    main.aria
    lib.aria
    user/
      mod.aria
      service.aria
      repository.aria
    product/
      mod.aria
      catalog.aria
```

---

## Naming Conventions

### Variables and Functions

```aria
// ✅ snake_case for variables and functions
user_count: i32 = 0;
max_retry_attempts: i32 = 3;

fn calculate_total_price(items: []Item) -> f64 {
    // Implementation
}

fn fetch_user_by_id(id: i32) -> Result<User> {
    // Implementation
}
```

---

### Types and Traits

```aria
// ✅ PascalCase for types, structs, enums, traits
struct UserProfile {
    name: string,
    email: string,
}

enum Status {
    Active,
    Inactive,
    Pending,
}

trait Displayable {
    fn display() -> string;
}
```

---

### Constants

```aria
// ✅ SCREAMING_SNAKE_CASE for constants
MAX_CONNECTIONS: i32 = 100;
DEFAULT_TIMEOUT_MS: i32 = 5000;
API_BASE_URL: string = "https://api.example.com";
```

---

## Error Handling

### Use Result for Fallible Operations

```aria
// ✅ Return Result for operations that can fail
fn read_config(path: string) -> Result<Config> {
    content: string = readFile(path)?;
    config: Config = parse_config(content)?;
    return Ok(config);
}

// ❌ Don't panic for expected errors
fn bad_read_config(path: string) -> Config {
    content: string = readFile(path).expect("Failed to read");  // ❌
    // ...
}
```

---

### Provide Context

```aria
// ✅ Add context to errors
fn load_user_data(user_id: i32) -> Result<UserData> {
    user: User = fetch_user(user_id)
        .context("Failed to fetch user $user_id")?;
    
    profile: Profile = fetch_profile(user_id)
        .context("Failed to fetch profile for user $user_id")?;
    
    return Ok(UserData { user, profile });
}
```

---

## Resource Management

### Use RAII

```aria
// ✅ Resources cleaned up automatically
fn process_file(path: string) -> Result<void> {
    file: File = open(path)?;
    defer file.close();  // Automatically called
    
    data: string = file.read_all()?;
    process(data);
    
    return Ok();
}
```

---

### Avoid Manual Memory Management

```aria
// ❌ Manual allocation/deallocation
fn bad_buffer() {
    buffer: *u8 = alloc(1024);
    // Use buffer
    free(buffer);  // Easy to forget!
}

// ✅ Use safe containers
fn good_buffer() {
    buffer: []u8 = [0; 1024];  // Automatically managed
    // Use buffer
}  // Automatically freed
```

---

## Performance

### Prefer Immutability

```aria
// ✅ Immutable by default
fn calculate(data: []i32) -> i32 {
    total: i32 = data.sum();  // data not modified
    return total * 2;
}

// Only use mut when necessary
fn process(data: []mut i32) {
    till(data.len() - 1, 1) {
        data[$] *= 2;  // Mutation needed
    }
}
```

---

### Avoid Unnecessary Allocations

```aria
// ❌ Inefficient - creates temp string each iteration
fn bad_concat(words: []string) -> string {
    Result: string = "";
    till(words.length - 1, 1) {
        result = result ++ words[$];  // Reallocates each time
    }
    return result;
}

// ✅ Efficient - pre-allocate capacity
fn good_concat(words: []string) -> string {
    total_len: i32 = words.map(|w| w.len()).sum();
    Result: StringBuilder = StringBuilder.with_capacity(total_len);
    till(words.length - 1, 1) {
        result.append(words[$]);
    }
    return result.to_string();
}
```

---

## Code Style

### Keep Functions Small

```aria
// ✅ Small, focused functions
fn validate_user(user: User) -> Result<void> {
    validate_name(user.name)?;
    validate_email(user.email)?;
    validate_age(user.age)?;
    return Ok();
}

fn validate_name(name: string) -> Result<void> {
    if name.len() == 0 {
        return Err("Name cannot be empty");
    }
    if name.len() > 100 {
        return Err("Name too long");
    }
    return Ok();
}
```

---

### Use Type Inference

```aria
// ✅ Let compiler infer obvious types
x := 42;
name := "Alice";
items := [1, 2, 3, 4, 5];

// ✅ Be explicit for clarity when needed
timeout: Duration = Duration.seconds(30);
config: Config = load_config()?;
```

---

### Avoid Deep Nesting

```aria
// ❌ Too much nesting
fn bad_nested(user: ?User) -> string {
    if user is Some {
        if user?.age > 18 {
            if user?.name.len() > 0 {
                return user?.name;
            }
        }
    }
    return "Unknown";
}

// ✅ Early returns
fn good_early_return(user: ?User) -> string {
    if user is None {
        return "Unknown";
    }
    
    if user?.age <= 18 {
        return "Minor";
    }
    
    if user?.name.len() == 0 {
        return "Unnamed";
    }
    
    return user?.name;
}
```

---

## Documentation

### Document Public APIs

```aria
/// Fetches user data from the database.
///
/// # Arguments
/// * `user_id` - The unique identifier of the user
///
/// # Returns
/// * `Ok(User)` - The user data if found
/// * `Err(string)` - Error message if user not found or database error
///
/// # Example
/// ```
/// user: User = fetch_user(42)?;
/// stdout << user.name;
/// ```
pub fn fetch_user(user_id: i32) -> Result<User> {
    // Implementation
}
```

---

### Use Meaningful Comments

```aria
// ✅ Explain why, not what
fn calculate_discount(price: f64) -> f64 {
    // Apply 10% discount for bulk orders
    // Based on business rule BR-2025-01
    return price * 0.9;
}

// ❌ Obvious comment
fn add(a: i32, b: i32) -> i32 {
    // Add a and b together  ← Obvious!
    return a + b;
}
```

---

## Testing

### Write Tests for Public APIs

```aria
#[test]
fn test_calculate_total() {
    items: []Item = [
        Item { price: 10.0 },
        Item { price: 20.0 },
    ];
    
    total: f64 = calculate_total(items);
    
    assert(total == 30.0);
}

#[test]
fn test_validate_email_invalid() {
    Result: Result<void> = validate_email("invalid");
    assert(result.is_err());
}
```

---

## Security

### Validate Input

```aria
// ✅ Always validate user input
fn create_user(name: string, age: i32) -> Result<User> {
    if name.len() == 0 {
        return Err("Name cannot be empty");
    }
    
    if age < 0 || age > 150 {
        return Err("Invalid age");
    }
    
    // Create user
}
```

---

### Avoid SQL Injection

```aria
// ❌ SQL injection vulnerability
fn bad_query(username: string) -> Result<User> {
    query: string = "SELECT * FROM users WHERE name = '$username'";
    return db.execute(query);  // ❌ Vulnerable!
}

// ✅ Use parameterized queries
fn good_query(username: string) -> Result<User> {
    query: string = "SELECT * FROM users WHERE name = ?";
    return db.execute(query, username);  // ✅ Safe
}
```

---

## Concurrency

### Prefer Message Passing

```aria
// ✅ Use channels for communication
fn producer_consumer() {
    channel: Channel<i32> = Channel.new();
    
    // Producer thread
    Thread.spawn(|| {
        till(99, 1) {
            channel.send($);
        }
    });
    
    // Consumer thread
    Thread.spawn(|| {
        while value = channel.recv() {
            process(value);
        }
    });
}
```

---

### Use Proper Synchronization

```aria
// ✅ Use Mutex for shared state
shared_counter: Mutex<i32> = Mutex.new(0);

fn increment() {
    lock = shared_counter.lock();
    *lock += 1;
}  // Automatically unlocked
```

---

## Related

- [common_patterns](common_patterns.md) - Common code patterns
- [idioms](idioms.md) - Aria idioms
- [error_handling](error_handling.md) - Error handling patterns

---

**Remember**: These practices help you write **cleaner**, **safer**, and more **maintainable** code!
