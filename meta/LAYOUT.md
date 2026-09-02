# Repository layout, and why it departs from the reference

Established in cycle **0.0.0**.

`../npkc-native/MAPPING.md` documents a self-hosted Nitpick frontend decomposition
that `CLAUDE.md` names as a reasonable starting point. It is — with five
departures, each forced by a decision made since. They are recorded here so they
are not "corrected" back later by someone comparing the two trees.

## The tree

```
nitpick-native/
├── nitpick.toml            # manifest (D-077)
├── nitpick.lock            # committed, read never written (D-078)
├── meta/                   # specs, roadmap, audits — not shipped
├── src/                    # THE COMPILER. Nitpick source only.
│   ├── main.npk            # entry; [build] entry points here
│   ├── frontend/           # built once, in full
│   │   ├── analysis/       #   static analyses (cycle 0.5)
│   │   └── macro/          #   expansion + comptime (cycle 0.6)
│   ├── backend/            # grown rung by rung
│   │   ├── ir/             #   LLVM IR text emission (cycle 0.7)
│   │   └── layout/         #   type layout / ABI
│   └── driver/             # manifest, module graph, subprocess invocation
├── npkg/                   # the build and test driver (D-206, 1.4.8): a shipped tool, not the compiler
├── bootstrap/              # Not the compiler. (D-085; survival map D-203)
│   ├── generator/          #   the seed generator: subset-1 .npk -> .ll (permanent, regeneration-only)
│   ├── runtime/            #   the runtime floor, hand-written .ll — permanent form; re-homes to runtime/ at 1.4.6 (D-015/D-203)
│   ├── harness/            #   the test runner, until `npkg` parity is proven (SWITCH.md)
│   └── seed/               #   committed fixpoint IR + STAMP, from the 1.4.6 switch (D-203)
└── tests/
    ├── conformance/        # subset 1 must compile (cycle 0.0.1)
    └── rejection/          # outside subset 1 → backend diagnostic, not parse error
```

**`src/` is the compiler and nothing else.** That boundary is load-bearing:
`[build] entry` points into it, and stage 2 — the artifact of record — is built
from it. Anything that is not the compiler lives outside, which is why the seed
generator and the test harness are not in there.

## The departures

**1. No `backend/ffi_bridge.npk`.** `npkc-native` reached the C++ backend through
an FFI bridge. **That bridge is the thing this project exists to eliminate**
(`CLAUDE.md`), and D-067 settles what replaces it: the compiler emits LLVM IR
*text* and invokes `llc` / `opt` / `ld.lld` as subprocesses, linking nothing. So
`src/backend/` is an emitter, not a binding layer.

**2. No separate `borrow_checker`.** D-004 makes borrows **second-class** — they
pass down the call stack and never up. That is enforced by the same escape
analysis that governs `stack`, inside the type checker, rather than by a
standalone pass over a lifetime graph. There is no lifetime graph to walk.

**3. No `preprocessor`.** `npkc-native` has one; Nitpick no longer does. D-046
moved macro invocation to `#name(args)` under the compiler-directive sigil, and
D-057 makes macros AST-native with bounded expansion before semantic analysis.
The `pre!` text-substitution preprocessor is gone. Expansion lives in
`frontend/macro/`.

**4. No `src/inc/`.** `CLAUDE.md`'s original scaffold listed it as *"shared
headers / includes"*. **Nitpick has modules, not headers** — shared declarations
are `pub` items in a module, resolved per `MODULE_REFERENCE.md` §2.3. The
directory was a C-shaped habit with nothing to put in it, and it is removed.

