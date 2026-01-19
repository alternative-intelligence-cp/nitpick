# Async Programming Model - Technical Specification

**Document Type**: Technical Specification  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design Phase (Implementation Pending Research)

---

## Table of Contents

1. [Overview](#overview)
2. [Async/Await Syntax](#asyncawait-syntax)
3. [Future and Promise Types](#future-and-promise-types)
4. [M:N Threading Model](#mn-threading-model)
5. [Task Scheduling](#task-scheduling)
6. [Integration with 6-Stream I/O](#integration-with-6-stream-io)
7. [Error Handling](#error-handling)

---

## Overview

Aria's async model is designed for **high-concurrency I/O** without the complexity of manual thread management:

| Concept | Purpose | Benefit |
|---------|---------|---------|
| **async/await** | Suspend execution at await points | Non-blocking I/O |
| **Future<T>** | Placeholder for future value | Composable async operations |
| **M:N Threading** | M tasks on N OS threads | Efficient CPU utilization |
| **Task Scheduler** | Distribute work across threads | Automatic load balancing |
| **Async Runtime** | Execute async tasks | Unified execution model |

**Philosophy**: **Make concurrency easy and safe**

---

## Async/Await Syntax

### Function Declaration

**Async Function**: Returns `Future<T>` instead of `T`

**Syntax**:
```aria
async func:fetch_data = string(url: string) {
    // Function body can use 'await'
}
```

**Return Type**: `Future<string>` (implicit)

---

### Await Expression

**Syntax**: `await future_expression`

**Semantics**: Suspend execution until future completes

**Example**:
```aria
async func:download_file = result<bytes>(url: string) {
    // Initiate HTTP request (returns Future)
    Future<socket>:sock = async net.connect(url, 80);
    
    // Suspend until connection established
    socket:conn = await sock;
    
    // Send request (also async)
    await conn.send("GET / HTTP/1.1\r\n\r\n");
    
    // Read response
    bytes:response = await conn.recv(4096);
    
    pass(response);
}
```

**Generated Code** (state machine):
```c
// Compiler generates state machine
enum download_file_state {
    STATE_CONNECT,
    STATE_SEND,
    STATE_RECV,
    STATE_DONE
};

struct download_file_future {
    enum download_file_state state;
    socket conn;
    bytes response;
    // ... other state
};

aria_future_status download_file_poll(void* context) {
    struct download_file_future* fut = context;
    
    switch (fut->state) {
        case STATE_CONNECT:
            if (net_connect_ready()) {
                fut->conn = net_connect_complete();
                fut->state = STATE_SEND;
            } else {
                return FUTURE_PENDING;
            }
            // Fall through
        
        case STATE_SEND:
            if (socket_send_ready(fut->conn)) {
                socket_send_complete(fut->conn);
                fut->state = STATE_RECV;
            } else {
                return FUTURE_PENDING;
            }
            // Fall through
        
        case STATE_RECV:
            if (socket_recv_ready(fut->conn)) {
                fut->response = socket_recv_complete(fut->conn);
                fut->state = STATE_DONE;
                return FUTURE_READY;
            } else {
                return FUTURE_PENDING;
            }
        
        case STATE_DONE:
            return FUTURE_READY;
    }
}
```

---

### Calling Async Functions

**From Async Context**:
```aria
async func:process_data = void() {
    string:data = await fetch_data("https://example.com");
    io.stdout.write(data);
}
```

**From Sync Context** (block until complete):
```aria
func:main = i64() {
    // Block on async function
    Future<string>:fut = fetch_data("https://example.com");
    string:data = fut.block();  // Wait for completion
    
    io.stdout.write(data);
    pass(0);
}
```

---

## Future and Promise Types

### Future<T>

**Definition**: Asynchronous result that may not be ready yet

**Type**:
```aria
struct:Future<T> = {
    state: FutureState,
    result: result<T>,
    waker: Waker,  // Callback to wake task when ready
};

enum:FutureState = {
    Pending,
    Ready,
};
```

---

### Promise<T>

**Definition**: Write-side of a Future (producer)

**Type**:
```aria
struct:Promise<T> = {
    future: wild Future<T>*,
};
```

**Usage Pattern**:
```aria
// Create promise/future pair
(Promise<i32>, Future<i32>):pair = Promise.new<i32>();
Promise<i32>:promise = pair.0;
Future<i32>:future = pair.1;

// Producer: Set result later
async func:producer = void(p: Promise<i32>) {
    // Do some work...
    await some_async_operation();
    
    // Complete promise
    p.complete(42);
}

// Consumer: Wait for result
async func:consumer = void(f: Future<i32>) {
    i32:result = await f;
    io.stdout.write("Got result: " + result + "\n");
}
```

---

### Future Methods

**`.await`**: Suspend until ready
```aria
T:value = await future;
```

**`.block()`**: Block current thread until ready (sync context only)
```aria
T:value = future.block();
```

**`.poll()`**: Check if ready without blocking
```aria
if (future.poll() == FutureState.Ready) {
    T:value = future.get();
}
```

**`.map()`**: Transform future value
```aria
Future<string>:name_future = fetch_name();
Future<i32>:len_future = name_future.map(|s| s.len());
```

**`.then()`**: Chain async operations
```aria
Future<bytes>:data = fetch_data()
    .then(|raw| parse(raw))
    .then(|parsed| validate(parsed));
```

---

## M:N Threading Model

### Concept

**M:N Threading**: M tasks running on N OS threads (M >> N)

**Example**: 10,000 tasks on 8 threads

**Benefit**: Tasks are lightweight (stack = 4KB), threads are expensive (stack = 2MB)

---

### Architecture

```
Application:
  ┌─────────────────────────────────────────┐
  │ 10,000 Async Tasks (4 KB each)          │
  │ - HTTP request handlers                 │
  │ - Database queries                      │
  │ - File I/O operations                   │
  └─────────────────────────────────────────┘
                   ↓
         Runtime Task Scheduler
  ┌─────────────────────────────────────────┐
  │ Work-Stealing Queue                     │
  │ - Distribute tasks to threads           │
  │ - Balance load dynamically              │
  └─────────────────────────────────────────┘
                   ↓
           OS Thread Pool (N=8)
  ┌────┬────┬────┬────┬────┬────┬────┬────┐
  │ T1 │ T2 │ T3 │ T4 │ T5 │ T6 │ T7 │ T8 │
  └────┴────┴────┴────┴────┴────┴────┴────┘
```

---

### Task Representation

**Lightweight Task**:
```c
typedef struct aria_task {
    aria_future* future;  // Current future being awaited
    void* stack;  // Small stack (4 KB)
    void* context;  // Task-local data
    struct aria_task* next;  // Queue link
} aria_task;
```

**Memory Usage**:
- 10,000 tasks × 4 KB = 40 MB (lightweight)
- vs. 10,000 threads × 2 MB = 20 GB (prohibitive)

---

## Task Scheduling

### Work-Stealing Scheduler

**Algorithm**:
1. Each thread has local queue of tasks
2. Thread pops tasks from own queue (LIFO for cache locality)
3. When queue empty, steal tasks from other threads (FIFO to avoid contention)

**Implementation**:
```c
typedef struct {
    aria_task* local_queue[1024];
    size_t local_head;
    size_t local_tail;
    
    aria_task* global_queue;  // Fallback for overflow
    pthread_mutex_t global_mutex;
} aria_scheduler;

void worker_thread(aria_scheduler* sched, int thread_id) {
    while (true) {
        // Try local queue first
        aria_task* task = pop_local(sched, thread_id);
        
        if (!task) {
            // Steal from other threads
            task = steal_task(sched, thread_id);
        }
        
        if (!task) {
            // Check global queue
            task = pop_global(sched);
        }
        
        if (!task) {
            // No work, park thread
            park_thread(sched, thread_id);
            continue;
        }
        
        // Execute task
        run_task(task);
        
        // If task is still pending, re-enqueue
        if (task->future->state == FUTURE_PENDING) {
            push_local(sched, thread_id, task);
        }
    }
}
```

---

### Task Lifecycle

**1. Spawn**:
```aria
async func:main = i64() {
    // Spawn async task
    Future<void>:task1 = spawn(async_function());
    Future<void>:task2 = spawn(another_async_function());
    
    // Wait for both
    await task1;
    await task2;
    
    pass(0);
}
```

**2. Schedule**: Runtime adds task to queue

**3. Execute**: Worker thread runs task until await point

**4. Suspend**: Task yields, re-queued if not ready

**5. Wake**: I/O completion wakes task

**6. Resume**: Task continues execution

**7. Complete**: Task finishes, future marked ready

---

### Cooperative Scheduling

**Key Property**: Tasks voluntarily yield at `await` points

**Non-Preemptive**: No interrupts, no context switching overhead

**Benefit**: Predictable performance, no race conditions

**Limitation**: CPU-bound tasks can block (must insert `await` points)

**Solution**: `yield()` primitive
```aria
async func:cpu_intensive = void() {
    for (i in 0..1000000) {
        // Heavy computation
        compute(i);
        
        // Yield periodically
        if (i % 1000 == 0) {
            await yield();  // Let other tasks run
        }
    }
}
```

---

## Integration with 6-Stream I/O

### Async I/O Operations

**All I/O is async** (non-blocking)

**Example**:
```aria
async func:read_file = result<string>(path: string) {
    // Open file (async)
    result<file>:f = await io.open(path);
    if (f.is_err()) err(f.err());
    
    // Read contents (async)
    result<string>:contents = await f.ok().read_all();
    if (contents.is_err()) err(contents.err());
    
    pass(contents.ok());
}
```

---

### 6-Stream Async API

**Runtime** (internal):
```c
// Register file descriptor with event loop (epoll/kqueue)
void aria_io_register_fd(int fd, aria_waker* waker) {
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;  // Edge-triggered
    ev.data.ptr = waker;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}

// Event loop (runs on I/O thread)
void aria_io_event_loop(void) {
    epoll_event events[256];
    
    while (true) {
        int n = epoll_wait(epoll_fd, events, 256, -1);
        
        for (int i = 0; i < n; i++) {
            aria_waker* waker = events[i].data.ptr;
            
            // Wake task
            waker->wake();
        }
    }
}
```

---

**Async Read (stdin)**:
```aria
async func:read_line = result<string>() {
    result<string>:line = await io.stdin.read_line();
    pass(line);
}
```

**Generated Runtime Call**:
```c
aria_future_string aria_stdin_read_line_async(void) {
    // Register stdin (FD 0) for read events
    aria_waker* waker = create_waker();
    aria_io_register_fd(0, waker);
    
    // Return pending future
    aria_future_string fut;
    fut.state = FUTURE_PENDING;
    fut.waker = waker;
    return fut;
}
```

**When Data Available**: Event loop wakes task, task resumes and reads data

---

### Concurrent I/O

**Pattern**: Spawn multiple I/O tasks, run concurrently

**Example** (fetch multiple URLs):
```aria
async func:fetch_all = array<string>(urls: array<string>) {
    array<Future<string>>:futures = array<Future<string>>(urls.len());
    
    // Spawn all requests concurrently
    for (i in 0..urls.len()) {
        futures[i] = spawn(fetch_url(urls[i]));
    }
    
    // Wait for all to complete
    array<string>:results = array<string>(urls.len());
    for (i in 0..urls.len()) {
        results[i] = await futures[i];
    }
    
    pass(results);
}
```

**Result**: All requests run in parallel, limited only by OS resources

---

## Error Handling

### Async Functions Return Result<T>

**Signature**:
```aria
async func:risky_operation = result<i32>() {
    // Can fail
}
```

**Actual Return Type**: `Future<result<i32>>`

---

### Error Propagation with `?`

**Example**:
```aria
async func:chain = result<string>() {
    result<socket>:sock = await net.connect("example.com", 80)?;
    result<void>:sent = await sock.send("GET / HTTP/1.1\r\n\r\n")?;
    result<bytes>:data = await sock.recv(4096)?;
    pass(parse(data));
}
```

**Semantics**: If any `await?` fails, entire function returns Err early

---

### Timeout

**Pattern**: Race future against timeout

**Example**:
```aria
async func:fetch_with_timeout = result<string>(url: string, timeout_ms: i32) {
    Future<result<string>>:fetch = spawn(fetch_url(url));
    Future<void>:timeout = sleep(timeout_ms);
    
    // Race: first to complete wins
    result<string>:result = await race(fetch, timeout);
    pass(result);
}
```

---

## Related Documents

- **[TYPE_SYSTEM](./TYPE_SYSTEM.md)**: Future<T> type definition
- **[ERROR_HANDLING](./ERROR_HANDLING.md)**: Result<T> with async
- **[IO_TOPOLOGY](./IO_TOPOLOGY.md)**: 6-stream async I/O
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Async runtime implementation
- **[MEMORY_MODEL](./MEMORY_MODEL.md)**: Task stack allocation

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Design specification (implementation pending Gemini research on async/await semantics and Future/Promise runtime mapping)

**Pending Research** (from Gemini tasks):
- Async/await syntax and semantics (language_advanced_research_task.txt)
- Future/Promise implementation strategies
- Task scheduler optimizations
- Integration with ownership/lifetime system
