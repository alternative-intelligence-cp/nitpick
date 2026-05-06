# Tool Qualification Plan (TQP)
# Aria Compiler Infrastructure — `npkc`
# DO-330 / EUROCAE ED-215 Compliance Document

**Document ID:** ARIA-TQP-001  
**Version:** 1.0  
**Date:** 2026-05-06  
**Status:** Draft — v0.18.7 Release  
**Tool:** `npkc` — the Aria language compiler  
**Branch:** `dev-0.18.x`, tag `v0.18.7`

---

## 1. Purpose and Scope

This Tool Qualification Plan (TQP) documents the intent, scope, methodology, and
planned activities to qualify the `npkc` Aria compiler under RTCA DO-330 /
EUROCAE ED-215 *Software Tool Qualification Considerations*.

The Aria programming language (`npkc`) is a safety-critical systems language
designed to structurally preclude undefined behavior at compile time. As a compiler
that transforms Aria source (`.npk` files) into executable machine code, `npkc`
is a **Criterion 1 tool**: its outputs are directly executed on target hardware
without an independent re-verification step.

### 1.1 Tools Covered by This Plan

| Tool | Version | Role | Criterion | Target TQL |
|------|---------|------|-----------|------------|
| `npkc` (Aria compiler) | v0.18.7 | Compiles .npk → native binary | Criterion 1 | **TQL-4** |
| `npkc --verify*` (Z3 backend) | v0.18.7 | Formal verification at compile time | Criterion 2 | TQL-5 |
| `npk-ls` (language server) | v0.18.7 | IDE diagnostics, no code output | Criterion 3 | TQL-5 |

**Rationale for TQL-4:** Per DO-330 Table 1, a Criterion 1 compiler used to
develop DAL D software (minor failure conditions) requires TQL-4. This is our
baseline target. Escalation to TQL-3 or higher requires additional effort outside
the current scope and is addressed in Section 7 (Future Work).

### 1.2 Out of Scope

