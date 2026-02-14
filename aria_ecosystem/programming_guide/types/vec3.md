# The `vec3` Type (3D Vector)

**Category**: Types → Vectors → 3D  
**Syntax**: `vec3`  
**Purpose**: Three-dimensional vector for 3D graphics, physics, and spatial mathematics  
**First Available**: Aria v0.0.1  
**Status**: ✅ Fully implemented with SIMD optimization

---

## Overview

`vec3` is Aria's **3-dimensional vector type**, representing points or directions in 3D space with `x`, `y`, and `z` components. This is the fundamental type for 3D graphics, game engines, physics simulations, robotics, and spatial computing.

**Key Characteristics**:
- **3 components**: `x` (horizontal), `y` (vertical), `z` (depth)
- **Component type**: Typically `float` or `double` (defaults to `float`)
- **Size**: 12 bytes (3 × 4-byte floats) but typically **16-byte aligned** (with 4 bytes padding)
- **SIMD optimized**: Can use SSE/AVX instructions (treats as 4-component with ignored W)
- **Memory layout**: Contiguous `[x, y, z, padding]` (cache-friendly)
- **Alignment**: 16-byte (for SIMD efficiency)

**Common uses**:
- ✅ 3D positions (world coordinates, object locations)
- ✅ 3D velocities and accelerations (physics)
- ✅ Direction vectors (camera facing, light direction, normals)
- ✅ Surface normals (lighting calculations)
- ✅ Cross products (perpendicular vectors)
- ✅ Rotation axes
- ✅ RGB colors (r, g, b as x, y, z)

**When NOT to use**:
- ❌ 2D graphics/UI (use `vec2` instead - simpler and smaller)
- ❌ Nikola consciousness (use `vec9` for 9D state)
- ❌ Single scalar values (use `float` or `double`)
- ❌ RGBA colors (use `vec4` to include alpha channel)

---

## Declaration & Initialization

### Basic Declaration

```aria
// Constructor with explicit components
vec3:position = vec3(10.0, 20.0, 30.0);  // x=10.0, y=20.0, z=30.0

// World position
vec3:player_position = vec3(0.0, 1.75, 0.0);  // Y=1.75m (eye height)

// Direction vector (unit length)
vec3:forward = vec3(0.0, 0.0, -1.0);  // Into the screen (OpenGL convention)
```

### Component Access

```aria
vec3:pos = vec3(100.0, 200.0, 300.0);

// Direct member access
float:x_coord = pos.x;  // 100.0
float:y_coord = pos.y;  // 200.0
float:z_coord = pos.z;  // 300.0

// Modification
pos.x = 150.0;  // Now vec3(150.0, 200.0, 300.0)
pos.y += 50.0;  // Now vec3(150.0, 250.0, 300.0)
pos.z *= 2.0;   // Now vec3(150.0, 250.0, 600.0)
```

### Special Constructors

```aria
// Uniform value (all components same)
vec3:ones = vec3(1.0);  // vec3(1.0, 1.0, 1.0)
vec3:fives = vec3(5.0);  // vec3(5.0, 5.0, 5.0)

// Zero vector
vec3:origin = vec3::ZERO;  // vec3(0.0, 0.0, 0.0)

// Unit vectors (length = 1.0) - Cartesian axes
vec3:right = vec3::RIGHT;   // vec3(1.0, 0.0, 0.0) - positive X axis
vec3:up = vec3::UP;         // vec3(0.0, 1.0, 0.0) - positive Y axis  
vec3:forward = vec3::FORWARD;  // vec3(0.0, 0.0, 1.0) - positive Z axis

vec3:left = vec3::LEFT;     // vec3(-1.0, 0.0, 0.0) - negative X axis
vec3:down = vec3::DOWN;     // vec3(0.0, -1.0, 0.0) - negative Y axis
vec3:back = vec3::BACK;     // vec3(0.0, 0.0, -1.0) - negative Z axis

// One vector
vec3:ones = vec3::ONE;  // vec3(1.0, 1.0, 1.0)
```

---

## Arithmetic Operations

### Component-wise Addition/Subtraction

