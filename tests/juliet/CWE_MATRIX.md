# Aria v0.18.5 — Juliet CWE Coverage Matrix

Aria implements structural safety mechanisms that prevent or detect vulnerability
classes from the NIST SAMATE Juliet CWE test suite at compile time or runtime.
This document maps each covered CWE to the Aria mechanism and the test result.

---

## Coverage Summary

| CWE | Name | Aria Mechanism | Prevention Layer | Result |
|-----|------|---------------|-----------------|--------|
| CWE-190 | Integer Overflow | TBB ERR sentinel | Runtime (graceful exit) | ✅ |
| CWE-190b | ERR Propagation (stickiness) | TBB ERR sticky semantics | Runtime (graceful exit) | ✅ |
| CWE-191 | Integer Underflow | TBB ERR sentinel | Runtime (graceful exit) | ✅ |
| CWE-369 | Divide by Zero | TBB ERR sentinel | Runtime (graceful exit) | ✅ |
| CWE-252 | Unchecked Return Value | Result<T> type system | Compile time | ✅ |
| CWE-415 | Double Free | ARIA-022 ownership tracking | Compile time | ✅ |
| CWE-416 | Use After Free | ARIA-022 ownership tracking | Compile time | ✅ |
| CWE-194 | Unexpected Sign Extension | Type system (no implicit cast) | Compile time | ✅ |
| CWE-196 | Unsigned to Signed Conversion | Type system (no implicit cast) | Compile time | ✅ |

**Total: 9 CWEs covered, 18/18 test cases pass.**

---

## Detailed Descriptions

### CWE-190: Integer Overflow or Wraparound

**C behavior**: `INT_MAX + 1` wraps silently to `INT_MIN`. No indication of error.

**Aria mechanism**: All integer arithmetic uses TBB (Trusted Balanced Bitfield) types
(`tbb8`, `tbb16`, `tbb32`, `tbb64`). Each TBB type reserves one sentinel value as `ERR`
(the type minimum, e.g., `tbb32` ERR = −2,147,483,648). Overflow transitions the value
to `ERR` instead of wrapping. The `pick` construct detects ERR explicitly.

```
tbb32:x = 2147483647;
tbb32:one = 1;
tbb32:y = x + one;   // → ERR (not silent wraparound)
pick(y) {
    (ERR) { exit 1; },
    (-2147483647..2147483647) { exit 0; }
}
```

**Test**: `cwe_190_integer_overflow/bad.npk` → exits 1 (overflow detected)  
**Test**: `cwe_190_integer_overflow/good.npk` → exits 0 (safe arithmetic)

---

### CWE-190b: ERR Propagation (Error Stickiness)

**C behavior**: `INT_MAX + 1` wraps to `INT_MIN`. Then `INT_MIN * 0 = 0` — the
original error is silently masked by a subsequent operation.

**Aria mechanism**: TBB ERR is sticky — `ERR * 0 = ERR` (not 0). Arithmetic errors
cannot be masked by subsequent operations. Once a computation produces ERR, it
propagates unconditionally through all further arithmetic until explicitly handled.

```
tbb32:y = overflow_result;   // ERR
tbb32:z = y * zero;          // ERR (not 0 — sticky)
```

**Test**: `cwe_190_err_propagation/bad.npk` → exits 1 (ERR not masked)  
**Test**: `cwe_190_err_propagation/good.npk` → exits 0 (clean path: 0 correctly = 0)

---

### CWE-191: Integer Underflow

**C behavior**: `INT_MIN - 1` wraps silently to `INT_MAX` (undefined behavior).

**Aria mechanism**: Same TBB ERR sentinel mechanism as CWE-190. Underflow (going
below the minimum representable TBB value) transitions to ERR.

**Test**: `cwe_191_integer_underflow/bad.npk` → exits 1 (underflow detected)  
**Test**: `cwe_191_integer_underflow/good.npk` → exits 0 (safe subtraction)

---

### CWE-369: Divide by Zero

**C behavior**: Integer division by zero causes `SIGFPE` — a crash (or undefined
behavior on some platforms), not a recoverable error state.

**Aria mechanism**: TBB division by zero transitions to ERR — no signal, no crash.
The result is detectable and handleable via `pick(ERR)`.

**Test**: `cwe_369_div_zero/bad.npk` → exits 1 (division by zero detected gracefully)  
**Test**: `cwe_369_div_zero/good.npk` → exits 0 (guarded division)

---

### CWE-252: Unchecked Return Value

**C behavior**: Ignoring a function's return value is silently legal. Callers routinely
discard error codes, leading to undetected failure.

**Aria mechanism**: All non-extern Aria functions return `Result<T>`. Assigning a
`Result<T>` directly to a base-type variable is a **compile error**:

```
// ERROR: Cannot silently unwrap Result<int32> into 'v' of type 'int32'.
int32:v = get_value();
```

Callers must either use `raw expr` (explicit acknowledgment of no-error assertion)
or check `.is_error` / use a `Result<T>` variable.

**Test**: `cwe_252_unchecked_return/bad_compile.npk` → compile rejected  
**Test**: `cwe_252_unchecked_return/good.npk` → exits 0 (`raw` explicit handling)

---

### CWE-415: Double Free

