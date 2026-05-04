# AUDIT — Aria v0.18.0 (K Framework Executable Semantics Seed)

**Date:** May 3, 2026  
**Branch:** `dev-0.18.x`  
**Final implementation slice before audit:** `9cb98ed` — `v0.18.0: add symbolic result proof lemmas`  
**Tag:** `v0.18.0` targets the final audit/closeout commit.

---

## Summary

v0.18.0 establishes Aria's first executable K Framework semantics seed: a
formal, runnable oracle for the modeled language subset. This release does not
attempt to model every Aria feature. Instead, it closes a coherent, validated
non-compiler slice covering core execution, safety routing, Result behavior,
structs, rules/limits, memory qualifiers, pointers, pins, borrows, field aliases,
and focused proof modules.

The release is now at the intended non-compiler wall: array/index borrow paths
remain blocked until `ariac` accepts indexed borrow initializers and the K model
grows array/index-path semantics. Larger unchecked roadmap items such as full
modules/FFI semantics, concurrency, richer arrays/rules/floats/integers, and
deeper compiler/K integration are future expansion tracks.

---

## Validation Snapshot

All validation was run locally on May 3, 2026 with K Framework v7.1.320 and
OpenJDK 21.

| Check | Result |
|-------|--------|
| `git diff --check` | PASS |
| `./k-semantics/run_k_tests.sh --require-k` | 105 passed, 0 failed |
| `bash ./k-semantics/run_k_proofs.sh --require-k` | 10 proof modules passed, 0 failed |
| `ctest --test-dir build --output-on-failure` | 8/8 tests passed |

Full CTest result from the final proof-hardening slice:

| Test | Result |
|------|--------|
| `test_runner_minimal` | PASS |
| `integration_tests` | PASS |
| `k_semantics_core` | PASS |
| `k_semantics_proofs` | PASS |
| `gpu_tests` | PASS |
| `gpu_tests_ptx_only` | PASS |
| `module_loading_diamond` | PASS |
| `module_loading_diamond_run` | PASS |

---

## Delivered Artifacts

- `k-semantics/aria.k` — active executable K seed.
- `k-semantics/run_k_tests.sh` — K core test runner with CTest-friendly skip behavior.
- `k-semantics/run_k_proofs.sh` — Haskell-backend `kprove` runner.
- `k-semantics/tests/core/` — 105 core executable-semantics programs.
- `k-semantics/proofs/` — 10 proof modules.
- `k-semantics/STATUS_v0.18.0.md` — detailed status, validation notes, and gaps.
- `k-semantics/README.md` — local K semantics usage notes.
- Root `README.md` — release-facing v0.18.0 status refresh.

---

## Modeled K Seed Scope

The v0.18.0 K seed covers:

- mandatory `func:main` / `func:failsafe` program envelope
- mutable and `fixed` bindings
- `exit`, `if`/`else`, bounded `loop(start, end, step)`, `pick`, and `fall`
- raw integer arithmetic plus typed `int32`, `int64`, and `tbb32` runtime values
- two's-complement `int32` / `int64` wrapping
- `tbb32` min-sentinel and overflow/underflow routing to sticky `ERR`
- sticky `ERR` and `Unknown` propagation in the modeled numeric subset
- `pass`, `fail`, `raw`, `drop`, `defaults`, and `?!` Result operations
- zero-, one-, and two-argument helper calls with isolated call frames
- string literals plus `print` / `println` stdout modeling
- one-, two-, and three-field canonical structs, literals, field reads, and writes
- untyped `Rules:` declarations, integer rule checks, cascaded limits, and
  `limit<Rules>` declaration/assignment enforcement
- failsafe routing for modeled semantic errors
- `stack`, `gc`, `wild`, and `wildx` memory qualifier tracking
- `alloc`, `free`, wild/wildx leak checks, double-free checks, and `defer` cleanup
- local pointer address-of, dereference, and store-through
- pointer-member and nested pointer-member reads/store-through
- pin registration, pin dereference, pinned-host protection, pin-derived pointer
  alias read-only tracking, and by-value pin escape rejection
- `$$i` / `$$m` borrow qualifiers, helper parameter borrow behavior, writeback,
  scope release, direct/two-level path-sensitive field borrows, and local
  mutable field-alias writeback

---

## Proof Corpus

Current proof modules:

| Module | Coverage |
|--------|----------|
| `arithmetic-proofs.k` | Symbolic `int32`, `int64`, `tbb32`, division-by-zero, numeric `ERR` / `Unknown` propagation |
| `result-proofs.k` | Symbolic `pass`, `fail`, `raw`, `drop`, `defaults`, and `?!` routing |
| `core-proofs.k` | Core concrete safety, stdout, helper polarity/frame restoration, and struct-field preservation |
| `field-alias-proofs.k` | Direct/nested mutable field-alias writeback and immutable alias rejection |
| `pin-proofs.k` | Pin registration and read-only mutation/reassignment rejection |
| `pin-by-value-proofs.k` | Pinned-host by-value rejection in calls and terminal exits |
| `pointer-proofs.k` | Local pointer address/deref/store-through and pointer-member behavior |
| `pointer-path-proofs.k` | Nested pointer/pin paths and pin-derived alias read/mutation rejection |
| `borrow-path-proofs.k` | Direct/nested path-sensitive borrow assignment behavior |
| `control-rules-proofs.k` | `pick`/`fall`, labeled-arm routing, and `limit<Rules>` commit/no-commit behavior |

The `control-rules-proofs.k` work exposed a real nondeterministic `fall` lookup
edge. The fix requires labeled-arm skipping to prove the label is nonmatching
(`L =/=K L2`) before skipping the arm.

---

## Known Intentional Gaps

These are not v0.18.0 regressions:

- remaining integer families (`int8`/`int16`, unsigned ints, `tbb8`/`tbb16`/`tbb64`)
- richer float semantics beyond type tokens/simple presence
- stderr / stddbg output cells
- arrays, array/index field paths, generic structs, and legacy struct shorthand
- rich `pick` patterns beyond value equality and `(*)`
- typed `Rules<T>`, non-integer rule values, struct-field rules, arrays, modulo
  expressions, and deeper SMT integration for `limit<Rules>`
- array/index field borrow paths until compiler and K array/index semantics exist
- modules/imports, module dot notation, extern/FFI boundary semantics
- concurrency primitives
- deeper symbolic type/memory/failsafe safety theorems beyond the focused proof corpus

---

## Release Decision

v0.18.0 is complete for the chosen K seed scope. The remaining large items are
future compiler/K expansion tracks rather than reasonable non-compiler slices.
Proceeding to the Nitpick rebrand runway is the recommended next action, while
preserving this tag as the final Aria-named v0.18.0 K semantics milestone unless
Randy decides otherwise.