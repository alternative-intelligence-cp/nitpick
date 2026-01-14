# Explicit Type Casting Design for Aria

## Philosophy: Zero Implicit, Explicit Everything

In Aria's Zero Implicit Conversion system, **all type conversions must be explicit**. No compiler magic, no guessing. You get what you ask for.

## Current Status

- ✅ **Typed literals work**: `int32:x = 42i32;`
- ✅ **Overflow detection works**: `int8:bad = 128i8;` → ERROR
- ✅ **Type matching enforced**: Must use correct suffix for type
- ❌ **Explicit casting NOT implemented yet** (syntax exists in comments but not functional)

## Proposed Syntax

Aria already has `@cast<TargetType>(value)` in stdlib documentation. This is clean and explicit:

```aria
// Widening (safe - no data loss)
int32:x = 42i32;
int64:y = @cast<int64>(x);  // Explicit: "I want to widen this to int64"

// Narrowing (unsafe - potential data loss)
int64:big = 1000i64;
int32:small = @cast<int32>(big);  // Explicit: "I understand this might truncate"

// Float conversions
flt64:pi = 3.14159f64;
flt32:pi32 = @cast<flt32>(pi);  // Explicit precision loss
int32:rounded = @cast<int32>(pi);  // Explicit truncation

// Signed/unsigned conversions
int32:signed = -42i32;
uint32:unsigned = @cast<uint32>(signed);  // Explicit: "I know what I'm doing"
```

## Design Principles

### 1. **Explicit Intent, Always**
Every cast **must** use `@cast<Type>(value)` - no shortcuts, no inference.

```aria
// ✅ GOOD - Crystal clear
int32:x = 42i32;
int64:y = @cast<int64>(x);

// ❌ BAD - Would be implicit (not allowed in Aria)
int32:x = 42i32;
int64:y = x;  // ERROR: Cannot assign int32 to int64 without explicit cast
```

### 2. **Safety Levels**

The compiler should categorize casts by safety:

#### **Safe Casts** (No data loss possible)
- Widening integer conversions: i8→i16→i32→i64→i128
- Unsigned to wider signed: u8→i16, u16→i32, u32→i64
- Float widening: f32→f64→f128
- Compiles with no warnings

```aria
int8:small = 127i8;
int64:big = @cast<int64>(small);  // ✅ Safe - always fits
```

#### **Checked Casts** (Runtime overflow check)
- Narrowing integer conversions: i64→i32, i32→i16, i16→i8
- Signed to unsigned (when value might be negative)
- Generates runtime check, panics on overflow

```aria
int64:x = 1000i64;
int8:y = @cast<int8>(x);  // ⚠️  Runtime check - panics if x > 127 or < -128
```

#### **Unchecked Casts** (Explicit "I know what I'm doing")
- Use `@cast_unchecked<Type>(value)` for performance-critical code
- Truncates/wraps without checks
- **Unsafe** - requires explicit opt-in

```aria
int64:x = 10000i64;
int8:y = @cast_unchecked<int8>(x);  // ⚠️  UNSAFE - truncates to (x & 0xFF)
```

### 3. **Float ↔ Integer Conversions**

These are inherently lossy and must be explicit:

```aria
// Float to int - truncates (no rounding by default)
flt64:pi = 3.14159f64;
int32:truncated = @cast<int32>(pi);  // → 3i32

// Int to float - potential precision loss for large ints
int64:big = 9007199254740993i64;  // Larger than f64 can represent exactly
flt64:approx = @cast<flt64>(big);  // ⚠️  Precision loss warning

// Rounding must be explicit
int32:rounded = @cast<int32>(round(pi));  // Use math function + cast
```

### 4. **Pointer Casts** (Already planned)

From stdlib examples, we see pointer casting:

```aria
wild void@:raw_ptr = @extern("aria_alloc", size);
wild int32@:typed_ptr = @cast<wild int32@>(raw_ptr);  // Explicit pointer cast
```

## Implementation Phases

