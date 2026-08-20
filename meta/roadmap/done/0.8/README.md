# Cycle 0.8 — `nlibc` core and runtime symbols

**Phase B.** Programs that can do something — and a compiler that is clean under
its own frontend.

## Goal

Two convergent lines:

1. **The self-check debt.** The compiler's own source has only ever been compiled
   by the seed, which enforces almost nothing. 0.7.8 named the residue (`=>` where
   `=>!` is required); the first `npkc src/main.npk` run found the layer above it
   (every `use` in the tree is spelled bare where the module spec requires `./`).
   The cycle opens by making `src/` **clean under the real checker**, and keeps it
   that way with a whole-tree harness check — because 1.2's fixpoint needs stage 1
   to compile these exact files, and every rule they violate today is a failure
   deferred to the worst possible time to meet it.
2. **The floor grows** (D-011, D-015): the symbols LLVM emits behind the
   program's back (`memcpy`, `memset`, the `__divti3` family when 0.9 brings wide
   integers), the undefined-symbol build check that turns the zero-dependency
   rule into a checked invariant, file output for `npkc`, and the first
   library-tier string/IO routines — written in Nitpick, over the syscall floor.

The sibling `../nlibc` repository is an empty skeleton by design; what this cycle
builds lives here, in the compiler's own tree, and graduates outward later per the
`n`-prefix rule (verified C-free before it carries the name).

## Decisions carried in

- **`enum =>! intN` reads the tag** (settled with the user, 0.8.0): `=>!` is the
  spelling for "I know what this does", and reading a tag as its number loses
  nothing. `enum => intN` stays refused — treating identity as quantity is an
  assertion, and assertions cost the bang. `intN => enum` stays impossible in
  both spellings: that direction doesn't lose information, it **fabricates** a
  value that may be no variant at all, which would silently break `pick`
  exhaustiveness. Precedent: `fd => int32` reads freely; `int32 =>! fd`
  manufactures a handle and is confined to `nlibc`.
- **`%Name` mangling belongs to D-064's family.** Generic instances need mangled
  type names in 1.0, and one reversible scheme must cover both; inventing a
  second one now would be two schemes for one job. Until 1.0 lands it, the 0.7.7
  duplicate-name refusal holds the line — no wrong name can ship in the interim.

## The subcycles

| # | Topic |
|---|---|
| 0.8.0 | The measurement, the `use`-path sweep, `enum =>!` |
| 0.8.1 | `src/` clean under the real checker, and the self-check harness stage |
| 0.8.2 | The undefined-symbol build check, and the D-011 symbol set |
| 0.8.3 | File output — `open`/`write`/`close` syscalls, `npkc -o` |
| 0.8.4 | The string floor the compiler actually calls |
| 0.8.5 | Line-discipline IO (D-050) and the diagnostics writer over it |

The table is the plan as of 0.8.0; the self-check measurement decides what 0.8.1
actually contains, and a subcycle gets inserted rather than stretched if it finds
another class.

## Watch for

- **The seed must keep compiling everything it compiles today.** The sweep edits
  the compiler's own sources; the seed is still what builds them. Every change is
  constrained to spellings both accept (`./`-prefixed paths, `=>!`, `tbb32`
  literals — all already in the seed's grammar).
- **A floor routine is TCB code.** Everything added here gets the same
  hand-written-IR discipline npkrt already carries, and the same three-way
  signature diff (`check_runtime_sigs_agree`) when it crosses the runtime
  boundary.
