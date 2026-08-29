# Digest of r4.md — LLVM toolchain output determinism (source: meta/roadmap/research/r4.md)

> Extraction from "Output Determinism and Reproducibility in the LLVM
> Pipeline" (current through the 2026 release cycle). Framing: with no
> clang, determinism responsibility bifurcates — the frontend's IR
> emission and the backend's internal discipline; **llc parses none of the
> `-f*-prefix-map` family**. Feeds subcycle 1.4.5 (D-204).

## The hazard checklist

| # | Hazard | Status | Control |
|---|---|---|---|
| H1 | **Hash-map iteration under ASLR** (DenseMap keyed by pointers; ASLR randomizes iteration order into DAG selection/regalloc/block sort) | **Open risk — no flag exists.** Core allocators and both selectors "rigorously tested"; regressions recur in new passes (llvm#62546 metadata `less_second`; LoopAccessAnalysis dep-set order) | pin the exact patch release; the build-twice check exercises this class (ASLR varies per run) |
| H2 | PRNG/UB interactions in opt (llvm#74724 seeding, #98750 JumpThreading under ABI-breaking checks, #31000 unswitch+GVN on undef) | needs LLVM-build config | record that the distro's LLVM is built `LLVM_ENABLE_ABI_BREAKING_CHECKS=OFF`; no plugins |
| H3 | Host CPU/triple leakage (omitted `-mtriple`/`-mcpu` auto-detect; `-mcpu=native` tunes to the build host) | fixed by flags | our .ll embeds `target triple` (satisfies the triple half); **add explicit `-mcpu` — the one flag our set lacks** |
| H4 | Parallel LTO nondeterminism (llvm#72206, #90857) | N/A for us | "standard llc on a single .ll processes sequentially — parallelism is not a factor" |
| H5 | ld.lld `--threads` | **output bytes deterministic** ("mathematically unaffected"); only stderr diagnostic ORDER races | ignore; `--threads=1` only if byte-stable logs ever matter |
| H6 | Archive order | deterministic (lld ignores `--start-group`; backward traversal by remembered resolutions) | none needed |
| H7 | Build-ID | `sha1`/`md5`/`fast` deterministic; **`uuid` injects entropy** | use `--build-id=sha1` if IDs are ever wanted; we currently pass none |
| H8 | Linker timestamps | **COFF/PE only** (llvm#74238); "standard Linux ELF binaries do not mandate embedding of compilation timestamps" by default | ELF: nothing to do |
| H9 | **Absolute paths in `!DIFile`** → `.debug_info`/`.debug_line`; llc translates "faithfully and unavoidably", no prefix-map flags exist at llc | frontend's job entirely | emit relative paths / no DI (we emit no debug info today; the rule binds when we do) |
| H10 | `SOURCE_DATE_EPOCH` | lld does not universally respect it (COFF gaps); `llvm-ar` needs `D` mode for archives | N/A today (no archives, no timestamps in ELF path) |
| H11 | The `-frandom-seed` analog | frontend discipline: sequential temporaries, no hash-order iteration, no threads in the emitter | already the house rule (D-078; fixpoint proves it) |
| H12 | `.comment`/producer strings | only from `!llvm.ident`/`!llvm.commandline`/`!DICompileUnit` in the .ll; **llc adds no fingerprint of its own** | we emit none — keep it so |

## Cross-machine answer

Conditionally yes: fixed .ll + fixed version + explicit flags "should
yield a byte-identical ELF object across repeated runs, across identical
target architectures, and across disparate host operating systems" —
**conditioned on the exact patch release being free of active
pointer-iteration bugs in the activated pipeline**. The evidence is
analytic (coding standards + determinism testing + bug history), not an
empirical cross-machine experiment — which is exactly why the repro stage
tests it rather than trusting it.

## Version sensitivity — the strongest directive

**"Pinning to a minor version (e.g., 20.1.x) is insufficient for strict
byte-identity."** Patch releases fix exactly the classes that alter output
(a DenseMap fix, llvm#55842's AsmPrinter OOB); even debug-info fixes shift
byte layout. Pin `20.1.2` exactly; treat ANY toolchain update, patch
included, as a breaking change that regenerates expected hashes.

## What llc-produced ELF objects contain beyond code

`.note.gnu.build-id` only per `--build-id`; `.stack_sizes` only with
`-stack-size-section`; `.remarks` only with `-remarks-section`;
`.comment` only from IR metadata; DWARF only from DI metadata. With a
clean .ll, the object is code + relocations + symbols — nothing to
sanitize.

## The report's minimal flag set

```
opt -O2 -mtriple=<T> -mcpu=<CPU> -S in.ll -o opt.ll
llc -filetype=obj -O2 -mtriple=<T> -mcpu=<CPU> opt.ll -o out.o
ld.lld --build-id=sha1 -m <EMU> -o bin out.o <archives>
```
(COFF only: `-Brepro`. ELF: no timestamp flags needed.)

## Digest caveats

- Citation quality uneven: the `--build-id=fast` hash-function claim is
  sourced to MOLD's docs, not lld's — verify against lld 20.1.2 before the
  plan asserts it (we don't use `fast`, so low stakes).
- `-relocation-model=static` is outside the report's scope (it flags
  nothing about it).
- Direct endorsements of the 1.4.5 design: the different-cwd build-twice
  comparison (cwd only enters through frontend paths; run-twice exercises
  H1), and exact-patch pinning in the manifest.
