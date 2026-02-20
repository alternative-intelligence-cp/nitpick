# Generic Parameters

**Category**: Functions → Generics → Reference  
**Concept**: Type parameters in generic definitions  
**Syntax**: `<T>`, `<T, U>`, `<T: Trait>`

---

## What Are Generic Parameters?

**Generic parameters** are the type variables (`T`, `U`, `K`, `V`, etc.) used in generic definitions. They're placeholders for concrete types.

---

## Basic Syntax

```aria
// Single parameter
fn identity<T>(value: T) -> T {
    return value;
}

// Multiple parameters
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}
```

---

## Naming Conventions

### Standard Names

```aria
// T - Generic Type
fn wrap<T>(value: T) -> Box<T> { }

// T, U - Two types
fn convert<T, U>(input: T) -> U { }

// K, V - Key, Value
fn insert<K, V>(key: K, value: V) { }

// E - Error type
fn try_parse<E>(input: string) -> Result<i32, E> { }

// R - Result type
fn execute<R>(query: string) -> R { }
```

### Descriptive Names

```aria
// Longer names when helpful
fn transform<Input, Output>(data: Input) -> Output { }

fn cache<Key, Value>(k: Key, v: Value) { }

fn convert<Source, Target>(s: Source) -> Target { }
```

---

## Parameter Declaration Locations

### On Functions

```aria
fn function<T>(param: T) -> T {
    // T available in this function
}
```

### On Structs

```aria
struct Container<T> {
    items: []T
}
// T available for all fields
```

### On Enums

```aria
enum Result<T, E> {
    Ok(T),
    Err(E)
}
// T and E available for all variants
```

### On Impl Blocks

```aria
impl<T> Container<T> {
    // T available for all methods
    fn new() -> Container<T> { }
    fn add(item: T) { }
}
```

### On Traits

```aria
trait Convert<T> {
    fn convert(value: T) -> Self;
}
// T available for all trait methods
```

---

## Type Parameter Scope

```aria
struct Box<T> {
    value: T
    // T in scope for struct
}

impl<T> Box<T> {
    // T from struct is in scope
    
    fn get() -> T {
        return self.value;
    }
    
    // U is NEW parameter, separate from T
    fn map<U>(f: fn(T) -> U) -> Box<U> {
        return Box{value: f(self.value)};
    }
}
```

---

## Parameter Constraints

### Single Constraint

```aria
// T must implement Display
fn print<T>(value: T) where T: Display {
    stdout << value << "\n";
}

// Inline syntax
fn print<T: Display>(value: T) {
    stdout << value << "\n";
}
```

### Multiple Constraints

```aria
// T must implement multiple traits
fn process<T>(value: T) 
    where T: Display, 
          T: Clone,
          T: Debug {
    // Implementation
}

// Inline with +
fn process<T: Display + Clone + Debug>(value: T) {
    // Implementation
}
```

### Different Parameters, Different Constraints

```aria
fn transform<T, U>(input: T) -> U
    where T: Serialize,
          U: Deserialize {
    data: []u8 = input.serialize();
    return U::deserialize(data);
}
```

---

## Default Type Parameters (If Supported)

```aria
// Default type for T
struct Container<T = i32> {
    items: []T
}

// Can omit type parameter
default: Container = Container{items: []};  // Container<i32>

// Or specify explicitly
custom: Container<string> = Container{items: []};
```

---

## Const Generic Parameters (If Supported)

```aria
// Generic over constant value
struct Array<T, const N: usize> {
    data: [T; N]
}

// N is compile-time constant
arr: Array<i32, 10>;  // Array of exactly 10 i32s
```

---

## Lifetime Parameters (If Supported)

```aria
// Lifetime parameter
fn longest<'a>(s1: &'a string, s2: &'a string) -> &'a string {
    when s1.len() > s2.len() then
        return s1;
    else
        return s2;
    end
}

// Multiple lifetimes
struct Pair<'a, 'b, T> {
    first: &'a T,
    second: &'b T
}
```

