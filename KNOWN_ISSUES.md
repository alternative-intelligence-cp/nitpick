# Known Issues - Aria v0.0.6

**Last Updated**: December 7, 2025

This document tracks known bugs, limitations, and workarounds in the current version of Aria.

---

## 🐛 Active Bugs

### 1. Macro Variable Scoping Bug (BLOCKING)
**Severity**: High  
**Affects**: Macros, Code Generation  
**Status**: Workaround available

**Description**:
When a macro uses `%1:varname` pattern for internal variables (not parameters), and the macro is invoked multiple times with different types, the compiler produces a parse error: "Unexpected token after type in parentheses".

**Example**:
```aria
%macro GEN_POPCOUNT 1
func:popcount_%1 = *int32(%1:value) {
    %1:val = value;  // ❌ Using %1 for internal variable
    %1:count = 0;    // ❌ This causes issues
    // ... function body
};
%endmacro

GEN_POPCOUNT(int8)   // ✅ First invocation works
GEN_POPCOUNT(uint8)  // ❌ Second invocation fails!
```

**Root Cause**:
The preprocessor or parser doesn't properly isolate variable names generated from macro parameters across multiple invocations. This appears to be a scoping bug where `%1:varname` creates naming conflicts.

**Workaround**:
Use **fixed types** for all internal variables. Only use `%1` for function parameters and return types.

```aria
// ✅ CORRECT - Use fixed types for internal variables
%macro GEN_POPCOUNT 1
func:popcount_%1 = *int32(%1:value) {
    int32:val = value;   // ✅ Fixed type
    int32:count = 0;     // ✅ Fixed type
    // ... function body
};
%endmacro

GEN_POPCOUNT(int8)   // ✅ Works
GEN_POPCOUNT(uint8)  // ✅ Works!
```

**Evidence**:
- Collections.aria (130 functions) uses `int64:i`, `int32:count` pattern → compiles successfully
- Bit operations using `%1:val`, `%1:count` pattern → fails with multiple invocations
- Manually written functions with identical variable names → work fine
- Only affects macro-generated code

**Impact**:
- ❌ Blocks bit_operations.aria (136 functions)
- ✅ Collections.aria works (already uses workaround)
- ✅ Math.aria works (simple functions, no complex internal vars)

**Next Steps**:
- Defer fixing to Phase 2 (after Tsoding review)
- Rewrite bit_operations.aria using workaround (time-consuming)
- Debug preprocessor.cpp macro expansion logic

---

### 2. String Library LLVM Verification Error
**Severity**: High  
**Affects**: Standard Library (string.aria)  
**Status**: No workaround

**Description**:
The string library compiles successfully through all parser stages but fails during LLVM IR verification with error: "Terminator found in the middle of a basic block! label %ifcont28"

**Error Message**:
```
LLVM IR VERIFICATION FAILED:
Terminator found in the middle of a basic block!
  label %ifcont28
Broken module found, compilation aborted!
```

**Root Cause**:
LLVM codegen bug in control flow. The backend is generating invalid IR where a terminator instruction (br, ret, etc.) appears in the middle of a basic block instead of at the end.

**Likely Source**:
- Complex if/while nesting in string functions
- Possible issue with `ifcont` label generation
- Control flow merge points not handled correctly

**Impact**:
- ❌ All string manipulation functions unavailable
- ⚠️ String literals and basic operations still work
- ⚠️ String interpolation still works (separate feature)

**Next Steps**:
- Examine generated LLVM IR for string.aria
- Compare with working collections.aria control flow patterns
- Debug llvm_backend.cpp basic block generation
- May require redesigning control flow codegen

---

### 3. Pointer Types Not Fully Implemented
**Severity**: Medium  
**Affects**: I/O Library, Advanced Features  
**Status**: Implementation incomplete

**Description**:
The `@` operator for pointer types exists in the syntax but is not fully implemented in the parser.

**Syntax**:
```aria
uint8@:buffer    // Pointer to uint8
```

**Error Example**:
```
Parse Error: Expected token type 9 but got 100 at line 73, col 36
```

**Occurrence**:
Line 73 of io.aria:
```aria
func:read = *int64(int32:fd, uint8@:buffer, int64:count) {
                                    ^ Parse fails here
```

**Impact**:
- ❌ Blocks io.aria (file I/O functions)
- ❌ Blocks advanced memory operations
- ❌ Cannot pass buffers to functions
- ⚠️ Arrays work fine (different syntax: `int32[]`)

**Conflict Note**:
`@` was previously used for LLVM intrinsics (like `@llvm.sqrt`), which was removed. The operator may not be fully wired through the parser for the new pointer type usage.

**Next Steps**:
- Review parser.cpp for `@` operator handling
- Ensure pointer type parsing is complete
- Test pointer arithmetic operations
- Add pointer dereference operations

---

### 4. `wildx` Type Not Implemented
**Severity**: Low  
**Affects**: Memory Library (mem.aria)  
**Status**: Feature not yet implemented

**Description**:
The `wildx` keyword for executable memory allocation is reserved but not implemented.

**Syntax**:
```aria
wildx uint8@(uint64:size)  // Allocate executable memory
```

