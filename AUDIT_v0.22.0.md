# Nitpick v0.22.0 — POLISH Triage Audit

**Date:** 2026-05-09
**Branch:** `dev-0.22.x` (from `main` @ `v0.21.6` / `4f916ee`)
**Baseline:** CTest 24/24, K core 127/127, K proofs 10/10, compiler tests 831+
**POLISH.md source:** `META/NITPICK_PORTS/POLISH.md`

---

## Ports Completed Before This Cycle

| Port | Tag | POLISH entries |
|------|-----|----------------|
| chip8 | chip8-v0.0.8 | POLISH-001 – POLISH-010 |
| jsmn | jsmn-v0.1.4 | POLISH-011 – POLISH-013 |
| brainfuck | bf-v0.2.0 | POLISH-014 |

Total POLISH entries at triage: **14** (001–014)

---

## Severity & Component Breakdown

| POLISH | Title | Severity | Component | Target |
|--------|-------|----------|-----------|--------|
| 001 | eprint/eprintln builtins missing | Missing | sema/stdlib | v0.22.2 |
| 002 | .aria extension rejected by module resolver | Bug | sema/module | v0.22.2 |
| 003 | npk_arg(i) broken ABI (returns pointer not value) | Bug | runtime/ABI | v0.22.3 |
| 004 | File extension conventions undocumented | Docs | docs | v0.22.6 |
| 005 | Heavy C-shim dependency / no Nitpick stdlib | Missing | stdlib/runtime | DEFERRED → v0.59.x + v0.61.x |
| 006 | pick on integer values untested | Docs/Missing | sema | v0.22.4 |
| 007 | Bitwise ops on int32 variables untested | Bug/Missing | irgen | v0.22.4 |
| 008 | Reserved keyword as var name → cryptic error | Bug/UX | parser/diag | v0.22.6 |
| 009 | \xNN hex escape not supported in string literals | Missing | lexer | v0.22.5 |
| 010 | "type X but expects X" across multi-module imports | Bug | sema | v0.22.2 |
| 011 | break/continue not implemented in compiler | Missing | parser/irgen | v0.22.3 |
| 012 | pass n; not counted as variable use | Bug | sema/diag | v0.22.1 |
| 013 | print() vs C stdio buffering interop gotcha | Polish | docs/runtime | v0.22.1 |
| 014 | while body not scanned by unused-var checker | Bug | sema/diag | v0.22.1 |

---

## Slice Assignment

### v0.22.1 — Diagnostic/Warning Polish
- POLISH-012: `pass n;` not counted as variable use → false unused-var warning
- POLISH-014: `while` body not scanned by unused-var checker (same root cause as 012)
- POLISH-013: `print()` vs C stdio buffering — document FFI interop gotcha in porting guide

### v0.22.2 — Sema / Type Fixes
- POLISH-001: `eprint`/`eprintln` not wired in sema/codegen
- POLISH-002: `.aria` extension rejected by module resolver
- POLISH-010: "type X but expects X" across multi-module struct imports

### v0.22.3 — Runtime/ABI & Loop Control
- POLISH-003: `npk_arg(i)` broken ABI (returns `AriaString*` by pointer)
- POLISH-011: `break`/`continue` in spec but not implemented

### v0.22.4 — Language Feature Testing & Fixes
- POLISH-006: `pick` on integer values — test int32/uint32/int64 dispatch, fix if needed
- POLISH-007: Bitwise ops on int32 variables — focused irgen test, fix if broken

### v0.22.5 — Lexer Additions
- POLISH-009: `\xNN` hex escape and `\u{NNNN}` unicode escape in string literals

### v0.22.6 — Diagnostics UX & Docs
- POLISH-004: File extension conventions (.aria vs .npk) — add to language guide
- POLISH-008: Reserved keyword used as var name → cryptic parse error; improve message

### Deferred
- POLISH-005: C-shim dependency / no Nitpick-native stdlib
  Rationale: Requires full allocator redesign + npklibc overhaul. Too large for v0.22.x.
  Target: v0.59.x (Nitpick-native memory functions) + v0.61.x (npklibc improvements)

---

## Design Notes

### POLISH-011 (break/continue) — not a trivial addition
`break` and `continue` are in the spec keyword list (specs_list.txt) and are
therefore a **promised feature**, not a new design decision. The compiler just
hasn't wired them. However, the implementation has meaningful scope:
- Parser must recognize the keyword in loop body position
- IR generator must emit an unconditional branch to the loop exit (break) or
  loop header (continue)
- All four loop forms must be covered: `while`, `loop`, `till`, `for`
- `when/then/end`: `break` must route to the `end` block (per spec semantics)
- Borrow checker: borrows held across a `break` must be released
- K semantics: new rules needed for `break`/`continue` in all loop forms

Given this scope, v0.22.3 may split into v0.22.3a (runtime ABI / POLISH-003)
and v0.22.3b (break/continue / POLISH-011) if needed. The slice plan handles
both together for now; re-evaluate after POLISH-003 lands.

### POLISH-010 (type identity across module imports) — root cause TBD
The symptom is clear but the fix depends on where struct types are interned.
If types are interned per-file (common in simple resolvers), the fix is to
deduplicate by canonical path before type-checking. This may touch the module
resolver, the type table, or both. Small RFC embedded in RELEASE_0.22.2.md.

### POLISH-007 (bitwise ops) — test-first approach
Do NOT assume it's broken. Write the test first. If it passes, POLISH-007 is
resolved as "no fix needed, tests added". If it fails, fix it. Either way the
test is a regression guard.

---

## Acceptance Criteria for v0.22.0

- [x] `AUDIT_v0.22.0.md` written (this file)
- [x] `dev-0.22.x` branch created from `main` @ `v0.21.6`
- [x] Every POLISH-001–014 assigned to a slice or explicitly deferred
- [x] Slice plans RELEASE_0.22.1 through RELEASE_0.22.6 updated with actual POLISH lists
- [x] POLISH.md: all 14 entries moved to "Triaged" section
- [ ] Commit and tag `v0.22.0`
- [ ] MASTER_ROADMAP.md updated
