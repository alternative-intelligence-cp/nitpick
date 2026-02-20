# Generic Functions

**Category**: Functions → Generics  
**Syntax**: `fn name<T>(param: T) -> T`  
**Philosophy**: Write once, use with any type - safely

---

## What are Generic Functions?

**Generic functions** work with **any type** instead of a specific type. They let you write code once that works for `i32`, `string`, `User`, or any other type.

---

## Basic Syntax

```aria
// Generic function with type parameter T
fn identity<T>(value: T) -> T {
    return value;
}

// Use with i32
num: i32 = identity<i32>(42);

// Use with string
text: string = identity<string>("hello");
```

---

## Type Parameters

### Single Type Parameter

```aria
fn first<T>(array: []T) -> T {
    return array[0];
}

numbers: []i32 = [1, 2, 3];
first_num: i32 = first<i32>(numbers);  // 1

names: []string = ["Alice", "Bob"];
first_name: string = first<string>(names);  // "Alice"
```

### Multiple Type Parameters

```aria
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

// i32 and string
Result: (i32, string) = pair<i32, string>(42, "answer");

// string and bool
flag: (string, bool) = pair<string, bool>("active", true);
```

---

## Type Inference

The compiler can often **infer** generic types from arguments:

```aria
fn swap<T>(a: T, b: T) -> (T, T) {
    return (b, a);
}

// Explicit types
Result: (i32, i32) = swap<i32>(1, 2);

// Inferred from arguments
Result: (i32, i32) = swap(1, 2);  // T inferred as i32
```

---

## Common Generic Functions

### Container Operations

```aria
// Generic array length
fn len<T>(array: []T) -> usize {
    return array.length();
}

// Generic array reverse
fn reverse<T>(array: []T) -> []T {
    reversed: []T = [];
    i = array.len() - 1;
    till(array.len() - 1, 1) {
        reversed.push(array[i - $]);
    }
    return reversed;
}
```

### Option/Result Handling

```aria
// Generic unwrap with default
fn unwrap_or<T>(value: T?, default: T) -> T {
    when value == ERR then
        return default;
    end
    return value;
}

// Usage
num: tbb32? = parse("42");
Result: tbb32 = unwrap_or(num, 0);  // 42 or 0 if parse failed
```

### Comparison

```aria
// Generic max function
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then
        return a;
    else
        return b;
    end
}

largest: i32 = max(10, 20);  // 20
biggest: f64 = max(3.14, 2.71);  // 3.14
```

---

## Type Constraints

### Trait Bounds

```aria
// T must implement Display trait
fn print_value<T>(value: T) where T: Display {
    stdout << value << "\n";
}

// T must implement Comparable
fn is_sorted<T>(array: []T) -> bool where T: Comparable {
    till(array.len() - 2, 1) {
        i: i32 = $;
        when array[i] > array[i + 1] then
            return false;
        end
    }
    return true;
}
```

### Multiple Constraints

```aria
// T must be both Comparable and Hashable
fn unique_sorted<T>(array: []T) -> []T 
    where T: Comparable, T: Hashable {
    
    seen: Set<T> = Set::new();
    unique: []T = [];
    
    till(array.length - 1, 1) {
        item: T = array[$];
        when !seen.contains(item) then
            seen.insert(item);
            unique.push(item);
        end
    }
    
    return sort(unique);
}
```

---

## Generic Structs with Generic Methods

```aria
struct Box<T> {
    value: T
}

impl<T> Box<T> {
    // Generic method on generic type
    fn new(value: T) -> Box<T> {
        return Box{value: value};
    }
    
    fn get() -> T {
        return self.value;
    }
    
    fn set(new_value: T) {
        self.value = new_value;
    }
    
    // Method with additional type parameter
    fn map<U>(f: fn(T) -> U) -> Box<U> {
        return Box<U>{value: f(self.value)};
    }
}

// Usage
int_box: Box<i32> = Box::new(42);
string_box: Box<string> = int_box.map(|x: i32| -> string {
    return format("{}", x);
});
```

---

## Monomorphization

Aria uses **monomorphization** - the compiler generates a separate version of the generic function for each type used:

```aria
fn add<T>(a: T, b: T) -> T {
    return a + b;
}

// Compiler generates:
// fn add_i32(a: i32, b: i32) -> i32 { return a + b; }
// fn add_f64(a: f64, b: f64) -> f64 { return a + b; }

x: i32 = add(1, 2);      // Calls add_i32
y: f64 = add(1.5, 2.5);  // Calls add_f64
```

**Benefits**:
- **Zero runtime overhead** - as fast as hand-written specific functions
- **No dynamic dispatch** - direct function calls
- **Full optimization** - compiler optimizes each version

**Trade-off**:
- **Larger binary size** - each type gets its own copy of the function

---

## Advanced Patterns

### Generic Builder Pattern

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

// Usage
numbers: []i32 = Builder<i32>::new()
    .add(1)
    .add(2)
    .add(3)
    .build();
