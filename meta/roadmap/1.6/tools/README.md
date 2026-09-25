# The 1.6.0 gate's tools — committed, outside every gate, built into nothing

**What this is.** The scripts and notes the bring-up gate of cycle 1.6
(`../1.6.0.md`) runs and is measured with: the instruction census of an
emission (`census.py`), the probe log the plan's §1 was written from
(`PROBES_2026-09-25.md`), and — as the steps land — the pinned builds of the
three engines (`engines.sh`, writing `pins.txt`), the input set
(`gate_inputs.py`), the runs (`gate_run.py`), the planted-defect controls
(`controls/`) and Alive2's recorded patch (`alive2-rlimit.patch`, D-321).
The precedent is `../../done/1.5/tools/`: planning and measurement tools
kept beside the plan they served, so a number in a record can be re-derived.

**What it is not.** Nothing here is built into the compiler, the runtime,
`npkg` or the harness; no gate of the tree invokes it; the zero-dependency
rule governs the artifact and its gates, and these are Python and shell on
the workbench (D-233's doctrine: the tools that check the artifact are
pinned and auditable, never part of it). The engines themselves live
OUTSIDE the tree under `~/.local/src/1.6/<engine>-<sha7>/`, cloned at the
commits `../1.6.0.md` §2.1 records and built by `engines.sh`; what the tree
keeps is the commit, the build recipe and the sha256 of every binary and
library the gate ran (`pins.txt`, P-4). 1.6.1's pinned-tool table in
`nitpick.toml` inherits those rows.

**How to run what exists.**

```
python3 meta/roadmap/1.6/tools/census.py build/npkc.ll            # opcodes, intrinsics, attributes, dbg/datalayout
python3 meta/roadmap/1.6/tools/census.py --assemble FILE.ll       # plus: which of llvm-as-14/18/20 accepts it
```

Every number in the plan's §1 has its command in `PROBES_2026-09-25.md`;
a number you cannot re-derive from a command there is one to distrust.
