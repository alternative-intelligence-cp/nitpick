# Transformer Experiment — Aria Language Capability Assessment

**Date**: March 9, 2026  
**Purpose**: Determine what Aria can express for ML compute; inform `stdlib.linalg` / `stdlib.ml` planning.

## What This Is

A minimal transformer encoder block — single-head scaled dot-product attention — implemented entirely
in current Aria (`ariac` LLVM-18 build). 

Architecture: SEQ=4, D_MODEL=8, D_K=4, D_V=4, D_FF=16, VOCAB=8.

All computation is inline in `main()`. No arrays are passed between functions (see **Blocked** section).

## Run It

```bash
cd /home/randy/Workspace/REPOS/aria

# Compile
./build/ariac examples/transformer/transformer_experiment.aria \
  -o examples/transformer/transformer_experiment 2>&1 | grep -v "^\[DEBUG\]"

# Run
./examples/transformer/transformer_experiment
```

Expected output:
```
=== Aria Minimal Transformer Forward Pass ===
  SEQ=4  D_MODEL=8  D_K=4  D_FF=16  VOCAB=8  single-head attention

Step 1 done: input embeddings loaded
Step 2 done: Q, K, V projections computed
Step 3 done: attention scores computed (Q @ K^T / sqrt(d_k))
Step 4 done: softmax applied
  softmax row 0 verified: sums to 1.0 (+/- 0.001)
Step 5 done: attention-weighted value aggregation
Step 6 done: output projection + residual connection #1
Step 7 done: layer normalization #1
Step 8 done: FFN hidden layer (W1 + ReLU)
Step 9 done: FFN second layer + residual connection #2
Step 10 done: layer normalization #2
Step 11 done: final linear projection (logits)

=== Forward Pass Validation ===
  Position 0 predicted token: 0
  Sum of all logits: 1.3322676295501878e-15

=== TRANSFORMER FORWARD PASS: COMPLETE ===
All 11 steps executed without error.
```

The logit sum is ~1.33e-15 (not exactly 0 due to floating point, but correct). Layer normalization
centers each position's hidden state around 0 with unit variance; the near-identity final projection
preserves this. The result is deterministic across runs.

## What Works ✅

| Feature | Notes |
|---------|-------|
| `flt64[N]` stack arrays | Up to `[128]` confirmed, flat row-major indexing |
| `arr[i]` element read/write | int32 subscript, correct at runtime |
| `while (i < N)` loops | With int32 counter, fully functional |
| Nested while loops | Three levels deep tested (row, col, k in matmul) |
| `flt64` arithmetic: `+`, `-`, `*`, `/` | All correct |
| `flt64` comparisons: `==`, `!=`, `<`, `>` | Work as expected |
| `extern "libm" { func:exp = flt64(flt64:x); }` | Declares libm symbols by C name |
| Thin wrapper functions over extern | `func:exponential = flt64(flt64:x) { pass(exp(x)); };` works |
| String interpolation `&{int32_var}` | Prints int32 values correctly |
| String interpolation `&{flt64_var}` | Prints flt64 values correctly |
| `if (cond) { ... }` (no else required in some contexts) | Works inline in while loops |
| Array literal initializer `= [v1, v2, ...]` | Required — uninitialized arrays crash |
| Large literal initializers (128 elements) | Compiles and runs correctly |

## What Is Blocked ❌ (Compiler Bugs / Missing Features)

These are all tracked in `META/ARIA/0.1.0_FEATURE_FREEZE_CHECKLIST.md`.

### P4 — Struct Fields Cannot Hold Fixed-Size Arrays

```aria
// FAILS — parser rejects flt64[N] inside struct body
struct:LinearLayer = {
    flt64[32]:weight;
    flt64[4]:bias;
};
```

Impact: weight matrices cannot be encapsulated. Everything must be ad-hoc locals.  
Workaround: all arrays are independent local variables in `main()`.

### P5 — No 2D Array Syntax

```aria
// FAILS — not implemented
flt64[4][8]:matrix;
```

Impact: all matrices are flat row-major `flt64[M*N]`. Access is `m[i * COLS + j]`.  
Workaround: manual row-major indexing — verbose but works correctly.

### BUG — Cannot Pass Stack Arrays to Functions via Pointer

All nine possible mechanisms were tested and all fail:

| Attempt | Error |
|---------|-------|
| `@arr` as argument to `flt64->` param | `@arr` gives `flt64[N]@`, not `flt64@` — type mismatch |
| `@arr[i]` to get element address | "Address-of (@) requires lvalue — only variables supported" |
| `flt64*` parameter type | "Use '->' for pointers, '*' only valid in extern blocks" |
| `flt64[N]@` parameter type | Parser rejects `@` after `[N]` in parameter position |
| `flt64[N]->` parameter type | Parser rejects `->` after `[N]` in parameter position |
| `wild flt64@:p = @arr` | Parser rejects `@` after `flt64` in `wild` declaration |
| `@cast<flt64@>(ptr)` | `@` terminates the `<>` block |
| `@cast<flt64->(ptr)` | `>` in `->` terminates the `<>` block |
| `wild flt64->:arr = alloc(32)` | `alloc()` returns `int8@`; no coercion to `flt64->` |

