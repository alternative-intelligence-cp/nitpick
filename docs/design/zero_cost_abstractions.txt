# Zero-Cost Abstractions: The `mute` Keyword

## Philosophy

Aria's design goal is to combine the best parts of multiple languages:
- **NASM**: Powerful macros, zero abstraction cost
- **Zig**: Compile-time execution (comptime)
- **Rust**: Memory safety without runtime overhead
- **C**: Explicit control, predictable performance

The `mute` keyword is Aria's approach to **compile-time safety with runtime silence** - abstractions that enforce correctness during compilation but disappear (go "mute"/silent) at runtime.

---

## The "$mut → mute → silent" Connection

The pattern started with the mutable borrow operator `$`:
- `$x` = mutable borrow (visible reference)
- `!$x` = immutable borrow (negated mutability)

This sparked the **"mute"** concept:
- "Mute" as in **silent** - hidden from runtime
- "Mute" as in **type-silenced** - erased after compilation
- "Mute" as **zero-cost** - no runtime representation

---

## Concept: Compile-Time Types

What if `mute` creates types that exist ONLY at compile time?

### Use Case 1: Type-Safe Units (Zero Runtime Cost)

```aria
// Define unit types with zero runtime overhead
mute type:Meters = float64;
mute type:Feet = float64;
mute type:Seconds = float64;
mute type:MetersPerSecond = float64;

// Type-checked at compile time
func:calculate_velocity = MetersPerSecond(Meters:distance, Seconds:time) {
    MetersPerSecond:velocity = distance / time;  // OK
    pass(velocity);
};

// At runtime: just float64 division - zero overhead!
// But at compile time: full type safety!

Meters:m = 100.0;
Feet:f = 328.0;

// ERROR at compile time: Cannot assign Feet to Meters
// Meters:bad = f;  // Prevented!

// ERROR at compile time: Wrong unit types
// calculate_velocity(f, m);  // Type mismatch!
```

**Runtime cost**: ZERO. Just regular float64 operations.  
**Compile-time benefit**: Can't mix meters and feet!

### Use Case 2: Phantom Types for State Machines

```aria
// State machine with compile-time state tracking
mute type:Unlocked = void;
mute type:Locked = void;

struct:Door<State> = {
    bool:is_open;
};

func:new_door = Door<Locked>() {
    Door<Locked>:door = { is_open = false };
    pass(door);
};

func:unlock = Door<Unlocked>(Door<Locked>:locked_door) {
    Door<Unlocked>:unlocked = { is_open = locked_door.is_open };
    pass(unlocked);
};

func:open = void(Door<Unlocked>:door) {
    door.is_open = true;
};

// Usage:
Door<Locked>:d1 = new_door();
// open(d1);  // ERROR: Cannot open locked door!

Door<Unlocked>:d2 = unlock(d1);
open(d2);  // OK!
```

**Runtime**: Door is just `struct { bool:is_open; }` - the state is GONE!  
**Compile-time**: Can't call open() on a locked door!

### Use Case 3: Newtype Pattern (Zero Cost)

```aria
// Different IDs that can't be mixed
mute type:UserId = int32;
mute type:ProductId = int32;
mute type:OrderId = int32;

func:get_user = User(UserId:id) {
    // ... database lookup
};

func:get_product = Product(ProductId:id) {
    // ... database lookup
};

UserId:user = 42;
ProductId:product = 100;

// ERROR: Type mismatch
// get_user(product);  // Prevented at compile time!

// But at runtime: just int32 values - no wrapper overhead!
```

### Use Case 4: Branded Types for Security

```aria
// Prevent mixing sanitized and unsanitized strings
mute type:SanitizedHTML = string;
mute type:UnsafeHTML = string;

func:sanitize = SanitizedHTML(UnsafeHTML:input) {
    // ... sanitization logic
    SanitizedHTML:safe = cleaned_string;
    pass(safe);
};

func:render_html = void(SanitizedHTML:html) {
    // Safe to render - type system guarantees it's sanitized
};

UnsafeHTML:user_input = "<script>alert('xss')</script>";

// ERROR: Cannot pass UnsafeHTML to render_html
// render_html(user_input);  // Compile error!

SanitizedHTML:safe = sanitize(user_input);
render_html(safe);  // OK!
```

**Runtime**: Both are just strings - zero overhead!  
**Compile-time**: Can't accidentally render unsanitized HTML!

---

## Syntax Exploration

### Type Definitions

```aria
// Define a mute type alias
mute type:TypeName = BaseType;

// Mute generics (state machines)
mute type:State = enum { Locked, Unlocked };
```

### Compile-Time Assertions

```aria
// Maybe mute can also do compile-time checks?
mute assert(sizeof(int32) == 4);  // Verified at compile time
mute assert(Meters != Feet);      // Type inequality check
```

