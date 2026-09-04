# Nitpick Build System

Written rather than adopted — the carried-over spec set has no build chapter, and
`PRE_PLANNING_REVIEW.md` §4 records it as *"needed early for the bootstrap
ladder"*.

Grounded on D-064 (cross-module monomorphization), D-067 (LLVM and Z3 are invoked,
never linked), D-075 (diagnostics through `dyn Writer`), D-077 … D-079, and
D-085 (the seed is purpose-built, not the prototype).

Two tools:

| Tool | Role |
|---|---|
| **`npkc`** | the compiler — one module set to one object, or to one artifact |
| **`npkg`** | the driver — reads the manifest, orders the work, invokes `npkc`, links |

> **Naming discrepancy to settle.** `CLAUDE.md` calls the package manager
> `npkpkg`; the repository and its binary are both `npkg`, as are its
> `cmd_build` / `cmd_install` / `cmd_test` sources. This document uses **`npkg`**,
> being what exists. The one-word fix belongs in `CLAUDE.md`.

---

## 1. The manifest

One schema (D-077). Four tables.

```toml
[project]
name        = "nlibc"
version     = "0.1.0"
description = "Nitpick-native C standard library replacement"
authors     = ["Randy"]
target      = "library"          # "library" | "executable"

[build]
entry     = "src/lib.npk"
output    = "build/libnlibc"
opt-level = 2

[toolchain]                      # D-204; landed 1.4.5
llvm          = "20.1.2"
llc-flags     = ["-O0", "-filetype=obj", "-relocation-model=static"]
llc-opt-flags = ["-O2", "-filetype=obj", "-relocation-model=static"]
opt-flags     = ["-O2", "-S"]
lld-flags     = ["-static"]

[dependencies]
nfs = { path = "../nfs", version = "0.3.1" }

[verify]                         # D-218.1/D-218.2; landed 1.5.0
z3         = true
z3-version = "4.16.0"            # what `z3 -version` must report
z3-sha256  = "9a1657b3…c069bc3c"  # the binary z3 resolves to on PATH, hashed
z3-options = ["smt.random_seed=0", "sat.random_seed=0", "rlimit=20000000"]

[verify.nikos]
domain = "interval"
```

- **`[project]` is identity, `[build]` is settings.** `npkg`'s split is adopted;
  `entry` does not live in `[project]`.
- **`[toolchain]` is an INPUT, not a setting** (D-204) — §5 says why. The
  version is an EXACT PATCH RELEASE: a minor-version pin is insufficient for
  byte-identity, because a patch release may change instruction selection or
  section ordering, so an update is a breaking change that regenerates every
  expected hash. The flag lists are pinned for the same reason and are READ by
  the thing that runs them rather than restated there — a stated flag nothing
  consumes is a document that goes stale in silence. `llc-opt-flags` is the
  optimised re-run every program goes through as a check (1.3.8), kept separate
  from the build's own `llc-flags` because it is an instrument. Two keys are
  deliberately absent: `-mcpu` (the emitted IR carries `target triple` and no
  `-mcpu` is passed, so llc uses the triple's generic CPU; an explicit
  `-mcpu=x86-64` is the fix if cross-machine divergence is ever observed,
  applied everywhere at once) and `--build-id`, whose `uuid` form injects
  entropy by design.
- **`[verify]` belongs in the manifest**, not on the command line. The flags a
  project must be verified under are a property of the project, not of whoever
  typed the command. `npkc-native`'s existing `[nikos]` table established this and
  becomes `[verify.nikos]`.
- **The solver is an INPUT like the toolchain** (D-218.1/D-218.2; 1.5.0). When
  `z3 = true`, the exact release, the sha256 of the binary `z3` resolves to on
  PATH, and the determinism profile are all required, and every z3 invocation
  is BUILT from the `z3-options` list. Both runners refuse a mismatched version
  or hash, a profile with no `rlimit=` budget, and any wall-clock or parallelism
  knob in the list (`timeout=`, `-T:`, `-t:`, `solver.timeout=`, `sat.threads=`
  other than 1, `smt.threads=`, `parallel.enable=`): a verdict is a function of
  (obligation, solver build, budget), never of the machine. The pinned binary is
  the workbench's own build of the tagged release (D-233's doctrine for every
  engine); a different build is a different pin and re-records the obligation
  file with it.
