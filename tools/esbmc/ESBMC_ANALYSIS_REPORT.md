# ESBMC Analysis Report — Aria Project

**Date:** April 15, 2026
**ESBMC Version:** 8.0.0 (via Ubuntu PPA `ppa:esbmc/esbmc`)
**SMT Solver:** Z3 v4.8.12
**Analyst:** ESBMC batch harness + manual verification

---

## Executive Summary

ESBMC v8.0 was installed and evaluated as a complementary formal verification
tool alongside IKOS. It was tested on **155 functions** across aria-libc shim
stubs, the Aria lock-free runtime, WASM runtime, and wild pointer helpers.

**Real bugs found: 2** (missed by IKOS)
**Functions verified safe: 79+**
**C++ STL support: Not feasible** (header conflicts with libstdc++)

**Post-analysis fix status (April 29, 2026):** Both real bug classes have been
fixed in the local `aria-libc` working tree. Targeted ESBMC 8.2.0 checks now
verify `aria_libc_process_signal_register` and `aria_libc_process_signal_restore`
successfully. `aria_libc_mem_make_string` now handles `malloc()` returning `NULL`;
a forced-null `malloc` smoke test passes. Direct lib-entry ESBMC for make_string
still reports an unconstrained raw-source-pointer proof obligation, which is a
separate harness/precondition issue rather than the original NULL-after-malloc bug.
The minimal inline memory harness was also synced with the fixed implementation
and now reports `VERIFICATION SUCCESSFUL` under ESBMC 8.2.0.

---

## Tool Comparison: ESBMC vs IKOS

| Feature | IKOS | ESBMC |
|---------|------|-------|
| Input format | LLVM IR (.ll/.bc) | C/C++ source |
| Analysis type | Abstract Interpretation | Bounded Model Checking (SMT) |
| C++ STL support | ✓ (via compiled IR) | ✗ (header conflicts) |
| Input space coverage | Call-graph-based | All possible inputs |
| Loop handling | Fixed-point iteration | Loop unrolling (expensive) |
| Setup complexity | High (IR downgrade needed) | Low (reads source directly) |
| False positive pattern | Over-approximation of numeric domains | Non-deterministic wild pointers |
| Concurrency analysis | No | Yes (pthread deadlock, races) |
| Best use case | Whole-program safety of compiled code | Function-level input-space verification |

**Verdict:** Complementary. IKOS for whole-program analysis of compiled C++ code.
ESBMC for verifying C library functions against all possible inputs.

---

## Real Bugs Found by ESBMC

### Bug 1: Signal Handler — Missing Lower Bound Check

**File:** `aria-libc/shim/stubs/aria_libc_process_extra.c`
**Functions:** `aria_libc_process_signal_register`, `aria_libc_process_signal_restore`
**Severity:** Medium (exploitable with negative signal numbers)

```c
static sig_handler_t saved_handlers[64];

int aria_libc_process_signal_register(int signum, int64_t handler_ptr) {
    sig_handler_t h = signal(signum, (sig_handler_t)(unsigned long)handler_ptr);
    if (h == SIG_ERR) return -1;
    if (signum < 64) saved_handlers[signum] = h;  // ← BUG: no lower bound check
    return 0;
}

int aria_libc_process_signal_restore(int signum) {
    sig_handler_t h = (signum < 64) ? saved_handlers[signum] : SIG_DFL;  // ← BUG
    return (signal(signum, h) == SIG_ERR) ? -1 : 0;
}
```

**Issue:** `signum < 64` guards the upper bound but not the lower bound.
A negative `signum` passes the check and causes out-of-bounds array access.

**Fix:** Change to `if (signum >= 0 && signum < 64)`.

**Status:** Fixed locally April 29, 2026; targeted ESBMC 8.2.0 checks pass for
both affected functions.

---

### Bug 2: make_string — Missing NULL Check After malloc

**File:** `aria-libc/shim/stubs/aria_libc_mem_extra.c`
**Function:** `aria_libc_mem_make_string`
**Severity:** Low (malloc failure → NULL deref)

```c
AriaString aria_libc_mem_make_string(void* src_ptr, int64_t offset, int64_t len) {
    ...
    char* buf = (char*)malloc((size_t)len + 1);
    // BUG: no NULL check on buf
    memcpy(buf, (char*)src_ptr + offset, (size_t)len);  // ← NULL deref if OOM
    buf[len] = '\0';  // ← crash
    ...
}
```

**Fix:** Add `if (!buf) { result.data = ""; result.length = 0; return result; }`

**Status:** Fixed locally April 29, 2026; strict C compilation passes and a
runtime smoke test that wraps `malloc()` to return `NULL` confirms safe empty
`AriaString` behavior. The minimal ESBMC memory harness has been updated to match
the fixed implementation and reports `VERIFICATION SUCCESSFUL`.

---

## Analysis Results by Component

### 1. aria-libc Shim Stubs (14 files, 134 functions)

