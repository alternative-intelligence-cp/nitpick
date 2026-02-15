# `await` Keyword

**Category**: Advanced Features → Concurrency  
**Syntax**: `await <async_expression>`  
**Purpose**: Wait for async operation to complete

---

## Overview

`await` **suspends** execution until an async operation completes.

---

## Basic Await

```aria
async fn fetch() -> Result<Data> {
    response: Response = await http.get("https://api.example.com")?;
    data: Data = await response.json()?;
    return Ok(data);
}
```

---

## What Await Does

```aria
// Without await - returns Future
future: Future<Data> = async_function();

// With await - returns Data
data: Data = await async_function();
```

---

## Sequential Awaits

```aria
async fn sequential() -> Result<void> {
    user: User = await fetch_user()?;       // Wait for user
    posts: []Post = await fetch_posts()?;   // Then wait for posts
    comments: []Comment = await fetch_comments()?;  // Then comments
    return Ok();
}
```

Total time: time(user) + time(posts) + time(comments)

---

## Concurrent Awaits

```aria
async fn concurrent() -> Result<void> {
    // Start all tasks
    user_task = fetch_user();
    posts_task = fetch_posts();
    comments_task = fetch_comments();
    
    // Wait for all (runs concurrently)
    user: User = await user_task?;
    posts: []Post = await posts_task?;
    comments: []Comment = await comments_task?;
    
    return Ok();
}
```

Total time: max(time(user), time(posts), time(comments))

---

## Error Handling

```aria
async fn with_error_handling() -> Result<Data> {
    match await fetch_data() {
        Ok(data) => return Ok(data),
        Err(e) => {
            stdout << "Error: $e";
            return Err(e);
        }
    }
}
```

---

## Await with Try Operator

```aria
async fn fetch_and_process() -> Result<Processed> {
    data: Data = await fetch_data()?;     // Propagate error
    validated: Valid = await validate(data)?;
    processed: Processed = await process(validated)?;
    return Ok(processed);
}
```

---

## Timeout

```aria
import std.time.timeout;

async fn with_timeout() -> Result<Data> {
    result = timeout(5000, async {  // 5 second timeout
        return await slow_operation();
    });
    
    match await result {
        Ok(data) => return Ok(data),
        Err(Timeout) => return Err("Operation timed out"),
    }
}
```

---

## Common Patterns

### Retry Logic

```aria
async fn fetch_with_retry(url: string, retries: i32) -> Result<Data> {
    till(retries - 1, 1) {
        match await fetch(url) {
            Ok(data) => return Ok(data),
            Err(e) => {
                if $ == retries - 1 {
                    return Err(e);
                }
                await sleep(1000 * ($ + 1));  // Backoff
            }
        }
    }
}
```

---

### Race Condition

```aria
import std.async.race;

async fn fetch_fastest() -> Result<Data> {
    task1 = fetch_from_server1();
    task2 = fetch_from_server2();
    task3 = fetch_from_server3();
    
    // Return whichever completes first
    Result: Data = await race([task1, task2, task3])?;
    return Ok(result);
}
```

---

### Join All

```aria
import std.async.join_all;

async fn fetch_multiple(urls: []string) -> Result<[]Data> {
    tasks: []Future<Data> = [];
    till(urls.length - 1, 1) {
        tasks.push(fetch(urls[$]));
    }
    
    results: []Data = await join_all(tasks)?;
    return Ok(results);
}
```

---

## Best Practices

### ✅ DO: Await in Async Functions

```aria
async fn correct() -> Result<Data> {
    data: Data = await fetch()?;  // ✅ In async function
    return Ok(data);
}
```

### ✅ DO: Handle Errors

```aria
async fn with_error_check() -> Result<Data> {
    match await fetch() {
        Ok(data) => process(data),
        Err(e) => {
            log_error(e);
            return Err(e);
        }
    }
}
```

### ❌ DON'T: Await in Sync Functions

```aria
fn sync_function() -> Data {
    data: Data = await fetch();  // ❌ Error: can't await in sync
}
```

### ❌ DON'T: Forget to Await

```aria
async fn wrong() {
    fetch();  // ❌ Returns Future, doesn't execute!
    
    await fetch();  // ✅ Actually executes
}
```

---

## Async Iteration

```aria
async fn process_stream() {
    stream: AsyncStream<Data> = get_stream();
    
    items = await stream.collect();
    till(items.length - 1, 1) {
        await process(items[$]);
    }
}
```

---

## Related

- [async](async.md) - Async keyword
- [async_await](async_await.md) - Async/await pattern
- [concurrency](concurrency.md) - Concurrency overview

---

**Remember**: `await` **suspends** - other tasks can run while waiting!
