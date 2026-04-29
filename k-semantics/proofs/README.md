# K Proofs

Proof artifacts will live here once the executable semantics seed compiles under
K and the first `kprove` claims are added.

Planned initial claims:

1. sticky `ERR` arithmetic remains sticky
2. `tbb32` overflow/underflow and the min sentinel normalize to `ERR`
3. `int32`/`int64` arithmetic stays within two's-complement bounds
4. zero-step loops route to failsafe instead of diverging
5. reassignment to `fixed` bindings cannot silently mutate state
6. `?!` on an error `Result` always reaches the failsafe path
7. zero-argument helper calls preserve `pass`/`fail` Result polarity
8. `println` appends exactly one newline and returns the emitted byte count
9. parameterized helper calls restore caller frames and preserve `pass`/`fail`
   Result polarity
10. struct field writes update only the selected field and preserve unrelated
   fields
11. `pick` dispatch chooses the first matching arm, and `fall label;` reaches
    only the named labeled arm before returning to the pick continuation