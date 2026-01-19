# Pointer Member Access (->)

**Category**: Operators → Access  
**Operator**: `->`  
**Purpose**: Access through pointer (C-style)

---

## Description

In Aria, the dot operator `.` auto-dereferences pointers, so `->` is rarely needed.

---

## Auto-Dereference

```aria
// Pointer to struct
ptr: *Point = get_pointer();

// Use dot - auto-dereferences
x: i32 = ptr.x;
ptr.y = 20;

// No need for ->
```

---

## C Interop

```aria
// In extern blocks, -> may be used
extern {
    struct CStruct {
        value: i32
    }
    
    fn use_c_struct(ptr: *CStruct) {
        // C-style access
        n: i32 = ptr->value;
    }
}
```

---

## Best Practice

```aria
// Aria style - use dot
ptr: *Data = &data;
Result: i32 = ptr.calculate();  // ✅ Auto-dereference

// Not needed
Result: i32 = ptr->calculate(); // ❌ Unnecessary
```

---

## Related

- [Dot (.)](dot.md) - Member access
- [Arrow (->)](arrow.md) - Return type annotation
- [Dereference (*)](multiply.md)

---

**Remember**: Use `.` - it **auto-dereferences** pointers!
