# AUDIT_v0.21.0.md — Nitpick Spec Gap Inventory

**Date:** 2026-06-11  
**Branch:** `dev-0.21.x`  
**Base commit:** `32b9ee8` (tag `v0.20.5`, merged from `dev-0.20.x`)  
**Compiler build:** clean (`cmake .. && make -j$(nproc)`, 0 warnings/errors)

---

## Baseline Validation

| Suite | Count | Result |
|-------|-------|--------|
| CTest | 18/18 | ✅ PASS |
| K core semantics | 127/127 | ✅ PASS |
| K proofs | 10/10 | ✅ PASS |
| Open bugs (META/ARIA/BUGS) | 0 | ✅ clean |

No regressions from v0.20.5. All v0.21.x work starts from a green baseline.

---

## Finding Index

| ID | Section | Severity | Target |
|----|---------|---------|--------|
| A-001 | Preprocessor | Missing | v0.21.1 |
| A-002 | Preprocessor | Missing | v0.21.1 |
| A-003 | Borrow Checker | Missing | v0.21.2 |
| A-004 | Borrow Checker | Missing | v0.21.2 |
| A-005 | Backend | Missing | v0.21.4 |
| A-006 | Backend | Polish | v0.21.4 |
| A-007 | Backend | Polish | v0.21.4 |
| A-008 | Frontend | Bug | v0.21.3 |
| A-009 | Verification | Missing | v0.21.6 |
| A-010 | Verification | Missing | v0.21.6 |
| A-011 | KNOWN_ISSUES | Polish | v0.21.5 |
| A-012 | KNOWN_ISSUES | Polish | v0.21.5 |
| A-013 | KNOWN_ISSUES | Missing | v0.21.3 |
| A-014 | Frontend | Polish | v0.21.3 |
| A-015 | Backend | Polish | v0.21.4 |

---

## Section 1 — Preprocessor & Macros

### What is implemented

Directive surface is complete for all documented v0.20.x directives:

| Directive | File | Status |
|-----------|------|--------|
| `%define` / `%undef` | preprocessor.cpp | ✅ |
| `%ifdef` / `%ifndef` / `%elif` / `%if` / `%else` / `%endif` | preprocessor.cpp | ✅ |
| `%include` (with circular-include detection) | preprocessor.cpp | ✅ |
| `%str()` stringify | preprocessor.cpp | ✅ |
| `%rep` repetition | preprocessor.cpp | ✅ |
| Variadic macros (`...`, `__VA_ARGS__`) | preprocessor.cpp | ✅ |
| Token paste (`##`) | preprocessor.cpp | ✅ |
| Hygiene counter (atomic `uint64_t`) | preprocessor.cpp | ✅ |
| `%error` / `%warning` | preprocessor.cpp (v0.20.1) | ✅ |
| `inline` / `noinline` | lexer→parser→type_checker_stmts.cpp:5676 → ir_generator.cpp:3510 `Attribute::AlwaysInline` | ✅ |
| `prove()` | lexer→parser→z3_verifier.cpp | ✅ |
| `assert_static()` | main.cpp:4139 Phase 6 pipeline | ✅ |

### **A-001 — `cfg` conditional compilation not implemented** (Missing, v0.21.1)

The keyword `cfg` is tokenized as `TOKEN_KW_CFG` in `src/frontend/lexer/lexer.cpp:87`
and registered in the runtime lexer (`src/runtime/lex/lex_helpers.cpp`), but is
**not handled anywhere in the parser or semantic analysis pipeline**. No grammar
rule, no AST node, no sema check.

Full conditional compilation (`cfg(target=linux)`, `cfg(debug)`, `cfg(feature=x)`,
`cfg(not(...))`, `cfg(any(...))`, `cfg(all(...))`) is unimplemented.

**Impact:** Any source file using `cfg` will encounter a parse error or silent
token skip. Target items (functions, structs, enums, `use`, `mod`) cannot be
conditionally included.

**Fix target:** v0.21.1 Phase 1.

### **A-002 — `derive(Display)` not wired in derive handler** (Missing, v0.21.1)