### Phase 1: Basic Integer Casts ✅ **READY TO IMPLEMENT**
- [x] Same-size casts: i32→u32, u32→i32
- [x] Widening casts: i8→i16→i32→i64
- [x] Narrowing with runtime checks: i64→i32→i16→i8
- [x] Safe unsigned widening: u8→i16, u16→i32, u32→i64

### Phase 2: Float Casts
- [ ] Float widening: f32→f64
- [ ] Float narrowing: f64→f32 (precision loss warning)
- [ ] Int to float: i32→f64, i64→f64
- [ ] Float to int: f64→i32 (truncation)

### Phase 3: Unchecked Casts
- [ ] `@cast_unchecked<Type>(value)` for performance
- [ ] Generates LLVM bitcast/trunc without checks
- [ ] Marked as `unsafe` in safety checker

### Phase 4: Advanced Casts
- [ ] Pointer casts (already in stdlib comments)
- [ ] Enum casts: enum→int, int→enum
- [ ] TBB type casts (symmetric range types)
- [ ] LBIM casts (big integer types)

## Error Messages

Clear, helpful errors that explain what's needed:

```
❌ ERROR: Cannot assign value of type 'int32' to variable of type 'int64'
   |
12 |     int64:x = y;
   |               ^
   |
   = help: Use explicit cast: `@cast<int64>(y)`
   = note: Aria requires all type conversions to be explicit

❌ ERROR: Cast from 'int64' to 'int8' may overflow at runtime
   |
15 |     int8:small = @cast<int8>(big);
   |                  ^^^^^^^^^^^^^^^^^
   |
   = note: This cast inserts a runtime overflow check
   = help: Use @cast_unchecked<int8>(big) to skip check (unsafe)
```

## Examples

### Common Patterns

```aria
// Array indexing (common case)
int32:index = 5i32;
int64:arr_index = @cast<int64>(index);  // Widen for array access
int32:value = arr[arr_index];

// Arithmetic with different types
int32:a = 10i32;
int64:b = 20i64;
int64:result = (@cast<int64>(a) + b);  // Explicit widening

// Function call type matching
func:takes_i64 = void(int64:x) { ... };
int32:value = 42i32;
takes_i64(@cast<int64>(value));  // Explicit conversion for parameter

// Bitwise operations
int32:flags = 0x1234i32;
uint32:uflags = @cast<uint32>(flags);  // Explicit signed→unsigned
```

### Anti-Patterns (Won't Compile)

```aria
// ❌ Implicit widening
int32:x = 42i32;
int64:y = x;  // ERROR: No implicit conversions

// ❌ Implicit narrowing
int64:x = 1000i64;
int32:y = x;  // ERROR: No implicit conversions

// ❌ Implicit float→int
flt64:x = 3.14f64;
int32:y = x;  // ERROR: No implicit conversions

// ❌ Literal without suffix
int32:x = 42;  // ERROR: Literals must have type suffix (42i32)
```

## Benefits

1. **Zero Ambiguity**: Every type conversion is visible in code
2. **Audit Trail**: grep for `@cast` to find all type conversions
3. **Safety**: Overflow checks by default for narrowing casts
4. **Performance**: Opt-in to unchecked casts when needed
5. **Clarity**: Code reviewer sees "developer explicitly chose this conversion"

## Next Steps

1. Add `CastExpr` to AST (`include/frontend/ast/expr.h`)
2. Update lexer to recognize `@cast` keyword
3. Update parser to parse `@cast<Type>(expr)`
4. Add cast type checking in semantic analyzer
5. Generate LLVM IR for casts (with runtime checks)
6. Add overflow checking intrinsics for narrowing casts
7. Write comprehensive tests
8. Update stdlib to use explicit casts where needed

## Open Questions

1. Should we allow `@cast` between unrelated types? (probably no)
2. Should TBB→int cast check for error sentinel?
3. Should we have `@try_cast<Type>(value)` that returns Optional<Type>?
4. Should string conversions be casts or functions? (probably functions)

---

**Status**: Design complete, ready for implementation  
**Priority**: High - needed for real-world code  
**Estimated Effort**: 1-2 weeks for Phase 1  
