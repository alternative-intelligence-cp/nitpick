# Coroutines

**Category**: Advanced Features → Concurrency  
**Purpose**: Cooperative multitasking with yielding

---

## Overview

Coroutines are **functions that can pause and resume** execution, enabling cooperative concurrency.

---

## Basic Coroutine

```aria
fn* counter() -> i32 {
    i: i32 = 0;
    loop {
        yield i;
        i += 1;
    }
}

fn main() {
    coro = counter();
    
    stdout << coro.next();  // 0
    stdout << coro.next();  // 1
    stdout << coro.next();  // 2
}
```

---

## Generator Syntax

```aria
// Generator function uses fn*
fn* fibonacci() -> i32 {
    a: i32 = 0;
    b: i32 = 1;
    
    loop {
        yield a;
        temp: i32 = a;
        a = b;
        b = temp + b;
    }
}

fn main() {
    fib = fibonacci();
    
    till(9, 1) {
        stdout << fib.next();
    }
    // Output: 0 1 1 2 3 5 8 13 21 34
}
```

---

## Yielding Values

```aria
fn* range(start: i32, end: i32) -> i32 {
    i: i32 = start;
    while i < end {
        yield i;
        i += 1;
    }
}

fn main() {
    values = range(0, 5);
    till(values.length - 1, 1) {
        stdout << values[$];  // 0 1 2 3 4
    }
}
```

---

## Receiving Values

```aria
fn* echo() -> string {
    loop {
        message: string = yield "Ready";
        yield "Received: $message";
    }
}

fn main() {
    coro = echo();
    
    stdout << coro.next();        // "Ready"
    stdout << coro.send("Hello"); // "Received: Hello"
    stdout << coro.next();        // "Ready"
}
```

---

## Async Coroutines

```aria
async fn* async_range(start: i32, end: i32) -> i32 {
    i: i32 = start;
    while i < end {
        await async_sleep(100);  // Async operation
        yield i;
        i += 1;
    }
}

async fn main() {
    values = await async_range(0, 5).collect();
    till(values.length - 1, 1) {
        stdout << values[$];
    }
}
```

---

## Common Patterns

### Lazy Evaluation

```aria
fn* lazy_map<T, R>(items: []T, f: fn(T) -> R) -> R {
    till(items.length - 1, 1) {
        yield f(items[$]);
    }
}

fn expensive(x: i32) -> i32 {
    // Expensive computation
    return x * x;
}

fn main() {
    numbers: []i32 = [1, 2, 3, 4, 5];
    
    // Values computed only when needed
    squared = lazy_map(numbers, expensive);
    
    // Only compute first 3
    till(2, 1) {
        stdout << squared.next();
    }
    // Computed: 1, 4, 9 (not 16, 25 - saved computation!)
}
```

---

### Stream Processing

```aria
fn* read_lines(file: File) -> string {
    while line = file.read_line() {
        yield line;
    }
}

fn* filter_comments(lines: Generator<string>) -> string {
    line_list = lines.collect();
    till(line_list.length - 1, 1) {
        if !line_list[$].starts_with("#") {
            yield line_list[$];
        }
    }
}

fn main() {
    file: File = open("data.txt");
    lines = read_lines(file);
    filtered = filter_comments(lines);
    filtered_list = filtered.collect();
    
    till(filtered_list.length - 1, 1) {
        process(filtered_list[$]);
    }
}
```

---

### Infinite Sequences

```aria
fn* naturals() -> i32 {
    n: i32 = 0;
    loop {
        yield n;
        n += 1;
    }
}

fn* primes() -> i32 {
    yield 2;
    candidates: []i32 = [];
    
    nat_seq = naturals().skip(3).step_by(2).collect();
    till(nat_seq.length - 1, 1) {
        n = nat_seq[$];
        is_prime: bool = true;
        till(candidates.length - 1, 1) {
            p = candidates[$];
            if p * p > n {
                break;
            }
            if n % p == 0 {
                is_prime = false;
                break;
            }
        }
        
        if is_prime {
            candidates.push(n);
            yield n;
        }
    }
}

fn main() {
    prime_gen = primes();
    
    // Get first 10 primes
    till(9, 1) {
        stdout << prime_gen.next();
    }
    // Output: 2 3 5 7 11 13 17 19 23 29
}
```

