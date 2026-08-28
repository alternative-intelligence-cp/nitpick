# Cycle 1.3 — The exotic numeric tier

**Phase C.** The types that exist because NIKOLA needs them, made primitives on
an explicitly uncertain performance hypothesis (the pinned memory records it):
`vec2`/`vec3`, `matrix`, `tensor`, the ternary-floating `tfp*` family, the
`frac*` rationals, `dim256`, `simd`, and `complex`. All of them parse, resolve
and type since Phase A; none of them lowers — their rungs cite this cycle by
name ("1.3 (G-3)").

> Created at 1.1-close by the user's G-3 decision (D-191): **a new cycle,
> inserted before self-hosting** — the second application of the D-183
> renumbering precedent (self-hosting is 1.4, verification 1.5, Astrée 1.6).
> The reasoning is the standing constraint: everything that is going in must
> land before the fixpoint re-close and the one-shot verified artifact, and
> "we aren't in any rush."

> Detailed **map**, not a plan. Subcycles are written when the cycle is
> reached; what is bounded now is the decision surface below.

## What exists already

- **Lexing/parsing/typing**: the full tier, since Phase A (the frontend was
  built once, in full). The two backend rungs are `ir_expr`'s vector
  constructor and arithmetic sites.
- **The ternary INTEGER widths (`tbb*`) are DONE** (0.9, D-144 — branch-free
  saturating arithmetic, the sticky ERR): this cycle's ternary work is the
  FLOATING tier (`tfp*`) only.
- **The hardware caveat is load-bearing** (the pinned ternary memory):
  ternary/nonary HARDWARE is a planned target, so every binary representation
  chosen here is a LOWERING choice and must never become the type's identity.

## The decision surface (settle at cycle open)

1. **Representations**: `vec2`/`vec3` (2×/3×flt64? alignment; the SIMD
   register question), `matrix`/`tensor` (owned heap vs value — likely the
   arena story), `frac*` (num/den widths, normalization timing, the
   div-by-zero posture), `tfp*` (the binary emulation of ternary floats —
   the D-144 precedent says saturate-and-flag, not trap), `dim256` (a fixed
   256-wide vector? layout), `complex` (2×flt64, IEEE pairing rules),
   `simd` (target width; the LLVM vector-type mapping).
2. **The operator surface per type** — which of the existing operators each
   admits (D-036's semantic-over-representation rule applies: no accidental
   integer semantics).
3. **The float-format question `tfp` inherits** from §6b (float `ToString`
   needs shortest-round-trip; the drift hazard is the safety rationale's own
   example).
4. **What Nikola actually consumes first** — the tier should land
   consumer-first (the D-143 precedent: totality decisions made against real
   uses, not speculatively).

## Watch for

- **`check_kinds_lowered_or_refused` and the `// ll:` discipline** carry this
  cycle: every representation lands with its marker row, and the rungs only
  retire as each type's arithmetic actually executes.
- **LLVM's optimiser removed a load-bearing guarantee once** (the pinned
  memory): whatever `simd`/vector lowering emits must be checked against the
  optimised output, not just `-O0`.
