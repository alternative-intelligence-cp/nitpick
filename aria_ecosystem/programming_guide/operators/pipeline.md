# Pipeline Operators (`|>` and `<|`)

**Category**: Operators → Functional Programming  
**Operators**: `|>` (forward), `<|` (backward)  
**Purpose**: Chain function calls in readable, functional style

---

## Overview

Pipeline operators pass the result of one expression as the argument to the next function, enabling elegant function composition without nested parentheses.

**Philosophy**: Data flows visibly through transformations, left-to-right or right-to-left.

---

## Forward Pipeline (`|>`)

### Syntax

```aria
value |> function       // Pass value as argument to function
value |> func(args)     // Pass value as first argument
```

### Basic Usage

```aria
// Without pipeline (nested)
result = reduce(transform(filter(data, isValid), normalize), sum);

// With forward pipeline (readable)
result = data
    |> filter(isValid)
    |> transform(normalize)
    |> reduce(sum);
```

### How It Works

The left-hand side becomes the **first argument** to the function on the right:

```aria
x |> f        ≡  f(x)
x |> f(y)     ≡  f(x, y)
x |> f(y, z)  ≡  f(x, y, z)
```

---

## Backward Pipeline (`<|`)

### Syntax

```aria
function <| value       // Pass value as argument to function
func(args) <| value     // Pass value as last argument
```

### Basic Usage

```aria
// Without pipeline (nested)
result = sum(reduce(normalize(transform(filter(data)))));

// With backward pipeline (right-to-left)
result = sum <| reduce <| normalize <| transform <| filter <| data;
```

### How It Works

The right-hand side becomes the **last argument** to the function on the left:

```aria
f <| x        ≡  f(x)
f(y) <| x     ≡  f(y, x)
f(y, z) <| x  ≡  f(y, z, x)
```

---

## Examples

### Example 1: Data Processing Pipeline

```aria
// Process array of numbers
int64[]:numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

int64:result = numbers
    |> filter((x) => x % 2 == 0)     // [2, 4, 6, 8, 10]
    |> transform((x) => x * 2)       // [4, 8, 12, 16, 20]
    |> reduce((sum, x) => sum + x);  // 60

print(result);  // 60
```

### Example 2: String Transformations

```aria
string:input = "  Hello, World!  ";

string:clean = input
    |> trim()           // "Hello, World!"
    |> toLowerCase()    // "hello, world!"
    |> replace(",", ""); // "hello world!"

print(clean);  // "hello world!"
```

### Example 3: HTTP Request Pipeline

```aria
obj:response = "https://api.example.com/users/123"
    |> httpGet()
    |> parseJSON()
    |> validateUser()
    |> transformUserData()
    |> saveToDatabase();
```

### Example 4: File Processing

```aria
Result<string>:content = "config.json"
    |> readFile()
    |> parseJSON()
    |> validateConfig()
    |> extractSettings()
    |> serializeToString();
```

---

## Forward vs Backward

### Forward (`|>`) - Left-to-Right Flow

Most readable for data transformations:

```aria
// Natural reading order: input → step1 → step2 → step3 → output
output = input
    |> step1
    |> step2
    |> step3;
```

### Backward (`<|`) - Right-to-Left Flow

Natural for function application:

```aria
// Natural function composition: f(g(h(x)))
output = f <| g <| h <| input;
```

### Choose Based on Context

```aria
// Forward for data pipelines (common)
users = database
    |> fetchUsers()
    |> filter(isActive)
    |> sort(byName);

// Backward for function composition
result = apply <| combine <| prepare <| data;
```

---

## With Lambda Functions

### Inline Lambdas

```aria
result = [1, 2, 3, 4, 5]
    |> filter((x) => x > 2)        // [3, 4, 5]
    |> transform((x) => x * x)     // [9, 16, 25]
    |> reduce((sum, x) => sum + x); // 50
```

### Named Lambdas

```aria
func:double = int64(int64:x) { pass(x * 2); };
func:isEven = bool(int64:x) { pass(x % 2 == 0); };

result = [1, 2, 3, 4, 5]
    |> filter(isEven)   // [2, 4]
    |> transform(double); // [4, 8]
```

---

## Method Chaining vs Pipelines

### Method Chaining (Future with UFCS)

```aria
// When Phase 2 UFCS complete
result = data
    .filter(isValid)
    .transform(normalize)
    .reduce(sum);
```

### Pipeline Operator (Now)

```aria
// Current approach with pipelines
result = data
    |> filter(isValid)
    |> transform(normalize)
    |> reduce(sum);
```

Both are equivalent once UFCS transformation is implemented!

---

## Complex Pipelines

### Multi-Stage Processing

```aria
obj:analytics = rawData
    |> cleanData()
    |> normalizeValues()
    |> extractFeatures()
    |> calculateStatistics()
    |> generateReport();
```

### Error Handling in Pipelines

