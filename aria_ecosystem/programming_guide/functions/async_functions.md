# Async Functions

**Category**: Functions → Advanced  
**Keywords**: `async`, `await`  
**Philosophy**: Non-blocking I/O without callback hell

---

## What are Async Functions?

**Async functions** allow non-blocking operations - they can pause execution while waiting for I/O (network, disk, timers) without blocking the entire program.

---

## Basic Syntax

```aria
async fn fetch_data(url: string) -> Data {
    response: Response = await http_get(url);
    data: Data = await response.parse_json();
    return data;
}
```

**Key parts**:
- `async fn` - Declares an async function
- `await` - Pauses until operation completes
- Returns same as sync function (runtime handles async details)

---

## How It Works

### Synchronous (Blocking)

```aria
fn download_files(urls: []string) -> []Data {
    results: []Data = [];
    
    for url in urls {
        data: Data = http_get(url);  // Blocks until complete
        results.push(data);
    }
    
    return results;
}
// Total time: sum of all downloads (sequential)
```

### Asynchronous (Non-Blocking)

```aria
async fn download_files(urls: []string) -> []Data {
    results: []Data = [];
    
    for url in urls {
        data: Data = await http_get(url);  // Yields while waiting
        results.push(data);
    }
    
    return results;
}
// Other tasks can run while waiting for HTTP responses
```

---

## Calling Async Functions

### From Async Context (with `await`)

```aria
async fn main() {
    // await pauses until fetch_data completes
    data: Data = await fetch_data("https://api.example.com/data");
    
    stdout << "Received: " << data.size() << " bytes\n";
}
```

### From Sync Context (blocking)

```aria
fn main() {
    // Block until async function completes
    data: Data = run_async(fetch_data("https://api.example.com/data"));
    
    stdout << "Received: " << data.size() << " bytes\n";
}
```

---

## Concurrent Operations

### Sequential (Slow)

```aria
async fn load_user_data(user_id: i32) -> UserData {
    // Each await waits for previous to complete
    profile: Profile = await fetch_profile(user_id);
    posts: []Post = await fetch_posts(user_id);
    friends: []User = await fetch_friends(user_id);
    
    return UserData{profile, posts, friends};
}
// Total time: profile + posts + friends (sequential)
```

### Concurrent (Fast)

```aria
async fn load_user_data(user_id: i32) -> UserData {
    // Start all requests concurrently
    profile_future: Future<Profile> = fetch_profile(user_id);
    posts_future: Future<[]Post> = fetch_posts(user_id);
    friends_future: Future<[]User> = fetch_friends(user_id);
    
    // Wait for all to complete
    profile: Profile = await profile_future;
    posts: []Post = await posts_future;
    friends: []User = await friends_future;
    
    return UserData{profile, posts, friends};
}
// Total time: max(profile, posts, friends) (parallel)
```

---

## Error Handling with Async

### Using TBB Results

```aria
async fn safe_fetch(url: string) -> Data? {
    response: Response? = await http_get(url);
    
    when fail response then
        stderr << "ERROR: Failed to fetch " << url << "\n";
        return ERR;
    end
    
    data: Data = await response.parse();
    return data;
}
```

### With `pass` Keyword

```aria
async fn load_config(path: string) -> Config? {
    file: File = pass await open_file(path);
    content: string = pass await file.read();
    config: Config = pass await parse_json(content);
    
    return config;
}
// Errors propagate with pass, as in sync code
```

---

## Common Patterns

### Timeout

```aria
async fn fetch_with_timeout(url: string, timeout_ms: i32) -> Data? {
    fetch_future: Future<Data> = fetch_data(url);
    timeout_future: Future<()> = sleep(timeout_ms);
    
    // Race: whichever completes first
    match await race(fetch_future, timeout_future) {
        Left(data) => return data,
        Right(()) => {
            stderr << "ERROR: Request timed out\n";
            return ERR;
        }
    }
}
```

### Retry Logic

```aria
async fn fetch_with_retry(url: string, max_attempts: i32) -> Data? {
    for attempt in 1..=max_attempts {
        stddbg << "Attempt " << attempt << "/" << max_attempts;
        
        data: Data? = await fetch_data(url);
        
        when fail data then
            when attempt < max_attempts then
                await sleep(1000);  // Wait 1 second before retry
                continue;
            else
                return ERR;
            end
        end
        
        return data;
    }
    
    return ERR;
}
```

### Background Tasks

