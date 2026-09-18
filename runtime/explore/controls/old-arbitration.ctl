; old-arbitration -- `trap-route`'s control of the same name (two-failsafes), applied to the floor,
; DIRECTED: the holder word read in one step and written in another, and -- as the model's `win`
; step does -- no stop walk, so two trapping threads can both read zero, both claim, and both run
; `failsafe`. `trap_two_threads` cannot see two `failsafe`s (both exit 41); `trap_one_failsafe`'s
; `failsafe` reads a pipe for a byte the earlier `failsafe` wrote and exits 77 on finding one. The
; window between one claimant's read and its write is one point wide, so the control names the
; write: each claimant yields after its read, before its write. Blind: 0 of 100 seeds.
;
; AND THE FROZEN STORE (DEF-57, 2026-09-18). With the write alone directed, 100 of 100 seeds were
; found from seed 1 -- but the second claimant was not a trapper: it was a thread whose task had not
; started, and whose executor saw the flag and trapped `Unreachable` on it, which is DEF-57's own
; window. With DEF-57 fixed that executor parks, and the write alone found nothing in 10 seeds. Two
; threads trap only when the second one's task is RUNNING when the first traps, so the control also
; names the first trapper's frozen store: it yields BEFORE the flag is published, and the other
; thread's task starts, runs and traps as well. Measured on the fixed floor: see the README.
program: tests/backend/programs/trap_one_failsafe.npk
verdict: wrong-exit
within: 10
preempt-at: @npk_trap store atomic i32 1, ptr @npk_frozen
preempt-at: @npk_trap store atomic i64 %selfv, ptr @npk_in_failsafe
old:
  %cx = cmpxchg ptr @npk_in_failsafe, i64 0, i64 %selfv seq_cst seq_cst
  %won = extractvalue { i64, i1 } %cx, 1
  br i1 %won, label %run, label %lost
new:
  %holder = load atomic i64, ptr @npk_in_failsafe seq_cst, align 8
  %won = icmp eq i64 %holder, 0
  br i1 %won, label %run, label %lost
old:
  %holder = extractvalue { i64, i1 } %cx, 0
  %mine = icmp eq i64 %holder, %selfv
new:
  %mine = icmp eq i64 %holder, %selfv
old:
  call void @npk_stop_others(ptr %self)
new:
  store atomic i64 %selfv, ptr @npk_in_failsafe seq_cst, align 8
