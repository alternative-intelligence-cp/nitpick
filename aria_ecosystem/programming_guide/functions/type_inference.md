# Type Inference for Generics

**Category**: Functions → Generics  
**Concept**: Compiler automatically determines generic type parameters  
**Philosophy**: Write less, infer more - but stay explicit when it matters

---

## What is Type Inference?

**Type inference** means the compiler figures out what type `T` should be **without you specifying it explicitly**.

---

## Basic Inference

### From Arguments

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// Explicit type
Result: i32 = identity<i32>(42);

// Inferred from argument
Result: i32 = identity(42);  // T inferred as i32
```

### From Return Type

```aria
fn parse<T>(input: string) -> T {
    // Implementation
}

// T inferred from return type annotation
num: i32 = parse("42");      // T = i32
flag: bool = parse("true");  // T = bool
```

### From Context

```aria
fn first<T>(array: []T) -> T {
    return array[0];
}

numbers: []i32 = [1, 2, 3];
value: i32 = first(numbers);  // T inferred as i32
```

---

## Multiple Type Parameters

### All Inferred

```aria
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

// Both T and U inferred
Result: (i32, string) = pair(42, "hello");  // T=i32, U=string
```

### Partial Inference

```aria
fn convert<From, To>(value: From) -> To {
    // Implementation
}

// From inferred, To specified
Result: string = convert::<_, string>(42);  // From=i32, To=string

// Or using turbofish syntax
Result: string = convert::<i32, string>(42);
```

---

## Inference with Constraints

### Trait Bounds Help Inference

```aria
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}

// T inferred as i32
Result: i32 = max(10, 20);

// Compiler knows both arguments must be same type
// This would error:
// result = max(10, 3.14);  // Error: mismatched types
```

---

## Inference Failures

### Ambiguous Types

```aria
fn create<T>() -> T {
    // Create default value
}

// Error: Cannot infer T
// value = create();  // What type?

// Fix: Specify type
value: i32 = create();  // OK: T = i32
```

### Multiple Possibilities

```aria
fn process<T>(f: fn() -> T) -> T {
    return f();
}

// Error: T could be anything
// result = process(|| 42);

// Fix: Annotate return type
Result: i32 = process(|| 42);  // T = i32
```

### No Context

```aria
fn wrap<T>(value: T) -> Box<T> {
    return Box{value: value};
}

// Error: T unknown
// box = wrap(nil);  // nil could be any optional type

// Fix: Specify type
box: Box<i32?> = wrap(nil);  // T = i32?
```

---

## Explicit Type Arguments

### Turbofish Syntax `::<>`

```aria
// When inference fails or you want to be explicit
Result: i32 = parse::<i32>("42");

// Multiple type arguments
Result: (i32, string) = convert::<f64, (i32, string)>(3.14);
```

### Underscore for Partial Inference

```aria
// Infer first, specify second
Result: string = convert::<_, string>(42);  // From inferred, To specified
```

---

## Inference in Collections

### Arrays

```aria
fn first<T>(array: []T) -> T {
    return array[0];
}

// T inferred from array element type
numbers: []i32 = [1, 2, 3];
value: i32 = first(numbers);  // T = i32

// Or inferred from array literal
value: i32 = first([1, 2, 3]);  // T = i32
```

### Maps

```aria
fn get<K, V>(map: Map<K, V>, key: K) -> V? {
    return map.get(key);
}

users: Map<i32, string> = Map::new();

// K=i32, V=string inferred from map type
name: string? = get(users, 42);
```

---

## Method Call Inference

### Self Type

```aria
struct Box<T> {
    value: T
}

impl<T> Box<T> {
    fn get() -> T {
        return self.value;
    }
    
    fn map<U>(f: fn(T) -> U) -> Box<U> {
        return Box{value: f(self.value)};
    }
}

int_box: Box<i32> = Box{value: 42};

// T inferred from int_box type
value: i32 = int_box.get();  // T = i32

// T inferred, U from lambda return type
str_box: Box<string> = int_box.map(|x| format("{}", x));  // U = string
```

---

## Closure Inference

### Parameter Types

```aria
numbers: []i32 = [1, 2, 3];

