# Higher-Order Functions

**Category**: Functions → Advanced  
**Definition**: Functions that take functions as parameters or return functions  
**Philosophy**: Functions are first-class citizens - treat them like any other value

---

## What are Higher-Order Functions?

**Higher-order functions** either:
1. **Accept functions as parameters**, or
2. **Return functions as results**, or  
3. **Both**

They enable powerful functional programming patterns.

---

## Functions as Parameters

### Basic Example

```aria
fn apply(value: i32, f: fn(i32) -> i32) -> i32 {
    return f(value);
}

fn double(x: i32) -> i32 {
    return x * 2;
}

fn square(x: i32) -> i32 {
    return x * x;
}

result1: i32 = apply(5, double);  // 10
result2: i32 = apply(5, square);  // 25
```

### With Lambda

```aria
Result: i32 = apply(5, |x| x + 10);  // 15
```

---

## Functions Returning Functions

### Function Factory

```aria
fn make_multiplier(factor: i32) -> fn(i32) -> i32 {
    return |x: i32| x * factor;
}

times_3: fn(i32) -> i32 = make_multiplier(3);
times_10: fn(i32) -> i32 = make_multiplier(10);

result1: i32 = times_3(5);   // 15
result2: i32 = times_10(5);  // 50
```

---

## Classic Higher-Order Functions

### Map - Transform Each Element

```aria
fn map<T, U>(array: []T, f: fn(T) -> U) -> []U {
    Result: []U = [];
    till(array.length - 1, 1) {
        item: T = array[$];
        result.push(f(item));
    }
    return result;
}

numbers: []i32 = [1, 2, 3, 4, 5];
doubled: []i32 = map(numbers, |x| x * 2);
// [2, 4, 6, 8, 10]
```

### Filter - Keep Elements That Match

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

numbers: []i32 = [1, 2, 3, 4, 5, 6];
evens: []i32 = filter(numbers, |x| x % 2 == 0);
// [2, 4, 6]
```

### Reduce - Combine Into Single Value

```aria
fn reduce<T, U>(array: []T, initial: U, f: fn(U, T) -> U) -> U {
    accumulator: U = initial;
    till(array.length - 1, 1) {
        item: T = array[$];
        accumulator = f(accumulator, item);
    }
    return accumulator;
}

numbers: []i32 = [1, 2, 3, 4, 5];
sum: i32 = reduce(numbers, 0, |acc, x| acc + x);
// 15

product: i32 = reduce(numbers, 1, |acc, x| acc * x);
// 120
```

---

## Combining Higher-Order Functions

### Chaining Operations

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Get sum of squares of even numbers
Result: i32 = numbers
    .filter(|x| x % 2 == 0)      // [2, 4, 6, 8, 10]
    .map(|x| x * x)               // [4, 16, 36, 64, 100]
    .reduce(0, |acc, x| acc + x); // 220
```

---

## Common Patterns

### ForEach - Execute for Each Element

```aria
fn for_each<T>(array: []T, action: fn(T)) {
    till(array.length - 1, 1) {
        item: T = array[$];
        action(item);
    }
}

names: []string = ["Alice", "Bob", "Carol"];
for_each(names, |name| {
    stdout << "Hello, " << name << "\n";
});
```

### Any - Check if Any Match

```aria
fn any<T>(array: []T, predicate: fn(T) -> bool) -> bool {
    till(array.length - 1, 1) {
        item: T = array[$];
        when predicate(item) then
            return true;
        end
    }
    return false;
}

numbers: []i32 = [1, 3, 5, 7, 8];
has_even: bool = any(numbers, |x| x % 2 == 0);  // true
```

### All - Check if All Match

```aria
fn all<T>(array: []T, predicate: fn(T) -> bool) -> bool {
    till(array.length - 1, 1) {
        item: T = array[$];
        when !predicate(item) then
            return false;
        end
    }
    return true;
}

numbers: []i32 = [2, 4, 6, 8];
all_even: bool = all(numbers, |x| x % 2 == 0);  // true
```

