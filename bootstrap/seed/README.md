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

The rule has a mirror direction, found at 1.4.7 and recorded here at the 1.4
close: **a fix in the compiler's BACKEND does not reach the tools until the
snapshot carries it.** The harness compiles `tools/check.npk`, `parse_check`
and `resolve_check` with the SNAPSHOT — the builder — so the tools' own
source (the frontend, `src/frontend/`) is always the current tree while
their binaries are emitted by the old backend. A checker rule added in
`src/` is in the built tools at once; an emitter fix is not (measured under
D-225: the npkc-built checker exited 0 and the snapshot-built one 3, same
sources). When a step needs an emitter fix visible to the tools, refresh at
the previous commit — the mid-cycle refreshes of 1.4.7 (D-224, D-225) and
1.4.8 step 3 are the precedents.

## Refreshing it

At cycle closes, with the push. The refresh is done BY THE SNAPSHOT, never by
the Python generator — that is what "the seed retires" means.

```sh
# 1. the floor, and the builder from the snapshot you have
llc -O0 -filetype=obj -relocation-model=static runtime/npkrt.ll -o /tmp/npkrt.o
llc -O0 -filetype=obj -relocation-model=static bootstrap/seed/stage1.ll -o /tmp/b.o
ld.lld -static -o /tmp/builder /tmp/b.o /tmp/npkrt.o

# 2. the builder compiles the CURRENT compiler
/tmp/builder src/npkc.npk -o /tmp/stage1.new.ll

# 3. THE FIXPOINT: stage2 == stage3, and INSTALL STAGE 2
llc -O0 -filetype=obj -relocation-model=static /tmp/stage1.new.ll -o /tmp/s2.o
ld.lld -static -o /tmp/npkc2 /tmp/s2.o /tmp/npkrt.o
/tmp/npkc2 src/npkc.npk > /tmp/stage2.ll
llc -O0 -filetype=obj -relocation-model=static /tmp/stage2.ll -o /tmp/s3.o
ld.lld -static -o /tmp/npkc3 /tmp/s3.o /tmp/npkrt.o
/tmp/npkc3 src/npkc.npk > /tmp/stage3.ll
cmp /tmp/stage2.ll /tmp/stage3.ll            # MUST be silent

# 4. install, and stamp what you installed
cp /tmp/stage2.ll bootstrap/seed/stage1.ll
sha256sum bootstrap/seed/stage1.ll
git rev-parse HEAD
```

Step 3 is not optional. A snapshot that compiles the compiler but whose output
does not rebuild itself is a snapshot that works exactly once, and the next
refresh from it produces something else again.

**The fixpoint is not the proof; the refresh's harness is (1.5.1b step 5c).**
Step 3's `stage2 == stage3` says the compiler compiles ITSELF consistently. It
says nothing about whether the tools and the compiler the NEW snapshot builds
BEHAVE as the old builder's did — a semantic change in the emitter (a moved-out
field left vacant, a temporary dropped at its statement's end) can leave every
byte of the fixpoint in place and change what `tools/check.npk` reports. The
first refresh after 1.5.1b's step 5 did exactly that: five rejection tests
changed verdict under the refreshed snapshot while every program stage was
green, because a sort in the compiler's own diagnostics read slots it had moved
out of. So a refresh is landed only under a full harness run WITH the refreshed
snapshot installed — that run is the first time the new compiler's semantics
compile the suite's tools — and a red there is a `src/` defect written against
the old semantics, never a reason to keep the old snapshot.

### The bridging variant: a prelude addition `src/` already uses (1.5.1b step 5b)

The prelude is EMBEDDED in a compiler when that compiler is built
(`prelude_source.npk` is a string constant), so a compiler resolves prelude
names against the prelude it was built with, never against the tree it is
compiling. A name the prelude gains that `src/` ALREADY uses through one of
its own modules — `List<T>` at 1.5.1b step 5b — therefore has no single tree
that both compiles under the committed snapshot and stops declaring the name:
the snapshot's prelude lacks it, and D-239 refuses the module's declaration
the moment a compiler's prelude owns it. The refresh runs in two hops inside
ONE commit's preparation:

1. **The bridge, never committed.** The prelude carries the addition AND the
   module still declares it. The committed snapshot compiles this tree (its
   own prelude lacks the name, so nothing is refused; the module supplies it)
   into a compiler whose embedded prelude HAS the name.
2. **The commit.** The module's declaration and every `use` of it are gone.
   The bridge compiler compiles this tree; the result rebuilds itself
   byte-identically (step 3 above, unchanged); it is installed with a STAMP
   whose `source-commit` line says so — the snapshot and the `src/` it builds
   land together, so the line names the commit's own tree, and the sha256 is
   the claim as always.

The 1.4.8 flag-families refresh was the one-hop form of the same rule:
`src/` began using the families only after a snapshot carried them.

**Run it from the tree root with `src/npkc.npk` spelled relatively, exactly as
written.** Until 1.4.8 D-179's site table recorded each source path AS GIVEN,
so a builder handed an absolute path embedded the machine's path into every
one of its ~1,500 site constants — and neither the fixpoint nor the STAMP
noticed, since each compares the emission with itself (found at 1.4.7's close
by doing exactly that in a dry run: 1,489 of 1,647 rows absolute). **D-236
closed that at 1.4.8**: every path now renders relative to the manifest root
whatever the argument's spelling, so an absolute invocation emits the same
bytes. The relative spelling stays the documented form, the `repro` stage
still refuses a snapshot whose site table carries an absolute path, and the
`selfhost` stage asserts the same of every fresh emission — belts over a
property the source manager now holds by construction.

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
`stage1.ll` back and asserts that it matches its STAMP (sha256 and byte count)
and that its site table carries no absolute path; that the snapshot still
BUILDS `src/` is the run's first act. It does NOT assert byte-equality with a
fresh emission — between refreshes the snapshot is legitimately older than
`src/` (D-205), and D-202's fixpoint compares two current-source emissions
precisely so a stale builder is tolerated. (This paragraph said otherwise until
1.4.7's close; 1.4.6 had corrected the stage and not the sentence.)

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
