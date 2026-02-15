# Compile-Time Computation

**Category**: Advanced Features → Compile-Time  
**Purpose**: Execute code during compilation

---

## Overview

Compile-time computation allows code to **run during compilation**, producing optimized runtime code.

---

## Why Compile-Time?

- ✅ Zero runtime cost
- ✅ Catch errors early
- ✅ Generate optimized code
- ✅ Type-safe metaprogramming
- ✅ Eliminate runtime checks

---

## Const Evaluation

### Simple Constants

```aria
const SIZE: i32 = 1024;
const DOUBLE: i32 = SIZE * 2;  // Computed at compile time
```

---

### Const Functions

```aria
const fn factorial(n: i32) -> i32 {
    if n <= 1 {
        return 1;
    }
    return n * factorial(n - 1);
}

const FACT_5: i32 = factorial(5);  // 120 - computed at compile time
```

---

### Const Expressions

```aria
const fn pow2(exp: i32) -> i32 {
    return 1 << exp;
}

const KB: i32 = pow2(10);   // 1024
const MB: i32 = pow2(20);   // 1048576
const GB: i32 = pow2(30);   // 1073741824
```

---

## Comptime Blocks

```aria
comptime {
    // This code runs at compile time
    stdout << "Compiling...";
    
    const value: i32 = expensive_computation();
}
```

---

## Type-Level Computation

```aria
const fn array_size<T>() -> usize {
    return sizeof(T) * 100;
}

const BUFFER_SIZE: usize = array_size<i32>();  // 400
let buffer: [u8; BUFFER_SIZE];
```

---

## Compile-Time Assertions

```aria
comptime {
    if sizeof(i32) != 4 {
        @compile_error("i32 must be 4 bytes");
    }
}

const fn assert_positive(value: i32) {
    if value <= 0 {
        @compile_error("Value must be positive");
    }
}

const SIZE: i32 = 100;
comptime assert_positive(SIZE);  // OK

const INVALID: i32 = -1;
comptime assert_positive(INVALID);  // Compile error!
```

---

## Code Generation

```aria
comptime {
    // Generate array at compile time
    const PRIMES: [i32; 10] = generate_primes(10);
}

const fn generate_primes(count: i32) -> [i32; count] {
    // Compute primes at compile time
    Result: [i32; count];
    // ... implementation ...
    return result;
}
```

---

## Conditional Compilation

```aria
comptime {
    if @is_debug() {
        const LOG_LEVEL: string = "DEBUG";
    } else {
        const LOG_LEVEL: string = "INFO";
    }
}
```

---

## Compile-Time Reflection

```aria
comptime {
    // Inspect types at compile time
    const field_count: i32 = @type_info(MyStruct).fields.len;
    const field_names: []string = @type_info(MyStruct).field_names;
}
```

---

## Loop Unrolling

```aria
const fn sum_array<const N: usize>(arr: [i32; N]) -> i32 {
    sum: i32 = 0;
    
    // Unrolled at compile time
    comptime till(N - 1, 1) {
        sum += arr[$];
    }
    
    return sum;
}

const VALUES: [i32; 5] = [1, 2, 3, 4, 5];
const TOTAL: i32 = sum_array(VALUES);  // 15
```

---

## Common Patterns

### Lookup Tables

```aria
const fn build_sin_table() -> [flt64; 360] {
    table: [flt64; 360];
    till(359, 1) {
        table[$] = sin($ * PI / 180.0);
    }
    return table;
}

const SIN_TABLE: [flt64; 360] = build_sin_table();

// Runtime - just table lookup!
fn fast_sin(degrees: i32) -> flt64 {
    return SIN_TABLE[degrees];
}
```

---

### Configuration Validation

```aria
comptime {
    const MAX_SIZE: i32 = 1000;
    const BUFFER_SIZE: i32 = 10000;
    
    if BUFFER_SIZE < MAX_SIZE {
        @compile_error("BUFFER_SIZE must be >= MAX_SIZE");
    }
}
```

---

### Platform-Specific Constants

```aria
comptime {
    #[cfg(target_pointer_width = "64")]
    const PTR_SIZE: i32 = 8;
    
    #[cfg(target_pointer_width = "32")]
    const PTR_SIZE: i32 = 4;
}
```

---

## Performance Benefits

### Before (Runtime)

```aria
fn calculate_offset(index: i32) -> i32 {
    return index * sizeof(Data);  // Runtime multiplication
}
```

### After (Compile-Time)

```aria
const ELEMENT_SIZE: i32 = sizeof(Data);

fn calculate_offset(index: i32) -> i32 {
    return index * ELEMENT_SIZE;  // Optimized - const known
}
```

Even better:

```aria
const fn calculate_offset(index: i32) -> i32 {
    return index * sizeof(Data);
}

// If index is const, entire calculation is compile-time!
const OFFSET: i32 = calculate_offset(10);
```

---

## Best Practices

### ✅ DO: Use for Fixed Computations

```aria
const LOOKUP_TABLE: [i32; 256] = generate_table();  // ✅ Computed once
```

### ✅ DO: Validate at Compile Time

```aria
comptime {
    if CONFIG.timeout < 0 {
        @compile_error("Invalid timeout");
    }
}
```

### ✅ DO: Generate Code

```aria
comptime {
    // Generate optimal code for specific sizes
    const SIZE: i32 = 16;
    generate_unrolled_loop(SIZE);
}
```

### ❌ DON'T: Overuse for Simple Cases

```aria
const VALUE: i32 = 42;  // ✅ Simple const

comptime {
    const VALUE: i32 = expensive_function();  // ❌ Overkill if not needed
}
```

---

## Limitations

```aria
const fn limited() {
    // ✅ Can do arithmetic
    x: i32 = 10 + 20;
    
    // ✅ Can call other const functions
    y: i32 = other_const_fn();
    
    // ❌ Can't do I/O
    // file = readFile("data.txt");
    
    // ❌ Can't allocate dynamically
    // ptr = malloc(100);
    
    // ❌ Can't call runtime functions
    // value = runtime_function();
}
```

---

## Related

- [const](const.md) - Const keyword
- [comptime](comptime.md) - Comptime expressions
- [metaprogramming](metaprogramming.md) - Metaprogramming

---

**Remember**: Move work from **runtime** to **compile time** for zero-cost abstractions!