### Find - Get First Match

```aria
fn find<T>(array: []T, predicate: fn(T) -> bool) -> T? {
    till(array.length - 1, 1) {
        item: T = array[$];
        when predicate(item) then
            return item;
        end
    }
    return nil;
}

numbers: []i32 = [1, 3, 4, 7, 9];
first_even: i32? = find(numbers, |x| x % 2 == 0);  // 4
```

---

## Function Composition

### Compose Two Functions

```aria
fn compose<T, U, V>(f: fn(U) -> V, g: fn(T) -> U) -> fn(T) -> V {
    return |x: T| -> V {
        return f(g(x));
    };
}

add_one: fn(i32) -> i32 = |x| x + 1;
double: fn(i32) -> i32 = |x| x * 2;

// Compose: double then add_one
double_then_add: fn(i32) -> i32 = compose(add_one, double);

Result: i32 = double_then_add(5);  // (5 * 2) + 1 = 11
```

### Pipe - Sequential Application

```aria
fn pipe<T, U, V>(x: T, f: fn(T) -> U, g: fn(U) -> V) -> V {
    return g(f(x));
}

Result: i32 = pipe(5, |x| x * 2, |x| x + 1);  // 11
```

---

## Advanced Patterns

### Currying

```aria
fn curry<A, B, C>(f: fn(A, B) -> C) -> fn(A) -> fn(B) -> C {
    return |a: A| -> fn(B) -> C {
        return |b: B| -> C {
            return f(a, b);
        };
    };
}

fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

curried_add: fn(i32) -> fn(i32) -> i32 = curry(add);
add_5: fn(i32) -> i32 = curried_add(5);

Result: i32 = add_5(3);  // 8
```

### Partial Application

```aria
fn partial<A, B, C>(f: fn(A, B) -> C, a: A) -> fn(B) -> C {
    return |b: B| -> C {
        return f(a, b);
    };
}

fn multiply(a: i32, b: i32) -> i32 {
    return a * b;
}

times_10: fn(i32) -> i32 = partial(multiply, 10);
Result: i32 = times_10(5);  // 50
```

### Memoization

```aria
fn memoize<T, U>(f: fn(T) -> U) -> fn(T) -> U where T: Hashable {
    cache: Map<T, U> = Map::new();
    
    return |arg: T| -> U {
        when cache.contains_key(arg) then
            return cache.get(arg);
        end
        
        Result: U = f(arg);
        cache.insert(arg, result);
        return result;
    };
}

expensive: fn(i32) -> i32 = |n| {
    stddbg << "Calculating for " << n << "\n";
    return n * n;
};

cached: fn(i32) -> i32 = memoize(expensive);

result1: i32 = cached(5);  // Prints "Calculating for 5", returns 25
result2: i32 = cached(5);  // Returns 25 (from cache, no print)
```

---

## Real-World Examples

### Retry Logic

```aria
fn retry<T>(f: fn() -> T?, max_attempts: i32) -> T? {
    attempts: i32 = 0;
    
    while attempts < max_attempts {
        Result: T? = f();
        when result != ERR then
            return result;
        end
        
        attempts = attempts + 1;
        stddbg << "Retry " << attempts << "/" << max_attempts << "\n";
    }
    
    return ERR;
}

// Usage
data: string? = retry(|| fetch_from_api(), 3);
```

### Pipeline Processing

```aria
fn pipeline<T>(value: T, steps: []fn(T) -> T) -> T {
    Result: T = value;
    till(steps.length - 1, 1) {
        step: fn(T) -> T = steps[$];
        result = step(result);
    }
    return result;
}

// Data transformation pipeline
text: string = "  Hello World  ";
processed: string = pipeline(text, [
    |s| s.trim(),
    |s| s.lowercase(),
    |s| s.replace(" ", "_")
]);
// "hello_world"
```

### Validation Chain

