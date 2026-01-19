# Generic Star Prefix (Alternative Syntax)

**Category**: Functions → Generics → Syntax  
**Syntax**: `<*T>` instead of `<T>`  
**Status**: Alternative/experimental syntax (check Aria specification)

---

## What is Star Prefix?

The **star prefix** `*T` is an alternative way to write generic type parameters. Instead of `<T>`, you can write `<*T>`.

---

## Basic Syntax

```aria
// Standard syntax
fn identity<T>(value: T) -> T {
    return value;
}

// Star prefix syntax (if supported)
fn identity<*T>(value: T) -> T {
    return value;
}
```

---

## Functionally Identical

Both syntaxes do **exactly the same thing**:

```aria
// These are equivalent:
fn max<T>(a: T, b: T) -> T where T: Comparable { }
fn max<*T>(a: T, b: T) -> T where T: Comparable { }

// Both create the same function
```

---

## Multiple Parameters

```aria
// Standard
fn pair<T, U>(first: T, second: U) -> (T, U) { }

// Star prefix
fn pair<*T, *U>(first: T, second: U) -> (T, U) { }
```

---

## On Structs

```aria
// Standard
struct Box<T> {
    value: T
}

// Star prefix
struct Box<*T> {
    value: T
}
```

---

## On Impl Blocks

```aria
// Standard
impl<T> Box<T> {
    fn get() -> T {
        return self.value;
    }
}

// Star prefix
impl<*T> Box<T> {
    fn get() -> T {
        return self.value;
    }
}
```

---

## Why Star Prefix?

The star prefix is meant to **emphasize** that `T` is a **type variable**:

```aria
// Standard: T looks like a type name
fn process<T>(value: T) { }

// Star: Makes it obvious T is a placeholder
fn process<*T>(value: T) { }
```

---

## When to Use

### Use Standard `<T>` When:

- ✅ Following common conventions
- ✅ Matching Rust/other language syntax
- ✅ Standard in documentation/examples
- ✅ Team/project doesn't use star prefix

### Use Star `<*T>` When:

- ✅ Emphasizing type variables
- ✅ Project convention uses it
- ✅ Distinguishing from concrete types
- ✅ Personal/team preference

---

## Mixed Usage Not Recommended

```aria
// Don't mix styles in same codebase
fn identity<T>(value: T) -> T { }      // Standard
fn wrap<*U>(value: U) -> Box<U> { }    // Star

// Pick one and stick with it!
```

---

## With Constraints

```aria
// Standard
fn max<T>(a: T, b: T) -> T where T: Comparable { }

// Star prefix
fn max<*T>(a: T, b: T) -> T where T: Comparable { }

// Both work the same
```

---

## Type Specification

```aria
// Standard explicit types
Result: i32 = identity::<i32>(42);

// Star prefix explicit types
Result: i32 = identity::<*i32>(42);

// Inference works the same
Result: i32 = identity(42);
```

---

## Compilation

Both compile to **identical** code:

```aria
// This code:
fn add<*T>(a: T, b: T) -> T { return a + b; }

// Compiles exactly the same as:
fn add<T>(a: T, b: T) -> T { return a + b; }

// No performance difference
// No size difference
// No runtime difference
```

---

## Community Convention

**Most Aria code uses standard `<T>` syntax**:

- Standard in Rust (Aria's inspiration)
- Familiar to developers
- Shorter to type
- Less visual noise

**Star prefix is rare but valid**:

- Some teams prefer explicit marking
- Might be used in teaching materials
- Experimental or specialized code

---

## Best Practices

### ✅ DO: Be Consistent

```aria
// Good: All standard
fn identity<T>(value: T) -> T { }
fn pair<T, U>(a: T, b: U) -> (T, U) { }

// Also good: All star
fn identity<*T>(value: T) -> T { }
fn pair<*T, *U>(a: T, b: U) -> (T, U) { }
```

### ✅ DO: Follow Project Conventions

```aria
// If project uses standard, use standard
// If project uses star, use star
```

### ❌ DON'T: Mix Styles

```aria
// Wrong: Inconsistent
fn foo<T>(x: T) { }      // Standard
fn bar<*U>(y: U) { }     // Star

// Right: Pick one
fn foo<T>(x: T) { }
fn bar<U>(y: U) { }
```

### ❌ DON'T: Overthink It

```aria
// They're the same! Just pick one
<T>   // Standard, common
<*T>  // Star, rare

// Both work identically
```

---

## Real-World Usage

### Standard (Common)

```aria
// Most codebases look like this
struct Vec<T> {
    data: []T
}

impl<T> Vec<T> {
    fn push(item: T) { }
}

fn map<T, U>(array: []T, f: fn(T) -> U) -> []U { }
```

### Star Prefix (Rare)

```aria
// Some codebases use star
struct Vec<*T> {
    data: []T
}

impl<*T> Vec<T> {
    fn push(item: T) { }
}

fn map<*T, *U>(array: []T, f: fn(T) -> U) -> []U { }
```

---

## Documentation

When writing documentation, **prefer standard syntax** for wider compatibility:

```aria
// Documentation example - use standard
/// Returns the first element
/// 
/// # Type Parameters
/// - `T`: The element type
/// 
fn first<T>(array: []T) -> T { }
```

---

## Summary

- `<T>` and `<*T>` are **functionally identical**
- Standard `<T>` is **more common**
- Star `<*T>` **emphasizes** type variables
- **Pick one** and stay consistent
- No performance difference
- **When in doubt, use standard `<T>`**

---

## Related Topics

- [Generic Parameters](generic_parameters.md) - Parameter details
- [Generic Syntax](generic_syntax.md) - Complete syntax reference
- [Generics](generics.md) - Generic overview

---

**Remember**: `<*T>` and `<T>` are **exactly the same** - just a stylistic choice. Most code uses `<T>`!
