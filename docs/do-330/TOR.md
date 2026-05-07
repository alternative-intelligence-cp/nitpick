# Tool Operational Requirements (TOR)
# Aria Compiler Infrastructure — `npkc`
# DO-330 / EUROCAE ED-215 Compliance Document

**Document ID:** ARIA-TOR-001  
**Version:** 1.0  
**Date:** 2026-05-06  
**Status:** Draft — v0.18.7 Release  
**Tool:** `npkc` — the Aria language compiler  
**Parent:** ARIA-TQP-001 (Tool Qualification Plan)

---

## 1. Purpose

This document defines the Tool Operational Requirements (TOR) for the `npkc`
Aria compiler. Each requirement is:
- Uniquely identified (TOR-NNN)
- Verifiable by test or formal proof
- Traceable to the Aria language specification and safety guarantees
- Mapped to verification evidence in TVP.md

---

## 2. Requirement Format

Each requirement follows this structure:

> **TOR-NNN:** [Title]  
> *Condition:* [When this requirement applies]  
> *Requirement:* [What the compiler must do]  
> *Rationale:* [Why this matters for safety]  
> *Verification:* [How it is verified — reference to TVP.md]

---

## 3. Core Compilation Requirements

### TOR-001: Valid Program Acceptance
*Condition:* Input `.npk` file is syntactically and semantically valid Aria.  
*Requirement:* `npkc` shall compile the program without error, producing a
  runnable native binary that faithfully implements the source program semantics.  
*Rationale:* Correct programs must compile correctly — missed compilation breaks
  developer trust and safety guarantees.  
*Verification:* TVP-CORE-001 — positive compilation tests across all language features.

### TOR-002: Invalid Program Rejection
*Condition:* Input `.npk` file contains a syntactic or semantic error.  
*Requirement:* `npkc` shall reject the program with a non-zero exit code and
  emit at least one diagnostic message identifying the error location (file, line,
  column) and an ARIA error code (ARIA-NNN).  
*Rationale:* Silent acceptance of invalid programs is a critical safety failure;
  every detected error must be surfaced to the developer.  
*Verification:* TVP-CORE-002 — negative compilation tests, ARIA error code matrix.

### TOR-003: Deterministic Output
*Condition:* Same source file, same compiler flags, same host environment.  
*Requirement:* `npkc` shall produce bit-identical output on repeated invocations.  
*Rationale:* Non-deterministic output prevents reproducible qualification evidence.  
*Verification:* TVP-CORE-003 — repeated compilation with binary diff.

### TOR-004: Flag Isolation
*Condition:* A compiler flag is not present on the command line.  
*Requirement:* `npkc` shall not perform any processing associated with that flag.  
  In particular: `--verify*` flags shall not affect compilation in their absence.  
*Rationale:* Unexpected behaviour changes from absent flags are a Criterion 1
  reliability failure.  
*Verification:* TVP-CORE-004 — output comparison with and without each flag.

---

## 4. TBB Type Requirements (Twisted Balanced Binary)

### TOR-010: TBB32 ERR Sentinel Reservation
*Condition:* `tbb32` type is used in any arithmetic context.  
*Requirement:* `npkc` shall reserve `INT32_MIN` (−2,147,483,648) as the ERR
  sentinel value for `tbb32`. No valid computation shall produce or accept this
  value as a numeric result.  
*Rationale:* ERR sentinel is the foundation of Aria's silent-failure prevention.  
*Verification:* TVP-TBB-001 — unit tests confirming INT32_MIN is never a valid
  arithmetic result.

### TOR-011: TBB32 Overflow to ERR
*Condition:* `tbb32` arithmetic operation would produce a result outside
  [−2,147,483,647, +2,147,483,647].  
*Requirement:* `npkc` shall emit code that transitions the result to ERR rather
  than silently wrapping or saturating.  
*Rationale:* Silent overflow is a class of undefined behaviour that DO-178C
  explicitly requires to be detected.  
*Verification:* TVP-TBB-002 — `unsafe_overflow_intmax.npk` (SV-COMP), Juliet CWE-190.

### TOR-012: TBB32 ERR Stickiness
*Condition:* An ERR-valued `tbb32` is used as an operand in any arithmetic operation.  
*Requirement:* `npkc` shall emit code such that any arithmetic operation with an
  ERR operand produces an ERR result. Specifically: ERR + x = ERR, ERR × 0 = ERR,
  ERR − ERR = ERR.  
