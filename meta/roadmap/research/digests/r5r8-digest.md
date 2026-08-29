# Digest of r5.md + r8.md — SMT encoding architecture and trusted-base design (sources: meta/roadmap/research/r5.md, r8.md)

> Extraction from "Architecting the SMT Backend for a Systems Language
> Program Verifier" (r5) and "The Architecture of Trust: Bounding and
> Justifying the Trusted Computing Base in Verification-Oriented
> Compilers" (r8). Bracketed numbers are the reports' citation indices.
> **Reliability notes at the end flag where load-bearing claims rest on
> weak citations — read them before citing this digest in a decision.**

## 1. Integer encoding (r5)

Two camps, cleanly split:
- **Bitvector (QF_BV): CBMC, ESBMC, Kani.** Hardware ints map to
  `(_ BitVec N)`; exact two's-complement wrapping/shifts/masks; overflow
  via built-in predicates (`bvaddo`/`bvsubo`, or sum `bvult` operand for
  unsigned add). Failure mode: "catastrophic performance degradation" on
  non-linear arithmetic (multiplier circuits; exponential SAT growth) —
  intractable for algebraic loop proofs [11, Verus SOSP 2024; 12].
- **Unbounded Int + range axioms: Dafny, Why3/Frama-C WP, Creusot,
  Verus.** u32 x carries `(and (>= x 0) (< x 4294967296))`; overflow is
  an explicit inequality obligation (`(<= (+ x y) MAX_U32)`). Weakness:
  bitwise ops need UFs/axioms → incompleteness. **The bridge**: explicit
  per-site casts into BV solely for bitwise operations, "quarantining the
  bit-blasting overhead" (Verus/Dafny practice) [11].

**Recommendation: both, partitioned by operation class** — Int+ranges for
ordinary arithmetic, QF_BV for bitwise, explicit theory-crossing casts.

## 2. Float and fixed-point encoding (r5)

- QF_FP exists; Z3/cvc5 parse it and internally bit-blast (SymFPU is
  "the gold standard" bridge — deeply integrated in cvc5/Bitwuzla, NOT
  Z3). SMT-COMP: "Bitwuzla and cvc5 continuously dominate QF_FP" [21][22].
  Nested non-linear FP (consecutive divisions, transcendentals,
  loop-accumulated error) exhausts memory/time [23, automotive benchmark].
- Practiced fallback: floats abstracted to **Real (QF_NRA) with rigid
  intervals** (KeY) — proves absence of special values (NaN, div-by-zero)
  fast, sacrifices 1-ULP precision [16][17].
- StageSAT-class numerical optimizers: faster but the report warns OFF for
  deterministic pipelines (external optimizer vs monolithic pinned solver).
- **Fixed-point (`tfp*`): unequivocal — scaled unbounded Int with
  compile-time scaling constants; "completely bypass the QF_FP theory";
  FP semantics on fixed-point "severely degrades performance without any
  corresponding gain in accuracy" [26].

## 3. Memory / aggregates (r5)

Three generations: (1) McCarthy arrays — sound, but axioms trigger
E-matching/MBQI loops in Z3 ("severe slowdowns and non-termination")
[27][28][29]; (2) field-as-function (Boogie/Viper) — avoids raw array
axioms, keeps the framing problem; (3) **linear-type-trusting encodings
(Verus, Creusot, Aeneas)**: with exclusive mutability guaranteed by the
type system, "safely eliminate the global monolithic heap entirely" —
mutations verified as pure functional transformations; Creusot's mutable
borrows become **prophecy variables** (current value, prophesied final
value) [1][3][33].

**Slices**: fat pointer trusted → encode as mathematical sequences (Seq
theory); the bounds obligation is localized:
`(and (>= i 0) (< i (Seq.len b)))` — "evaluated instantaneously without
consulting a global heap map" [39]. (See reliability note 3: a plain
length-scalar alternative deserves head-to-head evaluation given Seq's
youth and our determinism bar.)

**Recommendation**: ownership-trusting encoding — nitpick's
static-ownership model qualifies exactly as Rust's does for Verus/Creusot.

## 4. The obligation catalogue (r5) — the master table

| Class | Trigger | Assertion strategy |
|---|---|---|
| Overflow/underflow | before add/sub/mul/shl | range inequality (Int) or `bvaddo`-family (BV) |
| Division by zero | before `/` `%` | `(not (= den 0))` |
| Bounds | before array/slice/buffer access | `0 <= i < len` |
| Exhaustiveness | pattern matches on enums AND Result | variant disjunction covers the domain |
| Contract adherence | calls (`requires`) and returns (`ensures`) | caller asserts pre; body asserts post |
| Termination | recursion / unbounded loops | `decreases` variant strictly decreasing, well-founded |
| Aliasing (if required) | pointer casts / unsafe blocks | separation-style disjointness |

**Elision recording** (LLVM-specific, fits us exactly): discharged
overflow → tag the IR op `nsw`/`nuw`; discharged bounds/div-zero →
inject `call void @llvm.assume(i1 %cond)`; backend passes then prune the
runtime branches [42][43][44]. *(Companion constraint from r8 Lesson 1:
`nsw`/`nuw` make overflow POISON — sound only where the trap path was
proven dead.)*

**Gap the report leaves**: no cross-build obligation-identity / proof-
cache keying scheme (content-hashing, F*/Verus hint replay) — 1.5 must
design that itself; the `:named` tags below are deterministic per build
only.

## 5. Counterexample mapping (r5)

