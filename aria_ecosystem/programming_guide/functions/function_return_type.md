# Function Return Types

**Category**: Functions → Syntax  
**Syntax**: `-> ReturnType`  
**Purpose**: Specify what a function returns

---

## Basic Return Type Syntax

```aria
fn function_name() -> ReturnType {
    return value;
}
```

---

## No Return Type (Unit)

Functions with no return type implicitly return "unit" (like `void`):

```aria
fn print_message() {
    stdout << "Hello, World!\n";
}

// Equivalent to:
fn print_message() -> () {
    stdout << "Hello, World!\n";
}
```

---

## Primitive Return Types

```aria
// Integer
fn get_age() -> i32 {
    return 25;
}

// Float
fn get_price() -> f64 {
    return 19.99;
}

// Boolean
fn is_valid() -> bool {
    return true;
}

// Character
fn get_grade() -> char {
    return 'A';
}

// String
fn get_name() -> string {
    return "Alice";
}
```

---

## Optional Return Types

Use `?` suffix for functions that might fail:

```aria
// Returns i32 or ERR
fn parse_number(input: string) -> i32? {
    when input == "" then
        return ERR;
    end
    return input.parse();
}

// Usage
Result: i32? = parse_number("42");
when result == ERR then
    stderr << "Parse failed\n";
else
    stdout << "Number: " << result << "\n";
end
```

---

## Struct Return Types

```aria
struct User {
    name: string,
    age: i32
}

fn create_user(name: string, age: i32) -> User {
    return User{name: name, age: age};
}

// Usage
user: User = create_user("Alice", 30);
```

---

## Tuple Return Types

Return multiple values:

```aria
fn divide_with_remainder(a: i32, b: i32) -> (i32, i32) {
    quotient: i32 = a / b;
    remainder: i32 = a % b;
    return (quotient, remainder);
}

// Usage
(quot, rem): (i32, i32) = divide_with_remainder(17, 5);
stdout << quot;  // 3
stdout << rem;   // 2
```

---

## Array Return Types

```aria
fn get_primes() -> []i32 {
    return [2, 3, 5, 7, 11];
}

// Usage
primes: []i32 = get_primes();
```

---

## Enum Return Types

```aria
enum Status {
    Success,
    Error(string),
    Pending
}

fn check_status() -> Status {
    return Status::Success;
}
```

---

## Generic Return Types

```aria
fn identity<T>(value: T) -> T {
    return value;
}

// T inferred from usage
num: i32 = identity(42);      // Returns i32
text: string = identity("hi"); // Returns string
```

---

## Function Return Types

Functions can return other functions:

```aria
fn make_multiplier(factor: i32) -> fn(i32) -> i32 {
    return |x| x * factor;
}

// Usage
times_3: fn(i32) -> i32 = make_multiplier(3);
Result: i32 = times_3(5);  // 15
```

---

## Async Return Types

```aria
async fn fetch_data() -> Data {
    return await http_get("/api/data");
}

// Returns Future<Data>
// Must await to get Data
data: Data = await fetch_data();
```

---

## Optional with Generics

```aria
fn find<T>(array: []T, target: T) -> T? {
    till(array.length - 1, 1) {
        when array[$] == target then
            return array[$];
        end
    }
    return nil;
}

// Usage
numbers: []i32 = [1, 2, 3, 4, 5];
found: i32? = find(numbers, 3);  // Returns 3
not_found: i32? = find(numbers, 99);  // Returns ERR
```

---

## Complex Return Types

```aria
// Result type (custom)
enum Result<T, E> {
    Ok(T),
    Err(E)
}

func:safe_divide = Result<int32, string>(int32:a, int32:b) {
    when b == 0 then
        fail("Division by zero");
    end
    pass(a / b);
}
```

---

## Return Statement

### Explicit Return

```aria
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### Multiple Returns

```aria
fn abs(x: i32) -> i32 {
    when x < 0 then
        return -x;
    else
        return x;
    end
}
```

### Early Return

```aria
fn validate_age(age: i32) -> bool {
    when age < 0 then
        return false;  // Early exit
    end
    
    when age > 120 then
        return false;  // Early exit
    end
    
    return true;
}
```

---

## Return Type Inference (Limited)

Aria requires **explicit return types** on functions:

```aria
// ❌ Wrong: No return type inference for functions
// fn add(a: i32, b: i32) {
//     return a + b;
// }

// ✅ Right: Explicit return type
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

**Exception**: Lambdas can infer return types:

```aria
// Lambda: Return type inferred
add = |a, b| a + b;

// Function: Must specify
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Never Return Type

For functions that never return:

```aria
fn panic(message: string) -> ! {
    stderr << "PANIC: " << message << "\n";
    exit(1);
    // Never reaches here
}

fn infinite_loop() -> ! {
    while true {
        process_events();
    }
    // Never returns
}
```

---

## Unit Return Type (Explicit)

```aria
// Implicit unit
fn log(message: string) {
    stdout << message << "\n";
}

// Explicit unit
fn log(message: string) -> () {
    stdout << message << "\n";
    return ();
}
```

---

## Best Practices

### ✅ DO: Use Descriptive Return Types

```aria
// Good: Clear what's returned
fn get_user_count() -> i32 { }
fn is_authenticated() -> bool { }
fn load_config() -> Config { }
```

### ✅ DO: Use Option for Failable Operations

```aria
// Good: Clear it might fail
fn find_user(id: i32) -> User? {
    // Returns User or ERR
}
```

### ✅ DO: Use Tuples for Multiple Related Values

```aria
// Good: Related values
fn get_dimensions() -> (i32, i32) {
    return (width, height);
}
```

### ✅ DO: Use Struct for Multiple Unrelated Values

```aria
// Better: Named fields
struct UserInfo {
    name: string,
    age: i32,
    email: string
}

fn get_user_info() -> UserInfo {
    return UserInfo{...};
}
```

### ❌ DON'T: Return Complex Tuples

```aria
// Wrong: Hard to remember order
fn get_stats() -> (i32, i32, f64, bool, string) {
    return (count, total, avg, active, status);
}

// Right: Use struct
struct Stats {
    count: i32,
    total: i32,
    avg: f64,
    active: bool,
    status: string
}

fn get_stats() -> Stats { }
```

### ❌ DON'T: Forget Return Statement

```aria
// Wrong: Missing return
fn add(a: i32, b: i32) -> i32 {
    sum: i32 = a + b;
    // Error: No return statement
}

// Right: Always return
fn add(a: i32, b: i32) -> i32 {
    sum: i32 = a + b;
    return sum;
}
```

---

## Common Patterns

### Factory Functions

```aria
fn new_user(name: string) -> User {
    return User{
        name: name,
        created_at: Time::now(),
        id: generate_id()
    };
}
```

### Conversion Functions

```aria
fn to_string(value: i32) -> string {
    return format("{}", value);
}

fn from_string(s: string) -> i32? {
    return s.parse();
}
```

### Validation Functions

```aria
fn validate_email(email: string) -> bool {
    return email.contains("@") and email.contains(".");
}
```

### Builder Pattern

```aria
fn build() -> Config {
    return Config{
        host: self.host,
        port: self.port,
        timeout: self.timeout
    };
}
```

---

## Related Topics

- [Function Declaration](function_declaration.md) - Function basics
- [Function Syntax](function_syntax.md) - Complete syntax
- [Pass Keyword](pass_keyword.md) - Handling optional returns
- [Fail Keyword](fail_keyword.md) - Error handling

---

**Remember**: Aria requires **explicit return types** for all functions (except lambdas). Be clear about what your function returns!