- `npk-dap` debug adapter — no output used in airborne software
- `aria-pkg` package manager — tooling only, no code emission
- LLVM backend internals — qualified separately via commercial DO-330 LLVM kits
  (the Aria frontend's DO-330 compliance does not extend to LLVM internals)

---

## 2. Applicable Standards

| Standard | Title | Relevance |
|----------|-------|-----------|
| RTCA DO-330 / EUROCAE ED-215 | Software Tool Qualification Considerations | Primary standard for this TQP |
| RTCA DO-178C / EUROCAE ED-12C | Software Considerations in Airborne Systems | Parent standard for software DAL |
| RTCA DO-333 / EUROCAE ED-216 | Formal Methods Supplement to DO-178C | Applies to Z3/K-Framework verification (see DO333_SUPPLEMENT.md) |
| ISO/IEC 14882 (C++17) | Programming Language C++ | `npkc` compiler source language |
| LLVM Language Reference | LLVM IR Specification | Backend IR used by `npkc` |

---

## 3. Tool Description

### 3.1 Functional Overview

`npkc` accepts one or more `.npk` Aria source files and produces executable
native binaries targeting the host architecture (x86-64 default). Its key stages:

1. **Lexer/Parser** — Tokenises and parses Aria syntax into an AST
2. **Semantic Analyser** — Type-checks, borrow-checks, failsafe verification,
   ARIA error code emission (ARIA-001 through ARIA-099)
3. **Contract Verifier (Z3)** — Optional; activated by `--verify*` flags;
   proves `requires`/`ensures`, `prove()`, overflow absence, UAF absence
4. **IR Lowering** — Lowers Aria AST to internal IR (`ir_queue_*` instructions)
5. **LLVM Backend** — Lowers IR to LLVM IR, then to native code via LLVM
6. **Linker Integration** — Links against `libaria_runtime.a` and system libc

### 3.2 Safety-Critical Language Features

The Aria language enforces structural safety that `npkc` must faithfully preserve:

| Feature | Compiler Obligation |
|---------|---------------------|
| `tbb32` ERR sentinel | Arithmetic overflow/divide-by-zero must emit ERR propagation code, never silent wrap |
| `func:failsafe` | Every program must have a failsafe; compiler rejects programs without it |
| `requires`/`ensures` | If `--verify-contracts` set, Z3 must prove pre/postconditions |
| `wild`/`borrow`/`gc` memory | Compiler must insert correct lifetime tracking per memory mode |
| ARIA-022 (UAF/double-free) | Compiler must reject use-after-free and double-free at compile time |
| `pick()` exhaustiveness | Compiler must ensure all `tbb32` states (ERR + range) are handled |
| `failsafe` exit-only | Compiler must reject `pass`/`fail` in failsafe body |

### 3.3 Operational Environment

| Parameter | Value |
|-----------|-------|
| Host OS | Linux x86-64 (Ubuntu 22.04+ / Debian 12+) |
| Target arch | x86-64 (primary); WASM, PTX (secondary) |
| LLVM version | 20.1.x |
| Build system | CMake 3.20+ |
| Compiler source language | C++17 |
| Repository | `REPOS/aria/`, branch `dev-0.18.x` |

---

## 4. Tool Qualification Level Justification

### 4.1 Criterion Determination

`npkc` is a **Criterion 1** tool because:
- It produces executable object code that is run directly on the target system
- There is no independent re-verification of the generated binary by another tool
  in the standard workflow (unless `--verify*` flags are used, which qualifies
  the Z3 subsystem separately under Criterion 2)

### 4.2 TQL Selection

Per DO-330 Table 1:

| Criteria | DAL A | DAL B | DAL C | DAL D |
|----------|-------|-------|-------|-------|
| Criterion 1 | TQL-1 | TQL-2 | TQL-3 | **TQL-4** |

**Target: TQL-4** — appropriate for tools used in DAL D (minor failure) software.

This qualification level is also appropriate as a credible baseline for:
- Nikola AI safety-critical inference (internal DAL equivalent)
- Medical device software (IEC 62304 Class B analogy)
- Industrial automation (IEC 61508 SIL 1 analogy)

### 4.3 TQL-4 Objective Summary

At TQL-4, the following DO-330 Annex A tables are applicable:

| Table | Topic | Applicable at TQL-4? |
|-------|-------|----------------------|
| T-0 | Tool Operational Processes | YES (all TQLs) |
| T-1 | Tool Planning | YES |
| T-2 | Tool Development | YES (T2-1, T2-2 only) |
| T-3 | Tool Requirements Verification | YES (T3-1) |
| T-4 | Tool Design Verification | NO (TQL-3 and above) |
| T-5 | Tool Coding Verification | NO (TQL-3 and above) |
| T-6 | Tool Integration Testing | YES (T6-1, T6-2) |
| T-7 | Tool Test Verification | YES |
| T-8 | Configuration Management | YES |
| T-9 | Quality Assurance | YES |
| T-10 | Qualification Liaison | YES |

---

## 5. Qualification Strategy

### 5.1 Approach

The qualification strategy for `npkc` TQL-4 consists of four pillars:

1. **Tool Operational Requirements (TOR)** — Define all mandatory compiler
   behaviors as independently verifiable requirements (see TOR.md)

2. **Multi-layer Verification Evidence** — Cross-apply seven independent
   verification techniques from v0.18.0–v0.18.6 as TVP evidence (see TVP.md):
   - K Framework formal semantics (v0.18.0)
   - EMI/Metamorphic testing (v0.18.1)
   - Alive2 LLVM translation validation (v0.18.2)
   - IKOS abstract interpretation (v0.18.3)
   - KLEE symbolic execution (v0.18.4)
   - Juliet CWE prevention tests (v0.18.5)
   - SV-COMP benchmark conformance (v0.18.6)

3. **Structural Reject Testing** — Negative tests confirm the compiler rejects
   all invalid programs with correct ARIA error codes (ARIA-001 through ARIA-099)

4. **DO-333 Formal Methods Supplement** — K Framework proofs and Z3 contract
   verification are documented per DO-333 (see DO333_SUPPLEMENT.md)

### 5.2 COTS Component Strategy

`npkc` uses LLVM as its backend. LLVM is a COTS component not directly qualified
under this TQP. The mitigation strategy:
- `npkc` frontend qualification (this TQP) covers all Aria-specific semantics
- LLVM IR → machine code correctness is covered by Alive2 (v0.18.2), which
  validates LLVM optimization passes preserve semantics
- A complete TQL-1/TQL-2 qualification would require a commercially qualified
  LLVM distribution; this is out of scope for TQL-4

### 5.3 Feature Restriction Profile

For DO-178C DAL D software, the following `npkc` features are within scope:

| Feature | In-scope for TQL-4 |
|---------|-------------------|
| Core Aria syntax (functions, loops, tbb types) | YES |
| Memory modes: `gc`, `borrow`, `wild` | YES |
| `--verify-contracts`, `--verify-memory`, `--verify-overflow` | YES (Z3, Criterion 2) |
| `--verify-concurrency` | YES (limited — see TVP.md) |
| `wildx` (executable memory / JIT) | NO — not for safety-critical profiles |
| `async`/`await` | OUT OF SCOPE for this TQL-4 qualification |
| `simd<T,N>` SIMD types | OUT OF SCOPE |
| `--emit-ptx` (GPU code) | OUT OF SCOPE |

---

## 6. Lifecycle Model

The `npkc` qualification lifecycle follows the DO-330 planning → development
→ verification sequence, mapped to the Aria release model:

| DO-330 Phase | Aria Activity | Release |
|--------------|---------------|---------|
| Tool Planning | Define TQP, TOR scope | v0.18.7 (this release) |
| Tool Development | Compiler implementation | v0.1.x–v0.18.6 (completed) |
| TOR Definition | Define operational requirements | v0.18.7 (TOR.md) |
| Tool Verification | Seven-layer evidence (TVP) | v0.18.0–v0.18.6 (completed) |
| TAS Generation | Accomplishment summary | v0.18.7 (TAS.md) |
| Configuration Management | Git + tags + release notes | Ongoing |
| Quality Assurance | CTest regression suite | Ongoing |
| Qualification Liaison | This document set | v0.18.7 |

---

## 7. Configuration Management

All qualification artifacts are stored under version control:

| Artifact | Location | Version Control |
|---------|----------|-----------------|
| Compiler source | `REPOS/aria/src/` | Git, branch `dev-0.18.x` |
| Test suite | `REPOS/aria/tests/` | Git, same branch |
| Qualification documents | `REPOS/aria/docs/do-330/` | Git, same branch |
| Build definition | `REPOS/aria/CMakeLists.txt` | Git |
| Release tags | `v0.18.0` through `v0.18.7` | Git tags |

Change control: all changes to qualification scope or test baselines require
a new tagged commit on `dev-0.18.x` before the affected Annex A objectives
are re-verified.

---

## 8. Quality Assurance

The `npkc` regression suite (`ctest`) serves as the ongoing quality assurance
mechanism. Key test groups:

| CTest Label | Content | Count |
|-------------|---------|-------|
| `v0.18.0` | K Framework semantics | 8 tests |
| `v0.18.1` | EMI/metamorphic variants | 7 tests |
| `v0.18.4` | KLEE symbolic execution | 1 test (9 programs) |
| `v0.18.5` | Juliet CWE | 1 test (18 programs) |
| `v0.18.6` | SV-COMP benchmarks | 1 test (16 programs) |

All CTest labels must pass before any new qualification document revision is
considered valid.

---

## 9. Future Work (Escalation to TQL-3 / TQL-2)

To escalate qualification to TQL-3 (DAL C, Criterion 1):

1. Add Table T-4 (Tool Design Verification) — architecture traceability
2. Add Table T-5 (Tool Coding Verification) — source code compliance audit
3. MC/DC coverage on the compiler test suite
4. Qualified LLVM backend (commercial DO-330 LLVM distribution)

To escalate to TQL-2 / TQL-1, additionally:

5. Full formal proof of `npkc` internal modules (not just emitted code)
6. DO-330 T2-3 detailed architecture documentation
7. Tool Qualification Support Pack (TQSP) generation

---

## 10. Document Cross-Reference

| Document | ID | Purpose |
|----------|----|---------|
| This document | ARIA-TQP-001 | Tool Qualification Plan |
| TOR.md | ARIA-TOR-001 | Tool Operational Requirements |
| TVP.md | ARIA-TVP-001 | Tool Verification Plan |
| TAS.md | ARIA-TAS-001 | Tool Accomplishment Summary |
| DO333_SUPPLEMENT.md | ARIA-DO333-001 | Formal Methods Supplement |
| EVIDENCE_INDEX.md | ARIA-EVI-001 | Evidence Cross-Reference Index |
