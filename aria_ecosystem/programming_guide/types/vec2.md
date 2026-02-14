# The `vec2` Type (2D Vector)

**Category**: Types → Vectors → 2D  
**Syntax**: `vec2`  
**Purpose**: Two-dimensional vector for 2D graphics, physics, and mathematics  
**First Available**: Aria v0.0.1  
**Status**: ✅ Fully implemented with SIMD optimization

---

## Overview

`vec2` is Aria's **2-dimensional vector type**, representing points or directions in 2D space with `x` and `y` components. This is a fundamental type for 2D graphics, game development, physics simulations, and UI layout.

**Key Characteristics**:
- **2 components**: `x` (horizontal) and `y` (vertical)
- **Component type**: Typically `float` or `double` (defaults to `float`)
- **Size**: 8 bytes (2 × 4-byte floats) or 16 bytes (2 × 8-byte doubles)
- **SIMD optimized**: Can use SSE/AVX instructions for parallel operations
- **Memory layout**: Contiguous `[x, y]` (cache-friendly)
- **Alignment**:  8-byte (float) or 16-byte (double)

**Common uses**:
- ✅ 2D positions (screen coordinates, world positions)
- ✅ 2D velocities and accelerations (physics)
- ✅ Direction vectors (movement, facing)
- ✅ Texture coordinates (UV mapping)
- ✅ UI offsets and sizes
- ✅ 2D force vectors (gravity, thrust)

**When NOT to use**:
- ❌ 3D graphics/physics (use `vec3` instead)
- ❌ Nikola consciousness (use `vec9` for 9D state)
- ❌ Single scalar values (use `float` or `double`)

---

## Declaration & Initialization

### Basic Declaration

```aria
// Constructor with explicit components
vec2:position = vec2(10.0, 20.0);  // x=10.0, y=20.0

// Named components for clarity
vec2:screen_pos = vec2(640.0, 480.0);  // Center of 1280×960 screen

// Texture coordinates (typical range 0.0 to 1.0)
vec2:uv = vec2(0.5, 0.5);  // Center of texture
```

### Component Access

```aria
vec2:pos = vec2(100.0, 200.0);

// Direct member access
float:x_coord = pos.x;  // 100.0
float:y_coord = pos.y;  // 200.0

// Modification
pos.x = 150.0;  // Now vec2(150.0, 200.0)
pos.y += 50.0;  // Now vec2(150.0, 250.0)
```

### Special Constructors

```aria
// Uniform value (both components same)
vec2:ones = vec2(1.0);  // vec2(1.0, 1.0)
vec2:twos = vec2(2.0);  // vec2(2.0, 2.0)

// Zero vector
vec2:origin = vec2::ZERO;  // vec2(0.0, 0.0)

// Unit vectors (length = 1.0)
vec2:right = vec2::RIGHT;   // vec2(1.0, 0.0) - positive X axis
vec2:up = vec2::UP;         // vec2(0.0, 1.0) - positive Y axis
vec2:left = vec2::LEFT;     // vec2(-1.0, 0.0) - negative X axis
vec2:down = vec2::DOWN;     // vec2(0.0, -1.0) - negative Y axis

// One vector
vec2:ones = vec2::ONE;  // vec2(1.0, 1.0)
```

---

## Arithmetic Operations

### Component-wise Addition/Subtraction

```aria
vec2:a = vec2(10.0, 20.0);
vec2:b = vec2(5.0, 15.0);

// Addition (component-wise)
vec2:sum = a + b;  // vec2(15.0, 35.0)

// Subtraction (component-wise)
vec2:diff = a - b;  // vec2(5.0, 5.0)

// Negation
vec2:neg = -a;  // vec2(-10.0, -20.0)
```

### Scalar Multiplication/Division

```aria
vec2:velocity = vec2(100.0, 50.0);  // pixels per second

// Scale by scalar
vec2:doubled = velocity * 2.0;  // vec2(200.0, 100.0)
vec2:halved = velocity / 2.0;   // vec2(50.0, 25.0)

// Compound assignment
velocity *= 1.5;   // vec2(150.0, 75.0)
velocity /= 3.0;   // vec2(50.0, 25.0)
```

