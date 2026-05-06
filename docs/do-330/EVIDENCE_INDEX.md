# Evidence Index
# Aria Compiler Infrastructure — `npkc`
# DO-330 / EUROCAE ED-215 Compliance Document

**Document ID:** ARIA-EVI-001  
**Version:** 1.0  
**Date:** 2026-05-06  
**Status:** Draft — v0.18.7 Release  
**Tool:** `npkc` — the Aria language compiler  
**Parent:** ARIA-TAS-001 (Accomplishment Summary)

---

## 1. Purpose

This Evidence Index is the master cross-reference linking every DO-330 Annex A
qualification objective to:
- The specific TOR requirement(s) it supports
- The test file(s) that provide evidence
- The commit hash and release tag where evidence was captured
- The verification method employed

Use this index to locate the specific test artifact for any qualification objective.

---

## 2. Index Structure

Each entry follows this format:

> **[DO-330 Ref]** | **[TOR-ID]** | **[TVP-ID]** | **[Test File / Artifact]**  
> Method | Verdict | Commit | Tag | Release

---

## 3. T-0: All TQL Objectives

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T0-1 (requirements documented) | All TOR | — | `docs/do-330/TOR.md` | COMPLETE | v0.18.7 |
| T0-2 (requirements verifiable) | All TOR | All TVP | `docs/do-330/TVP.md` | COMPLETE | v0.18.7 |
| T0-3 (verification planned) | — | — | `docs/do-330/TQP.md` §6 | COMPLETE | v0.18.7 |
| T0-4 (failure behaviour documented) | — | — | `docs/do-330/TQP.md` §9 | COMPLETE | v0.18.7 |

---

