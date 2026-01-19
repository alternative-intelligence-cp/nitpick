# Function Return Types

**Category**: Types → Functions  
**Purpose**: Specifying function return types

---

## Basic Return Types

```aria
// Returns integer
fn get_value() -> i32 {
    return 42;
}

// Returns string
fn get_name() -> string {
    return "Alice";
}

// Returns nothing
fn print_hello() -> void {
    stdout << "Hello";
}
```

---

## Optional Returns

```aria
// Returns optional value
fn find_user(id: i32) -> ?User {
    when not found then
        return nil;
    end
    return user;
}
```

---

## Result Returns

```aria
// Returns Result for error handling
fn divide(a: i32, b: i32) -> Result<i32> {
    when b == 0 then
        return Err("Division by zero");
    end
    return Ok(a / b);
}
```

---

## Function Returns

```aria
// Returns function
fn make_multiplier(n: i32) -> fn(i32) -> i32 {
    return fn(x: i32) -> i32 {
        return x * n;
    };
}
```

---

## Multiple Returns (Tuple)

```aria
// Return multiple values as tuple
fn divide_with_remainder(a: i32, b: i32) -> (i32, i32) {
    quotient: i32 = a / b;
    remainder: i32 = a % b;
    return (quotient, remainder);
}

// Use
(q, r) := divide_with_remainder(17, 5);  // q=3, r=2
```

---

## Related

- [Functions](func.md)
- [Function Declaration](func_declaration.md)
- [Result](result.md)
- [Arrow (->)](../operators/arrow.md)

---

**Remember**: Use `->` to specify **return type**!
