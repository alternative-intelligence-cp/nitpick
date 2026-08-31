# The committed bootstrap snapshot

`stage1.ll` is the Nitpick compiler, in LLVM IR, as emitted by the compiler in
`src/`. `STAMP` records which commit it came from, which toolchain emitted it,
and its sha256.

**This is what you build from.** Since 1.4.6 (D-203/D-205) nothing else
bootstraps this project: given a checkout, an LLVM toolchain at the version
`nitpick.toml` pins, and nothing else — no Python, no C compiler, no package
manager — these two files plus `runtime/npkrt.ll` produce a working `npkc`.

```sh
llc -O0 -filetype=obj -relocation-model=static runtime/npkrt.ll -o npkrt.o
llc -O0 -filetype=obj -relocation-model=static bootstrap/seed/stage1.ll -o npkc.o
ld.lld -static -o npkc npkc.o npkrt.o
```

That `npkc` compiles the current `src/`. It is a compiler with no seed in it:
the Python generator under `bootstrap/generator/` produced the FIRST snapshot,
in August 2026, and has never built the compiler since.

## The rule this file exists to enforce (D-205)

> **`src/` may not use any construct its current builder cannot compile.**

The builder is `stage1.ll`. So a language feature enters `src/` only after a
snapshot that already understands it — implement the feature, refresh the
snapshot, *then* use it in the compiler's own source. Getting this backwards
produces a tree that cannot build itself from a clean checkout, which is the
one failure mode the committed snapshot exists to prevent.

## Refreshing it

At cycle closes, with the push. The refresh is done BY THE SNAPSHOT, never by
the Python generator — that is what "the seed retires" means.

```sh
# 1. the floor, and the builder from the snapshot you have
llc -O0 -filetype=obj -relocation-model=static runtime/npkrt.ll -o /tmp/npkrt.o
llc -O0 -filetype=obj -relocation-model=static bootstrap/seed/stage1.ll -o /tmp/b.o
ld.lld -static -o /tmp/builder /tmp/b.o /tmp/npkrt.o

# 2. the builder compiles the CURRENT compiler
/tmp/builder src/main.npk -o /tmp/stage1.new.ll

# 3. THE FIXPOINT: stage2 == stage3, and INSTALL STAGE 2
llc -O0 -filetype=obj -relocation-model=static /tmp/stage1.new.ll -o /tmp/s2.o
ld.lld -static -o /tmp/npkc2 /tmp/s2.o /tmp/npkrt.o
/tmp/npkc2 src/main.npk > /tmp/stage2.ll
llc -O0 -filetype=obj -relocation-model=static /tmp/stage2.ll -o /tmp/s3.o
ld.lld -static -o /tmp/npkc3 /tmp/s3.o /tmp/npkrt.o
/tmp/npkc3 src/main.npk > /tmp/stage3.ll
cmp /tmp/stage2.ll /tmp/stage3.ll            # MUST be silent

# 4. install, and stamp what you installed
cp /tmp/stage2.ll bootstrap/seed/stage1.ll
sha256sum bootstrap/seed/stage1.ll
git rev-parse HEAD
```

Step 3 is not optional. A snapshot that compiles the compiler but whose output
does not rebuild itself is a snapshot that works exactly once, and the next
refresh from it produces something else again.

**It compares stage2 with stage3, not stage1 with stage2, and it installs
STAGE 2** — corrected at 1.4.7/D-225, where the older spelling failed on a
correct refresh. Whenever a change alters what the compiler EMITS, stage1.new
(emitted by the OLD builder) and stage2 (emitted by a compiler that has the
change) differ BY CONSTRUCTION, and must: the refresh before this one passed
only because D-224's emission change happened to be inert for npkc's own
source. Stage1.new is also the wrong file to install — its BODY predates the
change even though its emitter carries it, so a snapshot built from it still
has the old behaviour inside, and the `repro` stage would fail against a fresh
emission on the next run. Stage 2 is the first output that is a fixed point.
This is D-202's lesson in a second place: the criterion is that the compiler
AGREES WITH ITSELF, never that two particular artifacts are byte-equal.

Write the sha256 and the commit into `STAMP`. The harness's `repro` stage reads
`stage1.ll` back and asserts it still matches a fresh emission, so a snapshot
left stale fails the suite rather than rotting quietly.

## Why the IR is committed rather than regenerated

D-085: rebuilding from nothing must need only the LLVM toolchain. Regenerating
this file requires the Python generator, so a build that regenerated it would
depend on Python — which is exactly the dependency the artifact may not have.
The generator survives as the tool that made the first one, and as the audit
path for pre-1.4.6 history: check out the pre-switch commit, regenerate, build
forward.

The file is large (15 MB of text) and it is meant to be. It is read by `llc`,
diffed by the harness, and reviewed by nobody line by line — its integrity
claim is the STAMP's sha256 and the fixpoint that re-derives it, not human
reading. Diverse double-compilation remains the Thompson-attack mitigation
D-085 records.