```aria
fn validate<T>(value: T, validators: []fn(T) -> bool) -> bool {
    till(validators.length - 1, 1) {
        validator: fn(T) -> bool = validators[$];
        when !validator(value) then
            return false;
        end
    }
    return true;
}

age: i32 = 25;
is_valid: bool = validate(age, [
    |x| x >= 0,       // Non-negative
    |x| x <= 120,     // Reasonable max
    |x| x >= 18       // Adult
]);
```

### Event Dispatcher

```aria
struct EventDispatcher<T> {
    handlers: []fn(T)
}

impl<T> EventDispatcher<T> {
    fn new() -> EventDispatcher<T> {
        return EventDispatcher{handlers: []};
    }
    
    fn on(handler: fn(T)) {
        self.handlers.push(handler);
    }
    
    fn emit(event: T) {
        till(self.handlers.length - 1, 1) {
            handler: fn(T) = self.handlers[$];
            handler(event);
        }
    }
}

// Usage
dispatcher: EventDispatcher<string> = EventDispatcher::new();

dispatcher.on(|msg| stdout << "Log: " << msg << "\n");
dispatcher.on(|msg| stddbg << "Debug: " << msg << "\n");
dispatcher.on(|msg| save_to_file(msg));

dispatcher.emit("User logged in");
// Calls all three handlers
```

### Sort with Custom Comparator

```aria
fn sort<T>(array: []T, compare: fn(T, T) -> i32) -> []T {
    // compare returns: -1 if a < b, 0 if a == b, 1 if a > b
    sorted: []T = array.copy();
    
    // Quick sort implementation
    // ... uses compare function for ordering
    
    return sorted;
}

users: []User = [...];

// Sort by age
by_age: []User = sort(users, |a, b| a.age - b.age);

// Sort by name (descending)
by_name: []User = sort(users, |a, b| b.name.compare(a.name));
```

---

## Best Practices

### ✅ DO: Use Higher-Order Functions for Data Processing

```aria
// Good: Clear, declarative
scores: []i32 = [85, 92, 78, 95, 88];
passing: []i32 = scores
    .filter(|x| x >= 80)
    .map(|x| x + 5);  // Bonus points
```

### ✅ DO: Keep Functions Small and Focused

```aria
// Good: Single responsibility
fn is_positive(x: i32) -> bool { return x > 0; }
fn double(x: i32) -> i32 { return x * 2; }

positives: []i32 = numbers
    .filter(is_positive)
    .map(double);
```

### ✅ DO: Use Type Annotations for Clarity

```aria
// Good: Clear parameter types
fn process(items: []string, transform: fn(string) -> string) -> []string {
    return items.map(transform);
}
```

### ❌ DON'T: Overuse Nested Lambdas

```aria
// Wrong: Hard to read
Result: []i32 = numbers.map(|x| {
    filter(x, |y| {
        when y > 0 then
            return transform(y, |z| z * 2);
        end
    });
});

// Right: Extract functions
is_positive: fn(i32) -> bool = |x| x > 0;
double: fn(i32) -> i32 = |x| x * 2;

Result: []i32 = numbers
    .filter(is_positive)
    .map(double);
```

### ❌ DON'T: Sacrifice Performance Unnecessarily

```aria
// Wrong: Creates intermediate arrays
Result: []i32 = numbers
    .map(|x| x * 2)
    .filter(|x| x > 10)
    .map(|x| x + 1);

// Better: Combine operations when possible
Result: []i32 = numbers.map(|x| {
    doubled: i32 = x * 2;
    when doubled > 10 then
        return doubled + 1;
    end
    return ERR;  // Filtered out
}).filter(|x| x != ERR);
```

---

## Related Topics

- [Lambda Expressions](lambda.md) - Anonymous function syntax
- [Closures](closures.md) - Functions capturing environment
- [Generic Functions](generic_functions.md) - Type-generic higher-order functions
- [Function Declaration](function_declaration.md) - Regular functions

---

**Remember**: Higher-order functions make your code **declarative** - you say WHAT you want, not HOW to do it. They're the foundation of functional programming in Aria!