```

### Generic Iterator

```aria
fn filter<T>(array: []T, predicate: fn(T) -> bool) -> []T {
    Result: []T = [];
    till(array.length - 1, 1) {
        item: T = array[$];
        when predicate(item) then
            result.push(item);
        end
    }
    return result;
}

fn map<T, U>(array: []T, transform: fn(T) -> U) -> []U {
    Result: []U = [];
    till(array.length - 1, 1) {
        item: T = array[$];
        result.push(transform(item));
    }
    return result;
}

// Usage
numbers: []i32 = [1, 2, 3, 4, 5];
evens: []i32 = filter(numbers, |x| x % 2 == 0);
doubled: []i32 = map(evens, |x| x * 2);
```

---

## Best Practices

### ✅ DO: Use Generics for Container Operations

```aria
// Good: Generic, reusable
fn last<T>(array: []T) -> T {
    return array[array.len() - 1];
}
```

### ✅ DO: Add Type Constraints When Needed

```aria
// Good: Clear requirements
fn sum<T>(array: []T) -> T where T: Numeric {
    total: T = 0;
    till(array.length - 1, 1) {
        item: T = array[$];
        total = total + item;
    }
    return total;
}
```

### ✅ DO: Use Descriptive Type Parameter Names

```aria
// Good for single type
fn wrap<T>(value: T) -> Box<T> { ... }

// Good for multiple types
fn convert<Input, Output>(value: Input) -> Output { ... }

// Good for specific domains
fn store<Key, Value>(key: Key, value: Value) { ... }
```

### ❌ DON'T: Overuse Generics

```aria
// Wrong: Unnecessarily generic
fn print_number<T>(x: T) {  // Only works with numbers anyway!
    stdout << x << "\n";
}

// Right: Be specific when you can
fn print_number(x: i32) {
    stdout << x << "\n";
}
```

### ❌ DON'T: Forget Type Constraints

```aria
// Wrong: Won't compile - T might not support +
fn add<T>(a: T, b: T) -> T {
    return a + b;  // Error if T isn't numeric
}

// Right: Add constraint
fn add<T>(a: T, b: T) -> T where T: Add {
    return a + b;
}
```

---

## Real-World Examples

### Generic Cache

```aria
struct Cache<K, V> {
    data: Map<K, V>
}

impl<K, V> Cache<K, V> where K: Hashable {
    fn new() -> Cache<K, V> {
        return Cache{data: Map::new()};
    }
    
    fn get(key: K) -> V? {
        return self.data.get(key);
    }
    
    fn set(key: K, value: V) {
        self.data.insert(key, value);
    }
    
    fn has(key: K) -> bool {
        return self.data.contains_key(key);
    }
}

// Usage with different types
user_cache: Cache<i32, User> = Cache::new();
session_cache: Cache<string, Session> = Cache::new();
```

### Generic Result Type

```aria
enum Result<T, E> {
    Ok(T),
    Err(E)
}

impl<T, E> Result<T, E> {
    fn is_ok() -> bool {
        return match self {
            Ok(_) => true,
            Err(_) => false,
        };
    }
    
    fn unwrap() -> T {
        return match self {
            Ok(value) => value,
            Err(err) => panic("Called unwrap on Err"),
        };
    }
    
    fn map<U>(f: fn(T) -> U) -> Result<U, E> {
        return match self {
            Ok(value) => Ok(f(value)),
            Err(err) => Err(err),
        };
    }
}
```

### Generic API Client

```aria
struct ApiClient<T> {
    base_url: string,
    response_type: type(T)
}

impl<T> ApiClient<T> where T: Deserializable {
    fn new(base_url: string) -> ApiClient<T> {
        return ApiClient{base_url: base_url, response_type: T};
    }
    
    async fn get(endpoint: string) -> T? {
        url: string = self.base_url + endpoint;
        response: Response = pass await http_get(url);
        data: T = pass response.deserialize<T>();
        return data;
    }
    
    async fn post(endpoint: string, payload: T) -> T? {
        url: string = self.base_url + endpoint;
        response: Response = pass await http_post(url, payload);
        data: T = pass response.deserialize<T>();
        return data;
    }
}

// Usage
user_api: ApiClient<User> = ApiClient::new("https://api.example.com");
user: User = await user_api.get("/users/123");
```

---

## Generic Type Aliases

```aria
// Create aliases for common generic combinations
type IntPair = (i32, i32);
type StringMap<T> = Map<string, T>;
type Result<T> = Result<T, Error>;

// Usage
coords: IntPair = (10, 20);
config: StringMap<string> = Map::new();
data: Result<Data> = fetch_data();
```

---

## Related Topics

- [Generic Syntax](generic_syntax.md) - Detailed syntax reference
- [Monomorphization](monomorphization.md) - How generics compile
- [Generic Structs](generic_structs.md) - Generic data structures
- [Type Inference](type_inference.md) - Type parameter inference
- [Generic Star Prefix](generic_star_prefix.md) - Alternative syntax

---

**Remember**: Generics let you **write once, use everywhere** - but the compiler generates optimized code for each type. Zero abstraction overhead!