### Compile-Time Constants

```aria
// Like Zig's comptime, but using mute
mute int32:MAX_SIZE = 1024;       // Compile-time constant
mute string:VERSION = "0.1.0";    // No runtime string storage
```

---

## Implementation Strategy

### Phase 1: Type Aliases (Simplest)
1. Parser recognizes `mute type:Name = Base;`
2. Type checker creates TypeAlias node
3. Type checking enforces distinct types
4. Codegen treats as underlying type (erases the wrapper)

### Phase 2: Phantom Generics
1. Allow `Type<MuteType>` in generic parameters
2. Type checker validates state transitions
3. Codegen strips phantom parameters

### Phase 3: Compile-Time Evaluation
1. Evaluate `mute` expressions at compile time
2. Fold constants and eliminate dead code
3. Similar to Zig's comptime

---

## Comparison to Other Languages

| Language | Feature | Runtime Cost | Aria `mute` |
|----------|---------|--------------|-------------|
| Rust | Newtype | Zero (with #[repr(transparent)]) | `mute type:X = Y` |
| Haskell | Phantom types | Zero (after optimization) | `Type<MuteState>` |
| Zig | comptime | Zero | `mute expr` |
| C++ | constexpr | Zero | `mute const` |
| TypeScript | Branded types | Erased (no runtime) | Same concept! |

---

## Open Questions

1. **Syntax**: Is `mute` the right keyword? Alternatives:
   - `phantom` (Haskell-inspired)
   - `static` (C-inspired, but overloaded)
   - `compile` or `comptime` (Zig-inspired)
   - `zero` (explicit zero-cost)
   - `mute` ✅ (follows the $ → mute pattern, means "silent")

2. **Scope**: Should `mute` apply to:
   - Types only? ✅ (start here)
   - Variables? (compile-time constants)
   - Functions? (compile-time evaluation)
   - Expressions? (constant folding)

3. **Interop**: How do `mute` types interact with:
   - FFI/C functions? (erased to base type)
   - Reflection? (no runtime type info)
   - Generics? (phantom parameters)

---

## Decision Points

### Should we implement this for 0.1.0?

**Arguments FOR:**
- Fits the language philosophy (NASM macros, Zig comptime)
- Solves real problems (type safety without cost)
- Simple to start (just type aliases)
- Can be expanded later (phantom types, comptime)

**Arguments AGAINST:**
- Feature creep (stay focused on 0.1.0 core)
- Complex to get right (type erasure, generics interaction)
- Can be added in 0.2.0

**Recommendation**: 
- **Document the concept now** ✅ (this file)
- **Add basic type aliases in 0.1.0** if time permits
- **Save full comptime for 0.2.0**

---

## Example: Full Program

```aria
// Type-safe dimensional analysis with ZERO runtime cost
mute type:Meters = float64;
mute type:Seconds = float64;
mute type:MetersPerSecond = float64;
mute type:MetersPerSecondSquared = float64;

// Physics functions with type safety
func:velocity = MetersPerSecond(Meters:distance, Seconds:time) {
    pass(distance / time);
};

func:acceleration = MetersPerSecondSquared(MetersPerSecond:delta_v, Seconds:time) {
    pass(delta_v / time);
};

// Position with dimensional analysis
func:position = Meters(
    Meters:initial_pos,
    MetersPerSecond:velocity,
    MetersPerSecondSquared:accel,
    Seconds:time
) {
    Meters:pos = initial_pos + (velocity * time) + (0.5 * accel * time * time);
    pass(pos);
};

func:main = int32() {
    Meters:x0 = 0.0;
    MetersPerSecond:v0 = 5.0;
    MetersPerSecondSquared:a = 9.8;
    Seconds:t = 2.0;
    
    Meters:final_pos = position(x0, v0, a, t);
    
    // This would be a compile error:
    // Meters:bad = position(t, x0, v0, a);  // Wrong argument order!
    
    // At runtime: just float64 math
    // At compile time: full dimensional analysis!
    
    int32:zero = 0;
    pass(zero);
};
```

**Compiler output**: Regular C code with float64, no wrapper types!  
**Safety**: Can't mix up meters, seconds, and acceleration at compile time!

---

## Conclusion

The `mute` keyword embodies Aria's philosophy:
- **NASM-style**: Zero runtime cost, compile-time power
- **Zig-style**: Compile-time computation
- **Rust-style**: Safety without overhead
- **Aria-style**: Simple syntax, powerful semantics

It's the missing piece between "write safe code" and "run fast code" - you get BOTH!

---

**Status**: CONCEPT EXPLORATION  
**Target**: 0.1.0 (basic type aliases) or 0.2.0 (full comptime)  
**Date**: December 20, 2024  
**Authors**: Randy Trueman & Aria Echo  
**Inspiration**: The "$mut → mute → silent" pattern revelation
