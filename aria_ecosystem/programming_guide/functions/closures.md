# Closures

**Category**: Functions → Advanced  
**Concept**: Functions that capture variables from their surrounding scope  
**Philosophy**: Lexical scoping with explicit capture semantics

---

## What is a Closure?

A **closure** is a function that **captures** (remembers) variables from the scope where it was created, even after that scope has ended.

---

## Basic Example

```aria
fn make_counter() -> fn() -> i32 {
    count: i32 = 0;
    
    // This lambda captures 'count'
    return || {
        $count = $count + 1;  // Modifies captured count
        return $count;
    };
}

counter: fn() -> i32 = make_counter();
stdout << counter();  // 1
stdout << counter();  // 2
stdout << counter();  // 3
```

**What happened**:
1. `make_counter` creates local variable `count`
2. Lambda captures reference to `count`
3. Even after `make_counter` returns, the lambda still has access to `count`
4. Each call increments the **same** `count`

---

## Capture by Value vs Reference

### Capture by Value (Copy)

```aria
fn make_adder(x: i32) -> fn(i32) -> i32 {
    // x is captured by value (copied)
    return |y: i32| x + y;
}

add_5: fn(i32) -> i32 = make_adder(5);
Result: i32 = add_5(3);  // 8 (5 + 3)

// Original x is not affected
```

### Capture by Reference (with $)

```aria
fn make_accumulator() -> fn(i32) {
    total: i32 = 0;
    
    // total is captured by reference
    return |value: i32| {
        $total = $total + value;  // Modifies original total
        stdout << "Total: " << $total << "\n";
    };
}

accumulate: fn(i32) = make_accumulator();
accumulate(10);  // Total: 10
accumulate(5);   // Total: 15
accumulate(3);   // Total: 18
```

---

## Multiple Captures

```aria
fn make_greeter(greeting: string, name: string) -> fn() -> string {
    // Captures both greeting and name
    return || greeting + ", " + name + "!";
}

say_hello: fn() -> string = make_greeter("Hello", "Alice");
stdout << say_hello();  // "Hello, Alice!"

say_hi: fn() -> string = make_greeter("Hi", "Bob");
stdout << say_hi();  // "Hi, Bob!"
```

---

## Closures in Data Structures

### Array of Closures

```aria
fn create_multipliers() -> []fn(i32) -> i32 {
    multipliers: []fn(i32) -> i32 = [];
    
    till(4, 1) {
        i = $ + 1;
        // Each closure captures its own copy of i
        multipliers.push(|x: i32| x * i);
    }
    
    return multipliers;
}

funcs: []fn(i32) -> i32 = create_multipliers();
stdout << funcs[0](10);  // 10 (10 * 1)
stdout << funcs[2](10);  // 30 (10 * 3)
stdout << funcs[4](10);  // 50 (10 * 5)
```

### Closure in Struct

```aria
struct Button {
    label: string,
    on_click: fn()
}

fn create_button(label: string, counter: $i32) -> Button {
    return Button{
        label: label,
        on_click: || {
            $counter = $counter + 1;
            stdout << label << " clicked " << $counter << " times\n";
        }
    };
}

clicks: i32 = 0;
btn: Button = create_button("Submit", $clicks);

btn.on_click();  // "Submit clicked 1 times"
btn.on_click();  // "Submit clicked 2 times"
stdout << clicks;  // 2
```

---

## Common Patterns

### Event Handlers

```aria
fn setup_ui() {
    user_name: string = "Alice";
    
    button.on_click(|| {
        stdout << "Hello, " << user_name << "!\n";
    });
    
    // user_name is captured, available when button is clicked
}
```

### Configuration Builder

```aria
fn create_validator(min: i32, max: i32) -> fn(i32) -> bool {
    return |value: i32| {
        return value >= min and value <= max;
    };
}

age_validator: fn(i32) -> bool = create_validator(0, 120);
is_valid: bool = age_validator(25);  // true
is_invalid: bool = age_validator(150);  // false
```

### Partial Application

```aria
fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}

fn partial_multiply(a: i32) -> fn(i32) -> i32 {
    return |b: i32| a * b;
}

times_5: fn(i32) -> i32 = partial_multiply(5);
Result: i32 = times_5(3);  // 15
```

---

## Closure Lifetime

```aria
fn create_closure() -> fn() -> i32 {
    value: i32 = 42;
    
    // value is captured and lives as long as the closure
    return || value;
    
    // Even though this function returns,
    // 'value' is kept alive by the closure
}

get_value: fn() -> i32 = create_closure();
Result: i32 = get_value();  // 42 - still accessible!
```

---

## Mutable Captures

### Single Mutable Capture

```aria
fn make_toggle() -> fn() -> bool {
    state: bool = false;
    
    return || {
        $state = !$state;  // Toggle state
        return $state;
    };
}

toggle: fn() -> bool = make_toggle();
stdout << toggle();  // true
stdout << toggle();  // false
stdout << toggle();  // true
```

