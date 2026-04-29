# K Proofs

Proof artifacts will live here once the executable semantics seed compiles under
K and the first `kprove` claims are added.

Planned initial claims:

1. sticky `ERR` arithmetic remains sticky
2. zero-step loops route to failsafe instead of diverging
3. reassignment to `fixed` bindings cannot silently mutate state
4. `?!` on an error `Result` always reaches the failsafe path