```aria
vec3:a = vec3(10.0, 20.0, 30.0);
vec3:b = vec3(5.0, 15.0, 25.0);

// Addition (component-wise)
vec3:sum = a + b;  // vec3(15.0, 35.0, 55.0)

// Subtraction (component-wise)
vec3:diff = a - b;  // vec3(5.0, 5.0, 5.0)

// Negation
vec3:neg = -a;  // vec3(-10.0, -20.0, -30.0)
```

### Scalar Multiplication/Division

```aria
vec3:velocity = vec3(10.0, 5.0, 2.0);  // m/s

// Scale by scalar
vec3:doubled = velocity * 2.0;  // vec3(20.0, 10.0, 4.0)
vec3:halved = velocity / 2.0;   // vec3(5.0, 2.5, 1.0)

// Compound assignment
velocity *= 1.5;   // vec3(15.0, 7.5, 3.0)
velocity /= 3.0;   // vec3(5.0, 2.5, 1.0)
```

### Component-wise Multiplication/Division

```aria
vec3:a = vec3(10.0, 20.0, 30.0);
vec3:b = vec3(2.0, 4.0, 5.0);

// Component-wise multiplication (Hadamard product)
vec3:product = a * b;  // vec3(20.0, 80.0, 150.0) - NOT dot product!

// Component-wise division
vec3:quotient = a / b;  // vec3(5.0, 5.0, 6.0)
```

**Note**: `*` between two vec3 is component-wise, NOT dot product! Use `dot()` method for dot product.

---

## Vector Operations

### Dot Product

The **dot product** measures how aligned two vectors are:

```aria
vec3:a = vec3(3.0, 4.0, 0.0);
vec3:b = vec3(1.0, 0.0, 0.0);

float:dot = a.dot(b);  // 3.0*1.0 + 4.0*0.0 + 0.0*0.0 = 3.0

// dot(a, b) = |a| * |b| * cos(θ)
// - Positive: vectors point same general direction (θ < 90°)
// - Zero: vectors perpendicular (θ = 90°)
// - Negative: vectors point opposite directions (θ > 90°)
```

**Common uses**:
- Lighting calculations (N·L for diffuse, R·V for specular)
- Check if surfaces face toward/away from light
- Calculate angle between vectors
- Project vector onto another

```aria
// Diffuse lighting (Lambertian)
vec3:surface_normal = vec3(0.0, 1.0, 0.0);  // Facing up
vec3:light_direction = vec3(0.707, 0.707, 0.0).normalize();  // 45° angle

float:diffuse = max(0.0, surface_normal.dot(light_direction));
// diffuse = 0.707 (surface receives ~70.7% of light at 45°)
```

### Cross Product (3D→Vector)

In 3D, the **cross product** returns a vector **perpendicular** to both input vectors:

```aria
vec3:a = vec3(1.0, 0.0, 0.0);  // X axis
vec3:b = vec3(0.0, 1.0, 0.0);  // Y axis

vec3:cross = a.cross(b);  // vec3(0.0, 0.0, 1.0) - Z axis (perpendicular to both!)

// Properties:
// - a.cross(b) = -b.cross(a) (anti-commutative)
// - |a.cross(b)| = |a| * |b| * sin(θ) (magnitude = parallelogram area)
// - Right-hand rule: curl fingers from a to b, thumb points in cross direction
```

**Common uses**:
- Calculate surface normals from triangles
- Compute rotation axes
- Determine handedness of coordinate systems
- Physics: torque = radius × force

```aria
// Calculate triangle normal
vec3:vertex_a = vec3(0.0, 0.0, 0.0);
vec3:vertex_b = vec3(1.0, 0.0, 0.0);
vec3:vertex_c = vec3(0.0, 1.0, 0.0);

vec3:edge1 = vertex_b - vertex_a;  // vec3(1.0, 0.0, 0.0)
vec3:edge2 = vertex_c - vertex_a;  // vec3(0.0, 1.0, 0.0)

vec3:normal = edge1.cross(edge2).normalize();  // vec3(0.0, 0.0, 1.0)
// Triangle faces toward positive Z (toward viewer)
```

### Length (Magnitude)