// Lambda parameter type inferred from map's signature
doubled: []i32 = numbers.map(|x| x * 2);  // x: i32
```

### Return Types

```aria
fn apply<T, U>(value: T, f: fn(T) -> U) -> U {
    return f(value);
}

// T=i32 from argument, U inferred from lambda body
Result: i32 = apply(42, |x| x * 2);  // U = i32
```

---

## Best Practices

### ✅ DO: Let Inference Work

```aria
// Good: Clean and readable
numbers: []i32 = [1, 2, 3];
doubled: []i32 = numbers.map(|x| x * 2);
```

### ✅ DO: Be Explicit When Unclear

```aria
// Good: Explicit when ambiguous
Result: i32 = parse::<i32>("42");

// Better than:
// let result = parse("42");  // What type?
```

### ✅ DO: Use Type Annotations for Clarity

```aria
// Good: Clear intent
users: Map<i32, User> = Map::new();
user: User? = users.get(123);
```

### ❌ DON'T: Over-specify

```aria
// Wrong: Too much annotation
Result: i32 = identity::<i32>(42_i32);

// Right: Let inference work
Result: i32 = identity(42);
```

### ❌ DON'T: Rely on Inference for Complex Types

```aria
// Wrong: Hard to read
let cache = Map::new();
cache.insert(1, User::new());

// Right: Explicit types
cache: Map<i32, User> = Map::new();
cache.insert(1, User::new());
```

---

## Inference Algorithm

The compiler uses **bidirectional type inference**:

### Forward Inference (Argument → Return)

```aria
fn double(x: i32) -> i32 {
    return x * 2;
}

// Flows forward: argument type known
result = double(42);  // Result: i32
```

### Backward Inference (Return → Argument)

```aria
fn parse<T>(input: string) -> T {
    // Implementation
}

// Flows backward: return type known
num: i32 = parse("42");  // T = i32
```

### Constraint Solving

```aria
fn process<T>(a: T, b: T) -> T {
    // Implementation
}

// Constraints:
// 1. a: T
// 2. b: T  
// 3. return: T
// Solution: T = i32 (from arguments)
Result: i32 = process(10, 20);
```

---

## Inference Limitations

### Cannot Infer From Implementation

```aria
fn mystery<T>() -> T {
    // Compiler sees this could return any T
    when true then
        return 42;  // Doesn't help: could be coerced
    end
}

// Still need to specify T
value: i32 = mystery::<i32>();
```

### Cannot Infer Unrelated Types

```aria
fn convert<From, To>(value: From) -> To {
    // No relationship between From and To
}

// Must specify at least one
Result: string = convert::<i32, string>(42);
// Or
Result: string = convert::<_, string>(42);
```

---

## Real-World Examples

### API Response Parsing

```aria
async fn fetch<T>(url: string) -> T? where T: Deserialize {
    response: Response = pass await http_get(url);
    data: T = pass response.json::<T>();
    return data;
}

// T inferred from return type
user: User? = await fetch("/api/user/123");
product: Product? = await fetch("/api/product/456");
```

### Generic Builder

```aria
struct Query<T> {
    sql: string,
    result_type: type(T)
}

impl<T> Query<T> {
    fn execute() -> []T {
        // Execute and return results
    }
}

// Type inferred from variable
users: []User = Query::from("SELECT * FROM users").execute();
```

### Container Operations

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

// All types inferred through chain
Result: i32 = numbers
    .filter(|x| x > 2)    // x: i32 inferred
    .map(|x| x * 2)       // x: i32 inferred
    .reduce(0, |a, b| a + b);  // a, b: i32 inferred
```

---

## Related Topics

- [Generic Functions](generic_functions.md) - Generic function overview
- [Generics](generics.md) - Generic types concept
- [Monomorphization](monomorphization.md) - How generics compile
- [Generic Syntax](generic_syntax.md) - Syntax reference

---

**Remember**: Type inference makes your code **cleaner and more readable** - but don't hesitate to add explicit types when they make intent clearer!
