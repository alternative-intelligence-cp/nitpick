# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status: Phase A, cycle 0.5 — the static analyses

The **specification set is complete** — `meta/specs/` holds twenty documents and
`DECISIONS.md` records 122 settled decisions. The **plan is in `meta/roadmap/`**,
organised as numbered cycle folders holding `x.y.z.md` subcycle files; finished
cycles move to `meta/roadmap/done/`. Start at `meta/roadmap/ROADMAP.md`.

**Cycles 0.0–0.4 are done** (`meta/roadmap/done/`): the lexer, the AST and parser,
the module/symbol/visibility passes, and the type system. **Cycle 0.5 — the static
analyses — is in progress**, through subcycle 0.5.6: second-class borrows and
escape (D-004), definite assignment with `fixed` / `const` (D-010), `move` with
use-after-free (D-065), `pick` exhaustiveness with the `tbb` ERR arm (D-008), and
the `unknown` taint on `Result.value` (D-007), and lock levels (D-056).
`src/frontend/` is ~49 `.npk` modules of real compiler, written in Nitpick, with
the analyses under `src/frontend/analysis/`.

`tools/check.npk` runs the whole frontend over a real program — load, resolve,
type-check, report — and exits 0 on a clean one. That is what Phase A's artifact
is: a checker that validates completely and emits nothing.

### Building and testing

```
python3 bootstrap/harness/harness.py                    # everything, ~11 minutes
python3 bootstrap/harness/harness.py --only type_stmt   # one test, ~1 minute
```

It compiles each suite with the **throwaway Python seed** in
`bootstrap/generator/` (D-085 — a generator, never a dependency of the artifact),
links against `bootstrap/runtime/npkrt.ll` via `llc` and `ld.lld`, runs the
result, and compares the exit code. It also feeds every source through the **real**
parser (`tools/parse_check.npk`) and re-checks that every AST node kind is
reachable.

For the middle of a subcycle, where the question is "does this one rule fire on
this one file", there is a faster loop that builds the checker once:

```
python3 bootstrap/harness/quickcheck.py tests/types/rejection/borrows.npk
```

It is **not** a substitute for the harness and skips every whole-suite check.
Rebuild it after every edit to `src/` — it watches nothing, and a stale binary
answering an old question is the failure mode to expect.

Three things to know before you use it:

- **`--only` is for iterating, never for concluding.** It skips every whole-suite
  check — node-kind reachability, the real-parser sweep, module rejection — and
  its output says so twice. **Nothing is committed on the strength of a filtered
  run**; do a full one first.
- **A test's expectation lives inside the test**, as an `exit` code per case. A
  failure reports `exited N, expected 0`; find `exit Ni32` in the file to see
  which case broke.
- **Every test builds the whole frontend through the seed**, which is why even one
  test costs about a minute. That is the floor, not something to optimise around.

### Reserved words that read like ordinary names

Each of these has cost an edit-build-fail cycle, because the error arrives as a
parse failure some lines away from the mistake:

| Looks like a name | Actually |
|---|---|
| `pid`, `tid`, `fd`, `uid`, `gid` | the five kernel identifier **types** (D-042) |
| `dn` | a **numeric literal** — base by suffix, `n` = balanced nonary, `a`–`d` = digits −1…−4, so `dn` is −4 |
| `limit` | the verification keyword (`limit<Rules>`) |
| `any` | the type |
| `as` | a keyword |

Three more shapes that are not what a C or Rust habit expects:

- **Adjacent string literals do not concatenate.** `"a" "b"` is two literals, not
  one; use `string_concat`.
- **`discard(expr);` and `defer { … }`** take parentheses and no trailing
  semicolon respectively — `discard x;` and `defer { … };` are both parse errors.
- **A file's `mod:` name must match its basename**, or the loader reports
  `NITPICK-RESOLVE-005` at line 1 rather than anything about the name.

There is **no `npkc` build of this compiler yet** — Phase A's artifact is a
checker, and the emitter arrives in cycle 0.7. The `npkc` on PATH is the *old*
C++ prototype (`../nitpick/build/npkc`) and is not this project's output.

```
src/          # THE COMPILER — Nitpick source only; nothing else belongs here
  frontend/   #   built once, in full (analysis/, macro/)
  backend/    #   grown rung by rung (ir/, layout/)
  driver/     #   manifest, module graph, subprocess invocation
bootstrap/    # THROWAWAY seed + generator (D-085) — never in an artifact
tools/        # check/resolve_check/parse_check — the real frontend, for the harness
tests/        # conformance/ (subset 1 compiles), rejection/ (backend rejects),
              #   modules/rejection/ (loader), types/rejection/ (type checker)
meta/specs/   # language specs — see below
meta/roadmap/ # the plan; meta/roadmap/done/ archives completed cycles
meta/LAYOUT.md# the tree, and why it departs from ../npkc-native
.internal/    # gitignored scratch area — never commit anything from here
```

