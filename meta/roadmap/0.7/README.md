# Cycle 0.7 — IR emission core

**Phase B — the backend, grown rung by rung.** The first rung.

## Goal

**A program that runs and returns a code**, compiled by this compiler rather than
by the seed. Integer arithmetic, calls, control flow, `exit` — and LLVM IR text as
the output.

At the end of 0.6 the artifact validates completely and emits nothing. At the end of
0.7 it emits something, for the smallest interesting slice of the language.

## What is already decided, and by whom

Three constraints were settled before this cycle and none of them is negotiable
here. They are listed first because each one rules out an approach that would
otherwise look reasonable.

### Determinism is a requirement, not a nicety (D-078)

**Identical input must produce byte-identical output.** Not "equivalent IR" —
identical bytes. The stage-1/stage-2 fixpoint check that D-085 rests on compares
two compilers' output for equality, and a single reordered map iteration makes that
check meaningless.

What it rules out: any iteration over an unordered structure, any temp numbering
that depends on visit order rather than source order, any "whichever came first"
tie-break. The seed already works this way and says so in its header; the real
emitter inherits the requirement rather than deciding it.

### Naive lowering, and `opt` does the rest

The seed's design note: *"No optimisation, no SSA construction: every local is an
alloca with loads and stores, and every expression is materialised as a value."*

**The real emitter does the same.** Building SSA with phi nodes directly is a
larger, subtler job that `mem2reg` already does correctly, and a hand-rolled phi
placement is exactly the kind of thing that is wrong only on merges — which is
cycle 0.5's whole lesson.

### The parser never restricts; the backend does (D-085)

`tests/rejection/` holds ten programs that are **valid Nitpick** outside subset 1,
each asserting `NITPICK-RUNG-001` **and the absence of a parse error**. That suite
was written before there was a parser to break.

**Today it tests the SEED**, because `check_negative` compiles through
`compile_files`. When the real emitter exists it has to give the same answer, and
the suite should assert against it.

## What has to be measured first, and one of them may be a defect

### Two layout authorities — measured, and already diffed

**The seed asks LLVM; the compiler computes by hand.**

- `bootstrap/generator/emit.py` lowers `#size_of<T>` to
  `getelementptr T, ptr null, i32 1` — *"Ask LLVM for the size rather than
  computing layout by hand. A hand-rolled size that disagrees with the one LLVM
  lays out survives testing and corrupts memory later."*
- `src/frontend/type_layout.npk`'s `struct_layout_bound` walks the fields,
  aligns each, and sums — the hand-rolled layout that comment warns about.

**They are already compared, and have been since 0.4.8.**
`tests/frontend/type_layout.npk` declares the same structs twice — once in Nitpick,
once inside the source string the checker is handed — then asserts
`type_size(…) == #size_of<T>()` for each. The file is compiled by the **seed**, so
the right-hand side is LLVM's answer and the left is the compiler's. Its header
states the concern in the same words, and it found a real defect when it was
written: **every struct in every program was size zero**, because `tt_intern` keys
on kind and operands and deliberately not on size, so `struct_layout` computed a
size and got the original zero-sized entry back.

Cycle 0.6.4 did not weaken it. Folding `#size_of` in the compiler only affects
programs compiled by `check.npk`; a frontend unit test still goes through the seed,
so both sides are still genuinely different authorities.

**Verified at the start of 0.7: it passes, and it covers structs, nesting, padding,
arrays and `Result<T>`.** The risk this section was written to schedule is already
mitigated — which is worth recording, because the instinct to build the instrument
again is strong and the instrument was already there.

**What 0.7 still owes it: the emitter's own type lowering.** `type_layout.npk`
compares *sizes*; nothing yet compares the LLVM **type text** the two emitters
produce, and that is what has to match for their output to interoperate. 0.7.2 owns
that.

### What the artifact is, and how the harness runs it

`tools/check.npk` takes a path and reports. The emitter needs a second tool that
takes a path and writes `.ll`, and the harness needs to build it, run `llc` and
`ld.lld` on its output, and compare an exit code — which is what `check_positive`
already does for the seed.

**The shape of that second tool is 0.7.0's to settle**, because every later subcycle
writes into it.

## The subcycles

| # | Topic |
|---|---|
| 0.7.0 | What is decided, what is measured, and the artifact's shape |
| 0.7.1 | The IR writer — a buffer, names, temps, deterministic ordering |
| 0.7.2 | Types to LLVM types, agreeing with the seed exactly |
| 0.7.3 | Functions, entry blocks, parameter slots, the `Result<T>` return |
| 0.7.4 | Expressions — integer arithmetic, comparison, casts, calls |
| 0.7.5 | Statements and control flow — `if`, `while`, `pass`, `exit` |
| 0.7.6 | The rung diagnostic, and `tests/rejection/` asserting against this backend |
| 0.7.7 | The driver, and the first program this compiler builds and runs |

## Two things to decide early rather than discover

### `nsw`/`nuw` are wrong here, and the default habit emits them

**D-037 makes integer overflow defined wrapping.** Every C-family frontend emits
`add nsw` on signed arithmetic, because C says overflow is undefined — and that flag
tells LLVM it may assume overflow never happens, which licenses deleting exactly the
kind of check a safety language adds.

The user has been bitten by this in the prototype, where the workaround was to
disable optimisation for the affected integer types. **That is a symptom-level fix**:
it makes a guarantee depend on a build flag, so the guarantee is not in the artifact
— and verification analyses the *optimised* output, so a property that survives only
at `-O0` is one the analyzer never sees.

**Emit no `nsw`, no `nuw`, ever, on ordinary arithmetic.** Treat "it only works
unoptimised" as a defect report about the emitter.

### The `Result<T>` shape is already fixed by the runtime

`npkrt.ll` declares `string_concat` and friends as returning
`{ {ptr, i64, i64}, i32 }` — a `Result<string>` — and the seed's checker types them
the same way. **The two must agree or `llc` rejects the caller**, which the runtime's
own comment says is how the mismatch was caught the first time.

So the emitter's `Result<T>` lowering is not a free choice: it is `{ T, i32 }` with
the error field second, and the runtime is the reference.

## Watch for

**The seed and the compiler must interoperate until the fixpoint closes.** Stage 1
is built by the seed; stage 2 is built by stage 1. Their IR meets at every runtime
symbol and every layout. A difference that does not matter within one compiler
matters across two.

**A rung that lowers something wrongly is worse than a rung that refuses it.** The
frontend's whole Phase A output is a promise that accepted programs are valid; an
emitter that silently produces wrong code turns that into a lie. When lowering is
uncertain, refuse with `NITPICK-RUNG-001` and name the rung.

**`tests/conformance/` is where a lifted construct lands.** Subset 1 shrinking is
measured by `tests/rejection/` shrinking, and each rung moves files between the two.
That movement is the cycle's real deliverable, more than any line count.
