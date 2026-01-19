# `const` Keyword

**Category**: Advanced Features → Compile-Time  
**Syntax**: `const <name>: <type> = <value>;`  
**Purpose**: Define compile-time constants

---

## Overview

`const` declares values computed and fixed at **compile time**.

---

## Basic Constant

```aria
const MAX_SIZE: i32 = 1000;
const PI: flt64 = 3.14159265359;
const APP_NAME: string = "MyApp";
```

---

## Compile-Time Evaluation

```aria
const BUFFER_SIZE: i32 = 1024;
const DOUBLE_BUFFER: i32 = BUFFER_SIZE * 2;  // 2048
const KILOBYTE: i32 = 1024;
const MEGABYTE: i32 = KILOBYTE * 1024;  // 1048576
```

All evaluated at compile time!

---

## Type Inference

```aria
const VALUE = 42;        // Inferred as i32
const RATE = 0.05;       // Inferred as flt64
const NAME = "Aria";     // Inferred as string
```

---

## Const vs Let

```aria
const IMMUTABLE: i32 = 42;  // Compile-time constant
let runtime: i32 = 42;       // Runtime variable
```

```aria
const SIZE: i32 = 100;
let array: [i32; SIZE];  // ✅ OK - SIZE is compile-time constant

let size: i32 = 100;
let array: [i32; size];  // ❌ Error - size is runtime value
```

---

## Const Expressions

```aria
const WIDTH: i32 = 1920;
const HEIGHT: i32 = 1080;
const AREA: i32 = WIDTH * HEIGHT;  // 2073600

const HALF_WIDTH: i32 = WIDTH / 2;
const ASPECT_RATIO: flt64 = WIDTH as flt64 / HEIGHT as flt64;
```

---

## Const Functions

```aria
const fn square(x: i32) -> i32 {
    return x * x;
}

const SQUARED: i32 = square(10);  // 100 - evaluated at compile time
```

---

## Arrays and Const

```aria
const SIZE: i32 = 10;
const VALUES: [i32; SIZE] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Array size must be const
let buffer: [u8; SIZE];  // ✅ OK
```

---

## Const Structs

```aria
struct Config {
    max_connections: i32,
    timeout: i32,
}

const DEFAULT_CONFIG: Config = Config {
    max_connections: 100,
    timeout: 30,
};
```

---

## Public Constants

```aria
pub const API_VERSION: string = "1.0";
pub const MAX_RETRIES: i32 = 3;
pub const DEFAULT_PORT: i32 = 8080;
```

---

## Module-Level Constants

```aria
// In config.aria
pub const DATABASE_URL: string = "localhost:5432";
pub const CACHE_SIZE: i32 = 1000;
pub const TIMEOUT_MS: i32 = 5000;

// In main.aria
import config;

db: Database = connect(config.DATABASE_URL);
```

---

## Const Generics

```aria
struct Buffer<const N: usize> {
    data: [u8; N],
}

const SMALL_BUFFER: Buffer<256> = Buffer { data: [0; 256] };
const LARGE_BUFFER: Buffer<4096> = Buffer { data: [0; 4096] };
```

---

## Common Patterns

### Configuration

```aria
pub const MAX_USERS: i32 = 1000;
pub const MAX_SESSIONS: i32 = 5000;
pub const SESSION_TIMEOUT: i32 = 3600;
pub const CACHE_SIZE: i32 = 10000;
```

---

### Mathematical Constants

```aria
pub const PI: flt64 = 3.14159265358979323846;
pub const E: flt64 = 2.71828182845904523536;
pub const GOLDEN_RATIO: flt64 = 1.61803398874989484820;
```

---

### Buffer Sizes

```aria
pub const SMALL_BUFFER: i32 = 256;
pub const MEDIUM_BUFFER: i32 = 1024;
pub const LARGE_BUFFER: i32 = 4096;
pub const HUGE_BUFFER: i32 = 65536;
```

---

## Best Practices

### ✅ DO: Use UPPER_CASE

```aria
const MAX_SIZE: i32 = 1000;  // ✅ Standard convention
const PI: flt64 = 3.14159;   // ✅ Clear constant
```

### ✅ DO: Group Related Constants

```aria
// Network constants
pub const DEFAULT_PORT: i32 = 8080;
pub const MAX_CONNECTIONS: i32 = 1000;
pub const TIMEOUT_MS: i32 = 5000;

// Buffer sizes
pub const READ_BUFFER: i32 = 4096;
pub const WRITE_BUFFER: i32 = 4096;
```

### ✅ DO: Document Magic Numbers

```aria
pub const HASH_SEED: u32 = 0x9e3779b9;  // Golden ratio based seed
pub const MAX_RETRIES: i32 = 3;          // Network retry limit
```

### ❌ DON'T: Confuse const with let

```aria
const value: i32 = get_value();  // ❌ Can't call runtime function

const VALUE: i32 = 42;           // ✅ Compile-time value
```

---

## Const vs Comptime

```aria
const VALUE: i32 = 42;           // Const - simple constant
comptime result = compute();     // Comptime - complex computation
```

Both are compile-time, but `comptime` allows more complex operations.

---

## Related

- [compile_time](compile_time.md) - Compile-time computation
- [comptime](comptime.md) - Comptime expressions
- [metaprogramming](metaprogramming.md) - Metaprogramming

---

**Remember**: `const` values are **immutable** and **evaluated at compile time**!
