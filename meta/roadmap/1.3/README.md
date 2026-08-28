# Cycle 1.3 — The exotic numeric tier

**Phase C.** The types that exist because NIKOLA needs them: `simd<T, N>` and
the library tier over it (`vec2`/`vec3`/`vec4`/`vec9`, `matrix<T>`, `tensor<T>`,
`tmatrix`, `ttensor`), the twisted fixed-point `tfp*` family and `dim256<Unit>`
over it, the balanced ternary/nonary bases `trit`/`tryte`/`nit`/`nyte`, the
`frac*` exact rationals, and `complex<T>`. All of the base names lex and parse
since Phase A; none of them resolves to a kind — `builtin_types.npk` carries
them `known: false`, and the two backend rungs (`ir_expr`'s vector constructor
and arithmetic sites) cite this cycle by name ("1.3 (G-3)").

> Created at 1.1-close by the user's G-3 decision (D-191): **a new cycle,
> inserted before self-hosting** — the second application of the D-183
> renumbering precedent (self-hosting is 1.4, verification 1.5, Astrée 1.6).

> **Correction at cycle open (1.3.0):** the first draft of this file called
> `tfp*` "ternary-floating". It is neither — **`tfp` is Twisted FIXED Point**
> (D-036/D-037): Q16.16/Q32.32/Q64.64/Q128.128 with the most-negative raw
> value as sticky ERR. The ternary tier is `trit`/`tryte`/`nit`/`nyte`, and
> nothing in this cycle is floating-point emulation.

## What is already SETTLED (implement, do not re-open)

- **D-135** — `simd<T, N>` is the vector mechanism (an LLVM vector type, in a
  register); `vec2`/`vec3`/`vec4`/`vec9`, `matrix<T>`, `tmatrix`, `tensor<T>`,
  `ttensor` are LIBRARY types built on it and are not keywords. The ternary
  family stays primitive — no hardware implements balanced ternary, so the
  compiler is the only place the emulation can be done well.
- **D-036** — `tfp*` is Twisted Fixed Point, four widths; `dim256<Unit>` is
  the former `fix256`: Q128.128 **with compile-time dimensional analysis**,
  identical to `tfp256` at the IR level (units erased before lowering).
- **D-037** — "twisted" is the family that reserves a value as sticky ERR;
  `int*`/`uint*` wrap, `flt*` is IEEE, and no plain numeric type carries a
  sentinel. The per-family posture (wrap / ERR / exact / IEEE) is chosen at
  the declaration, by type.
- **D-144** — the tbb integers' branch-free saturating arithmetic and sticky
  ERR (landed 0.9) is the implementation discipline every twisted family
  reuses.
- **D-007** — division by zero: plain integers trap to `failsafe`; a twisted
  type yields ERR. `tfp`/`frac` division follows the tbb rule, not the trap.
- **D-147** — the literal grammar for the tier is already lexed: `1T0t`
  (ternary, `T` = −1), `2An` (nonary, `a`..`d` = −1..−4), value-neutral
  leading zero, every literal beginning with a decimal digit.
- **The hardware caveat is load-bearing** (TYPE_REFERENCE §7): ternary/nonary
  HARDWARE is a planned target. The `i8`/`i16` backings and the binary-spare
  ERR sentinels are LOWERING choices of the binary rung — the frontend checks
  ternary arithmetic AS ternary, and nothing above the backend may assume the
  binary representation.

## What the cycle-open survey established (1.3.0)

- **The prototype's `tfp_ops.cpp` is the OBSOLETE design** — a deterministic
  software float ({exp: tbb16, mant: tbb16/tbb48}). D-036/D-037 record its
  deliberate replacement: redundant with `flt`'s NaN taint, repurposed to
  fixed point. The prototype is the oracle for the tiers that survived
  unchanged (ternary ops, frac invariants), NOT for `tfp`.
- **Prototype oracle, ternary** (`ternary_ops.cpp`): arithmetic (+ − ×; ÷ and
  mod at `tryte`/`nyte` only), comparison, negation, Kleene three-valued
  logic on the single digits (True=1, Unknown=0, False=−1), digit extraction,
  carry chains, and sticky ERR in a binary-spare state (−128 on `i8`).
- **Prototype oracle, frac** (`frac_ops.cpp`): mixed-number {whole, num,
  denom}, INVARIANT-normalized after every operation (denom > 0, proper
  fraction, gcd = 1, sign on whole), ERR via component sentinel or denom 0.
- **Spec defects found and fixed at 1.3.0**: TYPE_REFERENCE §7 said `nyte` =
  "2 nits" (it is 5 — 3^10 = 9^5 = 59049 states, which is why `tryte` and
  `nyte` are both `i16`); §21's `complex` support list said `fix32`/`fix64`
  (obsolete names — `tfp32`/`tfp64` under D-036).

## The subcycle map

| Subcycle | What lands | Depends on |
|---|---|---|
| **1.3.0** | The decision batch (G-4…G-10 → D-194…D-200), the survey above, spec fixes | — |
| **1.3.1** | `simd<T, N>`: TY_SIMD, resolution, `<N x T>` lowering, constructor/splat, elementwise ops, compares, indexing, casts, reductions | — |
| **1.3.2** | `tfp32/64/128/256`: Q-format arithmetic with D-144 ERR discipline, literals, casts, `ToString` | — |
| **1.3.3** | `dim256<Unit>`: the unit exponent-vector algebra in the checker, erased lowering over tfp256 | 1.3.2 |
| **1.3.4** | `trit`/`tryte`/`nit`/`nyte`: kinds, balanced literals into values, arithmetic + logic + digit ops, ERR, `ToString` | — |
| **1.3.5** | `frac8/16/32/64`: invariant-normalized mixed numbers, operators, ERR, `ToString` | — |
| **1.3.6** | `complex<T>`: {T,T}, operators (Smith's division on flt), methods, `ToString` | 1.3.2 |
| **1.3.7** | The library tier: `lib/nvec.npk` (vec2/3/4 over simd, vec9), `lib/ntensor.npk` (matrix/tensor/tmatrix/ttensor, heap-owned under the 1.2 managed regime) | 1.3.1, 1.3.4 |
| **1.3.8** | Close: whole-tier `// ll:` markers, `check_kinds_lowered_or_refused` retirement of the G-3 rungs, optimised-output check (the pinned LLVM-optimiser memory), doc sync | all |

Ordering is consumer-first (D-143 precedent) against Nikola's architecture:
the transformer/Mamba path consumes `simd` floats, the emulated
neurotransmitters are the drift-critical `tfp`/`dim256` story, and the
9-dimensional nonary manifold consumes the ternary tier and `ttensor` (the
prototype's `ttensor` carries `dims[9]` — rank 9 — natively).

## Watch for

- **`check_kinds_lowered_or_refused` and the `// ll:` discipline** carry this
  cycle: every representation lands with its marker row, and the rungs only
  retire as each type's arithmetic actually executes.
- **LLVM's optimiser removed a load-bearing guarantee once** (the pinned
  memory): whatever `simd`/vector lowering emits must be checked against the
  optimised output, not just `-O0`.
- **`builtin_types.npk` is GENERATED** from LEXICAL_REFERENCE.md — a type
  gaining a kind means editing the spec's table and regenerating, never
  hand-editing the table.
- **No unary minus exists** — negation in every new family is `0 - x` (or a
  method), matching the rest of the language.
