; frozen-traps -- `trap-route`'s control of the same name, applied to the floor, HELD (1.5.8 step 2c; DEF-57,
; DEF-67): the `frozen:` block of `npk_step` as it was before DEF-57's fix. An executor that sees the frozen flag
; traps `Unreachable` itself instead of parking, and so competes for the failsafe holder. In the window between a
; trapper's frozen store and its claim, the watcher can WIN, and `failsafe` runs with `Unreachable` where the fault
; was the trapper's -- exit 72 in `trap_one_failsafe`, the arm DEF-57's schedule named.
;
; The window is one point wide, between two arrivals at ONE site, the claim. A directed site cannot reverse them:
; it demotes each arrival below the last, so the trapper, which arrived first, claims first. 1.5.7 met the window
; blind once, at seed 371, and step 2 of 1.5.8 moved every schedule. With the pre-fix block planted, seeds 1..60,000
; then all exited 41, and the kept seed had gone stale with no signal (DEF-67). So the control HOLDS the first
; thread to arrive at the claim, until another thread passes it. That is the trapper, whose frozen store is
; already published. Every other executor that starts a step meanwhile reads the flag, traps and passes the claim
; first. Measured: see the README.
program: tests/backend/programs/trap_one_failsafe.npk
verdict: exit 72
within: 10
hold-at: @npk_trap %cx = cmpxchg ptr @npk_in_failsafe
old:
  %fzh = load atomic i64, ptr @npk_in_failsafe seq_cst, align 8
  %fzself = call ptr @npk_tls_self()
  %fzselfv = ptrtoint ptr %fzself to i64
  %fzmine = icmp eq i64 %fzh, %fzselfv
  br i1 %fzmine, label %fzhold, label %fzpark
new:
  br label %fzhold
