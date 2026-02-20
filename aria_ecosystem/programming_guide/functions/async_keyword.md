# The `async` Keyword

**Category**: Functions → Async  
**Syntax**: `async fn name() -> T`  
**Purpose**: Declare a function that can perform non-blocking operations

---

## What is `async`?

The **`async`** keyword marks a function as **asynchronous** - it can use `await` to perform non-blocking operations without blocking the thread.

---

## Basic Syntax

```aria
async fn fetch_user(id: i32) -> User? {
    response: Response = await http_get("/api/user/" + format("{}", id));
    user: User = pass response.deserialize();
    return user;
}
```

---

## Key Differences from Regular Functions

### Regular Function (Synchronous)

```aria
// Blocks the thread until complete
fn fetch_user_sync(id: i32) -> User? {
    response: Response = http_get_blocking("/api/user/" + format("{}", id));
    return response.deserialize();
}

// Calling thread is blocked
user: User = fetch_user_sync(123);  // Waits here
```

### Async Function

```aria
// Returns immediately, execution continues
async fn fetch_user(id: i32) -> User? {
    response: Response = await http_get("/api/user/" + format("{}", id));
    return response.deserialize();
}

// Must await the result
user: User = await fetch_user(123);  // Thread can do other work
```

---

## Using `async` Functions

### Must Use `await`

```aria
async fn get_data() -> string {
    return "data";
}

// ❌ Wrong: Returns Future, not string
// value: string = get_data();

// ✅ Right: Use await
value: string = await get_data();
```

### Can Only `await` in `async` Context

```aria
// ❌ Wrong: Can't await in regular function
fn process() {
    data: string = await get_data();  // Error!
}

// ✅ Right: Function must be async
async fn process() {
    data: string = await get_data();  // OK
}
```

---

## Async Main Function

```aria
// Programs can have async main
async fn main() {
    user: User = await fetch_user(123);
    stdout << "User: " << user.name << "\n";
}
```

---

## Return Types

### Explicit Return Type

```aria
async fn get_number() -> i32 {
    return 42;
}

// Returns: Future<i32>
// Await gets: i32
value: i32 = await get_number();
```

### Optional Returns

```aria
async fn might_fail() -> Data? {
    response: Response = pass await http_get("/api/data");
    return response.data();
}

// Can return ERR
data: Data? = await might_fail();
```

---

## Async with Generics

```aria
async fn fetch<T>(url: string) -> T? where T: Deserialize {
    response: Response = pass await http_get(url);
    data: T = pass response.deserialize();
    return data;
}

// Usage
user: User? = await fetch::<User>("/api/user");
product: Product? = await fetch::<Product>("/api/product");
```

---

## Async Methods

```aria
struct ApiClient {
    base_url: string
}

impl ApiClient {
    async fn get(endpoint: string) -> Response? {
        url: string = self.base_url + endpoint;
        return await http_get(url);
    }
    
    async fn post(endpoint: string, data: string) -> Response? {
        url: string = self.base_url + endpoint;
        return await http_post(url, data);
    }
}

// Usage
client: ApiClient = ApiClient{base_url: "https://api.example.com"};
response: Response = await client.get("/users");
```

---

## Concurrent Execution

### Sequential (One After Another)

```aria
async fn fetch_all_sequential() {
    user: User = await fetch_user(1);      // Wait
    product: Product = await fetch_product(1);  // Then wait
    order: Order = await fetch_order(1);    // Then wait
}
```

### Concurrent (All at Once)

```aria
async fn fetch_all_concurrent() {
    // Start all requests immediately
    user_future: Future<User> = fetch_user(1);
    product_future: Future<Product> = fetch_product(1);
    order_future: Future<Order> = fetch_order(1);
    
    // Wait for all to complete
    user: User = await user_future;
    product: Product = await product_future;
    order: Order = await order_future;
}
```

---

## Async Closures

```aria
// Async lambda
callback: async fn() -> i32 = async || {
    data: i32 = await fetch_number();
    return data * 2;
};

// Call it
Result: i32 = await callback();
```

---

## When to Use `async`

### ✅ Use for I/O-Bound Operations

```aria
// Good: Network calls
async fn fetch_data() -> Data {
    return await http_get("/api/data");
}

// Good: File operations
async fn read_file(path: string) -> string {
    return await fs::read_to_string(path);
}

// Good: Database queries
async fn get_user(id: i32) -> User {
    return await db.query("SELECT * FROM users WHERE id = ?", id);
}
```

### ❌ Don't Use for CPU-Bound Operations

```aria
// Wrong: CPU-bound work doesn't benefit from async
async fn calculate_fibonacci(n: i32) -> i32 {
    when n <= 1 then return n; end
    return calculate_fibonacci(n - 1) + calculate_fibonacci(n - 2);
}

// Right: Just use regular function
fn calculate_fibonacci(n: i32) -> i32 {
    when n <= 1 then return n; end
    return calculate_fibonacci(n - 1) + calculate_fibonacci(n - 2);
}
```

---

## Async vs Threads

| Aspect | `async`/`await` | Threads |
|--------|-----------------|---------|
| **Use case** | I/O-bound | CPU-bound |
| **Overhead** | Very low | Higher |
| **Scalability** | 1000s of tasks | 10s-100s threads |
| **Complexity** | Simpler | More complex |
| **Blocking** | Never blocks | Can block |

---

## Best Practices

### ✅ DO: Use for I/O Operations

```aria
// Good: Async for network I/O
async fn api_call() -> Result {
    return await http_get("/api/data");
}
```

### ✅ DO: Propagate Async Upward

```aria
// Good: Async functions call other async functions
async fn process_user(id: i32) {
    user: User = await fetch_user(id);
    orders: []Order = await fetch_orders(user.id);
    process_orders(orders);
}
```

### ✅ DO: Handle Errors with `pass`/`fail`

```aria
async fn safe_fetch() -> Data? {
    response: Response = pass await http_get("/api/data");
    data: Data = pass response.deserialize();
    return data;
}
```

### ❌ DON'T: Mix Blocking and Async

```aria
// Wrong: Blocking call in async function
async fn bad_example() {
    data: string = blocking_io_call();  // Blocks the executor!
    return data;
}

// Right: Use async version
async fn good_example() {
    data: string = await async_io_call();  // Non-blocking
    return data;
}
```

### ❌ DON'T: Overuse Async

```aria
// Wrong: No I/O, doesn't need to be async
async fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Right: Simple computation
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Common Patterns

### Timeout

```aria
async fn with_timeout<T>(fut: Future<T>, timeout_ms: i32) -> T? {
    Result: T? = await timeout(fut, timeout_ms);
    return result;
}

// Usage
user: User? = await with_timeout(fetch_user(123), 5000);
```

### Retry

```aria
async fn retry<T>(f: async fn() -> T?, max_attempts: i32) -> T? {
    till(max_attempts - 1, 1) {
        attempt: i32 = $ + 1;
        Result: T? = await f();
        when result != ERR then
            return result;
        end
        
        stddbg << "Retry " << attempt << "/" << max_attempts << "\n";
    }
    return ERR;
}
```

### Race

```aria
async fn race<T>(futures: []Future<T>) -> T {
    // Returns first future to complete
    return await select(futures);
}
```

---

## Related Topics

- [Async Functions](async_functions.md) - Detailed async programming guide
- [Function Declaration](function_declaration.md) - Function basics
- [Lambda Expressions](lambda.md) - Anonymous functions

---

**Remember**: `async` is for **I/O-bound** operations, not CPU-bound. Use it when you're waiting for something, not when you're computing something!
