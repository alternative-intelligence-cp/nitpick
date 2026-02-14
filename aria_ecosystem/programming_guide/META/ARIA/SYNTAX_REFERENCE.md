# Aria Syntax Reference - Canonical Examples

**Created**: February 14, 2026 (Session 27)  
**Purpose**: Single source of truth for correct Aria syntax patterns  
**Status**: Reference for batch syntax corrections (Sessions 28-30)

---

## Error Handling

### ✅ Correct: `pass()` and `fail()`

```aria
// Success
func:read_config = Result<Config, string>(string:path) {
    if !file_exists(path) then
        fail("Config file not found");
    end
    
    Config:config = parse_config(path);
    pass(config);
};

// Failure  
func:validate_user = Result<User, string>(string:id) {
    if id.length == 0 then
        fail("Invalid user ID");
    end
    
    User:user = fetch_user(id);
    pass(user);
};
```

### ❌ Incorrect: `return Ok/Err/Error`

```aria
// WRONG - don't use these patterns:
return Ok(value);        // ❌
return Err(error);       // ❌
return Error(message);   // ❌
Ok(value)               // ❌
Err(error)              // ❌
```

---

## Result Type

### ✅ Correct: `Result<T, E>` with explicit error type

```aria
// Always specify both success and error types
func:divide = Result<float64, string>(float64:a, float64:b) {
    if b == 0.0 then
        fail("Division by zero");
    end
    pass(a / b);
};

func:parse_int = Result<int64, ParseError>(string:s) {
    // Implementation
};

// Checking Results
Result<int64, string>:result = parse_number(input);

if result.err != NULL then
    print("Error: " + result.err);
else
    print("Value: " + result.value);
end
```

### ❌ Incorrect: `Result<T>` or omitted error type

```aria
// WRONG - always need error type:
func:foo = Result<int64>(...) { }     // ❌
Result<string>:result = bar();         // ❌
```

---

## Function Syntax

### ✅ Correct: `func:name = ReturnType(ParamType:param)`

```aria
// Basic function
func:add = int64(int64:a, int64:b) {
    pass(a + b);
};

// Multiple parameters
func:calculate = float64(float64:x, float64:y, string:op) {
    if op == "+" then pass(x + y); end
    if op == "-" then pass(x - y); end
    fail("Unknown operator");
};

// Complex return types
func:fetch_user = Result<User, string>(string:id) {
    // Implementation
};

// Generic functions
func:identity = T(T:value) {
    pass(value);
};
```

### ❌ Incorrect: Rust/C-style function syntax

```aria
// WRONG - don't use these patterns:
fn add(a: int64, b: int64) -> int64 { }           // ❌
function add(int64 a, int64 b): int64 { }         // ❌
func add(a: int64, b: int64) -> int64 { }         // ❌
```

---

## Lambda Functions

### ✅ Correct: `func(Type:param) { pass(expr); }`

```aria
// Single parameter
int64[]:evens = filter(numbers, func(int64:n) { pass(n % 2 == 0); });

// Multiple parameters
int64:sum = reduce(numbers, 0, func(int64:acc, int64:n) { pass(acc + n); });

// Multi-line lambda
int64[]:filtered = filter(numbers, func(int64:n) {
    if n < 0 then pass(false); end
    if n % 2 != 0 then pass(false); end
    pass(true);
});

// Complex type
Product[]:available = filter(products, func(Product:p) {
    pass(p.stock > 0 && p.price < 100.0);
});

// Returning a lambda from a function
func:createFilter = func:(bool)(int64)(int64:threshold) {
    pass(func(int64:n) { pass(n > threshold); });
};
```

### ❌ Incorrect: Arrow operator `=>`

```aria
// WRONG - => does not exist in Aria:
filter(numbers, n => n % 2 == 0)                    // ❌
map(numbers, x => x * 2)                            // ❌
reduce(numbers, 0, (acc, n) => acc + n)             // ❌
numbers.filter(n => n > 0)                          // ❌
createFilter(10, n => n > threshold)                // ❌
```

---

## Loops

### ✅ Correct: `till` with `$` index

```aria
// Basic till loop (0 to end, step 1)
till(10, 1) {
    print($);  // $ is the loop index
}

// Iterate array
string[]:names = ["Alice", "Bob", "Charlie"];
till(names.length - 1, 1) {
    print(names[$]);
}

// Step by 2
till(100, 2) {
    print($);  // 0, 2, 4, ..., 100
}

// While-style loop
int64:i = 0;
till(i < 100, 0) {
    print(i);
    i++;
}

// Loopback for reverse iteration
loopback(10, 0, 1) {
    print($);  // 10, 9, 8, ..., 0
}
```

### ❌ Incorrect: C-style for loops

```aria
// WRONG - don't use these patterns:
for (i := 0; i < 10; i++) { }                     // ❌
for (int i = 0; i < 10; i++) { }                  // ❌
for i in range(10) { }                            // ❌
for (auto& item : collection) { }                 // ❌
```

