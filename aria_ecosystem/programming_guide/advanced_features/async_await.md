# Async/Await Pattern

**Category**: Advanced Features → Concurrency  
**Purpose**: Write asynchronous code that looks synchronous

---

## Overview

Async/await provides **non-blocking concurrency** with **readable, sequential-looking code**.

---

## Basic Pattern

```aria
async fn fetch_user(id: i32) -> Result<User> {
    response: Response = await http.get("/users/$id")?;
    user: User = await response.json()?;
    return Ok(user);
}
```

---

## The Problem Async/Await Solves

### Without Async/Await (Callbacks)

```aria
fn fetch_user_callback(id: i32, callback: fn(Result<User>)) {
    http.get("/users/$id", fn(response) {
        response.json(fn(user) {
            callback(Ok(user));
        });
    });
}
// Callback hell!
```

---

### With Async/Await

```aria
async fn fetch_user(id: i32) -> Result<User> {
    response: Response = await http.get("/users/$id")?;
    user: User = await response.json()?;
    return Ok(user);
}
// Clean and readable!
```

---

## Sequential Operations

```aria
async fn process_user(id: i32) -> Result<void> {
    // Execute one after another
    user: User = await fetch_user(id)?;
    posts: []Post = await fetch_posts(user.id)?;
    comments: []Comment = await fetch_comments(posts[0].id)?;
    
    await save_to_cache(user, posts, comments)?;
    return Ok();
}
```

---

## Concurrent Operations

```aria
async fn fetch_dashboard(user_id: i32) -> Result<Dashboard> {
    // Start all requests concurrently
    user_future = fetch_user(user_id);
    posts_future = fetch_posts(user_id);
    notifications_future = fetch_notifications(user_id);
    
    // Wait for all to complete
    user: User = await user_future?;
    posts: []Post = await posts_future?;
    notifications: []Notification = await notifications_future?;
    
    return Ok(Dashboard {
        user: user,
        posts: posts,
        notifications: notifications,
    });
}
```

---

## Error Handling

```aria
async fn fetch_with_fallback(id: i32) -> User {
    match await fetch_user(id) {
        Ok(user) => return user,
        Err(e) => {
            stdout << "Primary fetch failed: $e";
            
            match await fetch_user_from_cache(id) {
                Ok(cached) => return cached,
                Err(_) => return User.default(),
            }
        }
    }
}
```

---

## Common Patterns

### HTTP Request

```aria
async fn api_request(endpoint: string) -> Result<Data> {
    response: Response = await http.get(endpoint)?;
    
    if !response.ok() {
        return Err("HTTP $response.status");
    }
    
    data: Data = await response.json()?;
    return Ok(data);
}
```

---

### Database Query

```aria
async fn fetch_user_with_posts(id: i32) -> Result<UserWithPosts> {
    user: User = await db.query("SELECT * FROM users WHERE id = ?", id)?;
    posts: []Post = await db.query("SELECT * FROM posts WHERE user_id = ?", id)?;
    
    return Ok(UserWithPosts {
        user: user,
        posts: posts,
    });
}
```

---

### File I/O

```aria
async fn process_file(path: string) -> Result<void> {
    content: string = await readFile(path)?;
    processed: string = await process_content(content)?;
    await writeFile(path ++ ".processed", processed)?;
    return Ok();
}
```

---

### WebSocket

```aria
async fn handle_websocket(socket: WebSocket) {
    messages = await socket.collect();
    till(messages.length - 1, 1) {
        match messages[$] {
            Text(text) => {
                response: string = await process_message(text)?;
                await socket.send(response)?;
            }
            Close => break,
        }
    }
}
```

---

## Joining Multiple Tasks

```aria
import std.async.join_all;

async fn fetch_all_users(ids: []i32) -> Result<[]User> {
    tasks: []Future<User> = [];
    
    till(ids.length - 1, 1) {
        tasks.push(fetch_user(ids[$]));
    }
    
    users: []User = await join_all(tasks)?;
    return Ok(users);
}
```

---

## Racing Tasks

```aria
import std.async.race;

async fn fetch_fastest() -> Result<Data> {
    primary = fetch_from_primary();
    backup = fetch_from_backup();
    
    // Return whichever completes first
    Result: Data = await race([primary, backup])?;
    return Ok(result);
}
```

---

## Timeout Pattern

```aria
import std.async.timeout;

async fn fetch_with_timeout(url: string) -> Result<Data> {
    result = timeout(5000, fetch(url));  // 5 second timeout
    
    match await result {
        Ok(data) => return Ok(data),
        Err(Timeout) => return Err("Request timed out"),
    }
}
```

---

## Retry Pattern

```aria
async fn fetch_with_retry(url: string, max_retries: i32) -> Result<Data> {
    retries: i32 = 0;
    
    loop {
        match await fetch(url) {
            Ok(data) => return Ok(data),
            Err(e) => {
                retries += 1;
                if retries >= max_retries {
                    return Err("Max retries exceeded");
                }
                
                await sleep(1000 * retries);  // Exponential backoff
            }
        }
    }
}
```

---

## Best Practices

### ✅ DO: Use Async for I/O

```aria
async fn io_operations() -> Result<void> {
    // Network I/O
    data: Data = await fetch_from_api()?;
    
    // File I/O
    content: string = await readFile("config.txt")?;
    
    // Database I/O
    users: []User = await db.query("SELECT * FROM users")?;
    
    return Ok();
}
```

### ✅ DO: Run Independent Tasks Concurrently

```aria
async fn concurrent_tasks() -> Result<void> {
    // Start both
    task1 = expensive_operation1();
    task2 = expensive_operation2();
    
    // Wait for both
    result1 = await task1?;
    result2 = await task2?;
    
    return Ok();
}
```

### ✅ DO: Handle Errors Gracefully

```aria
async fn robust_fetch() -> Result<Data> {
    match await fetch_data() {
        Ok(data) => return Ok(data),
        Err(e) => {
            log_error(e);
            return await fetch_from_cache();
        }
    }
}
```

### ❌ DON'T: Block the Executor

```aria
async fn blocking() {
    // ❌ Bad - blocks all async tasks
    till(999999, 1) {
        compute($);
    }
    
    // ✅ Good - yield periodically
    till(999999, 1) {
        if $ % 1000 == 0 {
            await yield_now();
        }
        compute($);
    }
}
```

---

## Related

- [async](async.md) - Async keyword
- [await](await.md) - Await keyword
- [concurrency](concurrency.md) - Concurrency overview

---

**Remember**: Async/await makes **asynchronous code look synchronous** while remaining **non-blocking**!