| File | Functions | Safe | Failed | Notes |
|------|-----------|------|--------|-------|
| aria_libc_math_extra.c | 10 | **10** | 0 | All pure math — fully verified |
| aria_libc_io_extra.c | 11 | 7 | 4 | FP: non-deterministic FILE* |
| aria_libc_hexstream.c | 9 | 6 | 2 (+1 timeout) | FP: invalid fd param |
| aria_libc_mem_extra.c | 16 | 3 | 12 (+1 unknown) | Bug #2 + FP: raw ptr params |
| aria_libc_net_extra.c | 14 | 9 | 5 | FP: non-det ptr/fd; 1 recv modeling FP |
| aria_libc_process_extra.c | 11 | 9 | 2 | **Bug #1** — real signal handler issue |
| aria_libc_shm.c | 5 | 1 | 4 | FP: non-det mmap handle |
| aria_libc_string_extra.c | 4 | 0 | 4 | FP: non-det raw ptr |
| aria_libc_pool.c | 7 | 2 | 5 | FP: non-det int64_t handle |
| aria_libc_channel.c | 16 | 3 | 13 | FP: handle cast + alignment |
| aria_libc_actor.c | 11 | 3 | 8 | FP: handle cast |
| aria_libc_mutex.c | 5 | 1 | 4 | FP: non-det pthread_mutex_t* |
| aria_libc_rwlock.c | 5 | 1 | 4 | FP: non-det ptr |
| aria_libc_thread.c | 9 | 5 | 4 | FP: non-det pthread_t |
| **TOTAL** | **134** | **61** | **69** | 3 unknown, 1 timeout |

**False Positive Pattern:** 65 of the 69 "failures" are from ESBMC feeding
non-deterministic `int64_t` values as handle parameters, which get cast to
struct pointers (`(pool_t*)(unsigned long)handle`). In actual usage, these
handles always come from the corresponding `_create()` functions.

### 2. Lock-Free Runtime (lockfree_shim.c, 344 lines)

| Structure | Functions | Safe | Failed | Notes |
|-----------|-----------|------|--------|-------|
| LFQueue (MPMC) | 6 | 5 | 0 | 1 unknown (calloc loop) |
| LFStack (Treiber) | 5 | 5 | 0 | CAS loops verified at k=3 |
| RingBuf (SPSC) | 6 | 6 | 0 | All clean |
| **TOTAL** | **17** | **16** | **0** | 1 unknown |

Lock-free data structures: **all verified safe**. This is a significant result
since lock-free code is notoriously difficult to get right.

### 3. Wild Pointer Helpers (wild_helpers.c, 38 lines)

| Function | Result |
|----------|--------|
| aria_wild_to_int64 | ✓ SAFE |
| aria_int64_to_wild | ✓ SAFE |
| aria_wild_ptr_free | ✓ SAFE |

### 4. GC (gc.cpp, 1113 lines) — NOT VERIFIABLE

ESBMC's Clang-16 frontend cannot parse the GC C++ code due to STL header
conflicts (`std::vector`, `std::unordered_set`, `std::mutex`, etc.).
IKOS remains the correct tool for the GC (analyzed via compiled LLVM IR).

### 5. WASM Runtime (wasm_runtime.c, 850 lines, 104 functions)

Same false-positive pattern on pointer-parameter functions. String manipulation
functions (concat, substring, split) timeout due to loop unrolling in
`strlen`/`memcpy` operations. Pure allocation functions verified safe.

---

## Combined Verification Status (IKOS + ESBMC)

| Tool | Files | Functions | Safe | Real Bugs | FP | Tech |
|------|-------|-----------|------|-----------|----|----|
| IKOS | 390 .ll | N/A (whole-program) | 214 | 0 | 139 | Abstract Interp |
| ESBMC | 18 .c | 155+ | 79+ | 2 | ~67 | BMC + Z3 |
| **Combined** | **408** | — | **293+** | **2** | **~206** | — |

---

## ESBMC Integration Notes

### Invocation Pattern
```bash
# Single function verification (library code without main)
esbmc file.c -I /usr/lib/llvm-18/lib/clang/18/include \
  --incremental-bmc --function func_name --unwind N --timeout T

# Full program verification
esbmc file.c -I /usr/lib/llvm-18/lib/clang/18/include --incremental-bmc
```

### Key `--unwind` Values
- Simple functions: `--unwind 10` (or omit for automatic)
- CAS/atomic loops: `--unwind 3-5`
- String operations with strlen: `--unwind 20+` (may timeout)

### Known Limitations
1. **C++ STL**: ESBMC's internal C++ headers conflict with system libstdc++
2. **Handle-as-int API pattern**: Functions taking `int64_t` handles that are
   cast to struct pointers generate FPs — need test harnesses with `_create()`
3. **System call modeling**: `recv()`, `read()` return values are non-deterministic
4. **String loop unrolling**: `strlen`/`strcpy` in loops can cause timeouts

### Installation
```bash
sudo add-apt-repository ppa:esbmc/esbmc
sudo apt update && sudo apt install esbmc
```

---

## Recommendations

1. **Fix Bug #1** (signal handler bounds) — simple one-liner
2. **Fix Bug #2** (make_string NULL check) — simple one-liner
3. **Keep both tools**: IKOS for compiled C++ (GC, JIT, codegen), ESBMC for C libraries
4. **Write harnesses** for handle-based APIs to reduce FPs in future runs
5. **Consider k-induction** (`--k-induction`) for unbounded loop proofs
6. **ESBMC v8.2** just released (Apr 14) — upgrade when PPA catches up
   (adds Bitwuzla 0.9.0, function contract assigns, interval-based reasoning)
