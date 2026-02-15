# Threading

**Category**: Advanced Features → Concurrency  
**Purpose**: Parallel execution using OS threads

---

## Overview

Threads enable **true parallelism** by running code on multiple CPU cores simultaneously.

---

## Basic Thread

```aria
import std.thread.Thread;

fn worker() {
    stdout << "Hello from thread!";
}

fn main() {
    thread: Thread = Thread.spawn(worker);
    thread.join();  // Wait for completion
}
```

---

## Thread with Arguments

```aria
import std.thread.Thread;

fn worker(id: i32, message: string) {
    stdout << "Thread $id: $message";
}

fn main() {
    thread: Thread = Thread.spawn(|| {
        worker(1, "Hello");
    });
    thread.join();
}
```

---

## Multiple Threads

```aria
import std.thread.Thread;

fn main() {
    threads: []Thread = [];
    
    till(3, 1) {
        thread: Thread = Thread.spawn(|| {
            stdout << "Worker $($)";
        });
        threads.push(thread);
    }
    
    // Wait for all threads
    till(threads.length - 1, 1) {
        threads[$].join();
    }
}
```

---

## Thread Return Values

```aria
import std.thread.Thread;

fn compute(n: i32) -> i32 {
    return n * n;
}

fn main() {
    thread: Thread<i32> = Thread.spawn(|| {
        return compute(10);
    });
    
    Result: i32 = thread.join();
    stdout << "Result: $result";  // Result: 100
}
```

---

## Thread Synchronization

### Mutex

```aria
import std.sync.Mutex;
import std.thread.Thread;

counter: Mutex<i32> = Mutex.new(0);

fn worker() {
    till(999, 1) {
        lock = counter.lock();
        *lock += 1;
        // Mutex automatically unlocked when lock goes out of scope
    }
}

fn main() {
    threads: []Thread = [];
    till(3, 1) {
        threads.push(Thread.spawn(worker));
    }
    
    till(threads.length - 1, 1) {
        threads[$].join();
    }
    
    stdout << "Counter: ${counter.lock()}";  // Counter: 4000
}
```

---

### Channels

```aria
import std.sync.Channel;
import std.thread.Thread;

fn producer(tx: Sender<i32>) {
    till(9, 1) {
        tx.send($);
    }
}

fn consumer(rx: Receiver<i32>) {
    while value = rx.recv() {
        stdout << "Received: $value";
    }
}

fn main() {
    channel: Channel<i32> = Channel.new();
    
    t1: Thread = Thread.spawn(|| producer(channel.sender()));
    t2: Thread = Thread.spawn(|| consumer(channel.receiver()));
    
    t1.join();
    channel.close();
    t2.join();
}
```

---

## Common Patterns

### Worker Pool

```aria
import std.sync.Channel;
import std.thread.Thread;

struct WorkerPool<T, R> {
    workers: []Thread,
    task_queue: Channel<T>,
    result_queue: Channel<R>,
}

impl<T, R> WorkerPool<T, R> {
    pub fn new(num_workers: i32, work_fn: fn(T) -> R) -> WorkerPool {
        task_queue: Channel<T> = Channel.new();
        result_queue: Channel<R> = Channel.new();
        workers: []Thread = [];
        
        till(num_workers - 1, 1) {
            rx = task_queue.receiver();
            tx = result_queue.sender();
            
            worker: Thread = Thread.spawn(|| {
                while task = rx.recv() {
                    Result: R = work_fn(task);
                    tx.send(result);
                }
            });
            workers.push(worker);
        }
        
        return WorkerPool {
            workers: workers,
            task_queue: task_queue,
            result_queue: result_queue,
        };
    }
    
    pub fn submit(task: T) {
        self.task_queue.send(task);
    }
    
    pub fn get_result() -> ?R {
        return self.result_queue.recv();
    }
    
    pub fn shutdown() {
        self.task_queue.close();
        till(self.workers.length - 1, 1) {
            self.workers[$].join();
        }
    }
}

// Usage
fn process_data(data: i32) -> i32 {
    return data * 2;
}

fn main() {
    pool: WorkerPool<i32, i32> = WorkerPool.new(4, process_data);
    
    // Submit tasks
    till(99, 1) {
        pool.submit($);
    }
    
    // Collect results
    till(99, 1) {
        Result: i32 = pool.get_result()?;
        stdout << result;
    }
    
    pool.shutdown();
}
```