### Component-wise Multiplication/Division

```aria
vec2:a = vec2(10.0, 20.0);
vec2:b = vec2(2.0, 4.0);

// Component-wise multiplication (Hadamard product)
vec2:product = a * b;  // vec2(20.0, 80.0) - NOT dot product!

// Component-wise division
vec2:quotient = a / b;  // vec2(5.0, 5.0)
```

**Note**: `*` between two vec2 is component-wise, NOT dot product! Use `dot()` method for dot product.

---

## Vector Operations

### Dot Product

The **dot product** measures how aligned two vectors are:

```aria
vec2:a = vec2(3.0, 4.0);
vec2:b = vec2(1.0, 0.0);

float:dot = a.dot(b);  // 3.0*1.0 + 4.0*0.0 = 3.0

// dot(a, b) = |a| * |b| * cos(θ)
// - Positive: vectors point same general direction (θ < 90°)
// - Zero: vectors perpendicular (θ = 90°)
// - Negative: vectors point opposite directions (θ > 90°)
```

**Common uses**:
- Check if vectors point same direction
- Project vector onto another
- Calculate angle between vectors
- Determine if objects face toward/away from each other

```aria
// Check if moving toward target
vec2:position = vec2(100.0, 100.0);
vec2:target = vec2(200.0, 150.0);
vec2:velocity = vec2(50.0, 25.0);

vec2:to_target = (target - position).normalize();
float:alignment = velocity.normalize().dot(to_target);

if (alignment > 0.0) {
    stdout.write("Moving toward target\n");
} else {
    stdout.write("Moving away from target\n");
}
```

### Cross Product (2D→Scalar)

In 2D, the **cross product** returns a scalar (the Z component if vectors were 3D):

```aria
vec2:a = vec2(3.0, 4.0);
vec2:b = vec2(1.0, 2.0);

float:cross = a.cross(b);  // (3.0*2.0) - (4.0*1.0) = 2.0

// Geometric meaning:
// - Positive: b is counter-clockwise from a
// - Zero: vectors parallel
// - Negative: b is clockwise from a
```

**Common uses**:
- Determine rotation direction (CW vs CCW)
- Calculate area of parallelogram formed by vectors
- Check which side of a line a point is on

```aria
// Determine turn direction
vec2:forward = vec2(1.0, 0.0);
vec2:desired = vec2(1.0, 1.0);  // 45° turn

float:turn = forward.cross(desired);
if (turn > 0.0) {
    stdout.write("Turn left (CCW)\n");
} else if (turn < 0.0) {
    stdout.write("Turn right (CW)\n");
} else {
    stdout.write("No turn needed\n");
}
```

### Length (Magnitude)

```aria
vec2:velocity = vec2(3.0, 4.0);

float:speed = velocity.length();  // sqrt(3²+4²) = 5.0

// Squared length (faster, avoids sqrt)
float:speed_sq = velocity.length_squared();  // 3²+4² = 25.0
```

**Optimization tip**: Use `length_squared()` for comparisons to avoid expensive `sqrt()`:

```aria
vec2:position = vec2(100.0, 100.0);
vec2:target = vec2(150.0, 150.0);
const float:MIN_DISTANCE_SQ = 25.0;  // 5.0² = 25.0

vec2:diff = target - position;
if (diff.length_squared() < MIN_DISTANCE_SQ) {
    stdout.write("Close enough!\n");  // Avoid sqrt!
}
```

### Normalization

**Normalizing** a vector gives a unit vector (length = 1.0) in the same direction:

```aria
vec2:velocity = vec2(300.0, 400.0);  // Length = 500.0

vec2:direction = velocity.normalize();  // vec2(0.6, 0.8), length = 1.0

// Manual normalization
float:len = velocity.length();
vec2:manual_dir = velocity / len;  // Same result
```

**Common uses**:
- Get pure direction (ignore magnitude)
- Unit vectors for consistent movement speed
- Basis vectors for coordinate systems

