# Function Declaration

**Category**: Types → Functions  
**Purpose**: Declaring function types and signatures

---

## Type Alias

```aria
// Create type alias for function signature
type Predicate = fn(i32) -> bool;

// Use it
fn filter(arr: []i32, pred: Predicate) -> []i32 {
    ...
}
```

---

## Variable Declaration

```aria
// Function in variable
operation: fn(i32, i32) -> i32;

// Assign later
operation = fn(a: i32, b: i32) -> i32 { return a + b; };
```

---

## Parameter Declaration

```aria
// Function as parameter
fn apply_twice(f: fn(i32) -> i32, x: i32) -> i32 {
    return f(f(x));
}
```

---

## Return Type Declaration

```aria
// Function returns function
fn get_operation() -> fn(i32, i32) -> i32 {
    return fn(a: i32, b: i32) -> i32 { return a + b; };
}
```

---

## Related

- [Functions](func.md)
- [Function Return](func_return.md)

---

**Remember**: Function types use `fn(params) -> return` syntax!