- **There is no `edition` key** (D-077). One language; keeping incompatible
  versions alive would multiply every verification obligation by the number of
  editions.

### 1.1 The lock file

`nitpick.lock` records, for every dependency in the transitive graph, the **exact
version and a content hash**. It is committed.

`npkg build` **reads the lock and never writes it.** A missing or stale lock is an
error, not an invitation to resolve.

---

## 2. A build never touches the network

**Dependency resolution is not part of a build** (D-078). `npkg build` reads
`nitpick.lock` and the vendored source tree, and nothing else.

Resolution happens only in `npkg update` — explicit, human-invoked, and separate.
It performs whatever version solving is needed, writes the lock, and vendors the
source into the repository. A build after that is a pure function of the files on
disk.

> Three reasons, in order of weight: the artifact verified must be the artifact
> shipped, and a graph that can resolve differently tomorrow makes verification a
> statement about something that no longer exists; fetching source at build time
> is a supply-chain surface of exactly the kind the zero-dependency rule exists to
> close; and a build that needs a network fails in a locked-down environment,
> which is where safety-critical software gets built.

---

## 3. Module resolution

`MODULE_REFERENCE.md` §2.3 is titled *"Search Paths & Transitivity"* and defines
only transitivity. This is the missing half.

A `use` path resolves in exactly one way, decided by its first character:

| Form | Resolves against |
|---|---|
| `use "./util.npk"` , `use "../x/y.npk"` | the **importing file's** directory |
| `use "nfs/path.npk"` | the **dependency roots** |
| `use std.math.*` | the standard library |

A dependency named `nfs` declared at `../nfs` roots at **`../nfs/src/`**, so
`use "nfs/path.npk"` is `../nfs/src/path.npk`.

**An ambiguous path is an error, not a first match.** If two dependencies both
supply `x/y.npk`, the build fails and names both. Resolution order must never be
something a reader has to know the manifest's declaration order to predict.

---

## 4. What a build does

Per D-067 the compiler **emits text and invokes tools**; it links nothing.

```
manifest + lock
   → module graph            (§3)
   → npkc: module → LLVM IR text (.ll)
   → opt          (subprocess, if opt-level > 0)
   → llc          (subprocess, AT opt-level) → object
   → undefined-symbol scan   (D-011: every object, against the runtime allowlist)
   → ld.lld       (subprocess) → executable or library
```

**The undefined-symbol scan is a permanent pipeline step, not a harness
convenience** (D-206): after codegen, every object is scanned and the build
fails on any undefined symbol outside the audited runtime allowlist. The link
line itself is closed-world — only `npkc`-produced objects plus that allowlist
may appear in it, **with no relaxing flag** — which is what makes "in-process
FFI does not exist" a structural guarantee for every program, not a project
convention (D-149).

> **The scan's reader is `npkg`'s own** (`npkg/elf.npk`, 1.4.8): the object's
> ELF64 symbol table read directly — every `SHN_UNDEF` entry with a name —
> and held to the allowlist derived from `runtime/npkrt.ll`'s own `define`s
> plus `main`, the one symbol the runtime may need because the program
> provides it. Not `llvm-readelf`: a fourth tool outside the `[toolchain]`
> pin, whose text output nothing checks, is a poor foundation for a rule that
> is law. The Python harness still spawns `llvm-readelf`, and the parity
> stage holds the two readers to each other on every object it scans. The
> link line `npkg` builds takes one program object and adds the runtime
> object; there is no parameter through which a third input could enter.

Verification, where `[verify]` requests it, runs against the IR and the source
before linking, over **SMT-LIB2 text** to `z3` (D-067).

A failure in any subprocess is a nonzero exit status the driver reports — a value,
not an uninterceptable fault, which is the whole reason for the subprocess
boundary.