```aria
vec3:velocity = vec3(3.0, 4.0, 0.0);

float:speed = velocity.length();  // sqrt(3²+4²+0²) = 5.0

// Squared length (faster, avoids sqrt)
float:speed_sq = velocity.length_squared();  // 3²+4²+0² = 25.0
```

**Optimization tip**: Use `length_squared()` for comparisons to avoid expensive `sqrt()`:

```aria
vec3:position = vec3(100.0, 100.0, 50.0);
vec3:target = vec3(150.0, 150.0, 50.0);
const float:MIN_DISTANCE_SQ = 100.0;  // 10.0² = 100.0

vec3:diff = target - position;
if (diff.length_squared() < MIN_DISTANCE_SQ) {
    stdout.write("Close enough!\n");  // Avoid sqrt!
}
```

### Normalization

**Normalizing** a vector gives a unit vector (length = 1.0) in the same direction:

```aria
vec3:velocity = vec3(300.0, 400.0, 0.0);  // Length = 500.0

vec3:direction = velocity.normalize();  // vec3(0.6, 0.8, 0.0), length = 1.0

// Manual normalization
float:len = velocity.length();
vec3:manual_dir = velocity / len;  // Same result
```

**Common uses**:
- Get pure direction (ignore magnitude)
- Surface normals for lighting
- Unit basis vectors for coordinate systems

```aria
// Constant-speed movement toward target
vec3:position = vec3(100.0, 50.0, 0.0);
vec3:target = vec3(200.0, 150.0, 100.0);
const float:SPEED = 5.0;  // units per frame

vec3:direction = (target - position).normalize();
vec3:velocity = direction * SPEED;  // Always moves at SPEED

position += velocity;  // Move toward target
```

### Distance

```aria
vec3:point_a = vec3(10.0, 20.0, 30.0);
vec3:point_b = vec3(40.0, 60.0, 70.0);

float:distance = point_a.distance(point_b);  
// sqrt((40-10)² + (60-20)² + (70-30)²) = sqrt(900 + 1600 + 1600) = 64.03

// Equivalent to:
float:manual_dist = (point_b - point_a).length();
```

### Linear Interpolation (Lerp)

**Lerp** smoothly interpolates between two vectors:

```aria
vec3:start = vec3(0.0, 0.0, 0.0);
vec3:end = vec3(100.0, 100.0, 100.0);

vec3:quarter = start.lerp(end, 0.25);  // vec3(25.0, 25.0, 25.0)
vec3:half = start.lerp(end, 0.5);      // vec3(50.0, 50.0, 50.0)
vec3:three_quarter = start.lerp(end, 0.75);  // vec3(75.0, 75.0, 75.0)

// lerp(a, b, t) = a + t*(b - a) = a*(1-t) + b*t
```

**Common uses**:
- Smooth animation between positions
- Camera movement
- Character movement interpolation (lerp vs slerp for rotations)

```aria
// Smooth camera follow (3D)
vec3:camera_pos = vec3(100.0, 100.0, 100.0);
vec3:player_pos = vec3(200.0, 150.0, 120.0);
const float:LERP_SPEED = 0.1;  // 10% per frame

camera_pos = camera_pos.lerp(player_pos, LERP_SPEED);  // Smooth follow
```

### Reflection

Reflect a vector across a surface normal:

```aria
// Reflect incident ray off surface
vec3:incident = vec3(0.707, -0.707, 0.0).normalize();  // Coming down at 45°
vec3:normal = vec3(0.0, 1.0, 0.0);  // Surface facing up

// Reflection formula: r = i - 2*(i·n)*n
vec3:reflected = incident.reflect(normal);  // vec3(0.707, 0.707, 0.0) - bounces up!
```

---

## Common Patterns

### 3D Movement & Physics

```aria
// Simple 3D physics
vec3:position = vec3(0.0, 10.0, 0.0);
vec3:velocity = vec3(5.0, 0.0, 2.0);  // m/s
vec3:acceleration = vec3(0.0, -9.8, 0.0);  // Gravity (m/s²)
const float:dt = 0.016;  // 60 FPS timestep

// Update velocity (Euler integration)
velocity += acceleration * dt;

// Update position
position += velocity * dt;

// Clamp to ground
if (position.y < 0.0) {
    position.y = 0.0;
    velocity.y = 0.0;  // Stop vertical motion at ground
}
```

