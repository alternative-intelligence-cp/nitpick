# Nitpick v0.25.7 — Cycle Audit

**Date:** May 13, 2026
**Branch:** `dev-0.25.x` → merged to `main`
**Tag:** `v0.25.7`
**Auditor:** Copilot / randy

---

## Cycle Summary

The v0.25.x cycle delivered the **Borrow Checker Hardening** subsystem,
turning a pre-cycle baseline of partial path-tracker coverage into a
full per-statement-kind, per-path borrow model with K-formalised core
rules and a 9-chapter borrow cookbook in the user guide.

**8 releases:** v0.25.0 (triage / coverage matrix) through v0.25.7
(K semantics tests + `guide/borrow/` + audit + closeout).

All 14 BORROW-* items planned for the cycle are resolved.

---

## Per-Slice Summary

| Version | Description | CTest |
|---------|-------------|-------|
| v0.25.0 | Statement-visitor audit + coverage matrix; `FAIL`/`TILL` dispatch (BORROW-001) | 47/47 |
| v0.25.1 | `defer` body borrow tracking (BORROW-002); early-exit leak audit (BORROW-003) | 48/48 |
| v0.25.2 | Multi-dim and nested array borrow paths (BORROW-004) | 49/49 |
| v0.25.3 | 3+-level struct field paths and `ptr->field` paths (BORROW-005, BORROW-006) | 50/50 |
| v0.25.4 | Inter-procedural parameter intent + return-borrow lifetime (BORROW-007, BORROW-008) | 51/51 |
| v0.25.5 | Two-phase borrows (BORROW-009); `$$m self` in trait impls (BORROW-010) | 52/52 |
| v0.25.6 | Closure capture borrow tracking (BORROW-011); multi-await polish (BORROW-012); ARIA-023/026 secondary spans (BORROW-013) | 53/53 |
| v0.25.7 | K semantics + 9-chapter `guide/borrow/` + audit + closeout (BORROW-014) | 53/53 |

---

## v0.25.7 Slice Details

### Phase 1 — K Semantics: 3 New Core Tests

Added under `k-semantics/tests/core/`:

- `143_borrow_multilevel_field_disjoint_pass.aria` — three disjoint
  `$$m` borrows on sibling fields of a 3-level nested struct
  (`box.leaf.{x,y,z}`) all coexist. Expected exit `60`.
- `144_borrow_two_phase_release_pass.aria` — combined two-phase pattern:
  `$$i` plain-var borrow released by inner scope, then `$$m` on an
  unrelated struct field. Expected exit `30`.
- `145_borrow_async_release_pass.aria` — repeated `await` on the same
  input value works because each async frame's loans release on
  completion. Expected exit `54`.

K core total: **145/145** (was 142/142 at v0.25.6).

### Phase 2 — K Proofs: Status

The existing 10 proof modules (notably `borrow-path-proofs.k`,
`field-alias-proofs.k`, `pointer-path-proofs.k`) already establish the
core path-disjointness and parent-child conflict theorems that the
v0.25.7 plan called for. No new proof modules were added in this slice;
the 3 new K core tests above pin the three additional concrete
behaviours (multilevel disjoint, two-phase release, async release) under
the existing rule set.

K proofs total: **10/10** (unchanged).

### Phase 3 — `aria-docs/guide/borrow/` Cookbook (9 chapters)

Created under `REPOS/aria-docs/guide/borrow/`:

- `README.md` — overview, conventions, validation snapshot, table of
  contents.
- `basic.md` — the `$$i` / `$$m` operators, the 1-mut XOR N-immut rule,
  scope release, the meaning of "host".
- `paths.md` — sibling fields disjoint, same-path conflicts, 3-level
  nesting, parent-child overlap, sibling assignment.
- `arrays.md` — literal vs dynamic index, `arr[*]` collapse, two
  dynamic borrows on same array, dynamic + literal on same array.
- `structs.md` — whole-struct borrow, struct update syntax, pick
  patterns, `$$m self` methods.
- `callsites.md` — `$$i`/`$$m` parameters, multiple `$$m` parameters,
  writeback, holding a borrow across a call.
- `two_phase.md` — scope release, sequenced loans, combined two-phase
  across hosts, what is *not* supported.
- `async.md` — borrows inside async bodies, repeated awaits on the
  same host, holding a borrow across `await`.
- `diagnostics.md` — `ARIA-023`, `ARIA-026`, `ARIA-022`, `ARIA-027`
  diagnostic cheat sheet with fixes.

