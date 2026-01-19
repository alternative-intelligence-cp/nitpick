# Concurrency

**Category**: Advanced Features → Concurrency  
**Purpose**: Execute multiple tasks simultaneously

---

## Overview

Concurrency enables **parallel execution** for better performance and responsiveness.

---

## Concurrency Models

### Threading

```aria
import std.thread;

fn worker(id: i32) {
    stdout << "Worker $id running";
}

thread1: Thread = Thread.spawn(worker, 1);
thread2: Thread = Thread.spawn(worker, 2);

thread1.join();
thread2.join();
```

---

### Async/Await

```aria
async fn fetch_data(url: string) -> Result<Data> {
    response: Response = await http.get(url)?;
    data: Data = await response.json()?;
    return Ok(data);
}

async fn main() {
    data: Data = await fetch_data("https://api.example.com/data")?;
    stdout << data;
}
```

---

### Coroutines

```aria
fn* generator() -> i32 {
    yield 1;
    yield 2;
    yield 3;
}

for value in generator() {
    stdout << value;  // Prints 1, 2, 3
}
```

---

## Thread Safety

### Atomic Operations

```aria
import std.sync.Atomic;

counter: Atomic<i32> = Atomic.new(0);

fn increment() {
    counter.fetch_add(1);  // Thread-safe
}
```

---

### Mutex

```aria
import std.sync.Mutex;

data: Mutex<i32> = Mutex.new(0);

fn update() {
    lock: MutexGuard = data.lock();
    *lock += 1;
    // Automatically unlocks when lock goes out of scope
}
```

---

### Channels

```aria
import std.sync.Channel;

channel: Channel<i32> = Channel.new();

// Producer
fn producer() {
    for i in 0..10 {
        channel.send(i);
    }
}

// Consumer
fn consumer() {
    while true {
        value: i32 = channel.recv()?;
        stdout << "Received: $value";
    }
}
```

---

## Common Patterns

### Producer-Consumer

```aria
import std.sync.{Channel, Thread};

fn producer(ch: Channel<i32>) {
    for i in 0..100 {
        ch.send(i);
    }
}

fn consumer(ch: Channel<i32>) {
    while true {
        match ch.recv() {
            Ok(value) => stdout << value,
            Err(_) => break,
        }
    }
}

channel: Channel<i32> = Channel.new();
prod: Thread = Thread.spawn(producer, channel.clone());
cons: Thread = Thread.spawn(consumer, channel);

prod.join();
cons.join();
```

---

### Worker Pool

```aria
import std.sync.{Channel, Thread};

struct WorkerPool {
    workers: []Thread,
    sender: Channel<Task>,
}

impl WorkerPool {
    pub fn new(size: i32) -> WorkerPool {
        channel: Channel<Task> = Channel.new();
        workers: []Thread = [];
        
        for i in 0..size {
            worker: Thread = Thread.spawn(worker_thread, channel.clone());
            workers.push(worker);
        }
        
        return WorkerPool {
            workers: workers,
            sender: channel,
        };
    }
    
    pub fn execute(task: Task) {
        self.sender.send(task);
    }
}
```

---

### Async Request Handling

```aria
async fn handle_request(req: Request) -> Response {
    // Multiple concurrent operations
    user_task = fetch_user(req.user_id);
    posts_task = fetch_posts(req.user_id);
    
    // Wait for both
    user: User = await user_task?;
    posts: []Post = await posts_task?;
    
    return Response.ok(render(user, posts));
}
```

---

## Synchronization Primitives

### RwLock (Reader-Writer Lock)

```aria
import std.sync.RwLock;

data: RwLock<HashMap<string, i32>> = RwLock.new(HashMap.new());

// Multiple readers
fn read_data(key: string) -> ?i32 {
    reader: ReadGuard = data.read();
    return reader.get(key);
}

// Exclusive writer
fn write_data(key: string, value: i32) {
    writer: WriteGuard = data.write();
    writer.insert(key, value);
}
```

---

### Barrier

```aria
import std.sync.Barrier;

barrier: Barrier = Barrier.new(3);  // 3 threads

fn worker(id: i32) {
    stdout << "Worker $id: Phase 1";
    barrier.wait();  // Wait for all threads
    
    stdout << "Worker $id: Phase 2";
    barrier.wait();
    
    stdout << "Worker $id: Done";
}
```

---

### Semaphore

```aria
import std.sync.Semaphore;

semaphore: Semaphore = Semaphore.new(3);  // Max 3 concurrent

fn limited_resource() {
    semaphore.acquire();
    // Only 3 threads can be here at once
    process_data();
    semaphore.release();
}
```

---

## Best Practices

### ✅ DO: Use Channels for Communication

```aria
// Good - communicate via channels
channel: Channel<Message> = Channel.new();
thread1.send_channel(channel.clone());
thread2.send_channel(channel);
```

### ✅ DO: Avoid Shared Mutable State

```aria
// Good - each thread owns its data
fn worker(data: Data) {
    // Process independently
}

// Bad - shared mutable state
global_data: Data;  // Dangerous!
```

### ✅ DO: Use RAII for Locks

```aria
{
    lock: MutexGuard = mutex.lock();
    // Critical section
}  // Lock automatically released here
```

### ⚠️ WARNING: Avoid Deadlocks

```aria
// Deadlock scenario
mutex1.lock();
mutex2.lock();  // Thread A

mutex2.lock();
mutex1.lock();  // Thread B - DEADLOCK!

// Solution: Always lock in same order
```

---

## Related

- [async](async.md) - Async keyword
- [async_await](async_await.md) - Async/await pattern
- [threading](threading.md) - Threading
- [atomics](atomics.md) - Atomic operations
- [coroutines](coroutines.md) - Coroutines

---

**Remember**: Concurrency enables **parallelism** but requires **careful synchronization**!
