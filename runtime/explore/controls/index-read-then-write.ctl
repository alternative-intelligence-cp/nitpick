; index-read-then-write -- `shared-arena`'s control of the same name (duplicate-index), applied to
; the floor: `npk_sarena_bump` reserves an index by ONE atomic add of `top`; as an atomic read, an
; add and an atomic write, two bumpers can be handed one index and the later writer's value lands in
; the earlier's slot. `shared_arena_race` keeps every handle and re-reads all of them once both
; fillers are done (a two-party barrier), so a foreign value is `!!! E9`, exit 91 instead of 42 --
; `shared_arena_spawn`, which checks each value right after its alloc, never sees it. Measured: 51 of
; 500 seeds, first at 24. (Both halves are atomic steps so the explorer can interleave them: a plain
; load and store would have no scheduling point between them.)
program: tests/backend/programs/shared_arena_race.npk
verdict: wrong-exit
within: 150
old:
  %idx = atomicrmw add ptr %tp, i64 1 seq_cst
new:
  %idx = load atomic i64, ptr %tp seq_cst, align 8
  %idx1 = add i64 %idx, 1
  store atomic i64 %idx1, ptr %tp seq_cst, align 8
