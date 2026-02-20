# Generic Syntax Reference

**Category**: Functions → Generics → Reference  
**Basic Form**: `<T>`  
**Purpose**: Complete syntax reference for generic types

---

## Basic Generic Syntax

### Function Declaration

```aria
// Single type parameter
fn identity<T>(value: T) -> T {
    return value;
}

// Multiple type parameters
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}
```

### Struct Declaration

```aria
// Generic struct
struct Box<T> {
    value: T
}

// Multiple type parameters
struct Pair<T, U> {
    first: T,
    second: U
}
```

### Enum Declaration

```aria
// Generic enum
enum Option<T> {
    Some(T),
    None
}

// Multiple type parameters
enum Result<T, E> {
    Ok(T),
    Err(E)
}
```

---

## Type Parameter Naming

### Convention: Single Letters

```aria
// T for single generic type
fn wrap<T>(value: T) -> Box<T> { }

// T, U for two types
fn convert<T, U>(input: T) -> U { }

// K, V for Key/Value
fn insert<K, V>(key: K, value: V) { }
```

### Descriptive Names (Rare)

```aria
// More descriptive when needed
fn transform<Input, Output>(data: Input) -> Output { }

fn cache<Key, Value>(k: Key, v: Value) { }
```

---

## Trait Bounds (Constraints)

### Single Constraint

```aria
// Using where clause
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}

// Inline constraint (alternative)
fn max<T: Comparable>(a: T, b: T) -> T {
    when a > b then return a; else return b; end
}
```

### Multiple Constraints

```aria
// Multiple where clauses
fn process<T>(value: T) -> T 
    where T: Display, 
          T: Clone {
    copy: T = value.clone();
    stdout << copy << "\n";
    return copy;
}

// Inline with +
fn process<T: Display + Clone>(value: T) -> T {
    copy: T = value.clone();
    stdout << copy << "\n";
    return copy;
}
```

### Multiple Type Parameters with Constraints

```aria
fn transform<T, U>(input: T) -> U
    where T: Serializable,
          U: Deserializable {
    data: []u8 = input.serialize();
    return U::deserialize(data);
}
```

---

## Type Argument Syntax

### Explicit Type Arguments

```aria
// Turbofish syntax ::<>
Result: i32 = identity::<i32>(42);

// Multiple arguments
pair: (i32, string) = pair::<i32, string>(42, "hello");
```

### Type Inference

```aria
// Inferred from argument
Result: i32 = identity(42);  // T inferred as i32

// Inferred from return type
num: i32 = parse("42");  // T inferred as i32
```

### Partial Inference (Underscore)

```aria
// Specify some, infer others
Result: string = convert::<_, string>(42);  // From inferred, To specified
```

---

## Generic Struct Usage

### Instantiation

```aria
struct Box<T> {
    value: T
}

// Explicit type
int_box: Box<i32> = Box{value: 42};

// Type inferred from field
str_box = Box{value: "hello"};  // Box<string>
```

### Implementation Blocks

```aria
// Generic implementation
impl<T> Box<T> {
    fn new(value: T) -> Box<T> {
        return Box{value: value};
    }
    
    fn get() -> T {
        return self.value;
    }
}

// Implementation for specific type
impl Box<i32> {
    fn is_even() -> bool {
        return self.value % 2 == 0;
    }
}

// Implementation with constraints
impl<T> Box<T> where T: Display {
    fn print() {
        stdout << self.value << "\n";
    }
}
```

---

## Generic Enum Usage

```aria
enum Option<T> {
    Some(T),
    None
}

// Create variants
some_value: Option<i32> = Some(42);
no_value: Option<i32> = None;

// Match on generic enum
Result: i32 = match some_value {
    Some(x) => x,
    None => 0
};
```

---

## Lifetime Parameters (If Supported)

```aria
// Lifetime parameter (rare in Aria)
fn longest<'a>(s1: &'a string, s2: &'a string) -> &'a string {
    when s1.len() > s2.len() then
        return s1;
    else
        return s2;
    end
}
```

---

## Associated Types

```aria
trait Iterator {
    type Item;  // Associated type
    
    fn next() -> Option<Self::Item>;
}

struct Counter;

impl Iterator for Counter {
    type Item = i32;  // Concrete type
    
    fn next() -> Option<i32> {
        // Implementation
    }
}
```

