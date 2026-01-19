# The `NIL` Constant

**Category**: Types → Constants  
**Syntax**: `NIL`  
**Purpose**: Represents "no value" or empty state

---

## Overview

`NIL` is a **constant** representing the absence of a value in optional types.

---

## Usage

```aria
// Optional type
value: ?i32 = NIL;

// Assign value
value = 42;

// Back to no value
value = NIL;
```

---

## Comparison

```aria
value: ?i32 = NIL;

// Check if NIL
when value == NIL then
    stdout << "No value";
else
    stdout << "Has value: $value";
end
```

---

## Pattern Matching

```aria
value: ?string = get_optional_string();

when value is string(s) then
    stdout << "Value: $s";
else
    stdout << "Value is NIL";
end
```

---

## Best Practices

### ✅ DO: Use for Optional Values

```aria
Result: ?User = find_user(id);
when result == NIL then
    return Err("User not found");
end
```

### ✅ DO: Initialize Optionals

```aria
cache: ?Data = NIL;  // Start with no cached value
```

### ❌ DON'T: Confuse with NULL or ERR

```aria
// NIL is for optionals
opt: ?i32 = NIL;  // ✅

// NULL is for pointers (legacy C)
ptr: *i32 = NULL;  // ⚠️ Use 'nil' instead

// ERR is for TBB error values
value: tbb32 = ERR;  // ❌ Different concept
```

---

## Related

- [nil](nil_null_void.md) - Lowercase nil (preferred in modern Aria)
- [NULL](NULL.md) - C-style null
- [ERR](ERR.md) - TBB error sentinel
- [void](void.md) - No return value
- [Optional (?)](../operators/question.md)

---

**Remember**: `NIL` = **no value** for optionals!
