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
├── bootstrap/              # THROWAWAY. Not the compiler. (D-085)
│   ├── generator/          #   the seed: subset-1 .npk -> .ll
│   ├── runtime/            #   the runtime floor, hand-written .ll (D-015)
│   └── seed/               #   committed seed IR (.ll), from cycle 0.7
├── tools/
│   └── harness/            # test runner (cycle 0.0.4)
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
prototype-as-stage-0 plan with a purpose-built seed: a throwaway generator that
emits a subset-1 compiler as LLVM IR, whose **emitted IR is committed** so that
rebuilding needs only the LLVM toolchain. Neither half is the compiler, neither
is ever in an artifact, and both are deleted once self-hosting closes — so
putting them under `src/` would misrepresent what ships.

`bootstrap/runtime/npkrt.ll` is the runtime floor — `_start`, raw syscalls, a
bump allocator that never frees, the `memcpy`/`memset` symbols LLVM emits calls
to, and enough string support to build a diagnostic. Hand-written LLVM IR is what
D-015 specifies for the first rung; real allocation and real I/O arrive with
`nlibc` in cycle 0.8.

## What is not here yet

No `npkg/`. `BUILD_REFERENCE.md` names two tools — `npkc` the compiler and `npkg`
the driver — and the real `npkg` needs a working compiler to write it with. The
minimal build driving that happens before then lives in `bootstrap/`.

Directories are created when a decision names them, not in anticipation. An empty
tree fifteen cycles deep is a guess, and guesses in a layout become `use` paths
that have to be rewritten.
