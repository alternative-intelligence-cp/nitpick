; drop-keep-path -- `park-unpark`'s control of the same name (spent-marker), applied to the floor:
; `npk_sl_push` exchanges the deadline into `wake_at` and, when the exchange returns the due mark a
; waker stamped between the channel link and the push, puts the mark back. Dropped, the mark is
; OVERWRITTEN by the deadline: the task sleeps to its deadline for a value that has already arrived,
; the program's answer is still right, and only the virtual clock sees it -- `channel_threads` exits
; 100 on every schedule and takes 0.6 ms of virtual time on the clean floor, 2 s (a recv deadline)
; when the mark is lost. The stamp oracle cannot see this one (the stamp is gone); the lateness lens
; is what D-301 named. Measured: 62 of 1000 seeds, first at 5 (a window one point wide, hit only when
; the sender and the receiver race without a sleep between them).
program: tests/backend/programs/channel_threads.npk
verdict: late 100000000
within: 150
old:
  br i1 %woken, label %keep, label %sleep
new:
  br label %sleep
