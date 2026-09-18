; heap-mutex-in-failsafe -- `trap-route`'s control of the same name (failsafe-blocked-on-heap),
; applied to the floor: the pre-D-292 allocator, `failsafe`'s allocations taken from the heap under
; its mutex instead of the failsafe region. `failsafe_alloc`'s churner allocates and frees in a tight
; loop, the stop parks it where it stands -- inside the allocator, holding the mutex, when the stop
; lands there -- and `failsafe`'s 64 KiB string then waits on that mutex with no deadline: the shim's
; DEADLOCK at quiescence (nobody can step, no deadline), which `wrong-exit` counts. Measured: 36 of
; 100 seeds, from seed 1 -- and 0 of 100 before X-16, when the fixed fairness bound made the churner
; (period three) rest before its LOCK on every slice of every seed, never inside the mutex.
program: tests/backend/programs/failsafe_alloc.npk
verdict: wrong-exit
within: 40
old:
  br i1 %infs, label %region, label %heap
new:
  br label %heap
