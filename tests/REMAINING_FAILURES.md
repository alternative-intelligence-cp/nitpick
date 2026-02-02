# Remaining Test Failures (7/146 files, 4.8%)

## Parse Success: 95.2% (139/146 tests)

### Category 1: Intentional Test Failures (1 file)

**tests/diagnostics_demo.aria** - Tests error message quality
```aria
int64:y = ;  // Syntax error - missing value (INTENTIONAL)
```
Status: ✅ Working as intended - tests compiler error reporting

### Category 2: Unimplemented Parser Features (6 files)

#### 1. Array Syntax - `tests/const_advanced_test.aria`
```aria
// Current error: Expected variable name
int64[ARRAY_SIZE]:buffer;
```
**Needs:** Array type syntax support `Type[size]:name`

#### 2. Generic Syntax - `tests/generic_struct_basic.aria`
```aria
// Current error: Expected struct name
struct<T, E>:Result = {
    T:ok;
    E:err;
};
```
**Needs:** Generic parameter syntax `<T, E>` for structs

#### 3. Extern Function Syntax - `tests/move_wild_test.aria`
```aria
// Current error: Expected library name string after 'extern'
extern func malloc(size: int64) -> wild int8*;
```
**Needs:** Support for `extern func` declarations outside extern blocks

#### 4. Visibility Keywords - `tests/ffi_safety_test.aria`
```aria
// Current error: Expected function or variable declaration in extern block
extern "C" {
    pub func:good_ffi_string = void(wild int8->:message);
}
```
**Needs:** `pub` keyword support in extern blocks

#### 5. Fraction Literal Syntax - `tests/numeric_types_parser_test.aria`
```aria
// Current error: Expected field name in object literal
frac8:half = {0, 1, 2};  // 0 + 1/2 in base-3
```
**Needs:** Ternary tuple syntax `{whole, nit1, nit2}` for fraction literals

#### 6. Static Member Syntax - `tests/safety_critical_suite.aria`
```aria
// Current error: Expected member name after '.'
if (overflow == int1024.ERR) {
    // ...
}
```
**Needs:** Static member access `Type.CONSTANT` for ERR sentinels

## Summary

All syntax modernization complete! Remaining failures represent 6 unimplemented language features:

1. **Arrays** - Type[size] syntax
2. **Generics** - struct<T, E> syntax  
3. **Extern funcs** - extern func outside blocks
4. **Visibility** - pub keyword
5. **Fraction literals** - {w, n1, n2} syntax
6. **Static members** - Type.MEMBER syntax

These features are specified but not yet implemented in parser. When ready to implement, these 6 test files will validate the implementations.

---
Generated: 2026-02-01 after test suite v0.1.0 syntax modernization  
Commit: 3730f5b