### Surface Normals from Triangles

```aria
// Calculate face normal for lighting
vec3:v0 = vec3(0.0, 0.0, 0.0);
vec3:v1 = vec3(1.0, 0.0, 0.0);
vec3:v2 = vec3(0.5, 1.0, 0.0);

vec3:edge1 = v1 - v0;
vec3:edge2 = v2 - v0;

vec3:face_normal = edge1.cross(edge2).normalize();
// Normal points toward viewer (right-hand rule: ccw winding)
```

### Camera Look-At

```aria
// Build camera coordinate system
vec3:camera_position = vec3(0.0, 5.0, 10.0);
vec3:target = vec3(0.0, 0.0, 0.0);  // Look at origin
vec3:world_up = vec3(0.0, 1.0, 0.0);

// Forward: toward target
vec3:forward = (target - camera_position).normalize();

// Right: perpendicular to forward and up
vec3:right = forward.cross(world_up).normalize();

// Up: perpendicular to right and forward
vec3:up = right.cross(forward);

// Now have orthonormal basis (right, up, forward) for camera transform
```

### Clamping to Bounds

```aria
// Clamp 3D position to bounding box
vec3:position = vec3(150.0, -10.0, 200.0);
const vec3:min_bounds = vec3(0.0, 0.0, 0.0);
const vec3:max_bounds = vec3(100.0, 100.0, 100.0);

position.x = clamp(position.x, min_bounds.x, max_bounds.x);  // 100.0
position.y = clamp(position.y, min_bounds.y, max_bounds.y);  // 0.0
position.z = clamp(position.z, min_bounds.z, max_bounds.z);  // 100.0

// Now position = vec3(100.0, 0.0, 100.0) - inside bounds
```

---

## Use Cases

### 3D Graphics

```aria
// Vertex positions
vec3:vertex = vec3(0.5, 0.5, 0.0);  // In clip space

// Surface normals (for lighting)
vec3:normal = vec3(0.0, 1.0, 0.0);  // Facing up

// Light direction (directional light)
vec3:light_dir = vec3(-0.707, -0.707, 0.0);  // Coming from top-right
```

### Game Development

```aria
// Player state (3D FPS game)
vec3:player_position = vec3(0.0, 1.75, 0.0);  // Y=1.75m (eye height)
vec3:player_velocity = vec3(0.0, 0.0, 0.0);
vec3:player_facing = vec3(0.0, 0.0, -1.0);  // Into screen
const float:MOVE_SPEED = 5.0;  // m/s

// WASD movement in local space
vec3:move_input = vec3(0.0, 0.0, 0.0);
if (key_pressed('W')) { move_input.z -= 1.0; }  // Forward
if (key_pressed('S')) { move_input.z += 1.0; }  // Backward
if (key_pressed('A')) { move_input.x -= 1.0; }  // Left
if (key_pressed('D')) { move_input.x += 1.0; }  // Right

// Transform to world space (assuming player_facing is forward)
vec3:right = player_facing.cross(vec3(0.0, 1.0, 0.0)).normalize();
vec3:forward = player_facing;

vec3:world_movement = (
    right * move_input.x +
    forward * move_input.z
).normalize() * MOVE_SPEED;

player_velocity = world_movement;
```

### Lighting Calculations

```aria
// Phong lighting model
vec3:surface_position = vec3(0.0, 0.0, 0.0);
vec3:surface_normal = vec3(0.0, 1.0, 0.0);  // Facing up
vec3:light_position = vec3(10.0, 10.0, 10.0);
vec3:camera_position = vec3(0.0, 5.0, 5.0);

// Diffuse component (Lambertian)
vec3:light_dir = (light_position - surface_position).normalize();
float:diffuse = max(0.0, surface_normal.dot(light_dir));

// Specular component (Phong)
vec3:view_dir = (camera_position - surface_position).normalize();
vec3:reflect_dir = (-light_dir).reflect(surface_normal);
float:specular = pow(max(0.0, view_dir.dot(reflect_dir)), 32.0);  // 32 = shininess

// Final color (assuming white light)
vec3:diffuse_color = vec3(0.8, 0.2, 0.2);  // Red surface
vec3:final_color = diffuse_color * diffuse + vec3(1.0, 1.0, 1.0) * specular;
```

