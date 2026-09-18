; link-without-cas -- `shared-arena`'s control of the same name (unlinked-forever), applied to the
; floor, DIRECTED: a grower links its chunk at the head by a cmpxchg loop; as a plain store, a
; concurrent grower's chunk is overwritten and unreachable from the head, and every later lookup of
; an index in its range walks the list forever (`npk_sarena_slot` re-walks from the head until the
; chunk appears) -- the shim's STEP BUDGET verdict, which `wrong-exit` counts. Two growers at ONE
; capacity boundary with one push inside the other's is a depth-3 interleaving of one-step windows:
; blind PCT found it in 0 of 500 seeds on this program and 0 of 500 on a four-filler variant, so the
; control names the two sites AFTER its windows (a directed site takes effect before the named
; instruction): every grower yields after reading the capacity, before reserving, and again after
; reading the head, before storing. Measured: 27 of 100 seeds, first at 4 (11 of 40 under the final shim); a
; found seed spins to the budget (about 30 s), the rest take milliseconds.
program: tests/backend/programs/shared_arena_race.npk
verdict: wrong-exit
within: 30
preempt-at: @npk_sarena_bump %base = atomicrmw add ptr %cp
preempt-at: @npk_sarena_bump store atomic i64 %ci, ptr %hp
old:
  %pair = cmpxchg ptr %hp, i64 %old, i64 %ci seq_cst seq_cst
  %okc = extractvalue { i64, i1 } %pair, 1
  br i1 %okc, label %check, label %push
new:
  store atomic i64 %ci, ptr %hp seq_cst, align 8
  br label %check
