# The `NULL` Constant

**Category**: Types → Constants (Legacy)  
**Syntax**: `NULL`  
**Purpose**: C-style null pointer (legacy compatibility)

---

## Overview

`NULL` is a **legacy constant** from C for null pointers. **Use `nil` instead** in modern Aria.

---

## Legacy Usage

```aria
// Old style (C compatibility)
ptr: *i32 = NULL;

// Modern Aria - use 'nil'
ptr: *i32 = nil;  // ✅ Preferred
```

---

## Comparison

```aria
// Check for NULL
when ptr == NULL then
    stdout << "Null pointer";
end

// Better - use nil
when ptr == nil then
    stdout << "Null pointer";
end
```

---

## Why Avoid NULL?

### ❌ NULL is Legacy

```aria
// Old C style
ptr: *Data = NULL;  // ❌ Outdated
```

### ✅ Use nil Instead

```aria
// Modern Aria
ptr: *Data = nil;  // ✅ Idiomatic
```

---

## FFI with C

```aria
// When interfacing with C libraries
extern "C" fn c_function() -> *void;

// C might return NULL
ptr: *void = c_function();
when ptr == NULL then
    handle_error();
end
```

---

## Related

- [nil](nil_null_void.md) - Modern null value (preferred)
- [NIL](NIL.md) - Optional empty value
- [Pointers](pointers.md)

---

**Remember**: **Use `nil` not `NULL`** - NULL is legacy C only!
