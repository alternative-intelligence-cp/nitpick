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

- `core-proofs.k` — concrete executable-core claims covering sticky `ERR`,
  bounded `int32` wraparound, `tbb32` overflow-to-`ERR`, zero-step loop
  failsafe routing, fixed reassignment failsafe routing, and `?!` error-result
  failsafe routing.
- `pointer-path-proofs.k` — concrete nested pointer and pin-path claims,
  including pin-derived pointer alias read and mutation-rejection behavior.

Planned initial claims:

1. sticky `ERR` arithmetic remains sticky — initial concrete claim added
2. `tbb32` overflow/underflow and the min sentinel normalize to `ERR` — initial
   concrete overflow claim added
3. `int32`/`int64` arithmetic stays within two's-complement bounds — initial
   concrete `int32` wrap claim added
4. zero-step loops route to failsafe instead of diverging — concrete claim added
5. reassignment to `fixed` bindings cannot silently mutate state — concrete claim added
6. `?!` on an error `Result` always reaches the failsafe path — concrete claim added
7. zero-argument helper calls preserve `pass`/`fail` Result polarity
8. `println` appends exactly one newline and returns the emitted byte count
9. parameterized helper calls restore caller frames and preserve `pass`/`fail`
   Result polarity
10. struct field writes update only the selected field and preserve unrelated
   fields
11. `pick` dispatch chooses the first matching arm, and `fall label;` reaches
    only the named labeled arm before returning to the pick continuation
12. integer `limit<Rules>` declarations and assignments either satisfy every
    cascaded rule condition or route to `failsafe` without committing the
    violating store update
