# The `vec3` Type

**Category**: Types → Vectors  
**Syntax**: `vec3`  
**Purpose**: 3D vector (x, y, z)

---

## Overview

`vec3` represents a **3D vector** with x, y, and z components.

---

## Declaration

```aria
// Basic
pos: vec3 = vec3(1.0, 2.0, 3.0);

// Components
x: f32 = pos.x;  // 1.0
y: f32 = pos.y;  // 2.0
z: f32 = pos.z;  // 3.0
```

---

## Operations

### Arithmetic

```aria
a: vec3 = vec3(1.0, 2.0, 3.0);
b: vec3 = vec3(4.0, 5.0, 6.0);

sum: vec3 = a + b;      // vec3(5.0, 7.0, 9.0)
diff: vec3 = a - b;     // vec3(-3.0, -3.0, -3.0)
scaled: vec3 = a * 2.0;  // vec3(2.0, 4.0, 6.0)
```

### Dot Product

```aria
a: vec3 = vec3(1.0, 2.0, 3.0);
b: vec3 = vec3(4.0, 5.0, 6.0);

dot: f32 = a.dot(b);  // 1*4 + 2*5 + 3*6 = 32.0
```

### Cross Product

```aria
a: vec3 = vec3(1.0, 0.0, 0.0);
b: vec3 = vec3(0.0, 1.0, 0.0);

cross: vec3 = a.cross(b);  // vec3(0.0, 0.0, 1.0)
```

### Length

```aria
v: vec3 = vec3(1.0, 2.0, 2.0);

len: f32 = v.length();  // 3.0
len_sq: f32 = v.length_squared();  // 9.0
```

### Normalization

```aria
v: vec3 = vec3(1.0, 2.0, 2.0);
norm: vec3 = v.normalize();  // Unit vector
```

---

## Use Cases

### 3D Graphics

```aria
// Vertex position
vertex: vec3 = vec3(1.0, 2.0, 3.0);

// Normal vector
normal: vec3 = vec3(0.0, 1.0, 0.0);

// Light direction
light_dir: vec3 = vec3(-1.0, -1.0, -1.0).normalize();
```

### Physics

```aria
// Position in 3D space
position: vec3 = vec3(0.0, 10.0, 0.0);

// Velocity
velocity: vec3 = vec3(5.0, 0.0, -3.0);

// Gravity
gravity: vec3 = vec3(0.0, -9.8, 0.0);

// Update
velocity += gravity * dt;
position += velocity * dt;
```

### Camera

```aria
// Camera vectors
eye: vec3 = vec3(0.0, 5.0, 10.0);
target: vec3 = vec3(0.0, 0.0, 0.0);
up: vec3 = vec3(0.0, 1.0, 0.0);

// View direction
forward: vec3 = (target - eye).normalize();
```

---

## Related

- [vec2](vec2.md) - 2D vector
- [vec9](vec9.md) - 9D vector (ternary)
- [matrix](matrix.md) - Matrices

---

**Remember**: `vec3` for **3D** graphics and physics!
