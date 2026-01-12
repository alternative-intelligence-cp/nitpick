# Aria Module System - Working Pattern

**Date**: January 6, 2026  
**Status**: ✅ VERIFIED WORKING

## Summary

The module system IS fully implemented and works! The key is understanding the correct syntax patterns.

## Verified Working Example

### Module File: `lib/std/io_minimal.aria`

```aria
extern "libc" {
    func:putchar = int32(int64:c);
}

pub func:print_char = void(int64:ch) {
    putchar(ch);
};

pub func:eprint_char = void(int64:ch) {
    putchar(ch);
};
```

### Import and Use:

```aria
use std.io_minimal.*;

func:main = int32() {
    print_char(72);  // 'H'
    print_char(10);  // newline
    pass(0);
};
```

**Result**: ✅ Compiles and runs successfully!

## Key Patterns

### 1. Function Declaration Syntax

```aria
pub func:name = return_type(params) {
    // body
};  // ← SEMICOLON REQUIRED!
```

**Critical**: Functions MUST end with `};` not just `}`

### 2. Module Imports

**Logical Paths** (recommended):
```aria
use std.module_name.*;        // Import all public items
use std.module.{item1, item2}; // Selective import
```

**File Paths**:
```aria
use "filename.aria";          // Relative file import
use "/absolute/path.aria";    // Absolute path
```

### 3. Visibility

- Functions without `pub` are private to the module
- `pub func:name` makes the function publicly accessible
- Only `pub` items are imported with `use`

### 4. Module Resolution

The compiler resolves module paths:
- `std.io_minimal` → `lib/std/io_minimal.aria`
- `std.math` → `lib/std/math.aria`
- Root for stdlib is `lib/std/`

## Stdlib File Requirements

For a stdlib module to be usable:

1. **Functions end with `;`**:
   ```aria
   pub func:add = int32(int32:a, int32:b) {
       pass(a + b);
   };  // ← Required!
   ```

2. **Use `pub` for exports**:
   ```aria
   pub func:exported = int32() { pass(0); };
   func:private = int32() { pass(0); };  // Not exported
   ```

3. **Correct control flow keywords**:
   - `while (condition)` not `loop (condition)`
   - `loop(start, limit, step)` for counted loops
   - `till(limit, step)` for simple iteration

4. **Pointer syntax**:
   - Aria code: `int32@` for pointers
   - extern blocks: `int32*` for C FFI

## Common Errors Found

### 1. Missing Semicolon After Function

```aria
// ❌ WRONG
func:test = int32() {
    pass(0);
}

// ✅ CORRECT
func:test = int32() {
    pass(0);
};
```

### 2. Wrong Loop Syntax

```aria
// ❌ WRONG
loop (condition) { }

// ✅ CORRECT  
while (condition) { }
loop(start, limit, step) { }
till(limit, step) { }
```

### 3. Wrong Pointer Syntax

```aria
// ❌ WRONG (outside extern)
func:test = void(*int32:arr) { }

// ✅ CORRECT
func:test = void(int32@:arr) { }
```

### 4. Wrong Import Keyword

```aria
// ❌ WRONG (import doesn't exist)
import "file.aria";

// ✅ CORRECT
use "file.aria";
use std.module.*;
```

## Implementation Status

### Working Modules
- ✅ `std.io_minimal` - Fully functional, tested

### Modules Needing Fixes
From documentation session (Jan 5):
- `algorithms.aria` - Has syntax fixes applied, needs type fixes
- `string.aria` - Has syntax fixes applied
- `path.aria` - Has syntax fixes applied
- `time.aria` - Has syntax fixes applied
- `math.aria` - Has syntax fixes applied

From earlier rebuild (Jan 4-5):
- `int.aria` - Exists, needs pub + testing
- `float.aria` - Exists, needs pub + testing  
- `numeric.aria` - Has pub, needs type fixes (int64 literals)
- `compare.aria` - Exists, needs pub + type fixes

## Syntax Fixes Applied

1. ✅ `import → use` in 5 files
2. ✅ `* → @` for pointers in 5 files
3. ✅ Added `};` after function bodies in 5 files
4. ✅ `loop(...) → while(...)` in 5 files

## Next Steps

1. Add `pub` to all stdlib functions
2. Fix type errors (int64 literals in int32 contexts)
3. Test each module independently
4. Create test suite showing usage patterns

## File Backups

All modified files have backups:
- `.bak_syntax` - Before syntax fixes
- `.bak_nopub` - Before adding pub declarations
