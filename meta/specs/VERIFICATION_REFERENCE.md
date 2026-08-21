# Nitpick Formal Verification & Design by Contract

Nitpick fundamentally rejects unsafe behavior. To achieve this, it deeply integrates with the Z3 SMT solver to mathematically prove the correctness of the code before it is allowed to execute.

## 1. Formal Proofs (`prove` and `assert_static`)

### 1.1 `assert_static`
The `assert_static` builtin allows developers to encode compile-time logic checks. If the expression evaluates to false, compilation immediately halts.

```nitpick
assert_static(1i32 == 1i32);
```

### 1.2 `prove`
The `prove` keyword interacts directly with the Z3 verification backend (when the `--verify` compiler flag is set). It forces the SMT solver to construct a mathematical proof that the subsequent expression holds true across all possible control flows and variable states. 

If the solver finds a path where the expression is false, compilation fails. The compiler extracts a counterexample showing the failing variable assignments (visible via `--prove-report`).

```nitpick
int32:x = get_val();
if (x > 0i32) {
    prove(x != 0i32); // Mathematically verified at compile time.
}
```

**Path Condition Accumulation:**
The `prove` keyword is path-condition-aware. Branch guards from enclosing `if`, `while`, and other control flow are accumulated and asserted as Z3 axioms before checking the proof obligation. This means `prove(x != 0i32)` inside `if (x > 0i32)` automatically benefits from the guard `x > 0i32`.

---

## 2. Value Constraints: `limit<Rules>`

Nitpick allows you to define constraints on value ranges called `Rules`. You can then bind these rules to variables using the `limit<RuleName>` syntax. 

```nitpick
// 1. Define a rule for an integer
// The '$' variable represents the value being constrained
Rules<int32>:r_positive = { $ > 0i32 };

func:main = int32() {
    // 2. Bind the rule to a variable
    limit<r_positive> int32:x = 5i32;
    exit 0i32;
};
```

**`limit<Rules>` is enforced in every build.** `--verify` decides only whether a
given check is *discharged statically and therefore elided*, never whether it
exists (D-068). With `--verify`, the integrated Z3 solver proves that the assigned
value (`5i32`) satisfies the constraint (`$ > 0i32`) and the check is removed.
Where it cannot be proven statically — reading user input, say — the check remains
at runtime, and a violation **traps to `failsafe`**.

> A safety property must not depend on a compiler flag. The useful consequence is
> that **proving a constraint removes its runtime check**, so `--verify` is also
> the mechanism by which constrained code reaches the speed of unconstrained code.

> `FORMAL_DRAFT` 12.6.1 says only that constraints are "enforced dynamically at
> runtime" without saying what a violation *does*. It traps to `failsafe`, as
> above — stated explicitly here because an unspecified failure mode in a
> constraint system is worse than no constraint system.

### 2.1 Z3 and the borrow checker

The borrow checker is **retained** (D-003) — dropping the garbage collector made
static ownership the memory model rather than an alternative to it. Z3 resolves
aliasing patterns that lifetime analysis alone cannot: when two mutable borrows
(`$$m`) target the same array through index variables constrained by different
`limit<Rules>`, the solver proves the indices unequal and the borrows disjoint,
suppressing a false-positive aliasing error.

```nitpick
Rules<int32>:EvenIdx = { $ % 2i32 == 0i32 };
Rules<int32>:OddIdx  = { $ % 2i32 == 1i32 };

func:update = int32(limit<EvenIdx> int32:i, limit<OddIdx> int32:j, int32[100]:arr) {
    $$m int32:a = arr[i];   // mutable borrow at even index
    $$m int32:b = arr[j];   // Z3 proves i != j → borrows are disjoint
    pass(a + b);
};
```

This synergy is a direct argument for static ownership over a tracing collector:
lexical lifetimes give the solver facts it can use, whereas object validity under
a collector is a global reachability property.

Borrows are **second-class** (D-004): they pass down the call stack and never up.
A borrow may not be returned, stored into anything outliving the frame, captured,
or carried across an `extern` call, a thread spawn, or an `await` point. This
removes the need for lifetime variables entirely — validity is bounded by the
callee frame, structurally.

### 2.2 Rules Composition and Subsumption

Rules can reference other Rules via `limit<OtherRule>`, creating a constraint hierarchy:

```nitpick
Rules<int32>:r_positive = { $ > 0i32 };
Rules<int32>:r_small_positive = { limit<r_positive>, $ < 100i32 };
// r_small_positive requires: $ > 0i32 AND $ < 100i32
```

The Z3 solver can prove that one Rules block subsumes another (e.g., `r_small_positive` mathematically implies `r_positive`), enabling safe narrowing at call sites without redundant checks.

---

## 3. Function Contracts: `requires` and `ensures`

Nitpick implements classic Design by Contract (DbC) on function boundaries.