*Rationale:* ERR stickiness ensures overflow cannot be silently masked by
  subsequent operations.  
*Verification:* TVP-TBB-003 — `unsafe_err_propagation.npk` (SV-COMP), Juliet CWE-190.

### TOR-013: TBB32 Division by Zero to ERR
*Condition:* `tbb32` division where the divisor is zero at runtime.  
*Requirement:* `npkc` shall emit code that transitions the result to ERR on
  division by zero rather than trapping with SIGFPE.  
*Rationale:* Uncontrolled SIGFPE violates the requirement for graceful degradation
  to failsafe; ERR stickiness must handle this case.  
*Verification:* TVP-TBB-004 — division-by-zero unit test, Juliet CWE-369.

### TOR-014: `pick()` Exhaustiveness
*Condition:* A `pick()` expression is used on a `tbb32` value.  
*Requirement:* `npkc` shall reject any `pick()` that does not cover both the ERR
  arm and a range arm covering all non-ERR tbb32 values.  
*Rationale:* Incomplete `pick()` leaves ERR states unhandled, allowing
  error propagation to escape the intended safety boundary.  
*Verification:* TVP-TBB-005 — negative tests for incomplete `pick()` arms.

---

## 5. Failsafe Requirements

### TOR-020: Failsafe Presence Mandate
*Condition:* Any `.npk` file containing a `func:main`.  
*Requirement:* `npkc` shall reject compilation if the program does not define
  `func:failsafe = int32(tbb32:err)`.  
*Rationale:* `failsafe` is the mandatory terminal safety net; its absence means
  no graceful degradation path exists.  
*Verification:* TVP-FAIL-001 — ARIA-008 error on programs missing failsafe.

### TOR-021: Failsafe Exit-Only Constraint
*Condition:* A `pass` or `fail` operator is used inside `func:failsafe`.  
*Requirement:* `npkc` shall reject compilation with ARIA-009. The `failsafe`
  function body must terminate via `exit(code)`.  
*Rationale:* `failsafe` must not resume normal execution; `pass`/`fail` imply
  continuation, which is semantically invalid in a terminal safety handler.  
*Verification:* TVP-FAIL-002 — ARIA-009 error test.

### TOR-022: Failsafe Unreachable Error Routing
*Condition:* An unhandled runtime error occurs (ERR value reaches program
  boundary, explicit `!!!` operator, etc.).  
*Requirement:* `npkc` shall emit code that routes unhandled errors to
  `func:failsafe` without calling any user-defined function between the error
  site and the failsafe invocation.  
*Rationale:* Intermediate function calls after an unhandled error may themselves
  fail, preventing the failsafe from ever being reached.  
*Verification:* TVP-FAIL-003 — runtime error routing test.

---

## 6. Memory Safety Requirements

### TOR-030: Use-After-Free Rejection (ARIA-022)
*Condition:* A `.npk` program accesses a pointer variable after it has been
  passed to `free()`.  
*Requirement:* `npkc` shall reject compilation with ARIA-022, identifying the
  file, line, and variable name involved.  
*Rationale:* UAF is a critical memory safety violation; compile-time prevention
  eliminates the entire vulnerability class structurally.  
*Verification:* TVP-MEM-001 — `unsafe_use_after_free.npk` (SV-COMP), Juliet CWE-416.

### TOR-031: Double-Free Rejection (ARIA-022)
*Condition:* A `.npk` program calls `free()` on a pointer variable that has
  already been freed.  
*Requirement:* `npkc` shall reject compilation with ARIA-022, identifying the
  file, line, and variable name involved.  
*Rationale:* Double-free is a critical heap corruption vulnerability; compile-time
  prevention is structurally sound.  
*Verification:* TVP-MEM-002 — `unsafe_double_free.npk` (SV-COMP), Juliet CWE-415.

### TOR-032: Z3 UAF Proof (--verify-memory)
*Condition:* `--verify-memory --verify-report` flags are supplied.  
*Requirement:* For programs with correct malloc+free lifecycles, `npkc` shall
  emit a Z3 verification report with `Proven ≥ 1` and `Disproven = 0`.  
*Rationale:* Z3 formal proof of UAF absence provides Criterion 2 evidence
  beyond the structural compile-time check.  
*Verification:* TVP-MEM-003 — `safe_alloc_free.npk`, `safe_two_allocs.npk` (SV-COMP).

### TOR-033: Memory Mode Enforcement
*Condition:* A `wild` pointer is used beyond its declared lifetime.  
*Requirement:* `npkc` shall enforce borrow-checker rules for `wild`-mode memory,
  rejecting programs that violate them at compile time.  
