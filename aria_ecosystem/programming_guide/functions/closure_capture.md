# Closure Capture Semantics

**Category**: Functions → Closures  
**Concept**: How closures capture and store variables from outer scopes  
**Syntax**: Value capture (default) or reference capture (via pointers)

---

## What is Capture?

When a function (closure) references variables from an outer scope, it **captures** them. The captured variables become part of the closure's environment.

```aria
func:make_adder = (int32)(int32)(int32:n) {
    // This closure captures 'n' from outer scope
    pass(int32(int32:x) { pass(x + n); });
};
```

---

## Capture Modes

Aria provides two capture modes:

1. **Value capture (copy)** - Default, safe
2. **Reference capture (pointer)** - Explicit, requires care

---

## Value Capture (Copy Semantics)

When you reference a variable without a pointer, it's **captured by value** (copied):

```aria
func:make_printer = (NIL)()(string:prefix) {
    // 'prefix' is COPIED into the closure
    pass(NIL() {
        println(`&{prefix}: message`);
        pass(NIL);
    });
};

string:greeting = "Hello";
(NIL)():print = make_printer(greeting)?;

// Change original
greeting = "Goodbye";

print()?;  // Prints "Hello: message" (uses captured copy)
```

**What happens**:
- Closure gets its **own copy** of `prefix`
- Original variable changes don't affect the closure
- Captured value is **immutable** inside the closure (it's a copy)
- **Safe** - no lifetime issues

---

## Reference Capture (Pointer Semantics)

To modify captured variables or avoid copying large data, use **pointers**:

```aria
func:make_counter = (NIL)() {
    int32:count = 0;
    int32->:count_ref = @count;  // Get pointer to count
    
    // Capture pointer by value (pointer itself is copied, not what it points to)
    pass(NIL() {
        <-count_ref = <-count_ref + 1;  // Dereference and modify
        println(<-count_ref);
        pass(NIL);
    });
};

(NIL)():counter = make_counter()?;
counter()?;  // Prints: 1
counter()?;  // Prints: 2
counter()?;  // Prints: 3
```

**What happens**:
- Closure captures a **pointer** (by value)
- Modifications affect the **original** variable
- Must use `<-` to dereference for both read and write
- **Unsafe** - pointer must outlive closure

---

## Capture Examples

### Immutable Capture

```aria
int32:multiplier = 10;

// Captures 'multiplier' by value
(int32)(int32):scale = int32(int32:x) { pass(x * multiplier); };

int32:result = scale(5)?;  // 50

multiplier = 100;  // Changing original doesn't affect closure
int32:result2 = scale(5)?;  // Still 50 (captured copy)
```

### Mutable Capture (via Pointer)

```aria
int32:total = 0;
int32->:total_ref = @total;

// Capture pointer to accumulate
(NIL)(int32):accumulate = NIL(int32:value) {
    <-total_ref = <-total_ref + value;
    pass(NIL);
};

accumulate(10)?;  // total = 10
accumulate(20)?;  // total = 30
accumulate(15)?;  // total = 45

println(total);  // 45
```

### Multiple Captures

```aria
int32:a = 10;
int32:b = 20;
int32->:a_ref = @a;

// Capture multiple variables (mix of value and reference)
(int32)(int32):complex = int32(int32:x) {
    pass(x * (<-a_ref) + b);  // a by reference, b by value
};

int32:result = complex(5)?;  // 5 * 10 + 20 = 70

a = 100;  // Affects closure (captured by reference)
b = 200;  // Doesn't affect closure (captured by value)

int32:result2 = complex(5)?;  // 5 * 100 + 20 = 520
```

---

## Capture Lifetime Safety

**Critical Rule**: Captured pointers must **outlive** the closure.

### SAFE: Pointer outlives closure

```aria
func:process_with_callback = NIL((NIL)(int32):callback) {
    int32:value = 42;
    callback(value)?;  // OK - value still alive
    pass(NIL);
};
```

### UNSAFE: Dangling pointer (closure outlives pointee)

```aria
// ❌ DANGEROUS: Don't do this!
func:make_broken_counter = (NIL)() {
    int32:local_count = 0;
    int32->:ref = @local_count;
    
    pass(NIL() {
        <-ref = <-ref + 1;  // ❌ DANGLING POINTER!
        pass(NIL);
    });
};  // local_count destroyed here, but closure still has pointer!

(NIL)():counter = make_broken_counter()?;
counter()?;  // ❌ UNDEFINED BEHAVIOR - pointer is invalid
```

**Fix**: Use value capture or heap allocation:

```aria
// ✅ SAFE: Value capture
func:make_safe_counter = (NIL)() {
    int32:count = 0;
    
    pass(NIL() {
        // Can't modify 'count' but that's intentional - it's a copy
        println(count);
        pass(NIL);
    });
};

// ✅ SAFE: Heap allocation (advanced)
func:make_heap_counter = (NIL)() {
    wild int32->:count = aria.alloc<int32>(1);
    <-count = 0;
    
    pass(NIL() {
        <-count = <-count + 1;
        println(<-count);
        pass(NIL);
    });
    // Note: This leaks memory! Need proper cleanup strategy
};
```

---

## Capture Best Practices

### ✅ DO: Prefer Value Capture

```aria
// Good: Safe, simple, no lifetime issues
int32:factor = 2;
(int32)(int32):double = int32(int32:x) { pass(x * factor); };
```

### ✅ DO: Use Pointers Only When Necessary

```aria
// Good: Need mutability, pointer lifetime is clear
int32:accumulator = 0;
int32->:acc_ref = @accumulator;

process_items(items, NIL(int32:value) {
    <-acc_ref = <-acc_ref + value;
    pass(NIL);
})?;

println(accumulator);  // Final sum
```

### ✅ DO: Document Capture Dependencies

```aria
// Good: Clear what's captured and why
func:make_validator = (bool)(Data)(int32:min, int32:max) {
    // Captures 'min' and 'max' by value for range checking
    pass(bool(Data:d) {
        pass(d.size() >= min && d.size() <= max);
    });
};
```

### ❌ DON'T: Return Closures with Dangling Pointers

```aria
// Wrong: Pointer outlives value
func:broken = (NIL)() {
    int32:local = 42;
    int32->:ptr = @local;
    pass(NIL() { println(<-ptr); pass(NIL); });  // ❌ DANGER
};

// Right: Return closure with value capture
func:safe = (NIL)() {
    int32:local = 42;
    pass(NIL() { println(local); pass(NIL); });  // ✅ SAFE
};
```

### ❌ DON'T: Capture Large Structs by Value Unnecessarily

```aria
// Wrong: Copying huge struct every time
HugeStruct:big_data = load_data()?;
process_items(items, NIL(Item:item) {
    // Closure captures entire 'big_data' by value (expensive copy!)
    transform(item, big_data)?;
    pass(NIL);
})?;

// Right: Use pointer for large data
HugeStruct:big_data = load_data()?;
HugeStruct->:data_ref = @big_data;
process_items(items, NIL(Item:item) {
    // Closure captures pointer (8 bytes), not entire struct
    transform(item, <-data_ref)?;
    pass(NIL);
})?;
```

---

## Capture Performance

### Value Capture (Copy)

**Pros**:
- Safe (no lifetime issues)
- Simple (no pointer dereferencing)
- Fast for small types (int, bool, pointers)

**Cons**:
- Expensive for large types (copy overhead)
- Can't modify original variable

### Reference Capture (Pointer)

**Pros**:
- No copy overhead (just pointer)
- Can modify original variable
- Good for large data

**Cons**:
- Unsafe (lifetime management required)
- Extra dereference on access
- More complex reasoning

---

## Real-World Examples

### Accumulator Pattern

```aria
func:sum_array = int32(int32[]:arr, int64:len) {
    int32:sum = 0;
    int32->:sum_ref = @sum;
    
    // Closure captures sum_ref to accumulate
    for_each(arr, len, NIL(int32:value) {
        <-sum_ref = <-sum_ref + value;
        pass(NIL);
    })?;
    
    pass(sum);
};
```

### Configuration with Closures

```aria
struct:ServerConfig {
    int32:port;
    (Response)(Request):handler;
};

func:create_server = ServerConfig(int32:port, Database->:db) {
    // Closure captures 'db' pointer for request handling
    (Response)(Request):handler = Response(Request:req) {
        // Access database through captured pointer
        Data:data = db->query(req.params)?;
        pass(Response.from_data(data)?);
    };
    
    ServerConfig:config = {
        port: port,
        handler: handler
    };
    
    pass(config);
};
```

### Event System with Closures

```aria
struct:EventBus {
    ((NIL)(Event))[][]:listeners;  // Array of listener arrays by event type
};

func:addEventListener = NIL(EventBus->:bus, int32:event_id, (NIL)(Event):callback) {
    // Store callback (which may capture variables)
    bus->listeners[event_id].push(callback)?;
    pass(NIL);
};

// Usage
EventBus:events = EventBus.new()?;
EventBus->:events_ref = @events;

int32:click_count = 0;
int32->:count_ref = @click_count;

addEventListener(events_ref, CLICK_EVENT, NIL(Event:e) {
    <-count_ref = <-count_ref + 1;
    println(`Total clicks: &{<-count_ref}`);
    pass(NIL);
})?;
```

---

## Comparison with Other Languages

### JavaScript (always reference capture)

```javascript
let counter = 0;
const increment = () => counter++;  // Captures 'counter' by reference
```

### Rust (explicit move/borrow)

```rust
let multiplier = 10;
let scale = |x| x * multiplier;  // Captures by reference
let scale_owned = move |x| x * multiplier;  // Moves ownership
```

### Aria (explicit value vs pointer)

```aria
int32:multiplier = 10;

// Value capture (copy)
(int32)(int32):scale1 = int32(int32:x) { pass(x * multiplier); };

// Reference capture (pointer)
int32->:mult_ref = @multiplier;
(int32)(int32):scale2 = int32(int32:x) { pass(x * (<-mult_ref)); };
```

**Aria's approach**: Explicit and visual - you see `@` and `<-` which tells you exactly what's happening.

---

## Related Topics

- [Lambda Expressions](lambda.md) - Anonymous function syntax
- [Anonymous Functions](anonymous_functions.md) - General concept
- [Pointers](../memory/pointers.md) - Pointer syntax and semantics
- [Memory Safety](../memory/safety.md) - Lifetime and safety rules

---

**Remember**: Prefer value capture (safe, simple). Use pointer capture only when you need mutability or have large data. Always ensure captured pointers outlive the closure.