*   **`requires`**: Preconditions that must be true when the function is called.
*   **`ensures`**: Postconditions that the function guarantees will be true when it returns. (Use the special `result` keyword to reference the return value).

```nitpick
func:divide = int32(int32:a, int32:b) 
    requires b != 0i32 
    ensures result > 0i32 
{
    pass(10i32); // Hardcoded for example
};
```

When you compile with the `--verify-contracts` flag, the compiler translates these contracts into Z3 assertions to prove they are mathematically valid. If you don't use the static verifier, Nitpick automatically enforces these contracts at runtime.

### 3.1 The `Result<T>` Intercept

One of the most powerful features of Nitpick's DbC implementation is how it interacts with the type system. If a function declares a `requires` clause, **Nitpick implicitly ensures its return type is wrapped in a `Result<T>`**. 

If a caller violates the precondition at runtime, the function immediately intercepts execution and returns a `Result` error rather than crashing or triggering the failsafe. This heavily intertwines contract programming with Nitpick's sticky error propagation system, forcing the caller to explicitly unwrap or handle potential contract violations using `raw`, `drop`, or `.is_error`.

```nitpick
func:main = int32() {
    // Because `divide` has a `requires` contract, we must 
    // unwrap it with `raw` or handle the potential failure.
    int32:y = raw divide(10i32, 2i32);
    
    exit 0i32;
};
```

---

## 4. Loop Invariants (`invariant`)

Loop constructs (`loop`, `while`, `till`, `when`) support an `invariant` clause specifying conditions that must hold at every iteration boundary.

```nitpick
func:sum_range = int32(int32:n)
    requires n > 0i32
    ensures result >= 0i32
{
    int32:total = 0i32;
    int32:i = 0i32;
    while (i < n) invariant total >= 0i32, i >= 0i32 {
        total = total + 1i32;
        i = i + 1i32;
    }
    pass(total);
};
```

When compiled with `--verify-contracts`, the Z3 solver verifies the inductive step: if the invariant holds at the start of an iteration and the loop condition is true, then the invariant still holds at the end of the iteration.

---

## 5. Verification Compiler Flags

| Flag | Purpose |
|------|---------|
| `--verify` | Enable Z3 Rules/limit verification |
| `--verify-contracts` | Verify requires/ensures/invariant contracts |
| `--verify-overflow` | Verify integer arithmetic overflow |
| `--verify-concurrency` | Verify data-race freedom and **lock-order freedom** (D-056). Deadlock freedom is *not* claimed outright — residual deadlock is contained by mandatory deadlines, not proven absent |
| `--verify-memory` | Verify use-after-free & recursion bounds |
| `--verify-level=N` | Controls verification depth (see table below) |
| `--smt-opt` | Enable SMT-guided optimizations (eliminates proven-safe checks). **Writes an elimination manifest** — see §8 |
| `--smt-manifest=<path>` | Path to the elimination manifest `--smt-opt` records against |
| `--smt-timeout=N` | Per-query Z3 solver timeout in ms (default: 5000) |
| `--prove-report` | Emit prove/assert_static outcomes with counterexamples |
| `--debug-z3` | Dump SMT-LIB2 for proof obligations |

### 5.1 Verification Levels

The `--verify-level=N` flag controls verification depth:

| Level | What is verified |
|-------|-----------------|
| `0` | Rules consistency only |
| `1` | Level 0 + value constraints + `prove` / `assert_static` |
| `2` | Level 1 + function contracts + arithmetic overflow |
| `3` | All verification (contracts, overflow, concurrency, memory, SMT optimizations) |


---

## 6. Verification Backends

Two complementary systems, operating at different levels.

### 6.1 Z3 SMT Solver — proves *programs* correct

Invoked during compilation (`npkc --verify`). Translates AST nodes into SMT
formulas and decides concrete proof obligations for one specific program.

Covers: `limit<Rules>` constraints, function contracts, loop invariants,
`prove` / `assert_static` assertions, arithmetic overflow, memory safety,
concurrency, and borrow-checker index disjointness.

### 6.2 K Framework / `kprove` — proves the *language* correct

Used offline during language development, not during compilation. Proves
**metatheoretic** properties about the semantics themselves — for example that
erasing verification constructs is sound, that the borrow rules preserve memory
safety invariants, and that `Result<T>` propagation is correct.

The full operational semantics live in `k-semantics/nitpick.k`, with proof claims
in `k-semantics/proofs/`.

### 6.3 Why both

> **Z3 ensures your *program* is correct. K ensures the *language* is correct.**

Together they give an unbroken chain from language specification to compiled
binary. A verified program on top of unverified semantics proves less than it
appears to — Z3 answers "does this code satisfy its contracts", K answers "do the
rules Z3 is reasoning about actually mean what we think".

This is also why the zero-dependency constraint reaches as far as it does: a
C library inside the trusted computing base sits outside **both** backends. Z3
cannot see its contracts and K cannot model its semantics.

### 6.4 The `wildx` verification boundary (D-035, built in 0.10.5)