*Rationale:* `wild` mode bypasses GC but must still be lifetime-safe.  
*Verification:* TVP-MEM-004 — borrow check rejection tests.

---

## 7. Contract Verification Requirements

### TOR-040: `requires` Clause Emission
*Condition:* A function is declared with a `requires(expr)` clause.  
*Requirement:* If `--verify-contracts` is NOT specified, `npkc` shall compile
  the function normally. If `--verify-contracts` IS specified, `npkc` shall
  invoke Z3 to verify that all call sites satisfy the `requires` predicate.  
*Rationale:* Contracts provide programmer intent; Z3 verification makes them
  machine-checkable.  
*Verification:* TVP-CONTRACT-001 — `unsafe_div_noguard.npk` (Disproven=1).

### TOR-041: Contract Violation Reporting
*Condition:* Z3 finds a call site that may violate a `requires` clause.  
*Requirement:* `npkc` shall emit a `[contract] Violation:` diagnostic identifying
  the callee, the violated predicate, and example values that trigger the violation.  
*Rationale:* Actionable diagnostics are required; silent contract failure is
  worse than no contract at all.  
*Verification:* TVP-CONTRACT-002 — violation output format test.

### TOR-042: Contract Chain Propagation
*Condition:* A function with `requires(a)` calls another function with `requires(b)`,
  and `a` logically implies `b`.  
*Requirement:* `npkc` Z3 backend shall propagate the outer `requires` into the
  inner call site analysis, proving the inner call satisfies its contract without
  requiring the user to re-state it.  
*Rationale:* Contract chain propagation reduces annotation burden while maintaining
  formal correctness.  
*Verification:* TVP-CONTRACT-003 — `unsafe_contract_chain.npk` (Proven=3, inner calls proven).

### TOR-043: `ensures` Clause Verification
*Condition:* A function is declared with an `ensures(expr)` clause and
  `--verify-contracts` is specified.  
*Requirement:* `npkc` shall verify that all return paths from the function satisfy
  the postcondition `ensures(expr)` using Z3.  
*Rationale:* Postconditions prove what the function guarantees to callers;
  unverified postconditions are unsound contracts.  
*Verification:* TVP-CONTRACT-004 — ensures verification test.

---

## 8. Overflow Verification Requirements

### TOR-050: Overflow Detection at Runtime
*Condition:* `tbb32` arithmetic at runtime produces INT32_MIN.  
*Requirement:* The compiled binary shall set the result to the ERR sentinel
  and continue execution without undefined behaviour.  
*Rationale:* TOR-011 (compile-time emit) + this requirement (runtime behaviour)
  together guarantee overflow safety end-to-end.  
*Verification:* TVP-OVF-001 — `unsafe_overflow_intmax.npk` runtime exit 1 test.

### TOR-051: Overflow Z3 Proof (--verify-overflow)
*Condition:* `--verify-overflow --verify-report` are specified and arithmetic
  operands are provably bounded.  
*Requirement:* `npkc` Z3 backend shall emit a verification report with `Proven ≥ 1`
  when it can prove no overflow is possible.  
*Rationale:* Static proof of overflow absence eliminates runtime ERR checks from
  the hot path in verified code.  
*Verification:* TVP-OVF-002 — `safe_small_arith.npk` with `--verify-overflow` (exit 0).

---

## 9. Reachability / Termination Requirements

### TOR-060: Bounded Loop Termination
*Condition:* `loop(start, end, step)` syntax is used.  
*Requirement:* `npkc` shall compile `loop(start, end, step)` as a fixed-bound
  loop that terminates in exactly `ceil((end-start)/step)` iterations with no
  possibility of infinite iteration.  
*Rationale:* Non-termination is a critical safety failure in real-time systems;
  fixed-bound loops guarantee termination structurally.  
*Verification:* TVP-TERM-001 — all four Termination benchmarks (SV-COMP, runtime exit 0).

### TOR-061: Nested Loop Termination
*Condition:* `loop()` is nested inside another `loop()`.  
*Requirement:* The product of the two loop bounds shall be the exact total
  iteration count. Termination is guaranteed.  
*Rationale:* Nested loops must not introduce infinite-loop risk.  
*Verification:* TVP-TERM-002 — `terminates_nested_loops.npk` (total=25, exits 0).

---

## 10. Diagnostic Requirements