```aria
async fn process_in_background(items: []Item) {
    for item in items {
        await process_item(item);
        
        // Yield to other tasks periodically
        when item.id % 100 == 0 then
            await yield();
        end
    }
}

// Spawn as background task
async fn main() {
    spawn(process_in_background(items));
    
    // Main continues while background task runs
    await do_other_work();
}
```

---

## Best Practices

### ✅ DO: Use for I/O Operations

```aria
// Good: Network, file I/O benefit from async
async fn load_from_network(url: string) -> Data {
    return await http_get(url);
}

async fn save_to_disk(file: string, data: Data) {
    await write_file(file, data);
}
```

### ✅ DO: Run Independent Operations Concurrently

```aria
// Good: Concurrent when possible
async fn load_dashboard() -> Dashboard {
    users_future = fetch_users();
    stats_future = fetch_stats();
    alerts_future = fetch_alerts();
    
    users: []User = await users_future;
    stats: Stats = await stats_future;
    alerts: []Alert = await alerts_future;
    
    return Dashboard{users, stats, alerts};
}
```

### ✅ DO: Handle Errors Explicitly

```aria
async fn safe_operation() -> Result? {
    data: Data? = await fetch_data();
    
    when fail data then
        stderr << "ERROR: Fetch failed\n";
        return ERR;
    end
    
    return process(data);
}
```

### ❌ DON'T: Use for CPU-Intensive Work

```aria
// Wrong: Async doesn't help CPU-bound tasks
async fn calculate_primes(n: i32) -> []i32 {
    // This just adds overhead!
    return await compute_primes(n);
}

// Right: Use regular function
fn calculate_primes(n: i32) -> []i32 {
    return compute_primes(n);
}
```

### ❌ DON'T: Await in Loops When You Can Batch

```aria
// Wrong: Sequential awaits in loop
async fn process_all(items: []Item) {
    for item in items {
        await process(item);  // Each waits for previous
    }
}

// Right: Batch process concurrently
async fn process_all(items: []Item) {
    futures: []Future = items.map(|item| process(item));
    await all(futures);  // All run concurrently
}
```

---

## Real-World Examples

### HTTP API Client

```aria
async fn get_user(user_id: i32) -> User? {
    url: string = format("https://api.example.com/users/{}", user_id);
    
    response: Response = pass await http_get(url);
    
    when response.status != 200 then
        stderr << "ERROR: HTTP " << response.status << "\n";
        return ERR;
    end
    
    user: User = pass await response.json();
    return user;
}
```

### Database Query Pool

```aria
async fn query_all_shards(query: string) -> []Result {
    shards: []Shard = get_shards();
    
    // Query all shards concurrently
    futures: []Future<Result> = shards.map(|shard| {
        return shard.query(query);
    });
    
    // Wait for all results
    results: []Result = await all(futures);
    
    return results;
}
```

### WebSocket Server

```aria
async fn handle_client(socket: WebSocket) {
    loop {
        // Wait for message from client
        message: Message? = await socket.receive();
        
        when fail message then
            stddbg << "Client disconnected";
            break;
        end
        
        // Process message
        response: Message = process_message(message);
        
        // Send response
        await socket.send(response);
    }
}

async fn main() {
    server: WebSocketServer = WebSocketServer::new("0.0.0.0:8080");
    
    loop {
        client: WebSocket = await server.accept();
        
        // Handle each client in separate task
        spawn(handle_client(client));
    }
}
```

---

## Async vs Threads

| Aspect | Async | Threads |
|--------|-------|---------|
| **Use Case** | I/O-bound | CPU-bound |
| **Overhead** | Low (cooperative) | High (preemptive) |
| **Scalability** | 1000s of tasks | 10s of threads |
| **Blocking** | Yields on await | Blocks thread |
| **Best For** | Network, file I/O | Parallel computation |

---

## Runtime Model

Aria's async runtime:
- **Single-threaded by default** (event loop)
- **Multi-threaded optional** (thread pool)
- **Cooperative scheduling** (tasks yield with `await`)
- **No callbacks** (async/await syntax)

---

## Related Topics

- [async Keyword](async_keyword.md) - Detailed async syntax
- [Lambda Expressions](lambda.md) - Async lambdas
- [pass/fail Keywords](pass_keyword.md) - Error handling in async
- [Higher-Order Functions](higher_order_functions.md) - Async function composition

---

**Remember**: Async is for **I/O-bound** operations. Use it for network, disk, timers - not for CPU-intensive calculations.
