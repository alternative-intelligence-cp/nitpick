# Aria Threading & Concurrency Implementation Plan
**Based on**: research_007_threading_library.txt (comprehensive design)  
**Date**: December 11, 2025  
**Status**: Research complete, ready for Phase 7 implementation

---

## Executive Summary

The Aria Concurrency System (ACS) implements a **Hybrid M:N + 1:1 Threading Model** that combines the best of both worlds:
- **M:N Task Runtime**: Lightweight coroutines (async/await) with work-stealing scheduler
- **1:1 Thread Subsystem**: Direct OS thread control for CPU-bound workloads

This design avoids the "colored function" problem through explicit bridging (`spawn_blocking`) and integrates seamlessly with Aria's Appendage Theory for memory-safe concurrency.

---

## The Hybrid Concurrency Model

### Two-Layer Architecture

```
┌─────────────────────────────────────────┐
│  Application Layer (Aria Code)          │
├─────────────────────────────────────────┤
│  aria.task (M:N)     │  aria.thread (1:1)│
│  • Lightweight       │  • OS threads     │
│  • Work-stealing     │  • pthread/Win32  │
│  • async/await       │  • CPU-bound work │
├─────────────────────────────────────────┤
│  Runtime Layer                           │
│  • Work-Stealing Scheduler               │
│  • Chase-Lev Deques (lock-free)          │
│  • Global Injection Queue                │
│  • Blocking Thread Pool                  │
├─────────────────────────────────────────┤
│  OS Kernel (Linux/Windows)               │
└─────────────────────────────────────────┘
```

### Design Rationale

| Model | Use Case | Example | Overhead |
|-------|----------|---------|----------|
| **M:N Task** | Async I/O, network, millions of concurrent operations | HTTP server, database queries | ~200-500 bytes/task |
| **1:1 Thread** | CPU-intensive, blocking C FFI, real-time priorities | Matrix multiply, crypto, legacy libs | ~8MB/thread |