```aria
Result<obj>:processed = input
    |> validateInput()     // Returns Result<obj>
    |> processIfValid()    // Returns Result<obj>
    |> saveResults();      // Returns Result<obj>

// Unwrap with ? operator
obj:final = processed ? defaultValue;
```

### Conditional Pipelines

```aria
result = data
    |> (condition is preprocessA : preprocessB)
    |> commonTransform()
    |> (debug is addLogging : identity);
```

---

## Common Patterns

### Pattern 1: ETL (Extract-Transform-Load)

```aria
func:etlPipeline = Result<int64>(string:source) {
    int64:count = source
        |> extract()      // Extract data
        |> transform()    // Transform data
        |> load();        // Load into destination
    
    pass(count);
};
```

### Pattern 2: HTTP API Response Processing

```aria
obj:user = "/api/users/me"
    |> httpGet()
    |> parseJSON()
    |> validateSchema()
    |> extractUserData();

print(`Welcome, &{user.name}!`);
```

### Pattern 3: Collection Processing

```aria
int64[]:primes = range(1, 100)
    |> filter(isPrime)
    |> sort(ascending)
    |> unique();
```

### Pattern 4: Functional Composition

```aria
// Build complex function from simple ones
func:processUser = obj(obj:raw) {
    pass(raw
        |> normalizeEmail
        |> validateAge
        |> enrichMetadata
        |> formatResponse);
};
```

---

## Performance Considerations

### Zero Overhead

Pipeline operators are **pure syntax sugar** - they compile to direct function calls:

```aria
// This pipeline...
result = x |> f |> g |> h;

// ...compiles identically to:
result = h(g(f(x)));
```

**No runtime cost whatsoever.**

### Avoid Unnecessary Allocations

```aria
// GOOD: Each step operates on references
result = data
    |> filterByRef()      // No copy
    |> transformInPlace() // No copy
    |> reduce();          // Final result

// BAD: Each step copies entire array
result = data
    |> filterWithCopy()   // Copy!
    |> transformWithCopy() // Copy!
    |> reduce();
```

---

## Edge Cases

### Dollar Variable (`$`) in Pipelines

The `$` operator captures the pipeline value:

```aria
result = 10
    |> ($ + 5)      // 15
    |> ($ * 2)      // 30
    |> ($ - 10);    // 20
```

### Multiple Arguments

```aria
// Forward pipeline: value becomes first argument
result = data |> process(arg2, arg3);
// Equivalent to: process(data, arg2, arg3)

// Backward pipeline: value becomes last argument  
result = process(arg1, arg2) <| data;
// Equivalent to: process(arg1, arg2, data)
```

### Mixing Forward and Backward

```aria
// Can mix, but avoid for clarity
result = a |> f <| b;  // Confusing!

// Better: stick to one direction
result = a |> f(b);    // Clear
result = f(b) <| a;    // Also clear
```

---

## Comparison with Other Languages

### Aria

```aria
result = data
    |> filter(isValid)
    |> transform(double)
    |> reduce(sum);
```

### F# / Elixir (|>)

```fsharp
result = data
    |> filter isValid
    |> transform double
    |> reduce sum
```

### Haskell / Elm (|>)

```haskell
result = data
    |> filter isValid
    |> map double
    |> foldl (+) 0
```

### Unix Pipes (|)

```bash
cat data.txt | grep "valid" | sed 's/x/y/' | wc -l
```

---

## Best Practices

### ✅ Use Pipelines for Multi-Step Transformations

```aria
// GOOD: Clear transformation flow
clean = rawData
    |> validate
    |> normalize
    |> sanitize;
```

### ✅ One Operation Per Line

```aria
// GOOD: Readable
result = data
    |> filter(isValid)
    |> transform(normalize)
    |> reduce(sum);

// BAD: Hard to read
result = data |> filter(isValid) |> transform(normalize) |> reduce(sum);
```

### ✅ Prefer Forward for Data Processing

```aria
// GOOD: Natural left-to-right reading
output = input |> step1 |> step2 |> step3;

// LESS CLEAR: Backward feels unnatural for data flow
output = step3 <| step2 <| step1 <| input;
```

### ❌ Don't Overuse

```aria
// BAD: Too simple for pipeline
result = x |> f;

// GOOD: Just call directly
result = f(x);
```

### ❌ Don't Mix Styles Unnecessarily

```aria
// BAD: Mixing pipeline and nesting
result = process(data |> filter(x) |> map(y), other);

// GOOD: Consistent style
filtered = data |> filter(x) |> map(y);
result = process(filtered, other);
```

---

## Related Topics

- [Lambda Functions](../functions/lambda.md) - Anonymous functions in pipelines
- [filter/transform/reduce](../standard_library/collections.md) - Common pipeline functions
- [Dollar Variable](dollar_variable.md) - `$` in pipelines
- [UFCS](../advanced_features/ufcs.md) - Method chaining (future)
- [Functional Programming](../advanced_features/functional.md) - FP patterns in Aria

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Lines 569-571  
**Critical for**: System utilities and functional programming style