### Physics

```aria
// Projectile motion
vec3:position = vec3(0.0, 0.0, 0.0);
vec3:initial_velocity = vec3(10.0, 20.0, 0.0);  // Launch angle
const vec3:gravity = vec3(0.0, -9.8, 0.0);  // m/s²
float:time = 0.0;
const float:dt = 0.016;

// Kinematic equation: p(t) = p0 + v0*t + 0.5*a*t²
vec3:velocity = initial_velocity + gravity * time;
position = position + velocity * dt;
time += dt;
```

### Rotation Around Axis

```aria
// Rotate point around arbitrary axis (Rodrigues' rotation formula)
vec3:point = vec3(1.0, 0.0, 0.0);  // Point to rotate
vec3:axis = vec3(0.0, 1.0, 0.0);   // Rotate around Y axis
float:angle = 1.5708;  // 90° in radians

// Simple rotation (for axis-aligned only)
// For general rotation, use quaternions or rotation matrices

// Y-axis rotation (special case)
vec3:rotated = vec3(
    point.x * cos(angle) - point.z * sin(angle),
    point.y,
    point.x * sin(angle) + point.z * cos(angle)
);  // vec3(0.0, 0.0, 1.0) - rotated 90° around Y
```

---

## Nikola Consciousness Applications

While Nikola uses **vec9** for full 9D consciousness state, vec3 can represent **simplified 3D cognitive/spatial states**:

### 3D Emotional Space

```aria
// Emotional state in 3D (valence, arousal, dominance)
vec3:emotional_state = vec3(0.0, 0.0, 0.0);  // Neutral

// X: Valence (-1.0 negative, +1.0 positive)
// Y: Arousal (-1.0 calm, +1.0 excited)
// Z: Dominance (-1.0 submissive, +1.0 dominant)

vec3:joyful = vec3(0.8, 0.7, 0.6);    // Happy, excited, confident
vec3:depressed = vec3(-0.7, -0.6, -0.5);  // Sad, low energy, weak
vec3:angry = vec3(-0.5, 0.9, 0.8);     // Negative, excited, dominant
```

### Spatial Awareness

```aria
// Nikola tracking body position in 3D therapy room
vec3:body_position = vec3(0.0, 1.0, 0.0);  // Standing, center of room
vec3:head_facing = vec3(0.0, 0.0, -1.0);   // Looking forward
vec3:attention_target = vec3(0.0, 1.5, -2.0);  // Therapist's face

// Calculate attention alignment
vec3:to_target = (attention_target - body_position).normalize();
float:attention_alignment = head_facing.dot(to_target);  // 1.0 = directly looking
```

---

## Performance

### SIMD Optimization

vec3 operations typically use **16-byte SIMD** (treating as vec4 with unused W component):

```aria
// Single operation (uses SSE/AVX as vec4)
vec3:a = vec3(1.0, 2.0, 3.0);
vec3:b = vec3(4.0, 5.0, 6.0);
vec3:c = a + b;  // ~4 CPU cycles (SIMD add with ignored W)

// Array of vec3 (SIMD batch processing)
vec3[1000]:positions;
vec3[1000]:velocities;

for (tbb32:i = 0i32; i < 1000i32; ++i) {
    positions[i] += velocities[i];  // Compiler can vectorize
}
```

**Performance**: ~3-4× faster than scalar floats when SIMD-optimized.

### Memory Padding

```aria
// vec3 is 12 bytes but typically aligned to 16 bytes
vec3:position = vec3(1.0, 2.0, 3.0);

// Memory layout:
// [float x] [float y] [float z] [4 bytes padding]
//  ↑ 4B      ↑ 4B      ↑ 4B      ↑ 4B (unused for alignment)
```

**Array memory**: `vec3[100]` = 1600 bytes (16 bytes per element with padding).

---

## Memory Layout

```aria
vec3:position = vec3(100.0, 200.0, 300.0);

// Memory layout (16 bytes with padding):
// [float x = 100.0] [float y = 200.0] [float z = 300.0] [padding = ???]
//  ↑ 4 bytes         ↑ 4 bytes         ↑ 4 bytes         ↑ 4 bytes
```

