# Borrowing

**Category**: Memory Model → References  
**Concept**: Temporary access without ownership  
**Philosophy**: Safe sharing through rules

---

## What is Borrowing?

**Borrowing** is taking a **temporary reference** to data without taking ownership, allowing safe sharing of data.

---

## Basic Concept

```aria
fn main() {
    value: i32 = 42;
    
    print_value(&value);  // Borrow value
    
    stdout << value;  // Still owns value
}

fn print_value(v: &i32) {
    stdout << v << "\n";  // Temporary access
}  // Borrow ends here
```

---

## Ownership vs Borrowing

### Ownership (Transfer)

```aria
fn take_ownership(s: string) {
    stdout << s;
}  // s dropped here

value: string = "hello";
take_ownership(value);
stdout << value;  // ❌ Error: value moved
```

### Borrowing (Reference)

```aria
fn borrow(s: &string) {
    stdout << s;
}  // Borrow ends, s still valid

value: string = "hello";
borrow(&value);
stdout << value;  // ✅ OK: still owns value
```

---

## Immutable Borrowing

Default borrows are **read-only**:

```aria
value: i32 = 42;
ref: &i32 = &value;

stdout << ref;   // ✅ Can read
ref = 100;       // ❌ Can't modify
```

---

## Mutable Borrowing

Use `$` for mutable borrows:

```aria
value: i32 = 42;
ref: &i32 = $value;  // Mutable borrow

ref = 100;  // ✅ Can modify through reference

stdout << value;  // 100
```

---

## Borrowing Rules

### Rule 1: Multiple Immutable Borrows OK

```aria
value: i32 = 42;

ref1: &i32 = &value;  // ✅ OK
ref2: &i32 = &value;  // ✅ OK
ref3: &i32 = &value;  // ✅ OK

stdout << ref1 << ref2 << ref3;  // All can read
```

### Rule 2: Only One Mutable Borrow

```aria
value: i32 = 42;

mut_ref1: &i32 = $value;  // ✅ OK
mut_ref2: &i32 = $value;  // ❌ Error: already borrowed mutably
```

### Rule 3: Can't Mix Immutable and Mutable

```aria
value: i32 = 42;

ref: &i32 = &value;      // Immutable borrow
mut_ref: &i32 = $value;  // ❌ Error: can't borrow mutably while immutably borrowed
```

---

## Why These Rules?

### Prevent Data Races

```aria
// Without rules (unsafe):
value: i32 = 42;
ref1: &i32 = &value;
ref2: &i32 = $value;  // Danger!

// Thread 1 reads ref1
// Thread 2 modifies through ref2
// Data race!

// With rules (safe):
// Can't have mutable and immutable borrows simultaneously
```

### Prevent Iterator Invalidation

```aria
// Without rules (unsafe):
vec: Vec<i32> = vec![1, 2, 3];
till(vec.length - 1, 1) {     // Iterate over vec
    vec.push(4);              // ❌ Modify while iterating
    // Iterator now invalid!
}

// With rules (safe):
// Can't modify while borrowed, iterator stays valid
```

---

## Borrow Scope

Borrows last until their **last use**:

```aria
value: i32 = 42;

{
    ref: &i32 = &value;  // Borrow starts
    stdout << ref;       // Last use of ref
}  // Borrow ends

// value can be borrowed again
mut_ref: &i32 = $value;  // ✅ OK
```

---

## Function Parameters

### By Value (Takes Ownership)

```aria
fn consume(s: string) {
    stdout << s;
}  // s dropped

text: string = "hello";
consume(text);
// text no longer valid
```

### By Reference (Borrows)

```aria
fn display(s: &string) {
    stdout << s;
}  // Borrow ends

text: string = "hello";
display(&text);  // Borrow
// text still valid
```

### Mutable Reference

```aria
fn modify(s: &string) {
    s.push_str(" world");
}

text: string = "hello";
modify($text);  // Mutable borrow
stdout << text;  // "hello world"
```

---

## Returning References

Can return references if they **outlive the function**:

