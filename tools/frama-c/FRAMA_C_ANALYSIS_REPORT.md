# Frama-C Eva Analysis Report — aria-libc Shim Stubs

**Tool:** Frama-C 25.0 (Manganese) — Eva plugin (Evolved Value Analysis)
**Date:** April 15, 2026
**Analyst:** Automated batch via `aria_frama_c.sh`
**Precision:** Level 3 (domains: cvalue, equality, gauges, symbolic-locations)
**Mode:** `-lib-entry` (each function analyzed as standalone entry with unconstrained inputs)

---

## Executive Summary

| Metric | Count |
|--------|-------|
| Total functions analyzed | 84 (81 shim + 3 runtime) |
| Verified safe (0 alarms) | 21 |
| With alarms | 63 |
| Errors / timeouts | 0 |
| Skipped (pthread/_Atomic) | 6 files (actor, channel, mutex, pool, rwlock, thread) + lockfree_shim |
| **Confirmed real bugs** | **3** (2 from signal handler, 1 from make_string) |
| Library-mode false positives | ~55 |
| Worth investigating | ~5 |

**Key finding:** Frama-C independently confirmed all bugs previously found by ESBMC, using a completely different analysis methodology (abstract interpretation vs. bounded model checking). This gives strong triangulated confidence in the findings.

**Post-analysis fix status (April 29, 2026):** The confirmed signal-handler and
`make_string` defects have been fixed in the local `aria-libc` working tree.
Targeted ESBMC checks verify the two signal functions after the lower-bound
guard was added, and a forced-null `malloc()` smoke test verifies the
`make_string` allocation-failure path.

---

## Alarm Classification

### Category 1: Confirmed Real Bugs (3 alarms)

These were independently found by ESBMC (BMC) and are real defects:

| Function | File | Line | Alarm | Bug |
|----------|------|------|-------|-----|
| `signal_register` | process_extra.c | 48 | `accessing out of bounds index. assert 0 ≤ signum` | Negative signum → OOB array access on `saved_handlers[signum]` |
| `signal_restore` | process_extra.c | 53 | `accessing out of bounds index. assert 0 ≤ signum` | Same: no lower-bound check before array index |
| `make_string` | mem_extra.c | 71-72 | `precondition 'valid_dest' got status invalid` + `out of bounds write` | malloc() return not NULL-checked before memcpy/write |

### Category 2: Library Precondition Unknown (~50 alarms)

In `-lib-entry` mode, function parameters are completely unconstrained. Eva correctly reports that it cannot prove libc preconditions when inputs are arbitrary. These are **not bugs** — they are Eva being sound.

**Subcategories:**

- **`precondition 'valid_string_s' got status unknown`** (strlen, strcmp, system) — Cannot prove `const char*` params point to valid NUL-terminated strings. In practice, Aria's compiler guarantees this.

- **`precondition 'valid_dest'/'valid_src' got status unknown`** (memcpy, memmove, memset) — Cannot prove pointer params are valid. These are raw memory shims; callers provide valid allocations.

- **`precondition 'valid_fd'/'valid_sockfd' got status unknown`** (read, write, close, socket ops) — Cannot prove file descriptors are valid integers. Runtime constraint, not a code bug.

- **`precondition 'valid_stream' got status invalid`** (fclose, feof, fseek, ftell, fread, fwrite, fgets) — FILE* params unconstrained; Eva marks as invalid since it can't see the fopen() that created them. Would be resolved with whole-program analysis.

- **`precondition 'freeable' got status unknown`** (free, realloc) — Cannot prove pointer came from malloc. Standard pattern for allocator wrappers.

- **`pointer comparison. assert \pointer_comparable(...)`** — Eva's strict compliance: comparing a non-concrete pointer to NULL may have undefined behavior in the abstract model. Harmless in practice.

### Category 3: Non-finite Math Values (6 alarms)

| Functions | Alarm |
|-----------|-------|
| copysign, fma, hypot | `non-finite double value. assert \is_finite(tmp)` |
| flt64_from_bits, flt32_bits, flt32_from_bits | `non-finite float/double value` |

Eva warns that math functions may produce NaN/Inf when inputs are unconstrained. These are **expected behavior** — `isnan()`, `isinf()` exist precisely for this. The bit-reinterpretation functions (`flt32_bits`, `flt64_from_bits`) intentionally handle all bit patterns.

### Category 4: Worth Investigating (5 alarms)

| Function | Alarm | Assessment |
|----------|-------|------------|
| `net_resolve` | `accessing uninitialized left-value. assert \initialized(&res)` | FP — `getaddrinfo()` initializes `res` via output param; Eva can't fully model this |
| `net_resolve` | `out of bounds read. assert \valid_read(&res->ai_addr)` | FP — follows from the above; `res` is valid after successful `getaddrinfo()` |
| `shm_destroy/size/read/write_int64` | `out of bounds read. assert \valid_read(&s->ptr)` | FP — handle-as-int64 pattern: struct pointer passed as int64, cast back. Same as ESBMC's FP. |
| `string_from_buf` | `out of bounds write. assert \valid(s + len)` | Borderline — malloc(len+1) could fail silently if len is near SIZE_MAX, but `if (!s) return "";` guard exists. Eva can't prove malloc allocated enough. |
| `net_set_blocking/nonblocking` | `precondition 'cmd_has_int_arg' got status invalid` | FP — Frama-C's fcntl model doesn't recognize F_GETFL/F_SETFL as accepting int args. Model limitation. |