**Alignment**: Typically 16-byte aligned (for SIMD efficiency).

**Padding**: 4 bytes padding after Z component (total 16 bytes).

---

## Type Conversions

```aria
// vec3 → individual components
vec3:pos = vec3(10.0, 20.0, 30.0);
float:x = pos.x;
float:y = pos.y;
float:z = pos.z;

// Individual components → vec3
float:x_val = 5.0;
float:y_val = 10.0;
float:z_val = 15.0;
vec3:reconstructed = vec3(x_val, y_val, z_val);

// Array subscript access (alternative)
float:first = pos[0];   // 10.0 (same as pos.x)
float:second = pos[1];  // 20.0 (same as pos.y)
float:third = pos[2];   // 30.0 (same as pos.z)

// vec2 → vec3 (add Z component)
vec2:pos_2d = vec2(10.0, 20.0);
vec3:pos_3d = vec3(pos_2d.x, pos_2d.y, 0.0);  // Z defaults to 0.0

// vec3 → vec2 (drop Z component)
vec3:pos_3d_orig = vec3(10.0, 20.0, 30.0);
vec2:pos_2d_proj = vec2(pos_3d_orig.x, pos_3d_orig.y);  // Ignore Z
```

---

## Comparison Operations

```aria
vec3:a = vec3(10.0, 20.0, 30.0);
vec3:b = vec3(10.0, 20.0, 30.0);
vec3:c = vec3(5.0, 25.0, 35.0);

// Equality (component-wise exact match)
bool:equal = (a == b);  // TRUE (all components match)
bool:not_equal = (a != c);  // TRUE (components differ)
```

**Note**: For approximate equality (floating-point safe), use epsilon comparison:

```aria
const float:EPSILON = 0.001;

func:almost_equal = bool(vec3:a, vec3:b) {
    vec3:diff = a - b;
    pass(diff.length_squared() < EPSILON * EPSILON);
};
```

---

## vs Other Vector Types

### vec3 vs vec2

- **Dimensions**: 3D vs 2D
- **Size**: 16 bytes (with padding) vs 8 bytes
- **Use when**: 3D graphics/physics vs 2D graphics/UI
- **Operations**: 3D cross product (vector) vs 2D cross product (scalar)
- **Complexity**: Higher memory/computation cost

### vec3 vs vec9

- **Dimensions**: 3D vs 9D
- **Purpose**: Graphics/physics vs Nikola consciousness (9D gradient state)
- **Size**: 16 bytes vs 36 bytes (or more with padding)
- **Complexity**: Simple 3D math vs complex 9D wave interference

---

## Related Topics

- [vec2](vec2.md) - 2D vectors for 2D graphics and physics
- [vec9](vec9.md) - 9D vectors for Nikola consciousness modeling
- [simd](simd.md) - SIMD vectorization for performance
- [float](float.md) - Component type (32-bit)
- [double](double.md) - Alternative component type (64-bit, more precision)

---

**Remember**:
1. **3D only** - use vec2 for 2D, vec9 for Nikola 9D
2. **Component-wise `*`** - NOT dot product (use `.dot()` method)
3. **Cross product returns vector** - perpendicular to both inputs (unlike vec2)
4. **Normalize for direction** - get unit vector (length = 1.0)
5. **Use `length_squared()` for comparisons** - avoid expensive sqrt
6. **SIMD-friendly** - operations vectorized as vec4 (with ignored W)
7. **16-byte aligned** - 12 bytes data + 4 bytes padding
8. **Right-hand rule** - cross product direction (curl fingers a→b, thumb points result)

**Common mistakes**:
- Using `*` expecting dot product (it's component-wise!)
- Forgetting to normalize direction vectors
- Not using `length_squared()` for distance comparisons
- Confusing cross product order (a × b ≠ b × a, they're opposite!)
- Mixing 2D (vec2) and 3D (vec3) without proper conversion

---

**Status**: ✅ Comprehensive  
**Session**: 16 (Vector Types)  
**Lines**: ~560  
**Cross-references**: Complete  
**Examples**: Executable Aria syntax
