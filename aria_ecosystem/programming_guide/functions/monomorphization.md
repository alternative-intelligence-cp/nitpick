# Monomorphization

**Category**: Functions → Generics → Compilation  
**Concept**: Generating specialized code for each generic type used  
**Philosophy**: "Pay for what you use" - zero abstraction penalty

---

## What is Monomorphization?

**Monomorphization** is the process where the compiler takes your **generic code** and generates **specialized versions** for each concrete type you actually use.

---

## How It Works

### Generic Code

```aria
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then
        return a;
    else
        return b;
    end
}
```

### Usage in Code

```aria
x: i32 = max(10, 20);
y: f64 = max(3.14, 2.71);
z: string = max("alice", "bob");
```

### Compiler Generates

```aria
// Specialized for i32
fn max_i32(a: i32, b: i32) -> i32 {
    when a > b then
        return a;
    else
        return b;
    end
}

// Specialized for f64
fn max_f64(a: f64, b: f64) -> f64 {
    when a > b then
        return a;
    else
        return b;
    end
}

// Specialized for string
fn max_string(a: string, b: string) -> string {
    when a > b then
        return a;
    else
        return b;
    end
}
```

### Final Code

```aria
x: i32 = max_i32(10, 20);           // Direct call
y: f64 = max_f64(3.14, 2.71);       // Direct call
z: string = max_string("alice", "bob");  // Direct call
```

---

## Benefits

### 1. Zero Runtime Overhead

```aria
// Generic version
fn add<T>(a: T, b: T) -> T {
    return a + b;
}

// After monomorphization, calling:
Result: i32 = add(5, 10);

// Is exactly as fast as:
Result: i32 = 5 + 10;
```

**No**:
- Virtual dispatch
- Runtime type checking
- Boxing/unboxing
- Indirection

### 2. Full Optimization

Each specialized version can be **independently optimized**:

```aria
fn process<T>(value: T) -> T {
    // Complex operations
}

// For i32: Compiler can use CPU integer ops
x: i32 = process(42);

// For f64: Compiler can use FPU float ops
y: f64 = process(3.14);

// For string: Compiler can inline string methods
z: string = process("hello");
```

### 3. Type Safety at Compile Time

```aria
fn multiply<T>(a: T, b: T) -> T where T: Numeric {
    return a * b;
}

// OK: i32 is Numeric
Result: i32 = multiply(5, 10);

// Error at COMPILE TIME: string is not Numeric
// Result: string = multiply("a", "b");  // Won't compile
```

---

## Code Size Trade-off

### More Types = More Code

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// Used with 3 types
a: i32 = identity(1);
b: f64 = identity(2.0);
c: string = identity("three");

// Generates 3 functions:
// - identity_i32
// - identity_f64
// - identity_string
```

**Binary Size Impact**:
- Small generic functions: Minimal impact
- Large generic functions: Can increase binary size significantly

### Optimization: Shared Code

For identical implementations, compilers can sometimes **share** the generated code:

```aria
struct Box<T> {
    value: T
}

// Box<i32> and Box<f64> have same memory layout
// Compiler MAY share the code if implementations are identical
```

---

## Unused Types Elimination

The compiler **only generates code for types you actually use**:

```aria
fn process<T>(value: T) -> T {
    // Complex 1000-line implementation
}

// Only use with i32
Result: i32 = process(42);

// Compiler generates ONLY:
// - process_i32

// Does NOT generate:
// - process_f64, process_string, etc.
```

---

## Complex Generics

### Nested Generics

```aria
fn wrap<T>(value: T) -> Box<T> {
    return Box{value: value};
}

fn double_wrap<T>(value: T) -> Box<Box<T>> {
    return wrap(wrap(value));
}

// Usage
x: Box<Box<i32>> = double_wrap(42);

// Generates:
// - wrap_i32 -> Box<i32>
// - wrap_Box_i32 -> Box<Box<i32>>
// - double_wrap_i32 -> Box<Box<i32>>
```

### Generic Containers

```aria
numbers: Vec<i32> = Vec::new();
strings: Vec<string> = Vec::new();
nested: Vec<Vec<i32>> = Vec::new();

// Generates:
// - Vec<i32> implementation
// - Vec<string> implementation
// - Vec<Vec<i32>> implementation
```

---

## Performance Comparison

### Monomorphization (Aria, Rust, C++)

```aria
fn sum<T>(array: []T) -> T where T: Numeric {
    total: T = T::zero();
    till(array.length - 1, 1) {
        item: T = array[$];
        total = total + item;
    }
    return total;
}

