# Pin Operator (pin)

**Category**: Operators → Memory  
**Operator**: `pin` / `unpin`  
**Purpose**: Pin memory location

---

See [Pin Operator](../memory_model/pin_operator.md) and [Pinning](../memory_model/pinning.md) for complete documentation.

---

## Quick Reference

```aria
// Pin object in memory
data: Data = aria_gc_alloc(Data);
pin data;
defer unpin data;

// Safe to get address
ptr: *Data = &data;

// Use with C interop
c_function(&data);
```

---

## When to Use

- Passing to C/foreign code
- Long-lived raw pointers
- Hardware/DMA access
- Preventing GC movement

---

**Remember**: Always `unpin` what you `pin`!
