# Closure Capture Semantics

**Category**: Functions → Closures  
**Concept**: How closures capture and store variables from outer scopes  
**Syntax**: `$variable` for mutable capture, plain name for immutable

---

## Capture Modes

Aria provides **explicit** capture semantics - you control how variables are captured.

---

## Immutable Capture (Copy)

When you reference a variable **without $**, it's **captured by value**:

```aria
fn make_printer(prefix: string) -> fn(string) {
    // 'prefix' is copied into the closure
    return |msg: string| {
        stdout << prefix << ": " << msg << "\n";
    };
}

greeting: string = "Hello";
print: fn(string) = make_printer(greeting);

// Change original
greeting = "Goodbye";

print("World");  // "Hello: World" (uses captured copy)
```

**What happens**:
- Closure gets its **own copy** of `prefix`
- Original variable changes don't affect the closure
- Captured value is **immutable** inside the closure

---

## Mutable Capture (Reference)

Use **$** to capture by **reference** and allow **modification**:

```aria
fn make_counter() -> fn() -> i32 {
    count: i32 = 0;
    
    // $count captures by reference
    return || {
        $count = $count + 1;
        return $count;
    };
}

counter: fn() -> i32 = make_counter();
stdout << counter();  // 1
stdout << counter();  // 2
stdout << counter();  // 3
```

**What happens**:
- Closure captures a **reference** to `count`
- Modifications affect the **original** variable
- Must use **$** prefix for both read and write

---

## $ Prefix Rules

### Reading Mutable Captures

```aria
fn make_accumulator() -> fn(i32) {
    total: i32 = 0;
    
    return |value: i32| {
        // Must use $ to read mutable capture
        $total = $total + value;
        stdout << "Total: " << $total << "\n";
    };
}
```

### Writing Mutable Captures

```aria
fn make_toggle() -> fn() -> bool {
    state: bool = false;
    
    return || {
        // Must use $ to write
        $state = !$state;
        return $state;
    };
}
```

### Consistency Rule

```aria
// ✅ Correct: $ for both read and write
$count = $count + 1;

// ❌ Wrong: Inconsistent $ usage
$count = count + 1;  // Error: count not in scope
count = $count + 1;  // Error: can't assign to immutable capture
```

---

## Multiple Captures

### Mixed Immutable and Mutable

```aria
fn make_logger(prefix: string) -> fn(string) {
    count: i32 = 0;
    
    // prefix: immutable (copy)
    // count: mutable (reference)
    return |msg: string| {
        $count = $count + 1;
        stdout << prefix << " #" << $count << ": " << msg << "\n";
    };
}

log: fn(string) = make_logger("INFO");
log("Starting");  // "INFO #1: Starting"
log("Running");   // "INFO #2: Running"
```

### Multiple Mutable Captures

```aria
fn make_stats() -> fn(i32) {
    count: i32 = 0;
    sum: i32 = 0;
    min: i32 = i32::MAX;
    max: i32 = i32::MIN;
    
    return |value: i32| {
        $count = $count + 1;
        $sum = $sum + value;
        
        when value < $min then
            $min = value;
        end
        
        when value > $max then
            $max = value;
        end
        
        avg: f64 = ($sum as f64) / ($count as f64);
        stdout << "Count: " << $count << ", Avg: " << avg;
        stdout << ", Min: " << $min << ", Max: " << $max << "\n";
    };
}

track: fn(i32) = make_stats();
track(10);  // Count: 1, Avg: 10.0, Min: 10, Max: 10
track(5);   // Count: 2, Avg: 7.5, Min: 5, Max: 10
track(15);  // Count: 3, Avg: 10.0, Min: 5, Max: 15
```

---

## Capture Lifetime

### Heap Allocation

Captured variables are **moved to the heap** when necessary:

```aria
fn create_closure() -> fn() -> i32 {
    value: i32 = 42;  // Stack variable
    
    return || value;  // Moved to heap, lives with closure
    
    // Stack frame destroyed, but 'value' lives on!
}

get_value: fn() -> i32 = create_closure();
Result: i32 = get_value();  // 42 - still accessible
```

### Shared Ownership

Multiple closures can share mutable captures:

```aria
fn make_pair() -> (fn(), fn()) {
    shared: i32 = 0;
    
    increment: fn() = || {
        $shared = $shared + 1;
        stdout << "Inc: " << $shared << "\n";
    };
    
    decrement: fn() = || {
        $shared = $shared - 1;
        stdout << "Dec: " << $shared << "\n";
    };
    
    return (increment, decrement);
}

(inc, dec): (fn(), fn()) = make_pair();
inc();  // Inc: 1
inc();  // Inc: 2
dec();  // Dec: 1
inc();  // Inc: 2
```

---

## Capture Scope Rules

### Nested Closures

```aria
fn make_nested() -> fn() -> fn() -> i32 {
    outer: i32 = 10;
    
    return || {
        middle: i32 = 20;
        
        return || {
            // Inner closure can capture from both outer scopes
            return outer + middle;  // 30
        };
    };
}

get_inner: fn() -> fn() -> i32 = make_nested();
inner: fn() -> i32 = get_inner();
Result: i32 = inner();  // 30
```