**5. `bootstrap/` is new, and deliberately outside `src/`.** D-085 replaced the
prototype-as-stage-0 plan with a purpose-built seed: a throwaway program that
reads subset-1 `.npk` and writes `.ll`. There is no separate "seed binary" — what
gets **committed** is `seed/stage1.ll`: from the 1.4.6 builder switch onward, the
**fixpoint emission of the real compiler**, beside `seed/STAMP` (source commit,
toolchain version, sha256). Rebuilding from nothing needs only the LLVM
toolchain — `llc` over `stage1.ll` and the runtime floor, `ld.lld -static`, and
the result rebuilds itself from `src/` to close the fixpoint — while the
generator is needed to *regenerate* tables and the historical seed, never to
build. The snapshot is pinned, refreshed at cycle closes, never per-commit.

**The survival map (D-203, amending this section's original "all of it is
deleted once self-hosting closes"):** `seed/` and `generator/` survive
indefinitely; `harness/` survives until `npkg` parity is proven, retiring under
`meta/SWITCH.md`'s coordinated operation; and `runtime/npkrt.ll` was never
bootstrap material at all — it is linked into every artifact including the one
that ships, its permanent form is reviewed hand-written LLVM IR (D-203 settling
D-015's "later" row), and it re-homes to top-level `runtime/` at 1.4.6. Nothing
that remains in `bootstrap/` is the compiler and nothing in it is ever in an
artifact — which is why it stays outside `src/`.

> **The switch happened at 1.4.6 and the map above is now past tense in two
> places.** `runtime/npkrt.ll` has re-homed — it is `runtime/`, top level, with
> `runtime/tests/` beside it, because nothing in it was ever bootstrap material
> and its address said otherwise for fifteen cycles. And `bootstrap/seed/`
> holds a real `stage1.ll` (15,292,234 bytes) plus its `STAMP` and a `README.md`
> carrying the refresh ritual: the Python generator built that first snapshot
> and has built nothing since. The harness imports none of it any more.

`runtime/npkrt.ll` is the runtime floor — `_start`, raw syscalls, the
allocator, the `memcpy`/`memset` symbols LLVM emits calls to, threads, the
executor's parking, and the driver registry. Hand-written LLVM IR was what
D-015 specified for the first rung; D-203 made it the permanent form.

**The harness is in `bootstrap/`, not `tools/`,** because it cannot yet be
written in Nitpick: subset 1 has no directory reading and no process spawning,
and the runtime floor has no `exec`. The permanent harness is `npkg test`
(`BUILD_REFERENCE.md` §7.1). `tools/` was created at 0.0.0 in anticipation of it
and removed at 0.0.5 — which is this document's own rule about not guessing at
directories, applied to itself. It returned at 0.7.8 for the three checker
drivers the harness builds (`check`, `parse_check`, `resolve_check`); the empty
`tools/harness/` placeholder that had survived from 0.0.0 went at 1.4.8, under
the same rule.

## `npkg/` — the build and test driver (added 1.4.8, D-206)

`npkg build` and `npkg test`, in full Nitpick against the compiler's own
modules — the path code, the growable list, the lexer that finds a test's
imports — and BUILT BY THE COMPILER UNDER TEST: it is not `src/`, so D-205's
snapshot rule does not bind it, and it may use whatever today's compiler
compiles. Top level because it is a shipped tool and `src/` is the compiler
and nothing else; outside `lib/` because it is a program, not library tier.
Its header comment (`npkg/main.npk`) is its README. `bootstrap/harness/`
keeps running beside it, its `parity` stage diffing the two runners' verdicts,
until `meta/SWITCH.md` retires the harness.

Directories are created when a decision names them, not in anticipation. An empty
tree fifteen cycles deep is a guess, and guesses in a layout become `use` paths
that have to be rewritten.

## `lib/` — the library tier (added 0.8.4)

Ordinary Nitpick over the runtime floor: the code the demoted builtin names
become. Deliberately OUTSIDE `src/` — that tree is the compiler and nothing
else — and destined for the `nlibc` sibling repository once the port plan's
n-prefix bar (verified C-free) is met. Swept by the real parser like everything
else; exercised end-to-end by `tests/backend/programs/` through npkc.