**Key Insight**: Explicit choice prevents hidden performance cliffs (unlike Go's implicit model).

---

## Work-Stealing Scheduler

### The Chase-Lev Deque (Lock-Free)

Core data structure for work-stealing: one owner (push/pop bottom), multiple thieves (steal top).

```cpp
template <typename T>
class WorkStealingDeque {
    std::atomic<T*> buffer;        // Cyclic buffer of tasks
    std::atomic<size_t> top;       // Thieves steal from top (FIFO)
    std::atomic<size_t> bottom;    // Owner push/pop bottom (LIFO)
    size_t capacity;

public:
    // Owner pushes task to bottom (hot path, cache-friendly)
    void push(T task) {
        size_t b = bottom.load(std::memory_order_relaxed);
        size_t t = top.load(std::memory_order_acquire);
        
        if (b - t >= capacity - 1) resize();  // Rare: grow buffer
        
        buffer[b % capacity].store(task, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        bottom.store(b + 1, std::memory_order_relaxed);
    }

    // Owner pops from bottom (LIFO = cache locality)
    T pop() {
        size_t b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t t = top.load(std::memory_order_relaxed);
        
        if (t > b) {  // Empty
            bottom.store(b + 1, std::memory_order_relaxed);
            return nullptr;
        }
        
        T task = buffer[b % capacity].load(std::memory_order_relaxed);
        
        if (t == b) {  // Last item: race with thieves
            if (!top.compare_exchange_strong(t, t + 1, 
                                            std::memory_order_seq_cst,
                                            std::memory_order_relaxed)) {
                bottom.store(b + 1, std::memory_order_relaxed);
                return nullptr;  // Lost to thief
            }
            bottom.store(b + 1, std::memory_order_relaxed);
        }
        return task;
    }

    // Thief steals from top (FIFO = work balancing)
    T steal() {
        size_t t = top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        size_t b = bottom.load(std::memory_order_acquire);
        
        if (t >= b) return nullptr;  // Empty
        
        T task = buffer[t % capacity].load(std::memory_order_relaxed);
        
        if (!top.compare_exchange_strong(t, t + 1,
                                        std::memory_order_seq_cst,
                                        std::memory_order_relaxed)) {
            return nullptr;  // Lost to another thief
        }
        return task;
    }
};
```

**Why This Works**:
- Owner operations (push/pop) are mostly relaxed (fast)
- Thieves use acquire/release (ensures visibility)
- CAS on last item prevents ABA problem
- LIFO for owner = hot data in cache
- FIFO for thieves = load balancing

### Scheduler Loop

```cpp
void WorkerThread::run() {
    while (!shutdown_requested) {
        Task* task = nullptr;
        
        // 1. Check global queue periodically (fairness)
        if (tick_count++ % 61 == 0) {
            task = global_queue.try_dequeue();
        }
        
        // 2. Pop from local queue (LIFO, hot cache)
        if (!task) {
            task = local_deque.pop();
        }
        
        // 3. Steal from random worker (FIFO, load balance)
        if (!task) {
            int victim = random() % num_workers;
            for (int i = 0; i < num_workers; i++) {
                task = workers[(victim + i) % num_workers].deque.steal();
                if (task) break;
            }
        }
        
        // 4. Park if still no work (save CPU)
        if (!task) {
            std::unique_lock<std::mutex> lock(park_mutex);
            park_cond.wait_for(lock, 100ms, [&]{ return has_work(); });
            continue;
        }
        
        // 5. Execute task
        task->resume();
        
        // 6. If task yielded (not complete), re-enqueue
        if (!task->is_complete()) {
            local_deque.push(task);
        } else {
            task->set_result();
            delete task;
        }
    }
}
```

**Key Mechanisms**:
- **Tick 61 heuristic**: Borrowed from Go, prevents global queue starvation
- **Random victim selection**: Prevents pathological patterns
- **Park/unpark**: Reduces CPU usage when idle (via condvar)

---

## API Design

### aria.task Module (M:N Scheduler)

```aria
mod task {
    // Spawn lightweight async task (eager execution)
    pub func:spawn<T> = Future<T>(async func:f) {
        // Task starts immediately, returns Future handle
    }
    
    // Bridge to blocking thread pool
    pub func:spawn_blocking<T> = Future<T>(func:f) {
        // Offloads sync work to dedicated pool
        // Prevents blocking M:N workers
    }
    
    // Cooperative yield (reschedule self)
    pub async func:yield_now = void() {
        // Task suspended, moved to back of queue
    }
    
    // Entry point: run async runtime from sync code
    pub func:block_on<T> = Result<T>(Future<T>:fut) {
        // Drives scheduler until future completes
    }
    
    // Get current task ID
    pub func:id = uint64();
}
```

**Usage Example**:
```aria
use aria.task;
use aria.io;

async func:process_request = void(int:id) {
    // Async I/O (M:N scheduler)
    string:data = await io.read_async("data.txt");
    
    // Heavy computation (blocking pool, prevents worker stalling)
    int:result = await task.spawn_blocking(func() {
        return heavy_math(data);  // CPU-bound work
    });
    
    print(`Request &{id}: &{result}`);
}

func:main = void() {
    task.block_on(async {
        // Spawn 10,000 concurrent tasks (M:N magic!)
        for (i in 0..10000) {
            task.spawn(process_request(i));
        }
    });
}
```

### aria.thread Module (1:1 OS Threads)

```aria
mod thread {
    // Handle to OS thread
    pub struct JoinHandle<T> {
        wild void*: native_handle;  // pthread_t or HANDLE
        
        pub func:join = Result<T>(self) {
            // Blocks until thread finishes
        }
    }
    
    // Spawn OS thread
    pub func:spawn<T> = Result<JoinHandle<T>>(func:f) {
        // Creates pthread (Linux) or CreateThread (Windows)
    }
    
    // Yield timeslice
    pub func:yield_now = void() {
        // sched_yield() or SwitchToThread()
    }
    
    // Sleep thread
    pub func:sleep = void(int64:milliseconds);
    
    // Get thread ID
    pub func:id = uint64();
}
```

**Usage Example**:
```aria
use aria.thread;

func:main = void() {
    // Spawn 4 OS threads for parallel computation
    thread.JoinHandle<int>[]handles = [];
    
    for (i in 0..4) {
        result:h = thread.spawn(func() {
            return expensive_computation(i);
        });
        handles.push(h.val);
    }
    
    // Wait for all threads
    for (h in handles) {
        int:result = h.join() ? 0;
        print(`Result: &{result}`);
    }
}
```

---

## Synchronization Primitives

### Async Mutex (Task-Aware)

```aria
mod sync {
    pub struct Mutex<T> {
        wild void*: state;  // Internal: wait queue of Wakers
        T: data;
        
        // Returns Future that resolves to guard
        pub async func:lock = MutexGuard<T>(self) {
            // If contested: suspend task, enqueue Waker
            // If available: acquire immediately
        }
        
        // Non-blocking attempt
        pub func:try_lock = Result<MutexGuard<T>>(self);
    }
    
    pub struct MutexGuard<T> {
        Mutex<T>*: lock_ref;
        
        pub func:get = T*(self);
        
        // RAII: drops guard releases lock, wakes next waiter
    }
}
```

**Implementation Strategy**:
1. **Fast path**: Atomic CAS to acquire (spin briefly)
2. **Slow path**: Suspend task, register Waker in wait queue
3. **Release**: Wake oldest Waker from queue

**Critical Safety Rule**: **Never hold async mutex across `await`** (compiler warning).

### Sync Mutex (Thread-Blocking)

```aria
mod thread {
    pub struct Mutex<T> {
        wild void*: native_mutex;  // pthread_mutex_t or SRWLOCK
        T: data;
        
        pub func:lock = MutexGuard<T>(self) {
            // Blocks thread (not task!)
        }
    }
}
```

**Warning**: Using `thread::Mutex` inside async block blocks the worker thread → deadlock risk.

### Channels (Go-Style CSP)

```aria
mod sync {
    pub func:channel<T> = tuple(Sender<T>, Receiver<T>)(int:capacity);
    
    pub struct Sender<T> {
        // Suspends if channel full (backpressure)
        pub async func:send = Result<void>(self, T:item);
        
        // Non-blocking try
        pub func:try_send = Result<void>(self, T:item);
    }
    
    pub struct Receiver<T> {
        // Suspends if channel empty
        pub async func:recv = Result<T>(self);
        
        // Iterator support
        pub async func:next = Result<T>(self);
    }
}
```

**Internal Mechanics**:
- Lock-free ring buffer for data
- Send waiters queue (when full)
- Recv waiters queue (when empty)
- CAS-based index management

**Usage Example**:
```aria
use aria.sync;
use aria.task;

func:main = void() {
    tuple(Sender<int>, Receiver<int>):ch = sync.channel<int>(10);
    
    // Producer
    task.spawn(async {
        for (i in 0..100) {
            await ch.0.send(i);
        }
    });
    
    // Consumer
    task.spawn(async {
        while (true) {
            result:msg = await ch.1.recv();
            if (msg.err != NULL) break;
            print(`Received: &{msg.val}`);
        }
    });
}
```

---

## Integration with Appendage Theory

### Thread Safety Traits

Aria enforces memory safety across threads via **Send** and **Sync** traits (like Rust):

1. **Send**: Type can be moved to another thread
   - TBB types ✓ (primitive values)
   - `wild` pointers ✗ (unless wrapped in Arc)
   - GC types ✓ (if GC is thread-safe)

2. **Sync**: Type's immutable reference (`$T`) can be shared across threads
   - Immutable primitives ✓
   - `Mutex<T>` is Sync if `T` is Send
   - Raw `wild` pointers ✗

### Compiler Enforcement

```aria
// Function signature enforces Send
pub func:spawn<T: Send> = Future<T>(async func:f);

// This compiles:
task.spawn(async {
    tbb64:value = 42;  // TBB types are Send
    // ... use value ...
});

// This fails at compile time:
wild int*:ptr = aria.alloc<int>(1);
task.spawn(async {
    *ptr = 42;  // ERROR: wild int* is not Send
});
```

### Pinning for Shared Data

When sharing GC objects across threads, use `#` (pin) to prevent GC movement:

```aria
use aria.sync;

string:shared_data = "hello";
wild string*:pinned = #shared_data;  // Pin it

Mutex<wild string*>:mutex = sync.Mutex::new(pinned);

// Thread 1
thread.spawn(func() {
    wild string*:ptr = mutex.lock().get();
    // Safe: pinned, won't move
});

// Thread 2
thread.spawn(func() {
    wild string*:ptr = mutex.lock().get();
    // Also safe
});
```

**Borrow Checker Integration**:
- Verifies `shared_data` remains pinned while `mutex` exists
- Ensures `mutex` doesn't outlive `shared_data`
- All checked at compile time!

---

## Blocking Thread Pool (spawn_blocking)

### Architecture

```
┌─────────────────────────────────────┐
│  M:N Task Scheduler (Fixed Size)    │
│  N workers = num_cores               │
├─────────────────────────────────────┤
│         ↓ spawn_blocking()           │
├─────────────────────────────────────┤
│  Blocking Thread Pool (Dynamic)     │
│  • Starts with N threads             │
│  • Grows on demand (max 512)         │
│  • Shrinks after idle timeout (60s)  │
└─────────────────────────────────────┘
```

### Implementation

```cpp
class BlockingPool {
    std::vector<std::thread> threads;
    MPMCQueue<Task*> work_queue;  // Multi-producer multi-consumer
    std::atomic<int> idle_count;
    std::atomic<int> total_count;
    const int MAX_THREADS = 512;

public:
    void submit(Task* task) {
        work_queue.enqueue(task);
        
        // Spawn new thread if all busy (up to MAX_THREADS)
        if (idle_count.load() == 0 && total_count.load() < MAX_THREADS) {
            spawn_worker();
        }
        
        // Wake a parked thread
        wake_one();
    }

private:
    void worker_loop() {
        while (true) {
            idle_count.fetch_add(1);
            
            Task* task = work_queue.dequeue_timeout(60s);
            
            idle_count.fetch_sub(1);
            
            if (!task) {
                // Timed out, shut down if excess capacity
                if (total_count.load() > num_cores) {
                    total_count.fetch_sub(1);
                    return;  // Thread exits
                }
                continue;
            }
            
            task->execute();
            task->notify_completion();  // Wake waiting async task
        }
    }
};
```

**Key Features**:
- **Dynamic scaling**: Grows under load, shrinks when idle
- **Isolation**: Never blocks M:N workers
- **Fairness**: FIFO queue prevents starvation

---

## Performance Characteristics

### Task vs Thread Comparison

| Metric | aria.task (M:N) | aria.thread (1:1) |
|--------|-----------------|-------------------|
| Memory per unit | ~200-500 bytes | ~8 MB |
| Creation cost | ~100 ns | ~10-100 μs |
| Context switch | ~10-50 ns | ~1-10 μs |
| Max concurrent | Millions | ~10K |
| Best for | I/O, network, events | CPU, blocking C FFI |

### Benchmark Goals

**C10K Test** (10,000 concurrent connections):
- Target: <100MB memory (10KB/connection)
- Target: <1ms p99 latency

**CPU-bound Test** (matrix multiplication):
- Target: Linear speedup with cores (1:1 threads)
- Target: No interference with async I/O

---

## Implementation Roadmap

### Phase 7.1: Work-Stealing Scheduler (3 weeks)

1. **Chase-Lev Deque** (`src/runtime/work_stealing_deque.{h,cpp}`)
   - Implement lock-free deque (500 lines C++)
   - Unit tests: push, pop, steal, resize
   - Validate with ThreadSanitizer

2. **Worker Threads** (`src/runtime/worker_thread.cpp`)
   - Implement scheduler loop (spawn N workers)
   - Local deque per worker
   - Steal logic (random victim selection)

3. **Global Injection Queue** (`src/runtime/global_queue.cpp`)
   - MPMC lock-free queue (or mutex-based fallback)
   - Integration with worker loop (check every 61 ticks)

### Phase 7.2: Task Runtime API (2 weeks)

1. **aria.task Module** (`stdlib/task.aria` + C++ runtime)
   - `spawn<T>()`: Create task, push to local deque
   - `yield_now()`: Suspend, re-enqueue
   - `block_on()`: Entry point, drives scheduler

2. **CoroutineFrame Layout** (LLVM backend integration)
   - State machine for async functions
   - Waker registration for resumption
   - Result storage for completed tasks

### Phase 7.3: Blocking Thread Pool (1 week)

1. **BlockingPool Class** (`src/runtime/blocking_pool.cpp`)
   - Dynamic thread pool (grows/shrinks)
   - MPMC work queue
   - Timeout-based cleanup

2. **spawn_blocking API**
   - Submit to blocking pool
   - Return Future that resolves on completion
   - Waker integration

### Phase 7.4: Thread Subsystem (1 week)

1. **aria.thread Module** (`stdlib/thread.aria` + C++ runtime)
   - `spawn()`: Wrapper around pthread_create / CreateThread
   - `JoinHandle`: RAII wrapper for thread handle
   - Platform-specific implementations

### Phase 7.5: Synchronization Primitives (3 weeks)

1. **Async Mutex** (`src/runtime/async_mutex.cpp`)
   - Atomic state + wait queue of Wakers
   - Fast path: CAS acquire
   - Slow path: Suspend task, enqueue Waker
   - RAII guard with automatic unlock

2. **Sync Mutex** (`src/runtime/sync_mutex.cpp`)
   - Wrapper around pthread_mutex_t / SRWLOCK
   - Simpler than async (just blocks thread)

3. **Channels** (`src/runtime/channel.cpp`)
   - Lock-free ring buffer (bounded)
   - Send/recv wait queues
   - Backpressure handling

4. **Semaphores** (if needed)
   - Both async and sync variants

### Phase 7.6: Borrow Checker Integration (2 weeks)

1. **Send/Sync Traits**
   - Define traits in type system
   - Implement for built-in types
   - Enforce in spawn/spawn_blocking

2. **Cross-await Mutex Check**
   - Detect holding async mutex across await
   - Emit compiler warning

3. **Pin Verification for Shared Data**
   - Ensure GC objects are pinned before sharing
   - Verify pins don't outlive data

### Phase 7.7: Testing & Optimization (2 weeks)

1. **Unit Tests**
   - Work-stealing correctness (1000 tasks, random work)
   - Mutex fairness (no starvation)
   - Channel throughput (SPSC, MPSC, MPMC)

2. **Integration Tests**
   - C10K test (10K concurrent connections)
   - Mixed CPU/IO workload
   - spawn_blocking stress test

3. **Benchmarks**
   - Compare with Tokio (Rust)
   - Compare with Go goroutines
   - Latency histograms (p50, p99, p999)

4. **Optimization**
   - Profile hot paths with perf
   - NUMA-aware worker placement
   - Cache-line padding for atomics

---

## Total Timeline

**Phase 7: Threading & Concurrency**
- 7.1 Work-Stealing Scheduler: 3 weeks
- 7.2 Task Runtime API: 2 weeks
- 7.3 Blocking Thread Pool: 1 week
- 7.4 Thread Subsystem: 1 week
- 7.5 Synchronization Primitives: 3 weeks
- 7.6 Borrow Checker Integration: 2 weeks
- 7.7 Testing & Optimization: 2 weeks

**Total: 14 weeks (~3.5 months)**

**Dependencies**:
- Phase 2: Borrow Checker (for Send/Sync enforcement)
- Phase 6: I/O Streams (for async I/O integration)
- LLVM Coroutines: Already supported (LLVM 20.1.2)

---

## Success Criteria

1. ✅ C10K test passes (<100MB memory, <1ms p99 latency)
2. ✅ CPU-bound test scales linearly with cores
3. ✅ No deadlocks in mixed async/sync code (compiler prevents)
4. ✅ Zero-cost abstractions validated (benchmarks vs hand-rolled)
5. ✅ Thread safety enforced at compile time (Send/Sync)
6. ✅ Work-stealing fairness (no starvation in stress tests)
7. ✅ Integration with borrow checker (pins verified)
8. ✅ Documentation complete with examples

---

## Comparison with Other Languages

| Feature | Aria | Go | Rust (Tokio) | Java (Loom) |
|---------|------|-----|--------------|-------------|
| Model | Hybrid (explicit) | M:N (implicit) | Hybrid (explicit) | M:N (implicit) |
| Task size | 200-500 bytes | ~2KB (stack) | ~200 bytes | ~1KB |
| Scheduler | Work-stealing | Work-stealing | Work-stealing | ForkJoinPool |
| Blocking | Explicit (spawn_blocking) | Transparent | Explicit (spawn_blocking) | Transparent |
| Safety | Appendage Theory | GC | Ownership | GC |
| Zero-cost | ✓ | ✗ (runtime) | ✓ | ✗ (JVM) |

**Aria's Advantages**:
- **Explicit performance boundaries** (no hidden cliffs)
- **GC + manual memory** (hybrid safety)
- **Compile-time thread safety** (no data races)
- **Direct OS thread access** (real-time, affinity)

---

## Open Questions / Future Work

1. **NUMA Awareness**: Pin workers to specific cores for cache locality?
2. **Priority Scheduling**: Support high/low priority tasks?
3. **Work Stealing Variants**: Try randomized victim selection vs round-robin?
4. **Async Drop**: Handle cleanup for async resources (similar to Rust's AsyncDrop RFC)?
5. **Structured Concurrency**: Nursery/scope pattern for task groups (like Trio in Python)?

---

## References

- **Research Document**: `/docs/gemini/responses/research_007_threading_library.txt`
- **Aria Specification**: `/docs/info/aria_specs.txt` (async/await, threading requirements)
- **Related Papers**:
  - Chase-Lev Deque: Dynamic Circular Work-Stealing Deque (2005)
  - Go Scheduler: Understanding the Go Scheduler (Go team blog)
  - Tokio Design: Asynchronous Programming with Tokio
  - Java Loom: Project Loom Technical Deep Dive