---

### Producer-Consumer

```aria
import std.sync.Channel;
import std.thread.Thread;

fn producer(tx: Sender<string>) {
    till(9, 1) {
        message: string = "Message $($)";
        tx.send(message);
        Thread.sleep(100);  // Simulate work
    }
}

fn consumer(id: i32, rx: Receiver<string>) {
    while message = rx.recv() {
        stdout << "Consumer $id got: $message";
        Thread.sleep(50);  // Simulate processing
    }
}

fn main() {
    channel: Channel<string> = Channel.new();
    
    // One producer
    p: Thread = Thread.spawn(|| producer(channel.sender()));
    
    // Multiple consumers
    consumers: []Thread = [];
    till(2, 1) {
        rx = channel.receiver();
        consumer: Thread = Thread.spawn(|| consumer($, rx));
        consumers.push(consumer);
    }
    
    p.join();
    channel.close();
    
    till(consumers.length - 1, 1) {
        consumers[$].join();
    }
}
```

---

### Parallel Map

```aria
import std.thread.Thread;

fn parallel_map<T, R>(items: []T, f: fn(T) -> R, num_threads: i32) -> []R {
    results: []R = [];
    chunk_size: i32 = items.len() / num_threads;
    threads: []Thread<[]R> = [];
    
    till(num_threads - 1, 1) {
        i = $;
        start: i32 = i * chunk_size;
        end: i32 = if i == num_threads - 1 {
            items.len()
        } else {
            (i + 1) * chunk_size
        };
        
        chunk: []T = items[start..end];
        
        thread: Thread<[]R> = Thread.spawn(|| {
            chunk_results: []R = [];
            till(chunk.length - 1, 1) {
                chunk_results.push(f(chunk[$]));
            }
            return chunk_results;
        });
        threads.push(thread);
    }
    
    till(threads.length - 1, 1) {
        chunk_results: []R = threads[$].join();
        results.extend(chunk_results);
    }
    
    return results;
}

// Usage
fn square(x: i32) -> i32 {
    return x * x;
}

fn main() {
    numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8];
    squared: []i32 = parallel_map(numbers, square, 4);
    stdout << squared;
}
```

---

## Thread-Local Storage

```aria
import std.thread.ThreadLocal;

thread_id: ThreadLocal<i32> = ThreadLocal.new();

fn worker(id: i32) {
    thread_id.set(id);
    
    // Each thread has its own copy
    my_id: i32 = thread_id.get();
    stdout << "Thread $my_id";
}
```

---

## Best Practices

### ✅ DO: Use for CPU-Bound Work

```aria
fn parallel_compute(data: []f64) -> f64 {
    threads: []Thread<f64> = [];
    chunks = data.chunks(1000);
    
    till(chunks.length - 1, 1) {
        thread: Thread<f64> = Thread.spawn(|| {
            return expensive_computation(chunks[$]);
        });
        threads.push(thread);
    }
    
    sum: f64 = 0.0;
    till(threads.length - 1, 1) {
        sum += threads[$].join();
    }
    return sum;
}
```

### ✅ DO: Use Synchronization Primitives

```aria
import std.sync.Mutex;

shared_data: Mutex<HashMap<string, i32>> = Mutex.new(HashMap.new());

fn update_shared() {
    lock = shared_data.lock();
    lock.insert("key", 42);
    // Automatically unlocked
}
```

### ⚠️ WARNING: Avoid Data Races

```aria
// ❌ Bad - data race!
counter: i32 = 0;

fn increment() {
    counter += 1;  // Not thread-safe!
}

// ✅ Good - use atomics or mutex
import std.sync.Atomic;
counter: Atomic<i32> = Atomic.new(0);

fn increment() {
    counter.fetch_add(1);  // Thread-safe
}
```

### ❌ DON'T: Create Too Many Threads

```aria
// ❌ Bad - too many threads
till(9999, 1) {
    Thread.spawn(worker);
}

// ✅ Good - use thread pool
pool: WorkerPool = WorkerPool.new(8);  // Match CPU cores
till(9999, 1) {
    pool.submit(work);
}
```

---

## Related

- [atomics](atomics.md) - Atomic operations
- [async](async.md) - Async/await
- [concurrency](concurrency.md) - Concurrency overview

---

**Remember**: Threads provide **true parallelism** but require **synchronization** to avoid races!
