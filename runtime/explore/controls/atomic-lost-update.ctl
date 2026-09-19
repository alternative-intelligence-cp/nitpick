; atomic-lost-update -- a PROGRAM control (1.5.7 step 6; X-21): the defect is planted in the program's
; SOURCE, not the floor. `atomic_threads` counts to 400 with one `fetch_add` per increment -- one
; read-modify-write, one step. Planted: the increment as a `load` and then a `store`, two atomic steps
; with a window between them, so a thread preempted there writes back a stale value and an increment
; is lost. The explorer sees it only through the points step 6 puts in the PROGRAM's own IR: without
; them, each thread's loop runs from one floor step to the next with no scheduling point inside it, and
; no schedule can split a load from its store. The program's answer is exit 1 (a total short of 400).
program: tests/backend/programs/atomic_threads.npk
verdict: wrong-exit
within: 10
program-old:
        discard(atomic_from_ptr::<int64>(cell).fetch_add(1i64));
program-new:
        int64:v = atomic_from_ptr::<int64>(cell).load();
        atomic_from_ptr::<int64>(cell).store(v + 1i64);
