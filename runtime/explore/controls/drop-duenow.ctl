; drop-duenow -- `reactor-io`'s control of the same name (declined-but-asleep), applied to the floor:
; when `epoll_ctl` declines a watch (EPERM: a regular file, always ready by definition; EBADF),
; `npk_io_register`'s `duenow` block stamps the waiting frame due so the caller retries at once.
; Dropped, the task sleeps to its deadline on a descriptor nothing will ever report:
; `io_ready_declined` waits on its own executable with a half-second deadline and requires the wait
; to end inside 100 ms -- it exits 10 instead of 42 (the verdict word: any exit but the program's).
; Measured: 50 of 50 seeds, from seed 1, the virtual clock at exactly the deadline.
program: tests/backend/programs/io_ready_declined.npk
verdict: wrong-exit
within: 10
old:
  store atomic i64 1, ptr %dnp seq_cst, align 8
new:
  ; the declined watch's due stamp dropped (control)
