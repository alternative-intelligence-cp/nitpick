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

[dependencies]
nfs = { path = "../nfs", version = "0.3.1" }

[verify]
z3 = true

[verify.nikos]
domain = "interval"
```

- **`[project]` is identity, `[build]` is settings.** `npkg`'s split is adopted;
  `entry` does not live in `[project]`.
- **`[verify]` belongs in the manifest**, not on the command line. The flags a
  project must be verified under are a property of the project, not of whoever
  typed the command. `npkc-native`'s existing `[nikos]` table established this and
  becomes `[verify.nikos]`.
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
   → ld.lld       (subprocess) → executable or library
```

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

This is what lets anyone confirm that the binary they are running is the binary
that was verified, and §6's fixpoint check is impossible without it.

---

## 6. The bootstrap ladder

The prototype builds with **CMake**, which is barred here, and nitpick-native
cannot build itself before it can compile anything.

| Stage | What it is | Written in | Fate |
|---|---|---|---|
| **Seed** | a **subset-1** → LLVM IR compiler | a throwaway generator script | discarded once stage 1 exists; its emitted IR is **committed** as the reproducible seed |
| **1** | the real compiler — **full frontend**, rung-1 backend | **Nitpick, subset 1** | permanent |
| **2** | the same source, compiled by stage 1 | Nitpick | **the artifact of record** |

**Stage 1 and stage 2 must be byte-identical.** If they differ, something from the
seed still influences the output and the result is not self-hosted.

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
| `npkg test` | builds and runs the `[[test]]` targets (§7.1); diagnostics captured through `dyn Writer` (D-075), so expected output is compared rather than eyeballed |
| `npkg update` | the **only** command that resolves versions; writes `nitpick.lock` and vendors source |
| `npkg verify` | a clean build with `[verify]` enforced, ending in the stage-1/stage-2 comparison for the compiler itself |

### 7.1 Test targets

Declared in the manifest as an array of tables:

```toml
[[test]]
name = "conformance"
kind = "positive"
path = "tests/conformance"

[[test]]
name = "rejection"
kind = "negative"
path = "tests/rejection"
```

| Kind | Passes when |
|---|---|
| `positive` | compiles, links, runs, and exits with the expected code |
| `negative` | **fails to compile, emitting exactly the expected diagnostics** |
| `diagnostic` | compiles, emitting exactly the expected warnings |

**Expectations live in the test file**, next to the code, so a test and its
expectation cannot drift apart:

```nitpick
// expect-error: NITPICK-RUNG-001
// expect-error-at: 14:9
// expect-exit: 7
// expect-no-parse-error
```

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
  ignores extras stops noticing new problems.

**`expect-no-parse-error` is the load-bearing one.** It asserts that a file
reached the *backend* to be rejected, rather than tripping the parser. That is
D-085's rule — the parser never restricts, the backend does — made checkable, and
it is what stops the grammar being quietly made partial.

**The harness is itself tested.** A suite that only ever agrees with what it is
handed is worse than no suite, because it reports green while checking nothing.
So there is a self-check that feeds the harness wrong expectations — wrong code,
wrong line, wrong exit status, a negative test that compiles, a negative test
with no expectation, and a rejection file that fails at parse time — and requires
it to report every one as a failure.

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
