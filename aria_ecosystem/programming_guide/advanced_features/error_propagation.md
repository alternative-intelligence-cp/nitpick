# Error Propagation

**Category**: Advanced Features → Patterns  
**Purpose**: Propagate errors through call stack elegantly

---

## Overview

Error propagation handles errors **cleanly** without deeply nested error checking.

---

## The `?` Operator

```aria
fn read_file(path: string) -> Result<string> {
    file: File = open(path)?;  // Returns Err if open fails
    content: string = file.read_all()?;  // Returns Err if read fails
    return Ok(content);
}

// Without ?
fn read_file_verbose(path: string) -> Result<string> {
    match open(path) {
        Ok(file) => {
            match file.read_all() {
                Ok(content) => return Ok(content),
                Err(e) => return Err(e),
            }
        }
        Err(e) => return Err(e),
    }
}
```

---

## Propagating Multiple Errors

```aria
fn process_user_file(user_id: i32) -> Result<Data> {
    user: User = fetch_user(user_id)?;          // Network error?
    file: File = open(user.file_path)?;         // IO error?
    content: string = file.read_all()?;         // Read error?
    data: Data = parse_content(content)?;       // Parse error?
    validated: Data = validate(data)?;          // Validation error?
    return Ok(validated);
}
```

---

## Error Context

```aria
fn load_config(path: string) -> Result<Config> {
    content: string = readFile(path)
        .context("Failed to read config file")?;
    
    config: Config = parse_config(content)
        .context("Failed to parse config")?;
    
    validated: Config = validate_config(config)
        .context("Invalid configuration")?;
    
    return Ok(validated);
}
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
        return Err("Invalid email");
    }
    
    return Ok();
}
```

---

## Combining Results

```aria
fn fetch_all_data() -> Result<AllData> {
    user: User = fetch_user()?;
    posts: []Post = fetch_posts()?;
    comments: []Comment = fetch_comments()?;
    
    return Ok(AllData {
        user: user,
        posts: posts,
        comments: comments,
    });
}
```

---

## Optional to Result

```aria
fn get_user(id: i32) -> Result<User> {
    user: ?User = database.find(id);
    
    // Convert Option to Result
    return user.ok_or("User not found");
}

fn parse_number(s: string) -> Result<i32> {
    num: ?i32 = s.parse();
    return num.ok_or("Invalid number");
}
```

---

## Common Patterns

### File Processing

```aria
fn process_file(input_path: string, output_path: string) -> Result<void> {
    // Read input
    content: string = readFile(input_path)?;
    
    // Process
    processed: string = transform(content)?;
    
    // Write output
    writeFile(output_path, processed)?;
    
    return Ok();
}
```

---

### HTTP Request Chain

```aria
async fn fetch_user_posts(user_id: i32) -> Result<[]Post> {
    // Fetch user
    user_resp: Response = await http.get("/users/$user_id")?;
    user: User = await user_resp.json()?;
    
    // Fetch posts
    posts_resp: Response = await http.get("/posts?user=$user_id")?;
    posts: []Post = await posts_resp.json()?;
    
    return Ok(posts);
}
```

---

### Database Transaction

```aria
fn create_user(name: string, email: string) -> Result<User> {
    tx: Transaction = db.begin()?;
    
    // Insert user
    user_id: i32 = tx.execute(
        "INSERT INTO users (name, email) VALUES (?, ?)",
        name, email
    )?;
    
    // Create profile
    tx.execute(
        "INSERT INTO profiles (user_id) VALUES (?)",
        user_id
    )?;
    
    // Commit transaction
    tx.commit()?;
    
    // Fetch created user
    user: User = get_user(user_id)?;
    return Ok(user);
}
```

---

### Validation Chain

```aria
fn register_user(data: UserData) -> Result<User> {
    // Validate each field
    validate_name(data.name)?;
    validate_email(data.email)?;
    validate_password(data.password)?;
    validate_age(data.age)?;
    
    // Check uniqueness
    check_email_unique(data.email)?;
    
    // Create user
    user: User = create_user(data)?;
    
    return Ok(user);
}
```

---

### Resource Cleanup

```aria
fn copy_file(src: string, dst: string) -> Result<void> {
    src_file: File = open(src)?;
    defer src_file.close();
    
    dst_file: File = create(dst)?;
    defer dst_file.close();
    
    // Copy content
    content: []u8 = src_file.read_all()?;
    dst_file.write_all(content)?;
    
    return Ok();
}
```

---

## Error Transformation

```aria
fn fetch_data() -> Result<Data, CustomError> {
    // Transform error type
    response: Response = http_get("/data")
        .map_err(|e| CustomError.NetworkError(e))?;
    
    data: Data = parse_response(response)
        .map_err(|e| CustomError.ParseError(e))?;
    
    return Ok(data);
}
```

---

## Fallible Iterator

```aria
fn process_files(paths: []string) -> Result<void> {
    for path in paths {
        // Each iteration can fail
        content: string = readFile(path)?;
        processed: string = process(content)?;
        writeFile(path ++ ".processed", processed)?;
    }
    return Ok();
}
```

---

## Collecting Results

```aria
fn fetch_all_users(ids: []i32) -> Result<[]User> {
    users: []User = [];
    
    for id in ids {
        user: User = fetch_user(id)?;  // Fail on first error
        users.push(user);
    }
    
    return Ok(users);
}

// Or collect all results
fn try_fetch_all(ids: []i32) -> []Result<User> {
    results: []Result<User> = [];
    
    for id in ids {
        results.push(fetch_user(id));  // Continue on errors
    }
    
    return results;
}
```

---

## Best Practices

### ✅ DO: Use ? for Clean Error Handling

```aria
fn load_and_process() -> Result<Data> {
    config: Config = load_config()?;
    input: Input = load_input()?;
    data: Data = process(config, input)?;
    return Ok(data);
}
```

### ✅ DO: Provide Context

```aria
fn load_user_data(user_id: i32) -> Result<UserData> {
    user: User = fetch_user(user_id)
        .context("Failed to fetch user $user_id")?;
    
    profile: Profile = fetch_profile(user_id)
        .context("Failed to fetch profile for user $user_id")?;
    
    return Ok(UserData { user, profile });
}
```

### ✅ DO: Use Early Returns

```aria
fn validate(data: Data) -> Result<void> {
    if data.is_empty() {
        return Err("Data cannot be empty");
    }
    
    if !data.is_valid() {
        return Err("Invalid data format");
    }
    
    return Ok();
}
```

### ⚠️ WARNING: Don't Ignore Errors

```aria
// ❌ Bad - silently ignores error
fn bad_process() {
    _ = risky_operation();  // Error ignored!
}

// ✅ Good - handle or propagate
fn good_process() -> Result<void> {
    risky_operation()?;  // Propagate error
    return Ok();
}

// ✅ Or explicitly handle
fn handle_process() {
    match risky_operation() {
        Ok(_) => stdout << "Success",
        Err(e) => stderr << "Error: $e",
    }
}
```

### ❌ DON'T: Use ? in Non-Result Functions

```aria
// ❌ Error - can't use ? here
fn bad() {
    content: string = readFile("file.txt")?;  // Error!
}

// ✅ Good - return Result
fn good() -> Result<string> {
    content: string = readFile("file.txt")?;
    return Ok(content);
}
```

---

## Related

- [pattern_matching](pattern_matching.md) - Pattern matching
- [destructuring](destructuring.md) - Destructuring

---

**Remember**: The `?` operator makes error handling **elegant** while maintaining **safety**!
