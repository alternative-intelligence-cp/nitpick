# Function Syntax Reference

**Category**: Functions → Reference  
**Purpose**: Complete syntax reference for function declarations

---

## Basic Function Declaration

```aria
fn function_name(param1: Type1, param2: Type2) -> ReturnType {
    // Function body
    return value;
}
```

---

## Components

### Function Keyword

```aria
// Short form (common)
fn name() { }

// Long form (rare)
func name() { }
```

### Function Name

```aria
// Convention: snake_case
fn calculate_total() { }
fn get_user_by_id() { }

// Single word
fn process() { }
fn validate() { }
```

### Parameters

```aria
// No parameters
fn greet() { }

// Single parameter
fn double(x: i32) -> i32 { }

// Multiple parameters
fn add(a: i32, b: i32) -> i32 { }

// Different types
fn format_user(id: i32, name: string, active: bool) -> string { }
```

### Return Type

```aria
// No return (implicitly returns unit/void)
fn print_message() {
    stdout << "Message\n";
}

// Explicit return type
fn calculate() -> i32 {
    return 42;
}

// Optional return
fn might_fail() -> Data? {
    return ERR;
}
```

---

## Generic Functions

```aria
// Single type parameter
fn identity<T>(value: T) -> T {
    return value;
}

// Multiple type parameters
fn pair<T, U>(first: T, second: U) -> (T, U) {
    return (first, second);
}

// With constraints
fn max<T>(a: T, b: T) -> T where T: Comparable {
    when a > b then return a; else return b; end
}

// Multiple constraints
fn process<T>(value: T) -> T 
    where T: Display, T: Clone {
    // Implementation
}
```

---

## Async Functions

```aria
// Basic async
async fn fetch_data() -> Data {
    return await http_get("/api/data");
}

// Async with parameters
async fn fetch_user(id: i32) -> User? {
    return await http_get("/api/user/" + format("{}", id));
}

// Async generic
async fn fetch<T>(url: string) -> T? where T: Deserialize {
    response: Response = await http_get(url);
    return response.deserialize();
}
```

---

## Method Syntax

### Instance Methods

```aria
struct User {
    name: string,
    age: i32
}

impl User {
    // Instance method (has self)
    fn greet() {
        stdout << "Hello, " << self.name << "\n";
    }
    
    // Mutable instance method
    fn grow_older() {
        self.age = self.age + 1;
    }
}
```

### Associated Functions (Static)

```aria
impl User {
    // No self - associated function
    fn new(name: string, age: i32) -> User {
        return User{name: name, age: age};
    }
}

// Called with ::
user: User = User::new("Alice", 30);
```

---

## Lambda/Closure Syntax

```aria
// Basic lambda
add: fn(i32, i32) -> i32 = |a, b| a + b;

// With type annotations
add: fn(i32, i32) -> i32 = |a: i32, b: i32| -> i32 {
    return a + b;
};

// Multi-line body
process: fn(i32) -> i32 = |x| {
    Result: i32 = x * 2;
    result = result + 1;
    return result;
};

// Closure (captures variables)
counter: fn() -> i32 = || {
    count: i32 = 0;
    $count = $count + 1;
    return $count;
};
```

---

## Function Types

### As Variable Type

```aria
// Function that takes i32, returns i32
callback: fn(i32) -> i32 = double;

// Function that takes nothing, returns string
getter: fn() -> string = get_name;

// Multiple parameters
processor: fn(i32, string, bool) -> Result = process_data;
```

### As Parameter Type

```aria
fn apply(value: i32, f: fn(i32) -> i32) -> i32 {
    return f(value);
}

fn for_each<T>(array: []T, action: fn(T)) {
    till(array.length - 1, 1) {
        action(array[$]);
    }
}
```

### As Return Type

```aria
fn make_multiplier(factor: i32) -> fn(i32) -> i32 {
    return |x| x * factor;
}
```

---

## Visibility Modifiers

```aria
// Public (default in Aria for functions)
pub fn public_function() { }

// Private (only visible in current module)
fn private_function() { }

// Package-private
pub(package) fn package_function() { }
```

---

## Complete Examples

### Simple Function

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### Generic Function

```aria
fn swap<T>(a: T, b: T) -> (T, T) {
    return (b, a);
}
```

### Async Function

```aria
async fn fetch_user(id: i32) -> User? {
    response: Response = pass await http_get("/api/user/" + format("{}", id));
    user: User = pass response.deserialize();
    return user;
}
```

### Method

```aria
struct Calculator;

impl Calculator {
    fn add(a: i32, b: i32) -> i32 {
        return a + b;
    }
}
```

### Higher-Order Function

```aria
fn map<T, U>(array: []T, f: fn(T) -> U) -> []U {
    Result: []U = [];
    till(array.length - 1, 1) {
        result.push(f(array[$]));
    }
    return result;
}
```

### Complex Generic with Constraints

```aria
async fn fetch_and_process<T, U>(
    url: string,
    transform: fn(T) -> U
) -> U? 
    where T: Deserialize, 
          U: Serialize {
    
    response: Response = pass await http_get(url);
    data: T = pass response.deserialize();
    Result: U = transform(data);
    return result;
}
```

---

## Special Syntax

### Default Parameters (Not Supported)

```aria
// ❌ Wrong: Aria doesn't have default parameters
// fn greet(name: string = "World") { }

// ✅ Right: Use function overloading or separate functions
fn greet() {
    greet_name("World");
}

fn greet_name(name: string) {
    stdout << "Hello, " << name << "\n";
}
```

### Variadic Functions (Not Supported)

```aria
// ❌ Wrong: Aria doesn't have variadic functions
// fn sum(numbers: ...i32) -> i32 { }

// ✅ Right: Use array
fn sum(numbers: []i32) -> i32 {
    total: i32 = 0;
    till(numbers.length - 1, 1) {
        total = total + numbers[$];
    }
    return total;
}
```

### Named Arguments (Not Supported)

```aria
// ❌ Wrong: No named arguments
// user = create_user(name: "Alice", age: 30);

// ✅ Right: Positional arguments
user = create_user("Alice", 30);
```

---

## Calling Convention

```aria
// Regular call
Result: i32 = add(5, 10);

// Method call
user.greet();

// Associated function call
user: User = User::new("Alice", 30);

// Generic function call (explicit)
value: i32 = identity::<i32>(42);

// Generic function call (inferred)
value: i32 = identity(42);

// Async function call
user: User = await fetch_user(123);

// Lambda call
Result: i32 = callback(42);
```

---

## Related Topics

- [Function Declaration](function_declaration.md) - Function basics
- [Function Parameters](function_params.md) - Parameter details
- [Function Return Types](function_return_type.md) - Return type details
- [Generic Syntax](generic_syntax.md) - Generic syntax details
- [Lambda Syntax](lambda_syntax.md) - Lambda syntax details

---

**Remember**: Aria's function syntax is **explicit and consistent** - always declare parameter types and return types!
