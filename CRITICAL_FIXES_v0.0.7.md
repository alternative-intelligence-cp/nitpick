# Aria v0.0.7 Critical Fixes Roadmap

**Date:** December 7, 2025  
**Status:** Implementation Planning  
**Source:** Technical Review and Analysis - Aria Compiler v0.0.6

## Executive Summary

A comprehensive technical review identified critical gaps between the Aria v0.0.6 implementation and the language specification. This document prioritizes fixes required to achieve production readiness for v0.0.7.

---

## 🔴 CRITICAL (P0) - System Safety Violations

### 1. TBB Sticky Error Propagation
**Severity:** CRITICAL  
**Component:** Backend (codegen.cpp)  
**Impact:** Memory corruption, undefined behavior

**Problem:**  
Twisted Balanced Binary (tbb8, tbb16, tbb32, tbb64) types currently map to standard LLVM integers without sentinel value handling. The specification mandates:
- Sentinel value = minimum signed value (e.g., -128 for tbb8)
- `ERR + x = ERR` (sticky propagation)
- Overflow → ERR
- Current implementation allows ERR to "heal" via wrapping

**Example Failure:**
```aria
const tbb8:x = -128;  // ERR sentinel
const tbb8:y = x - 1;  // Should be ERR, currently wraps to 127
```

**Solution:**  
Implement `TBBLowerer` class with LLVM overflow intrinsics:
- `llvm.sadd.with.overflow`
- `llvm.ssub.with.overflow`
- `llvm.smul.with.overflow`

**Files to Create:**
- `src/backend/codegen_tbb.cpp`
- `src/backend/codegen_tbb.h`

**Validation:**
- Unit test: `(-128) + (-1) == -128`
- Unit test: `127 + 1 == -128`
- Unit test: `ERR op x == ERR` for all operations

---

### 2. Flow-Sensitive Lifetime Analysis
**Severity:** CRITICAL  
**Component:** Semantics (borrow_checker.cpp)  
**Impact:** Dangling pointer vulnerabilities, use-after-free

