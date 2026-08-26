# Cycle 1.4 — Verification

**Phase C.** `prove`, `limit<Rules>`, contracts (`requires`/`ensures`), Z3 over
SMT-LIB2, and NIKOS (or its deferral). This is the cycle that makes Layer 1 real —
the mathematical prevention of invalid states that is the whole first line of the
safety architecture.

> Detailed **map**. The audit's verdict: this is **the least-built major subsystem**,
> and the specs "answer *what* but almost never *how*." Its subcycles cannot be
> written in full until five decisions settle — but the decisions and the subcycle
> shape are captured now so the cycle is bounded work, not open research.

## The state this cycle starts from (the audit's finding)

The **surface is built**: the grammar, AST, and name-resolution for contracts,
`limit`, invariants, and `Rules` bodies all parse and bind and are grammar-tested.
Everything from **typing through Z3 is not** — and one piece is worse than unbuilt:
the backend currently **drops `limit`/contracts/invariants silently** (LIVE-1, which
0.9.0 converts to an honest rung refusal). So 1.4 turns those refusals into real
checks, and it does so on top of a surface that was left un-type-checked (a
`requires 5i32` passes today; rule names in `limit<R>` are never resolved).

## Decisions in (five, before the cycle starts — see `../OPEN_DECISIONS.md` §4)

- **C-17 — the SMT emitter + invocation architecture.** *Blocks the cycle's start.*
  Theory choices, the obligation catalogue matching the manifest's `kind` column, the
  counterexample→span contract, and **the process-spawn primitive** to invoke z3
  (which the language lacks and `npkg` — built in 1.3 — provides). The cycle's first
  act.
- **C-14 — elision ownership** (`--verify` vs `--smt-opt`; both can't own it without
  reintroducing D-039's non-determinism for the artifact Astrée reads).
- **C-15 — limit-check placement/typing/subsumption** (+ close the frontend holes:
  resolve rule names, type `Rules` bodies).
- **C-16 — contract runtime semantics under universal `Result`** (violation channel,
  error codes, `result`'s type, `old()`, and **implement D-014's injected `ensures
  result > 0` on `failsafe`** — currently nowhere).
- **B-5 — NIKOS: specify or defer** (a named deliverable with zero spec).

## Subcycle shape (to be filled when reached)

| # | Topic | Gated on |
|---|---|---|
| 1.4.0 | **The SMT emitter + z3 invocation** — AST→SMT-LIB2 per the theory choices; z3 as a spawned tool (over 1.3's spawn primitive); model→span mapping; the manifest (D-040) | C-17 |
| 1.4.1 | **Type the verification surface** — resolve `limit<R>` rule names, type `Rules` bodies (`$` typed, clauses `bool`), type contract conditions (`requires`/`result`) | C-15, C-16 |
| 1.4.2 | **`limit<Rules>`** — static discharge via z3, runtime-check injection where undischarged (C-15), subsumption (Rules composition); replaces 0.9.0's rung refusal | C-14, C-15 |
| 1.4.3 | **Contracts** — `requires`/`ensures` static + runtime; the D-014 `failsafe` injection + non-empty-body check (C-16); replaces 0.9.0's rung refusal | C-16 |
| 1.4.4 | **`prove` / `assert_static`** — z3 obligations with path-condition accumulation; `assert_static` folds at the frontend (the comptime evaluator exists, 0.6); the `prove`-without-`--verify` behavior (verification F6) | C-14 |
| 1.4.5 | **The aliasing/disjointness analysis §2.1 presupposes** — the conservative `$$m`/`$$i` refusal that z3 then relaxes (the 0.5 analyses don't contain it; 1.4 must create the error it suppresses) | — |
| 1.4.6 | **NIKOS** — per B-5, either the reference + implementation, or the clean deferral | B-5 |

## Watch for

- **The Astrée gate (C-19) must be answered before this cycle exits**, not at 1.5 —
  because if AbsInt confirms Astrée reads C (not LLVM IR / not monomorphized output),
  a C-emission path becomes Phase-C work, and discovering that at the start of a
  non-renewable 30-day trial is the worst possible timing. Promote it to a 1.4-exit
  gate (it is in `../1.5/README.md` and `OPEN_DECISIONS` C-19).
- **Verification's obligations were carried from every cycle** — 0.9.0's rung
  refusals, D-014's failsafe contract, the counted-loop `step > 0`, cast-range,
  bounds, overflow. 1.4 is where the tooling is wired, not where correctness starts;
  the obligation catalogue (C-17) must enumerate all of them or the manifest's `kind`
  column has gaps.
- **z3 is invoked, never linked** (D-039/D-067) — and its version is recorded in the
  manifest but the non-determinism it can introduce (timeout-dependent binaries) is
  exactly what C-14 must contain. A verification pass that changes the artifact by
  the solver's mood is the one thing this cycle must not ship.
