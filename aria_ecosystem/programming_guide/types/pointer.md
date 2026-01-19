# The `pointer` Type

**Category**: Types → Memory  
**Purpose**: Alternative name for pointer types

---

See [Pointers (*T)](pointers.md) for complete documentation.

---

## Quick Reference

```aria
// pointer is same as *T
ptr: pointer<i32> = nil;

// Equivalent to
ptr: *i32 = nil;

// Prefer *T syntax
value: *Data = get_data();  // ✅ Common
```

---

**Remember**: Use `*T` syntax - it's more common!