**Problem:**  
Current borrow checker uses basic set tracking without scope depth awareness. Appendage Theory requires:
- Safe references ($) cannot outlive pinned hosts (#)
- References declared at depth N cannot point to hosts at depth N+1
- Current implementation allows escape of inner-scope references

**Example Failure:**
```aria
func dangling() -> int8$ {
    const int8#:x = 42;  // Pinned at depth 1
    return $x;            // Reference escapes! Should be rejected
}
```

**Solution:**  
Enhance `BorrowContext` with:
- `var_depths` map (variable → scope depth)
- `reference_origins` map (reference → host)
- Scope entry/exit tracking
- Transitive dependency checking

**Files to Modify:**
- `src/frontend/sema/borrow_checker.cpp`

**Validation:**
- Reject references that outlive hosts
- Allow same-depth or outer-scope references
- Track transitive dependencies (ref → ref → host)

---

### 3. Async/Await Coroutine Implementation
**Severity:** CRITICAL  
**Component:** Backend (codegen.cpp)  
**Impact:** Concurrency features non-functional

**Problem:**  
`async` keyword parsed but ignored in codegen. Functions marked `async` compile as synchronous, breaking the concurrency model.

**Example Failure:**
```aria
async func fetchData() -> string {
    await httpGet("https://api.example.com");  // Blocks entire thread!
}
```

**Solution:**  
Implement LLVM coroutine intrinsics:
- `llvm.coro.id` - Coroutine identification
- `llvm.coro.begin` - Allocate coroutine frame
- `llvm.coro.suspend` - Suspend points
- `llvm.coro.end` - Cleanup

**Files to Modify:**
- `src/backend/codegen.cpp` (add `visit(AsyncBlock*)`)

**Validation:**
- Generate coroutine handle (i8*)
- Verify suspend/resume points
- Integration with runtime scheduler

---

## 🟡 HIGH PRIORITY (P1) - Correctness Issues

### 4. Pattern Matching (Pick Statement)
**Severity:** HIGH  
**Component:** Backend (codegen.cpp)  
**Impact:** Advanced control flow non-functional

**Problem:**  
`pick` statement has skeleton implementation only. Cannot handle:
- Ranges: `1..10`, `0..<5`
- Comparison operators: `< 100`
- Fallthrough: `fall(label)`
- Wildcard: `_`

**Example Failure:**
```aria
pick x {
    1..10: print("single digit");     // Crashes or wrong behavior
    < 100: print("double digit");     // Not implemented
    _: print("large");                // Not implemented
}
```

**Solution:**  
Implement decision chain lowering:
1. Create BasicBlocks for each case body
2. Generate comparison chain (ICmpEQ, ICmpSGE, ICmpSLE)
3. Handle fallthrough via label map
4. Support exclusive ranges (`<` operator)

**Files to Modify:**
- `src/backend/codegen.cpp` (`visit(PickStmt*)`, `visit(FallStmt*)`)

**Validation:**
- Range matching correctness
- Fallthrough behavior
- Wildcard catch-all
- Comparison operators

---

### 5. Directive Sanitization
**Severity:** HIGH  
**Component:** Frontend (lexer.cpp)  
**Impact:** Parser confusion, misleading errors

**Problem:**  
Lexer lacks directive whitelist. Invalid directives like `@tesla` or `@foo` treated as address-of operator, causing confusing error messages.

**Example Failure:**
```aria
@inlne func foo() {}  // Typo: should be @inline
// Error: "Cannot take address of 'func'" (misleading!)
```

**Solution:**  
Implement whitelist in `nextToken()`:
```cpp
static const std::set<std::string> valid_directives = {
    "inline", "noinline", "pack", "align", "section",
    "export", "import", "pure", "const", "volatile"
};
```

**Files to Modify:**
- `src/frontend/lexer.cpp`

**Validation:**
- Valid directives → TOKEN_DIRECTIVE
- Invalid directives → TOKEN_INVALID with clear error
- `@variable` (no identifier) → TOKEN_AT

---

### 6. Struct vs Variable Disambiguation
**Severity:** HIGH  
**Component:** Frontend (parser.cpp)  
**Impact:** Parser crashes on valid code

**Problem:**  
Grammar ambiguity between:
- `const Name = struct { ... }` (struct declaration)
- `const Type:name = value;` (variable declaration)

Parser consumes tokens prematurely, corrupting state.

**Example Failure:**
```aria
const Point = struct { x:int8, y:int8 };  // May fail
const Point:p = makePoint();              // May parse as struct
```

**Solution:**  
Implement lookahead without consuming:
1. Save parser state (position, line, col)
2. Peek ahead to distinguish `=` vs `:`
3. Route to `parseStructDeclFromState` or variable logic
4. Restore state if needed

**Files to Modify:**
- `src/frontend/parser.cpp` (`parseVarDecl`, new `parseStructDeclFromState`)

**Validation:**
- Struct declarations parse correctly
- User-defined types in variable decls work
- Built-in types unaffected

---

## 📋 Implementation Timeline (v0.0.7)

### Week 1: Critical Safety (P0)
- **Day 1-2:** TBB Sticky Error Propagation
  - Create `codegen_tbb.cpp/h`
  - Implement `TBBLowerer` class
  - Unit tests for sentinel behavior
  
- **Day 3-4:** Flow-Sensitive Lifetime Analysis
  - Enhance `BorrowContext` with scope tracking
  - Implement depth-based validation
  - Integration tests for escape analysis

- **Day 5:** Async/Await Foundation
  - Add coroutine intrinsic calls
  - Basic suspend/resume logic
  - Defer full scheduler to v0.0.8

### Week 2: Correctness (P1)
- **Day 6-7:** Pick Statement Lowering
  - Range comparison logic
  - Fallthrough label resolution
  - Comprehensive test suite

- **Day 8:** Frontend Fixes
  - Directive whitelist
  - Struct/variable disambiguation
  - Parser error message improvements

- **Day 9-10:** Integration and Validation
  - Full compiler test suite
  - Standard library compatibility
  - Performance regression tests

---

## 🧪 Validation Strategy

### TBB Safety Tests
```aria
// tests/backend/test_tbb_safety.aria
const tbb8:err = -128;
const tbb8:max = 127;

assert(err + 1 == err);        // Sticky propagation
assert(max + 1 == err);        // Overflow detection
assert(err - 1 == err);        // Subtraction stickiness
assert(err * 2 == err);        // Multiplication stickiness
```

### Lifetime Analysis Tests
```aria
// tests/sema/test_lifetime.aria
func inner_escape() -> int8$ {
    const int8#:local = 42;
    return $local;  // REJECT: escapes inner scope
}

func outer_valid() -> int8$ {
    const int8#:outer = 42;
    func inner() {
        const int8$:ref = $outer;  // ACCEPT: outer lives longer
    }
    return $outer;
}
```

### Pick Statement Tests
```aria
// tests/backend/test_pick.aria
pick x {
    1..10: assert(x >= 1 && x <= 10);
    < 100: assert(x >= 11 && x < 100);
    _: assert(x >= 100);
}
```

---

## 📊 Compliance Matrix (v0.0.7 Target)

| Feature | v0.0.6 Status | v0.0.7 Target | Blocker |
|---------|---------------|---------------|---------|
| TBB Sticky Errors | ❌ Maps to i8 | ✅ Full safety | P0 |
| Lifetime Analysis | ⚠️ Basic sets | ✅ Flow-sensitive | P0 |
| Async/Await | ❌ Ignored | ✅ Coroutines | P0 |
| Pick Ranges | ❌ Skeleton | ✅ Full impl | P1 |
| Directive Whitelist | ❌ None | ✅ Validated | P1 |
| Struct Parsing | ⚠️ Ambiguous | ✅ Robust | P1 |

---

## 🎯 Success Criteria

**v0.0.7 is production-ready when:**

1. ✅ All TBB arithmetic preserves sentinel values
2. ✅ Zero false negatives in borrow checker (no unsafe code compiles)
3. ✅ Async functions generate valid coroutine frames
4. ✅ Pick statements handle all syntax variations
5. ✅ Parser never crashes on valid Aria code
6. ✅ 100% of language spec examples compile and run

**Gate Review:** All P0 items must pass validation before v0.0.7 release.

---

## 📚 References

- Technical Review: `docs/research/Aria Language Bug Report and Fixes.txt`
- Language Spec: `docs/spec/Aria_v0.0.6_Specs.txt`
- Current Status: See individual component headers in `src/`

---

**Next Steps:** Begin implementation with TBB safety (highest risk, clearest scope).