> **`llc` must be invoked at the manifest's `opt-level`, not at its own default.**
> `llc` defaults to `-O2`, so a driver that omits the flag optimises even when the
> manifest says not to. On the bootstrap's naive alloca-heavy IR that measured
> **68 seconds against 2.7** for one module — a 25× cost for optimisation the
> manifest had already declined. The general rule: every subprocess is invoked
> with settings derived from the manifest, never with its defaults.

### 4.1 Separate compilation, not whole-program

Each module compiles to its own object; `ld.lld` links them. D-064 already assumes
this: a generic's body is exported with its module, instantiation happens in the
using module, and identical specializations are folded at link time.

Whole-program compilation is available as an opt-in for release and verification
builds, where the extra time is affordable and the wider view is worth having.

### 4.2 Incremental builds are a development convenience only

**A verification build and a release build are always clean builds.** Incremental
output is never the artifact of record.

The reason is D-064: a change to a generic body invalidates **every module that
instantiated it**, not just the module that declares it. That dependency edge is
real, and getting it wrong produces a binary assembled from two versions of one
generic — precisely the class of inconsistency that must not reach a verified
artifact.

---

## 5. Reproducibility

**The same inputs produce a byte-identical output** (D-078).

- No timestamps, build paths, hostnames, or environment values in the artifact.
- Deterministic ordering everywhere it could vary — module compilation order, and
  the order in which monomorphized instantiations are emitted and deduplicated.
- D-064's mangled names are readable and reversible with **no hash**, so no symbol
  name depends on how the compiler was invoked.
- **The toolchain is a pinned input** (D-204): the manifest's `[toolchain]`
  records the LLVM version and the exact `opt`/`llc`/`ld.lld` flag sets, the
  driver refuses a mismatching toolchain loudly, and "same inputs" includes the
  tools. A `repro` check builds twice from different working directories and
  byte-compares the emissions — reproducibility is a tested property, not a
  claim about one process on one machine.

This is what lets anyone confirm that the binary they are running is the binary
that was verified, and §6's fixpoint check is impossible without it.

---

## 6. The bootstrap ladder

The prototype builds with **CMake**, which is barred here, and this compiler
cannot build itself before it can compile anything.

| Stage | What it is | Written in | Fate |
|---|---|---|---|
| **Seed** | a **subset-1** → LLVM IR compiler | a throwaway generator script | discarded once stage 1 exists; its emitted IR is **committed** as the reproducible seed |
| **1** | the real compiler — **full frontend**, rung-1 backend | **Nitpick, subset 1** | permanent |
| **2** | the same source, compiled by stage 1 | Nitpick | **the artifact of record** |

**Self-hosting is the fixpoint of the compiler's emission of itself** (D-202,
restating the D-079 sentence this section carried): **the first stage built from
the current source, and the next stage built by it, must emit the compiler
byte-identically.** Binaries are then identical from that emission onward — the
comparison is between emissions, never between binaries produced by two
different emitters. Before the 1.4.6 builder switch the first current-source
stage is the seed-built compiler; after it, the snapshot-built one. When the
builder snapshot is older than the source, the comparison is stage-N vs
stage-N+1 where stage N is the first current-source compiler — §6.2's
three-pass shape. If successive emissions differ, something from the previous
stage still influences the output and the result is not self-hosted.

> **Declared at 1.4.9 (2026-09-02).** The criterion held on the final 1.4
> tree in both spellings the tree uses: the README's refresh from the tree
> root gives stage2 == stage3 at 15,631,627 bytes (sha256 `9ce0ec8d3de5b2c83da4a1f11d3f89965728f6cf938f70042ea053eff5defaaf`) from the
> source at `80784f3`, installed as the snapshot with its STAMP; and the
> harness's `selfhost` stage rebuilds stage 1 byte-identically on the same
> tree, with `repro` (cwd-independent, `llc` deterministic, the STAMP
> matching, zero absolute site rows) and `parity` (906 verdicts agreeing
> between the two runners, `build/npkc` byte-identical) green beside it. Every
> snapshot before 1.4.7's was emitted by a compiler whose own source was
> subset 1; this one is emitted by, and is, the adopted compiler (SUBSET_1 §4).