Impact: no helper functions possible for matrix operations — everything inline in `main()`.  
Workaround: all 11 transformer steps are inline (see `transformer_experiment.aria`).

### BUG — Global flt64 Array Element Loads Use Wrong IR Type

```aria
// Compiles but crashes at runtime with LLVM FATAL:
// "Cannot select: fadd i64 ... ConstantFP:f64"
flt64[4]:global_weights = [1.0, 2.0, 3.0, 4.0];

func:main = int32() {
    flt64:v = global_weights[0];   // BUG: loaded as i64, not f64
    ...
}
```

Root cause: IR generator's GEP + load for global array element access uses `i64` load type instead
of `f64`.  
Note: global scalar `flt64:x = 1.0` works fine. Only *array element loads* from global arrays break.  
Workaround: all arrays are local (`main()` stack).

### BUG — `use math;` Broken from Non-Root Source Paths + math.aria Has Internal Errors

Two separate issues:

1. **Module path resolution**: `rootPath` is set to the directory of the source FILE being compiled,
   not the working directory. Compiling `examples/transformer/foo.aria` sets rootPath to 
   `examples/transformer/`, so `use math;` searches for `examples/transformer/lib/std/math.aria`,
   which doesn't exist. Can be worked around with `-I /path/to/aria/lib/std`.

2. **math.aria internal errors**: Even with `-I`, `math.aria` fails type-checking because several
   of its `pub func` bodies use the `return` keyword instead of `pass`, and some functions reference
   `fabs` (which has a scoping issue with extern-declared names). The whole module fails, making
   `exponential()` and `square_root()` unavailable even though those specific functions are correct.

   Affected functions in math.aria: `floor_f64`, `ceil_f64`, `round_f64`, `trunc_f64`, `mod_f64`,
   `factorial_i32`, `factorial_f64`, `binomial_i32`, `lerp_f64`, `inverse_lerp`, `map_range`,
   `smoothstep`, `smootherstep`, `clamp_f64`, `wrap_f64`, `ping_pong`, `sign_f64`, `copysign`.

   Workaround: declare extern functions directly in caller file:
   ```aria
   extern "libm" {
       func:exp  = flt64(flt64:x);
       func:sqrt = flt64(flt64:x);
   }
   func:exponential = flt64(flt64:x) { pass(exp(x));  };
   func:square_root = flt64(flt64:x) { pass(sqrt(x)); };
   ```

### MINOR: `fail` Is a Reserved Keyword

Cannot use `fail` as a variable or parameter name. Use `ec`, `err`, `retcode` etc.

### MINOR: `defer` Requires a Block

`defer { aria.free(x); }` is correct; `defer aria.free(x);` is rejected.

### MINOR: NIL-Returning Functions Must Be Called with `drop()`

The compiler warns "Unused result value must be checked" if you call any function without consuming
its return value. Since `NIL` functions are typed as returning `NIL` (not nothing), you need the
`drop()` pattern. Use `int32` return + `pass(0)` + `drop(call())` as the workaround.

## Architecture Notes for Future stdlib.ml / stdlib.linalg

This experiment confirms the minimal viable set for an Aria ML library:

1. **Immediate prerequisite (blocks all helper functions)**: Fix stack array-to-pointer passing (see P4/P5 bugs above). Without this, every matrix operation must be inlined at the call site — completely impractical for real use.

2. **Desirable (for clean APIs)**:
   - 2D array syntax (`flt64[M][N]`) or syntax sugar over flat arrays
   - Struct fields holding arrays (allows `struct:Tensor = { flt64[512]:data; int32[4]:shape; }`)
   - Fix math.aria (replace `return` with `pass`, fix `fabs` scoping)
   - Fix global float array element load type bug

3. **What works well now**:
   - Raw compute (6 levels of nested while loops, 128+ element arrays, full flt64 arithmetic)
   - The language has sufficient expression power for forward passes once the array-passing bug is fixed
   - `extern "libm"` is a clean FFI mechanism for math primitives

4. **Design validation**: The transformer experiment confirmed that `while`/`for` loop syntax 
   (not `till`/`when`) was the right choice — the readability of counter-based matrix loops
   demonstrates why iterator/range-based loops alone would be insufficient for ML compute.

## Files in This Directory

- `transformer_experiment.aria` — The main experiment (full annotated transformer forward pass)
- `probe_01_float_arrays.aria` — Early probes of float array mechanics
- `probe_alloc.aria` — Heap allocation probes (result: `alloc()` only returns `int8@`)
- `README.md` — This file