**C behavior**: Calling `free()` on the same pointer twice is undefined behavior —
typically heap corruption or arbitrary code execution.

**Aria mechanism**: The compiler tracks ownership of `wild` (raw/unmanaged) pointers.
A second `free()` on an already-freed variable is a **compile error** (ARIA-022):

```
free(buf);
free(buf);  // ERROR: [ARIA-022] Double free of variable 'buf'
```

**Test**: `cwe_415_double_free/bad_compile.npk` → compile rejected (ARIA-022)  
**Test**: `cwe_415_double_free/good.npk` → exits 0 (single free, clean lifecycle)

---

### CWE-416: Use After Free

**C behavior**: Using a pointer after `free()` is undefined behavior — dangling
pointer access, potential arbitrary code execution.

**Aria mechanism**: The compiler tracks ownership state. Any use of a freed `wild`
pointer is a **compile error** (ARIA-022):

```
free(buf);
wild ?->:alias = buf;  // ERROR: [ARIA-022] Use after free: 'buf' was already freed
```

**Test**: `cwe_416_use_after_free/bad_compile.npk` → compile rejected (ARIA-022)  
**Test**: `cwe_416_use_after_free/good.npk` → exits 0 (use before free only)

---

### CWE-194: Unexpected Sign Extension

**C behavior**: Assigning a signed integer to an unsigned variable (or vice versa)
is implicit and legal in C, silently producing incorrect values (e.g., `-1` becomes
`0xFFFFFFFF`).

**Aria mechanism**: Aria has no implicit signed/unsigned conversion. Assigning across
signedness boundaries is a **compile error**:

```
int32:neg = -1;
uint32:u = neg;  // ERROR: Cannot initialize 'uint32' with value of type 'int32'
```

**Test**: `cwe_194_sign_extension/bad_compile.npk` → compile rejected  
**Test**: `cwe_194_sign_extension/good.npk` → exits 0 (consistent types)

---

### CWE-196: Unsigned to Signed Conversion Error

**C behavior**: Assigning a `uint64` to `int32` silently truncates and may flip sign,
producing a completely wrong value. No warning required by the C standard.

**Aria mechanism**: Aria rejects all implicit narrowing and cross-sign type assignments:

```
uint64:large = 9999999999;
int32:small = large;  // ERROR: Cannot initialize 'int32' with value of type 'uint64'
```

**Test**: `cwe_196_unsigned_to_signed/bad_compile.npk` → compile rejected  
**Test**: `cwe_196_unsigned_to_signed/good.npk` → exits 0 (correct types throughout)

---

## Priority Classification

| Priority | CWEs | Rationale |
|----------|------|-----------|
| P1 — Critical | 190, 191, 369, 252, 415, 416 | Memory safety + arithmetic; most exploitable |
| P2 — Medium | 190b (ERR propagation), 194, 196 | Type safety; common source of subtle bugs |
| P3 — N/A (by design) | 121, 122, 124, 125, 126, 787 | Stack/buffer overflows: Aria has no unsafe arrays |

### P3 Explanation (Not Applicable)

CWEs 121 (Stack-Based Buffer Overflow), 122 (Heap-Based Buffer Overflow), 124 (Buffer
Underwrite), 125 (Out-of-Bounds Read), 126 (Buffer Over-read), and 787 (Out-of-Bounds
Write) are not applicable to Aria by architectural design:

- Aria has no raw C-style array type
- All sequence types are bounds-checked at compile time or via runtime Result<T>
- There is no pointer arithmetic on typed pointers
- `wild` pointers (FFI) are untyped and cannot be bounds-traversed without casting

These CWEs cannot be expressed in Aria's type system, making them structurally
impossible in pure-Aria code. FFI boundaries (`wild ?->`) are documented explicitly
as the only escape hatch.

---

## Test Infrastructure

```
tests/juliet/
  run_juliet.sh                    # test runner (compile + runtime + compile-reject)
  CWE_MATRIX.md                    # this file
  cwe_190_integer_overflow/        bad.npk, good.npk
  cwe_190_err_propagation/         bad.npk, good.npk
  cwe_191_integer_underflow/       bad.npk, good.npk
  cwe_369_div_zero/                bad.npk, good.npk
  cwe_252_unchecked_return/        bad_compile.npk, good.npk
  cwe_415_double_free/             bad_compile.npk, good.npk
  cwe_416_use_after_free/          bad_compile.npk, good.npk
  cwe_194_sign_extension/          bad_compile.npk, good.npk
  cwe_196_unsigned_to_signed/      bad_compile.npk, good.npk
```

### Test Categories

| Filename | Logic | Pass Condition |
|----------|-------|----------------|
| `good.npk` | Safe Aria pattern | Compiles + exits 0 |
| `bad.npk` | Vulnerable pattern (runtime prevention) | Compiles + exits non-zero |
| `bad_compile.npk` | Vulnerable pattern (compile-time prevention) | Compile fails (exit ≠ 0) |

### Running

```bash
cd tests/juliet
bash run_juliet.sh             # summary output
bash run_juliet.sh --verbose   # show compiler errors for compile-reject tests
NPKC=/path/to/npkc bash run_juliet.sh  # explicit compiler path
```

### CTest

```bash
cd build && ctest -R juliet_cwe_tests -V
```