The derive macro handler in `src/frontend/sema/type_checker_stmts.cpp` auto-generates
trait impls for `ToString` (line 5377), `Eq` (5422), `Hash` (5479), `Clone` (5513),
`Debug` (5542), and `Ord` (5582). The `Display` trait is **absent from this list**.

Display for user types requires an explicit `impl:Display:for:TypeName` block.
`derive(Display)` is silently ignored or falls through.

**Note:** Display infrastructure is complete (codegen_expr.cpp:2312, type_checker.cpp:155
registers and propagates Display for primitives). The gap is purely in the derive
dispatch table.

**Fix target:** v0.21.1 Phase 2 (single `else if (traitName == "Display")` branch).

---

## Section 2 — Borrow Checker

### What is implemented

The `BorrowChecker` class covers all standard statement and expression contexts:

| Context | Method | Status |
|---------|--------|--------|
| Variable declarations | `checkVarDecl` | ✅ |
| Assignments | `checkAssignment` | ✅ |
| `if`/`else` | `checkIfStmt` | ✅ |
| `while` | `checkWhileStmt` | ✅ |
| `loop()` | `checkLoopStmt` | ✅ |
| `for` | `checkForStmt` | ✅ |
| `pick` | `checkPickStmt` | ✅ |
| `when` | `checkWhenStmt` | ✅ |
| `defer` (with deep free-scan) | `checkDeferStmt` + `scanDeferBlockForFree` | ✅ |
| Closures (escape detection) | `checkLambdaExpr` | ✅ |
| `move` expressions | `checkMoveExpr` | ✅ |
| Traits / `impl` blocks | `checkTraitDeclStmt` / `checkImplDeclStmt` | ✅ |
| Return borrow escape | `checkReturnBorrowEscape` | ✅ |
| Call ownership | `checkCallOwnership` | ✅ |
| Block scope / leak detection | `checkBlockStmt` / `checkForLeaks` | ✅ |
| Wild allocation / defer-free pairing (ARIA-014) | `checkForLeaks` | ✅ |
| Field aliases, array index borrows, pin borrows | K tests 086–109 | ✅ |
| Dynamic-index borrows (v0.19.0 fix) | K tests 110–114 | ✅ |

### **A-003 — No borrow checking across async function boundaries** (Missing, v0.21.2)

The borrow checker has no `checkAsync` method and does not model borrows around
`await` suspension points. Specifically:

- A `$m` (mutable) borrow taken before an `await` expression is not detected as
  potentially aliased during the suspension window.
- A `$i` (immutable) borrow of a value modified in another coroutine branch is
  not detected.
- The coroutine promise pointer (`current_async_promise`) is tracked only in the
  IR generator (codegen model), not in the borrow checker's alias/path analysis.

The `await` lowering in `codegen_expr_compound.cpp:1106` uses a synchronous
resume model (runs callee to completion), which makes the borrow unsoundness
less catastrophic in practice — but the checker should still flag cross-await
borrows to prepare for a true async scheduler.

**Fix target:** v0.21.2 Phase 3.

### **A-004 — `limit<Rules>` constraint not integrated into borrow path analysis** (Missing, v0.21.2)

Comment at `src/frontend/sema/borrow_checker.cpp:2432`:

```
// which is not yet wired in. When Rules constraints are available
```

The `Rules`/`limit` type constraint system (Z3 verified) is wired into the
main compile pipeline (`main.cpp:4139`) but the borrow checker does not consume
Rules constraint results. A borrow path that is statically safe given a proven
`limit` constraint can still be rejected by a conservative borrow rule.

**Fix target:** v0.21.2 Phase 4 (consult `VerifyResult` from Z3 verifier before
emitting ARIA-023/ARIA-026 borrow errors).

---

## Section 3 — Frontend (Lexer / Parser / Sema)

### What is implemented