> **The prototype `npkc` is not the seed** (D-085, superseding D-079). It
> implements the language Nitpick *used to be* — no `relay`, no `cstring` — so
> seeding from it would force our own sources into a foreign dialect and create a
> migration debt to undo later. It remains a **behavioural oracle**, which is the
> only role it ever had.

**The parser never restricts; the backend does.** The frontend accepts the whole
grammar from day one, and a construct the current rung cannot lower produces a
*backend* diagnostic rather than a parse error. That is what stops the grammar
being partial and re-widened rung by rung — the failure that ended
`nitpick-bootstrap`.

The seed is **invoked once, ever** — weaker than D-067's invoked-never-linked,
since `llc` runs at every build. Because its emitted IR is committed, rebuilding
needs only the LLVM toolchain; the generator is needed to *regenerate* the seed,
never to build.

> **What the fixpoint does not prove.** A self-reproducing compiler backdoor
> introduced at the **seed** would survive it — Thompson's *Reflections on
> Trusting Trust*, a property of bootstrapping in general rather than of this
> arrangement. The check establishes self-consistency, not the absence of an
> adversarial seed. The mitigation, if ever required, is **diverse
> double-compilation**: build stage 1 from a second, independently obtained seed
> and confirm the stage 2 outputs match.
>
> That exposure is **smaller under D-085 than it was under D-079**, and it is the
> second reason for the change: a purpose-built seed of a few thousand readable
> lines can actually be audited. A 26,000-file C++ prototype cannot.

### 6.1 The capability ladder is orthogonal

The bootstrap stages above are about *who compiled the compiler*. The **capability
ladder** in `CLAUDE.md` is about *what the backend can yet emit*: the frontend is
built once in full, and the backend grows rung by rung.

They interact in one place. A backend rung that cannot yet compile the compiler's
own sources cannot host stage 1, so **the rung that closes self-hosting is the
milestone that matters**, and rungs before it are validated against the seed's
output rather than by self-compilation.

**Subset 1 is what the two ladders share.** It is bounded from both sides: large
enough to write a complete frontend in, small enough for a throwaway seed to lower.
Roughly — integers and chars, `bool`, pointers, slices, structs, tagged enums,
arrays, functions returning `Result<T>`, `if` / `while` / `pick`, `pass` / `fail` /
`raw`, and allocation. Not generics, traits, `async`, macros, `comptime`, or
contracts. The AST is expressible without generics because it is **tagged enums
over composable structs**, which `CLAUDE.md` already records as the transferable
frontend technique — and that is precisely what makes the subset viable.

### 6.2 Changing a primitive after self-hosting

A property worth stating, because it is the practical payoff of D-085 and it is
easy to assume the opposite.

**Once stage 2 exists, primitives are implemented in Nitpick.** A primitive's
lowering lives in the compiler's backend, which is Nitpick source like everything
else — so changing one is editing Nitpick and rebuilding. **The seed is not
involved**, and there is no separate language or toolchain to drop into.

This is exactly what a C-based bootstrap cannot offer. There, a primitive is
whatever the C beneath it made it, the only place to change it is back in that C,
and even then the result is bounded by C's own semantics. The binding is
contagious: it survives every rung climbed above it, because the rungs are built
on it.

**One nuance.** A change that alters a *layout or ABI* needs **two rebuild
passes**, not one:

| Pass | Produces a compiler that is… | …and emits |
|---|---|---|
| build with the current compiler | the **old** form | the **new** form |
| build again with that one | the **new** form | the **new** form |
| build a third time | identical to the second | — |

The second and third are byte-identical, and **that is precisely what §6's
fixpoint check tests.** So the machinery for confirming a primitive change has
fully converged already exists and is already required; an ABI change costs one
extra pass and nothing more.

**The seed is re-run essentially never** — only to rebuild the world from nothing,
or to verify that path still works. It is not part of the ordinary edit cycle,
even for edits at the very bottom of the language.

---

## 7. Commands