```aria
// Constant-speed movement toward target
vec2:position = vec2(100.0, 100.0);
vec2:target = vec2(200.0, 150.0);
const float:SPEED = 5.0;  // pixels per frame

vec2:direction = (target - position).normalize();
vec2:velocity = direction * SPEED;  // Always moves at SPEED

position += velocity;  // Move toward target
```

### Distance

```aria
vec2:point_a = vec2(10.0, 20.0);
vec2:point_b = vec2(40.0, 60.0);

float:distance = point_a.distance(point_b);  // sqrt((40-10)² + (60-20)²) = 50.0

// Equivalent to:
float:manual_dist = (point_b - point_a).length();
```

### Linear Interpolation (Lerp)

**Lerp** smoothly interpolates between two vectors:

```aria
vec2:start = vec2(0.0, 0.0);
vec2:end = vec2(100.0, 100.0);

vec2:quarter = start.lerp(end, 0.25);  // vec2(25.0, 25.0)
vec2:half = start.lerp(end, 0.5);      // vec2(50.0, 50.0)
vec2:three_quarter = start.lerp(end, 0.75);  // vec2(75.0, 75.0)

// lerp(a, b, t) = a + t*(b - a) = a*(1-t) + b*t
```

**Common uses**:
- Smooth animation between positions
- Camera movement
- UI transitions

```aria
// Smooth camera follow
vec2:camera_pos = vec2(100.0, 100.0);
vec2:player_pos = vec2(200.0, 150.0);
const float:LERP_SPEED = 0.1;  // 10% per frame

camera_pos = camera_pos.lerp(player_pos, LERP_SPEED);  // Smooth follow
```

---

## Common Patterns

### Movement & Physics

```aria
// Simple 2D physics
vec2:position = vec2(100.0, 100.0);
vec2:velocity = vec2(50.0, -20.0);  // pixels/second
vec2:acceleration = vec2(0.0, 98.0);  // gravity (pixels/s²)
const float:dt = 0.016;  // 60 FPS timestep

// Update velocity (Euler integration)
velocity += acceleration * dt;

// Update position
position += velocity * dt;

// Clamp to screen bounds
if (position.y > 600.0) {
    position.y = 600.0;
    velocity.y = 0.0;  // Stop at ground
}
```

### Direction Vectors

```aria
// Calculate direction from one point to another
vec2:enemy_pos = vec2(200.0, 150.0);
vec2:player_pos = vec2(100.0, 100.0);

vec2:to_player = (player_pos - enemy_pos).normalize();

// Move toward player at constant speed
const float:CHASE_SPEED = 3.0;
enemy_pos += to_player * CHASE_SPEED;
```

### Clamping to Bounds

```aria
// Clamp position to screen rectangle
vec2:position = vec2(1300.0, 500.0);  // Off-screen!
const vec2:min_bounds = vec2(0.0, 0.0);
const vec2:max_bounds = vec2(1280.0, 720.0);

position.x = clamp(position.x, min_bounds.x, max_bounds.x);  // 1280.0
position.y = clamp(position.y, min_bounds.y, max_bounds.y);  // 500.0

// Now position = vec2(1280.0, 500.0) - clamped to screen
```

### Reflecting Vectors

```aria
// Reflect velocity off surface (e.g., ball bouncing off wall)
vec2:velocity = vec2(50.0, -30.0);
vec2:surface_normal = vec2(0.0, 1.0);  // Horizontal surface (facing up)

// Reflection formula: v' = v - 2*(v·n)*n
float:dot = velocity.dot(surface_normal);
vec2:reflected = velocity - (surface_normal * (2.0 * dot));

// Result: vec2(50.0, 30.0) - velocity reversed in Y, X unchanged
```

---

## Use Cases

### 2D Graphics

```aria
// Screen pixel coordinates
vec2:mouse_pos = vec2(640.0, 360.0);  // Center of 1280×720 screen

// Texture coordinates (UV mapping)
vec2:tex_coord = vec2(0.5, 0.5);  // Center of texture (range 0.0-1.0)

// Sprite position
vec2:sprite_pos = vec2(100.0, 200.0);
vec2:sprite_size = vec2(64.0, 64.0);
```

### Game Development

