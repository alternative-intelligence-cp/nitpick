# Aria Generics System - User Guide

## Overview

Aria's generics system provides **zero-cost abstractions** through compile-time monomorphization. Generic functions are compiled into specialized versions for each concrete type used, with no runtime overhead.

## Basic Syntax

### Generic Function Declaration

```aria
func<T>:identity = *T(*T:value) {
    pass value;
}
```

**Key points:**
- `<T>` declares the type parameter
- `*T` references the type parameter in the signature and body
- The `*` sigil makes generic types syntactically distinct

### Calling Generic Functions

**Implicit inference (preferred):**
```aria
int32:x = identity(42);  // T inferred as int32
```

**Explicit turbofish syntax:**
```aria
int32:x = identity::<int32>(42);
```

## Type Inference

Aria uses **bidirectional type inference** to automatically deduce type parameters from arguments:

```aria
func<T>:max = *T(*T:a, *T:b) {
    if (a > b) { pass a; }
    pass b;
}

int32:result = max(10, 20);  // T = int32
flt64:pi = max(3.14, 2.71);  // T = flt64
```

### Type Conflicts

All uses of the same type parameter must have the same type:

```aria
// ERROR: T cannot be both int32 and flt64
int32:bad = max(10, 3.14);
```

**Fix:** Use explicit cast or turbofish:
```aria
flt64:good = max::<flt64>(10.0, 3.14);
```

## Trait Constraints

Constrain type parameters to types that implement specific traits:

```aria
func<T: Display>:show = void(*T:item) {
    print(item.toString());
}
```

### Multiple Constraints

Use `&` to require multiple traits:

```aria
func<T: Hashable & Display>:showHash = void(*T:item) {
    print(`&{item.toString()} -> &{item.hash()}`);
}
```

## Multiple Type Parameters

Functions can have multiple generic parameters:

```aria
func<T, U>:convert = *U(*T:input) {
    pass cast::<*U>(input);
}

flt64:result = convert::<int32, flt64>(42);
```

## Implementation Details

### Monomorphization

Generic functions are **monomorphized** at compile time:

1. Each unique type combination creates a specialized version
2. Specialized functions are cached to prevent code bloat
3. Link-time merging removes duplicates across compilation units

Example:
```aria
func<T>:identity = *T(*T:x) { pass x; }

identity(42);        // Creates: _Aria_M_identity_<hash>_int32
identity(3.14);      // Creates: _Aria_M_identity_<hash>_flt64
identity(42);        // Reuses: _Aria_M_identity_<hash>_int32 (cache hit)
```

### Name Mangling

Specialized functions use deterministic name mangling:

**Format:** `_Aria_M_<FuncName>_<TypeHash>_<ReadableTypes>`

**Example:** `_Aria_M_max_F4A19C88_int32_int32`

### Depth Limits

To prevent infinite instantiation loops:
- Maximum nesting depth: **64 levels**
- Cycle detection prevents recursive instantiation
- Clear error messages when limits exceeded

## Error Messages

### Cannot Infer Type

```
Error: Cannot infer type parameter 'T' from arguments
  --> example.aria:5:10
Hint: Use turbofish syntax to specify explicitly: default::<T=int32>(...)
```

### Type Conflict

```
Error: Type conflict: 'T' cannot be both 'int32' and 'flt64'
  --> example.aria:5:15
   |
 5 |     max(10, 3.14)
   |            ^^^^ expected int32 but got flt64
   |
   = note: All uses of the same type parameter must have the same type
```

### Constraint Violation

```
Error: Type 'int32' does not satisfy trait bound 'Display'
  --> example.aria:8:5
Hint: Implement Display trait for int32 or use a different type
```

## Best Practices

1. **Prefer inference over turbofish** - Let the compiler deduce types when possible
2. **Use descriptive type parameter names** - `<Key, Value>` is clearer than `<K, V>`
3. **Add constraints early** - Trait bounds catch errors at definition time
4. **Keep generic functions focused** - One clear responsibility per function
5. **Document expected types** - Use comments for complex generic relationships