| Command | Behaviour |
|---|---|
| `npkg build` | reads lock + vendored source; never resolves, never fetches |
| `npkg test` | builds the compiler (§6's ladder), runs the runner self-check (§7.1), then every `[[test]]` entry in manifest order — the real-parser sweep, the five rejection suites, the fixtures, the backend programs with their `opt -O2` re-run, the runtime floor's tests and the acceptance suite are entries, not code (D-238). A test's diagnostics are the child compiler's stderr, captured through the supervised spawn (`lib/nproc.npk`, D-206) and compared on codes and spans — D-075's `dyn Writer` capture was superseded by D-229 and, for a child process, by the pipe. The `cost` stage (1.5.1b step 0) spawns the compiler and the probe programs with `NPK_HEAP_STATS` added to the environment and reads the runtime's `heap:` line from the same pipe. `--only SUBSTR` narrows to the compile-stage `[[test]]` files whose path holds it and skips every other stage, saying so; `--selfcheck` runs the self-check alone; `--verdicts PATH` writes every unit's verdict, the list the parity stage diffs (D-206 §5) |
| `npkg update` | the **only** command that resolves versions; writes `nitpick.lock` and vendors source |
| `npkg verify` | the ladder, then the VERIFIED build (1.5.0; D-218, D-219): `[build] entry` is compiled with `--obligations`, every function's D-218 obligations are decided by the pinned z3 under the pinned profile (one fresh process per function, D-218.3), the rows are held to the committed `nitpick.obligations` — absent or different is a failure by name; **`--record`** writes it, on purpose, the deliberate re-baseline — then the entry is compiled again with `--elide nitpick.obligations`, every guard the manifest discharged giving way to `llvm.assume` (D-218.9), the verified IR cross-checked (an `assume` per discharged site, a trap per retained one), assembled, closed-world linked to `build/verify/npkc`, and shown to rebuild the compiler byte-identically (D-202 over the verified build). **`--explain`** adds `build/verify/explain.txt`: a model per open row, the reason per budget row, an unsat core per discharged one — never on the gate path (P-7). `[verify.nikos]` is declared and not run until 1.6.0's gate names an engine (D-217, D-233) |

### 7.1 Test targets

EVERY suite either runner runs is declared in the manifest as an array of
tables, in run order, and both runners read the one table (D-238, 1.4.8b) — a
manifest that declared four of fourteen suites was a document a reader could
not trust to say what ran, the stale-document shape D-204 refused for flags. An
entry a runner cannot honour is refused BY NAME before anything runs (exit 2),
never skipped: a stage it does not know, a `kind` on a stage that has none, a
compile entry with no kind, no `paths`/`path` (or both), a key the schema lacks.

```toml
[[test]]
name = "conformance"          # the suite's label in every verdict line and stage line
stage = "compile"             # the tool that judges the suite (the table below); compile is the default
kind = "positive"             # compile only: positive | negative | diagnostic
path = "tests/conformance"    # or `paths = [...]`; `recursive = true` sweeps subdirectories

[[test]]
name = "types"
stage = "check"
recursive = true
path = "tests/types/rejection"

[[test]]
name = "programs"
stage = "program"
paths = ["tests/backend/programs", "tests/conformance"]
```

| Stage | Judged by | Passes when |
|---|---|---|
| `compile` (the default) | the compiler under test | held to `kind`: `positive` compiles, links, runs, and exits with the expected code; `negative` **fails to compile, emitting exactly the expected diagnostics**; `diagnostic` compiles, emitting exactly the expected warnings |
| `parse` | `tools/parse_check` | accepted with no diagnostic — D-085's sweep, every source in the tree, each once |
| `resolve` | `tools/resolve_check` | refused by the LOADER with exactly the expected codes |
| `check` | `tools/check` | refused by the frontend — the type checker, a static analysis, expansion, derive, whichever the suite's directory names — with exactly the expected codes |
| `accept` | `tools/check` | accepted in silence |
| `fixture` | the compiler under test | built like a program and never run; its uppercased stem becomes an `// argv:` token (a `.c` here is a reference driver built with the system C compiler — test tooling outside the TCB, D-149) |
| `program` | the compiler under test | emitted, scanned, assembled, linked, run at -O0 and again through `opt -O2`, the same exit required |
| `runtime` | `llc` + `ld.lld` | a hand-written `.ll` assembled, linked against the floor, run, its `expect-exit:` met |
| `verify` | the compiler under test, z3 | (1.5.0, D-218) compiled with `--obligations`, its rows decided by the pinned z3 under the pinned profile, the (kind, verdict) counts of the test's OWN module equal to its `expect-obligation:` lines exactly, then the VERIFIED build emitted with that run's manifest, cross-checked, linked and run at -O0 and under opt -O2 with `expect-exit:` met both times |
| `cost` | the compiler under test, the runtime's `NPK_HEAP_STATS` | (1.5.1b step 0) each `.toml` unit under the entry's paths is measured by the ALLOCATOR'S OWN NUMBERS — bytes requested in total, the peak of bytes live, allocations, which the runtime prints as `heap: allocated=<n> peak_live=<n> count=<n>` on fd 2 at exit when the environment carries `NPK_HEAP_STATS` — and held to the bound the unit states; wall-clock is printed as colour and is never a verdict. A `recipe` unit regenerates one of DEF-1's three shapes at `n` and `n * scale` and holds both ratios to `bound`; a `probe` unit compiles, links and RUNS `program` and `against` and holds the first's peak to `bound` times the second's; a `self` unit compiles `entry` and reports, held to `ceiling` bytes of peak when non-zero. `expect = "fail"` is the negative control: the bound must be VIOLATED until the commit named in `until` lands, and a unit that holds early fails until its row is removed. Every compile and run must exit 0 — a fast failure looks like a fast compile |

Membership stays with the stage: a `resolve`/`check` file with no `expect-error`
is a fixture another file imports and is skipped; a `compile`/`program` file
some other file in its suite imports is skipped. A suite runs in manifest
order — `fixtures` before `programs`, since the second reads the map the first
fills. `--only SUBSTR` narrows the `compile`-stage entries to the files whose
path holds it and skips every other stage, saying so.

**Expectations live in the test file**, next to the code, so a test and its
expectation cannot drift apart:

```nitpick
// expect-error: NITPICK-RUNG-001     a finding that must be reported (appended)
// expect-error-at: 14:9              moves the LAST expect-error to that line[:column]
// expect-note: NITPICK-MACRO-009     a note that must be reported (its own channel)
// expect-note-at: 3                  the same for the last expect-note
// expect-exit: 7                     the exit a run must produce (0 when absent)
// stress: 40                         run it that many times, the SAME answer every time
// argv: MOCK_DRIVER 555              extra argv; a fixture's name becomes its built path
// expect-no-parse-error              the file is meant to reach the backend (D-085)
// expect-obligation: div-zero open 1   a `verify` test's row: KIND VERDICT N, or `none` (1.5.0)
```

Both runners read exactly this grammar, marker for marker and in this order
(`bootstrap/harness/harness.py` `read_expectations`, `npkg/expect.npk`).

Three rules make this worth having rather than decorative:

- **Assert on codes and spans, never on message text.** Messages must stay free
  to improve without breaking the suite, which is why diagnostic codes are
  stable identifiers rather than prose. The rendered line is
  `CODE path:line:col: message` (1.0.8), with `note ` or `warning ` in front
  for those severities — an error is the unmarked case — and `<no span>` in
  place of the position for a spanless diagnostic (D-162). One formatter
  (`diag_line`, `diagnostics.npk`) renders it for every driver, and the
  harness reads the code as the first token and the span as the second,
  ignoring everything after; the format's shape is pinned once, in
  `tests/frontend/diagnostics.npk`.
- **A negative test with no `expect-error` is a failing test.** Asserting only
  *"it did not compile"* stops noticing when a test starts failing for a
  different reason than the one it was written to guard.
- **Unexpected diagnostics fail a test as surely as missing ones.** A suite that
  ignores extras stops noticing new problems. **Enforced from 1.4.8b (D-237)**,
  and this is the rule as both runners implement it: on the error channel —
  findings, with `warning` counted as a finding — the SET of codes a rejection
  test reports must EQUAL the set its expectations name, and every
  `expect-error-at` still binds its code to its line and column. A code
  reported that no expectation names fails the test by name (`reported X,
  which no expectation names`), as a missing one always has. The note channel
  keeps its own rule: an expected note must be reported at its place, and an
  unexpected note is not a finding (`NITPICK-MACRO-009` says where a body was
  expanded, and every expansion test would otherwise have to name a
  location). One function per runner (`check_module_rejection`,
  `check_rejection`), the parity stage proving they agree, and the runner
  self-check's `unasserted-extra` case proving the rule bites. An extra is
  resolved one of two ways and never a third: a finding the test MEANS is
  named with an `expect-error` line beside its construct; an incidental defect
  in the test's own text is corrected so the file reports only what it tests.

  > The subset rule this replaced — every expected code must appear, extras
  > pass — ran from 0.8 to 1.4.8 in the harness and was ported as found into
  > `npkg`, and the sentence above described nothing either enforced: the
  > dormant-rule pattern, in the test runner. Measured at 1.4.8 Part D, 17 of
  > 131 rejection files reported a code nobody asserted — nine from one
  > `resolve_check` defect (fixed there), and eight resolved at 1.4.8b: two
  > expectations still spelling the arity code 1.4.2 retired, two `failsafe`s
  > written before D-210 made `IntOverflow` reachable, a test whose assoc was
  > named `Error` before D-179 made that the builtin error type, and four
  > second findings the tests meant and never named.

**A `verify` test names its rows exactly** (1.5.0, P-22): the multiset of
(kind, verdict) counts over the rows of the test's own module — its `main`,
its `failsafe`, its `@"npk.<module>.…"` functions; the prelude's rows ride in
the same manifest and are not the test's to name — must EQUAL the set its
`expect-obligation:` lines name, and a verify test with no such line is a
failing test. The verdicts are `discharged`, `open`, `budget` and
`unencoded` (VERIFICATION_REFERENCE §7b), and `expect-exit:` is met by the
VERIFIED binary at -O0 and under opt -O2.

**`expect-no-parse-error` is the load-bearing one.** It asserts that a file
reached the *backend* to be rejected, rather than tripping the parser. That is
D-085's rule — the parser never restricts, the backend does — made checkable, and
it is what stops the grammar being quietly made partial.

**The harness is itself tested.** A suite that only ever agrees with what it is
handed is worse than no suite, because it reports green while checking nothing.
So there is a self-check that feeds the harness wrong expectations — wrong code,
wrong line, wrong exit status, a negative test that compiles, a negative test
with no expectation, and a rejection file that fails at parse time — and requires
it to report every one as a failure. Both runners carry it, over one case set:
`bootstrap/harness/selfcheck.py` for the Python harness and `npkg test
--selfcheck` for `npkg` (D-206 §5 transferred the obligation; a full `npkg
test` runs it first), each with D-204's toolchain pin driven through its
FAILURE path — a pin that is not the installed version must refuse, no pin
must refuse, the real pin must pass.

**Parity between the runners is measured, not assumed** (D-206 §5, 1.4.8).
The harness's `parity` stage builds `npkg` with the compiler under test, runs
`npkg test --verdicts` from the manifest root, and diffs the two verdict lists
unit for unit — the same suites, the same files, the same pass or fail — then
byte-compares `npkg build`'s compiler with its own. The harness retires only
under `meta/SWITCH.md`, after parity has held through cycle 1.5.

---

## 8. Open items

- ~~**Test-target declaration.**~~ — **settled; see §7.1.** `[[test]]` tables
  with three kinds, in-file expectations asserting on codes and spans rather
  than message text, and a harness that is itself tested against wrong
  expectations. The prototype's `WILL_FAIL TRUE` and
  `// Expected: COMPILER ERROR` conventions were the material; what they lacked
  was any assertion about *which* diagnostic, which is the half that makes a
  negative test worth keeping.
- **Cross-compilation.** `--target` and how `[build]` expresses more than one.
  Not needed until a second target exists, but the manifest shape should not have
  to change when it does.
- **The vendored-tree layout.** Where `npkg update` writes, and whether vendored
  sources are committed verbatim or as an archive plus hash. Committed verbatim is
  the assumption above; an archive would make review harder, which argues against
  it.