### Mutable in Nested

```aria
fn make_counter_factory() -> fn() -> fn() -> i32 {
    factory_count: i32 = 0;
    
    return || {
        $factory_count = $factory_count + 1;
        counter_id: i32 = $factory_count;
        count: i32 = 0;
        
        return || {
            $count = $count + 1;
            stdout << "Counter #" << counter_id << ": " << $count << "\n";
            return $count;
        };
    };
}

factory: fn() -> fn() -> i32 = make_counter_factory();
counter1: fn() -> i32 = factory();  // Counter #1
counter2: fn() -> i32 = factory();  // Counter #2

counter1();  // Counter #1: 1
counter1();  // Counter #1: 2
counter2();  // Counter #2: 1
```

---

## Copy vs Move Semantics

### Copyable Types (by value)

```aria
fn capture_primitives() {
    num: i32 = 42;
    flag: bool = true;
    
    closure: fn() = || {
        // num and flag are copied
        stdout << num << ", " << flag << "\n";
    };
    
    // Original still accessible
    stdout << num;  // 42
}
```

### Move Types (moved)

```aria
fn capture_move() {
    data: Vec<i32> = vec![1, 2, 3];
    
    closure: fn() = || {
        // data is moved into closure
        stdout << data.len() << "\n";
    };
    
    // Error: data was moved
    // stdout << data.len();
}
```

### Explicit Reference

```aria
fn capture_reference() {
    large_data: Vec<i32> = vec![1, 2, 3, 4, 5];
    
    closure: fn() = || {
        // Capture reference to avoid moving large data
        $large_data.push(6);
        stdout << $large_data.len() << "\n";
    };
    
    closure();  // 6
    stdout << large_data.len();  // 6 (same data)
}
```

---

## Best Practices

### ✅ DO: Use $ for Mutable State

```aria
// Good: Explicit mutable capture
fn make_counter() -> fn() -> i32 {
    count: i32 = 0;
    return || {
        $count = $count + 1;
        return $count;
    };
}
```

### ✅ DO: Capture Minimal State

```aria
// Good: Only capture what you need
fn make_handler(user: User) -> fn() {
    user_id: i32 = user.id;  // Extract only ID
    return || {
        process(user_id);  // Uses ID, not entire User
    };
}
```

### ✅ DO: Document Captured Variables

```aria
// Good: Clear what's captured
fn create_validator(min: i32, max: i32) -> fn(i32) -> bool {
    // Captures: min, max (by value)
    return |value: i32| -> bool {
        return value >= min and value <= max;
    };
}
```

### ❌ DON'T: Forget $ Prefix

```aria
// Wrong: Inconsistent $ usage
fn broken() -> fn() {
    count: i32 = 0;
    return || {
        count = count + 1;  // Error: can't modify immutable capture
    };
}

// Right: Use $ for mutable
fn works() -> fn() {
    count: i32 = 0;
    return || {
        $count = $count + 1;  // Correct
    };
}
```

### ❌ DON'T: Capture Large Objects Unnecessarily

```aria
// Wrong: Captures entire database connection
fn make_query(db: Database) -> fn(string) -> Result {
    return |sql: string| {
        db.execute(sql)  // Entire DB captured!
    };
}

// Right: Capture only what's needed or use reference
fn make_query(db: $Database) -> fn(string) -> Result {
    return |sql: string| {
        $db.execute(sql)  // Reference to DB
    };
}
```

---

## Advanced Patterns

### Closure State Machine

```aria
enum State {
    Idle,
    Running,
    Paused,
    Stopped
}

fn make_state_machine() -> fn(string) {
    state: State = State::Idle;
    
    return |action: string| {
        $state = match (action, $state) {
            ("start", State::Idle) => State::Running,
            ("pause", State::Running) => State::Paused,
            ("resume", State::Paused) => State::Running,
            ("stop", _) => State::Stopped,
            _ => $state  // No change
        };
        
        stdout << "State: " << $state << "\n";
    };
}

machine: fn(string) = make_state_machine();
machine("start");   // State: Running
machine("pause");   // State: Paused
machine("resume");  // State: Running
machine("stop");    // State: Stopped
```

### Shared Counter

```aria
fn make_shared_counter() -> (fn() -> i32, fn() -> i32, fn() -> i32) {
    count: i32 = 0;
    
    increment: fn() -> i32 = || {
        $count = $count + 1;
        return $count;
    };
    
    decrement: fn() -> i32 = || {
        $count = $count - 1;
        return $count;
    };
    
    get: fn() -> i32 = || {
        return $count;
    };
    
    return (increment, decrement, get);
}

(inc, dec, get): (fn() -> i32, fn() -> i32, fn() -> i32) = make_shared_counter();

inc();  // 1
inc();  // 2
dec();  // 1
stdout << get();  // 1
```

---

## Related Topics

- [Closures](closures.md) - Closure overview
- [Lambda Expressions](lambda.md) - Lambda syntax
- [Higher-Order Functions](higher_order_functions.md) - Functions using closures
- [Function Declaration](function_declaration.md) - Regular functions

---

**Remember**: **$** means **mutable capture** - it's your explicit way of saying "this closure can modify this variable". No $ means immutable copy!
