# Lambda Expressions (Anonymous Functions)

**Category**: Functions → Anonymous Functions  
**Syntax**: `returnType(paramType:param) { body }`  
**Philosophy**: Functions are first-class values - no special lambda syntax needed

---

## What is a Lambda?

A **lambda** is an anonymous function - a function **without a name** that can be passed as a value, stored in variables, or used inline.

**Key Insight**: In Aria, there is **no special lambda syntax**. Lambdas are just regular functions without a binding. No `=>`, no `|params|`, no magic - just functions.

If you understand functions and pointers, you already understand lambdas.

---

## Basic Syntax

```aria
// Anonymous function (lambda)
int32(int32:a, int32:b) { pass(a + b); }

// Assign to variable (function pointer)
(int32)(int32, int32):add = int32(int32:a, int32:b) { pass(a + b); };

// Use it
int32:result = add(5, 3)?;  // 8 (unwrap Result)
```

---

## Function Pointer Types

The type of a function pointer is: `(returnType)(paramTypes)`

```aria
// Function pointer type declarations
(int32)(int32):unary_op;              // Takes int32, returns int32
(int32)(int32, int32):binary_op;      // Takes two int32, returns int32
(NIL)():callback;                     // Takes nothing, returns NIL
(string)(obj):formatter;              // Takes obj, returns string
```

**Note**: All Aria functions return `Result<T, E>`, so these actually return `Result<int32, ERR>`, etc. The `?` operator unwraps the Result.

---

## Immediate Execution

Add `()` after the function body to execute immediately:

```aria
// Define and execute immediately
int32:result = int32(int32:x) { pass(x * x); }(5)?;  // 25

// Without immediate execution (stored for later)
(int32)(int32):square = int32(int32:x) { pass(x * x); };
int32:result = square(5)?;  // 25
```

---

## Lambda as Function Parameter

```aria
// Higher-order function
func:apply_twice = int32((int32)(int32):f, int32:x) {
    int32:temp = f(x)?;
    pass(f(temp)?);
};

// Pass lambda inline
int32:result = apply_twice(
    int32(int32:n) { pass(n * 2); },
    5
)?;
// First: 5 * 2 = 10
// Second: 10 * 2 = 20
// result = 20
```

---

## Common Patterns

### Array Operations (map, filter, reduce)

```aria
int32[]:numbers = [1, 2, 3, 4, 5];

// Map: transform each element
int32[]:doubled = map(numbers, 5, int32(int32:x) { pass(x * 2); })?;
// [2, 4, 6, 8, 10]

// Filter: keep elements matching predicate
int32[]:evens = filter(numbers, 5, bool(int32:x) { pass(x % 2 == 0); })?;
// [2, 4]

// Reduce: combine elements
int32:sum = reduce(numbers, 5, 0, int32(int32:acc, int32:x) { pass(acc + x); })?;
// 15
```

### Inline Event Handlers

```aria
struct:Button {
    string:label;
    (NIL)():onClick;  // Function pointer for callback
};

func:setup_button = NIL() {
    Button:btn = {
        label: "Click Me",
        onClick: NIL() {
            println("Button clicked!");
            pass(NIL);
        }
    };
    
    // Trigger callback later
    btn.onClick();
    pass(NIL);
};
```

### Sorting with Custom Comparator

```aria
User[]:users = get_users()?;

// Sort by age (using spaceship operator)
sort_by(users, int32(User:a, User:b) { pass(a.age <=> b.age); });

// Sort by name length
sort_by(users, int32(User:a, User:b) { pass(a.name.len() <=> b.name.len()); });
```

---

## Closures (Capturing Variables)

Lambdas can **capture** variables from their surrounding scope:

### Capture by Value (Copy Semantics)

When you reference a variable in a closure, it's **copied by value**:

```aria
func:make_adder = (int32)(int32)(int32:n) {
    // Returns a closure that captures 'n' by value
    pass(int32(int32:x) { pass(x + n); });
};

(int32)(int32):add5 = make_adder(5)?;
int32:result = add5(10)?;  // 15

(int32)(int32):add100 = make_adder(100)?;
int32:result2 = add100(10)?;  // 110
```