| Feature | Status |
|---------|--------|
| All keyword tokens (50+) | ✅ |
| `when`/`then` — parsed (parser.cpp:2939) | ✅ |
| `dyn Trait` — fat-pointer `{ ptr data, ptr vtable }`, vtable thunk gen (ir_generator.cpp:1930, 4043) | ✅ |
| `obj TypeName` — handled as alias for field-typed struct access (parser.cpp:865) | ✅ |
| `opaque struct` — parsed, creates empty struct type (type_checker_stmts.cpp:4470) | ✅ |
| Generics / monomorphization — `Monomorphizer` class, struct specialization, `requestStructSpecialization` | ✅ |
| `inline` / `noinline` attributes — parsed and propagated to LLVM attrs | ✅ |
| `derive(ToString/Eq/Hash/Clone/Debug/Ord)` — auto-generated impls | ✅ |
| Display for primitives + explicit `impl:Display` blocks | ✅ |
| Six-stream I/O init (FDs 0–5, ir_generator.cpp:3589) | ✅ |

### **A-008 — type_checker.cpp:440 type inference ICE on unknown node types** (Bug, v0.21.3)

```
src/frontend/sema/type_checker.cpp:440:
    addError("Type inference not yet implemented for node type: " +
             ASTNode::nodeTypeToString(expr->type) +
             ". Use an explicit type annotation, e.g. Type:name = expr;", expr);
```

When the type inference path encounters an expression node type it does not
recognize, it emits a user-facing error with the internal node type name. This
is confusing to users (exposes AST internals) and may mask genuine compiler bugs
(the node type might be one that should be handled). The fallback path is a
proxy for unimplemented type inference cases.

**Action:** Audit which node types can reach line 440 (e.g., certain lambda
shapes, comptime expressions, tuple destructuring candidates). Handle them or
change the message to an ICE if the node type is definitively unreachable.

**Fix target:** v0.21.3 Phase 1.

### **A-014 — `catch` keyword reserved but not implemented** (Polish, v0.21.3)

`catch` is tokenized (`lexer.cpp:79`, `lex_helpers.cpp:211`) but no parser rule
consumes `TOKEN_KW_CATCH` as a statement construct. There is no `CatchStmt` AST
node. Using `catch` in source will produce a parse error.

The spec lists `catch` as a reserved keyword. If the spec does not define catch
semantics yet, the error message should say "catch is reserved and not yet
implemented" rather than a generic parse error.

**Fix target:** v0.21.3 Phase 2 (either implement catch or emit a clear
"reserved keyword, not yet implemented" diagnostic).

---

## Section 4 — Backend (IR Generator / Codegen)

### What is implemented

| Feature | Status |
|---------|--------|
| Sub-byte integers: `int1/int2/int4`, `uint1/uint2/uint4` → `i1/i2/i4` (ir_generator.cpp:1989–2064) | ✅ |
| Wide integers: `int128–int4096` / `uint128–uint4096` via LBIM structs | ✅ |
| `flt128` → `llvm::Type::getFP128Ty()` (hardware quad-precision) | ✅ |
| `flt256` / `flt512` → LBIM 4×i64 / 8×i64 limb structs | ✅ |
| `fix256` (Q128.128 deterministic fixed-point) → LBIM struct (ir_generator.cpp:2049) | ✅ |
| Ternary codegen — `TernaryCodegen` object instantiated (ir_generator.cpp:32/48) | ✅ |
| WASM target — `--emit-wasm`, `--target=wasm32-wasi` (main.cpp:326) | ✅ |
| GPU/NVVM intrinsics for CUDA kernels (codegen_expr_compound.cpp:1831+) | ✅ |
| `dyn` vtable + thunk generation for trait dispatch (ir_generator.cpp:4043+) | ✅ |
| Async coroutine frame + promise (ir_generator.cpp:38–41, codegen_stmt.cpp:891) | ✅ |
| `await` lowering — synchronous resume model (codegen_expr_compound.cpp:1106+) | ✅ |

**Not audited in this release** (require dedicated investigation in target slice):
`frac8`–`frac64`, `tfp32`/`tfp64`, `trit`/`tryte`/`nit`/`nyte`, `vec2`/`vec3`,
`simd<T,N>`, `atomic<T>`, `complex<T>`, `Handle<T>`.

### **A-005 — AArch64 JIT not implemented** (Missing, v0.21.4)

```
src/runtime/assembler/aarch64_stub.cpp:94:
    // Stub: AArch64 is not yet implemented
```

The AArch64 JIT assembler is a stub. All JIT execution on AArch64 hosts (Apple
Silicon, Ampere Altra, Raspberry Pi 5, etc.) will fall back to interpretation
or fail. The x86-64 JIT (`assembler.cpp`) is functional.

