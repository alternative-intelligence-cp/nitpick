# `async` Keyword

**Category**: Advanced Features → Concurrency  
**Syntax**: `async fn <name>() -> <type>`  
**Purpose**: Define asynchronous functions

---

## Overview

`async` marks functions that can **suspend** and **resume** execution, enabling non-blocking I/O.

---

## Basic Async Function

```aria
async fn fetch_data(url: string) -> Result<string> {
    response: Response = await http.get(url)?;
    text: string = await response.text()?;
    return Ok(text);
}
```

---

## Calling Async Functions

```aria
// Must use await
async fn main() {
    data: string = await fetch_data("https://example.com")?;
    stdout << data;
}
```

---

## Async vs Sync

```aria
// Synchronous - blocks thread
fn sync_fetch(url: string) -> Result<string> {
    // Blocks until complete
    return http_blocking.get(url)?;
}

// Asynchronous - doesn't block
async fn async_fetch(url: string) -> Result<string> {
    // Suspends, other tasks can run
    return await http.get(url)?;
}
```

---

## Return Types

```aria
async fn get_number() -> i32 {
    return 42;
}

async fn may_fail() -> Result<Data> {
    data: Data = await fetch()?;
    return Ok(data);
}

async fn optional_data() -> ?Data {
    if available {
        return Some(await fetch());
    }
    return None;
}
```

---

## Async Blocks

```aria
fn main() {
    future = async {
        data: Data = await fetch_data();
        await process(data);
    };
    
    runtime.block_on(future);
}
```

---

## Concurrent Execution

```aria
async fn fetch_all() -> Result<void> {
    // Start both requests concurrently
    task1 = fetch_data("url1");
    task2 = fetch_data("url2");
    
    // Wait for both to complete
    data1: Data = await task1?;
    data2: Data = await task2?;
    
    stdout << "Got $data1 and $data2";
    return Ok();
}
```

---

## Common Patterns

### Sequential Async

```aria
async fn sequential() -> Result<void> {
    user: User = await fetch_user()?;
    posts: []Post = await fetch_posts(user.id)?;
    comments: []Comment = await fetch_comments(posts[0].id)?;
    return Ok();
}
```

---

### Concurrent Async

```aria
async fn concurrent() -> Result<void> {
    // Start all at once
    user_task = fetch_user();
    posts_task = fetch_posts();
    stats_task = fetch_stats();
    
    // Wait for all
    user: User = await user_task?;
    posts: []Post = await posts_task?;
    stats: Stats = await stats_task?;
    
    return Ok();
}
```

---

### Async Iterator

```aria
async fn* async_generator() -> i32 {
    for i in 0..10 {
        await async_sleep(100);
        yield i;
    }
}

async fn use_generator() {
    async for value in async_generator() {
        stdout << value;
    }
}
```

---

## Best Practices

### ✅ DO: Use for I/O Operations

```aria
async fn read_files() -> Result<void> {
    // Good - async I/O
    content1: string = await readFile("file1.txt")?;
    content2: string = await readFile("file2.txt")?;
    return Ok();
}
```

### ✅ DO: Run Independent Tasks Concurrently

```aria
async fn fetch_dashboard() -> Dashboard {
    user = fetch_user();
    posts = fetch_posts();
    notifications = fetch_notifications();
    
    return Dashboard {
        user: await user,
        posts: await posts,
        notifications: await notifications,
    };
}
```

### ❌ DON'T: Use for CPU-Bound Work

```aria
async fn compute() -> i32 {
    // ❌ Bad - blocks thread with CPU work
    sum: i32 = 0;
    for i in 0..1000000 {
        sum += i;
    }
    return sum;
}

// Better - use regular function or thread
fn compute() -> i32 {
    // CPU work in sync function
}
```

---

## Related

- [await](await.md) - Await keyword
- [async_await](async_await.md) - Async/await pattern
- [concurrency](concurrency.md) - Concurrency overview

---

**Remember**: `async` enables **non-blocking** I/O - use for network, disk, and user input!