### Multiple Mutable Captures

```aria
fn make_stats_tracker() -> fn(i32) {
    count: i32 = 0;
    sum: i32 = 0;
    
    return |value: i32| {
        $count = $count + 1;
        $sum = $sum + value;
        
        avg: f64 = ($sum as f64) / ($count as f64);
        stdout << "Count: " << $count << ", Average: " << avg << "\n";
    };
}

track: fn(i32) = make_stats_tracker();
track(10);  // Count: 1, Average: 10.0
track(20);  // Count: 2, Average: 15.0
track(30);  // Count: 3, Average: 20.0
```

---

## Closure vs Regular Function

| Aspect | Regular Function | Closure |
|--------|------------------|---------|
| **Captures variables** | No | Yes |
| **Scope access** | Only parameters | Parameters + outer scope |
| **Memory** | Stateless | Stores captured state |
| **Use case** | Pure logic | Stateful callbacks |

---

## Best Practices

### ✅ DO: Use Closures for Callbacks

```aria
// Good: Closure captures context
fn process_async(callback: fn(Result)) {
    user_id: i32 = 123;
    
    fetch_data(|| {
        // Closure has access to user_id
        stddbg << "Fetching for user " << user_id;
    });
}
```

### ✅ DO: Capture Minimal State

```aria
// Good: Only capture what you need
fn make_printer(prefix: string) -> fn(string) {
    return |msg: string| {
        stdout << prefix << ": " << msg << "\n";
    };
}
```

### ✅ DO: Be Explicit with $

```aria
// Good: Clear which variables are modified
fn make_counter() -> fn() -> i32 {
    count: i32 = 0;
    return || {
        $count = $count + 1;  // Explicit mutable capture
        return $count;
    };
}
```

### ❌ DON'T: Capture Large Objects Unnecessarily

```aria
// Wrong: Captures entire large struct
fn make_handler(data: LargeStruct) -> fn() {
    return || {
        // Uses only one field!
        stdout << data.id << "\n";
    };
}

// Right: Capture only what you need
fn make_handler(data: LargeStruct) -> fn() {
    id: i32 = data.id;
    return || {
        stdout << id << "\n";
    };
}
```

### ❌ DON'T: Create Circular References

```aria
// Wrong: Can cause memory leaks
fn create_cycle() {
    closure1: fn() = || {
        // Captures closure2
    };
    
    closure2: fn() = || {
        // Captures closure1 - circular!
    };
}
```

---

## Real-World Examples

### Debounced Function

```aria
fn debounce(f: fn(), delay_ms: i32) -> fn() {
    last_call: Time? = nil;
    
    return || {
        now: Time = Time::now();
        
        when last_call != nil and (now - last_call) < delay_ms then
            // Too soon, ignore call
            return;
        end
        
        $last_call = now;
        f();  // Call original function
    };
}

// Usage
save_config: fn() = debounce(|| {
    stdout << "Saving configuration...\n";
}, 1000);

// Multiple rapid calls only execute once
save_config();
save_config();  // Ignored (too soon)
save_config();  // Ignored (too soon)
```

### Memoization

```aria
fn memoize<T, U>(f: fn(T) -> U) -> fn(T) -> U {
    cache: Map<T, U> = Map::new();
    
    return |arg: T| -> U {
        when cache.contains_key(arg) then
            return cache.get(arg);
        end
        
        Result: U = f(arg);
        cache.insert(arg, result);
        return result;
    };
}

// Expensive calculation
fibonacci: fn(i32) -> i32 = |n: i32| -> i32 {
    when n <= 1 then return n; end
    return fibonacci(n - 1) + fibonacci(n - 2);
};

// Memoized version
fast_fib: fn(i32) -> i32 = memoize(fibonacci);
Result: i32 = fast_fib(40);  // Calculated only once per n
```

### Rate Limiter

```aria
fn rate_limit(max_calls: i32, window_ms: i32) -> fn() -> bool {
    calls: []Time = [];
    
    return || -> bool {
        now: Time = Time::now();
        
        // Remove old calls outside window
        $calls = calls.filter(|t| (now - t) < window_ms);
        
        when calls.len() >= max_calls then
            return false;  // Rate limit exceeded
        end
        
        $calls.push(now);
        return true;  // Allow call
    };
}

// Max 5 calls per second
can_call: fn() -> bool = rate_limit(5, 1000);

till(9, 1) {
    i: i32 = $ + 1;
    when can_call() then
        stdout << "API call " << i << "\n";
    else
        stdout << "Rate limited!\n";
    end
}
```

---

## Related Topics

- [Closure Capture](closure_capture.md) - Detailed capture semantics
- [Lambda Expressions](lambda.md) - Anonymous function syntax
- [Higher-Order Functions](higher_order_functions.md) - Functions taking functions
- [Function Declaration](function_declaration.md) - Regular functions

---

**Remember**: Closures are **stateful functions** - they remember where they came from. Use them for callbacks, event handlers, and any time you need a function with memory.
