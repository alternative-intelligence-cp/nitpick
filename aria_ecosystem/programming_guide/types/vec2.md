# The `vec2` Type

**Category**: Types → Vectors  
**Syntax**: `vec2`  
**Purpose**: 2D vector (x, y)

---

## Overview

`vec2` represents a **2D vector** with x and y components.

---

## Declaration

```aria
// Basic
pos: vec2 = vec2(10.0, 20.0);

// Components
x: f32 = pos.x;  // 10.0
y: f32 = pos.y;  // 20.0
```

---

## Operations

### Arithmetic

```aria
a: vec2 = vec2(1.0, 2.0);
b: vec2 = vec2(3.0, 4.0);

sum: vec2 = a + b;      // vec2(4.0, 6.0)
diff: vec2 = a - b;     // vec2(-2.0, -2.0)
scaled: vec2 = a * 2.0;  // vec2(2.0, 4.0)
```

### Dot Product

```aria
a: vec2 = vec2(1.0, 2.0);
b: vec2 = vec2(3.0, 4.0);

dot: f32 = a.dot(b);  // 1*3 + 2*4 = 11.0
```

### Length

```aria
v: vec2 = vec2(3.0, 4.0);

len: f32 = v.length();  // 5.0 (Pythagorean)
len_sq: f32 = v.length_squared();  // 25.0 (faster)
```

### Normalization

```aria
v: vec2 = vec2(3.0, 4.0);
norm: vec2 = v.normalize();  // vec2(0.6, 0.8)
```

---

## Use Cases

### 2D Graphics

```aria
// Position
player_pos: vec2 = vec2(100.0, 200.0);

// Velocity
velocity: vec2 = vec2(5.0, -3.0);

// Update
player_pos += velocity;
```

### Physics

```aria
// Force and acceleration
force: vec2 = vec2(10.0, 0.0);
mass: f32 = 2.0;
acceleration: vec2 = force / mass;
```

---

## Related

- [vec3](vec3.md) - 3D vector
- [flt32](flt32.md) - Component type

---

**Remember**: `vec2` for **2D** coordinates and vectors!