**`src/inc/` is gone.** It was listed as "shared headers / includes"; Nitpick has
modules, not headers. See `meta/LAYOUT.md` for that and the four other departures
from the `npkc-native` decomposition, each with the decision that forced it.

`meta/specs/` holds ten `.md` reference documents carried over from
`../nitpick-next/meta/specs/` (the Gemini experiment), plus two written here:

- `PROTOTYPE_DELTA.md` — what changed between the prototype's specs and these,
  and which questions the carried-over set leaves open.
- `PRE_PLANNING_REVIEW.md` — safety concerns, cross-document contradictions,
  missing specs, and a suggested decision order. **Read this before planning
  implementation work.**

⚠️ **The carried-over specs contradict each other in several places** and have
not yet been reconciled — they were written for a separate experiment and some
content came from a verbal retelling of prototype-vs-new differences. Do not
treat any single one as authoritative without checking `PRE_PLANNING_REVIEW.md`
Part 3 first. Notably, the memory model (GC vs RAII) is **an open decision**, not
settled fact, despite `SPEC_GAPS_AND_AMBIGUITIES.md` reading as resolved.

## Why this language exists: Nikola

Nitpick will be released publicly for general safety-critical use, but that is
not its primary purpose. Nitpick is the **host language for Nikola**, a
physics-based AGI, and essentially every unusual decision in the language traces
back to Nikola's requirements. Without this context most of the pedantry looks
like over-engineering; with it, it is load-bearing.

**Nikola's intended users are why the safety bar is where it is.** The primary
use case is a companion for neurodivergent children, extending later to children
in long-term hospital stays, and eventually a teacher's-assistant role where each
student gets a tutor that can also help with homework at home. Several of these
goals involve **robotics**.

Repeated safety reviews of the engineering documents surfaced the finding that
drives the design: **small drift in numbers can produce behavior resembling PTSD
or schizophrenia**. Around vulnerable children that is categorically
unacceptable, and preventing it outranks schedule and effort.

Two language features follow directly:

- **`Result<T>` everywhere, no exceptions.** Errors are values the caller is
  forced to handle.
- **`exit` only from `main` or `failsafe`.** Anything uncaught must be caught by
  the runtime and routed through `failsafe` so shutdown is *controlled*. An
  uncontrolled stop with actuators live is a physical safety event, not a
  debugging inconvenience.

This is also the second, stronger reason for the zero-dependency rule below:
**past the FFI barrier the runtime cannot intercept a fault** and route it
through `failsafe`, which breaks the controlled-shutdown guarantee outright.

**Performance is a first-class requirement** — Nikola is computationally enormous
and will not reach intended speed until purpose-built hardware exists;
demonstrating viable performance is what funds getting there. **But performance
is explicitly subordinate to safety.** Never trade a safety property for speed.
Raise the tradeoff instead.

When a safety mechanism looks excessive or redundant, preserve it. The standing
instruction is that these requirements remain as they are or become *more*
pedantic if required.

## The hard constraint

Nitpick is a safety-critical language, and this compiler is subject to formal
verification requirements. Consequently:

**No external dependencies. No C, no C++, no Rust, no Python, no third-party
packages of any kind.** Everything in the trusted computing base must be
verifiable, and an unverified third-party toolchain or runtime breaks that
guarantee.

This is not a stylistic preference and it is not negotiable. When a task appears
to need a dependency, the correct response is to surface the tradeoff and design
an in-house replacement — never to quietly add one. Expect a large share of the
low-level work to be hand-written LLVM IR, which is the level at which the
project can do systems work without inheriting a runtime.

The build-out is therefore much larger than a compiler of comparable scope would
normally be. The prototype (see `../nitpick`) exceeded 50k lines *with* heavy
C/C++ dependency use; this implementation is expected to be bigger precisely
because those dependencies are being replaced with verifiable in-house code.

## Bootstrap strategy: the capability ladder

The frontend is built **once, in full**. The backend is grown **incrementally**,
rung by rung. The entire point of this arrangement is to avoid rewriting the
parser at every bootstrap stage — a failure mode that the predecessor efforts hit
repeatedly.

Practical consequence when proposing changes: treat the frontend as the stable
component and the backend as the part that advances. A change that would require
reworking the lexer/parser/AST to unblock a backend rung is almost always the
wrong shape, and should be raised rather than implemented.

## Memory model

The language has four allocation regimes, spelled as modifiers in
source. **`DECISIONS.md` is the authority here**, not
`../nitpick-docs/specs/memory_specs.txt`, which still describes a collector this
language does not have:

| Modifier   | Regime                                                      |
|------------|-------------------------------------------------------------|
| *(default)* | Managed — static ownership, RAII at scope exit               |
| `stack`    | Stack-scoped                                                 |
| `wild`     | Unmanaged / manual (paired with `defer` blocks and `nodrop`) |
| `wildx`    | Executable memory — W^X, backs the JIT                       |

