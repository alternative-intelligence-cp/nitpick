; no-rouse -- the ABSORBED NOTIFICATION (r6's tokio#2057; `park-unpark`'s `absorbed-notification`):
; `npk_ch_wake_one` stamps the woken task due and never rouses the executor that sleeps on it. The
; program still exits right, late -- every wait has a deadline -- so nothing but the quiescence
; oracle sees it: LOST-WAKE. The prototype measured `nested_wait` at 100 of 100 seeds by the oracle,
; 0 of 100 by exit code, 0 of 10 stress runs; `thread_root_wake` here answers at seed 1.
program: tests/backend/programs/thread_root_wake.npk
verdict: LOST-WAKE
within: 20
old:
  br i1 %noown, label %done, label %rouse
new:
  br label %done