Tag every assertion `(! expr :named <tag>)` with tags derived from
variable + line + operation (e.g. `var_x_line42`) — Verus's AIR layer is
the precedent [41][46]. On SAT: `(get-model)` for concrete values +
`(get-unsat-core)` (cvc5: `(get-unsat-core-lemmas)`) for the critical
path; the parser reverse-maps tags to AST nodes and reconstructs the
trace.

## 6. Determinism (r5) — the configuration profile

"Raw speed must always be subordinated to strict reproducibility."

| Parameter | Value | Why |
|---|---|---|
| `smt.random_seed` | 0 (fixed) | identical PRNG path on all machines |
| `sat.random_seed` | 0 (fixed) | identical SAT branching/conflict learning |
| wall-clock `timeout` | **DISABLED** | eliminates load/frequency variance |
| `rlimit` | e.g. 20000000 | budget in deterministic internal ops — "proofs become a function of budget, not speed" [41, Verus CODE.md] |
| version | exact, **SHA-256-pinned** | heuristics change across patches |

## 7. Incrementality (r5)

Monolithic push/pop trees REJECTED at scale (learned-clause retention →
"severe memory leaks and degraded traversal performance") [55][28]. What
scales: **per-function/per-module fresh solver processes** (Verus
"buckets" / `spinoff_prover`) — clears lemma pollution, parallelizes
cleanly, and (implicitly) makes each verdict order-independent; push/pop
only for micro-scoping INSIDE one function's instance [41].

## 8. Trusted-base landscape (r8)

| System | Paradigm | TCB | Runtime handling | Where bugs were found |
|---|---|---|---|---|
| CompCert | certified compiler (Coq) | extraction+OCaml, AST printer, assembler, linker, formal C/asm specs | stdlib unverified; CompCertOC adds threads (PLDI'25) | Csmith: 6 CPU-years, ZERO wrong-code in verified middle-end; bugs in unverified frontend + assembly PRINTER [13][15] |
| CakeML | in-logic bootstrap (HOL4) | HOL4 kernel + ISA spec | GC/bignum/FFI verified IN the boundary; **Pancake**: no-runtime, zero-allocation imperative sibling for drivers [25] | foundational reliance on HOL4 kernel |
| seL4/Sewell | binary translation validation | SMT solvers (Z3+SONOLAR), ARM ISA model, decompiler | **handwritten assembly and volatile accesses EXCLUDED from the proof, documented as TCB bottom** [10] | aliasing assumptions; SMT state thrash |
| Alive2 | bounded TV of IR passes | Alive2+Z3+IR semantics | backend (isel/regalloc) unverified unless arm-tv | **~18% of detected miscompiles solely due to `undef`** [41]; poison/freeze/byte-types the fixes |
| EquiVM (2026) | PCC in Lean 4 | Lean kernel + EVM semantics | compiler AND proof-agent (LLM) untrusted | spec-intent misalignment only |

## 9. r8's transferable lessons (written for our exact shape)

1. **Never emit `undef`** — poison semantics exclusively, `freeze` where
   unavoidable; respect pointer-provenance/int-punning boundaries (the
   byte-types direction) or Z3 refinement checking breaks. **Emitting
   LLVM IR leaves the whole LLVM backend in the TCB — without arm-tv-style
   validation of machine code, the honest claim is "verified middle-end",
   with `llc`/`ld.lld` named as trusted.** (The 1.3.8 opt-O2 leg is a
   testing instrument on this boundary, not a proof.)
2. **The hand-written IR floor is "the single highest risk concentration
   in the TCB."** Directives: specify it mathematically; have Z3 prove
   its execution traces simulate the compiler's memory-model semantics
   where feasible; quarantine what Z3 cannot handle (volatile accesses,
   syscall trampolines, futex paths, clone/execve) and **document it
   explicitly as the enumerated bottom of the TCB** — the seL4 precedent.
3. **The Astrée/Z3 synergy** (MPF-style): Astrée does the heavy lifting
   (widening-based invariants, memory safety); **Z3 prunes Astrée's
   false alarms** (feed the abstract error path to exact BV/array
   semantics, decide concrete feasibility); Z3 also does Alive2-style
   refinement of emitted IR vs AST semantics. Resulting TCB: Astrée +
   Z3 + the LLVM-IR memory-model encoding.

## Reliability notes (verify before citing)

1. r5's "Gröbner bases solve non-linear constraints in polynomial time"
   is FALSE as stated (NIA is undecidable; the cited paper is unrelated).
   The direction (Int beats bit-blasting on non-linear) is well supported
   [11][14]; never repeat the complexity claim.
2. Determinism-section citations [50][51] are mismatched; the FLAGS are
   corroborated by the official Z3 parameter docs [52][53] and Verus
   CODE.md [41]. Verify each against the pinned Z3's parameter list.
3. The Seq-theory slice recommendation ignores Seq's youth (quantifier-
   adjacent, historical perf/nondeterminism soft spot). Evaluate the
   alternative — plain integer length + arithmetic bounds under the
   ownership assumption, no Seq — head-to-head before adopting.
4. Neither report covers cross-build obligation identity / proof caching
   — a genuine 1.5 design gap.
5. r5's FP recommendation names cvc5/Bitwuzla (SymFPU), not Z3, then
   argues elsewhere for one monolithic pinned solver — an unreconciled
   tension. Options: accept Z3's weaker QF_FP for the simple tier +
   Real-intervals for the heavy tier, or pin a second solver with its own
   determinism profile.
6. 2026-era arXiv claims (CompCertOC, EquiVM, byte types) deserve
   primary-source checks where a decision hinges on them.
