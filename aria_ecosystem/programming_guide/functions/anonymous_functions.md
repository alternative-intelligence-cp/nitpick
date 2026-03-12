# Anonymous Functions

**Category**: Functions → Lambda  
**Concept**: Functions without names  
**Also Known As**: Lambdas, closures (when capturing variables)

---

## What Are Anonymous Functions?

**Anonymous functions** are functions **without names** - they're defined inline where you need them.

In Aria, there is **no special syntax** for anonymous functions. They use the same syntax as named functions, just without binding to a name:

```aria
// Named function
func:double = int32(int32:x) { pass(x * 2); };

// Anonymous function (same syntax, no binding)
int32(int32:x) { pass(x * 2); }
```

**Key insight**: "Anonymous" just means "not bound to a name." The function itself is identical.

---

## Basic Anonymous Function

```aria
// Anonymous function standing alone
int32(int32:x) { pass(x * 2); }

// Assign to variable (now it has a name through the variable)
(int32)(int32):double = int32(int32:x) { pass(x * 2); };

// Use it
int32:result = double(5)?;  // 10
```

---

## Using Anonymous Functions

### Assigned to Variable

```aria
// Store in variable for later use
(int32)(int32):square = int32(int32:x) { pass(x * x); };

int32:result = square(5)?;  // 25
```

### Passed as Argument

```aria
// Pass to higher-order function
int32[]:numbers = [1, 2, 3, 4, 5];

// Pass anonymous function to map
int32[]:doubled = map(numbers, 5, int32(int32:x) { pass(x * 2); })?;
// [2, 4, 6, 8, 10]
```

### Returned from Function

```aria
// Return anonymous function (closure)
func:make_multiplier = (int32)(int32)(int32:factor) {
    // Return anonymous function that captures 'factor'
    pass(int32(int32:x) { pass(x * factor); });
};

(int32)(int32):times_5 = make_multiplier(5)?;
int32:result = times_5(3)?;  // 15
```

---

## Anonymous vs Named Functions

### Named Function

```aria
func:greet = NIL(string:name) {
    stdout.write(`Hello, &{name}\n`);
    pass(NIL);
};

// Can be called by name anywhere in scope
greet("Alice")?;
```

### Anonymous Function

```aria
// Stored in variable
(NIL)(string):greet_fn = NIL(string:name) {
    stdout.write(`Hello, &{name}\n`);
    pass(NIL);
};

// Must be called through variable
greet_fn("Alice")?;

// Or passed/used inline without storing
process_name("Alice", NIL(string:name) {
    stdout.write(`Hello, &{name}\n`);
    pass(NIL);
})?;
```

**Difference**: Only the binding. The function itself is identical.

---

## Common Use Cases

### Event Handlers

```aria
struct:Button {
    string:label;
    (NIL)():onClick;
};

Button:btn = {
    label: "Click Me",
    onClick: NIL() {
        println("Button clicked!");
        pass(NIL);
    }
};

// Trigger event
btn.onClick()?;
```

### Array Operations

```aria
int32[]:numbers = [1, 2, 3, 4, 5];

// Filter with anonymous function
int32[]:evens = filter(numbers, 5, bool(int32:x) { 
    pass(x % 2 == 0); 
})?;

// Map with anonymous function
int32[]:doubled = map(numbers, 5, int32(int32:x) { 
    pass(x * 2); 
})?;

// Reduce with anonymous function
int32:sum = reduce(numbers, 5, 0, int32(int32:acc, int32:x) { 
    pass(acc + x); 
})?;
```

### Sorting with Custom Comparator

```aria
User[]:users = get_users()?;

// Sort by age using anonymous comparator
sort_by(users, int32(User:a, User:b) { 
    pass(a.age <=> b.age); 
})?;
```

### Async Callbacks

```aria
func:fetch_data = NIL(
    string:url,
    (NIL)(Data):on_success,
    (NIL)(Error):on_error
) {
    Result<Data, Error>:result = http_get(url);
    
    if result.is_error then
        on_error(result.err)?;
    else
        on_success(raw(result))?;
    end
    
    pass(NIL);
};

// Pass anonymous callbacks
fetch_data(
    "https://example.com",
    NIL(Data:data) {
        println(`Success: &{data}`);
        pass(NIL);
    },
    NIL(Error:error) {
        stderr.write(`Error: &{error}\n`);
        pass(NIL);
    }
)?;
```

---

## Closure Behavior

Anonymous functions can **capture** variables from surrounding scope (making them closures):

```aria
int32:multiplier = 10;

// Anonymous function captures 'multiplier' by value
(int32)(int32):scale = int32(int32:x) { pass(x * multiplier); };

int32:result = scale(5)?;  // 50

multiplier = 100;  // Changing original doesn't affect closure
int32:result2 = scale(5)?;  // Still 50 (captured copy)
```

See [Closure Capture](closure_capture.md) for detailed capture semantics.

---

## Immediately Invoked Function (IIFE)

```aria
// Define and call immediately
int32:result = int32(int32:x) { pass(x * x); }(5)?;  // 25

// Useful for complex initialization with local scope
Config:config = Config() {
    Config:c = Config.new()?;
    c.timeout = 30;
    c.retries = 3;
    pass(c);
}()?;
```

---

## Multi-Statement Anonymous Functions

```aria
(NIL)(Data):process = NIL(Data:data) {
    stddbg.write(`Processing: &{data.id}\n`);
    
    if data.size() > MAX_SIZE then
        stderr.write("Data too large\n");
        fail("Data too large");
    end
    
    Data:validated = validate(data)?;
    Data:transformed = transform(validated)?;
    
    pass(NIL);
};
```

---

## Best Practices

### ✅ DO: Use Anonymous Functions for Short, One-Off Logic

```aria
// Good: Simple inline transformation
int32[]:doubled = map(numbers, len, int32(int32:x) { pass(x * 2); })?;
```

### ✅ DO: Use Named Functions for Reused Logic

```aria
// Good: Reusable validation
func:is_valid = bool(Data:d) {
    pass(d.size() > 0 && d.size() < MAX_SIZE);
};

// Use named function multiple times
bool:valid1 = is_valid(data1)?;
bool:valid2 = is_valid(data2)?;
```

### ❌ DON'T: Make Anonymous Functions Too Long

```aria
// Wrong: Too complex for anonymous function
process(data, Data(Item:item) {
    // 50 lines of complex logic
    // Should be a named function!
})?;

// Right: Named function with clear purpose
func:transform_item = Data(Item:item) {
    // Complex logic with descriptive name
    pass(result);
};

process(data, transform_item)?;
```

---

## Why "Anonymous"?

The term "anonymous" comes from lambda calculus, but in practice it just means:

- **Not bound to a name** at definition site
- Can be **passed directly** as an argument
- Can be **stored** in variables/arrays/structs
- **Same behavior** as named functions

In Aria, the distinction is minimal - it's just whether you bind the function to a name or not.

---

## Related Topics

- [Lambda Expressions](lambda.md) - Full lambda documentation
- [Closure Capture](closure_capture.md) - How closures capture variables
- [Higher-Order Functions](higher_order_functions.md) - Functions taking functions
- [Function Pointers](function_pointers.md) - Function pointer types

---

**Remember**: Anonymous functions are regular functions that happen not to have a name. Use them for short, inline logic. For anything complex or reused, give it a name.
