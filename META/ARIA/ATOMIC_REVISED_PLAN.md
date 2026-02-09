# atomic<T> Phase 2.3: REVISED Implementation Plan

**Date**: February 7, 2026  
**Status**: Ready to implement using existing features!

## What We Actually Have ✅

### 1. Preprocessor Macro System
- Full preprocessor at `src/frontend/preprocessor/`
- Macro definitions: `%macro NAME PARAM_COUNT ... %endmacro`
- Include system: `%include "file.aria"`
- Define constants: `%define NAME value`
- Context-local variables: `%$variable`

### 2. Class/Method Syntax via Macros (`lib/std/class.aria`)
```aria
%CLASS Point
  x: int64,
  y: int64
%END_CLASS

IMPL Point
  def new = Point(int64:x, int64:y) {
    Point:p;
    p.x = x;
    p.y = y;
    pass(p);
  }
  
  def magnitude = int64(Point:self) {
    pass(self.x * self.x + self.y * self.y);
  }
END_IMPL

// Usage:
Point:p = Point.new(10, 20);        // Expands to: Point_new(10, 20)
int64:mag = p.magnitude();         // Expands to: Point_magnitude(p)
```

**How it works**:
- `%CLASS` creates a struct
- `IMPL` block defines methods
- `def method_name` expands to `func ClassName_method_name`
- Instance methods take `self` as first param
- Static methods (like `new`) don't

### 3. Module System
- `pub` keyword for public functions - fully implemented
- `extern` keyword for external C functions - fully implemented  
- Parser handles both, generates correct linkage

### 4. Generic Types
- `atomic<int32>`, `atomic<bool>` already type-check ✅
- Type validation complete (Phase 2.2) ✅
- Can create struct instances with generic types

## The Revised Plan

Instead of aspirational "future API", we can implement atomic<T> **NOW** using:

1. **Extern C bindings** for runtime atomic operations
2. **Class macro system** for method-style API
3. **Per-type specialization** via macros

### Example: What We Can Actually Build

```aria
%include "std/class.aria"

// Extern runtime functions (implemented in C)
extern func aria_atomic_int32_create(int32) = @void;
extern func aria_atomic_int32_store(@void, int32, int32) = void;
extern func aria_atomic_int32_load(@void, int32) = int32;
extern func aria_atomic_int32_destroy(@void) = void;

// Wrapper struct
struct atomic_int32 {
  ptr: @void;  // Runtime handle
};

// Class-style methods using IMPL
IMPL atomic_int32
  def new = atomic_int32(int32:value) {
    atomic_int32:a;
    a.ptr = aria_atomic_int32_create(value);
    pass(a);
  }
  
  def store = void(atomic_int32:self, int32:value, int32:order) {
    aria_atomic_int32_store(self.ptr, value, order);
    pass();
  }
  
  def load = int32(atomic_int32:self, int32:order) {
    pass(aria_atomic_int32_load(self.ptr, order));
  }
  
  def destroy = void(atomic_int32:self) {
    aria_atomic_int32_destroy(self.ptr);
    pass();
  }
END_IMPL

// Usage:
func:test = int32() {
    atomic_int32:counter = atomic_int32.new(0);
    counter.store(42, 2);  // 2 = RELEASE
    int32:val = counter.load(1);  // 1 = ACQUIRE
    counter.destroy();
    pass(val);
};
```

### Memory Ordering Constants

```aria
%define RELAXED 0
%define ACQUIRE 1  
%define RELEASE 2
%define ACQ_REL 3
%define SEQ_CST 4
```

## Implementation Tasks

### Task 1: Create C Runtime Library (~2-3 hours)
**File**: `runtime/atomic.c` + `runtime/atomic.h`

Implement for each supported type:
- `aria_atomic_TYPE_create(value)` → `void*`
- `aria_atomic_TYPE_store(ptr, value, order)` → `void`
- `aria_atomic_TYPE_load(ptr, order)` → `TYPE`
- `aria_atomic_TYPE_exchange(ptr, value, order)` → `TYPE`
- `aria_atomic_TYPE_compare_exchange(ptr, expected, desired, success_order, failure_order)` → `bool`
- `aria_atomic_TYPE_fetch_add(ptr, value, order)` → `TYPE`
- `aria_atomic_TYPE_fetch_sub(ptr, value, order)` → `TYPE`
- `aria_atomic_TYPE_destroy(ptr)` → `void`