**There is no `gc` and no tracing collector.** D-003 dropped both: static
ownership covers unique and scoped data, and **arenas with `Handle<T>`** cover the
graph-shaped and cyclic data a collector would otherwise be needed for. `gc` is
not a keyword in the lexer.

This table listed `gc` as a fifth regime until cycle 0.5.3, and the default row
read "implicit GC / RAII" — both carried over from the prototype, both
contradicting a decision settled long before.

Anything touching allocation, lifetimes, drop semantics, or codegen for
references must be reasoned about against **all** of these, not just the one
being edited. `wildx` in particular carries the W^X invariant: a page is never
simultaneously writable and executable, and the JIT depends on that transition
being correct.

## What this replaces, and what must change

`../npkc-native` is the direct predecessor and the most useful structural
reference: a self-hosted Nitpick frontend (`.npk` sources) organized as
`src/frontend/`, `src/backend/`, `src/driver/`, `src/tools/`. Its module
breakdown — `lexer`, `parser`, `type_system`, `type_checker_*`, `borrow_checker`,
`symbol_table`, `module_{table,resolver,loader}`, `diagnostics`, `source_location`
— is a reasonable starting decomposition.

**But its backend is exactly what this project exists to eliminate.** `npkc-native`
reached the C++ nitpick backend (LLVM 20, Z3, IKOS) through an FFI bridge
(`src/backend/ffi_bridge.npk`). That bridge, and everything behind it, is
disallowed here. Read `../npkc-native/MAPPING.md` for the frontend decomposition;
ignore its backend arrangement.

One transferable frontend technique documented there: Nitpick has no OOP
inheritance, so the C++ AST class hierarchy is expressed as tagged enums over
composable structs rather than base/derived nodes.

## Reference material (read-only siblings)

These live outside this repo and must never be modified:

- `../nitpick-docs/specs/` — the language specification, split by topic
  (`memory_specs.txt`, `formal_verification_specs.txt`, `safety_systems_specs.txt`,
  `compiler_specs.txt`, `pointer_system_specs.txt`, `traits_oop_specs.txt`, …).
  `FULL_specs.txt` is the ~14k-line consolidated version. **This is the
  authority on language semantics.**
- `../nitpick-docs/reference/COMPILER_ARCHITECTURE.md` — pipeline walkthrough for
  the C++ prototype (preprocessor → lexer → parser → type/borrow check → IR gen).
  Good for *what the stages do*; its implementation is dependency-laden and is
  not a model to copy.
- `../nitpick-docs/reference/` — also has `TYPE_SYSTEM_DESIGN.md`,
  `TRAITS_AND_BORROW_SEMANTICS_RFC.md`, `UNDEFINED_STATE_PREVENTION.md`,
  `GC_TUNING_GUIDE.md`, `abi.md`, `RESERVED_WORDS.md`.
- `../nitpick/` — the ~26k-file C/C++ prototype compiler. Useful as a behavioral
  oracle; its dependency choices are **not** precedent.
- `../nitpick-proofs/` — verification harnesses (`esbmc/`, `frama-c/`, `smt/`).
- `../nitpick-bootstrap/`, `../nitpick-next/` — earlier bootstrap attempts.

## Ecosystem conventions

- Source extension is `.npk`; package manifest is `nitpick.toml`.
- `npkc` is the compiler, `npkpkg` the package manager. Both resolve on PATH
  today — `npkc` points at the prototype build (`../nitpick/build/npkc`), so
  treat it as the *old* compiler, not this one.
- **LLVM 20.1.2** is the toolchain, matching the version the prototype targets.
  Ubuntu/Mint ship only versioned binaries (`llc-20`, `opt-20`, …) because LLVM
  14, 18, and 20 coexist on this machine, so unversioned names are provided by
  symlinks in `~/.local/bin` pointing into `/usr/lib/llvm-20/bin`. Available
  unversioned: `llc`, `opt`, `lli`, `llvm-as`, `llvm-dis`, `llvm-link`,
  `llvm-config`, `llvm-extract`, `llvm-reduce`, `bugpoint`, `llvm-jitlink`,
  `llvm-mc`, `llvm-objdump`, `llvm-readelf`, plus `FileCheck` / `not` /
  `split-file` for test harnesses. `clang` is on update-alternatives and already
  resolves to 20.
  - Verify with `llvm-config --version` (expect 20.1.2). If unversioned names
    stop resolving, the symlinks are the thing to check, not the packages.
  - `lld-20` is installed and symlinked; `ld.lld --version` reports 20.1.2, so
    the linker is version-matched with the rest of the toolchain.
- Note the naming migration in flight across the docs: older material uses
  earlier project names. Prefer current naming in new code.
