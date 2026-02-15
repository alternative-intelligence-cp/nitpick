# `comptime` Expressions

**Category**: Advanced Features → Compile-Time  
**Syntax**: `comptime <expression>`  
**Purpose**: Force compile-time evaluation

---

## Overview

`comptime` explicitly marks code to **execute during compilation**.

---

## Basic Comptime

```aria
comptime {
    stdout << "This prints during compilation!";
}
```

---

## Comptime Variables

```aria
comptime value: i32 = expensive_computation();

// 'value' is known at compile time
let array: [u8; value];  // ✅ OK - value is comptime
```

---

## Comptime vs Const

```aria
const VALUE: i32 = 42;              // Simple constant
comptime result = compute();        // Complex computation

const SIZE: i32 = 100;              // Value-based
comptime type T = choose_type();    // Type-based
```

---

## Comptime Functions

```aria
fn regular_function(x: i32) -> i32 {
    return x * 2;
}

comptime Result: i32 = regular_function(21);  // Called at compile time
// result = 42
```

---

## Type-Level Programming

```aria
comptime {
    if sizeof(i32) == 4 {
        type IntType = i32;
    } else {
        type IntType = i64;
    }
}

value: IntType = 42;  // Type chosen at compile time
```

---

## Comptime Conditionals

```aria
comptime {
    #[cfg(debug)]
    const OPTIMIZATION_LEVEL: i32 = 0;
    
    #[cfg(release)]
    const OPTIMIZATION_LEVEL: i32 = 3;
}
```

---

## Code Generation

```aria
comptime {
    // Generate different code based on platform
    #[cfg(arch = "x86_64")]
    {
        fn optimized_function() {
            // x86_64 SIMD code
        }
    }
    
    #[cfg(arch = "aarch64")]
    {
        fn optimized_function() {
            // ARM NEON code
        }
    }
}
```

---

## Comptime Assertions

```aria
comptime {
    if MAX_SIZE > BUFFER_SIZE {
        @compile_error("MAX_SIZE cannot exceed BUFFER_SIZE");
    }
}

comptime {
    @assert(sizeof(Pointer) == 8, "Expected 64-bit pointers");
}
```

---

## Comptime Loops

```aria
comptime {
    till(9, 1) {
        stdout << "Generating code for iteration $($)";
        generate_function($);
    }
}
```

---

## Reflection

```aria
comptime {
    type_info: TypeInfo = @type_info(MyStruct);
    
    till(type_info.fields.length - 1, 1) {
        stdout << "Field: ${type_info.fields[$].name}, Type: ${type_info.fields[$].type}";
    }
}
```

---

## Generic Specialization

```aria
fn process<T>(value: T) -> T {
    comptime {
        if @type_is_integer(T) {
            return value * 2;
        } else if @type_is_float(T) {
            return value * 2.0;
        } else {
            @compile_error("Unsupported type");
        }
    }
}
```

---

## Comptime String Building

```aria
comptime {
    name: string = "MyApp";
    version: string = "1.0";
    full_name: string = name ++ " v" ++ version;
}

const APP_NAME: string = full_name;  // "MyApp v1.0"
```

---

## Comptime Array Generation

```aria
comptime fn generate_powers_of_2(n: i32) -> [i32; n] {
    Result: [i32; n];
    till(n - 1, 1) {
        result[$] = 1 << $;
    }
    return result;
}

const POWERS: [i32; 10] = comptime generate_powers_of_2(10);
// [1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
```

---

## Common Patterns

### Configuration Selection

```aria
comptime {
    #[cfg(feature = "fast")]
    type Allocator = FastAllocator;
    
    #[cfg(not(feature = "fast"))]
    type Allocator = SafeAllocator;
}

allocator: Allocator = Allocator.new();
```

---

### Lookup Table Generation

```aria
comptime fn crc_table() -> [u32; 256] {
    table: [u32; 256];
    till(255, 1) {
        i = $;
        value: u32 = i;
        till(7, 1) {
            if (value & 1) != 0 {
                value = (value >> 1) ^ 0xEDB88320;
            } else {
                value = value >> 1;
            }
        }
        table[i] = value;
    }
    return table;
}

const CRC_TABLE: [u32; 256] = comptime crc_table();
```

---

### Version String

```aria
comptime {
    const MAJOR: i32 = 1;
    const MINOR: i32 = 2;
    const PATCH: i32 = 3;
    
    const VERSION: string = "$MAJOR.$MINOR.$PATCH";
}
```

---

## Best Practices

### ✅ DO: Use for Code Generation

```aria
comptime {
    sizes = [8, 16, 32, 64, 128, 256];
    till(sizes.length - 1, 1) {
        generate_buffer_type(sizes[$]);
    }
}
```

### ✅ DO: Validate Configurations

```aria
comptime {
    if THREAD_COUNT < 1 || THREAD_COUNT > 64 {
        @compile_error("THREAD_COUNT must be between 1 and 64");
    }
}
```

### ✅ DO: Optimize Based on Types

```aria
fn process<T>(value: T) -> T {
    comptime {
        if @type_size(T) <= 8 {
            return fast_process(value);  // Small type
        } else {
            return slow_process(value);  // Large type
        }
    }
}
```

### ❌ DON'T: Overuse

```aria
comptime {
    value: i32 = 42;  // ❌ Just use: const VALUE: i32 = 42;
}
```

---

## Comptime Functions

```aria
comptime fn is_power_of_2(n: i32) -> bool {
    return n > 0 && (n & (n - 1)) == 0;
}

comptime {
    if !is_power_of_2(BUFFER_SIZE) {
        @compile_error("BUFFER_SIZE must be power of 2");
    }
}
```

---

## Debugging Comptime

```aria
comptime {
    stdout << "SIZE = $SIZE";
    stdout << "ALIGNMENT = $ALIGNMENT";
    
    @dump_type(MyStruct);
    @dump_value(CONSTANT);
}
```

---

## Related

- [const](const.md) - Const keyword
- [compile_time](compile_time.md) - Compile-time computation
- [metaprogramming](metaprogramming.md) - Metaprogramming

---

**Remember**: `comptime` is **powerful** - use it for **zero-cost abstractions**!