Runtime-generated code (`wildx`, the JIT) is the one place inside a Nitpick
program that sits outside both backends — for the same reason an FFI call
does: **the code does not exist when the verifier runs**, so there is no AST
for Z3 to translate and no term for K to reduce. This is inherent to runtime
code generation, not to Nitpick's design; no language can verify code that
does not yet exist.

What IS verified is the **container**, and it is verified structurally, not
by a runtime check:

- **W^X is a one-way transition.** `wildx_seal` moves a page RW→RX and there
  is no reverse; a page is never writable and executable at once. The
  analysis (bindings.npk, 0.10.5) refuses any write after seal
  (`NITPICK-WILDX-001`) and any execute before it (`NITPICK-WILDX-002`), so
  the transition cannot run backwards in a program that type-checks.
- **The lifecycle is a state machine** — `alloc → write → seal → execute →
  free` — with seal-after-free, double-free, and use-after-free falling out
  of the move machinery (a free is a move, D-065) and no-live-pages-at-exit
  out of the `<wild-live>` registry (D-151). Guard pages turn an
  over/underrun into a fault, and the page is placed by the kernel's mmap
  randomisation (ASLR).

The guarantee `wildx` delivers is therefore **containment**: the JIT cannot
corrupt the host program's memory safety. The *contents* of the generated
bytes are validated by Nikola's sandbox and oracle rounds, not by these
backends — exactly the division of labour D-035 settled.

**Certification note.** A program containing `wildx` will not reach the
highest assurance levels of DO-178C, IEC 61508, or ISO 26262, which require
structural coverage over code that exists before execution. That is a
property of the *program*, not the language: **`--extra-picky=no-wildx`**
(0.10.5) is a build mode that excludes runtime code generation entirely, so
the same language serves both the JIT-using and the highest-assurance
audiences without changing. It is a rule separate from `no-wild` because
manual memory and executable memory are different risks.

---

## 7. Deadlock: proven where possible, contained otherwise (D-056)

Data-race freedom is accounted for by three structural properties
(`CONCURRENCY_REFERENCE.md` §5.3): borrows cannot cross a thread spawn or
`await` (D-004), tasks do not migrate between threads (D-032), and shared arenas
never move memory or reuse slots (D-017).

**Deadlock is addressed in two layers**, because the second is what makes the
first honest:

1. **Lock-order freedom is proven.** Every blocking primitive — `mutex`,
   `rwlock`, `condvar`, `channel`, `barrier` — carries a compile-time `LEVEL` in
   its type, and acquisition must strictly increase. Circular wait is impossible
   by construction. A whole-program analysis computes each function's transitive
   acquisition set; dynamically dispatched methods declare a maximum level and
   are checked against it, and an undeclared method may not acquire at all.

2. **Residual deadlock is contained, not proven absent.** Every blocking
   operation takes a deadline and returns `Result`; there is no infinitely
   blocking acquire. What the analysis cannot cover — priority inversion, an
   unresponsive peer process, a declared-but-broad dynamic bound — surfaces as a
   timeout error at a known point rather than a wedged process.

The flag is documented as verifying **data-race and lock-order freedom**
specifically. It does not claim deadlock freedom, and the previous wording that
did has been corrected: a safety claim nothing backs is worse than an absent one,
because it invites reliance.

---

## 8. The SMT elimination manifest

`--smt-opt` is the only verification flag that changes generated code: where Z3
**proves** a runtime check unnecessary, the check is removed; where it cannot
prove it, the check stays and runs at runtime. Proof can only ever remove
something provably redundant.

That creates a reproducibility hazard. `--smt-timeout` defaults to 5000 ms, so a
proof succeeding on one machine and timing out on another would emit **different
binaries from identical sources**. For certification, where the artifact analysed
must be the artifact shipped, that is not acceptable.

**Every elision is therefore recorded in a manifest, and the manifest is
authoritative on subsequent builds** (D-040):

```
# nitpick-smt-manifest v1
# compiler: 0.1.0   z3: 4.13.0   target: x86_64-linux   timeout-ms: 5000
a3f1…  npk_parse_expr   bounds     discharged
b7c2…  npk_hash_mix     overflow   discharged
```

| Situation | Outcome |
|---|---|
| manifest matches exactly | build proceeds, binary reproducible |
| Z3 proves **more** than recorded | **build fails** |
| Z3 proves **less** than recorded | **build fails** |
| no manifest | generated; build marked *not reproducibility-verified* |

Obligations are identified by a **hash of their normalised SMT-LIB2 form**, not by
source location — line numbers shift with any edit above them, while the
obligation itself does not.

This does not make Z3 deterministic. It makes divergence **detectable and fatal**
rather than silent, which is the achievable guarantee.

It is also an **audit artifact**: the manifest is evidence that every removed
check had a proof, reproducible on demand. Certification runs may record full
proof certificates (unsat cores) rather than outcomes alone.
