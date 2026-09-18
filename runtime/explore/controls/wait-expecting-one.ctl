; wait-expecting-one -- `futex-mutex`'s control of the same name, applied to the floor: the waiter
; marks the word 1 instead of 2 and waits expecting 1. The release exchanges 1 for 0, sees no
; contention, wakes nobody; the waiter sleeps on a word that is now 0 -- the value it waited on is
; gone and no wake is coming: LOST-FUTEX-WAKE at quiescence. Two lines, seven apart, in
; `npk_mx_lock`'s loop and wait blocks (the first control to need two pairs).
program: tests/backend/programs/mutex_basic.npk
verdict: LOST-FUTEX-WAKE
within: 20
old:
  %old = atomicrmw xchg ptr %w, i32 2 seq_cst
new:
  %old = atomicrmw xchg ptr %w, i32 1 seq_cst
old:
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 128, i64 2, i64 0, i64 0, i64 0)
new:
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 128, i64 1, i64 0, i64 0, i64 0)