## 4. T-1: Operational Requirements Traceability

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T1-1 (TOR → test case) | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_*/good.npk` (18 files) | PASS | v0.18.5 |
| T1-1 | TOR-002 | TVP-CORE-002 | `tests/juliet/cwe_*/bad_compile.npk` (7 files) | PASS | v0.18.5 |
| T1-1 | TOR-011 | TVP-TBB-002 | `tests/sv-comp/no_overflow/unsafe_overflow_intmax.npk` | PASS | v0.18.6 |
| T1-1 | TOR-012 | TVP-TBB-003 | `tests/sv-comp/no_overflow/unsafe_err_propagation.npk` | PASS | v0.18.6 |
| T1-1 | TOR-030 | TVP-MEM-001 | `tests/sv-comp/mem_safety/unsafe_use_after_free.npk` | PASS | v0.18.6 |
| T1-1 | TOR-031 | TVP-MEM-002 | `tests/sv-comp/mem_safety/unsafe_double_free.npk` | PASS | v0.18.6 |
| T1-1 | TOR-060 | TVP-TERM-001 | `tests/sv-comp/termination/terminates_bounded_loop.npk` | PASS | v0.18.6 |
| T1-2 (coverage 100%) | All TOR | All TVP | `docs/do-330/TVP.md` §5 (27/27 PASS) | COMPLETE | v0.18.7 |
| T1-3 (env documented) | — | — | `docs/do-330/TQP.md` §4 | COMPLETE | v0.18.7 |

---

## 5. T-2: Normal and Robustness Tests

### 5.1 Normal Range Tests (T2-1)

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Commit | Tag |
|-----------|--------|--------|----------|---------|--------|-----|
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/sv-comp/reach_safety/safe_counter.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/sv-comp/reach_safety/safe_bounded_loop.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/sv-comp/reach_safety/safe_function_call.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-051 | TVP-OVF-002 | `tests/sv-comp/no_overflow/safe_small_arith.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-051 | TVP-OVF-002 | `tests/sv-comp/no_overflow/safe_loop_sum.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-032 | TVP-MEM-003 | `tests/sv-comp/mem_safety/safe_alloc_free.npk` | Proven=1 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-032 | TVP-MEM-003 | `tests/sv-comp/mem_safety/safe_two_allocs.npk` | Proven=2 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_190/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_369/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_415/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_416/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_457/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_476/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_561/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-001 | TVP-CORE-001 | `tests/juliet/cwe_688/good.npk` | exit 0 | `68a21c1` | — |
| T2-1 | TOR-060 | TVP-TERM-001 | `tests/sv-comp/termination/terminates_bounded_loop.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-060 | TVP-TERM-001 | `tests/sv-comp/termination/terminates_countdown.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-060 | TVP-TERM-001 | `tests/sv-comp/termination/terminates_linear_search.npk` | exit 0 | `82d2c93` | v0.18.6 |
| T2-1 | TOR-061 | TVP-TERM-002 | `tests/sv-comp/termination/terminates_nested_loops.npk` | exit 0 | `82d2c93` | v0.18.6 |

### 5.2 Robustness / Error Tests (T2-2)

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Commit | Tag |
|-----------|--------|--------|----------|---------|--------|-----|
| T2-2 | TOR-011 | TVP-TBB-002 | `tests/sv-comp/no_overflow/unsafe_overflow_intmax.npk` | exit 1 (ERR) | `82d2c93` | v0.18.6 |
| T2-2 | TOR-012 | TVP-TBB-003 | `tests/sv-comp/no_overflow/unsafe_err_propagation.npk` | exit 1 (ERR sticky) | `82d2c93` | v0.18.6 |
| T2-2 | TOR-030 | TVP-MEM-001 | `tests/sv-comp/mem_safety/unsafe_use_after_free.npk` | compile_reject | `82d2c93` | v0.18.6 |
| T2-2 | TOR-031 | TVP-MEM-002 | `tests/sv-comp/mem_safety/unsafe_double_free.npk` | compile_reject | `82d2c93` | v0.18.6 |
| T2-2 | TOR-040 | TVP-CONTRACT-001 | `tests/sv-comp/reach_safety/unsafe_div_noguard.npk` | Disproven=1 | `82d2c93` | v0.18.6 |
| T2-2 | TOR-042 | TVP-CONTRACT-003 | `tests/sv-comp/reach_safety/unsafe_contract_chain.npk` | Proven=3, Disproven=1 | `82d2c93` | v0.18.6 |
| T2-2 | TOR-011 | TVP-TBB-002 | `tests/juliet/cwe_190/bad.npk` | exit non-zero | `68a21c1` | — |
| T2-2 | TOR-013 | TVP-TBB-004 | `tests/juliet/cwe_369/bad.npk` | exit non-zero | `68a21c1` | — |
| T2-2 | TOR-031 | TVP-MEM-002 | `tests/juliet/cwe_415/bad_compile.npk` | compile_reject | `68a21c1` | — |
| T2-2 | TOR-030 | TVP-MEM-001 | `tests/juliet/cwe_416/bad_compile.npk` | compile_reject | `68a21c1` | — |
| T2-2 | TOR-002 | TVP-CORE-002 | `tests/juliet/cwe_457/bad_compile.npk` | compile_reject | `68a21c1` | — |
| T2-2 | TOR-002 | TVP-CORE-002 | `tests/juliet/cwe_476/bad_compile.npk` | compile_reject | `68a21c1` | — |
| T2-2 | TOR-002 | TVP-CORE-002 | `tests/juliet/cwe_561/bad_compile.npk` | compile_reject | `68a21c1` | — |
| T2-2 | TOR-002 | TVP-CORE-002 | `tests/juliet/cwe_688/bad_compile.npk` | compile_reject | `68a21c1` | — |

---

## 6. T-3: Test Review Objectives

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T3-1 (results reviewed) | All TOR | All TVP | `docs/do-330/TAS.md` §3, §5 | COMPLETE | v0.18.7 |

---

## 7. T-6: COTS Qualification Objectives

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T6-1 (COTS identified) | — | — | `docs/do-330/TQP.md` §8 | COMPLETE | v0.18.7 |
| T6-2 (COTS strategy: LLVM) | TOR-001 | TVP-CORE-001 | `tests/alive2/` (Alive2 IR validation) | PASS | v0.18.2 |
| T6-2 (COTS strategy: Z3) | TOR-032,040,051 | TVP-MEM-003, TVP-CONTRACT-001, TVP-OVF-002 | Proof certificate architecture | COMPLETE | v0.18.7 |

---

## 8. T-7: Verification Independence Objectives

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T7-1 (traceability) | All TOR | All TVP | `docs/do-330/TOR.md` §11 | COMPLETE | v0.18.7 |
| T7-2 (independence: K Framework) | TOR-010 to TOR-061 | TVP-TBB-*, TVP-TERM-* | `tests/k_semantics/` (68/68) | PASS | v0.18.0 |
| T7-2 (independence: Alive2) | TOR-001 | TVP-CORE-001 | `tests/alive2/` | PASS | v0.18.2 |
| T7-2 (independence: IKOS) | TOR-030 to TOR-033 | TVP-MEM-* | `tests/static_analysis/` | PASS | v0.18.3 |
| T7-2 (independence: KLEE) | TOR-011 to TOR-013, TOR-030 to TOR-031 | TVP-TBB-*, TVP-MEM-* | `tests/klee/` (9/9) | PASS | v0.18.4 |

---

## 9. T-8: Structural Coverage

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Coverage | Release |
|-----------|--------|--------|----------|----------|---------|
| T8-1 (statement) | TOR-001 to TOR-072 | All TVP | Juliet 18/18 + SV-COMP 16/16 | All program statements exercised | v0.18.5, v0.18.6 |
| T8-2 (decision) | TOR-010 to TOR-014 | TVP-TBB-* | K Framework rules 68/68 | All TBB decision branches proven | v0.18.0 |
| T8-2 (decision) | TOR-060, TOR-061 | TVP-TERM-* | K loop termination lemma | All loop exit conditions proven | v0.18.0 |

---

## 10. T-9: Supplementary Coverage

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Coverage | Release |
|-----------|--------|--------|----------|----------|---------|
| T9-1 (symbolic) | TOR-011 to TOR-013, TOR-030 to TOR-031 | TVP-TBB-*, TVP-MEM-* | `tests/klee/` (9/9 symbolic paths) | Exhaustive symbolic path coverage | v0.18.4 |
| T9-2 (formal) | TOR-010 to TOR-043 | TVP-TBB-*, TVP-FAIL-*, TVP-MEM-*, TVP-CONTRACT-* | K proofs 68/68 + Z3 6 programs | All properties formally verified | v0.18.0, v0.18.6 |

---

## 11. T-10: Accomplishment Summary Objectives

| DO-330 Ref | TOR-ID | TVP-ID | Artifact | Verdict | Release |
|-----------|--------|--------|----------|---------|---------|
| T10-1 (TAS produced) | — | — | `docs/do-330/TAS.md` | COMPLETE | v0.18.7 |
| T10-2 (all activities complete) | — | — | `docs/do-330/TAS.md` §3, §5 | 27/27 PASS | v0.18.7 |
| T10-3 (open items) | — | — | `docs/do-330/TAS.md` §6 | Documented | v0.18.7 |
| T10-4 (qualification declared) | — | — | `docs/do-330/TAS.md` §7 | DECLARED | v0.18.7 |

---

## 12. DO-333 Formal Methods Evidence Index

| DO-333 Ref | Formal Method | TOR-ID | Artifact | Verdict | Release |
|-----------|---------------|--------|----------|---------|---------|
| FM.A-5 FM1 (src complies with req) | K Framework | TOR-010 to TOR-043 | `tests/k_semantics/` 68/68 | PASS | v0.18.0 |
| FM.A-5 FM2 (accurate/consistent) | K Framework | All | `docs/k_semantics/aria.k` (no conflicts) | PASS | v0.18.0 |
| FM.A-5 FM3 (supports verification) | K Framework | TOR-040 to TOR-043 | K contract semantics (kprove 1/1) | PASS | v0.18.0 |
| FM.A-5 FM6 (no unintended functions) | Z3 backend | TOR-030, TOR-040, TOR-050 | Z3 reports (all 6 FALSE programs) | PASS | v0.18.6 |
| FM.A-7 (property preservation) | Alive2 | TOR-001 | `tests/alive2/` IR validation | PASS | v0.18.2 |

---

## 13. Complete Release-to-Evidence Map

| Release | Commit | Tag | CTest Label | Count | DO-330 Tables |
|---------|--------|-----|-------------|-------|---------------|
| v0.18.0 | `0f76874` | `v0.18.0` | `k_semantics` | 68/68 + 1/1 kprove | T-1, T-3, T-7, T-8, T-9 |
| v0.18.1 | `9292af5` | — | `emi_tests` | 7/7 | T-2 |
| v0.18.2 | `2840a09` | — | — | All Alive2 | T-6, T-7 |
| v0.18.3 | `2840a09` | — | — | IKOS + ESBMC + Frama-C | T-7 |
| v0.18.4 | `b5789af` | — | `klee_symbolic` | 9/9 | T-9 |
| v0.18.5 | `68a21c1` | — | `juliet_cwe_tests` | 18/18 | T-2, T-8 |
| v0.18.6 | `82d2c93` | `v0.18.6` | `svcomp_benchmarks` | 16/16 (+26) | T-2, T-8, T-9, T-10 |

---

*ARIA-EVI-001 — Aria DO-330 Evidence Index*  
*Released as part of Aria v0.18.7*