// Generates optimized code for each type
int_sum: i32 = sum([1, 2, 3]);      // Fast integer arithmetic
float_sum: f64 = sum([1.0, 2.0, 3.0]);  // Fast FPU arithmetic
```

**Performance**: ⚡ **Identical to hand-written code**

### Dynamic Dispatch (Java, C#)

```java
// Java example
<T extends Number> T sum(List<T> array) {
    // Runtime type checks
    // Boxing/unboxing overhead
    // Virtual method calls
}
```

**Performance**: ⚠️ **Runtime overhead** from indirection and boxing

---

## Compile Time Impact

### Longer Compilation

More generic instantiations = longer compile times:

```aria
// This function is used with 50 different types
fn process<T>(value: T) -> T {
    // Large implementation
}

// Compiler must generate 50 versions
// = 50x compilation time for this function
```

### Incremental Compilation Helps

Most build systems only recompile what changed:
- Change non-generic code: Fast rebuild
- Change generic code: Recompile all instantiations
- Add new type usage: Only compile new instantiation

---

## Best Practices

### ✅ DO: Use Generics for Core Operations

```aria
// Good: Small, frequently used
fn swap<T>(a: T, b: T) -> (T, T) {
    return (b, a);
}

// Minimal code size impact
// Maximum reusability
```

### ✅ DO: Extract Common Non-Generic Code

```aria
// Good: Generic wrapper around non-generic core
fn process_bytes(data: []u8) {
    // Large implementation
}

fn process<T>(value: T) -> T where T: Serializable {
    bytes: []u8 = value.serialize();
    process_bytes(bytes);  // Shared across all types
    return T::deserialize(bytes);
}
```

### ✅ DO: Profile Binary Size if Concerned

```bash
# Check which generic instantiations are largest
aria build --verbose --size-analysis

# Shows:
# - process_User: 15KB
# - process_Product: 14KB
# - process_Order: 13KB
```

### ❌ DON'T: Use Generics for Very Large Functions

```aria
// Wrong: 5000-line generic function
fn massive_algorithm<T>(value: T) -> T {
    // 5000 lines of code
}

// Used with 20 types = 100,000 lines in binary!
```

### ❌ DON'T: Optimize Prematurely

```aria
// Wrong: Avoiding generics for "performance"
fn process_i32(value: i32) { ... }
fn process_f64(value: f64) { ... }
fn process_string(value: string) { ... }

// Right: Use generics, profile later
fn process<T>(value: T) { ... }
```

---

## Viewing Generated Code

### Compiler Flags

```bash
# Show monomorphized output
aria build --emit=mir --show-monomorphization

# Shows:
# Function: max<T>
# Instantiations:
#   - max<i32> at line 10
#   - max<f64> at line 11
#   - max<string> at line 12
```

### Debug Information

```aria
// With debug symbols
aria build --debug

// Debugger shows:
// - max_i32 (from max<i32>)
// - max_f64 (from max<f64>)
```

---

## Comparison with Alternatives

### Monomorphization (Aria, Rust, C++)

**Pros**:
- ⚡ Zero runtime overhead
- 🎯 Full optimization per type
- 🔒 Compile-time type safety

**Cons**:
- 📦 Larger binary size
- ⏱️ Longer compile times
- 🔄 Code bloat for large generics

### Dynamic Dispatch (Java, C#)

**Pros**:
- 📦 Smaller binary
- ⚡ Fast compilation

**Cons**:
- ⚠️ Runtime overhead
- 📦 Boxing/unboxing allocations
- 🐌 Slower than monomorphized code

### Template Instantiation (C++)

**Same as Aria** - C++ templates use monomorphization

---

## Real-World Impact

### Small Generic Functions

```aria
fn max<T>(a: T, b: T) -> T { ... }  // ~10 instructions

// Used with 100 types = 1000 instructions
// Impact: Negligible (~4KB)
```

### Medium Generic Functions

```aria
fn parse<T>(input: string) -> T { ... }  // ~200 instructions

// Used with 20 types = 4000 instructions
// Impact: Moderate (~16KB)
```

### Large Generic Functions

```aria
fn complex_algorithm<T>(data: []T) -> Result<T> {
    // 10,000 instructions
}

// Used with 10 types = 100,000 instructions
// Impact: Significant (~400KB)
```

---

## Related Topics

- [Generic Functions](generic_functions.md) - Generic function overview
- [Generics](generics.md) - General generics concept
- [Type Inference](type_inference.md) - How types are inferred
- [Generic Syntax](generic_syntax.md) - Syntax reference

---

**Remember**: Monomorphization is **compile-time magic** - you write generic code once, and the compiler generates optimized specialized versions for each type you use. The result is **zero runtime overhead** at the cost of some binary size!