**Fix target:** v0.21.4 (AArch64 JIT implementation is likely out of scope for a
single slice; v0.21.4 should at minimum replace the silent stub with a clear
runtime error and document the limitation).

### **A-006 — ir_generator.cpp:7308 ICE on unhandled statement node** (Polish, v0.21.4)

```
src/backend/ir/ir_generator.cpp:7308:
    [ICE] ir_generator: unhandled statement node type
```

The statement codegen switch has a fallthrough ICE. If a new statement AST node
is added without a corresponding codegen case, the compiler crashes rather than
producing a useful error. Should emit an actionable ICE with the node type name
and source location.

**Fix target:** v0.21.4 Phase 2 (add node type name and source position to ICE
message; audit which node types are absent from the switch).

### **A-007 — ir_generator.cpp:12936 ICE on unhandled expression node** (Polish, v0.21.4)

Same pattern as A-006 for expression codegen. The expression switch falls
through to an ICE without identifying which node type triggered it.

**Fix target:** v0.21.4 Phase 2 (same pass as A-006).

### **A-015 — DAP server has unimplemented handler** (Polish, v0.21.4)

```
src/tools/debugger/dap_server.cpp:736:
    // Not implemented yet
    response.body = new nlohmann::json(nlohmann::json::object());
```

One DAP protocol handler returns an empty body without implementing the
requested capability. The specific request type must be identified and either
implemented or documented as unsupported.

**Fix target:** v0.21.4 Phase 3.

---

## Section 5 — Standard Library

**Package count:** 104 packages in `REPOS/aria-packages/packages/`.

*(Note: user memory recorded 103 — corrected to 104 as of v0.20.5.)*

Full package-level audit (test coverage per package, missing builtins, version
pinning) is the dedicated scope of **v0.21.5**. Items from KNOWN_ISSUES that
touch the standard library are pre-assigned there (see Section 7).

---

## Section 6 — Formal Verification

### K Semantics Coverage

**Proof files (10):** `arithmetic-proofs.k`, `borrow-path-proofs.k`,
`control-rules-proofs.k`, `core-proofs.k`, `field-alias-proofs.k`,
`pin-by-value-proofs.k`, `pin-proofs.k`, `pointer-path-proofs.k`,
`pointer-proofs.k`, `result-proofs.k`.

**Type coverage in `aria.k` syntax:**

```k
syntax Type ::= "int8" | "int32" | "int64" | "tbb32" | "flt32" | "flt64"
             | "string" | Type "->" | Type "[" Int "]" | Id
```

This is a very thin slice of the full type lattice.

### **A-009 — K type syntax does not cover the full type lattice** (Missing, v0.21.6)

Types absent from K formalization:

| Category | Absent types |
|----------|-------------|
| Bounded integers | `tbb8`, `tbb16`, `tbb64` |
| Sub-byte integers | `int1`, `int2`, `int4`, `uint1`, `uint2`, `uint4` |
| Wide integers | `int128`–`int4096`, `uint128`–`uint4096` |
| Extended floats | `flt128`, `flt256`, `flt512` |
| Typed floats | `tfp32`, `tfp64` |
| Fixed-point | `fix256` |
| Fractions | `frac8`, `frac16`, `frac32`, `frac64` |
| Ternary/nonary | `trit`, `tryte`, `nit`, `nyte` |
| Vector | `vec2`, `vec3`, `vec9` |
| Compound | `simd<T,N>`, `atomic<T>`, `complex<T>`, `Handle<T>` |
| Polymorphic | `dyn Trait`, `obj TypeName`, generic `T` |

**Fix target:** v0.21.6 — prioritize `tbb8`/`tbb16`/`tbb64` (natural extension
of existing `tbb32` rules) and `int128`/`uint128` (high compiler usage).

### **A-010 — K semantics does not formalize key language features** (Missing, v0.21.6)

Features absent from the K specification:

