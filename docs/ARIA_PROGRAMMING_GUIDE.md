# Aria Programming Language v0.0.6 - Complete Programming Guide
**Date**: December 7, 2025  
**Status**: Working Compiler - Core Features Complete

## Table of Contents
1. [Quick Start](#quick-start)
2. [Critical Rules (Read First!)](#critical-rules)
3. [Type System](#type-system)
4. [Function Syntax](#function-syntax)
5. [Macro System](#macro-system)
6. [Control Flow](#control-flow)
7. [Memory Model](#memory-model)
8. [Operators](#operators)
9. [Standard Library](#standard-library)
10. [Common Pitfalls](#common-pitfalls)
11. [Working Examples](#working-examples)

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
int8, int16, int32, int64       // Signed integers
uint8, uint16, uint32, uint64   // Unsigned integers
int1, int2, int4                // Bit-sized integers
```

### Floating Point
```aria
flt32, flt64                    // Standard floats
flt128, flt256, flt512          // Extended precision
```

### Twisted Balanced Binary (TBB) - Special Error-Handling Integers
```aria
tbb8:value = 100;               // Range: [-127, +127]
tbb8:error = ERR;               // -128 is error sentinel

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
func:min_%1 = *%1(%1[]:arr, int64:length) {
    %1:minVal = arr[0];
    int64:i = 1;
    while (i < length) {
        if (arr[i] < minVal) {
            minVal = arr[i];
        }
        i = i + 1;
    }
    return minVal;
};
%endmacro

GEN_MIN(int8)
GEN_MIN(int32)
GEN_MIN(flt32)
```

### Example 2: Math Library (24 Functions)
```aria
%macro GEN_ABS 1
func:abs_%1 = *%1(%1:value) {
    if (value < 0) {
        return 0 - value;
    }
    return value;
};
%endmacro

%macro GEN_CLAMP 1
func:clamp_%1 = *%1(%1:value, %1:lo, %1:hi) {
    if (value < lo) {
        return lo;
    }
    if (value > hi) {
        return hi;
    }
    return value;
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
func:fibonacci = *int64(int64:n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
};

func:main = *int8() {
    int64:fib10 = fibonacci(10);
    print(`Fibonacci(10) = &{fib10}`);
    return 0;
};
```

### Example 4: Closures and Higher-Order Functions
```aria
func:makeMultiplier = *func(int8:factor) {
    func:multiply = *int8(int8:x) {
        return x * factor;  // Captures 'factor'
    };
    return multiply;
};

func:main = *int8() {
    func:times3 = makeMultiplier(3);
    func:times5 = makeMultiplier(5);
    
    int8:a = times3(10);  // 30
    int8:b = times5(10);  // 50
    
    return 0;
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
2. Ensure all functions have * prefix if using `return value;`
3. Verify semicolons after function definitions
4. Check macro parameter substitution (%1, %2, etc.)

---

**This guide compiled from**: 
- aria_v0_0_6_specs.txt
- Practical discoveries during stdlib development (December 7, 2025)
- Preprocessor implementation in src/frontend/preprocessor.cpp
- Parser implementation in src/frontend/parser.cpp
- Working examples: collections.aria, math.aria