Types: int8, int16, int32, int64, uint8, uint16, uint32, uint64, bool (9 types × 8 functions = 72 functions, but many are similar)

### Task 2: Create Stdlib Wrapper (~2-3 hours)
**File**: `stdlib/atomic/int32.aria` (example for one type)

```aria
%include "std/class.aria"

// Extern declarations
extern func aria_atomic_int32_create(int32) = @void;
extern func aria_atomic_int32_store(@void, int32, int32) = void;
// ... etc

// Memory ordering constants
%define RELAXED 0
%define ACQUIRE 1
%define RELEASE 2
%define ACQ_REL 3
%define SEQ_CST 4

// Wrapper struct
struct atomic_int32 {
  ptr: @void;
};

// Methods
IMPL atomic_int32
  def new = atomic_int32(int32:value) { ... }
  def store = void(atomic_int32:self, int32:value, int32:order) { ... }
  def load = int32(atomic_int32:self, int32:order) { ... }
  // ... etc
END_IMPL
```

Repeat for all 9 types.

### Task 3: Testing (~1 hour)
Create tests using the class-style API:

```aria
%include "stdlib/atomic/int32.aria"

func:test_basic = int32() {
    atomic_int32:counter = atomic_int32.new(0);
    counter.store(42, SEQ_CST);
    int32:value = counter.load(SEQ_CST);
    counter.destroy();
    
    if value == 42 {
        pass(0);
    } else {
        pass(1);
    }
};
```

### Task 4: Documentation (~30 min)
Update docs with real working examples.

## Advantages of This Approach

1. **Works NOW** - Uses existing features, no compiler changes needed
2. **Clean API** - `counter.store(42, RELEASE)` looks great
3. **Type safe** - Each type has its own struct
4. **Zero overhead** - Macros expand at compile time
5. **Real atomics** - C runtime uses actual atomic instructions
6. **Testable** - Can verify lock-free behavior in real programs

## Disadvantages

1. **Manual per-type** - Need to create wrapper for each of 9 types (but they're identical except type name)
2. **Not truly generic** - Can't do `atomic<MyType>`, only built-in atomic_TYPE structs
3. **Constant integers** - Memory order is int32, not enum (but %define makes it readable)

## Could We Make It Generic?

**Potential future enhancement** using macro generation:

```aria
%macro DEFINE_ATOMIC_TYPE 1
  extern func aria_atomic_%1_create(%1) = @void;
  extern func aria_atomic_%1_store(@void, %1, int32) = void;
  // ... etc
  
  struct atomic_%1 {
    ptr: @void;
  };
  
  IMPL atomic_%1
    def new = atomic_%1(%1:value) { ... }
    // ... etc
  END_IMPL
%endmacro

// Generate all types:
%DEFINE_ATOMIC_TYPE int32
%DEFINE_ATOMIC_TYPE int64
%DEFINE_ATOMIC_TYPE bool
// ... etc
```

This reduces duplication significantly!

## Recommendation

**Let's implement this!** Time estimate: ~6-7 hours total

1. C runtime library: 2-3 hours (straightforward, using C11 `<stdatomic.h>`)
2. Aria wrappers: 2-3 hours (repetitive but simple)
3. Tests: 1 hour
4. Docs: 30 min

We'll have:
- Real working atomic operations
- Clean method-style API
- Full test coverage
- Actual lock-free concurrency

The API will be slightly different than the aspirational `stdlib/atomic.aria`, but it'll be **real and working**.

## Next Steps

Should we:
**A)** Implement this practical atomic<T> using existing features (~6-7 hours)  
**B)** Move to another P0 feature and revisit atomic later  
**C)** Something else?

I recommend **A** - we're so close, and this will prove out the macro system + extern bindings in a real-world scenario!
