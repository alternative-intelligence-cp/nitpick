# The explorer's negative controls (1.5.7 step 3; X-10, D-301)

An instrument that claims to find lost wakeups is worth nothing until it is shown FINDING one. Each
`.ctl` here plants a defect in the floor by a text substitution on `runtime/npkrt.ll` (`old:` lines,
which must occur exactly once, replaced by `new:` lines), names a program, the VERDICT the explorer
must reach and within how many seeds. Both runners build the patched floor through the one
transformer, run the program under the seeds, and REQUIRE the verdict — a control the explorer is
blind to is a red run by name (`explore-control-blind`), the models' rule (`floor-control-blind`)
applied to the explorer. The patched floor never leaves the run's build directory.

The two landed with the mechanism are the two the planning prototype measured: `store-release` (a
mutex released without its wake — `LOST-FUTEX-WAKE`) and `no-rouse` (the absorbed notification —
`LOST-WAKE`). Step 4 walks the models' nineteen controls: each becomes a `.ctl`, or is recorded
below as NOT A FLOOR BUG with the reason and the measurement — which is also a finding about the
model, recorded in its file.

## Model controls measured and found not to be floor bugs

(none yet — step 4)
