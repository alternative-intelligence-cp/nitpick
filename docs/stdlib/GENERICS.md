# Phase 4.5: Generics & Templates

## Status: ✅ IMPLEMENTED AND WORKING

Aria's generic/template system is **fully functional** with zero-cost monomorphization!

## Overview

Aria implements generics via compile-time monomorphization (like C++ templates and Rust generics). Generic functions are specialized for each concrete type combination used, resulting in zero runtime overhead.

## Syntax

### Generic Function Declaration

```aria
func<T>:identity = *T(*T:value) {
    return value;
};
```

- `<T>` - Declares type parameter T
- `*T` - References type parameter in return type and parameter types
- At call site, T is inferred from arguments

### Multiple Type Parameters

```aria
func<T,U>:first = *T(*T:a, *U:b) {
    return a;
};
```

## Features

### ✅ Type Inference
Type arguments are automatically inferred from function call arguments:

```aria
int64:x = identity(42);      // T = int64
flt64:y = identity(3.14);    // T = flt64
```

### ✅ Monomorphization
Each unique type combination creates a specialized function:

```llvm
define i64 @_Aria_M_identity_f9d0c88f42834344_int64(i64 %value)
define double @_Aria_M_identity_8a31f1a9c766def3_flt64(double %value)
```

Mangled names include:
- Function name: `identity`
- Hash of type signature
- Type parameters: `int64`, `flt64`

### ✅ Multi-Parameter Generics
Functions with multiple type parameters work perfectly:

```aria
func<T,U>:pair_first = *T(*T:a, *U:b) { return a; };
func<T,U>:pair_second = *U(*T:a, *U:b) { return b; };

int64:x = pair_first(42, 3.14);   // T=int64, U=flt64
flt64:y = pair_second(42, 3.14);  // T=int64, U=flt64
```

## Implementation Details

### Code Generation
- **Parser**: Recognizes `func<T,U>` syntax and `*T` type references
- **Type Checker**: Performs bidirectional type inference
- **Generic Resolver**: Creates specialized AST for each type combination
- **IR Generator**: Generates LLVM IR for each specialization
- **Memoization**: Caches specializations to avoid duplicate codegen

### Name Mangling Scheme
```
_Aria_M_{func_name}_{hash}_{type1}_{type2}...
```

Example:
```
_Aria_M_first_2beeabe3f7294464_int64_flt64
```

### Zero-Cost Abstraction
Generics have **zero runtime overhead**:
- No vtables or dynamic dispatch
- Full inlining and optimization by LLVM
- Identical performance to hand-written specialized code
- Dead code elimination removes unused specializations

## Limitations & Future Work

### Current Limitations
1. **No Trait Constraints**: Cannot constrain T to types with specific operations
   - Future: `func<T: Comparable>:max = ...`

2. **No Explicit Type Arguments**: Cannot manually specify types yet
   - Future: Turbofish syntax `identity::<int64>(value)`

3. **No Generic Structs**: Only functions support generics currently
   - Future: `struct<T>:Box { *T:value; };`

4. **No Const Generics**: Cannot parameterize by values
   - Future: `func<N: int64>:fixed_array = ...`

5. **Limited Error Messages**: Type inference failures need better diagnostics

### Planned Features (Phase 4.5.1+)

#### Trait Constraints
```aria
trait:Addable {
    func:add = Self(Self:a, Self:b);
};

func<T: Addable>:sum = *T(*T:a, *T:b) {
    return a.add(b);
};
```

#### Generic Structs
```aria
struct<T>:Box {
    *T:value;
};

func:main = int64() {
    Box<int64>:b = Box::<int64>{ value: 42 };
    return b.value;
};
```

#### Const Generics
```aria
func<N: int64>:make_array = int64[N]() {
    int64[N]:arr;
    return arr;
};
```

## Testing

### Test Files
- `tests/generics/test_identity.aria` - Basic single-parameter generic
- `tests/generics/test_swap.aria` - Multiple functions, same type parameter
- `tests/generics/test_multi_param.aria` - Multi-parameter generics (T, U)

### Running Tests
```bash
cd tests/generics
../build/ariac test_identity.aria -o test && ./test
../build/ariac test_swap.aria -o test && ./test
../build/ariac test_multi_param.aria -o test && ./test
```

All tests should exit with code 0 (success).

## Example: Generic Identity Function

**Source Code:**
```aria
func<T>:identity = *T(*T:value) {
    return value;
};

func:main = int64() {
    int64:x = identity(42);
    flt64:y = identity(3.14);
    return 0;
};
```

**Generated LLVM IR:**
```llvm
define i64 @_Aria_M_identity_f9d0c88f42834344_int64(i64 %value) {
entry:
  ret i64 %value
}

define double @_Aria_M_identity_8a31f1a9c766def3_flt64(double %value) {
entry:
  ret double %value
}

define i64 @main() {
entry:
  %x = alloca i64, align 8
  %0 = call i64 @_Aria_M_identity_f9d0c88f42834344_int64(i64 42)
  store i64 %0, ptr %x, align 4
  
  %y = alloca double, align 8
  %1 = call double @_Aria_M_identity_8a31f1a9c766def3_flt64(double 3.140000e+00)
  store double %1, ptr %y, align 8
  
  ret i64 0
}
```

**Performance:** After LLVM optimization, the identity calls are completely inlined and eliminated!

## Research & References

- Implementation based on `research_027_generics_templates.txt`
- Generic resolver: `src/frontend/sema/generic_resolver.cpp`
- Generic resolver header: `include/frontend/sema/generic_resolver.h`
- Parser support: `src/frontend/parser/parser.cpp`
- IR generation: `src/backend/ir/ir_generator.cpp`

## Conclusion

Phase 4.5 Generics are **production-ready**! The monomorphization approach provides:
- ✅ Zero runtime overhead
- ✅ Full type safety
- ✅ Automatic type inference
- ✅ Multiple type parameters
- ✅ Clean, readable syntax

Next phases can build on this foundation to add trait constraints, generic structs, and const generics!
