# Mutable Borrow Syntax Design Considerations

## Current Status

**What Works Now:**
- `int32$:ref = $x` - Immutable borrow (safe reference)
- `wild int32@:pin = #x` - Pin reference (prevents GC/movement)
- `wild int32@:ptr = @x` - Raw address-of

## The Challenge with Mutable Borrows

### Rust's Approach
```rust
let x = 42;
let r1 = &x;      // immutable borrow
let r2 = &mut x;  // mutable borrow (ERROR if r1 exists)
```

**Why this might not feel right:**
- The `&` vs `&mut` distinction puts mutability on the *operator* side
- But the *type* (`&T` vs `&mut T`) also changes
- Creates two separate concepts that are tightly coupled

### Aria's Design Space

#### Option 1: Mutability in the Type (Current Direction)
```aria
int32$:ref = $x         // Immutable borrow, type is int32$
int32$mut:ref = $x      // Mutable borrow, type is int32$mut
```

**Pros:**
- Type system clearly distinguishes mutable from immutable references
- Follows Aria's `type:name` pattern consistently
- Type determines capability, not the operation

**Cons:**
- Need to define `$mut` as part of type syntax (lexer/parser work)
- Slightly more verbose

#### Option 2: Mutability as a Modifier (Like `wild`)
```aria
int32$:ref = $x              // Immutable borrow
mut int32$:ref = $x          // Mutable borrow
```

**Pros:**
- `mut` keyword already exists in language
- Separates "storage class" (mut, wild, const) from type
- Shorter syntax

**Cons:**
- `mut` on the reference, not the referent
- Potential confusion: does `mut` apply to the reference or what it points to?

#### Option 3: Different Operators
```aria
int32$:ref = $x         // Immutable borrow with $
int32$:ref = $!x        // Mutable borrow with $!
```

**Pros:**
- Clear visual distinction at the borrow site
- Type stays the same (`int32$`)
- Operator conveys intent

**Cons:**
- Introduces new operator `$!`
- Type system loses information about mutability
- Can't distinguish mutable vs immutable refs in function signatures

#### Option 4: Explicit Borrow Keywords
```aria
int32$:ref = borrow x         // Immutable
int32$:ref = borrow_mut x     // Mutable
```

**Pros:**
- Very explicit, no ambiguity
- Familiar to Rust developers
- Type could be the same or different

**Cons:**
- Very verbose
- Doesn't leverage Aria's symbol-based syntax philosophy

## Key Design Questions

1. **Should mutability be in the TYPE or the OPERATION?**
   - Type: Better for compile-time checking, function signatures
   - Operation: More concise, but loses information

2. **Should we follow Rust's model or forge our own path?**
   - Rust's model is proven and battle-tested
   - But Aria has different syntax philosophy (symbols over keywords)

3. **How does this interact with wild pointers?**
   ```aria
   wild int32@:ptr = ...      // Wild pointer (can write through it)
   int32$mut:ref = ...        // Mutable borrow (tracked by borrow checker)
   ```
   These serve different purposes but both allow mutation.

4. **Pattern matching and type inference implications?**
   - If `int32$` and `int32$mut` are different types, they need full type system support
   - Coercion rules: can `int32$mut` coerce to `int32$`?

## Aria's Philosophical Position

Looking at Aria's overall design:
- Uses symbols heavily: `@` `#` `$` `?` `!`
- Prefer terseness over verbosity (unlike Rust's keywords)
- Type-first syntax: `type:name` not `name: type`
- Memory safety through compile-time analysis

## Recommendation Areas to Explore

1. **Type-based approach with symbol syntax:**
   ```aria
   int32$:ref = $x        // int32$ type (immutable ref)
   int32$!:ref = $x       // int32$! type (mutable ref)
   ```

2. **Operator variant with type inference:**
   ```aria
   int32$:ref = $x        // $ for immutable
   int32$:ref = $!x       // $! for mutable (type stays int32$)
   ```

3. **Hybrid: Type determines capability, operator is just sugar:**
   ```aria
   int32$:ref = $x        // Compiler infers int32$ (immutable)
   int32$:mut_ref = $x    // Compiler infers int32$! (mutable)
   // The variable NAME hints at intent, type system enforces
   ```

## Current Implementation Notes

The borrow checker *implementation* is ready:
- Tracks mutable vs immutable in `Loan` struct (`is_mutable` bool)
- Enforces "1 mutable XOR N immutable" rule
- Just needs syntax to trigger `is_mutable = true`

Whatever syntax we choose, the backend just needs:
```cpp
bool is_mutable = /* determine from syntax */;
recordBorrow(host, reference, is_mutable, node);
```

## Next Steps

1. Decide on syntax philosophy (type-based vs operator-based)
2. Implement lexer/parser support for chosen syntax
3. Add tests for mutable borrow violations
4. Document the chosen approach and rationale
