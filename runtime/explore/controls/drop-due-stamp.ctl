; drop-due-stamp -- `reactor-io`'s control of the same name (event-consumed-task-asleep), applied to
; the floor: `npk_park_sleep`'s delivery loop reads the event whose payload is the waiting frame and
; stamps it due. Dropped, the one-shot has fired and disarmed, the event is consumed, and the task
; sleeps to its deadline: `io_ready_basic`'s late writer makes the pipe readable at 40 ms, the wait's
; deadline is 1 s, and the program exits 82 (EWait) instead of 42. Measured: 50 of 50 seeds, from seed
; 1 -- every schedule loses the event, so this control tests the lens, not the scheduler.
program: tests/backend/programs/io_ready_basic.npk
verdict: wrong-exit
within: 10
old:
  %wa = getelementptr %npk.hdr, ptr %frp, i32 0, i32 8
  store atomic i64 1, ptr %wa seq_cst, align 8
new:
  %wa = getelementptr %npk.hdr, ptr %frp, i32 0, i32 8
