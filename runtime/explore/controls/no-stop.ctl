; no-stop -- `trap-route`'s control of the same name (step-after-failsafe), applied to the floor: the
; winner of the failsafe arbitration enters `failsafe` without stopping the other threads (the
; pre-D-291 route). `trap_stops_runner`'s runner keeps running its task while `failsafe` runs, which
; the program was written to see: it exits other than 43. Measured: 100 of 100 seeds, from seed 1.
program: tests/backend/programs/trap_stops_runner.npk
verdict: wrong-exit
within: 10
old:
  call void @npk_stop_others(ptr %self)
new:
  ; the stop dropped (control)
