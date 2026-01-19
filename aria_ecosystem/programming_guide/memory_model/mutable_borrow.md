# Mutable Borrow

**Category**: Memory Model → References  
**Operator**: `$`  
**Concept**: Mutable reference

---

## What is a Mutable Borrow?

A **mutable borrow** is a mutable reference created with `$` that allows modifying data through the reference.

---

## Basic Syntax

```aria
value: i32 = 42;
ref: &i32 = $value;  // Mutable borrow

stdout << ref;  // ✅ Can read
ref = 100;      // ✅ Can modify

stdout << value;  // 100
```

---

## Exclusive Access

Only **one** mutable borrow at a time:

```aria
value: i32 = 42;

ref1: &i32 = $value;  // ✅ OK
ref2: &i32 = $value;  // ❌ Error: already borrowed mutably
```

---

## In Functions

```aria
fn double(value: &i32) {
    value = value * 2;
}

x: i32 = 21;
double($x);  // Mutable borrow
stdout << x;  // 42
```

---

## Modifying Structs

```aria
struct Point {
    x: i32,
    y: i32
}

fn move_point(p: &Point, dx: i32, dy: i32) {
    p.x = p.x + dx;
    p.y = p.y + dy;
}

point: Point = Point{x: 10, y: 20};
move_point($point, 5, 5);
stdout << point.x << ", " << point.y;  // 15, 25
```

---

## Rules

### ✅ CAN: One Mutable Borrow

```aria
value: i32 = 42;
ref: &i32 = $value;  // ✅ OK
ref = 100;           // ✅ Can modify
```

### ❌ CAN'T: Multiple Mutable Borrows

```aria
value: i32 = 42;

ref1: &i32 = $value;  // ✅ OK
ref2: &i32 = $value;  // ❌ Error: already borrowed
```

### ❌ CAN'T: Mix with Immutable Borrows

```aria
value: i32 = 42;

ref1: &i32 = &value;  // Immutable
ref2: &i32 = $value;  // ❌ Error: can't borrow mutably
```

---

## Why Only One?

### Prevent Data Races

```aria
// If multiple mutable borrows were allowed:
value: i32 = 42;
ref1: &i32 = $value;
ref2: &i32 = $value;

// Thread 1: ref1 = 100
// Thread 2: ref2 = 200
// Race condition!

// With one mutable borrow:
// Guaranteed safe access
```

### Prevent Aliasing

```aria
// If allowed:
vec: Vec<i32> = vec![1, 2, 3];
ref1: &Vec<i32> = $vec;
ref2: &Vec<i32> = $vec;

ref1.clear();  // Clears vec
ref2.push(4);  // Undefined behavior!

// With one mutable:
// Clear guarantees
```

---

## Borrow Scope

Borrow lasts until last use:

```aria
value: i32 = 42;

{
    ref: &i32 = $value;
    ref = 100;
    stdout << ref;  // Last use
}  // Borrow ends

// Can borrow again
ref2: &i32 = $value;  // ✅ OK
```

---

## Common Patterns

### In-Place Modification

```aria
fn increment(counter: &i32) {
    counter = counter + 1;
}

count: i32 = 0;
increment($count);
increment($count);
stdout << count;  // 2
```

### Update Fields

```aria
fn update_user(user: &User, name: string) {
    user.name = name;
    user.updated_at = now();
}

user: User = load_user();
update_user($user, "Alice");
```

### Swap Values

```aria
fn swap(a: &i32, b: &i32) {
    temp: i32 = a;
    a = b;
    b = temp;
}

x: i32 = 10;
y: i32 = 20;
swap($x, $y);
stdout << x << ", " << y;  // 20, 10
```

---

## With Collections

```aria
fn append_to_all(strings: &[]string, suffix: string) {
    for $str in strings {
        str = str + suffix;
    }
}

words: []string = ["hello", "world"];
append_to_all($words, "!");
// words is now ["hello!", "world!"]
```

---

## Reborrowing

Can create new borrow from existing:

```aria
fn modify(value: &i32) {
    helper($value);  // Reborrow as mutable
}

fn helper(v: &i32) {
    v = v * 2;
}
```

---

## Best Practices

### ✅ DO: Use When Modification Needed

```aria
// Good: Need to modify
fn reset(config: &Config) {
    config.load_defaults();
}
```

### ✅ DO: Keep Borrows Short

```aria
// Good: Short-lived borrow
{
    ref: &i32 = $value;
    ref = ref * 2;
}  // Borrow ends

// Can use value again
```

### ❌ DON'T: Hold Longer Than Needed

```aria
// Wrong: Held too long
ref: &i32 = $value;

// Lots of code...

ref = 100;  // Finally used

// Right: Defer until needed
// ... lots of code ...
{
    ref: &i32 = $value;
    ref = 100;
}
```

### ❌ DON'T: Borrow When Ownership Works

```aria
// Wrong: Unnecessary borrowing
fn take_ownership(value: &i32) {
    temp: i32 = value;  // Just copying anyway
}

// Right: Take by value
fn take_ownership(value: i32) {
    // Direct ownership
}
```

---

## Real-World Examples

### Batch Update

```aria
fn apply_discount(products: &[]Product, discount: f64) {
    for $product in products {
        product.price = product.price * (1.0 - discount);
    }
}

items: []Product = load_products();
apply_discount($items, 0.2);  // 20% off
```

### Cache Update

```aria
struct Cache {
    data: Map<string, string>
}

fn update_cache(cache: &Cache, key: string, value: string) {
    cache.data.insert(key, value);
}

cache: Cache = Cache::new();
update_cache($cache, "user:1", "Alice");
```

### State Transition

```aria
struct Connection {
    state: State,
    data: []byte
}

fn transition(conn: &Connection, new_state: State) {
    conn.state = new_state;
    when new_state == State::Closed then
        conn.data.clear();
    end
}
```

---

## Related Topics

- [Borrowing](borrowing.md) - Borrowing overview
- [Immutable Borrow](immutable_borrow.md) - Read-only references
- [Borrow Operator](borrow_operator.md) - $ operator
- [Dollar Variable](../control_flow/dollar_variable.md) - $ in loops

---

**Remember**: Mutable borrow = **exclusive write access** - only one at a time, enforced at compile time!