Each chapter cross-links to the relevant K test number for executable
backing.

### Phase 4 — Validation Sweep

- **CTest:** 53/53 (release build, K semantics enabled).
- **K core:** 145/145.
- **K proofs:** 10/10.
- **Borrow regression bug suite:** bug039 ... bug204 (105 borrow tests)
  all pass; CMake target `bug_tests_v0256` retained.
- **No regressions** detected against v0.25.6 baseline.

### Phase 5 — Paperwork Closeout

- This audit (`AUDIT_v0.25.7.md`).
- `KNOWN_ISSUES.md` bumped to v0.25.7 banner; bug173–204 listed as
  resolved per slice.
- `META/NITPICK_BORROW/BORROW.md` — all 14 BORROW-* items ✅;
  Resolved 14/14, Open 0/14.
- `META/NITPICK_BORROW/COVERAGE.md` — BORROW-014 row marked ✅.
- `META/NITPICK/ROADMAP/0.25/RELEASE_0.25.7.md` → moved to
  `done/0.25/`, marked COMPLETE.
- `META/NITPICK/ROADMAP/MASTER_ROADMAP.md` → v0.25.x ✅, Next Up = v0.26.x.
- `REPOS/aria/README.md` Current Status block bumped to v0.25.7.

### Phase 6 — Merge / Tag / Push

- Commit on `dev-0.25.x`.
- Tag `v0.25.7`.
- Merge `dev-0.25.x` → `main` (no-ff).
- Push branches + tags to `alternative-intelligence-cp/nitpick`.
- Push `aria-docs` `main`.
- GitHub release `v0.25.7 — Borrow Checker Hardening (cycle close)`.

---

## Path-Tracker Model (as of v0.25.7)

A *loan* records:
- **host** — the variable name, or a `(host, segments)` path pair where
  segments are `field(name)`, `lit(index)`, `dyn`, or `deref`.
- **mode** — `$$i` (immutable, copy-borrow allowed) or `$$m`
  (mutable, exclusive).
- **scope id** — the lexical scope that owns the borrowing binding.
- **source location** — for diagnostics, including the secondary-span
  pointer used by `ARIA-023` / `ARIA-026`.

Disjointness of two paths `P1`, `P2` on the same host:

- Two literal-index paths are disjoint iff their indices differ.
- Two field paths are disjoint iff at the first differing segment both
  are literal `field(name)` segments with different names.
- Any path containing a `dyn` segment is not disjoint from any other
  path with the same prefix on the same host.
- A parent path is never disjoint from any of its descendants.
- Different hosts are always disjoint.

This is intentionally conservative — we reject some safe programs in
favour of a model that is simple to formalise and easy to explain.

---

## Known Limitations (carried into v0.26.x triage)

- **No flow-sensitive borrow release across `if`/`else` arms** —
  releasing in one arm does not free the host in the join.
- **Implicit two-phase borrows are not supported** — methods take
  their receiver loan up front for the whole call.
- **Closure capture for non-primitive types defaults to BY_VALUE** —
  capture-by-reference is supported only for primitive scalars
  (BORROW-011 v0.25.6 scope).
- **IR-gen path-arg lowering for `$$m` parameters** is not yet wired
  through end-to-end; the borrow checker enforces the rules but the
  codegen path uses the existing pointer-passing convention.
- **Cross-module return-borrow inference** is summary-based, not
  global — known-good shapes work; novel shapes may need explicit
  scoping.
- **K model depth for field paths is 3** — the parser supports up to
  3-level field syntax (`a.b.c`); deeper paths are not yet expressible
  in the K model.
- **Out-of-bounds array access** is not borrow-checked; that lives
  in the verification stack (KLEE / CBMC, see
  `guide/verification/klee.md`).

---

## Cycle Validation Snapshot

| Metric | v0.24.7 | v0.25.7 |
|--------|---------|---------|
| CTest | 46/46 | **53/53** |
| K core | 142/142 | **145/145** |
| K proofs | 10/10 | **10/10** |
| Bug regression count | 172 | **204** |
| Borrow-specific bugs | ~75 | **105** |
| Stdlib modules | 72 | 72 |
| Packages | 103 | 103 |
| BORROW-* items resolved | n/a | **14/14** |

---

## Next Cycle

**v0.26.x** — TBD per `META/NITPICK/ROADMAP/0.26/idea.txt`. Planning
documents written alongside this audit; implementation begins next
session.

---

*End of audit.*