```aria
// ✅ OK: Reference to parameter
fn first(arr: &[]i32) -> &i32 {
    return &arr[0];
}

// ❌ Error: Reference to local
fn create() -> &i32 {
    x: i32 = 42;
    return &x;  // x freed at end of function!
}
```

---

## Common Patterns

### Read-Only Access

```aria
fn get_length(s: &string) -> i32 {
    return s.length();  // Read only
}

text: string = "hello";
len: i32 = get_length(&text);
```

### Modify in Place

```aria
fn double(value: &i32) {
    value = value * 2;
}

x: i32 = 21;
double($x);
stdout << x;  // 42
```

### Multiple Readers

```aria
fn compare(a: &i32, b: &i32) -> bool {
    return a < b;
}

x: i32 = 10;
y: i32 = 20;

// Both borrowed simultaneously
Result: bool = compare(&x, &y);
```

---

## Borrow Checker

Aria's **borrow checker** enforces these rules at compile time:

```aria
// ❌ Compile error caught early
value: i32 = 42;
ref1: &i32 = &value;
ref2: &i32 = $value;  // Error: can't borrow mutably while immutably borrowed

// vs C++ (runtime error)
// Compiles but crashes at runtime!
```

---

## Best Practices

### ✅ DO: Prefer Borrowing

```aria
// Good: Borrow when you don't need ownership
fn process(data: &Data) {
    analyze(data);
}
```

### ✅ DO: Use Immutable by Default

```aria
// Good: Immutable unless mutation needed
fn read(s: &string) { }     // Read-only
fn modify(s: &string) { }   // Needs mutation
```

### ✅ DO: Keep Borrows Short

```aria
// Good: Short borrow scope
{
    ref: &i32 = &value;
    stdout << ref;
}  // Borrow ends

// Can now modify value
value = 100;
```

### ❌ DON'T: Hold Borrows Too Long

```aria
// Wrong: Borrow held too long
ref: &i32 = &value;

// Lots of code...

value = 100;  // ❌ Error: can't modify while borrowed
stdout << ref;  // Still using borrow
```

### ❌ DON'T: Return References to Locals

```aria
// Wrong: Dangling reference
fn bad() -> &i32 {
    x: i32 = 42;
    return &x;  // ❌ x freed at end
}

// Right: Return value
fn good() -> i32 {
    x: i32 = 42;
    return x;  // Copied
}
```

---

## Real-World Examples

### Configuration Reader

```aria
struct Config {
    host: string,
    port: i32
}

fn get_connection_string(config: &Config) -> string {
    // Borrows config, doesn't take ownership
    return format("{}:{}", config.host, config.port);
}

config: Config = load_config();
conn_str: string = get_connection_string(&config);
// config still available
```

### Collection Processing

```aria
fn sum(numbers: &[]i32) -> i32 {
    total: i32 = 0;
    till(numbers.length - 1, 1) {
        total = total + numbers[$];
    }
    return total;
}

values: []i32 = [1, 2, 3, 4, 5];
Result: i32 = sum(&values);
// values still usable
```

### In-Place Update

```aria
fn apply_discount(price: &f64, discount: f64) {
    price = price * (1.0 - discount);
}

item_price: f64 = 100.0;
apply_discount($item_price, 0.2);
stdout << item_price;  // 80.0
```

---

## Borrowing in Loops

```aria
items: []Item = load_items();

// Immutable borrow
till(items.length - 1, 1) {
    stdout << items[$].name;  // Read only
}

// Mutable borrow - using $ for mutable index
till(items.length - 1, 1) {
    items[$].update();  // Can modify
}
```

---

## Related Topics

- [Immutable Borrow](immutable_borrow.md) - Read-only references
- [Mutable Borrow](mutable_borrow.md) - Mutable references
- [Borrow Operator](borrow_operator.md) - & and $ operators
- [Ownership](ownership.md) - Ownership system

---

**Remember**: Borrowing = **temporary access without ownership** - multiple readers OR one writer, enforced at compile time!
