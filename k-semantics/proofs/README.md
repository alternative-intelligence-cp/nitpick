# K Proofs

Proof artifacts for the Aria v0.18.x executable semantics live here. Run them
from the repository root with:

```bash
bash ./k-semantics/run_k_proofs.sh --require-k
```

The proof runner compiles `k-semantics/aria.k` with the Haskell backend required
by `kprove`, discovers `*.k` files in this directory, reads each file's
`// proof-module: MODULE` header, and expects `kprove` to return `#Top`.

Current proof modules:

- `arithmetic-proofs.k` — symbolic arithmetic claims for `int32`, `int64`, and
   `tbb32` add/subtract/multiply normalization, division-by-zero sentinel
   routing, and numeric-operand `ERR` / `Unknown` propagation.
- `core-proofs.k` — concrete executable-core claims covering sticky `ERR`,
  bounded `int32` wraparound, `tbb32` overflow-to-`ERR`, zero-step loop
   failsafe routing, fixed reassignment failsafe routing, `?!` error-result
   failsafe routing, `println` newline/byte-count behavior, zero-argument helper
   `pass`/`fail` polarity, and direct struct field writes preserving unrelated
   fields, plus modeled one- and two-argument helper `pass`/`fail` polarity with
   caller-frame restoration.
- `field-alias-proofs.k` — direct and nested mutable field-alias writeback plus
   immutable field-alias assignment rejection.
- `pin-proofs.k` — pin registration, read-only pin mutation rejection, and
   pinned-host reassignment rejection.
- `pin-by-value-proofs.k` — pinned-host by-value rejection for helper-call and
   terminal-exit positions.
- `pointer-proofs.k` — local address-of/deref/store-through and pointer-member
   behavior.
- `pointer-path-proofs.k` — concrete nested pointer and pin-path claims,
  including pin-derived pointer alias read and mutation-rejection behavior.
- `borrow-path-proofs.k` — direct and nested path-sensitive borrow assignment
   claims.
- `control-rules-proofs.k` — concrete `pick`/`fall` dispatcher and labeled-arm
   routing claims, plus `limit<Rules>` declaration/assignment commit and
   failsafe no-commit claims.

Planned initial claims:

1. sticky `ERR` arithmetic remains sticky — initial concrete claim added
2. `tbb32` overflow/underflow and the min sentinel normalize to `ERR` — initial
   concrete overflow claim added
3. `int32`/`int64` arithmetic stays within two's-complement bounds — initial
   concrete `int32` wrap claim added
4. zero-step loops route to failsafe instead of diverging — concrete claim added
5. reassignment to `fixed` bindings cannot silently mutate state — concrete claim added
6. `?!` on an error `Result` always reaches the failsafe path — concrete claim added
7. zero-argument helper calls preserve `pass`/`fail` Result polarity — concrete claims added
8. `println` appends exactly one newline and returns the emitted byte count — concrete claim added
9. parameterized helper calls restore caller frames and preserve `pass`/`fail`
   Result polarity — concrete one- and two-argument claims added
10. struct field writes update only the selected field and preserve unrelated
   fields — concrete claim added
11. `pick` dispatch chooses the first matching arm, and `fall label;` reaches
    only the named labeled arm before returning to the pick continuation —
    concrete dispatcher and label-routing claims added
12. integer `limit<Rules>` declarations and assignments either satisfy every
    cascaded rule condition or route to `failsafe` without committing the
    violating store update — concrete commit/no-commit claims added
13. symbolic arithmetic lemmas generalize the concrete numeric proof seed for
   `int32`, `int64`, `tbb32`, division-by-zero, and numeric `ERR` / `Unknown`
   propagation — initial symbolic claims added