### TOR-070: ARIA Error Code Format
*Condition:* `npkc` emits a compile-time error.  
*Requirement:* All compile-time errors shall be formatted as:
  `<file>:<line>:<col>: error: [ARIA-NNN] <message>`
  where NNN is a decimal code in [001, 099].  
*Rationale:* Consistent error format allows automated test harnesses to parse
  and verify diagnostic correctness (used in Juliet and SV-COMP compile_reject tests).  
*Verification:* TVP-DIAG-001 — error format compliance tests.

### TOR-071: Z3 Report Format
*Condition:* `--verify-report` is specified.  
*Requirement:* The Z3 verification report shall be emitted to stdout in the
  following exact format:
  ```
  === Z3 Verification Report ===
    Proven:    N
    Disproven: N
    Unknown:   N
    Total:     N
  ==============================
  ```  
*Rationale:* Machine-parseable format is required for automated test harnesses
  (run_svcomp.sh, run_juliet.sh) to extract verdict counts.  
*Verification:* TVP-DIAG-002 — Z3 report format test.

### TOR-072: Exit Code Contract
*Condition:* Any `npkc` invocation.  
*Requirement:* `npkc` shall exit with code 0 on successful compilation (possibly
  including Z3 proofs), and with a non-zero code on any error (syntax, semantic,
  or Z3 violation). A Z3 `Disproven` result shall cause a non-zero exit.  
*Rationale:* Exit codes are the primary machine-readable signal used by build
  systems and CI pipelines to determine compilation success.  
*Verification:* TVP-DIAG-003 — exit code compliance across all test types.

---

## 11. Requirement Traceability Matrix

| TOR-ID | Category | Verification Method | TVP Ref | SV-COMP / Juliet Evidence |
|--------|----------|---------------------|---------|--------------------------|
| TOR-001 | Core | Positive compilation | TVP-CORE-001 | All TRUE benchmarks |
| TOR-002 | Core | Negative compilation | TVP-CORE-002 | All compile_reject |
| TOR-003 | Core | Binary diff | TVP-CORE-003 | — |
| TOR-004 | Core | Flag isolation test | TVP-CORE-004 | — |
| TOR-010 | TBB | Unit test | TVP-TBB-001 | — |
| TOR-011 | TBB | Runtime test | TVP-TBB-002 | unsafe_overflow_intmax |
| TOR-012 | TBB | Runtime test | TVP-TBB-003 | unsafe_err_propagation |
| TOR-013 | TBB | Runtime test | TVP-TBB-004 | Juliet CWE-369 |
| TOR-014 | TBB | Negative compile | TVP-TBB-005 | — |
| TOR-020 | Failsafe | Negative compile | TVP-FAIL-001 | — |
| TOR-021 | Failsafe | Negative compile | TVP-FAIL-002 | — |
| TOR-022 | Failsafe | Runtime test | TVP-FAIL-003 | — |
| TOR-030 | Memory | Compile reject | TVP-MEM-001 | unsafe_use_after_free, CWE-416 |
| TOR-031 | Memory | Compile reject | TVP-MEM-002 | unsafe_double_free, CWE-415 |
| TOR-032 | Memory | Z3 verify | TVP-MEM-003 | safe_alloc_free, safe_two_allocs |
| TOR-033 | Memory | Compile reject | TVP-MEM-004 | — |
| TOR-040 | Contract | Z3 verify | TVP-CONTRACT-001 | unsafe_div_noguard |
| TOR-041 | Contract | Diagnostic check | TVP-CONTRACT-002 | unsafe_div_noguard output |
| TOR-042 | Contract | Z3 verify | TVP-CONTRACT-003 | unsafe_contract_chain |
| TOR-043 | Contract | Z3 verify | TVP-CONTRACT-004 | ensures test |
| TOR-050 | Overflow | Runtime test | TVP-OVF-001 | unsafe_overflow_intmax |
| TOR-051 | Overflow | Z3 verify | TVP-OVF-002 | safe_small_arith |
| TOR-060 | Termination | Runtime test | TVP-TERM-001 | terminates_bounded_loop |
| TOR-061 | Termination | Runtime test | TVP-TERM-002 | terminates_nested_loops |
| TOR-070 | Diagnostic | Format test | TVP-DIAG-001 | All ARIA-022 tests |
| TOR-071 | Diagnostic | Format test | TVP-DIAG-002 | All Z3 verify tests |
| TOR-072 | Diagnostic | Exit code test | TVP-DIAG-003 | All test categories |
