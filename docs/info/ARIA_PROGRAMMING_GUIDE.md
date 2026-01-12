# Aria Programming Language v0.1.0 - Complete Programming Guide
**Date**: December 31, 2025  
**Status**: Working Compiler - ARIA-025 Complete (int1024, uint1024, fix256 for post-quantum crypto & safety-critical systems)

## Table of Contents
1. [Quick Start](#quick-start)
2. [Critical Rules (Read First!)](#critical-rules)
3. [Type System](#type-system)
4. [Function Syntax](#function-syntax)
5. [Generics (Template Functions)](#generics-template-functions)
6. [Macro System](#macro-system)
7. [Control Flow](#control-flow)
8. [Memory Model](#memory-model)
9. [Operators](#operators)
10. [Standard Library](#standard-library)
11. [Common Pitfalls](#common-pitfalls)
12. [Working Examples](#working-examples)

---

## Quick Start

### Minimal Hello World
```aria
func:main = int8() {
    puts("Hello, Aria!");
    pass(0);
};
```

### Compile and Run
```bash
./ariac program.aria -o program.ll
lli program.ll
```

---

## Critical Rules (Read First!)

### 🚨 RESERVED KEYWORDS - DO NOT USE AS VARIABLE NAMES
```aria
// ❌ WRONG - 'result' is a reserved keyword (it's a TYPE!)
func:helper = *int32(int32:x) {
    int32:result = x + 1;  // PARSE ERROR!
    return result;
};

// ✅ CORRECT - Use any other name
func:helper = *int32(int32:x) {
    int32:value = x + 1;   // Works fine
    return value;
};
```

**Reserved Keywords**: `result`, `func`, `wild`, `defer`, `async`, `const`, `use`, `mod`, `pub`, `extern`, `ERR`, `stack`, `gc`, `wildx`, `struct`, `enum`, `type`, `is`, `in`, `fall`, `pass`, `fail`

### ⚡ Return Value Helpers
All Aria functions return a `result` type containing `{err, val}`. Use helper functions for cleaner code:

```aria
func:divide = int32(int32:a, int32:b) {
    if (b == 0) {
        fail(1);  // Return {err: 1, val: 0}
    }
    pass(a / b);  // Return {err: 0, val: result}
};

// Alternative: Explicit result construction
func:divide = int32(int32:a, int32:b) {
    if (b == 0) {
        return {err: 1, val: 0};
    }
    return {err: 0, val: a / b};
};
```

**Best Practice**: Use `pass(value)` for success and `fail(code)` for errors instead of constructing result structs manually.

### 📝 Function Declaration Syntax
```aria
// Function stored in variable (can be called later)
func:myFunction = int8(int8:x, int8:y) {
    pass(x + y);
};

// Immediate execution (result stored in variable)
int8:sum = int8(int8:x, int8:y) { pass(x + y); }(3, 4) ? 0;
//         ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^  ^^^^^^
//         Anonymous lambda                      Call it immediately

// Call stored function
int8:result = myFunction(3, 4);
```

---

## Type System

### Integer Types
```aria
// Standard signed integers
int1, int2, int4                // Bit-sized integers
int8, int16, int32, int64       // Common signed integers
int128, int256, int512          // Extended precision signed

// Standard unsigned integers
uint8, uint16, uint32, uint64   // Common unsigned integers
uint128, uint256, uint512       // Extended precision unsigned
```

### ARIA-025: Post-Quantum & Safety-Critical Types

#### int1024 - 1024-bit Signed Integer (LBIM)
```aria
// For post-quantum cryptography (RSA-4096, lattice crypto)
int1024:privateKey;              // 16x64-bit limbs
int1024:modulus;                 // Large modular arithmetic

// ERR sentinel: 0x8000...0000 in high limb
int1024:result = aria_int1024_mul(a, b);
if (aria_int1024_is_err(result)) {
    fail(1);  // Overflow detected
}
```

**Properties**:
- 16 limbs × 64 bits = 1024 bits total
- ERR sentinel: `limbs[15] = 0x8000000000000000`, others = 0
- Sticky error propagation (ERR + anything = ERR)
- Division by zero → ERR
- Overflow/underflow → ERR

#### uint1024 - 1024-bit Unsigned Integer (LBIM)
```aria
// For post-quantum cryptography (unsigned operations)
uint1024:publicExponent;         // RSA public key operations
uint1024:hash;                   // Large hash values

// Shares runtime with int1024, sign bit handled separately
uint1024:sum = aria_uint1024_add(x, y);
```

**Properties**:
- Same representation as int1024 (16 limbs)
- ERR sentinel: `limbs[15] = 0x8000000000000000`
- Range: 0 to 2^1024 - 1 (excluding ERR)

#### fix256 - Q128.128 Deterministic Fixed-Point
```aria
// For safety-critical systems, AGI substrate, robotics, physics
fix256:position = aria_fix256_from_int(42);      // Integer part
fix256:velocity = aria_fix256_from_double(9.81); // From floating-point

// Deterministic arithmetic (bit-identical across platforms)
fix256:newPos = aria_fix256_add(position, velocity);
fix256:scaled = aria_fix256_mul(newPos, aria_fix256_from_int(2));

// NO floating-point drift - critical for consciousness substrate
fix256:result = aria_fix256_div(a, b);
if (aria_fix256_is_err(result)) {
    fail(1);  // Division by zero or overflow
}
```

**Properties**:
- Q128.128 format: 128-bit integer + 128-bit fractional
- 4 limbs × 64 bits = 256 bits total
- ERR sentinel: `limbs[3] = 0x8000000000000000`, others = 0
- Precision: 2^-128 ≈ 2.9×10^-39 (4 orders finer than Planck length)
- **Deterministic**: Bit-identical results across all platforms
- **No drift**: Prevents "must not harm" → "must harm" in AGI substrate
- Sticky error propagation + overflow/underflow detection

**Why fix256 exists**: Prevents value drift in self-improving AI systems where floating-point accumulation errors could invert safety constraints over billions of iterations.

### Floating Point
```aria
flt32, flt64                    // Standard floats (IEEE 754)
flt128, flt256, flt512          // Extended precision
```

### Twisted Balanced Binary (TBB) - Special Error-Handling Integers
```aria
tbb8:value = 100;               // Range: [-127, +127]
tbb8:error = ERR;               // -128 is error sentinel

tbb16:largeVal = 30000;         // Range: [-32767, +32767]
tbb32:bigVal = 2000000000;      // Range: [-2147483647, +2147483647]
tbb64:hugeVal;                  // Symmetric range, min = ERR

// Sticky error propagation
tbb8:a = 100;
tbb8:b = 50;
tbb8:result = a + b;            // 150 > 127, so result = ERR
tbb8:chained = result + 10;     // ERR + anything = ERR
```

**TBB Benefits**:
- Symmetric range (no abs(-128) bug)
- Automatic overflow detection
- Error propagation without exceptions
- Min value always reserved as ERR sentinel

### Exact Rational Arithmetic (frac types)
```aria
// Mixed-fraction rational numbers (whole + num/denom)
frac8:half = {whole: 0, num: 1, denom: 2};       // 0 + 1/2 = 0.5
frac16:pi_approx = {whole: 3, num: 1, denom: 7}; // 3 + 1/7 ≈ π
frac32:precise;                                   // Exact rational arithmetic
frac64:scientific;                                // Large-scale exact rationals

// NO rounding errors - exact fraction representation
// Sticky ERR propagation through TBB components
```

**frac Properties**:
- `frac8`: struct {tbb8:whole; tbb8:num; tbb8:denom;}
- `frac16`: struct {tbb16:whole; tbb16:num; tbb16:denom;}
- `frac32`: struct {tbb32:whole; tbb32:num; tbb32:denom;}
- `frac64`: struct {tbb64:whole; tbb64:num; tbb64:denom;}
- Exact arithmetic (no approximation)
- ERR propagation through TBB components

### Twisted Floating Point
```aria
tfp32:deterministicFloat;        // tbb16 exponent + tbb16 mantissa
// Deterministic float: no -0, unified ERR, predictable behavior
```

### Exotic Types
```aria
trit:t = -1;                    // Balanced ternary: -1, 0, 1
tryte:ty;                       // 10 trits in uint16
nit:n = 4;                      // Balanced nonary: -4 to 4
nyte:ny;                        // 5 nits in uint16
```

### Compound Types
```aria
string:name = "Alice";
func:callback = int8(int8:x) { pass(x * 2); };
dyn:flexible = 42;              // Can change type
obj:config = {port: 8080};      // Object literal
int8[10]:array;                 // Fixed-size array
int8[]:dynamic_array = [1,2,3]; // Dynamic array with initializer

// Pointer Types (@ suffix)
uint8@:ptr = 0;                 // Pointer to uint8
int32@:num_ptr = 0;             // Pointer to int32
flt64@:float_ptr = 0;           // Pointer to flt64
```

---

## Function Syntax

### Basic Function
```aria
func:add = int32(int32:a, int32:b) {
    pass(a + b);
};
```

### Closures
```aria
int8:multiplier = 3;

func:multiply = int8(int8:x) {
    pass(x * multiplier);  // Captures 'multiplier' from outer scope
};

int8:result = multiply(5) ? 0;  // 15
```

### Recursive Functions
```aria
func:factorial = int64(int64:n) {
    if (n <= 1) {
        pass(1);
    }
    int64:subresult = factorial(n - 1) ? 0;
    pass(n * subresult);  // Can call itself
};
```

### Lambda as Argument
```aria
func:applyTwice = int8(func:operation, int8:value) {
    int8:temp = operation(value) ? 0;
    int8:result = operation(temp) ? 0;
    pass(result);
};

int8:result = applyTwice(
    int8(int8:x) { pass(x + 1); },  // Anonymous lambda
    5
) ? 0;  // Result: 7
```

---

## Generics (Template Functions)

Aria supports generic programming through template functions with C++/Rust-style monomorphization.

### Basic Generic Function
```aria
// Generic identity function
func<T>:identity = *T(*T:value) {
    pass(value);
};

// Explicit type argument
int8:x = identity<int8>(42) ? 0;

// Type inference from argument
int32:y = identity(100) ? 0;  // Infers T = int32
```

### Generic Type Marker: The `*` Prefix
In generic functions, `*` marks each use of a generic type parameter:
- `*T` in return type position
- `*T` in parameter types
- `*T` for local variables of generic type

```aria
func<T>:swap = void(*T:a, *T:b) {
    *T:temp = a;  // Local variable of generic type
    a = b;
    b = temp;
    pass();
};
```

### Multiple Type Parameters
```aria
func<T,U>:pair = *T(*T:first, *U:second) {
    // Returns the first value, ignores second
    pass(first);
};

int8:result = pair<int8,string>(42, "hello") ? 0;
```

### Generic Array Operations
```aria
func<T>:max = *T(*T[]:arr, int64:length) {
    *T:maxVal = arr[0];
    int64:i = 1;
    till i >= length {
        if (arr[i] > maxVal) {
            maxVal = arr[i];
        }
        i = i + 1;
    };
    pass(maxVal);
};

int32[]:numbers = [10, 20, 5, 15];
int32:largest = max<int32>(numbers, 4) ? 0;  // Returns 20

// With type inference
int32:largest = max(numbers, 4) ? 0;  // Compiler infers T = int32
```

### How Generics Work
1. **Declaration**: Generic templates are parsed and stored but NOT type-checked or compiled
2. **Call Site**: When you call `identity<int8>(42)`, the compiler:
   - Detects the generic function call
   - Infers or extracts concrete type (int8)
   - Generates a specialized version: `func:identity_int8 = int8(int8:value) { pass(value); };`
   - Type-checks and compiles the specialized version
   - Caches it for reuse
3. **Monomorphization**: Each unique type instantiation generates separate machine code

### Advantages
- Zero runtime overhead (fully compiled away)
- Type safety at compile time
- No code until actually used (unlike macros which generate upfront)
- Clear, explicit syntax with `*` markers

### Current Limitations (v0.0.7)
- Type inference not yet implemented (must use explicit type args)
- Monomorphization not yet implemented (templates parse but don't instantiate)
- Constraints/bounds not supported
- Generic structs not yet implemented

### Difference from Macros
```aria
// MACRO - generates code during preprocessing, before parsing
%macro GEN_MAX 1
func:max_%1 = %1(%1:a, %1:b) {
    if (a > b) { pass(a); }
    pass(b);
};
%endmacro
GEN_MAX(int8)   // Generates max_int8 immediately
GEN_MAX(int32)  // Generates max_int32 immediately

// GENERIC - generates code at call site during compilation
func<T>:max = *T(*T:a, *T:b) {
    if (a > b) { pass(a); }
    pass(b);
};
// No code generated yet...
int8:x = max<int8>(10, 20) ? 0;    // NOW generates max_int8
int32:y = max<int32>(100, 200) ? 0;  // NOW generates max_int32
```

Macros are preprocessor text substitution; generics are type-aware templates with compile-time instantiation.

---

## Macro System

### Basic Macro Definition
```aria
%macro MACRO_NAME PARAM_COUNT
    // Macro body
    // Use %1, %2, etc. for parameters
%endmacro
```

### Single Parameter Macro
```aria
%macro GEN_ABS 1
func:abs_%1 = *%1(%1:value) {
    if (value < 0) {
        return 0 - value;
    }
    return value;
};
%endmacro

// Invoke macro
GEN_ABS(int32)  // Generates: func:abs_int32 = *int32(int32:value) { ... }
```

### Multiple Parameter Macro
```aria
%macro GEN_CLAMP 1
func:clamp_%1 = *%1(%1:value, %1:min_val, %1:max_val) {
    if (value < min_val) {
        return min_val;
    }
    if (value > max_val) {
        return max_val;
    }
    return value;
};
%endmacro

GEN_CLAMP(int8)
GEN_CLAMP(int32)
GEN_CLAMP(flt32)
```

### Macro with Numeric Parameter
```aria
%macro GEN_BIT_COUNT 2
func:count_bits_%1 = *int32(%1:x) {
    int32:count = 0;
    int32:i = 0;
    while (i < %2) {  // %2 = number of bits
        if ((x & 1) != 0) {
            count = count + 1;
        }
        x = x >> 1;
        i = i + 1;
    }
    return count;
};
%endmacro

GEN_BIT_COUNT(int8, 8)
GEN_BIT_COUNT(int16, 16)
GEN_BIT_COUNT(int32, 32)
```

### Macro Best Practices
```aria
// ✅ GOOD: Use fixed types for internal variables (like collections.aria)
%macro GEN_CONTAINS 1
func:contains_%1 = *int8(%1[]:arr, int64:length, %1:value) {
    int64:i = 0;  // Fixed type, not %1:i
    while (i < length) {
        if (arr[i] == value) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
};
%endmacro

// ✅ GOOD: Generate for multiple types
GEN_CONTAINS(int8)
GEN_CONTAINS(int16)
GEN_CONTAINS(int32)
GEN_CONTAINS(uint8)
GEN_CONTAINS(flt32)

// ⚠️ AVOID: Using %1 for internal variable types can cause issues
%macro PROBLEMATIC_MACRO 1
func:process_%1 = *int32(%1:x) {
    %1:temp = x;  // May cause issues with multiple invocations
    return temp;
};
%endmacro

// ❌ AVOID: Using reserved keywords in generated code
%macro BAD_MACRO 1
func:process_%1 = %1(%1:input) {
    %1:result = input * 2;  // ERROR: 'result' is reserved!
    pass(result);
};
%endmacro

// ✅ FIXED:
%macro GOOD_MACRO 1
func:process_%1 = %1(%1:input) {
    %1:output = input * 2;  // OK: 'output' is not reserved
    pass(output);
};
%endmacro
```

### Critical Macro Limitation (December 7, 2025 Discovery)

**Problem**: Using `%1:varname` for internal variables in macros that are invoked multiple times can cause parse errors.

**Symptom**: `Parse Error: Unexpected token after type in parentheses` when compiling macros with:
- Multiple invocations of the same macro
- Internal variables declared as `%1:varname`

**Solution**: Use **fixed types** (like `int32`, `int64`) for internal variables instead of `%1`:

```aria
// ❌ FAILS with multiple invocations
%macro GEN_POPCOUNT 1
func:popcount_%1 = int32(%1:x) {
    %1:val = x;  // Problematic!
    pass(0);
};
%endmacro

GEN_POPCOUNT(int8)   // First invocation works
GEN_POPCOUNT(uint8)  // Second invocation causes parse error!

// ✅ WORKS - use fixed type for internal variables
%macro GEN_POPCOUNT 1
func:popcount_%1 = int32(%1:x) {
    int32:val = x;  // Fixed type - works with multiple invocations
    pass(0);
};
%endmacro

GEN_POPCOUNT(int8)   // Works
GEN_POPCOUNT(uint8)  // Also works!
```

**Explanation**: The macro parameter `%1` is intended for the **parameter style pattern** (`%1:x` creates typed parameters). When used for internal variables, the preprocessor may create naming conflicts across multiple invocations.

**Best Practice**: Follow the collections.aria pattern - use `int64` for loop counters, `int32` for intermediate calculations, and only use `%1` for function parameters and return values.

---

## Control Flow

### If/Else
```aria
if (x > 0) {
    print("Positive");
} else if (x < 0) {
    print("Negative");
} else {
    print("Zero");
}
```

### While Loop
```aria
int8:i = 0;
while (i < 10) {
    print(`&{i}`);
    i = i + 1;
}
```

### For Loop (C-style)
```aria
for (int8:i = 0; i < 10; i = i + 1) {
    print(`&{i}`);
}
```

### Till Loop (Aria-specific, uses $ iteration variable)
```aria
// Count up
till(10, 1) {
    print(`&{$}`);  // $ = current iteration (0..9)
}

// Count down
till(10, -1) {
    print(`&{$}`);  // $ = 9..0
}
```

### When/Then/End Loop
```aria
int8:count = 0;
int8:max = 5;

when (count < max) {
    print(`Count: &{count}`);
    count = count + 1;
} then {
    print("Loop completed successfully");
} end {
    print("Loop ended");
}
```

### Pick Statement (Pattern Matching)
```aria
pick(value) {
    1: {
        print("One");
    }
    2: {
        print("Two");
    }
    10..20: {  // Range matching
        print("Between 10 and 20");
    }
    *: {  // Wildcard (default case)
        print("Other");
    }
}

// With explicit fallthrough
pick(x) {
    case_a: 1: {
        print("One");
        fall(case_b);  // Explicitly fall through to case_b
    }
    case_b: 2: {
        print("Two");
    }
}
```

### Defer (RAII-style cleanup)
```aria
func:processFile = *int8(string:filename) {
    wild int64*:buffer = aria.alloc(1024);
    defer aria.free(buffer);  // Always called before function returns
    
    // Use buffer...
    
    return 0;  // aria.free(buffer) called automatically here
};
```

---

## Memory Model

### Default (GC-Managed)
```aria
string:name = "Alice";  // Automatically garbage collected
obj:config = {port: 8080};
```

### Wild (Manual Management)
```aria
wild int64*:ptr = aria.alloc(1024);
defer aria.free(ptr);  // Manual cleanup

// Wild allocator functions
wild buffer:buf = aria.alloc_buffer(4096);
wild string:str = aria.alloc_string(256);
wild array:arr = aria.alloc_array(100);
```

### Stack Allocation
```aria
stack int64[1000]:buffer;  // On stack, not heap
stack string[256]:temp;
```

### Pinning (Prevent GC Movement)
```aria
string:data = "important";
string$:safe_ref = #data;  // Pin and create safe reference
// 'data' now cannot be moved by GC
```

---

## Operators

### Arithmetic
```aria
+  -  *  /  %  ++  --
+=  -=  *=  /=  %=
```

### Comparison
```aria
==  !=  <  >  <=  >=
<=>  // Spaceship: returns -1, 0, or 1
```

### Logical
```aria
&&  ||  !
```

### Bitwise
```aria
&  |  ^  ~  <<  >>
```

### Special
```aria
@      // Address-of (pointer type suffix: uint8@:ptr)
#      // Pin memory (prevent GC movement)
$      // Iteration variable (in till loops)
?      // Unwrap operator
?.     // Safe navigation
??     // Null coalesce
|>     // Pipeline forward
<|     // Pipeline backward
..     // Inclusive range
...    // Exclusive range
```

### Pointer Operations
```aria
// Pointer type declaration (@ suffix on type)
func:process = int32(uint8@:data_ptr, int64:len) {
    pass(0);
};

// Pointer variable
uint8@:my_ptr = 0;
int32@:num_ptr = 0;

// Pass pointer to function
int32:status = process(my_ptr, 100) ? 0;

// Note: Address-of (&) and dereference (*) operators not yet implemented
// The * prefix in generics context (func<T>) marks generic type usage
```

### String Interpolation
```aria
int8:x = 42;
string:msg = `The value is &{x}`;  // "The value is 42"
string:expr = `Sum: &{x + 8}`;     // "Sum: 50"
```

### Unwrap Operator
```aria
// Without unwrap
result:r = someFunction();
int8:value = is r.err == NULL : r.val : -1;

// With unwrap (cleaner)
int8:value = someFunction() ? -1;  // Returns val if no error, else -1
```

---

## Standard Library

### Console I/O
```aria
print("Hello");
puts("Hello\n");  // Adds newline
```

### File I/O
```aria
result:content = readFile("config.txt");
string:text = content ? "default";  // Unwrap with default

result:status = writeFile("output.txt", "data");
if (status.err != NULL) {
    print("Write failed");
}
```

### Process Management
```aria
result:child = spawn("./worker", ["--input", "data.txt"]);
if (child.err == NULL) {
    int:exit_code = wait(child.val) ? -1;
}
```

---

## Common Pitfalls

### 1. Using Reserved Keywords as Variables
```aria
// ❌ ERROR
int32:result = 42;   // 'result' is a type keyword!

// ✅ CORRECT
int32:output = 42;
int32:value = 42;
int32:res = 42;      // OK (abbreviation)
```

### 2. Forgetting Auto-Wrap Prefix
```aria
// ❌ ERROR: No * prefix, must use pass()
func:add = int8(int8:a, int8:b) {
    return a + b;  // ERROR: Must use pass(a + b)
};

// ✅ CORRECT: Add * prefix
func:add = *int8(int8:a, int8:b) {
    return a + b;  // OK with *
};
```

### 3. Macro Parameter Confusion
```aria
// ❌ WRONG: %1 followed by literal ':'
%macro BAD 1
func:test_%1 = *int32(%1:x) {
    int32:result = x;  // Also uses reserved keyword!
    return result;
};
%endmacro

// ✅ CORRECT
%macro GOOD 1
func:test_%1 = *int32(%1:x) {
    int32:value = x;  // Non-reserved variable name
    return value;
};
%endmacro
```

### 4. Missing Semicolons
```aria
// ❌ ERROR: Missing semicolon after function
func:test = *int8() {
    return 0;
}  // Missing semicolon!

// ✅ CORRECT
func:test = *int8() {
    return 0;
};  // Semicolon required
```

---

## Working Examples

### Example 1: Collections Library (130 Functions)
```aria
%macro GEN_MIN 1
func:min_%1 = %1(%1[]:arr, int64:length) {
    %1:minVal = arr[0];
    int64:i = 1;
    while (i < length) {
        if (arr[i] < minVal) {
            minVal = arr[i];
        }
        i = i + 1;
    }
    pass(minVal);
};
%endmacro

GEN_MIN(int8)
GEN_MIN(int32)
GEN_MIN(flt32)
```

### Example 2: Math Library (24 Functions)
```aria
%macro GEN_ABS 1
func:abs_%1 = %1(%1:value) {
    if (value < 0) {
        pass(0 - value);
    }
    pass(value);
};
%endmacro

%macro GEN_CLAMP 1
func:clamp_%1 = %1(%1:value, %1:lo, %1:hi) {
    if (value < lo) {
        pass(lo);
    }
    if (value > hi) {
        pass(hi);
    }
    pass(value);
};
%endmacro

GEN_ABS(int8)
GEN_ABS(int32)
GEN_ABS(int64)
GEN_CLAMP(int8)
GEN_CLAMP(int32)
```

### Example 3: Recursive Fibonacci
```aria
func:fibonacci = int64(int64:n) {
    if (n <= 1) {
        pass(n);
    }
    int64:fib1 = fibonacci(n - 1) ? 0;
    int64:fib2 = fibonacci(n - 2) ? 0;
    pass(fib1 + fib2);
};

func:main = int8() {
    int64:fib10 = fibonacci(10) ? 0;
    print(`Fibonacci(10) = &{fib10}`);
    pass(0);
};
```

### Example 4: Closures and Higher-Order Functions
```aria
func:makeMultiplier = func(int8:factor) {
    func:multiply = int8(int8:x) {
        pass(x * factor);  // Captures 'factor'
    };
    pass(multiply);
};

func:main = int8() {
    func:times3 = makeMultiplier(3) ? fail(1);
    func:times5 = makeMultiplier(5) ? fail(1);
    
    int8:a = times3(10) ? 0;  // 30
    int8:b = times5(10) ? 0;  // 50
    
    pass(0);
};
```

---

## ARIA-025 Usage Examples

### Example 1: Post-Quantum RSA-4096 Key Generation
```aria
func:generateRSAKey = int8() {
    // 1024-bit prime generation for RSA-4096
    int1024:p = aria_int1024_generate_prime();
    int1024:q = aria_int1024_generate_prime();
    
    // Check for errors (very rare with good RNG)
    if (aria_int1024_is_err(p) || aria_int1024_is_err(q)) {
        fail(1);  // Prime generation failed
    }
    
    // Compute modulus n = p × q (2048 bits, fits in int1024)
    int1024:n = aria_int1024_mul(p, q);
    if (aria_int1024_is_err(n)) {
        fail(2);  // Overflow (shouldn't happen with proper primes)
    }
    
    // Format and display
    string:n_str = aria_format_int1024(n);
    print(`RSA-4096 modulus: &{n_str}`);
    
    pass(0);
};
```

### Example 2: Deterministic Physics Simulation (Robotics)
```aria
func:updateRobotPosition = fix256(fix256:pos, fix256:vel, fix256:dt) {
    // Physics: new_pos = pos + vel × dt
    // 100% deterministic - same inputs ALWAYS give identical output
    
    fix256:displacement = aria_fix256_mul(vel, dt);
    if (aria_fix256_is_err(displacement)) {
        fail(ERR);  // Overflow in velocity calculation
    }
    
    fix256:new_pos = aria_fix256_add(pos, displacement);
    if (aria_fix256_is_err(new_pos)) {
        fail(ERR);  // Position overflow
    }
    
    pass(new_pos);
};

func:main = int8() {
    // Initial conditions
    fix256:pos = aria_fix256_from_int(0);           // Start at origin
    fix256:vel = aria_fix256_from_double(1.5);      // 1.5 m/s
    fix256:dt = aria_fix256_from_double(0.016);     // 60 FPS (16ms)
    
    // Simulate 1000 timesteps
    int32:i = 0;
    while (i < 1000) {
        pos = updateRobotPosition(pos, vel, dt) ? fail(ERR);
        i = i + 1;
    }
    
    // After 1000 steps, position is EXACTLY the same on all platforms
    // No drift, no rounding errors, bit-identical results
    string:final_pos = aria_format_fix256(pos);
    print(`Final position: &{final_pos}`);
    
    pass(0);
};
```

### Example 3: Safety-Critical ERR Propagation
```aria
func:computeSafetyMargin = fix256(fix256:distance, fix256:speed) {
    // Safety margin = distance / speed (time to collision)
    
    fix256:margin = aria_fix256_div(distance, speed);
    
    // Division by zero (stopped) → ERR automatically
    if (aria_fix256_is_err(margin)) {
        // ERR propagates through entire calculation chain
        // System knows something went wrong
        fail(ERR);
    }
    
    pass(margin);
};

func:evaluateSafety = int8() {
    fix256:obstacle_dist = aria_fix256_from_int(10);  // 10 meters
    fix256:robot_speed = aria_fix256_from_int(0);     // Stopped!
    
    fix256:margin = computeSafetyMargin(obstacle_dist, robot_speed) ? fail(ERR);
    
    // This line never executes because margin = ERR
    // ERR is "sticky" - once error occurs, it propagates
    fix256:adjusted = aria_fix256_mul(margin, aria_fix256_from_int(2)) ? fail(ERR);
    
    pass(0);
};
```

### Example 4: AGI Consciousness Substrate (Determinism)
```aria
// In self-improving AI, floating-point drift over billions of iterations
// could cause "must not harm humans" to drift into random values.
// fix256 prevents this nightmare scenario through determinism.

func:updateNeuralWeight = fix256(fix256:weight, fix256:gradient, fix256:learning_rate) {
    // Gradient descent: w_new = w_old - learning_rate × gradient
    
    fix256:delta = aria_fix256_mul(learning_rate, gradient);
    if (aria_fix256_is_err(delta)) {
        fail(ERR);  // Gradient overflow
    }
    
    fix256:new_weight = aria_fix256_sub(weight, delta);
    if (aria_fix256_is_err(new_weight)) {
        fail(ERR);  // Weight overflow/underflow
    }
    
    // This result is BIT-IDENTICAL across all platforms
    // After 1 billion iterations, no drift whatsoever
    // Safety constraints remain geometrically enforced
    pass(new_weight);
};
```

### Example 5: TBB Automatic Overflow Detection
```aria
func:safeAdd = tbb8(tbb8:a, tbb8:b) {
    tbb8:result = a + b;  // Automatic overflow check
    
    // If a=100, b=50: result=ERR (150 > 127)
    if (result == ERR) {
        print("Overflow detected!");
        fail(ERR);
    }
    
    pass(result);
};

func:chainedOperations = tbb16() {
    tbb16:x = 30000;
    tbb16:y = 5000;
    
    // This overflows (35000 > 32767)
    tbb16:sum = x + y;  // sum = ERR
    
    // ERR is sticky - propagates through entire chain
    tbb16:doubled = sum * 2;      // ERR * 2 = ERR
    tbb16:incremented = doubled + 1;  // ERR + 1 = ERR
    
    // All subsequent operations preserve ERR
    pass(incremented);  // Returns ERR
};
```

---

## Compiler Usage

### Basic Compilation
```bash
./ariac program.aria -o program.ll
```

### Preprocessor Only
```bash
./ariac program.aria -E > preprocessed.aria
```

### With Include Paths
```bash
./ariac program.aria -I ../lib -I ./includes -o program.ll
```

### Execute LLVM IR
```bash
lli program.ll
```

---

## stdlib Status (December 7, 2025)

### ✅ Working Libraries
1. **collections.aria** - 130 functions (252KB LLVM IR)
   - Contains, indexOf, count, reverse, fill, copy
   - min, max, sum, allEqual, anyEqual
   - swap, countIfGt
   - Generated for 10 types each

2. **math.aria** - 24 functions (38KB LLVM IR)
   - abs, min, max, clamp
   - Generated for int8, int32, int64, uint8, uint32, uint64

### ⚠️ In Progress
3. **bit_operations.aria** - Syntax fixed, compilation issues remain
   - Would provide 136 bit manipulation functions
   - Needs simplification or debugging

---

## Debugging Tips

### Check Preprocessor Output
```bash
./ariac file.aria -E > expanded.aria
# Review expanded.aria to see what macros generated
```

### Common Error Messages
- `"Unexpected token after type in parentheses"` → Check for reserved keyword usage
- `"Expected token type 9 but got 98"` → Type/colon mismatch, often from 'result' keyword
- `"Undefined function"` → Function not declared before use

### Verify Syntax
1. Check all variable names against reserved keywords
2. Ensure all functions use `pass(value)` or `fail(code)` for returns
3. Verify semicolons after function definitions
4. Check macro parameter substitution (%1, %2, etc.)
5. In generic functions, use `*T` to mark each generic type usage

---

**This guide compiled from**: 
- aria_specs.txt (v0.1.0 - ARIA-025 complete)
- ARIA-025 implementation: int1024, uint1024, fix256 (December 2025)
- Comprehensive testing: 65 tests across 4 test suites (all passing)
- Practical discoveries during stdlib development (December 7, 2025)
- Preprocessor implementation in src/frontend/preprocessor.cpp
- Parser implementation in src/frontend/parser.cpp
- Runtime implementations in src/runtime/math/
- Working examples: collections.aria, math.aria, ARIA-025 test suites