---

## Type Annotations

### ✅ Correct: `Type:name`

```aria
// Variable declarations
int64:count = 0;
string:name = "Alice";
float64:average = 3.14;

// Function parameters
func:process = void(int64:id, string:name, bool:active) {
    // Implementation
};

// Struct fields
%STRUCT User {
    string:name,
    int64:age,
    bool:active
}

// Array types
int64[]:numbers = [1, 2, 3];
string[]:words = ["hello", "world"];
```

### ❌ Incorrect: Colon on the right

```aria
// WRONG - don't use these patterns:
count: int64 = 0;                                 // ❌
name: string = "Alice";                           // ❌
func process(id: int64, name: string) { }         // ❌
```

---

## NULL vs NIL

### ✅ Correct: `NULL` (all capitals)

```aria
// NULL is the proper null value in Aria
string | NULL:maybe = NULL;

if value != NULL then
    print(value);
end

// Nullable types
int64 | NULL:optional = get_value();
```

### ❌ Incorrect: `NIL`, `nil`, `null`

```aria
// WRONG - don't use these:
string?:maybe = NIL;                              // ❌
if value != nil then { }                          // ❌
x = null;                                         // ❌
```

---

## Match Statements

### ✅ Correct: Use `if/then/else` for Result checking

```aria
Result<int64, string>:result = parse_number(input);

if result.err != NULL then
    print("Error: " + result.err);
else
    print("Value: " + result.value);
end

// Pattern matching for specific cases
if result.err != NULL then
    if result.err == "Division by zero" then
        print("Cannot divide by zero");
    else
        print("Other error: " + result.err);
    end
else
    int64:value = result.value;
    print(value);
end
```

### ❌ Incorrect: Rust-style match on Result

```aria
// WRONG - don't use Rust match patterns:
match result {                                    // ❌
    Ok(value) => { },
    Err(error) => { }
}
```

---

## Async Functions

### ✅ Correct: `async:func_name`

```aria
async:fetch_data = Result<Data, string>(string:url) {
    // Async implementation
    pass(data);
};

async:process_all = void() {
    Result<Data, string>:result = await fetch_data("https://api.example.com");
    
    if result.err != NULL then
        print("Failed to fetch data");
    else
        process(result.value);
    end
};
```

### ❌ Incorrect: Other async patterns

```aria
// WRONG - don't use these:
async fn fetch_data() -> Result<Data> { }         // ❌
async function fetch_data(): Promise<Data> { }    // ❌
```

---

## Common Patterns

### Pipeline Operator

```aria
// Using |> for left-to-right flow
int64[]:result = data
    |> filter(func(int64:n) { pass(n > 0); })
    |> transform(func(int64:n) { pass(n * 2); })
    |> reduce(0, func(int64:acc, int64:n) { pass(acc + n); });
```

### Named Predicates

```aria
// Reusable predicate functions
func:isPositive = bool(int64:n) {
    pass(n > 0);
};

func:isEven = bool(int64:n) {
    pass(n % 2 == 0);
};

int64[]:positive = filter(numbers, isPositive);
int64[]:evens = filter(numbers, isEven);
```

### Error Propagation

```aria
func:complex_operation = Result<Data, string>(string:input) {
    Result<int64, string>:num = parse_number(input);
    if num.err != NULL then fail(num.err); end
    
    Result<Data, string>:data = fetch_data(num.value);
    if data.err != NULL then fail(data.err); end
    
    pass(data.value);
};
```

---

## Summary of Key Differences from Other Languages

| Concept | ❌ Other Languages | ✅ Aria |
|---------|-------------------|---------|
| Success | `return Ok(x)`, `Ok(x)` | `pass(x)` |
| Failure | `return Err(e)`, `Err(e)` | `fail(e)` |
| Result | `Result<T>` | `Result<T, E>` |
| Lambda | `n => n * 2` | `func(T:n) { pass(n * 2); }` |
| Function | `fn foo(x: T) -> R` | `func:foo = R(T:x)` |
| Type annotation | `x: int64` | `int64:x` |
| NULL | `nil`, `null`, `None` | `NULL` |
| Loop index | `for i := 0; i < n; i++` | `till(n, 1) { $... }` |

---

**Next Steps**:
- Use these patterns for batch corrections in Sessions 28-30
- Reference this file when uncertain about syntax
- Update this file if new canonical patterns emerge

**Related Documents**:
- [SYNTAX_AUDIT_FEB14_2026.md](SYNTAX_AUDIT_FEB14_2026.md) - Comprehensive issue list
- [UPDATE_PROGRESS.md](UPDATE_PROGRESS.md) - Overall progress tracking
- [error_handling.md](../advanced_features/error_handling.md) - Fixed error handling guide
- [filter.md](../stdlib/filter.md) - Fixed stdlib example