**What happens**: The closure gets its **own copy** of `n`. Changes to the original don't affect the closure.

```aria
int32:multiplier = 10;
(int32)(int32):scale = int32(int32:x) { pass(x * multiplier); };

int32:result = scale(5)?;  // 50

multiplier = 100;  // Changing original doesn't affect closure
int32:result2 = scale(5)?;  // Still 50 (captured copy)
```

### Capture by Reference (Pointer Semantics)

To modify captured variables, use **pointers**:

```aria
int32:counter = 0;
int32->:counter_ref = @counter;  // Get pointer to counter

(NIL)():increment = NIL() {
    <-counter_ref = <-counter_ref + 1;  // Dereference and modify
    pass(NIL);
};

increment();  // counter = 1
increment();  // counter = 2
increment();  // counter = 3
```

**⚠️ Safety**: Captured pointers must outlive the closure. Dangling pointers cause undefined behavior.

```aria
// UNSAFE: Pointer outlives value
func:make_broken_counter = (NIL)() {
    int32:local_count = 0;
    int32->:ref = @local_count;
    
    pass(NIL() {
        <-ref = <-ref + 1;  // ❌ DANGLING POINTER when local_count destroyed!
        pass(NIL);
    });
};  // local_count destroyed here, but closure still has pointer!
```

---

## Type Annotations

Function pointer types must be explicit when declaring variables:

```aria
// Explicit function pointer type
(int32)(string, int32):processor = int32(string:s, int32:n) {
    pass(is s.len() > n : 1 : 0);
};

// Multiple parameters
(bool)(int32, int32, int32):in_range = bool(int32:val, int32:min, int32:max) {
    pass(val >= min && val <= max);
};
```

---

## Multi-Line Lambdas

```aria
(Result<Data, ERR>)(Data):complex_operation = Result<Data, ERR>(Data:data) {
    stddbg.write(`Processing data: &{data.id}`);
    
    if data.size() > MAX_SIZE then
        stderr.write("ERROR: Data too large\n");
        fail("Data too large");
    end
    
    Data:processed = transform(data)?;
    Data:validated = validate(processed)?;
    
    pass(validated);
};
```

---

## Lambdas in Data Structures

```aria
// Array of function pointers
(int32)(int32)[]:operations = [
    int32(int32:x) { pass(x + 1); },
    int32(int32:x) { pass(x * 2); },
    int32(int32:x) { pass(x * x); }
];

// Apply all operations in sequence
int32:value = 5;
till (i < operations.len()) {
    value = operations[$](value)?;
}
// 5 → 6 → 12 → 144
```

---

## Immediate Invocation for Initialization

```aria
// Define and call lambda immediately for complex initialization
Config:config = Config() {
    Config:c = Config.new()?;
    c.timeout = 30;
    c.retries = 3;
    pass(c);
}()?;

// Simpler example
int32:squared = int32(int32:x) { pass(x * x); }(5)?;  // 25
```

---

## Best Practices

### ✅ DO: Use for Short, Inline Logic

```aria
// Good: Simple transformation
int32[]:doubled = map(numbers, len, int32(int32:x) { pass(x * 2); })?;
```

### ✅ DO: Be Explicit with Types

```aria
// Good: Clear parameter and return types
sort_by(users, int32(User:a, User:b) { 
    pass(a.priority <=> b.priority); 
})?;
```

### ✅ DO: Capture by Value When Possible

```aria
// Good: Safe value capture (copy)
int32:multiplier = 10;
(int32)(int32):scale = int32(int32:x) { pass(x * multiplier); };
```

### ❌ DON'T: Make Lambdas Too Complex

```aria
// Wrong: Lambda is too long and complex
Result<Data[], ERR>:result = map(items, len, Data(Item:item) {
    // 50 lines of complex logic...
    // This should be a named function!
})?;

// Right: Extract to named function
func:process_item = Data(Item:item) {
    // Complex logic here with clear name
    pass(transformed_data);
};

Result<Data[], ERR>:result = map(items, len, process_item)?;
```

