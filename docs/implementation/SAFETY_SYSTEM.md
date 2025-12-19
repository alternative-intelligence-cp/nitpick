# Aria Safety System Implementation

## Overview

The Aria Safety System enforces the principle that **dangerous operations must be explicit**. You cannot accidentally cause undefined behavior - you must explicitly opt-in to unsafe operations using specific keywords.

## Implementation Status

### ✅ Completed

1. **SAFETY.md** - Comprehensive documentation of the safety contract
   - Safety philosophy and principles
   - Explicit unsafe keywords (wild, async, @, extern, wildx)
   - Compiler safety levels (permissive, strict, paranoid)
   - Safety acknowledgment system
   - Examples and best practices

2. **SafetyChecker Class** - Compiler-integrated safety analysis
   - Location: `src/frontend/sema/safety_checker.{h,cpp}`
   - Detects unsafe operations in AST
   - Issues warnings/errors based on safety level
   - Tracks safety acknowledgments from code comments

3. **Compiler Integration** - Added to CMake build system
   - Integrated into semantic analysis phase
   - Compiled with rest of compiler

### 🚧 To Be Integrated

The safety checker is implemented but not yet integrated into the main compilation pipeline. To complete integration:

1. **Add to main.cpp**:
```cpp
#include "frontend/sema/safety_checker.h"

// After semantic analysis:
aria::SafetyChecker safety_checker(safety_level);
if (!safety_checker.check(ast)) {
    safety_checker.printIssues();
    if (safety_checker.hasErrors()) {
        return 1;  // Exit on errors
    }
}
```

2. **Add command-line flags**:
```cpp
// In argument parsing:
--strict        // SafetyLevel::STRICT
--paranoid      // SafetyLevel::PARANOID  
--permissive    // SafetyLevel::PERMISSIVE (default)
```

3. **Parse SAFETY comments** from source:
```cpp
// Extract comments during lexing:
// SAFETY: explanation
// SAFETY_BLOCK: explanation
// SAFETY_MODULE: explanation
```

## How It Works

### 1. Unsafe Operation Detection

The safety checker walks the AST looking for:

- **wild memory**: Variables with 'wild' storage class
- **async functions**: Functions marked with 'async'
- **Pointer operations**: @ operator, pointer arithmetic
- **FFI calls**: extern function invocations
- **Executable memory**: wildx allocations

### 2. Safety Levels

**PERMISSIVE (default)**:
- Shows warnings for all unsafe operations
- Compilation proceeds regardless
- Development mode

**STRICT**:
- Unsafe operations without acknowledgments become errors
- Requires SAFETY comments
- Pre-production mode

**PARANOID**:
- Maximum checks enabled
- All unsafe operations must be acknowledged
- Suggests safer alternatives
- Production mode

### 3. Safety Acknowledgments

Developers acknowledge unsafe operations with comments:

```aria
// SAFETY: Manually managing memory for zero-copy buffer
wild byte@:buffer = aria.alloc(size);
defer aria.free(buffer);
```

The compiler parses these and marks the operation as acknowledged.

### 4. Warning Output Format

```
warning: [safety] wild memory allocation without automatic management
  --> src/buffer.aria:42:5
   |
   = note: wild memory operation detected
   = help: consider using 'defer aria.free(...)' for automatic cleanup
   = help: wild memory must be manually freed - you are responsible for deallocation

For more information, see SAFETY.md
To enforce safety acknowledgments, use: --strict or --paranoid
```

## Examples

### Example 1: Wild Memory (Detected)

```aria
func:allocate_buffer = wild byte@(int64:size) {
    // ⚠️ WARNING: wild memory operation
    wild byte@:buf = aria.alloc(size);
    pass(buf);
}
```

**Output**:
```
warning: [safety] wild memory allocation without automatic management
  = help: consider using 'defer aria.free(...)' for automatic cleanup
```

### Example 2: Wild Memory (Acknowledged)

```aria
func:allocate_buffer = wild byte@(int64:size) {
    // SAFETY: Caller responsible for freeing returned buffer
    // Documented in function contract - see buffer.md
    wild byte@:buf = aria.alloc(size);
    pass(buf);
}
```

**Output**: No warning (acknowledged)

### Example 3: Strict Mode (Error)

```bash
ariac --strict unsafe_code.aria
```

```
error: [safety] wild memory allocation without automatic management
  = note: add 'SAFETY:' comment to acknowledge this risk
```

## Integration Checklist

- [x] Create SAFETY.md documentation
- [x] Implement SafetyChecker class
- [x] Add to CMake build system
- [x] Compile successfully
- [ ] Integrate into main compiler pipeline
- [ ] Add command-line flags (--strict, --paranoid)
- [ ] Parse SAFETY comments from source
- [ ] Add tests for safety checker
- [ ] Update compiler help text with safety info

## Testing

Once integrated, create test files:

**tests/safety/wild_memory.aria**:
```aria
func:test_wild = int32() {
    wild int64@:ptr = aria.alloc(8);
    pass(0);  // Warning: forgot to free!
}
```

**tests/safety/wild_with_defer.aria**:
```aria
func:test_wild_safe = int32() {
    // SAFETY: Using defer for automatic cleanup
    wild int64@:ptr = aria.alloc(8);
    defer aria.free(ptr);
    pass(0);  // OK - acknowledged and properly managed
}
```

Run with different levels:
```bash
# Should warn but compile
./build/ariac tests/safety/wild_memory.aria

# Should fail without acknowledgment
./build/ariac --strict tests/safety/wild_memory.aria

# Should succeed with acknowledgment
./build/ariac --strict tests/safety/wild_with_defer.aria
```

## Future Enhancements

1. **Static Analysis Integration**:
   - Use-after-free detection
   - Double-free detection
   - Null pointer dereference checks
   - Buffer overflow detection

2. **Borrow Checker Integration**:
   - Track ownership of wild memory
   - Enforce single ownership or explicit shared ownership
   - Lifetime annotations

3. **Runtime Checks** (optional flag):
   - Bounds checking on pointer operations
   - Null checks before dereferences
   - Double-free detection

4. **IDE Integration**:
   - Show safety warnings inline
   - Quick-fix suggestions
   - Safety level indicators

5. **Documentation Generation**:
   - Extract SAFETY comments
   - Generate safety audit reports
   - Track unsafe operation density

## Philosophy

The Aria Safety System embodies our core principle:

> **"You can't shoot yourself in the foot accidentally.  
> You must load the gun, aim it, and pull the trigger intentionally."**

By making unsafe operations explicit and requiring acknowledgment, we:
- Prevent accidental undefined behavior
- Make code review easier (grep for 'wild', '@', 'extern')
- Create audit trails for safety-critical code
- Educate developers about risks upfront

This is not about preventing all bugs - that's impossible. It's about ensuring developers **knowingly** accept responsibility when using dangerous features.

---

**Status**: Core implementation complete, ready for integration into compiler pipeline.