---

## Cross-Tool Comparison

### Three-Tool Triangulation Matrix

| Defect | IKOS (AI/LLVM) | ESBMC (BMC/AST) | Frama-C (AI/Source) |
|--------|----------------|------------------|---------------------|
| signal_register negative index | ✗ Not found | ✓ Found | ✓ Found |
| signal_restore negative index | ✗ Not found | ✓ Found | ✓ Found |
| make_string NULL-after-malloc | ✗ Not found | ✓ Found | ✓ Found |
| Handle-as-int64 FP pattern | N/A (LLVM level) | 65 FP | 4 FP (shm only) |
| pthread/atomic code | ✓ Analyzed (via LLVM) | ✗ C++ STL conflict | ✗ _Atomic parse fail |
| wild_helpers (3 funcs) | ✓ Safe | ✓ Safe | ✓ Safe |

### Why IKOS Missed These Bugs

IKOS operates on LLVM IR bitcode, which has already been through Clang's optimizer. The optimizer:
1. May inline the signal handler array access with known-constant indices
2. May eliminate the NULL-check-needed path via dead code elimination
3. Operates at a lower abstraction level where source-level patterns like "negative array index" are harder to detect

ESBMC and Frama-C both operate at the **source/AST level**, preserving the semantic meaning of the original C code.

### Tool Characteristics

| Property | IKOS | ESBMC | Frama-C Eva |
|----------|------|-------|-------------|
| **Analysis basis** | LLVM IR | C source (Clang AST) | C source (CIL) |
| **Method** | Abstract interpretation | Bounded model checking | Abstract interpretation |
| **Soundness** | Sound | Sound within bounds | Sound |
| **C++ support** | Full (via LLVM) | Partial (STL issues) | None (Frama-Clang experimental) |
| **C11 atomics** | Yes | Yes | No |
| **pthreads** | Yes (sequential model) | Yes | No |
| **Speed (84 funcs)** | ~45 min (with IR conversion) | ~8 min | **28 seconds** |
| **False positive rate** | Low (0 in our run) | Medium (handle-as-int) | High in -lib-entry mode |
| **Annotation support** | None | `__ESBMC_assume()` | ACSL (rich specification language) |

---

## Files Analyzed

### Pure C (8 files, 81 functions)
| File | Functions | Safe | Alarms |
|------|-----------|------|--------|
| aria_libc_hexstream.c | 10 | 2 | 8 |
| aria_libc_io_extra.c | 11 | 1 | 10 |
| aria_libc_math_extra.c | 10 | 4 | 6 |
| aria_libc_mem_extra.c | 16 | 2 | 14 |
| aria_libc_net_extra.c | 14 | 3 | 11 |
| aria_libc_process_extra.c | 11 | 4 | 7 |
| aria_libc_shm.c | 5 | 1 | 4 |
| aria_libc_string_extra.c | 4 | 1 | 3 |

### Runtime (1 file, 3 functions)
| File | Functions | Safe | Alarms |
|------|-----------|------|--------|
| wild_helpers.c | 3 | 3 | 0 |

### Skipped (6 files — pthreads/_Atomic)
- aria_libc_actor.c, aria_libc_channel.c, aria_libc_mutex.c
- aria_libc_pool.c, aria_libc_rwlock.c, aria_libc_thread.c
- lockfree_shim.c

---

## Recommendations

1. **Fix the 2 confirmed real bugs** (already known from ESBMC): **done locally April 29, 2026**
   - `signal_register`/`signal_restore`: Add `signum >= 0 &&` to bounds check
   - `make_string`: Add `if (!buf) { result.data = ""; result.length = 0; return result; }` after malloc

2. **ACSL annotations** (future enhancement): Frama-C's real power is with ACSL contracts. Adding `/*@ requires \valid(ptr); ensures \result >= 0; */` annotations would eliminate most Category 2 alarms and enable the WP plugin for formal proofs. This is the path to true certification-grade analysis.

3. **Upgrade to Frama-C Germanium** (low priority): Version 32.0 has improved Eva precision and better stdlib models. Install via opam when/if ACSL annotation work begins.

4. **Recommended verification strategy**:
   - **IKOS** for C++ runtime code (GC, JIT, compiler internals) — operates at LLVM level, handles STL
   - **ESBMC** for targeted bug-finding with counterexamples — best for complex logic
   - **Frama-C** for pure C code with annotations — fastest, most precise when annotated

---

## Reproduction

```bash
# Install
sudo apt install frama-c-base

# Single function
frama-c -eva -eva-precision 3 -lib-entry -main aria_libc_mem_make_string \
  -no-unicode REPOS/aria-libc/shim/stubs/aria_libc_mem_extra.c

# Batch analysis
bash REPOS/aria/tools/frama-c/aria_frama_c.sh 3
```

---

*Analysis completed in 28 seconds across 84 functions. All results stored in `tools/frama-c/results_20260415_173416/`.*
