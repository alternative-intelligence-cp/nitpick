; old-loser-exits -- `trap-route`'s control of the same name (exit-mid-failsafe), applied to the
; floor, DIRECTED: a second trapping thread that lost the arbitration takes the re-entry arm and
; `exit_group(70)`s the process out from under the holder's `failsafe` -- what `trap_two_threads`
; exited on the pre-D-291 floor. On this floor the winner's stop walk parks the loser before it can
; trap, so the loser's arm is reached only when the second trap lands between the winner's claim and
; its stop: the control names the stop walk's first registry read, so the winner yields there and
; the second thread traps first. Blind: 0 of 100 seeds (a window one point wide in a 106,000-step
; run).
;
; AND THE FROZEN STORE (DEF-57, 2026-09-18). With the stop walk alone directed, 100 of 100 seeds were
; found from seed 1 -- but the loser was not a trapper: it was a thread whose task had not started,
; and whose executor saw the flag and trapped `Unreachable` on it, which is DEF-57's own window. With
; DEF-57 fixed that executor parks, and the stop walk alone found nothing in 10 seeds. The loser must
; be a thread whose task is RUNNING when the winner traps, so the control also names the frozen
; store: the first trapper yields before the flag is published, and the other thread's task starts
; and runs to its own trap. That thread arrives at the same store and is held there in turn; the
; winner claims and yields at its stop walk, and the second thread then publishes, loses and takes the
; planted arm. Measured on the fixed floor: see the README.
program: tests/backend/programs/trap_two_threads.npk
verdict: wrong-exit
within: 10
preempt-at: @npk_trap store atomic i32 1, ptr @npk_frozen
preempt-at: @npk_stop_others %st = load atomic i64, ptr %sp acquire
old:
  br i1 %mine, label %hard, label %park
new:
  br label %hard