| Feature | Absence scope |
|---------|--------------|
| `async`/`await` | No coroutine frame semantics, no suspension rules |
| Closures (beyond 1/2-param lambdas) | Capture semantics, closure type rules |
| Preprocessor / macros | `%define`, `%ifdef`, `cfg`, derive expansion |
| Generics / monomorphization | Type substitution, specialization |
| Traits + `impl` dispatch | Static UFCS, vtable rules |
| `when`/`then`/`end` | Beyond basic `WhenStmt` |
| `defer` full semantics | FIFO/LIFO ordering, exceptions |
| Syscall tier whitelist enforcement | `sys`/`sys!!`/`sys!!!` safety rules |
| Six-stream lowering | FD assignment, stream types |
| `prove` / `assert_static` | SMT-backed compile-time assertion semantics |
| WASM / GPU targets | Target-specific rules |

**Fix target:** v0.21.6 — prioritize `async`/`await` (high-impact correctness gap)
and `defer` ordering (safety-critical).

### KLEE Symbolic Execution

KLEE integration is present (`tools/klee/aria_klee.sh`, `tools/klee/tests/`),
last formally audited at v0.18.4 (9/9 tests, 34 inputs generated, 0 violations,
commit `b5789af`). Features added in v0.18.5–v0.21.0 have not been added to the
KLEE test suite.

**Recommendation:** v0.21.6 should add KLEE tests for closure bodies,
optional<T> navigation chains, and vec9 dynamic indexing.

### Alive2 Verification

No Alive2 integration found in `REPOS/aria/src/` (zero matches for
`alive2`/`Alive2`). IR optimization correctness for the Nitpick-specific
passes (tbb overflow, LBIM arithmetic, fix256 ops, Result elision) is not
verified by Alive2. This is not an immediate blocker but should be documented.

---

## Section 7 — KNOWN_ISSUES Cross-Check

All open items from `KNOWN_ISSUES.md` (v0.20.5 state) assigned to target slices:

| ID | Issue | Severity | Target |
|----|-------|---------|--------|
| A-011 | `ahset` silently returns `-1` on capacity overflow (no ARIA error, no diagnostic) | Medium | v0.21.5 |
| A-012 | Nested `pub`→`pub` module re-export triggers GC OOM on large module graphs | Medium | v0.21.5 |
| A-013 | `pick` range arms not exhaustiveness-checked for `int32`/`int64` — missing branches silently fall through to failsafe | Medium | v0.21.3 |
| — | `flt32` ABI: passes as `double` to C extern functions (spec says `float`) | Low | v0.21.4 docs |
| — | String ABI asymmetric: `extern` returning `string` must use `AriaString` by value | Low | v0.21.4 docs |
| — | `extern` pointer return wrapped in `{ i1, ptr }` result struct | Low | v0.21.4 docs |
| — | `fixed` with compile-time arithmetic can produce negative values without error | Low | v0.21.3 |

---

## Slice Assignment Summary

| Slice | Primary scope | Findings assigned |
|-------|--------------|-------------------|
| **v0.21.1** | Preprocessor & Macro Polish | A-001 (`cfg`), A-002 (`derive(Display)`) |
| **v0.21.2** | Borrow Checker Polish | A-003 (async borrows), A-004 (Rules/limit wiring) |
| **v0.21.3** | Frontend Sema Polish | A-008 (type inference ICE), A-013 (pick exhaustiveness), A-014 (`catch` diagnostic), fixed negative comptime |
| **v0.21.4** | Backend Polish | A-005 (AArch64 JIT), A-006/A-007 (IR ICEs), A-015 (DAP), ABI docs |
| **v0.21.5** | Standard Library Audit | A-011 (ahset overflow), A-012 (GC OOM), full 104-package test coverage |
| **v0.21.6** | K Semantics & Verification | A-009 (type lattice), A-010 (feature gaps), KLEE suite expansion |

---

## Notes for v0.21.1 Implementer

1. Start with `cfg` (A-001): add `parseConfigAttr()` in `parser.cpp`, define
   `CfgAttrNode` or reuse the existing `AttributeNode`, wire into
   `type_checker_stmts.cpp` to conditionally include/exclude the annotated item.
2. `derive(Display)` (A-002): single `else if` in the derive dispatch in
   `type_checker_stmts.cpp` near line 5582. Generate a `func:display` method
   (or reuse the `ToString` template) that calls the Display impl.
3. Keep CTest 18/18, K core 127/127, K proofs 10/10 green throughout.