---

## Generic Type Aliases

```aria
// Type alias with generics
type Result<T> = Result<T, Error>;

// Usage
fn parse(input: string) -> Result<i32> {
    // Returns Result<i32, Error>
}

// Multiple parameters
type HashMap<K, V> = Map<K, V>;
```

---

## Nested Generics

```aria
// Box of box
nested: Box<Box<i32>> = Box{value: Box{value: 42}};

// Vec of optional values
items: Vec<Option<i32>> = vec![Some(1), None, Some(3)];

// Map with generic values
cache: Map<string, Box<User>> = Map::new();
```

---

## Generic Function Pointers

```aria
// Generic function type
transformer: fn<T>(T) -> T;  // Function that works with any T

// Concrete function type
doubler: fn(i32) -> i32 = |x| x * 2;
```

---

## Special Syntax

### Star Prefix (Alternative Syntax)

```aria
// Alternative generic syntax (if supported)
fn identity<*T>(value: T) -> T {
    return value;
}

// Equivalent to:
fn identity<T>(value: T) -> T {
    return value;
}
```

### Const Generics (If Supported)

```aria
// Generic over constant values
struct Array<T, const N: usize> {
    data: [T; N]
}

// Usage
arr: Array<i32, 10> = Array::new();
```

---

## Complete Examples

### Generic Container

```aria
struct Vec<T> {
    data: []T,
    len: usize,
    capacity: usize
}

impl<T> Vec<T> {
    fn new() -> Vec<T> {
        return Vec{
            data: [],
            len: 0,
            capacity: 0
        };
    }
    
    fn push(item: T) {
        // Implementation
    }
    
    fn get(index: usize) -> T? {
        when index >= self.len then
            return nil;
        end
        return self.data[index];
    }
}

// Usage
numbers: Vec<i32> = Vec::new();
numbers.push(1);
numbers.push(2);
```

### Generic Function with Multiple Constraints

```aria
fn unique_sorted<T>(array: []T) -> []T
    where T: Comparable,
          T: Hashable,
          T: Clone {
    
    seen: Set<T> = Set::new();
    unique: []T = [];
    
    till(array.length - 1, 1) {
        item: T = array[$];
        when !seen.contains(item.clone()) then
            seen.insert(item.clone());
            unique.push(item);
        end
    }
    
    return sort(unique);
}

// Usage
numbers: []i32 = [3, 1, 4, 1, 5, 9, 2, 6, 5];
Result: []i32 = unique_sorted(numbers);  // [1, 2, 3, 4, 5, 6, 9]
```

---

## Best Practices

### ✅ DO: Use where Clauses for Complex Constraints

```aria
// Good: Readable
fn process<T, U>(input: T, output: U) -> Result
    where T: Serializable + Clone,
          U: Deserializable + Display {
    // Implementation
}
```

### ✅ DO: Use Type Inference When Obvious

```aria
// Good: Types clear from context
numbers: []i32 = [1, 2, 3];
doubled: []i32 = numbers.map(|x| x * 2);
```

### ✅ DO: Use Descriptive Names for Domain Types

```aria
// Good: Clear purpose
fn cache_lookup<Key, Value>(key: Key) -> Value? { }
```

### ❌ DON'T: Over-specify Types

```aria
// Wrong: Too explicit
Result: i32 = identity::<i32>(42_i32);

// Right: Let inference work
Result: i32 = identity(42);
```

### ❌ DON'T: Use Too Many Type Parameters

```aria
// Wrong: Confusing
fn complex<T, U, V, W, X>(a: T, b: U, c: V, d: W) -> X { }

// Right: Simplify or use structs
struct Config<T, U, V, W> { }
fn complex<X>(config: Config, ...) -> X { }
```

---

## Related Topics

- [Generic Functions](generic_functions.md) - Generic function details
- [Generics](generics.md) - Generic concept overview
- [Type Inference](type_inference.md) - Type parameter inference
- [Monomorphization](monomorphization.md) - How generics compile

---

**Remember**: Generic syntax is **angle brackets** `<T>` for declaration, **turbofish** `::<T>` for explicit instantiation!
