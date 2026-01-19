# The `vec9` Type

**Category**: Types → Vectors  
**Syntax**: `vec9`  
**Purpose**: 9D vector (ternary computing)

---

## Overview

`vec9` represents a **9-dimensional vector**, primarily used in ternary computing contexts.

---

## Declaration

```aria
// 9 components
v: vec9 = vec9(v0, v1, v2, v3, v4, v5, v6, v7, v8);

// Access
val: f32 = v[3];  // Fourth component
```

---

## Use Cases

### Ternary Computing

```aria
// Represent ternary state
ternary_state: vec9 = calculate_ternary_state();
```

### Specialized Math

```aria
// Higher-dimensional calculations
Result: vec9 = transform_9d(input);
```

---

## Related

- [vec2](vec2.md), [vec3](vec3.md) - Lower dimensional vectors
- [tryte](tryte.md) - Ternary byte
- [balanced_ternary](balanced_ternary.md)

---

**Remember**: `vec9` is for **specialized** ternary applications!