```aria
// Player state
vec2:player_position = vec2(320.0, 480.0);
vec2:player_velocity = vec2(0.0, 0.0);
const float:MOVE_SPEED = 200.0;  // pixels/second

// Input handling (WASD movement)
vec2:input_direction = vec2(0.0, 0.0);
if (key_pressed('W')) { input_direction.y -= 1.0; }
if (key_pressed('S')) { input_direction.y += 1.0; }
if (key_pressed('A')) { input_direction.x -= 1.0; }
if (key_pressed('D')) { input_direction.x += 1.0; }

// Normalize to prevent faster diagonal movement
if (input_direction.length_squared() > 0.0) {
    input_direction = input_direction.normalize();
}

player_velocity = input_direction * MOVE_SPEED;
```

### UI Layout

```aria
// Button position and size
vec2:button_position = vec2(100.0, 50.0);
vec2:button_size = vec2(200.0, 60.0);

// Check if mouse is over button
vec2:mouse = vec2(150.0, 70.0);

bool:mouse_over = (
    mouse.x >= button_position.x &&
    mouse.x <= button_position.x + button_size.x &&
    mouse.y >= button_position.y &&
    mouse.y <= button_position.y + button_size.y
);
```

### Physics Simulation

```aria
// Circular orbit simulation
vec2:planet_pos = vec2(0.0, 0.0);  // At origin
vec2:satellite_pos = vec2(100.0, 0.0);  // 100 units away
vec2:satellite_vel = vec2(0.0, 50.0);  // Tangent velocity
const float:GRAVITY = 1000.0;
const float:dt = 0.01;  // Timestep

// Gravitational acceleration toward planet
vec2:to_planet = planet_pos - satellite_pos;
float:dist_sq = to_planet.length_squared();
vec2:accel = to_planet.normalize() * (GRAVITY / dist_sq);

// Update orbit
satellite_vel += accel * dt;
satellite_pos += satellite_vel * dt;
```

---

## Nikola Consciousness Applications

While Nikola primarily uses 9D vec9 for full consciousness state, **vec2 can represent simplified 2D emotional/cognitive spaces**:

### 2D Emotional Space

```aria
// Emotional state on 2D valence-arousal model
vec2:emotional_state = vec2(0.0, 0.0);  // Neutral (valence=0, arousal=0)

// Valence: -1.0 (negative) to +1.0 (positive)
// Arousal: -1.0 (calm) to +1.0 (excited)

vec2:happy_excited = vec2(0.8, 0.7);   // High valence, high arousal (joy)
vec2:sad_calm = vec2(-0.6, -0.4);      // Low valence, low arousal (sadness)
vec2:angry_excited = vec2(-0.5, 0.9);  // Low valence, high arousal (anger)
vec2:content_calm = vec2(0.5, -0.2);   // Mid valence, low arousal (contentment)

// Smooth emotional transition
emotional_state = emotional_state.lerp(happy_excited, 0.05);  // 5% per frame
```

### Simplified Cognitive State

```aria
// 2D cognitive focus (attention vs processing depth)
vec2:cognitive = vec2(0.0, 0.0);

// X-axis: Attention breadth (-1.0 narrow focus, +1.0 wide awareness)
// Y-axis: Processing depth (-1.0 shallow/fast, +1.0 deep/slow)

vec2:focused_deep = vec2(-0.8, 0.9);    // Narrow focus, deep thought
vec2:aware_quick = vec2(0.7, -0.6);     // Wide awareness, quick reactions
vec2:meditative = vec2(0.0, 0.8);       // Balanced attention, deep processing
```

---

## Performance

### SIMD Optimization

vec2 operations can use **SIMD instructions** (SSE/AVX) for performance:

```aria
// Single operation (scalar)
vec2:a = vec2(1.0, 2.0);
vec2:b = vec2(3.0, 4.0);
vec2:c = a + b;  // ~4 CPU cycles (SIMD add)

// Array of vec2 (SIMD batch processing possible)
vec2[1000]:positions;
vec2[1000]:velocities;

for (tbb32:i = 0i32; i < 1000i32; ++i) {
    positions[i] += velocities[i];  // Compiler can vectorize (2-4 ops at once)
}
```

