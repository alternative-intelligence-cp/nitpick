# Dollar Operator ($)

**Category**: Operators → Reference  
**Operator**: `$`  
**Purpose**: Create mutable reference

---

## Syntax

```aria
$<variable>
```

---

## Description

The dollar operator `$` creates a **mutable reference** (borrow) to a variable, allowing modifications through the reference.

---

## Basic Usage

```aria
value: i32 = 42;
mutable_ref: $i32 = $value;

// Modify through reference
*mutable_ref = 100;

stdout << value;  // 100
```

---

## Function Parameters

```aria
fn increment(x: $i32) {
    *x += 1;
}

count: i32 = 5;
increment($count);
stdout << count;  // 6
```

---

## Exclusive Access

```aria
data: i32 = 42;

// Only ONE mutable borrow allowed
mutable: $i32 = $data;

// Error: Can't have multiple mutable borrows
another: $i32 = $data;  // ❌ Error!

// Error: Can't mix with immutable borrows
immutable: &i32 = &data;  // ❌ Error!
```

---

## Mutable Struct Fields

```aria
struct Point {
    x: i32,
    y: i32
}

fn move_point(p: $Point, dx: i32, dy: i32) {
    p.x += dx;
    p.y += dy;
}

point: Point = Point { x: 10, y: 20 };
move_point($point, 5, -3);
stdout << point.x << ", " << point.y;  // 15, 17
```

---

## In-Place Modification

```aria
fn double_all(arr: $[]i32) {
    for i in 0..arr.length() {
        arr[i] *= 2;
    }
}

numbers: []i32 = [1, 2, 3, 4, 5];
double_all($numbers);
// numbers is now [2, 4, 6, 8, 10]
```

---

## Borrowing Rules

### Rule 1: Only ONE Mutable Borrow

```aria
x: i32 = 42;
m1: $i32 = $x;
m2: $i32 = $x;  // ❌ Error: Already borrowed mutably
```

### Rule 2: Can't Mix with Immutable

```aria
x: i32 = 42;
r: &i32 = &x;
m: $i32 = $x;  // ❌ Error: Already borrowed immutably
```

---

## Best Practices

### ✅ DO: Use for In-Place Updates

```aria
fn normalize(vec: $Vector) {
    len: f64 = vec.length();
    vec.x /= len;
    vec.y /= len;
    vec.z /= len;
}
```

### ✅ DO: Limit Scope

```aria
data: i32 = 42;

{
    mutable: $i32 = $data;
    *mutable = 100;
}  // Borrow ends

// Can borrow again
immutable: &i32 = &data;
```

### ❌ DON'T: Hold Mutable Borrow Longer Than Needed

```aria
// Wrong: Long-lived mutable borrow
global_ref: $Data = $global_data;

// Better: Borrow briefly
fn modify() {
    temp: $Data = $global_data;
    temp.update();
}  // Borrow released
```

---

## Difference from Pointers

```aria
// Mutable reference: Safe, checked
ref: $i32 = $value;
*ref = 42;  // Safe

// Raw pointer: Unsafe
ptr: *i32 = &value;
*ptr = 42;  // Can be unsafe
```

---

## Related

- [Ampersand (&)](ampersand.md) - Immutable borrow
- [Mutable Borrow](../memory_model/mutable_borrow.md)
- [Borrowing](../memory_model/borrowing.md)

---

**Remember**: `$` creates **exclusive mutable** access - only **ONE** at a time!