---

## Parameter Position

### In Function Signature

```aria
fn process<T>(
    input: T,        // T used in parameter
    count: i32       // Regular type
) -> Vec<T> {        // T used in return
    // Implementation
}
```

### In Struct Fields

```aria
struct Container<T> {
    data: T,         // T used in field
    size: usize      // Regular type
}
```

### In Type Aliases

```aria
type Result<T> = Result<T, Error>;
//         ↑ parameter    ↑ used here
```

---

## Unused Parameters (Phantom Types)

```aria
// T not used in fields but marks the type
struct Marker<T> {
    _phantom: PhantomData<T>
}

// Different markers are different types
type UserId = Marker<User>;
type ProductId = Marker<Product>;
```

---

## Parameter Inference

### From Arguments

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// T inferred as i32
Result: i32 = identity(42);
```

### From Return Type

```aria
fn default<T>() -> T {
    return T::default();
}

// T inferred as string
text: string = default();
```

### Partial Inference

```aria
fn convert<From, To>(value: From) -> To {
    // Implementation
}

// From inferred, To specified
Result: string = convert::<_, string>(42);
```

---

## Multiple Parameter Relationships

### Independent Parameters

```aria
// T and U are unrelated
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}
```

### Related Parameters

```aria
// U depends on T through constraint
fn map<T, U>(array: []T, f: fn(T) -> U) -> []U {
    Result: []U = [];
    till(array.length - 1, 1) {
        item: T = array[$];
        result.push(f(item));
    }
    return result;
}
```

### Same Parameter Used Multiple Times

```aria
// T used in multiple positions
fn swap<T>(a: T, b: T) -> (T, T) {
    return (b, a);
}
```

---

## Best Practices

### ✅ DO: Use Single Letters for Simple Generics

```aria
// Good: Standard and clear
fn identity<T>(value: T) -> T { }
fn pair<T, U>(a: T, b: U) -> (T, U) { }
```

### ✅ DO: Use Descriptive Names for Complex Generics

```aria
// Good: Clear purpose
fn transform<InputData, OutputFormat>(
    data: InputData
) -> OutputFormat { }
```

### ✅ DO: Add Constraints When Needed

```aria
// Good: Clear requirements
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}
```

### ✅ DO: Keep Parameter Count Reasonable

```aria
// Good: 2-3 parameters
fn convert<From, To>(value: From) -> To { }

// OK: 4 parameters if needed
fn process<Input, Output, Error, Config>(...) { }
```

### ❌ DON'T: Use Too Many Parameters

```aria
// Wrong: Too many
fn complex<T, U, V, W, X, Y, Z>(...) { }

// Right: Simplify or use structs
struct Config<T, U> { }
fn complex<V, W>(config: Config<V, W>) { }
```

### ❌ DON'T: Use Generic When Specific Works

```aria
// Wrong: Always i32 anyway
fn add<T>(a: T, b: T) -> T {
    return a + b;
}

// Right: Be specific
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Common Parameter Patterns

### Container Element Type

```aria
struct Vec<T> {
    items: []T
}
```

### Key-Value Pairs

```aria
struct Map<K, V> {
    entries: []Entry<K, V>
}
```

### Input-Output Transform

```aria
fn map<T, U>(array: []T, f: fn(T) -> U) -> []U { }
```

### Error Type Parameter

```aria
enum Result<T, E> {
    Ok(T),
    Err(E)
}
```

---

## Related Topics

- [Generics](generics.md) - Generic overview
- [Generic Functions](generic_functions.md) - Generic functions
- [Generic Syntax](generic_syntax.md) - Complete syntax
- [Type Inference](type_inference.md) - Parameter inference

---

**Remember**: Generic parameters are **type placeholders** - they get replaced with concrete types when you use the generic type or function!
