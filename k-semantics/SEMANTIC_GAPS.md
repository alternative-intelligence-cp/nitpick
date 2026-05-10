# Nitpick K Semantics — Semantic Gaps

**Last updated:** v0.22.7 (May 2026)

This file tracks language features and type-system constructs that are **not yet
formalized** in `k-semantics/aria.k`. Items are removed here when they graduate
to full K coverage.

---

## Type Lattice Gaps (A-009)

As of v0.21.6 the following types have been added to the K model:

| Type | K status |
|------|----------|
| `int8` / `int32` / `int64` | ✅ Full arithmetic |
| `tbb8` / `tbb16` / `tbb32` / `tbb64` | ✅ Full arithmetic + ERR sentinel |
| `flt32` / `flt64` | Syntax only — no float arithmetic rules |
| `string` | ✅ Store/normalize only; no string-op rules |

### Still absent from K

| Type | Priority | Notes |
|------|----------|-------|
| `int128` / `uint128` | High | Used in compiler; LBIM struct representation |
| `int256–int4096` / `uint256–uint4096` | Low | Rarely used in Nitpick programs |
| `int1` / `int2` / `int4` | Low | Bit-field primitives |
| `flt128` / `flt256` / `flt512` | Low | Extended-precision floats |
| `fix256` | Low | Fixed-point |
| Float arithmetic rules (`flt32` / `flt64`) | Medium | All float ops missing |

**Recommended next target:** `int128` / `uint128` (v0.22.x type lattice work).

---

## Feature Gaps (A-010)

### Added in v0.21.6 (Synchronous / Stub Model)

| Feature | K status | Notes |
|---------|----------|-------|
| `async func:` | ✅ Syntax + load rules | Treated as sync; no coroutine frame |
| `await` | ✅ Reduction rule | Identity on a fully-evaluated value |

### Still absent or stub-only

| Feature | Priority | Notes |
|---------|----------|-------|
| True coroutine frame semantics | High | Suspension / resume state machine |
| `async` borrow lifetime extension | High | A-003; borrow crosses `await` point |
| `defer` with ERR-path ordering | Medium | LIFO guaranteed; K only covers happy-path |
| Macro / preprocessor expansion | Low | `%define`, `%ifdef`, `derive(...)` |
| Generics / parametric types | Medium | Type-variable substitution not modeled |
| Trait / impl dispatch | Medium | Dynamic dispatch semantics absent |
| Six-stream lowering | Low | Hardware stream model; very compiler-internal |
| `prove` / `assert_static` semantics | Low | SMT interaction hard to model in K |
| Syscall tier semantics | Low | OS-call semantics outside K scope |
| Closures (> 2 params) | Medium | Current K supports 1- and 2-param lambdas |

---

## Verification Tool Gaps

| Tool | Gap | Priority |
|------|-----|----------|
| KLEE | Suite last updated v0.18.4; closures, `optional<T>`, vec9 not covered | Medium |
| Alive2 | Not integrated; IR optimization correctness for Nitpick passes unverified | Low |

---

## Prioritization for v0.22.x+

1. `int128`/`uint128` K arithmetic (type lattice)
2. True async frame semantics (coroutine suspension/resume)
3. Closure n-param generalization (> 2 params)
4. Trait dispatch modeling
5. KLEE suite expansion (closures, optional<T>)
