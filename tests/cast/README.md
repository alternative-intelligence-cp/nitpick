# Cast Expression Tests

This directory contains comprehensive tests for Aria's explicit casting system (@cast and @cast_unchecked).

## Test Coverage

### 01_safe_widening.aria
Tests safe widening casts that always succeed:
- Signed integer widening (i8→i16→i32→i64)
- Unsigned integer widening (u8→u16→u32→u64)
- Float widening (f32→f64)

### 02_checked_narrowing.aria
Tests narrowing casts with runtime overflow checking:
- Signed integer narrowing (i64→i32→i16→i8)
- Unsigned integer narrowing (u64→u32→u16→u8)
- Values that fit within target type range

### 03_unchecked_narrowing.aria
Tests unchecked narrowing casts (no runtime checks):
- Truncation of large values
- Multi-step unchecked conversions
- Potential data loss scenarios

### 04_int_to_float.aria
Tests integer to floating-point conversions:
- Signed int to float (i8→f32, i32→f64, i64→f64)
- Unsigned int to float (u8→f32, u32→f64)

### 05_float_to_int.aria
Tests floating-point to integer conversions:
- Float to signed int (f32→i32, f64→i64)
- Float to unsigned int (f32→u8, f64→u32)
- Unchecked float to int conversions

### 06_mixed_scenarios.aria
Tests complex casting scenarios:
- Chained casts (i8→i64→f64→i32→i16→i64)
- Casts in expressions
- Casts in return statements

### 07_same_type_cast.aria
Tests redundant same-type casts (should be no-ops):
- i32→i32
- f64→f64
- u64→u64

## Running Tests

```bash
# Compile and run all cast tests
for test in tests/cast/*.aria; do
    echo "Testing: $test"
    ./build/ariac "$test" -o "/tmp/cast_test" && /tmp/cast_test && echo "✅ PASS"
done
```

## Design Philosophy

Aria's casting system follows the "Zero Implicit Conversion" principle:
- **All conversions must be explicit** using @cast or @cast_unchecked
- **Safe casts** (widening) always succeed at compile time
- **Checked casts** (narrowing) include runtime overflow detection
- **Unchecked casts** provide performance at the cost of safety

## Implementation Status

- ✅ AST nodes (CastExpr)
- ✅ Lexer (@cast and @cast_unchecked keywords)
- ✅ Parser (@cast<Type>(expr) syntax)
- ✅ Type checker (validates cast compatibility)
- ✅ IR generation (sext, zext, trunc, fpext, fptrunc, sitofp, uitofp, fptosi, fptoui)
- ✅ Test suite (comprehensive scenarios)

## Future Enhancements

- [ ] Runtime overflow checking for @cast (currently just truncates)
- [ ] Panic on overflow for narrowing casts
- [ ] Support for pointer casts
- [ ] Support for struct/enum casts