## Performance Notes

- **Zero runtime cost** - Generic calls compile to direct, non-virtual function calls
- **No type erasure** - Full type information available at compile time
- **Optimal code generation** - Specialized versions allow maximum optimization
- **Link-time deduplication** - Identical specializations merged automatically

## Complete Example

```aria
// Generic swap function with Display constraint for debugging
func<T: Display>:swap = void(*T:a, *T:b) {
    print(`Swapping &{a.toString()} and &{b.toString()}`);
    *T:temp = a;
    a = b;
    b = temp;
}

// Usage
int32:x = 10;
int32:y = 20;
swap(x, y);  // T inferred as int32, calls _Aria_M_swap_<hash>_int32
```

## TBB Type Widening (ARIA-018 Safety Constraint)

**CRITICAL:** Implicit TBB widening is **forbidden** due to sentinel discontinuity. Each TBB width has a different ERR sentinel value, and standard sign-extension does not preserve error state.

### Problem Example (FORBIDDEN)

```aria
tbb8:temp = getSensorReading();  // Returns ERR on failure
// tbb16:wide = temp;  // ❌ COMPILE ERROR! ERR would "heal" to valid -128
```

**Why this fails:**
- `tbb8` ERR = -128 (0x80)
- Sign-extension → `tbb16` = -128 (0xFF80) - **valid number, NOT ERR!**
- `tbb16` ERR = -32768 (0x8000) - **different value**

### Correct Usage: `tbb_widen<T>()`

Use the explicit widening intrinsic:

```aria
tbb8:temp = getSensorReading();
tbb16:wide = tbb_widen<tbb16>(temp);  // ✓ Preserves ERR correctly
```

**How it works:**
```aria
// Internal implementation (simplified)
func<T>:tbb_widen = *T(tbb8:val) {
    if (val == TBB8_ERR) {
        pass ERR;  // Maps to appropriate ERR for destination type
    }
    pass sext(val);  // Standard sign-extension for valid values
}
```

### Available Widening Functions

```aria
tbb_widen<tbb16>(tbb8)   // 8-bit → 16-bit
tbb_widen<tbb32>(tbb8)   // 8-bit → 32-bit
tbb_widen<tbb64>(tbb8)   // 8-bit → 64-bit
tbb_widen<tbb32>(tbb16)  // 16-bit → 32-bit
tbb_widen<tbb64>(tbb16)  // 16-bit → 64-bit
tbb_widen<tbb64>(tbb32)  // 32-bit → 64-bit
```

### Performance Note

`tbb_widen<T>()` compiles to a **single branchless instruction** (cmov/csel) with zero runtime overhead. The safety constraint has no performance cost.

### Real-World Safety Impact

```aria
// Temperature control system
tbb8:sensor = readTemperature();  // Returns ERR on sensor failure

// WRONG: Implicit widening (FORBIDDEN)
// tbb16:temp = sensor;  // Would convert ERR to -128°C (valid!)
// if (temp < -100) { activateHeaters(); }  // Heaters activate on error! 🔥

// CORRECT: Explicit widening preserves ERR
tbb16:temp = tbb_widen<tbb16>(sensor);
if (temp == ERR) {
    failSafe();  // System halts safely
} else if (temp < -100) {
    activateHeaters();  // Only activates on valid readings
}
```

**See Also:**
- [TBB Type System Spec](../specs/TBB_TYPE_SYSTEM_SPEC.md) - Complete ARIA-018 documentation
- [aria_specs.txt](aria_specs.txt) - Formal safety constraints

---

## See Also

- [Type System Reference](type_system.md)
- [Trait System](traits.md)
- [research_027_generics_templates.txt](../gemini/responses/research_027_generics_templates.txt) - Complete specification