**Performance**: ~2-4× faster than scalar floats when SIMD-optimized.

### Cache Efficiency

```aria
// Good: Contiguous vec2 array (cache-friendly)
vec2[10000]:particle_positions;  // 80 KB contiguous memory

// Better: Structure of Arrays (SoA) for maximum cache efficiency
struct:ParticleSystem {
    float[10000]:x_positions;  // 40 KB contiguous
    float[10000]:y_positions;  // 40 KB contiguous
    // Better cache locality when only accessing X or Y
};
```

---

## Memory Layout

```aria
vec2:position = vec2(100.0, 200.0);

// Memory layout (8 bytes for float components):
// [float x = 100.0] [float y = 200.0]
//  ↑ 4 bytes         ↑ 4 bytes
```

**Alignment**: Typically 8-byte aligned (for vec2<float>) or 16-byte aligned (for vec2<double>).

**Padding**: No padding within vec2 (components are contiguous).

---

## Type Conversions

```aria
// vec2 → individual components
vec2:pos = vec2(10.0, 20.0);
float:x = pos.x;
float:y = pos.y;

// Individual components → vec2
float:x_val = 5.0;
float:y_val = 10.0;
vec2:reconstructed = vec2(x_val, y_val);

// Array subscript access (alternative to .x/.y)
float:first = pos[0];   // 10.0 (same as pos.x)
float:second = pos[1];  // 20.0 (same as pos.y)
```

---

## Comparison Operations

```aria
vec2:a = vec2(10.0, 20.0);
vec2:b = vec2(10.0, 20.0);
vec2:c = vec2(5.0, 25.0);

// Equality (component-wise exact match)
bool:equal = (a == b);  // TRUE (both components match)
bool:not_equal = (a != c);  // TRUE (components differ)

// Comparison operators work component-wise
// (less useful for vectors, typically use length comparisons instead)
```

**Note**: For approximate equality (floating-point safe), use epsilon comparison:

```aria
const float:EPSILON = 0.001;

func:almost_equal = bool(vec2:a, vec2:b) {
    vec2:diff = a - b;
    pass(diff.length_squared() < EPSILON * EPSILON);
};
```

---

## vs Other Vector Types

### vec2 vs vec3

- **Dimensions**: 2D vs 3D
- **Size**: 8 bytes vs 12 bytes (float) or 16 bytes (aligned)
- **Use when**: 2D graphics/UI vs 3D graphics/physics
- **Operations**: 2D cross product (scalar) vs 3D cross product (vector)

### vec2 vs vec9

- **Dimensions**: 2D vs 9D
- **Purpose**: Graphics/physics vs Nikola consciousness (9D gradient state)
- **Size**: 8 bytes vs 36 bytes (float)
- **Complexity**: Simple 2D math vs complex 9D wave interference

---

## Related Topics

- [vec3](vec3.md) - 3D vectors for 3D graphics and physics
- [vec9](vec9.md) - 9D vectors for Nikola consciousness modeling
- [simd](simd.md) - SIMD vectorization for performance
- [float](float.md) - Component type (32-bit)
- [double](double.md) - Alternative component type (64-bit, more precision)

---

**Remember**:
1. **2D only** - use vec3 for 3D applications
2. **Component-wise `*`** - NOT dot product (use `.dot()` method)
3. **Normalize for direction** - get unit vector (length = 1.0)
4. **Use `length_squared()` for comparisons** - avoid expensive sqrt
5. **SIMD-friendly** - operations can be vectorized for performance
6. **Contiguous memory** - `[x, y]` layout (cache-efficient)
7. **8-byte size** - (float components) or 16 bytes (double)

**Common mistakes**:
- Using `*` expecting dot product (it's component-wise!)
- Forgetting to normalize direction vectors
- Not using `length_squared()` for distance comparisons
- Mixing 2D (vec2) and 3D (vec3) operations

---

**Status**: ✅ Comprehensive  
**Session**: 16 (Vector Types)  
**Lines**: ~540  
**Cross-references**: Complete  
**Examples**: Executable Aria syntax