---

### State Machine

```aria
enum State {
    Start,
    Running,
    Paused,
    Done,
}

fn* state_machine() -> State {
    yield State.Start;
    yield State.Running;
    yield State.Paused;
    yield State.Running;
    yield State.Done;
}

fn main() {
    machine = state_machine();
    
    while state = machine.next() {
        match state {
            State.Start => stdout << "Starting...",
            State.Running => stdout << "Running...",
            State.Paused => stdout << "Paused",
            State.Done => {
                stdout << "Done!";
                break;
            }
        }
    }
}
```

---

### Async Data Producer

```aria
async fn* fetch_pages(base_url: string, num_pages: i32) -> Page {
    till(num_pages - 1, 1) {
        url: string = "$base_url/page/$($)";
        page: Page = await http.get(url)?;
        yield page;
    }
}

async fn main() {
    total_size: i32 = 0;
    pages = await fetch_pages("https://api.example.com", 10).collect();
    
    till(pages.length - 1, 1) {
        total_size += pages[$].size;
        stdout << "Downloaded page ${pages[$].id}";
    }
    
    stdout << "Total: $total_size bytes";
}
```

---

## Coroutine Methods

```aria
fn* example() -> i32 {
    yield 1;
    yield 2;
    yield 3;
}

fn main() {
    coro = example();
    
    // Get next value
    value: ?i32 = coro.next();
    
    // Check if done
    done: bool = coro.is_done();
    
    // Reset to start
    coro.reset();
    
    // Send value to coroutine
    coro.send(42);
}
```

---

## Best Practices

### ✅ DO: Use for Lazy Evaluation

```aria
fn* generate_data() -> Data {
    till(999999, 1) {
        // Generate only when needed
        yield expensive_computation($);
    }
}

fn main() {
    data = generate_data();
    
    // Process only what you need
    till(9, 1) {
        process(data.next());
    }
}
```

### ✅ DO: Use for Stream Processing

```aria
fn* parse_log(file: File) -> LogEntry {
    lines = file.lines();
    till(lines.length - 1, 1) {
        if entry = parse_line(lines[$]) {
            yield entry;
        }
    }
}

fn main() {
    entries = parse_log(open("app.log")).collect();
    till(entries.length - 1, 1) {
        if entries[$].level == Error {
            report_error(entries[$]);
        }
    }
}
```

### ✅ DO: Combine Generators

```aria
fn* map<T, R>(gen: Generator<T>, f: fn(T) -> R) -> R {
    values = gen.collect();
    till(values.length - 1, 1) {
        yield f(values[$]);
    }
}

fn* filter<T>(gen: Generator<T>, pred: fn(T) -> bool) -> T {
    values = gen.collect();
    till(values.length - 1, 1) {
        if pred(values[$]) {
            yield values[$];
        }
    }
}

fn main() {
    numbers = range(0, 100);
    evens = filter(numbers, |x| x % 2 == 0);
    squared = map(evens, |x| x * x);
    squared_five = squared.take(5);
    
    till(squared_five.length - 1, 1) {
        stdout << squared_five[$];  // 0 4 16 36 64
    }
}
```

### ❌ DON'T: Use for CPU-Bound Parallelism

```aria
// ❌ Bad - coroutines are cooperative, not parallel
fn* parallel_compute() -> i32 {
    // This still runs sequentially!
    till(999999, 1) {
        yield expensive($);
    }
}

// ✅ Good - use threads for parallelism
import std.thread.Thread;

fn parallel_compute() -> []i32 {
    threads: []Thread<i32> = [];
    till(7, 1) {
        threads.push(Thread.spawn(|| compute_chunk($)));
    }
    return threads.map(|t| t.join());
}
```

---

## Related

- [async](async.md) - Async functions
- [await](await.md) - Await keyword
- [concurrency](concurrency.md) - Concurrency overview

---

**Remember**: Coroutines provide **cooperative** multitasking with **yield** for lazy evaluation and streams!