### ❌ DON'T: Capture Dangling Pointers

```aria
// Wrong: Captured pointer outlives value
func:broken = (NIL)() {
    int32:local = 42;
    int32->:ptr = @local;
    pass(NIL() {
        println(<-ptr);  // ❌ DANGLING POINTER!
        pass(NIL);
    });
};  // local destroyed, but closure has pointer!

// Right: Capture by value
func:safe = (NIL)() {
    int32:local = 42;
    pass(NIL() {
        println(local);  // ✅ Safe copy
        pass(NIL);
    });
};
```

---

## Lambda vs Named Function

**Use Lambda (anonymous function) when**:
- Function is used once inline
- Logic is short and simple (< 10 lines)
- Passed as callback/handler to another function
- Complex initialization needs scoped logic

**Use Named Function when**:
- Function is reused multiple times
- Logic is complex or long
- Function needs a descriptive, self-documenting name
- Testing/debugging requires clear function identity

---

## Real-World Examples

### Event Processing

```aria
struct:App {
    (NIL)():on_start;
    (NIL)(Error):on_error;
    (NIL)():on_shutdown;
};

func:setup_handlers = NIL(App->:app) {
    app->on_start = NIL() {
        stddbg.write("Application started\n");
        load_config()?;
        pass(NIL);
    };
    
    app->on_error = NIL(Error:err) {
        stderr.write(`ERROR: &{err.message}\n`);
        log_error(err)?;
        pass(NIL);
    };
    
    app->on_shutdown = NIL() {
        stddbg.write("Shutting down...\n");
        cleanup_resources()?;
        pass(NIL);
    };
    
    pass(NIL);
};
```

### Data Pipeline

```aria
func:process_logs = ErrorEntry[](LogEntry[]:logs, int64:count) {
    // Filter errors
    LogEntry[]:errors = filter(logs, count, bool(LogEntry:log) {
        pass(log.level == "ERROR");
    })?;
    
    // Transform to ErrorEntry
    ErrorEntry[]:entries = map(errors, errors.len(), ErrorEntry(LogEntry:log) {
        ErrorEntry:entry = {
            timestamp: log.timestamp,
            message: log.message,
            severity: calculate_severity(log)?
        };
        pass(entry);
    })?;
    
    // Filter by severity
    ErrorEntry[]:critical = filter(entries, entries.len(), bool(ErrorEntry:e) {
        pass(e.severity > 5);
    })?;
    
    pass(critical);
};
```

### Async Completion Handlers

```aria
func:fetch_data = NIL(string:url, (NIL)(string):onComplete) {
    // Async HTTP request
    string:data = http_get(url) ? "";
    
    // Call completion handler with result
    onComplete(data)?;
    pass(NIL);
};

// Usage
fetch_data("https://example.com", NIL(string:response) {
    println(response);
    pass(NIL);
})?;
```

---

## No Magic: Just Functions

**Important**: Aria has no special lambda operator (`=>`, `|params|`, etc.). The "magic" of lambdas demystifies once you understand:

1. **Functions are first-class values** - you can pass them around like any data
2. **Function pointers** - just pointers to code (like C function pointers, but type-safe)
3. **Closures capture by value** - they get a copy of the environment (or pointer if you need mutability)

Drop down to assembly for a few minutes and see what a function actually is: just a pointer to an address in code memory. That's it. No magic.

---

## Related Topics

- [Anonymous Functions](anonymous_functions.md) - General concept and patterns
- [Closure Capture](closure_capture.md) - Detailed capture semantics
- [Higher-Order Functions](higher_order_functions.md) - Functions taking functions
- [Function Declaration](function_declaration.md) - Named functions

---

**Remember**: Lambdas are just **anonymous functions**. Use them for concise, inline logic. For anything complex, use a named function with a descriptive name instead.
