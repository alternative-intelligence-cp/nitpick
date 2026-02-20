# Generics Overview

**Category**: Functions → Generics  
**Concept**: Write code once, use with any type  
**Philosophy**: Type safety without code duplication

---

## What are Generics?

**Generics** let you write functions, structs, and types that work with **any type** instead of a specific type. They're like templates that get filled in with actual types when you use them.

---

## Why Generics?

### Without Generics (Code Duplication)

```aria
fn max_i32(a: i32, b: i32) -> i32 {
    when a > b then return a; else return b; end
}

fn max_f64(a: f64, b: f64) -> f64 {
    when a > b then return a; else return b; end
}

fn max_string(a: string, b: string) -> string {
    when a > b then return a; else return b; end
}

// And so on for every type...
```

### With Generics (Write Once)

```aria
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}

// Works with any comparable type!
x: i32 = max(10, 20);
y: f64 = max(3.14, 2.71);
z: string = max("alice", "bob");
```

---

## Generic Type Parameters

### Syntax

```aria
fn function_name<T>(param: T) -> T {
    // T is a type parameter
    // Can be any type
}
```

### Single Type Parameter

```aria
fn identity<T>(value: T) -> T {
    return value;
}

num: i32 = identity(42);
text: string = identity("hello");
```

### Multiple Type Parameters

```aria
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

Result: (i32, string) = pair(42, "answer");
```

### Descriptive Names

```aria
// Single generic: T (type)
fn wrap<T>(value: T) -> Box<T> { ... }

// Input/Output: In, Out
fn convert<In, Out>(value: In) -> Out { ... }

// Key/Value: K, V
fn insert<K, V>(key: K, value: V) { ... }

// Element: T or E
fn sum<T>(array: []T) -> T { ... }
```

---

## Type Constraints

### Trait Bounds

```aria
// T must implement Display
fn print<T>(value: T) where T: Display {
    stdout << value << "\n";
}

// T must implement Comparable
fn sort<T>(array: []T) where T: Comparable {
    // Can use >, <, ==, etc.
}

// T must implement both traits
fn process<T>(value: T) where T: Display, T: Clone {
    copy: T = value.clone();
    stdout << copy << "\n";
}
```

### Multiple Constraints

```aria
// Method 1: where clause
fn unique<T>(array: []T) -> []T 
    where T: Comparable, T: Hashable {
    // Implementation
}

// Method 2: inline (less common)
fn unique<T: Comparable + Hashable>(array: []T) -> []T {
    // Implementation
}
```

---

## Generic Structs

```aria
struct Box<T> {
    value: T
}

impl<T> Box<T> {
    fn new(value: T) -> Box<T> {
        return Box{value: value};
    }
    
    fn get() -> T {
        return self.value;
    }
}

int_box: Box<i32> = Box::new(42);
str_box: Box<string> = Box::new("hello");
```

---

## Generic Enums

```aria
enum Option<T> {
    Some(T),
    None
}

enum Result<T, E> {
    Ok(T),
    Err(E)
}

// Usage
maybe_num: Option<i32> = Some(42);
Result: Result<string, Error> = Ok("success");
```

---

## Type Inference

The compiler can often infer generic types:

```aria
fn first<T>(array: []T) -> T {
    return array[0];
}

// Explicit type
value: i32 = first<i32>([1, 2, 3]);

// Inferred from array type
value: i32 = first([1, 2, 3]);  // T inferred as i32

// Inferred from return type
numbers: []i32 = [1, 2, 3];
value: i32 = first(numbers);  // T inferred as i32
```

---

## Monomorphization

Aria generates **specialized code** for each type you use:

```aria
fn add<T>(a: T, b: T) -> T {
    return a + b;
}

x: i32 = add(1, 2);
y: f64 = add(1.5, 2.5);
```

**Compiler generates**:
```aria
// Generated for i32
fn add_i32(a: i32, b: i32) -> i32 {
    return a + b;
}

// Generated for f64
fn add_f64(a: f64, b: f64) -> f64 {
    return a + b;
}
```

**Benefits**:
- ✅ **Zero runtime overhead** - as fast as hand-written code
- ✅ **Full optimization** - each version optimized independently
- ✅ **No dynamic dispatch** - direct function calls

**Trade-off**:
- ⚠️ **Larger binary** - separate code for each type

---

## Common Generic Patterns

### Container Types

```aria
struct Vec<T> {
    data: []T,
    len: usize,
    capacity: usize
}

struct Map<K, V> {
    entries: []Entry<K, V>
}

struct Set<T> {
    data: Map<T, bool>
}
```

### Iterator Pattern

```aria
fn map<T, U>(array: []T, f: fn(T) -> U) -> []U {
    Result: []U = [];
    till(array.length - 1, 1) {
        item: T = array[$];
        result.push(f(item));
    }
    return result;
}

numbers: []i32 = [1, 2, 3];
doubled: []i32 = map(numbers, |x| x * 2);
```

### Builder Pattern

```aria
struct Builder<T> {
    items: []T
}

impl<T> Builder<T> {
    fn new() -> Builder<T> {
        return Builder{items: []};
    }
    
    fn add(item: T) -> Builder<T> {
        self.items.push(item);
        return self;
    }
    
    fn build() -> []T {
        return self.items;
    }
}
```

---

## Best Practices

### ✅ DO: Use Generics for Reusable Code

```aria
// Good: Works with any type
fn swap<T>(a: T, b: T) -> (T, T) {
    return (b, a);
}
```

### ✅ DO: Add Constraints When Needed

```aria
// Good: Clear what T must support
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}
```

### ✅ DO: Use Descriptive Type Names

```aria
// Good: Clear purpose
fn cache_get<Key, Value>(key: Key) -> Value? { ... }

// Avoid: Unclear
fn cache_get<A, B>(key: A) -> B? { ... }
```

### ❌ DON'T: Overuse Generics

```aria
// Wrong: Only works with numbers anyway
fn add_numbers<T>(a: T, b: T) -> T {
    return a + b;
}

// Right: Be specific
fn add_numbers(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### ❌ DON'T: Forget Type Constraints

```aria
// Wrong: Won't compile - T might not support +
fn sum<T>(array: []T) -> T {
    total: T = 0;  // Error: can't assign 0 to T
    till(array.length - 1, 1) {
        item: T = array[$];
        total = total + item;  // Error: T might not have +
    }
    return total;
}

// Right: Add constraint
fn sum<T>(array: []T) -> T where T: Numeric {
    total: T = T::zero();
    till(array.length - 1, 1) {
        item: T = array[$];
        total = total + item;
    }
    return total;
}
```

---

## Related Topics

- [Generic Functions](generic_functions.md) - Function generics in detail
- [Generic Structs](generic_structs.md) - Struct generics
- [Generic Syntax](generic_syntax.md) - Syntax reference
- [Monomorphization](monomorphization.md) - How generics compile
- [Type Inference](type_inference.md) - Type parameter inference

---

**Remember**: Generics are **compile-time templates** - they generate specialized code for each type you use. Zero runtime overhead!
