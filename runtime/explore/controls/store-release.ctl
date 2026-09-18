; store-release -- `futex-mutex`'s control of the same name, applied to the floor: `npk_mx_unlock`
; releases the word and never wakes the waiter the contended state promised a wake to. The waiter's
; word changed under it (2 -> 0) and nobody woke it: LOST-FUTEX-WAKE at quiescence. The prototype
; measured it at seed 1 (268 of 300 seeds; stress on the same floor: 29 of 40 runs, 59 s).
program: tests/backend/programs/mutex_basic.npk
verdict: LOST-FUTEX-WAKE
within: 20
old:
  br i1 %contended, label %wake, label %done
new:
  br label %done
