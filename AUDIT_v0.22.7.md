# Nitpick v0.22.7 — Cycle Audit

**Date:** May 10, 2026  
**Branch:** `dev-0.22.x` → merged to `main`  
**Tag:** `v0.22.7`  
**Auditor:** Copilot / randy

---

## Cycle Summary

The v0.22.x cycle was port-driven polish: three real-world Nitpick ports
(chip8, jsmn, brainfuck) exposed 14 friction points (POLISH-001–014). This
cycle resolved 13 of them; POLISH-005 is deferred.

**8 releases:** v0.22.0 (triage) through v0.22.7 (closeout)

---

## Per-Slice Summary

| Version | Commit | Description | CTest |
|---------|--------|-------------|-------|
| v0.22.0 | `9ee2f42` | Triage — 14 POLISH entries filed, priority ordered | 25/25 |
| v0.22.1 | `8d6c5e6` | POLISH-012/014 unused-var; POLISH-013 FFI docs | 25/25 |
| v0.22.2 | `acb81ab` | POLISH-001/002/010 eprint, .aria ext, struct type id | 25/25 |
| v0.22.3 | `3b0ba4d` | POLISH-003/011 get_argc/argv builtins, break/continue | 25/25 |
| v0.22.4 | `c3530f9` | POLISH-006/007 pick on integers, bitwise ops (tests only) | 26/26 |
| v0.22.5 | `2933768` | POLISH-009 `\xNN` + `\u{NNNN}` escape sequences | 27/27 |
| v0.22.6 | `f6973e0` | POLISH-008/004 reserved-keyword diagnostics, file ext docs | 27/27 |
| v0.22.7 | (this)   | `isValidSourceFile` rename, audit, docs, closeout | 30/30 |

---

## POLISH.md Statistics

- **Started:** 14 entries (POLISH-001 through POLISH-014)
- **Resolved this cycle:** 13 entries
- **Deferred:** 1 entry (POLISH-005 — heavy C-shim dependency)
- **POLISH-005 rationale:** Eliminating the C-shim layer requires a Nitpick
  stdlib with the same coverage as the current `aria_rt.c` + per-port shims.
  This is a multi-version effort planned for v0.59.x (stdlib foundation) and
  v0.61.x (self-hosting runtime). Deferring avoids blocking the port work.

---

## Per-Port Summary

All three ports that drove this cycle were re-validated against the final
v0.22.7 compiler binary.

| Port | Language | Tests | Result |
|------|----------|-------|--------|
| chip8 | Nitpick | 6/6 | PASS |
| jsmn | Nitpick | 5/5 | PASS |
| brainfuck | Nitpick | 6/6 | PASS |

---

## Test Counts

| Suite | Before Cycle (v0.21.6) | After Cycle (v0.22.7) |
|-------|------------------------|----------------------|
| CTest (non-K) | 25 | 27 |
| CTest (full incl. K) | 28 | 30 |
| K core | 127 | 139 |
| K proofs | 10 | 10 |
| bug<NNN> tests | ~104 | 120 |

---

## Sign-off Checklist

- [x] CTest all pass — 30/30
- [x] K core count at or above v0.21.6 baseline — 139/139 (was 127)
- [x] K proofs count at or above baseline — 10/10
- [x] No regressions on any prior `bug<NNN>` test
- [x] No new ICEs reported
- [x] Docs updated — `use_import.md`, `ffi_advanced.md`, `README.md` (v0.22.7)
- [x] All three ports build and run against new compiler
- [x] POLISH.md: 13/14 resolved; 1/14 deferred (POLISH-005)
- [x] `isValidAriaFile()` renamed to `isValidSourceFile()`, KNOWN_ISSUES entry removed
- [x] KNOWN_ISSUES.md header updated to v0.22.7
- [x] SEMANTIC_GAPS.md updated to v0.22.7
- [x] `dev-0.22.x` merged to `main`, tagged `v0.22.7`

---

## Notable Changes (for release notes)

1. **`\xNN` / `\u{NNNN}` escape sequences** — now supported in all string
   literals and template literals (`\u{1F600}` → 😀)
2. **Reserved-keyword diagnostics** — using `ok`, `end`, `fail`, `raw`, etc.
   as a variable name now gives a friendly error with rename suggestions
3. **`eprint`/`eprintln`** builtins wired end-to-end (type checker + codegen)
4. **`.aria` extension** accepted by module resolver as legacy alias for `.npk`
5. **`get_argc`/`get_argv`** builtins replace the broken `npk_arg(i)` ABI
6. **`break`/`continue`** confirmed working end-to-end in all loop types
7. **Unused-variable checker** now scans `while` bodies and treats `pass n;`
   as a use
8. **`isValidSourceFile()`** — internal rename completing the Nitpick rebrand

---

*Audit complete. Ready for v0.23.x planning.*
