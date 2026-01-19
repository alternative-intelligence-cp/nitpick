# The `ERR` Constant

**Category**: Types → TBB Constants  
**Syntax**: `ERR`  
**Purpose**: TBB error sentinel value

---

## Overview

`ERR` is the **error sentinel** used in TBB (Twisted Balanced Binary) types to represent errors.

---

## What is ERR?

In TBB types, `ERR` is a **special value** that represents an error state. It's not a regular number.

```aria
// TBB32 can hold values -2^30+1 to 2^30-1, plus ERR
value: tbb32 = ERR;
```

---

## Automatic Propagation

```aria
a: tbb32 = 10;
b: tbb32 = ERR;  // Error value

// ERR propagates automatically
c: tbb32 = a + b;  // c = ERR (not a crash!)
```

---

## Checking for ERR

```aria
value: tbb32 = compute();

when value == ERR then
    stderr << "Computation failed";
    return;
end

// Safe to use
Result: tbb32 = value * 2;
```

---

## Sticky Errors

```aria
// Once ERR, always ERR
a: tbb32 = ERR;
b: tbb32 = a + 10;  // b = ERR
c: tbb32 = b * 5;   // c = ERR
d: tbb32 = c - 3;   // d = ERR

// ERR "sticks" through all operations
```

---

## Use Cases

### Division by Zero

```aria
fn safe_divide(a: tbb32, b: tbb32) -> tbb32 {
    when b == 0 then
        return ERR;
    end
    return a / b;
}

// Use
Result: tbb32 = safe_divide(10, 0);  // ERR
```

### Chain Operations

```aria
// Any ERR in chain makes whole result ERR
Result: tbb32 = step1()
    .then(step2)
    .then(step3);  // ERR if any step fails
```

---

## Best Practices

### ✅ DO: Check After Critical Operations

```aria
value: tbb32 = critical_operation();
when value == ERR then
    handle_error();
    return ERR;
end
```

### ✅ DO: Use for Error Propagation

```aria
// Errors propagate automatically
fn complex_calc() -> tbb32 {
    a: tbb32 = step1();  // Might be ERR
    b: tbb32 = step2();  // Might be ERR
    return a + b;  // ERR if either is ERR
}
```

### ❌ DON'T: Confuse with NIL or NULL

```aria
// ERR is for TBB errors
value: tbb32 = ERR;  // ✅

// NIL is for optionals
opt: ?i32 = NIL;  // Different!

// NULL is for pointers (legacy)
ptr: *i32 = NULL;  // Different!
```

---

## ERR vs Result

```aria
// TBB with ERR
fn divide_tbb(a: tbb32, b: tbb32) -> tbb32 {
    when b == 0 then return ERR; end
    return a / b;
}

// Result type (more explicit)
fn divide_result(a: i32, b: i32) -> Result<i32> {
    when b == 0 then return Err("Division by zero"); end
    return Ok(a / b);
}
```

**TBB ERR**: Fast, lightweight, propagates automatically  
**Result**: Explicit, carries error message, type-safe

---

## Related

- [TBB Overview](tbb_overview.md) - TBB type system
- [TBB ERR Sentinel](tbb_err_sentinel.md) - ERR details
- [TBB Sticky Errors](tbb_sticky_errors.md) - Error propagation
- [NIL](NIL.md) - Optional empty value
- [NULL](NULL.md) - Legacy null pointer
- [Result](result.md) - Alternative error handling

---

**Remember**: `ERR` = **TBB error sentinel** - propagates automatically!