**Error Message**:
```
Parse Error: Unexpected token in expression: wildx at line 16
```

**Purpose**:
`wildx` is intended for allocating executable memory regions (for JIT compilation, dynamic code generation, etc.).

**Impact**:
- ❌ Blocks mem.aria (memory allocation library)
- ❌ Cannot implement JIT features
- ⚠️ Stack allocation works fine
- ⚠️ Basic memory operations work

**Next Steps**:
- Add `wildx` to parser token handling
- Implement executable memory allocation in runtime
- Add platform-specific mprotect/VirtualProtect calls
- Design safety model for executable memory

---

## ⚠️ Limitations (By Design or Deferred)

### 1. No `use` or `#include` Directive
**Status**: Not implemented

**Description**:
There is no way to include other Aria files. All code must be in a single file or manually concatenated.

**Workaround**:
```bash
cat lib/stdlib/collections.aria lib/stdlib/math.aria myprogram.aria > combined.aria
./ariac combined.aria -o myprogram
```

**Impact**: Makes large programs cumbersome, requires manual file management.

**Next Steps**: Add `%include` preprocessor directive in Phase 2.

---

### 2. Reserved Keyword: `result`
**Status**: By design (mostly working)

**Description**:
`result` is a reserved keyword for the result type system. Cannot be used as variable name.

**Fixed In**: v0.0.6 (December 7, 2025)
- ✅ io.aria line 63: `int32:result` → `int32:status`
- ✅ bit_operations.aria: Multiple instances fixed

**Usage**:
```aria
// ❌ ERROR - 'result' is reserved
func:test = *int32() {
    int32:result = 0;  // Parse error
    return result;
};

// ✅ CORRECT - Use any other name
func:test = *int32() {
    int32:status = 0;
    return status;
};
```

**Note**: Future versions will implement result types:
```aria
func:divide = result(int32:a, int32:b) {
    if (b == 0) { fail(1); }
    pass(a / b);
};
```

---

### 3. Arrays/Object Literals Not Implemented
**Status**: Deferred to Phase 2

**Description**:
Array and object literal syntax is not yet implemented.

**Current**: Must initialize arrays element-by-element:
```aria
int32[]:arr = stack 3;
arr[0] = 10;
arr[1] = 20;
arr[2] = 30;
```

**Future**: Array literals:
```aria
int32[]:arr = [10, 20, 30];  // Not yet implemented
```

**Impact**: Verbose initialization code.

---

### 4. No Semantic Analyzer
**Status**: Planned for Phase 2

**Description**:
There is no semantic analysis phase. Type errors, undefined variables, and scope issues may not be caught until LLVM backend.

**Examples of Uncaught Errors**:
- Using undefined variables
- Type mismatches (may cause LLVM errors)
- Dead code
- Unreachable code

**Impact**: Error messages may be cryptic or delayed.

**Next Steps**: Implement full semantic analyzer after pointer types and macro bug fixes.

---

## 📋 Working Workarounds Summary

| Issue | Workaround | Effort | Status |
|-------|-----------|--------|--------|
| Macro variable bug | Use `int32:var` not `%1:var` | Low | ✅ Known |
| No `use` directive | Manually concatenate files | Low | ✅ Works |
| Reserved `result` keyword | Use `status`, `res`, etc. | Low | ✅ Fixed everywhere |
| String library LLVM bug | None | N/A | ❌ No workaround |
| Pointer types | None | N/A | ❌ Needs implementation |
| `wildx` type | None | N/A | ❌ Needs implementation |

---

## 🔍 Investigation Needed

### 1. Preprocessor Macro Expansion
- Why does `%1:varname` cause conflicts?
- Is it preprocessor or parser?
- Does it affect nested macros?

### 2. LLVM Basic Block Generation
- Why are terminators appearing mid-block?
- Is it specific to certain control flow patterns?
- Are other files at risk?

### 3. Pointer Type Parsing
- Where did `@` operator parsing break?
- Is it just parsing or also codegen?
- Does `@` conflict with anything else?

---

## 📈 Bug Metrics

**Total Known Issues**: 4 critical, 4 limitations  
**Blocking Stdlib**: 3 issues (macros, pointers, wildx)  
**Has Workaround**: 3 issues  
**No Workaround**: 1 issue (string LLVM bug)

**Stability**: 
- ✅ Core compiler: Stable (93-95%)
- ✅ Collections library: Production-ready
- ✅ Math library: Production-ready
- ⚠️ String library: Compiles but broken
- ❌ I/O library: Blocked
- ❌ Memory library: Blocked
- ❌ Bit operations: Blocked (has workaround)

---

## 🗓️ Resolution Timeline

**Phase 1 (Tsoding-Ready)**: Document and workaround  
**Phase 2 (Nikola-Ready)**: Fix all blockers

Priority order for Phase 2:
1. Implement pointer types (@ operator) - Unblocks I/O
2. Fix macro variable scoping bug - Unblocks bit operations
3. Fix string library LLVM bug - Completes string support
4. Implement wildx - Unblocks memory library
5. Add semantic analyzer - Improves error quality

---

**Last Updated**: December 7, 2025  
**Version**: v0.0.6  
**Maintainer**: AILP Development Team
