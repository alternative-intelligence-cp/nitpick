; The Nitpick runtime floor.
;
; PERMANENT, and hand-written LLVM IR ON PURPOSE (D-203). This file said
; THROWAWAY "alongside the seed" for fifteen cycles, and D-015's first-rung
; story -- runtime symbols start as hand-written IR and are replaced at a later
; rung -- described a replacement that is not coming. What this project is
; removing is the C and C++ layer; LLVM was never the enemy. This is linked
; into every artifact including the one that ships, it is inside the D-015 TCB,
; and reviewed IR is the form a verifier reads.
;
; It re-homed from `bootstrap/runtime/` to `runtime/` at the 1.4.6 switch, for
; the same reason: nothing in here was ever bootstrap material, and its address
; had been saying otherwise since cycle 0.
;
; Freestanding: no libc, no crt. `_start` is provided here, and every kernel
; call is a raw syscall.

target triple = "x86_64-unknown-linux-gnu"

; ---------------------------------------------------------------------------
; Entry point.
;
; Written as module-level assembly rather than as an LLVM function on purpose:
; at _start the stack is NOT laid out the way it is at a normal call -- there is
; no return address -- so a compiler-generated prologue can leave %rsp
; misaligned and any later SSE spill faults. Aligning by hand sidesteps the
; whole class.
; ---------------------------------------------------------------------------

; --- the two shapes every other definition here refers to -------------------
;
; Declared FIRST because LLVM sizes a `getelementptr null` at parse time: a
; type used before it is defined is "base element must be sized", which the
; TLS boot found by being the earliest user.
;
; The child's trampoline block, which `%fs` points at:
;   [ 0 self | 1 exec | 2 root frame | 3 resume fn | 4 tid word ]
%npk.tls = type { ptr, ptr, ptr, ptr, i32 }

; THE CLONE TRAMPOLINE (D-181). In assembly for one reason: after the
; syscall the child runs on a DIFFERENT STACK, so it may not return into
; compiler-laid-out frame offsets that belong to the parent. It pops the
; entry function and its argument off the child stack (written there before
; the syscall), calls with a correctly aligned %rsp, and exits THIS THREAD
; on return — which is what clears the CHILD_CLEARTID word and wakes the
; joiner. The parent just returns the tid.
;   rdi=flags rsi=child_stack_top rdx=ctid rcx=tls r8=fn r9=arg
module asm ".globl npk_clone_raw"
module asm "npk_clone_raw:"
module asm "  subq $16, %rsi"
module asm "  movq %r8, 0(%rsi)"
module asm "  movq %r9, 8(%rsi)"
module asm "  movq %rdx, %r10"
module asm "  movq %rcx, %r8"
module asm "  movl $56, %eax"
module asm "  syscall"
module asm "  testq %rax, %rax"
module asm "  jz 1f"
module asm "  ret"
module asm "1:"
module asm "  xorl %ebp, %ebp"
module asm "  popq %r11"
module asm "  popq %rdi"
module asm "  callq *%r11"
module asm "  xorl %edi, %edi"
module asm "  movl $60, %eax"
module asm "  syscall"
module asm "  hlt"

module asm ".globl _start"
module asm "_start:"
module asm "  xorq %rbp, %rbp"
module asm "  movq %rsp, %rdi"         ; the ORIGINAL rsp, before alignment
module asm "  andq $-16, %rsp"
module asm "  callq npk_start"
module asm "  hlt"

; ---------------------------------------------------------------------------
; The process boundary.
;
; `main` takes `cstring[]:argv` and nothing else (D-089): a slice carries its
; length (D-070), so there is no argc to pass and no second copy of a fact to
; disagree with the first.
;
; At _start the stack holds argc, then argv[0..argc-1], then a NULL, then envp.
; That layout is only reachable through the ORIGINAL %rsp, which is why the
; prologue above captures it into %rdi BEFORE the alignment that destroys it.
;
; This runs before `main`, so it allocates the cstring array from the heap
; (0.10.0) and never frees it -- argv outlives everything regardless, and the
; first allocation is also what draws the heap secret.
; ---------------------------------------------------------------------------

declare i32 @main({ ptr, i64 })

; A NUL-terminated pointer array -- argv, envp -- as the `cstring[]` slice the
; language sees. Every entry's length is measured exactly once, HERE, at the
; boundary (D-049): a cstring carries its length, and nothing downstream scans
; for a NUL again. ONE builder for both (1.4.8, D-206): the environment used to
; be left on the stack here, and `npkg` needs `PATH`.
;
; THE ARRAY IS ITS OWN MAPPING, never a heap block (1.5.1b step 0). It was an
; internal heap allocation "never freed" -- but `wild_release_all` unmaps
; every chunk wholesale, so a program that released and then read its own
; argv (npkc's `main` releases before every exit; a `failsafe` may do both)
; read unmapped memory: found by this step's exit-time report, which walked
; the environment after the compiler's release and faulted. A page-rounded
; `npk_hmap` outside the chunk and large tables is what "outlives everything"
; actually requires. The strings themselves are the kernel's, on the stack.
define internal { ptr, i64 } @npk_cstr_slice(ptr %base) {
entry:
  br label %count

count:                                            ; preds = %entry, %count
  %n = phi i64 [ 0, %entry ], [ %n1, %count ]
  %ep = getelementptr ptr, ptr %base, i64 %n
  %e = load ptr, ptr %ep
  %n1 = add i64 %n, 1
  %atend = icmp eq ptr %e, null
  br i1 %atend, label %sized, label %count

sized:                                            ; preds = %count
  %bytes = mul i64 %n, 16                         ; sizeof(cstring) = {ptr,i64}
  %b4095 = add i64 %bytes, 4095
  %msz0 = and i64 %b4095, -4096
  %empty = icmp eq i64 %msz0, 0
  %msz = select i1 %empty, i64 4096, i64 %msz0    ; an empty array is still a page
  %bufa = call i64 @npk_hmap(i64 %msz)
  %buf = inttoptr i64 %bufa to ptr
  br label %loop

loop:                                             ; preds = %sized, %next
  %i = phi i64 [ 0, %sized ], [ %i1, %next ]
  %done = icmp uge i64 %i, %n
  br i1 %done, label %ready, label %body

body:                                             ; preds = %loop
  %slotp = getelementptr ptr, ptr %base, i64 %i
  %s = load ptr, ptr %slotp
  br label %slen

slen:                                             ; preds = %body, %slen
  %k = phi i64 [ 0, %body ], [ %k1, %slen ]
  %cp = getelementptr i8, ptr %s, i64 %k
  %ch = load i8, ptr %cp
  %k1 = add i64 %k, 1
  %atnul = icmp eq i8 %ch, 0
  br i1 %atnul, label %store, label %slen

store:                                            ; preds = %slen
  %pf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 0
  store ptr %s, ptr %pf
  %lf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 1
  store i64 %k, ptr %lf
  br label %next

next:                                             ; preds = %store
  %i1 = add i64 %i, 1
  br label %loop

ready:                                            ; preds = %loop
  %sl0 = insertvalue { ptr, i64 } zeroinitializer, ptr %buf, 0
  %sl1 = insertvalue { ptr, i64 } %sl0, i64 %n, 1
  ret { ptr, i64 } %sl1
}

; THE ENVIRONMENT, kept from `_start` for `environ()` (1.4.8, D-206). Written
; once before `main` and read-only after, so no thread ever races it.
@npk_environ_slice = internal global { ptr, i64 } zeroinitializer

; NPK_HEAP_STATS IS DECIDED HERE, before `main`, by one walk of the kernel's
; raw envp on the stack (1.5.1b step 0): the report at `npk_exit` then reads
; a flag and touches no memory the program could have released. The name is
; matched exactly -- `NPK_HEAP_STATS` alone or `NPK_HEAP_STATS=<anything>` --
; so no value spelling is a second way to switch it on.
@npk_hs_on = internal global i64 0
@npk_hs_key = internal constant [14 x i8] c"NPK_HEAP_STATS"

define internal void @npk_hs_arm(ptr %base) {
entry:
  br label %scan
scan:
  %i = phi i64 [ 0, %entry ], [ %i1, %next ]
  %ep = getelementptr ptr, ptr %base, i64 %i
  %s = load ptr, ptr %ep
  %atend = icmp eq ptr %s, null
  br i1 %atend, label %done, label %cmp
cmp:
  %k = phi i64 [ 0, %scan ], [ %k1, %cmpstep ]
  %kdone = icmp eq i64 %k, 14
  br i1 %kdone, label %tail, label %cmpstep
cmpstep:
  ; the key has no NUL, so a shorter entry mismatches at its NUL and no byte
  ; past an entry's end is ever read
  %cp = getelementptr i8, ptr %s, i64 %k
  %c = load i8, ptr %cp
  %kp = getelementptr [14 x i8], ptr @npk_hs_key, i64 0, i64 %k
  %kc = load i8, ptr %kp
  %eq = icmp eq i8 %c, %kc
  %k1 = add i64 %k, 1
  br i1 %eq, label %cmp, label %next
tail:
  %tp = getelementptr i8, ptr %s, i64 14
  %tc = load i8, ptr %tp
  %isnul = icmp eq i8 %tc, 0
  %iseq = icmp eq i8 %tc, 61
  %hit = or i1 %isnul, %iseq
  br i1 %hit, label %arm, label %next
arm:
  store i64 1, ptr @npk_hs_on
  br label %done
next:
  %i1 = add i64 %i, 1
  br label %scan
done:
  ret void
}

define internal void @npk_start(i64 %sp) noreturn {
entry:
  ; the executor's TLS block first: every allocation, trap and error chain
  ; below reaches its executor through `%fs:8` (D-181).
  call void @npk_tls_boot()
  %spp = inttoptr i64 %sp to ptr
  %argc = load i64, ptr %spp
  %argvp = getelementptr i8, ptr %spp, i64 8      ; &argv[0]
  %argv = call { ptr, i64 } @npk_cstr_slice(ptr %argvp)
  ; envp starts one slot past argv's NULL terminator: argv[argc] is that NULL.
  %envoff = add i64 %argc, 1
  %envpp = getelementptr ptr, ptr %argvp, i64 %envoff
  %env = call { ptr, i64 } @npk_cstr_slice(ptr %envpp)
  store { ptr, i64 } %env, ptr @npk_environ_slice
  call void @npk_hs_arm(ptr %envpp)
  %rc = call i32 @main({ ptr, i64 } %argv)
  call void @npk_exit(i32 %rc)
  unreachable
}

; `environ() -> cstring[]`: the process environment as the kernel handed it
; over, `KEY=VALUE` entries measured at `_start`. D-089 keeps `main`'s
; signature to `argv` alone; the environment is the other half of the same
; stack and is reached by asking. The block is the kernel's, never freed.
define { ptr, i64 } @npk_environ() {
entry:
  %e = load { ptr, i64 }, ptr @npk_environ_slice
  ret { ptr, i64 } %e
}

; ---------------------------------------------------------------------------
; Raw syscall
; ---------------------------------------------------------------------------

; PUBLIC since 0.9.7: the `sys` builtin calls straight through it.
define i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,
                              i64 %a4, i64 %a5, i64 %a6) {
  %r = call i64 asm sideeffect "syscall",
       "={ax},{ax},{di},{si},{dx},{r10},{r8},{r9},~{rcx},~{r11},~{memory},~{dirflag},~{fpsr},~{flags}"
       (i64 %nr, i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6)
  ret i64 %r
}

; --- the executor (D-071/D-083, 1.1.8) --------------------------------------
;
; ONE EXECUTOR PER THREAD, and blocking is always task suspension (D-071):
; a task that waits suspends, and the thread parks only when it has nothing
; runnable left. The state is a few words of thread-local — until threads
; exist (1.1.9+) that is process-global, which is the same thing on one
; thread and keeps the shape honest for the move.
;
; THE PARK REQUEST is how a suspension says WHY. The deepest suspending
; primitive writes it as it returns SUSPENDED (`npk_park_until`), the run
; loop reads it the instant the resume returns: one word, no frame-layout
; change, and no way for a suspension to be silent about its wake condition.
; A suspension with no request is a defect — nothing may park forever.
@npk_frozen = internal global i32 0         ; D-063: a trap happened; resume nothing

; The FUTEX word the thread sleeps on. Nothing ever wakes it in a
; single-threaded program — the timeout is the wake — but the wait is a real
; FUTEX_WAIT_BITSET so a cross-thread wake (1.1.9) needs no new mechanism.

; THE FRAME HEADER, AS THE EXECUTOR SEES IT. Must match the `%"npk.frame.hdr"`
; the emitter writes (emit_runtime_declares) — one shape, two spellings, and
; the harness's runtime-signature check is what keeps them honest.
;   0 resume_fn | 1 state | 2 windup | 3 result | 4 join_head
;   5 sibling   | 6 awaitee | 7 qnext | 8 wake_at
; 9 thread_tls: non-null exactly when this frame is a THREAD's root task
; (D-181). The join reads it to know whether to pump the executor (a task) or
; wait on the kernel's CHILD_CLEARTID word (a thread) -- a dedicated slot
; rather than an overload of `qnext`, because an ambiguous discriminator in a
; join is where a program waits on the wrong thing forever.
; 10 chan_next, 11 owner: the channel WAIT (1.1.10-C2). A task blocked on a
; channel is on TWO lists at once, which is why this is a second link rather
; than a reuse of `qnext`: its own executor's sleepers, holding the deadline
; that bounds the wait, and the channel's waiter list, shared across threads,
; which is how a peer operation wakes it early. `owner` is the executor the
; task is pinned to (D-032) -- the waker has to reach it, and no other slot
; answers that: `thread_tls` is set only on a thread's root task.
%npk.hdr = type { ptr, i32, i32, { i32 }, ptr, ptr, ptr, ptr, i64, ptr, ptr, ptr }

; THE EXECUTOR (D-181): every word 1.1.8 kept as a runtime global, in one
; struct, because with threads each of them is PER-THREAD — two threads
; sharing a ready queue would migrate tasks, which D-032 forbids outright;
; sharing a park word would wake the wrong thread; sharing an origin chain
; would interleave two failures into one history and make the diagnostic
; fiction. The frame arena (D-034) joins them when threads land.
;
;   0 rq_head | 1 rq_tail | 2 sl_head | 3 park_at | 4 park_pending
;   5 park_word | 6 join_ns | 7 grace_ns | 8 chain[8] | 9 chain_n
;   10 windup_seen | 11 epfd | 12 evfd     (B-3a, 1.1.12a: the reactor --
;   both 0 until the first io_ready; 0 is a safe sentinel because fd 0 is
;   stdin and the kernel never hands it out again while it is open)
;   13 cur_task -- THE FRAME npk_step IS RUNNING (1.1.12a). Awaits drive
;   child coroutines inline, so the frame a nested wait sits in is NOT the
;   frame the sweep sleeps and wakes -- the task root is. Every waiter
;   registration (channel, lock, condvar, barrier, reactor) resolves this
;   slot rather than trusting the immediate frame it was lowered in; same-
;   thread only, so no atomics.
%npk.exec = type { ptr, ptr, ptr, i64, i32, i32, i64, i64, [8 x i32], i32, i32, i32, i32, ptr }

; The join deadline (D-083) and the wind-up grace (D-177) are STATED
; CONSTANTS, not "whatever the runtime felt like": five seconds and 250ms,
; so total containment of a stuck task is deadline+grace and both halves are
; auditable at a glance. A thread's `joins` clause overrides the first
; where its executor is created.
@npk_main_exec = internal global %npk.exec { ptr null, ptr null, ptr null,
    i64 0, i32 0, i32 0, i64 5000000000, i64 250000000,
    [8 x i32] zeroinitializer, i32 0, i32 0, i32 0, i32 0, ptr null }

; THE CURRENT THREAD'S EXECUTOR. One accessor, so the thread-local move is
; one edit rather than forty: 1.1.9c makes this read `%fs:8`, which
; `CLONE_SETTLS` installs per thread. Today there is one thread and one
; executor, and the shape is already honest.
; THE MAIN THREAD'S TLS BLOCK, installed before anything runs. Freestanding
; means nothing sets `%fs` for us — the first cut read `%fs:8` on a thread
; whose `%fs` base was zero, which is a load from address 8 and exactly the
; segfault it deserves. `arch_prctl(ARCH_SET_FS)` gives the main thread the
; same shape `CLONE_SETTLS` gives every other one, so `npk_exec` has one path
; rather than a branch that is right by accident of ordering.
; Byte-zero, for the runtime's own blocks. `npk_calloc` is the user-visible
; one and registers what it hands back; this does not.
define internal void @npk_zero(ptr %p, i64 %n) {
entry:
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %ni, %step ]
  %done = icmp uge i64 %i, %n
  br i1 %done, label %fin, label %step
step:
  %q = getelementptr i8, ptr %p, i64 %i
  store i8 0, ptr %q
  %ni = add i64 %i, 1
  br label %loop
fin:
  ret void
}

define internal void @npk_tls_boot() {
entry:
  ; INTERNAL, not `npk_alloc`: the user-visible allocator registers its
  ; blocks in the <wild-live> set (D-151), so the runtime's own TLS and
  ; executor would be reported as leaks at a clean exit — which is exactly
  ; what the leak tests said the first time this ran.
  ;
  ; AND NOT A HEAP BLOCK AT ALL (1.5.1b step 5): `wild_release_all()` unmaps
  ; every chunk, and a trap raised after it — by `exit`'s own operand, the one
  ; statement TYPE-062 still admits there — reads `%fs:8` on its way to
  ; `failsafe`. With this block in a chunk that read faulted, and a refused
  ; free became SIGSEGV instead of a controlled stop (found by the first unit
  ; tests to drop a resolver after their release). One raw mapping, in no
  ; table, unmapped by nothing: the process ends with it.
  %tlsa = call i64 @npk_hmap(i64 ptrtoint (ptr getelementptr (%npk.tls, ptr null, i32 1) to i64))
  %tls = inttoptr i64 %tlsa to ptr
  %self = getelementptr %npk.tls, ptr %tls, i32 0, i32 0
  store ptr %tls, ptr %self
  %ex = getelementptr %npk.tls, ptr %tls, i32 0, i32 1
  store ptr @npk_main_exec, ptr %ex
  %root = getelementptr %npk.tls, ptr %tls, i32 0, i32 2
  store ptr null, ptr %root
  %res = getelementptr %npk.tls, ptr %tls, i32 0, i32 3
  store ptr null, ptr %res
  %tid = getelementptr %npk.tls, ptr %tls, i32 0, i32 4
  store i32 0, ptr %tid
  %ti = ptrtoint ptr %tls to i64
  ; arch_prctl(ARCH_SET_FS = 0x1002, tls)
  %r = call i64 @npk_sys6(i64 158, i64 4098, i64 %ti, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp ne i64 %r, 0
  br i1 %bad, label %nofs, label %ok
nofs:
  call void @npk_trap(i32 -4102)
  unreachable
ok:
  ret void
}

define ptr @npk_exec() {
entry:
  ; `%fs:8` is this thread's executor — installed by `CLONE_SETTLS` for a
  ; spawned thread and by `npk_tls_boot` for the main one, so there is one
  ; answer everywhere and no ordering to get wrong.
  %v = call i64 asm sideeffect "movq %fs:8, $0", "=r,~{dirflag},~{fpsr},~{flags}"()
  %p = inttoptr i64 %v to ptr
  ret ptr %p
}

; --- channels (D-072/D-182, 1.1.10) -----------------------------------------
;
; THE RUNTIME OWNS CHANNEL STORAGE; THE SCOPE OWNS ITS LIFE. An endpoint is an
; index and a generation, not an address — which is what lets it cross a
; spawn (D-004/D-180 refuse a borrow there), ride in a message, or travel
; through another channel. A stale endpoint is `StaleHandle` (-4106), a
; caught error rather than a dangling read, exactly as arenas already work.
;
; ONE TABLE OF POINTERS, GROWN BY DOUBLING, NEVER SHRUNK, AND NEVER MOVED OUT
; FROM UNDER A READER. The table used to hold the channel STRUCTS inline, and
; growing it copied them to new addresses — fine with one thread, and a
; use-after-free with two, because a reader mid-operation holds the old
; address. Now each channel is allocated on its own and never moves; only the
; array of POINTERS grows, and the old array is never freed. A reader that
; loaded the previous array still sees valid pointers at every index below the
; count it observed, so no reader needs the open lock. The wasted arrays total
; about one final array across the program's life, and are runtime-internal
; storage (D-151) rather than anything a program is accountable for.
;
; The count is published with a RELEASE store after the pointer is in place,
; and read with an ACQUIRE load, so a reader that sees the count sees the
; channel behind it.
;
; Generations are EVEN while live and ODD once reclaimed, the parity
; discipline D-152 uses for handles. `close` does NOT move the generation —
; closing is an end of stream on a live channel, reclamation is what
; invalidates a handle, and nothing reclaims one until the managed lowering
; lands (D-182, B-6). The channel struct is deliberately permanent, which is
; what will let a reclaimed index keep its generation across reuse.
;
;   0 buf | 1 cap | 2 elem_size | 3 head | 4 tail | 5 count
;   6 gen | 7 closed | 8 lock | 9 recv_waiters | 10 send_waiters
%npk.chan = type { ptr, i64, i64, i64, i64, i64, i32, i32, i32, ptr, ptr }

@npk_ch_tab = internal global ptr null
; RECLAIMED SLOT INDEXES (D-183, 1.2.5) — a LIFO the open path pops before
; growing the table. Guarded by the open lock, like the table itself.
@npk_ch_fstk = internal global ptr null
@npk_ch_fn = internal global i64 0
@npk_ch_fcap = internal global i64 0
@npk_ch_cap = internal global i64 0
@npk_ch_n = internal global i64 0
@npk_ch_open_lock = internal global i32 0

; A FUTEX MUTEX, three-state (0 free, 1 held, 2 held-and-contended). The third
; state is what keeps the uncontended path syscall-free in BOTH directions: an
; unlock only enters the kernel when it can see that somebody is waiting.
;
; A CHANNEL WITHOUT ONE LOSES MESSAGES. Before this, `count`, `head` and `tail`
; were read, compared and written with plain loads and stores; two threads
; sending to one channel both saw room, both wrote the same slot, and the
; receiver waited out its deadline for values that had been overwritten. It
; reproduced in 21 of 40 runs of a two-thread producer test — a data race that
; silently drops data, which in this language is not a performance bug.
define internal void @npk_mx_lock(ptr %w) {
entry:
  %c0 = cmpxchg ptr %w, i32 0, i32 1 seq_cst seq_cst
  %ok0 = extractvalue { i32, i1 } %c0, 1
  br i1 %ok0, label %done, label %loop
loop:
  ; Claim it as CONTENDED whatever it was. If it was free we now hold it; if
  ; it was held, the holder will see 2 on release and wake us.
  %old = atomicrmw xchg ptr %w, i32 2 seq_cst
  %free = icmp eq i32 %old, 0
  br i1 %free, label %done, label %wait
wait:
  %wp = ptrtoint ptr %w to i64
  ; futex(word, FUTEX_WAIT|PRIVATE, expected 2, NULL, NULL, 0). A spurious
  ; return or a value that already changed simply retries the claim.
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 128, i64 2, i64 0, i64 0, i64 0)
  br label %loop
done:
  ret void
}

define internal void @npk_mx_unlock(ptr %w) {
entry:
  %old = atomicrmw xchg ptr %w, i32 0 seq_cst
  %contended = icmp eq i32 %old, 2
  br i1 %contended, label %wake, label %done
wake:
  %wp = ptrtoint ptr %w to i64
  ; futex(word, FUTEX_WAKE|PRIVATE, 1, ...)
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 129, i64 1, i64 0, i64 0, i64 0)
  br label %done
done:
  ret void
}

define internal void @npk_ch_lock(ptr %ch) {
entry:
  %w = getelementptr %npk.chan, ptr %ch, i32 0, i32 8
  call void @npk_mx_lock(ptr %w)
  ret void
}

define internal void @npk_ch_unlock(ptr %ch) {
entry:
  %w = getelementptr %npk.chan, ptr %ch, i32 0, i32 8
  call void @npk_mx_unlock(ptr %w)
  ret void
}

define internal ptr @npk_ch_at(i32 %i) {
entry:
  %t = load atomic ptr, ptr @npk_ch_tab acquire, align 8
  %ix = sext i32 %i to i64
  %slot = getelementptr ptr, ptr %t, i64 %ix
  %p = load ptr, ptr %slot
  ret ptr %p
}

; --- waiting on a channel (D-071/D-176, 1.1.10-C2) --------------------------
;
; A BLOCKED TASK IS ON TWO LISTS AT ONCE, and that is the whole design. Its own
; executor's SLEEPERS hold it against the caller's deadline, through `qnext`;
; the CHANNEL's waiter list holds it against a peer operation, through
; `chan_next`. Whichever comes first wins, and neither needs to know about the
; other: a wake makes the task due, a deadline makes it due, and in both cases
; it re-runs the same call and re-tests the same condition.
;
; What this replaces is a 1ms re-poll. That was bounded and correct — no thread
; blocked, no deadline was missed — and it cost up to a millisecond per
; hand-off plus a wakeup per millisecond per blocked task, and it could not
; express a rendezvous, because a poll cannot ask whether a receiver is there.

define internal void @npk_ch_wait_link(ptr %hp, ptr %fr_unused) {
entry:
  ; THE LINKED IDENTITY IS THE TASK, NOT THE IMMEDIATE FRAME (1.1.12a).
  ; Awaits drive children inline, so a wait lowered inside a helper
  ; coroutine hands this function the helper's frame -- but the frame the
  ; sweep sleeps, dues and wakes is the task root's. Linking anything else
  ; is a due mark nobody reads: the task sleeps to its deadline for a value
  ; that arrived. Unlink resolves the same identity, which keeps the
  ; entry-unlink discipline sound at any nesting depth.
  %ex = call ptr @npk_exec()
  %ctp = getelementptr %npk.exec, ptr %ex, i32 0, i32 13
  %fr = load ptr, ptr %ctp
  %h = load ptr, ptr %hp
  %cn = getelementptr %npk.hdr, ptr %fr, i32 0, i32 10
  store ptr %h, ptr %cn
  store ptr %fr, ptr %hp
  ret void
}

; Remove a frame from one list if it is on it. O(n) in the waiters, walked
; under the channel's lock — a task that gave up on its deadline MUST come
; off, or a later peer would make a task due that is no longer waiting, and
; `wake_at` is also how a finished task is recognised (-1).
define internal void @npk_ch_wait_unlink(ptr %hp, ptr %fr_unused) {
entry:
  ; the same identity link records -- see npk_ch_wait_link
  %ex = call ptr @npk_exec()
  %ctp = getelementptr %npk.exec, ptr %ex, i32 0, i32 13
  %fr = load ptr, ptr %ctp
  br label %loop
loop:
  %slot = phi ptr [ %hp, %entry ], [ %nextslot, %step ]
  %cur = load ptr, ptr %slot
  %end = icmp eq ptr %cur, null
  br i1 %end, label %done, label %test
test:
  %hit = icmp eq ptr %cur, %fr
  br i1 %hit, label %unlink, label %step
step:
  %nextslot = getelementptr %npk.hdr, ptr %cur, i32 0, i32 10
  br label %loop
unlink:
  %cn = getelementptr %npk.hdr, ptr %cur, i32 0, i32 10
  %nx = load ptr, ptr %cn
  store ptr %nx, ptr %slot
  store ptr null, ptr %cn
  br label %done
done:
  ret void
}

; Take one waiter off a list and make it runnable on ITS OWN executor.
;
; THE ORDER IS THE PROTOCOL. `wake_at` is set first, then the owner's park
; word, then the futex wake. An executor about to sleep clears its park word,
; re-checks its sleepers, and only then waits — so a waker that arrives before
; the sleep is seen by the re-check, and one that arrives after finds a
; non-zero park word waiting for it and the FUTEX_WAIT returns immediately.
; Reversing either half is a lost wakeup: a task that sleeps to its deadline
; and reports DeadlineExceeded for a value that already arrived.
define internal void @npk_ch_wake_one(ptr %hp) {
entry:
  %f = load ptr, ptr %hp
  %none = icmp eq ptr %f, null
  br i1 %none, label %done, label %pop
pop:
  %cn = getelementptr %npk.hdr, ptr %f, i32 0, i32 10
  %nx = load ptr, ptr %cn
  store ptr %nx, ptr %hp
  store ptr null, ptr %cn
  ; DUE-NOW IS 1, NOT 0, AND THE DIFFERENCE IS LOAD-BEARING. A monotonic
  ; timepoint of 1ns is always in the past, so it is due; and it is
  ; distinguishable from the 0 a FRESH frame carries, which is what lets
  ; `npk_sl_push` below tell "a waker got here first" from "never slept".
  %wa = getelementptr %npk.hdr, ptr %f, i32 0, i32 8
  store atomic i64 1, ptr %wa seq_cst, align 8
  %op = getelementptr %npk.hdr, ptr %f, i32 0, i32 11
  %ow = load ptr, ptr %op
  %noown = icmp eq ptr %ow, null
  br i1 %noown, label %done, label %rouse
rouse:
  %pw = getelementptr %npk.exec, ptr %ow, i32 0, i32 5
  store atomic i32 1, ptr %pw seq_cst, align 4
  %wp = ptrtoint ptr %pw to i64
  ; futex(word, FUTEX_WAKE|PRIVATE, 1, ...)
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 129, i64 1, i64 0, i64 0, i64 0)
  ; ...and the eventfd, when the owner's idle wait is the reactor's
  ; (B-3a, 1.1.12a): an epoll_pwait sleeper hears no futex.
  %evp = getelementptr %npk.exec, ptr %ow, i32 0, i32 12
  %evfd = load atomic i32, ptr %evp acquire, align 4
  %noev = icmp eq i32 %evfd, 0
  br i1 %noev, label %done, label %ping
ping:
  %one = alloca i64, align 8
  store i64 1, ptr %one
  %onep = ptrtoint ptr %one to i64
  %evl = sext i32 %evfd to i64
  %wr = call i64 @npk_sys6(i64 1, i64 %evl, i64 %onep, i64 8, i64 0, i64 0, i64 0)
  br label %done
done:
  ret void
}

define internal void @npk_ch_wake_all(ptr %hp) {
entry:
  br label %loop
loop:
  %h = load ptr, ptr %hp
  %empty = icmp eq ptr %h, null
  br i1 %empty, label %done, label %one
one:
  call void @npk_ch_wake_one(ptr %hp)
  br label %loop
done:
  ret void
}

; A fresh channel: `cap` slots of `esz` bytes each. Returns the packed handle
; { index, generation } as an i64 so one register carries it.
define i64 @npk_ch_open(i64 %cap, i64 %esz) {
entry:
  call void @npk_mx_lock(ptr @npk_ch_open_lock)
  %n = load i64, ptr @npk_ch_n
  %c = load i64, ptr @npk_ch_cap
  %full = icmp sge i64 %n, %c
  br i1 %full, label %grow, label %have
grow:
  %nc0 = shl i64 %c, 1
  %first = icmp eq i64 %c, 0
  %nc = select i1 %first, i64 16, i64 %nc0
  %bytes = mul i64 %nc, 8
  %m = call ptr @npk_alloc_internal(i64 %bytes)
  call void @npk_zero(ptr %m, i64 %bytes)
  %old = load ptr, ptr @npk_ch_tab
  %was = icmp eq ptr %old, null
  br i1 %was, label %pub, label %copy
copy:
  %oldbytes = mul i64 %c, 8
  call ptr @memcpy(ptr %m, ptr %old, i64 %oldbytes)
  ; THE OLD ARRAY IS NOT FREED. A reader holding it still resolves every index
  ; below the count it observed, which is what lets `npk_ch_at` run without
  ; taking this lock.
  br label %pub
pub:
  store atomic ptr %m, ptr @npk_ch_tab release, align 8
  store i64 %nc, ptr @npk_ch_cap
  br label %have
have:
  ; REUSE A RECLAIMED SLOT FIRST (D-183, 1.2.5). The slot's chan struct is
  ; immortal; reviving it means a fresh buffer, cleared ring state, and the
  ; generation bumped from the reclaim's ODD back to EVEN — at which point
  ; every handle from the slot's previous life answers StaleHandle, which is
  ; the reachability D-152's discipline promised.
  %rfn = load i64, ptr @npk_ch_fn
  %rhave = icmp sgt i64 %rfn, 0
  br i1 %rhave, label %revive, label %fresh
revive:
  %rfn2 = add i64 %rfn, -1
  store i64 %rfn2, ptr @npk_ch_fn
  %rstk = load ptr, ptr @npk_ch_fstk
  %rslotp = getelementptr i64, ptr %rstk, i64 %rfn2
  %ridx = load i64, ptr %rslotp
  %rt = load ptr, ptr @npk_ch_tab
  %rchp = getelementptr ptr, ptr %rt, i64 %ridx
  %rch = load ptr, ptr %rchp
  call void @npk_ch_lock(ptr %rch)
  %rrendez = icmp eq i64 %cap, 0
  %rslots = select i1 %rrendez, i64 1, i64 %cap
  %rbufsz = mul i64 %rslots, %esz
  %rbuf = call ptr @npk_alloc_internal(i64 %rbufsz)
  %rbp = getelementptr %npk.chan, ptr %rch, i32 0, i32 0
  store ptr %rbuf, ptr %rbp
  %rcapp = getelementptr %npk.chan, ptr %rch, i32 0, i32 1
  store i64 %cap, ptr %rcapp
  %reszp = getelementptr %npk.chan, ptr %rch, i32 0, i32 2
  store i64 %esz, ptr %reszp
  %rhp = getelementptr %npk.chan, ptr %rch, i32 0, i32 3
  store i64 0, ptr %rhp
  %rtp = getelementptr %npk.chan, ptr %rch, i32 0, i32 4
  store i64 0, ptr %rtp
  %rnp = getelementptr %npk.chan, ptr %rch, i32 0, i32 5
  store i64 0, ptr %rnp
  %rgp = getelementptr %npk.chan, ptr %rch, i32 0, i32 6
  %rg = load i32, ptr %rgp
  %rg2 = add i32 %rg, 1
  store i32 %rg2, ptr %rgp
  %rclp = getelementptr %npk.chan, ptr %rch, i32 0, i32 7
  store i32 0, ptr %rclp
  call void @npk_ch_unlock(ptr %rch)
  call void @npk_mx_unlock(ptr @npk_ch_open_lock)
  %rpl = and i64 %ridx, 4294967295
  %rph0 = zext i32 %rg2 to i64
  %rph = shl i64 %rph0, 32
  %rpacked = or i64 %rph, %rpl
  ret i64 %rpacked
fresh:
  %idx = load i64, ptr @npk_ch_n
  ; THE CHANNEL IS ALLOCATED ON ITS OWN AND NEVER MOVES, so growing the table
  ; cannot pull it out from under an operation in flight on another thread.
  %chbytes = add i64 0, ptrtoint (ptr getelementptr (%npk.chan, ptr null, i32 1) to i64)
  %ch = call ptr @npk_alloc_internal(i64 %chbytes)
  call void @npk_zero(ptr %ch, i64 %chbytes)
  ; a rendezvous channel (CAP 0) still needs one slot to hand the value over
  %rendez = icmp eq i64 %cap, 0
  %slots = select i1 %rendez, i64 1, i64 %cap
  %bufsz = mul i64 %slots, %esz
  %buf = call ptr @npk_alloc_internal(i64 %bufsz)
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  store ptr %buf, ptr %bp
  %cp = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  store i64 %cap, ptr %cp
  %ep = getelementptr %npk.chan, ptr %ch, i32 0, i32 2
  store i64 %esz, ptr %ep
  ; head, tail, count, closed and the lock word are already zero. The
  ; generation starts at TWO, not zero (D-183, 1.2.5b): an all-zero handle —
  ; a zeroed struct field, a failed call's zeroed value half — must alias NO
  ; live channel, and slot gen 0 would have matched it. Even is what live
  ; means, and 2 is the first even a handle can carry — the arena's virgin
  ; promotion, applied here.
  %gp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  store i32 2, ptr %gp
  %gen = load i32, ptr %gp
  ; PUBLISH THE POINTER, THEN THE COUNT. A reader acquires the count and so
  ; cannot see an index whose pointer is not yet stored.
  %tab = load ptr, ptr @npk_ch_tab
  %slot = getelementptr ptr, ptr %tab, i64 %idx
  store ptr %ch, ptr %slot
  %ni = add i64 %idx, 1
  store atomic i64 %ni, ptr @npk_ch_n release, align 8
  call void @npk_mx_unlock(ptr @npk_ch_open_lock)
  %i32i = trunc i64 %idx to i32
  %packed_lo = zext i32 %i32i to i64
  %packed_hi0 = zext i32 %gen to i64
  %packed_hi = shl i64 %packed_hi0, 32
  %packed = or i64 %packed_hi, %packed_lo
  ret i64 %packed
}

; Resolve a handle, or null when it is stale — the slot was closed and its
; generation moved on (D-152's parity discipline).
define internal ptr @npk_ch_get(i64 %h) {
entry:
  %ix64 = and i64 %h, 4294967295
  %n = load atomic i64, ptr @npk_ch_n acquire, align 8
  %oob = icmp uge i64 %ix64, %n
  br i1 %oob, label %stale, label %live
live:
  %i32i = trunc i64 %ix64 to i32
  %ch = call ptr @npk_ch_at(i32 %i32i)
  %gp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  %g = load i32, ptr %gp
  %want0 = lshr i64 %h, 32
  %want = trunc i64 %want0 to i32
  %same = icmp eq i32 %g, %want
  br i1 %same, label %ok, label %stale
ok:
  ret ptr %ch
stale:
  ret ptr null
}

; Close: the generation moves to ODD, so every endpoint naming this slot
; becomes stale at once and the slot can be reused later without ambiguity.
define i32 @npk_ch_close(i64 %h) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %ok
ok:
  ; CLOSING IS NOT RECLAIMING. The generation guards the SLOT — it moves when
  ; a channel's storage is handed to a different channel, so a handle kept
  ; past that point is caught rather than aimed at a stranger's buffer.
  ; Closing changes a live channel's STATE: the slot, the buffer and every
  ; value still in it are exactly where the holder left them, and a receiver
  ; has to be able to drain them. Bumping the generation here made every
  ; outstanding endpoint stale the instant the producer finished, so a reader
  ; mid-drain was told its handle was dangling — StaleHandle standing in for
  ; ChannelClosed, a use-after-free report for an orderly end of stream.
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
gck.ok:
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  store i32 1, ptr %clp
  ; A CLOSE CHANGES THE ANSWER FOR EVERYBODY, in both directions: a waiting
  ; receiver now gets ChannelClosed instead of a value, and a waiting sender
  ; gets it instead of room. Waking one would leave the rest asleep until
  ; their deadlines for a question already answered.
  %rw2 = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  call void @npk_ch_wake_all(ptr %rw2)
  %sw2 = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wake_all(ptr %sw2)
  call void @npk_ch_unlock(ptr %ch)
  ret i32 0
stale:
  ret i32 -4106
}

; Is this channel closed (or its handle stale)?
define i32 @npk_ch_closed(i64 %h) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %yes, label %look
look:
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  ; a reclaimed slot's old handle asks a question with one answer
  call void @npk_ch_unlock(ptr %ch)
  ret i32 1
gck.ok:
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  %c = load i32, ptr %clp
  call void @npk_ch_unlock(ptr %ch)
  ret i32 %c
yes:
  ret i32 1
}

; RECLAIM (D-183, 1.2.5): the creating function's exit — after its defers,
; drops and child joins — hands the channel back. Closing was never
; reclaiming (the drain doctrine above); THIS is where the generation moves,
; every outstanding endpoint becomes stale at once, the buffer is freed, and
; the slot index enters the free stack for `open` to reuse. The slot's chan
; STRUCT is immortal — an op on another thread that resolved its handle
; before this ran and blocks on the lock wakes to the re-check, never to
; freed memory. Waiters are woken so nobody sleeps out a deadline on a
; question that now answers StaleHandle. A generation at the retire cap
; pins the slot out of the stack forever, the arena's own rule.
define i32 @npk_ch_reclaim(i64 %h) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %live
live:
  call void @npk_ch_lock(ptr %ch)
  %gp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  %g = load i32, ptr %gp
  %want0 = lshr i64 %h, 32
  %want = trunc i64 %want0 to i32
  %same = icmp eq i32 %g, %want
  br i1 %same, label %take, label %lost
lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
take:
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  store i32 1, ptr %clp
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  %buf = load ptr, ptr %bp
  %nobuf = icmp eq ptr %buf, null
  br i1 %nobuf, label %woken, label %freebuf
freebuf:
  call void @npk_dalloc(ptr %buf)
  store ptr null, ptr %bp
  br label %woken
woken:
  %rw = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  call void @npk_ch_wake_all(ptr %rw)
  %sw = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wake_all(ptr %sw)
  call void @npk_ch_unlock(ptr %ch)
  ; retire at the cap: a slot at 0xFFFFFFFE never re-enters the stack
  %tired = icmp uge i32 %g2, -2
  br i1 %tired, label %done, label %push
push:
  call void @npk_mx_lock(ptr @npk_ch_open_lock)
  %fn = load i64, ptr @npk_ch_fn
  %fc = load i64, ptr @npk_ch_fcap
  %full = icmp sge i64 %fn, %fc
  br i1 %full, label %grow, label %put
grow:
  %nc0 = shl i64 %fc, 1
  %first = icmp eq i64 %fc, 0
  %nc = select i1 %first, i64 16, i64 %nc0
  %nbytes = mul i64 %nc, 8
  %nm = call ptr @npk_alloc_internal(i64 %nbytes)
  %old = load ptr, ptr @npk_ch_fstk
  %had = icmp eq ptr %old, null
  br i1 %had, label %swap, label %copyold
copyold:
  %obytes = mul i64 %fc, 8
  call ptr @memcpy(ptr %nm, ptr %old, i64 %obytes)
  call void @npk_dalloc(ptr %old)
  br label %swap
swap:
  store ptr %nm, ptr @npk_ch_fstk
  store i64 %nc, ptr @npk_ch_fcap
  br label %put
put:
  %stk = load ptr, ptr @npk_ch_fstk
  %slotp = getelementptr i64, ptr %stk, i64 %fn
  %idx = and i64 %h, 4294967295
  store i64 %idx, ptr %slotp
  %fn2 = add i64 %fn, 1
  store i64 %fn2, ptr @npk_ch_fn
  call void @npk_mx_unlock(ptr @npk_ch_open_lock)
  br label %done
done:
  ret i32 0
stale:
  ret i32 -4106
}

; --- the mutex (D-056, 1.1.11) ----------------------------------------------
;
; One managed cell per mutex: [ state i32 | listlock i32 | waiters ptr | T ].
; The STATE is task-level (0 free, 1 held) and changes only under the
; LISTLOCK, a thread-level npk_mx futex held for a few instructions — the
; same two-tier shape the channel uses, and the same waiter protocol
; (npk_ch_wait_link/unlink/wake_one ride the frame's chan_next and owner
; slots; a task blocked on a mutex is not blocked on a channel, so the reuse
; is free). Acquisition is task-fair-enough: a woken waiter re-runs the
; acquire and may lose to a barger, which the deadline bounds.
define i32 @npk_mutex_acquire_wait(ptr %cell, i64 %abs, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  ; unlink self, unconditionally — idempotent across the first try, a wake
  ; by the releaser, and a wake by the deadline (the channel's own rule).
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  %st = load i32, ptr %cell
  %free = icmp eq i32 %st, 0
  br i1 %free, label %take, label %busy
take:
  store i32 1, ptr %cell
  call void @npk_mx_unlock(ptr %llp)
  ret i32 0
busy:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
park:
  call void @npk_ch_wait_link(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  call void @npk_park_until(i64 %abs)
  ret i32 1
expired:
  call void @npk_mx_unlock(ptr %llp)
  ret i32 -4107
}

; The release IS the guard's generated drop (D-183), and ONE symbol serves
; every exclusive hold: the cell's KIND (at +16) chooses the wake policy —
; a mutex wakes one waiter (they all want the same thing), an rwlock's
; writer release wakes ALL (a crowd of readers may now proceed together).
define void @npk_guard_release(ptr %cell) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  store i32 0, ptr %cell
  %kp = getelementptr i8, ptr %cell, i64 16
  %kind = load i32, ptr %kp
  %wp = getelementptr i8, ptr %cell, i64 8
  %isrw = icmp eq i32 %kind, 1
  br i1 %isrw, label %all, label %one
one:
  call void @npk_ch_wake_one(ptr %wp)
  br label %out
all:
  call void @npk_ch_wake_all(ptr %wp)
  br label %out
out:
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

; A reader's release: the count comes down, and zero wakes the crowd — a
; parked writer is in it.
define void @npk_rw_release_read(ptr %cell) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %st = load i32, ptr %cell
  %st2 = add i32 %st, -1
  %gone = icmp sle i32 %st2, 1
  %nst = select i1 %gone, i32 0, i32 %st2
  store i32 %nst, ptr %cell
  %iszero = icmp eq i32 %nst, 0
  br i1 %iszero, label %wake, label %out
wake:
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wake_all(ptr %wp)
  br label %out
out:
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

; The rwlock's two acquires, over the mutex cell's own shape. State: 0 free,
; 1 a writer, N>=2 is N-1 readers.
define i32 @npk_rw_read_wait(ptr %cell, i64 %abs, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  %st = load i32, ptr %cell
  %held = icmp eq i32 %st, 1
  br i1 %held, label %busy, label %take
take:
  %isfree = icmp eq i32 %st, 0
  %inc = add i32 %st, 1
  %nst = select i1 %isfree, i32 2, i32 %inc
  store i32 %nst, ptr %cell
  call void @npk_mx_unlock(ptr %llp)
  ret i32 0
busy:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
park:
  call void @npk_ch_wait_link(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  call void @npk_park_until(i64 %abs)
  ret i32 1
expired:
  call void @npk_mx_unlock(ptr %llp)
  ret i32 -4107
}

define i32 @npk_rw_write_wait(ptr %cell, i64 %abs, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  %st = load i32, ptr %cell
  %free = icmp eq i32 %st, 0
  br i1 %free, label %take, label %busy
take:
  store i32 1, ptr %cell
  call void @npk_mx_unlock(ptr %llp)
  ret i32 0
busy:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
park:
  call void @npk_ch_wait_link(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  call void @npk_park_until(i64 %abs)
  ret i32 1
expired:
  call void @npk_mx_unlock(ptr %llp)
  ret i32 -4107
}

; --- the condvar (D-056, 1.1.11b) -------------------------------------------
;
; `timedwait`'s phase 0: LINK FIRST, THEN RELEASE — a signal landing between
; a release and a late link would be lost; one landing after the link marks
; the frame due, which the sleeper-push protocol keeps (the channel's own
; guarantee). The mutex release is the ordinary guard release. The park is
; bounded by the caller's absolute deadline, like every wait in the surface.
define void @npk_cv_begin(ptr %cv, ptr %m, i64 %abs, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cv, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cv, i64 8
  call void @npk_ch_wait_link(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  call void @npk_guard_release(ptr %m)
  call void @npk_park_until(i64 %abs)
  ret void
}

; Off the list, on every completed path — idempotent like every unlink.
define void @npk_cv_done(ptr %cv, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cv, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cv, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

define void @npk_cv_signal(ptr %cv) {
entry:
  %llp = getelementptr i8, ptr %cv, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cv, i64 8
  call void @npk_ch_wake_one(ptr %wp)
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

define void @npk_cv_broadcast(ptr %cv) {
entry:
  %llp = getelementptr i8, ptr %cv, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cv, i64 8
  call void @npk_ch_wake_all(ptr %wp)
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

; --- the barrier (D-056, 1.1.11b) -------------------------------------------
;
; [ count | listlock | waiters | kind | generation +20 | N +24 ]. The N-th
; arrival resets the count, moves the GENERATION and wakes everyone; an
; earlier arrival waits the generation out. A timed-out or wound-up party
; has NOT arrived: its slot is handed back under the lock, so the barrier is
; not wedged one short forever — unless its round already completed, in
; which case there is nothing to hand back.
define i64 @npk_barrier_arrive(ptr %cell) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %cnt = load i32, ptr %cell
  %c2 = add i32 %cnt, 1
  %np = getelementptr i8, ptr %cell, i64 24
  %n = load i32, ptr %np
  %full = icmp sge i32 %c2, %n
  br i1 %full, label %release, label %waitgen
release:
  store i32 0, ptr %cell
  %gp = getelementptr i8, ptr %cell, i64 20
  %g = load i32, ptr %gp
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wake_all(ptr %wp)
  call void @npk_mx_unlock(ptr %llp)
  ret i64 -1
waitgen:
  store i32 %c2, ptr %cell
  %gp2 = getelementptr i8, ptr %cell, i64 20
  %gw = load i32, ptr %gp2
  call void @npk_mx_unlock(ptr %llp)
  %gz = zext i32 %gw to i64
  ret i64 %gz
}

define i32 @npk_barrier_poll(ptr %cell, i64 %mygen, i64 %abs, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  %gp = getelementptr i8, ptr %cell, i64 20
  %g = load i32, ptr %gp
  %gz = zext i32 %g to i64
  %moved = icmp ne i64 %gz, %mygen
  br i1 %moved, label %done, label %still
done:
  call void @npk_mx_unlock(ptr %llp)
  ret i32 0
still:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
expired:
  %cnt = load i32, ptr %cell
  %c2 = add i32 %cnt, -1
  %neg = icmp slt i32 %c2, 0
  %nc = select i1 %neg, i32 0, i32 %c2
  store i32 %nc, ptr %cell
  call void @npk_mx_unlock(ptr %llp)
  ret i32 -4107
park:
  call void @npk_ch_wait_link(ptr %wp, ptr %fr)
  call void @npk_mx_unlock(ptr %llp)
  call void @npk_park_until(i64 %abs)
  ret i32 1
}

define void @npk_barrier_cancel(ptr %cell, i64 %mygen, ptr %fr) {
entry:
  %llp = getelementptr i8, ptr %cell, i64 4
  call void @npk_mx_lock(ptr %llp)
  %wp = getelementptr i8, ptr %cell, i64 8
  call void @npk_ch_wait_unlink(ptr %wp, ptr %fr)
  %gp = getelementptr i8, ptr %cell, i64 20
  %g = load i32, ptr %gp
  %gz = zext i32 %g to i64
  %same = icmp eq i64 %gz, %mygen
  br i1 %same, label %giveback, label %out
giveback:
  %cnt = load i32, ptr %cell
  %c2 = add i32 %cnt, -1
  %neg = icmp slt i32 %c2, 0
  %nc = select i1 %neg, i32 0, i32 %c2
  store i32 %nc, ptr %cell
  br label %out
out:
  call void @npk_mx_unlock(ptr %llp)
  ret void
}

; THE ONE CALL A BLOCKED OPERATION MAKES. Returns 0 done, 1 parked (the task
; must return SUSPENDED and will be re-entered here), or a negative error.
;
; It begins by UNLINKING ITSELF, unconditionally. That one line is what makes
; the call idempotent across all three ways a task can arrive here: the first
; try, a wake by a peer, and a wake by its own deadline. Without it a task
; that timed out would stay on the channel's list, and the next peer would
; make a task due that had stopped waiting — writing over a `wake_at` that may
; by then mean "finished" (-1).
define i32 @npk_ch_recv_wait(i64 %h, ptr %dst, i64 %abs, ptr %fr) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %live
live:
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
gck.ok:
  %rw = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  call void @npk_ch_wait_unlink(ptr %rw, ptr %fr)
  %np = getelementptr %npk.chan, ptr %ch, i32 0, i32 5
  %cnt = load i64, ptr %np
  %empty = icmp eq i64 %cnt, 0
  br i1 %empty, label %nothing, label %take
take:
  %ep = getelementptr %npk.chan, ptr %ch, i32 0, i32 2
  %esz = load i64, ptr %ep
  %hp = getelementptr %npk.chan, ptr %ch, i32 0, i32 3
  %head = load i64, ptr %hp
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  %buf = load ptr, ptr %bp
  %off = mul i64 %head, %esz
  %src = getelementptr i8, ptr %buf, i64 %off
  call ptr @memcpy(ptr %dst, ptr %src, i64 %esz)
  %cp = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  %cap = load i64, ptr %cp
  %rendez = icmp eq i64 %cap, 0
  %limit = select i1 %rendez, i64 1, i64 %cap
  %h2 = add i64 %head, 1
  %wrap = icmp sge i64 %h2, %limit
  %h3 = select i1 %wrap, i64 0, i64 %h2
  store i64 %h3, ptr %hp
  %c2 = sub i64 %cnt, 1
  store i64 %c2, ptr %np
  %sw = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wake_one(ptr %sw)
  call void @npk_ch_unlock(ptr %ch)
  ret i32 0
nothing:
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  %cl = load i32, ptr %clp
  %isclosed = icmp ne i32 %cl, 0
  br i1 %isclosed, label %closed, label %maybewait
maybewait:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
park:
  call void @npk_ch_wait_link(ptr %rw, ptr %fr)
  ; ON A RENDEZVOUS, REGISTERING IS ITSELF THE EVENT. A CAP 0 sender is
  ; waiting for a receiver to exist, and one just did — so it is woken here
  ; rather than by a value arriving, which on this channel never happens
  ; first. Without it both sides park and neither is the other's wake: a
  ; deadlock that resolves only when the deadlines run out.
  %cp2 = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  %cap2 = load i64, ptr %cp2
  %rz = icmp eq i64 %cap2, 0
  br i1 %rz, label %tellsender, label %parked
tellsender:
  %sw2 = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wake_one(ptr %sw2)
  br label %parked
parked:
  call void @npk_ch_unlock(ptr %ch)
  call void @npk_park_until(i64 %abs)
  ret i32 1
expired:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4107
closed:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4108
stale:
  ret i32 -4106
}

define i32 @npk_ch_send_wait(i64 %h, ptr %src, i64 %abs, ptr %fr) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %live
live:
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
gck.ok:
  %sw = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wait_unlink(ptr %sw, ptr %fr)
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  %cl = load i32, ptr %clp
  %isclosed = icmp ne i32 %cl, 0
  br i1 %isclosed, label %closed, label %room
room:
  %cp = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  %cap = load i64, ptr %cp
  %np = getelementptr %npk.chan, ptr %ch, i32 0, i32 5
  %cnt = load i64, ptr %np
  %rendez = icmp eq i64 %cap, 0
  %limit = select i1 %rendez, i64 1, i64 %cap
  %isfull = icmp sge i64 %cnt, %limit
  ; A RENDEZVOUS WAITS FOR A RECEIVER, NOT FOR ROOM (D-072). The buffer's one
  ; slot is the hand-off, not storage: a CAP 0 send may only deposit when a
  ; receiver is registered and therefore certain to take it. Waiting on room
  ; alone is what made a rendezvous quietly behave as a capacity-1 buffer,
  ; returning the moment it deposited with no receiver in sight — the same
  ; source, not synchronising.
  %rwh = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  %rww = load ptr, ptr %rwh
  %norecv = icmp eq ptr %rww, null
  %rendez_blocked = and i1 %rendez, %norecv
  %blocked = or i1 %isfull, %rendez_blocked
  br i1 %blocked, label %maybewait, label %write
write:
  %ep = getelementptr %npk.chan, ptr %ch, i32 0, i32 2
  %esz = load i64, ptr %ep
  %tp = getelementptr %npk.chan, ptr %ch, i32 0, i32 4
  %tail = load i64, ptr %tp
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  %buf = load ptr, ptr %bp
  %off = mul i64 %tail, %esz
  %dst = getelementptr i8, ptr %buf, i64 %off
  call ptr @memcpy(ptr %dst, ptr %src, i64 %esz)
  %t2 = add i64 %tail, 1
  %wrap = icmp sge i64 %t2, %limit
  %t3 = select i1 %wrap, i64 0, i64 %t2
  store i64 %t3, ptr %tp
  %c2 = add i64 %cnt, 1
  store i64 %c2, ptr %np
  %rw = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  call void @npk_ch_wake_one(ptr %rw)
  call void @npk_ch_unlock(ptr %ch)
  ret i32 0
maybewait:
  %now = call i64 @npk_mono_now()
  %late = icmp sge i64 %now, %abs
  br i1 %late, label %expired, label %park
park:
  call void @npk_ch_wait_link(ptr %sw, ptr %fr)
  call void @npk_ch_unlock(ptr %ch)
  call void @npk_park_until(i64 %abs)
  ret i32 1
expired:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4107
closed:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4108
stale:
  ret i32 -4106
}

; ONE NON-BLOCKING STEP OF EACH OPERATION. The blocking is NOT here: under
; D-071 a full or empty channel suspends the TASK, and suspension is the
; emitted code's business (it owns the state machine). The runtime offers the
; step and says whether it happened, so the same primitive serves a task that
; parks and a `send` with a zero deadline that gives up immediately.
;
;   0 = done   1 = would block   -4106 = stale handle   -4108 = closed
define i32 @npk_ch_try_send(i64 %h, ptr %src) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %live
live:
  ; EVERY READ AND WRITE OF THE RING IS UNDER THE CHANNEL'S LOCK. `count`,
  ; `head` and `tail` are a single piece of state, and testing one then
  ; writing another is only atomic if nothing else can run between them.
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
gck.ok:
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  %cl = load i32, ptr %clp
  %isclosed = icmp ne i32 %cl, 0
  br i1 %isclosed, label %closed, label %room
room:
  %cp = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  %cap = load i64, ptr %cp
  %np = getelementptr %npk.chan, ptr %ch, i32 0, i32 5
  %cnt = load i64, ptr %np
  ; a rendezvous channel (CAP 0) holds exactly one in flight: the receiver
  ; takes it before the sender is released, which the task-level protocol
  ; enforces by the sender suspending until `count` returns to zero.
  %rendez = icmp eq i64 %cap, 0
  %limit = select i1 %rendez, i64 1, i64 %cap
  %isfull = icmp sge i64 %cnt, %limit
  br i1 %isfull, label %wouldblock, label %write
write:
  %ep = getelementptr %npk.chan, ptr %ch, i32 0, i32 2
  %esz = load i64, ptr %ep
  %tp = getelementptr %npk.chan, ptr %ch, i32 0, i32 4
  %tail = load i64, ptr %tp
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  %buf = load ptr, ptr %bp
  %off = mul i64 %tail, %esz
  %dst = getelementptr i8, ptr %buf, i64 %off
  call ptr @memcpy(ptr %dst, ptr %src, i64 %esz)
  %t2 = add i64 %tail, 1
  %wrap = icmp sge i64 %t2, %limit
  %t3 = select i1 %wrap, i64 0, i64 %t2
  store i64 %t3, ptr %tp
  %c2 = add i64 %cnt, 1
  store i64 %c2, ptr %np
  ; A VALUE ARRIVED: whoever is waiting for one is now able to proceed.
  %rw = getelementptr %npk.chan, ptr %ch, i32 0, i32 9
  call void @npk_ch_wake_one(ptr %rw)
  call void @npk_ch_unlock(ptr %ch)
  ret i32 0
wouldblock:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 1
closed:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4108
stale:
  ret i32 -4106
}

define i32 @npk_ch_try_recv(i64 %h, ptr %dst) {
entry:
  %ch = call ptr @npk_ch_get(i64 %h)
  %bad = icmp eq ptr %ch, null
  br i1 %bad, label %stale, label %live
live:
  call void @npk_ch_lock(ptr %ch)
  %gckp = getelementptr %npk.chan, ptr %ch, i32 0, i32 6
  ; RECLAIM RE-CHECK (D-183, 1.2.5): the handle was resolved before this
  ; lock was taken, and a reclaim may have moved the slot's generation while
  ; we blocked on it. The slot's struct is immortal, so the load is safe; the
  ; BUFFER is not ours unless the generation still matches.
  %gnow = load i32, ptr %gckp
  %gwant0 = lshr i64 %h, 32
  %gwant = trunc i64 %gwant0 to i32
  %gsame = icmp eq i32 %gnow, %gwant
  br i1 %gsame, label %gck.ok, label %gck.lost
gck.lost:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4106
gck.ok:
  %np = getelementptr %npk.chan, ptr %ch, i32 0, i32 5
  %cnt = load i64, ptr %np
  %empty = icmp eq i64 %cnt, 0
  br i1 %empty, label %nothing, label %take
take:
  %ep = getelementptr %npk.chan, ptr %ch, i32 0, i32 2
  %esz = load i64, ptr %ep
  %hp = getelementptr %npk.chan, ptr %ch, i32 0, i32 3
  %head = load i64, ptr %hp
  %bp = getelementptr %npk.chan, ptr %ch, i32 0, i32 0
  %buf = load ptr, ptr %bp
  %off = mul i64 %head, %esz
  %src = getelementptr i8, ptr %buf, i64 %off
  call ptr @memcpy(ptr %dst, ptr %src, i64 %esz)
  %cp = getelementptr %npk.chan, ptr %ch, i32 0, i32 1
  %cap = load i64, ptr %cp
  %rendez = icmp eq i64 %cap, 0
  %limit = select i1 %rendez, i64 1, i64 %cap
  %h2 = add i64 %head, 1
  %wrap = icmp sge i64 %h2, %limit
  %h3 = select i1 %wrap, i64 0, i64 %h2
  store i64 %h3, ptr %hp
  %c2 = sub i64 %cnt, 1
  store i64 %c2, ptr %np
  ; A SLOT FREED: whoever is waiting for room is now able to proceed.
  %sw = getelementptr %npk.chan, ptr %ch, i32 0, i32 10
  call void @npk_ch_wake_one(ptr %sw)
  call void @npk_ch_unlock(ptr %ch)
  ret i32 0
nothing:
  ; EMPTY AND CLOSED IS AN END, NOT A WAIT. A closed channel that still holds
  ; values delivers them first — a producer's last writes are not lost by its
  ; closing.
  %clp = getelementptr %npk.chan, ptr %ch, i32 0, i32 7
  %cl = load i32, ptr %clp
  %isclosed = icmp ne i32 %cl, 0
  br i1 %isclosed, label %closed, label %wouldblock
wouldblock:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 1
closed:
  call void @npk_ch_unlock(ptr %ch)
  ret i32 -4108
stale:
  ret i32 -4106
}

; --- threads (D-181, 1.1.9c) -------------------------------------------------
;
; A THREAD IS A CLONE WITH ITS OWN EXECUTOR. `clone(2)` rather than
; pthread_create: the zero-dependency rule, and this runtime already owns
; `_start`, guard-paged mmap and `exit`. The child gets
;   [ PROT_NONE guard | 2 MiB stack ]
; from one mmap — the three-region shape `wildx` already uses — plus a
; two-word TLS block `[ self | executor ]` that CLONE_SETTLS points `%fs` at,
; honouring the ABI's `%fs:0 = self` convention and taking `%fs:8` for ours.
;
; CHILD_CLEARTID IS THE JOIN. The kernel zeroes a word and futex-wakes it when
; the thread exits, so the joining scope waits on exactly that word — which is
; what makes D-083's "there is no thread handle" implementable rather than
; aspirational: nothing has to be named to be joined.
@npk_thread_stack_bytes = internal constant i64 2097152

; The child's trampoline arguments, handed over in its own TLS block so no
; shared state is read after the clone returns.
;   [ 0 self | 1 exec | 2 root frame | 3 resume fn | 4 tid word ]

define ptr @npk_thread_start(ptr %root, ptr %resume, i64 %join_ns) {
entry:
  ; stack: guard page + 2 MiB in one mapping, the three-region shape `wildx`
  ; already uses.
  %sz = load i64, ptr @npk_thread_stack_bytes
  %tot = add i64 %sz, 4096
  %bi = call i64 @npk_hmap(i64 %tot)
  %stack_lo = add i64 %bi, 4096
  ; the guard page is made PROT_NONE, and a FAILURE THERE IS FATAL: a stack
  ; without its guard turns an overflow into silent corruption of whatever
  ; the allocator put below it, which is the class this runtime exists to
  ; refuse.
  %mp = call i64 @npk_sys6(i64 10, i64 %bi, i64 4096, i64 0, i64 0, i64 0, i64 0)
  %mpbad = icmp ne i64 %mp, 0
  br i1 %mpbad, label %noguard, label %guarded
noguard:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
guarded:
  %sp = add i64 %stack_lo, %sz

  ; the child's executor and its TLS block. The executor carries the join
  ; deadline the `joins` clause stated (D-083: fixed where the executor is
  ; created) and the same wind-up grace every executor uses.
  ; ZEROED, not merely allocated: an executor's queue heads must start empty,
  ; and `npk_alloc` hands back whatever the slab held. The first thread this
  ; runtime ever started appended its root task to a garbage tail pointer.
  %ex = call ptr @npk_alloc_internal(i64 ptrtoint (ptr getelementptr (%npk.exec, ptr null, i32 1) to i64))
  call void @npk_zero(ptr %ex, i64 ptrtoint (ptr getelementptr (%npk.exec, ptr null, i32 1) to i64))
  %jn = getelementptr %npk.exec, ptr %ex, i32 0, i32 6
  store i64 %join_ns, ptr %jn
  %gr = getelementptr %npk.exec, ptr %ex, i32 0, i32 7
  %mg = call i64 @npk_windup_grace()
  store i64 %mg, ptr %gr

  %tls = call ptr @npk_alloc_internal(i64 ptrtoint (ptr getelementptr (%npk.tls, ptr null, i32 1) to i64))
  %t_self = getelementptr %npk.tls, ptr %tls, i32 0, i32 0
  store ptr %tls, ptr %t_self
  %t_exec = getelementptr %npk.tls, ptr %tls, i32 0, i32 1
  store ptr %ex, ptr %t_exec
  %t_root = getelementptr %npk.tls, ptr %tls, i32 0, i32 2
  store ptr %root, ptr %t_root
  %t_res = getelementptr %npk.tls, ptr %tls, i32 0, i32 3
  store ptr %resume, ptr %t_res
  %t_tid = getelementptr %npk.tls, ptr %tls, i32 0, i32 4
  store i32 0, ptr %t_tid

  ; 0x3d0f00 = CLONE_VM 0x100 | FS 0x200 | FILES 0x400 | SIGHAND 0x800
  ;          | THREAD 0x10000 | SYSVSEM 0x40000 | SETTLS 0x80000
  ;          | PARENT_SETTID 0x100000 | CHILD_CLEARTID 0x200000
  ;
  ; PARENT_SETTID AND CHILD_CLEARTID ARE A PAIR, and the first cut had only
  ; the second: the kernel cleared the word at exit but nothing ever WROTE
  ; the tid, so the join read zero, concluded the thread had finished, and
  ; freed the frame a running thread was still executing on. The word is
  ; both the handle and the wake, so it needs both halves.
  ;
  ; Written as one literal with its bits named — a clone flag word off by a
  ; bit is a thread that shares the wrong thing.
  %tlsi = ptrtoint ptr %tls to i64
  %ctid = ptrtoint ptr %t_tid to i64
  %r = call i64 @npk_clone_raw(i64 4001536, i64 %sp, i64 %ctid, i64 %tlsi,
                               ptr @npk_thread_entry, i64 %tlsi)
  %failed = icmp slt i64 %r, 0
  br i1 %failed, label %bad, label %ok
bad:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4103)
  unreachable
ok:
  ret ptr %tls
}

; THE CHILD'S ENTRY, reached with a fresh frame on its own stack. This is why
; the clone rides an assembly trampoline rather than a branch in IR: after
; `clone` the child's `%rsp` is the new stack, while every frame offset the
; compiler laid out belongs to the parent — a child that "continues" in IR
; reads its own uninitialised stack for anything spilled. The trampoline
; hands control here with a real call, so this function is ordinary code.
define void @npk_thread_entry(i64 %tlsi) {
entry:
  %tls = inttoptr i64 %tlsi to ptr
  %rp = getelementptr %npk.tls, ptr %tls, i32 0, i32 2
  %root = load ptr, ptr %rp
  call void @npk_rq_push(ptr %root)
  %rc = call i32 @npk_run_until(ptr %root, i64 0)
  ret void
}

define void @npk_thread_exit() noreturn {
entry:
  %r = call i64 @npk_sys6(i64 60, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

; THE JOIN: wait on the CHILD_CLEARTID word until the kernel zeroes it, under
; the deadline. Returns 0 when the thread exited, 1 when the deadline passed.
;
; THE WAIT IS NOT PRIVATE, AND THAT IS THE WHOLE POINT (found at 1.4.4). It
; was `FUTEX_WAIT_BITSET | FUTEX_PRIVATE_FLAG` (op 137) from 1.1.9 until then,
; and every thread join in the ecosystem slept its ENTIRE deadline -- five
; seconds -- before succeeding. The kernel clears this word at thread exit and
; wakes it from `mm_release`, and that wake is a SHARED futex wake; a private
; waiter hashes to a different key and never receives it. So the word was
; zeroed promptly, the wake went nowhere, the wait ran to its absolute timeout,
; and the loop then reloaded the word, saw zero and reported success. Correct
; answer, five seconds late, every time -- which is why nothing ever failed and
; nothing ever pointed at it. glibc's `lll_wait_tid` is non-private for exactly
; this reason.
;
; It is not only speed. D-062 makes the join deadline MANDATORY so that a stuck
; task is caught, and a join that consumes the whole deadline on every success
; makes "finished" and "stuck" indistinguishable for its full length -- the
; measurement the deadline exists to make was being thrown away. Measured
; before and after on the same machine: `mutex_basic` 5.00s -> 0.12s,
; `thread_spawn_join` 5.00s -> 0.12s, `join_order` 5.00s -> 0.06s. The
; programs that still take seconds are the ones that mean to: `windup_drop`
; drives a real wind-up (deadline + grace), `sync_prims` waits a real 2s.
define i32 @npk_thread_join(ptr %tls, i64 %dl) {
entry:
  %tp = getelementptr %npk.tls, ptr %tls, i32 0, i32 4
  br label %loop
loop:
  %t = load i32, ptr %tp
  %gone = icmp eq i32 %t, 0
  br i1 %gone, label %done, label %check
check:
  %now = call i64 @npk_mono_now()
  %over = icmp sgt i64 %now, %dl
  br i1 %over, label %expired, label %wait
wait:
  ; FUTEX_WAIT on the tid word, with the join deadline as the absolute
  ; monotonic timeout — the same clock discipline every wait here uses.
  %ts = alloca [2 x i64], align 16
  %sec = sdiv i64 %dl, 1000000000
  %rem = srem i64 %dl, 1000000000
  %s0 = getelementptr [2 x i64], ptr %ts, i64 0, i64 0
  store i64 %sec, ptr %s0
  %s1 = getelementptr [2 x i64], ptr %ts, i64 0, i64 1
  store i64 %rem, ptr %s1
  %tsi = ptrtoint ptr %ts to i64
  %wp = ptrtoint ptr %tp to i64
  %te = zext i32 %t to i64
  %fr = call i64 @npk_sys6(i64 202, i64 %wp, i64 9, i64 %te, i64 %tsi,
                           i64 0, i64 -1)
  br label %loop
done:
  ret i32 0
expired:
  ret i32 1
}

; How many hardware threads the program may use (D-073: the prototype
; hardcoded 4). sched_getaffinity over a 1024-bit mask, popcounted.
define i64 @npk_hardware_concurrency() {
entry:
  %mask = alloca [16 x i64], align 16
  %mi = ptrtoint ptr %mask to i64
  %z = call i64 @npk_sys6(i64 204, i64 0, i64 128, i64 %mi, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %z, 0
  br i1 %bad, label %one, label %count
count:
  br label %loop
loop:
  %i = phi i64 [ 0, %count ], [ %ni, %step ]
  %acc = phi i64 [ 0, %count ], [ %nacc, %step ]
  %at_end = icmp uge i64 %i, 16
  br i1 %at_end, label %fin, label %step
step:
  %wp2 = getelementptr [16 x i64], ptr %mask, i64 0, i64 %i
  %w = load i64, ptr %wp2
  %pc = call i64 @llvm.ctpop.i64(i64 %w)
  %nacc = add i64 %acc, %pc
  %ni = add i64 %i, 1
  br label %loop
fin:
  %none = icmp eq i64 %acc, 0
  br i1 %none, label %one, label %give
give:
  ret i64 %acc
one:
  ret i64 1
}

declare i64 @llvm.ctpop.i64(i64)
declare i64 @npk_clone_raw(i64, i64, i64, i64, ptr, i64)

; ---------------------------------------------------------------------------
; THE DRIVER REGISTRY (1.1.13a; D-149 over D-055, v3 plan §8).
;
; Every foreign capability runs as a SUPERVISED CHILD PROCESS -- and since
; 1.4.8 (D-206) so does every tool the build spawns: `npkc`, `llc`, `ld.lld`,
; a test binary. One registry, one clone (`npk_clone_exec`), and this table
; is what makes "supervised" hold on the failure paths: it is preallocated
; .bss (failsafe cannot allocate, D-014), CAS-claimed, and walked by the trap
; route itself — `npk_driver_kill_all` runs BEFORE user `failsafe`, because
; safing is mechanism, not policy (D-013), and an uncontrolled driver DURING
; failsafe is exactly the hazard class D-055 exists for (actuators live while
; the runtime is dying).
;
; 16 entries × 4 words [ state | pid | pidfd | pad ], flat i32s. state is
; 0 free / 1 claiming / 2 active, atomic; pid is diagnostic only; the PIDFD
; is the kill handle — signals travel through it exclusively, so a signal
; after the child is reaped answers ESRCH and can never touch a recycled pid
; (poc test 2). Fixed capacity is the point, not a limit to lift: spawn
; refuses when full (-EAGAIN), bounded like everything failsafe walks.
;
; THE ENTRY OUTLIVES EVERY RESOURCE IT GUARDS (v3 §4.2): the slot is
; PUBLISHED (state 2) before the clone, with pidfd prefilled -1 (the walker
; skips a negative pidfd), and CLONE_PIDFD's parent_tidptr aims INTO THE
; SLOT — the kernel writes the kill handle into registry storage during the
; clone itself, so there is never an instant where a live child has no
; killable entry. Retirement is teardown's LAST step, after the process is
; dead and reaped; the harmless residue of that order (kill_all signalling a
; pidfd whose process was just reaped) is an ESRCH, by design.
@npk_driver_reg = internal global [64 x i32] zeroinitializer

; Spawn a SUPERVISED CHILD PROCESS: claim a registry slot, clone(SIGCHLD |
; CLONE_PIDFD), and in the child run the fixed descriptor-and-exec sequence.
; Returns Result<int64> ({ pid, 0 } or { 0, -errno }; -EAGAIN when the
; registry is full, -EINVAL when the block breaks the descriptor rule).
;
; ONE PRIMITIVE FOR EVERY CHILD (D-206, 1.4.8). This was the driver's
; clone-exec with the driver's descriptor map baked in -- stdin and stdout
; to /dev/null, a mandatory control fd on 3. The build's tools are the same
; kind of thing to the runtime: a process it must be able to kill on the
; trap path and must not abandon at a clean exit (D-188), so they share the
; registry and this one clone, and what differed between them is now what
; the block carries in full. Ten i64s, PREPARED BY THE CALLER BEFORE THE
; CLONE (the npk_thread precedent -- no shared state is read after the child
; exists that was not written before it):
;   [0] path   (ptr, NUL-terminated)      [5] stderr fd    (dup3 -> 2)
;   [1] argv   (ptr, NULL-terminated)     [6] ctrl fd      (dup3 -> 3), < 0 for none
;   [2] envp   (ptr, NULL-terminated)     [7] parent pid   (pre-recorded)
;   [3] stdin fd  (dup3 -> 0)             [8] OUT: registry slot
;   [4] stdout fd (dup3 -> 1)             [9] OUT: pidfd
;
; THE CHILD PATH IS ALLOCATION-FREE BY CONSTRUCTION: it reads the block and
; issues raw syscalls, nothing else. Another thread may hold the allocator
; futex at clone time, and the child -- a copy with ONE thread -- would
; deadlock on the copied lock word at its first allocation. The same
; reasoning bars running ANY Nitpick code in the child: prctl, getppid,
; dup3 x3 (x4 with a ctrl fd), execve, exit_group(127), in that order, and
; nothing more.
;
; THE DESCRIPTOR RULE, CHECKED HERE BEFORE ANYTHING IS CLAIMED: every
; child-bound source ([3..5], and [6] when present) is >= 4. The dup3
; shuffle onto 0/1/2/3 must never clobber a source before it is consumed,
; and dup3 refuses oldfd == newfd, so a low descriptor is re-homed upward by
; the caller (F_DUPFD_CLOEXEC; `fd_above_std` in lib/nsys.npk) -- "the child
; inherits our stdin" is spelled by dup'ing it above the trio first, one
; rule for every slot. It was a comment the caller was trusted to honour;
; a block that breaks it is now refused as -EINVAL, before the clone, where
; the failure is still a value. dup3 with flags 0 clears CLOEXEC on the new
; fd: exactly the descriptors meant to survive the exec do, and nothing
; else does (birth-CLOEXEC makes fd-leak-freedom structural, v3 s4.3).
define { i64, i32 } @npk_clone_exec(ptr %blk) {
entry:
  %p3 = getelementptr i64, ptr %blk, i64 3
  %f3 = load i64, ptr %p3
  %p4 = getelementptr i64, ptr %blk, i64 4
  %f4 = load i64, ptr %p4
  %p5 = getelementptr i64, ptr %blk, i64 5
  %f5 = load i64, ptr %p5
  %p6 = getelementptr i64, ptr %blk, i64 6
  %f6 = load i64, ptr %p6
  %lo3 = icmp slt i64 %f3, 4
  %lo4 = icmp slt i64 %f4, 4
  %lo5 = icmp slt i64 %f5, 4
  %has6 = icmp sge i64 %f6, 0
  %lo6a = icmp slt i64 %f6, 4
  %lo6 = and i1 %has6, %lo6a
  %bad1 = or i1 %lo3, %lo4
  %bad2 = or i1 %bad1, %lo5
  %bad = or i1 %bad2, %lo6
  br i1 %bad, label %einval, label %scan
einval:
  ; a child-bound descriptor below the std trio: refused, nothing claimed.
  ret { i64, i32 } { i64 0, i32 -22 }
scan:
  %i = phi i64 [ 0, %entry ], [ %inx, %miss ]
  %full = icmp sge i64 %i, 16
  br i1 %full, label %nofree, label %try
try:
  %base = mul i64 %i, 4
  %sp = getelementptr i32, ptr @npk_driver_reg, i64 %base
  %cx = cmpxchg ptr %sp, i32 0, i32 1 acq_rel monotonic
  %won = extractvalue { i32, i1 } %cx, 1
  br i1 %won, label %claimed, label %miss
miss:
  %inx = add i64 %i, 1
  br label %scan
nofree:
  ; the registry is FULL: refuse, bounded -- the D-055 posture, never grow.
  ret { i64, i32 } { i64 0, i32 -11 }
claimed:
  %b1 = add i64 %base, 1
  %b2 = add i64 %base, 2
  %pidp = getelementptr i32, ptr @npk_driver_reg, i64 %b1
  %pfdp = getelementptr i32, ptr @npk_driver_reg, i64 %b2
  store i32 0, ptr %pidp
  store i32 -1, ptr %pfdp
  ; PUBLISH BEFORE THE CLONE (release pairs with the walker's acquire): from
  ; here a trap on any thread finds this slot, even though the child does
  ; not exist yet -- the walker skips pidfd -1, and the kernel overwrites it
  ; with the real pidfd during the clone below.
  store atomic i32 2, ptr %sp release, align 4
  ; clone(SIGCHLD | CLONE_PIDFD, stack 0, parent_tidptr = &slot.pidfd).
  ; 4113 = SIGCHLD 17 | CLONE_PIDFD 0x1000. Stack 0 is fork-shape: the child
  ; continues HERE on a copy-on-write copy of this stack -- legal in ordinary
  ; IR (unlike the CLONE_VM thread clone, which must ride the asm
  ; trampoline), because every frame offset it reads is its own copy.
  %pfdi = ptrtoint ptr %pfdp to i64
  %r = call i64 @npk_sys6(i64 56, i64 4113, i64 0, i64 %pfdi, i64 0, i64 0, i64 0)
  %isch = icmp eq i64 %r, 0
  br i1 %isch, label %child, label %parent
parent:
  %failed = icmp slt i64 %r, 0
  br i1 %failed, label %cfail, label %ok
cfail:
  ; the clone itself failed: retire the claimed slot, hand the errno up.
  store atomic i32 0, ptr %sp release, align 4
  %ec = trunc i64 %r to i32
  %f0 = insertvalue { i64, i32 } zeroinitializer, i64 0, 0
  %f1 = insertvalue { i64, i32 } %f0, i32 %ec, 1
  ret { i64, i32 } %f1
ok:
  %pc = trunc i64 %r to i32
  store i32 %pc, ptr %pidp
  %pfd = load i32, ptr %pfdp
  %o8 = getelementptr i64, ptr %blk, i64 8
  store i64 %i, ptr %o8
  %pfde = sext i32 %pfd to i64
  %o9 = getelementptr i64, ptr %blk, i64 9
  store i64 %pfde, ptr %o9
  %k0 = insertvalue { i64, i32 } zeroinitializer, i64 %r, 0
  %k1 = insertvalue { i64, i32 } %k0, i32 0, 1
  ret { i64, i32 } %k1
child:
  ; THE CHILD. PDEATHSIG first -- if the runtime dies by ANY means, SIGKILL
  ; included (where failsafe never runs), the kernel reaps this process
  ; (poc test 3). The prctl races the parent dying between clone and here;
  ; the recorded-parent check closes it: a mismatched getppid means the
  ; death signal is already unarmed, so exit now.
  %d1 = call i64 @npk_sys6(i64 157, i64 1, i64 9, i64 0, i64 0, i64 0, i64 0)
  %pp = call i64 @npk_sys6(i64 110, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  %wantp = getelementptr i64, ptr %blk, i64 7
  %want = load i64, ptr %wantp
  %orph = icmp ne i64 %pp, %want
  br i1 %orph, label %cdie, label %harden
harden:
  ; NO_NEW_PRIVS: unconditional, free -- a compromised child cannot escalate
  ; through setuid/fscaps, and it is the precondition for any later seccomp
  ; filter (v3 s12).
  %d2 = call i64 @npk_sys6(i64 157, i64 38, i64 1, i64 0, i64 0, i64 0, i64 0)
  %d3 = call i64 @npk_sys6(i64 292, i64 %f3, i64 0, i64 0, i64 0, i64 0, i64 0)
  %d4 = call i64 @npk_sys6(i64 292, i64 %f4, i64 1, i64 0, i64 0, i64 0, i64 0)
  %d5 = call i64 @npk_sys6(i64 292, i64 %f5, i64 2, i64 0, i64 0, i64 0, i64 0)
  br i1 %has6, label %dupctrl, label %doexec
dupctrl:
  %d6 = call i64 @npk_sys6(i64 292, i64 %f6, i64 3, i64 0, i64 0, i64 0, i64 0)
  br label %doexec
doexec:
  %pathp = getelementptr i64, ptr %blk, i64 0
  %path = load i64, ptr %pathp
  %argvp = getelementptr i64, ptr %blk, i64 1
  %argv = load i64, ptr %argvp
  %envpp = getelementptr i64, ptr %blk, i64 2
  %envp = load i64, ptr %envpp
  %dx = call i64 @npk_sys6(i64 59, i64 %path, i64 %argv, i64 %envp, i64 0, i64 0, i64 0)
  br label %cdie
cdie:
  ; execve returned (or the parent vanished pre-prctl): nothing to clean --
  ; the child owns no Nitpick state -- and one obligation: STOP. 127 is the
  ; shell's exec-failure convention; a driver's parent observes it as ctrl
  ; EOF at the handshake, a tool's as the exit status the pidfd reports.
  %dz = call i64 @npk_sys6(i64 231, i64 127, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

; Retire a registry slot — teardown's LAST step (v3 §4.2), after the process
; is dead and reaped. A retire of a slot that is not active is the registry's
; double-free: a defect, and it traps as one (-4102), exactly as the
; allocator treats a foreign pointer.
define void @npk_driver_retire(i64 %slot) {
entry:
  %neg = icmp slt i64 %slot, 0
  %big = icmp sge i64 %slot, 16
  %oob = or i1 %neg, %big
  br i1 %oob, label %bad, label %check
check:
  %base = mul i64 %slot, 4
  %sp = getelementptr i32, ptr @npk_driver_reg, i64 %base
  %st = load atomic i32, ptr %sp acquire, align 4
  %live = icmp eq i32 %st, 2
  br i1 %live, label %clear, label %bad
clear:
  store atomic i32 0, ptr %sp release, align 4
  ret void
bad:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
}

; SIGKILL every registered driver, via pidfd only. Called by the trap route
; before user `failsafe` runs — and callable nowhere else. No graceful
; shutdown, no reaping, no munmap, no state change: SAFING, not cleanup. A
; pidfd whose process is gone answers ESRCH; a prefilled -1 (mid-clone slot)
; is skipped — that child, if it materialises, dies with the runtime by
; PDEATHSIG, the layer beneath this one.
define void @npk_driver_kill_all() {
entry:
  br label %scan
scan:
  %i = phi i64 [ 0, %entry ], [ %inx, %next ]
  %done = icmp sge i64 %i, 16
  br i1 %done, label %fin, label %look
look:
  %base = mul i64 %i, 4
  %sp = getelementptr i32, ptr @npk_driver_reg, i64 %base
  %st = load atomic i32, ptr %sp acquire, align 4
  %live = icmp eq i32 %st, 2
  br i1 %live, label %kill, label %next
kill:
  %b2 = add i64 %base, 2
  %pfdp = getelementptr i32, ptr @npk_driver_reg, i64 %b2
  %pfd = load i32, ptr %pfdp
  %unset = icmp slt i32 %pfd, 0
  br i1 %unset, label %next, label %sig
sig:
  ; pidfd_send_signal(pidfd, SIGKILL, NULL, 0) — allocation-free,
  ; mask-independent, ESRCH-safe against pid reuse.
  %pfde = sext i32 %pfd to i64
  %z = call i64 @npk_sys6(i64 424, i64 %pfde, i64 9, i64 0, i64 0, i64 0, i64 0)
  br label %next
next:
  %inx = add i64 %i, 1
  br label %scan
fin:
  ret void
}

; How many drivers are registered — what the controlled exit checks (D-188,
; the D-151 K-semantics rule extended to the second registry): a clean exit
; 0 with a live driver is a program that never decided its driver's fate,
; and it traps (-4109) rather than abandoning a supervised process to the
; kernel backstops. The backstops still hold — kill_all runs on that trap's
; path, then PDEATHSIG at exit_group — so the trap is the REPORT, and the
; safing is unconditional either way.
define i64 @npk_driver_live_count() {
entry:
  br label %scan
scan:
  %i = phi i64 [ 0, %entry ], [ %inx, %step ]
  %acc = phi i64 [ 0, %entry ], [ %nacc, %step ]
  %done = icmp sge i64 %i, 16
  br i1 %done, label %fin, label %step
step:
  %base = mul i64 %i, 4
  %sp = getelementptr i32, ptr @npk_driver_reg, i64 %base
  %st = load atomic i32, ptr %sp acquire, align 4
  %live = icmp eq i32 %st, 2
  %one = zext i1 %live to i64
  %nacc = add i64 %acc, %one
  %inx = add i64 %i, 1
  br label %scan
fin:
  ret i64 %acc
}

; THE READY QUEUE and THE SLEEPERS, both intrusive through `qnext` — a task is
; ready or sleeping, never both, so one link word serves. `wake_at` is the
; sleeper's absolute monotonic timepoint, and **-1 once the task has
; completed**: a task never sleeps to a negative timepoint, so the marker
; cannot collide with a real deadline, and a join that arrives after its
; child already finished reads it instead of waiting forever.

define void @npk_rq_push(ptr %f) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_rq_head = getelementptr %npk.exec, ptr %ex, i32 0, i32 0
  %p_npk_rq_tail = getelementptr %npk.exec, ptr %ex, i32 0, i32 1
  %qn = getelementptr %npk.hdr, ptr %f, i32 0, i32 7
  store ptr null, ptr %qn
  %t = load ptr, ptr %p_npk_rq_tail
  %empty = icmp eq ptr %t, null
  br i1 %empty, label %first, label %append
first:
  store ptr %f, ptr %p_npk_rq_head
  store ptr %f, ptr %p_npk_rq_tail
  ret void
append:
  %tq = getelementptr %npk.hdr, ptr %t, i32 0, i32 7
  store ptr %f, ptr %tq
  store ptr %f, ptr %p_npk_rq_tail
  ret void
}

define ptr @npk_rq_pop() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_rq_head = getelementptr %npk.exec, ptr %ex, i32 0, i32 0
  %p_npk_rq_tail = getelementptr %npk.exec, ptr %ex, i32 0, i32 1
  %h = load ptr, ptr %p_npk_rq_head
  %none = icmp eq ptr %h, null
  br i1 %none, label %empty, label %take
empty:
  ret ptr null
take:
  %hq = getelementptr %npk.hdr, ptr %h, i32 0, i32 7
  %nx = load ptr, ptr %hq
  store ptr %nx, ptr %p_npk_rq_head
  %last = icmp eq ptr %nx, null
  br i1 %last, label %clear, label %done
clear:
  store ptr null, ptr %p_npk_rq_tail
  br label %done
done:
  store ptr null, ptr %hq
  ret ptr %h
}

define void @npk_sl_push(ptr %f, i64 %at) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_sl_head = getelementptr %npk.exec, ptr %ex, i32 0, i32 2
  ; A WAKE THAT LANDED BEFORE THE SLEEP MUST SURVIVE IT. Registering on a
  ; channel and being pushed onto this list are two steps, and the channel's
  ; lock covers only the first — so a peer on another thread can wake this
  ; task in between, and a plain store of the deadline here would erase that.
  ; The task then slept to its deadline for a value that had already arrived,
  ; and the deadline is the only thing that ever woke it: a sender reporting
  ; DeadlineExceeded with room in the buffer, or an executor with nothing
  ; ready and nothing sleeping declaring deadlock, both under load and neither
  ; reproducibly. 27 runs in 200 before the exchange went in.
  %wa = getelementptr %npk.hdr, ptr %f, i32 0, i32 8
  %prev = atomicrmw xchg ptr %wa, i64 %at seq_cst
  %woken = icmp eq i64 %prev, 1
  br i1 %woken, label %keep, label %sleep
keep:
  ; put the marker back: this task is due now, not at `at`
  store atomic i64 1, ptr %wa seq_cst, align 8
  br label %sleep
sleep:
  %qn = getelementptr %npk.hdr, ptr %f, i32 0, i32 7
  %h = load ptr, ptr %p_npk_sl_head
  store ptr %h, ptr %qn
  store ptr %f, ptr %p_npk_sl_head
  ret void
}

; The earliest sleeper's timepoint, or 0 when nothing sleeps.
define i64 @npk_sl_earliest() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_sl_head = getelementptr %npk.exec, ptr %ex, i32 0, i32 2
  %h = load ptr, ptr %p_npk_sl_head
  br label %loop
loop:
  %cur = phi ptr [ %h, %entry ], [ %nx, %step ]
  %best = phi i64 [ 0, %entry ], [ %nb, %step ]
  %at_end = icmp eq ptr %cur, null
  br i1 %at_end, label %done, label %step
step:
  %wa = getelementptr %npk.hdr, ptr %cur, i32 0, i32 8
  %w = load atomic i64, ptr %wa seq_cst, align 8
  %unset = icmp eq i64 %best, 0
  %lower = icmp slt i64 %w, %best
  %take = or i1 %unset, %lower
  %nb = select i1 %take, i64 %w, i64 %best
  %qn = getelementptr %npk.hdr, ptr %cur, i32 0, i32 7
  %nx = load ptr, ptr %qn
  br label %loop
done:
  ret i64 %best
}

; Move every sleeper whose timepoint has arrived onto the ready queue. The
; list is rebuilt rather than spliced: one pass, no removal bookkeeping.
define void @npk_sl_wake_due(i64 %now) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_sl_head = getelementptr %npk.exec, ptr %ex, i32 0, i32 2
  ; the cursor is a SLOT rather than a phi: the body rejoins the head from
  ; two blocks, and a phi over three predecessors written as two is the
  ; verifier error this cost once already.
  %cs = alloca ptr, align 8
  %h = load ptr, ptr %p_npk_sl_head
  store ptr %h, ptr %cs
  store ptr null, ptr %p_npk_sl_head
  br label %loop
loop:
  %cur = load ptr, ptr %cs
  %at_end = icmp eq ptr %cur, null
  br i1 %at_end, label %done, label %again
again:
  %qn = getelementptr %npk.hdr, ptr %cur, i32 0, i32 7
  %nx = load ptr, ptr %qn
  store ptr %nx, ptr %cs
  %wa = getelementptr %npk.hdr, ptr %cur, i32 0, i32 8
  %w = load atomic i64, ptr %wa seq_cst, align 8
  %due = icmp sle i64 %w, %now
  br i1 %due, label %ready, label %keep
ready:
  call void @npk_rq_push(ptr %cur)
  br label %loop
keep:
  %sh = load ptr, ptr %p_npk_sl_head
  store ptr %sh, ptr %qn
  store ptr %cur, ptr %p_npk_sl_head
  br label %loop
done:
  ret void
}

; ONE STEP OF THE EXECUTOR (D-071). Returns the frame it drove to DONE, or
; null. Everything blocking in the language ends up here: a task that waits
; is a task off the ready queue, so a SIBLING runs — which is the property
; D-071 exists to guarantee and the reason blocking is never a syscall on
; the thread while work remains.
define ptr @npk_step(i64 %dl) {
entry:
  %fz = load i32, ptr @npk_frozen
  %stop = icmp ne i32 %fz, 0
  br i1 %stop, label %frozen, label %go
frozen:
  ; D-063: after a trap nothing is resumed, anywhere, ever again.
  call void @npk_trap(i32 -4102)
  unreachable
go:
  %now = call i64 @npk_mono_now()
  call void @npk_sl_wake_due(i64 %now)
  %t = call ptr @npk_rq_pop()
  %idle = icmp eq ptr %t, null
  br i1 %idle, label %nothing, label %run
run:
  ; the one resume site records WHICH task is running: nested waits and the
  ; reactor register the task root, not the coroutine frame they sit in
  %exr = call ptr @npk_exec()
  %ctp = getelementptr %npk.exec, ptr %exr, i32 0, i32 13
  store ptr %t, ptr %ctp
  ; CONSUME THE WAKE MARKER (1.1.13b) — UNLESS THE TASK IS WOUND UP. The 1
  ; a waker stamps into `wake_at` (channel, reactor, rouse) survives the
  ; registration-vs-sleep race by `npk_sl_push`'s exchange — but a marker
  ; still standing at the RESUME it caused is SPENT, and leaving it made
  ; every later sleep of a once-woken task due on arrival: the executor
  ; never parked again, every wait after the first wake a busy-poll.
  ; Invisible for a year of retry-loop waits (they re-poll correctly, just
  ; hot); the first task to SLEEP after an io wake — the Bridge mock's hang
  ; kernel — returned instantly and turned a deadline test into a fault
  ; test. cmpxchg 1→0: only the due marker is consumed — a completed
  ; task's -1 and a deadline timepoint pass through, and a waker's 1
  ; landing after this clear is a live registration's, worth exactly the
  ; one spurious resume the retry discipline absorbs.
  ;
  ; THE WOUND EXEMPTION is windup_drain.npk's documented contract: "a wound
  ; task cannot linger in a wait — every park returns immediately", which
  ; is precisely the persistent 1 doing its ONE legitimate job. The windup
  ; word (slot 2) is stored before the due stamp and read after the
  ; sweep's seq_cst load of it, so a wound resume always sees it set.
  %wup = getelementptr %npk.hdr, ptr %t, i32 0, i32 2
  %wu = load i32, ptr %wup
  %wound = icmp ne i32 %wu, 0
  br i1 %wound, label %resume, label %spend
spend:
  %wam = getelementptr %npk.hdr, ptr %t, i32 0, i32 8
  %wac = cmpxchg ptr %wam, i64 1, i64 0 seq_cst seq_cst, align 8
  br label %resume
resume:
  %rfp = getelementptr %npk.hdr, ptr %t, i32 0, i32 0
  %rf = load ptr, ptr %rfp
  %rc = call i8 %rf(ptr %t)
  %done = icmp eq i8 %rc, 0
  br i1 %done, label %finished, label %parked
finished:
  %wa = getelementptr %npk.hdr, ptr %t, i32 0, i32 8
  store atomic i64 -1, ptr %wa seq_cst, align 8
  ret ptr %t
parked:
  %at = call i64 @npk_park_take()
  %nowake = icmp eq i64 %at, 0
  br i1 %nowake, label %unwakeable, label %sleep
unwakeable:
  ; a suspension that named no wake condition: nothing could ever resume it
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
sleep:
  call void @npk_sl_push(ptr %t, i64 %at)
  ret ptr null
nothing:
  ; CLEAR THE PARK WORD, THEN LOOK AGAIN. This is the executor's half of the
  ; channel wake protocol (1.1.10-C2), and the order is the whole of it. A
  ; waker on another thread makes its task due and THEN sets this word; so a
  ; waker that ran before the clear is caught by the re-check below, and one
  ; that runs after it leaves the word non-zero, which makes the FUTEX_WAIT
  ; (expecting zero) return immediately instead of sleeping. Without the
  ; re-check, a wake that landed in the window between the last look and the
  ; sleep would be lost, and the task would wait out its deadline for a value
  ; that had already arrived.
  %ex2 = call ptr @npk_exec()
  %pw2 = getelementptr %npk.exec, ptr %ex2, i32 0, i32 5
  store atomic i32 0, ptr %pw2 seq_cst, align 4
  %now2 = call i64 @npk_mono_now()
  call void @npk_sl_wake_due(i64 %now2)
  %rqh2 = getelementptr %npk.exec, ptr %ex2, i32 0, i32 0
  %appeared = load ptr, ptr %rqh2
  %woke = icmp ne ptr %appeared, null
  br i1 %woke, label %retry, label %idle2
retry:
  ; something became runnable; the caller loops and steps again
  ret ptr null
idle2:
  %e = call i64 @npk_sl_earliest()
  %dead = icmp eq i64 %e, 0
  br i1 %dead, label %deadlock, label %wait
deadlock:
  ; nothing ready and nothing sleeping, with work outstanding: no future
  ; event can advance the program
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
wait:
  ; THE WAIT IS CLAMPED BY THE CALLER'S DEADLINE. Without this the executor
  ; sleeps to the earliest sleeper — which may be far past a join deadline —
  ; and the containment D-062 promises becomes a wait. The first
  ; long-sleeping child caught it: a 30-second task ran to completion under
  ; a 120ms join.
  %has_dl = icmp ne i64 %dl, 0
  br i1 %has_dl, label %clamp, label %plain
clamp:
  %sooner = icmp slt i64 %dl, %e
  %until = select i1 %sooner, i64 %dl, i64 %e
  call void @npk_park_sleep(i64 %until)
  ret ptr null
plain:
  call void @npk_park_sleep(i64 %e)
  ret ptr null
}

; Has this task completed? (`wake_at` is -1 exactly then.)
define i32 @npk_task_done(ptr %f) {
entry:
  %wa = getelementptr %npk.hdr, ptr %f, i32 0, i32 8
  %w = load atomic i64, ptr %wa seq_cst, align 8
  %d = icmp eq i64 %w, -1
  %r = zext i1 %d to i32
  ret i32 %r
}

; RUN UNTIL ONE TASK COMPLETES — the shape both the entry shim (root) and the
; join (each child) need. `dl` is an absolute monotonic deadline, or 0 for
; none. Returns 0 when the target completed, 1 when the deadline expired.
define i32 @npk_run_until(ptr %target, i64 %dl) {
entry:
  br label %loop
loop:
  %already = call i32 @npk_task_done(ptr %target)
  %fin = icmp ne i32 %already, 0
  br i1 %fin, label %ok, label %check
check:
  %has_dl = icmp ne i64 %dl, 0
  br i1 %has_dl, label %timecheck, label %step
timecheck:
  %now = call i64 @npk_mono_now()
  %over = icmp sgt i64 %now, %dl
  br i1 %over, label %expired, label %step
step:
  %r = call ptr @npk_step(i64 %dl)
  br label %loop
ok:
  ret i32 0
expired:
  ret i32 1
}

; THE WIND-UP SETTER (D-177/D-083): the join's deadline expired, so every
; unfinished child in this list is told to unwind — the cooperative model,
; where a task learns at its next resume that it is being wound up and runs
; its own `defer`s. Preemptive destruction stays removed (D-062).
define void @npk_windup_all(ptr %head) {
entry:
  br label %loop
loop:
  %cur = phi ptr [ %head, %entry ], [ %nx, %next ]
  %at_end = icmp eq ptr %cur, null
  br i1 %at_end, label %done, label %mark
mark:
  %wp = getelementptr %npk.hdr, ptr %cur, i32 0, i32 2
  store i32 1, ptr %wp
  ; AND WOKEN. A wind-up a sleeping task cannot see is not a grace period,
  ; it is dead time: the flag is read at the next RESUME, so the resume has
  ; to happen. Due-ing the sleeper (wake_at 0, which every `now` is past)
  ; brings it back through the ordinary wake path — no list surgery, and a
  ; task already ready or running is unaffected because only sleepers read
  ; this word. A completed task keeps its -1.
  %wa = getelementptr %npk.hdr, ptr %cur, i32 0, i32 8
  %w = load atomic i64, ptr %wa seq_cst, align 8
  %fin = icmp eq i64 %w, -1
  br i1 %fin, label %next, label %due
due:
  ; DUE-NOW IS 1, NOT 0 -- the same load-bearing distinction the channel
  ; waker records: 1 is always in the past, and distinguishable from the 0 a
  ; fresh frame carries. And the OWNER EXECUTOR IS ROUSED (1.2.4b): a wound
  ; task sleeping on ANOTHER thread's executor is inside that executor's
  ; clamped FUTEX_WAIT -- possibly clamped to the task's own far deadline --
  ; and a due mark nobody looks at is not a wind-up. Park word then futex
  ; wake, the waker protocol npk_ch_wake_one already carries; the window is
  ; closed by the executor's clear-then-recheck on its side. A same-thread
  ; child's rouse is a harmless self-wake.
  store atomic i64 1, ptr %wa seq_cst, align 8
  %op = getelementptr %npk.hdr, ptr %cur, i32 0, i32 11
  %ow = load ptr, ptr %op
  %noown = icmp eq ptr %ow, null
  br i1 %noown, label %next, label %rouse
rouse:
  %pw = getelementptr %npk.exec, ptr %ow, i32 0, i32 5
  store atomic i32 1, ptr %pw seq_cst, align 4
  %wpi = ptrtoint ptr %pw to i64
  ; futex(word, FUTEX_WAKE|PRIVATE, 1, ...)
  %fr = call i64 @npk_sys6(i64 202, i64 %wpi, i64 129, i64 1, i64 0, i64 0, i64 0)
  ; and the reactor's eventfd, for an epoll_pwait sleeper (B-3a)
  %evpw = getelementptr %npk.exec, ptr %ow, i32 0, i32 12
  %evfdw = load atomic i32, ptr %evpw acquire, align 4
  %noevw = icmp eq i32 %evfdw, 0
  br i1 %noevw, label %next, label %pingw
pingw:
  %onew = alloca i64, align 8
  store i64 1, ptr %onew
  %onepw = ptrtoint ptr %onew to i64
  %evlw = sext i32 %evfdw to i64
  %wrw = call i64 @npk_sys6(i64 1, i64 %evlw, i64 %onepw, i64 8, i64 0, i64 0, i64 0)
  br label %next
next:
  %sp = getelementptr %npk.hdr, ptr %cur, i32 0, i32 5
  %nx = load ptr, ptr %sp
  br label %loop
done:
  ret void
}

; The program-level JOIN DEADLINE (D-083): fixed where the executor is
; created — one greppable location, an auditable value, and not "whatever the
; runtime felt like". Five seconds: long enough that no correct computation
; trips it, short enough that a stuck task is contained rather than waited on
; forever. 1.1.9's `Thread.spawn` sets it per executor.

; THE WIND-UP GRACE (D-177): how long a wound-up task has to run its own
; `defer`s and finish. Unwinding is a few resumes — microseconds in practice
; — so 250ms is enormously generous, and a SHORT stated grace keeps total
; containment at `deadline + 250ms` rather than twice the deadline. The
; first cut granted a second full deadline; halving the audit surface is
; worth more than the generosity.

define i64 @npk_windup_grace() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_windup_grace_ns = getelementptr %npk.exec, ptr %ex, i32 0, i32 7
  %v = load i64, ptr %p_npk_windup_grace_ns
  ret i64 %v
}

define i64 @npk_join_deadline() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_join_deadline_ns = getelementptr %npk.exec, ptr %ex, i32 0, i32 6
  %v = load i64, ptr %p_npk_join_deadline_ns
  ret i64 %v
}

define void @npk_park_until(i64 %at) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_park_at = getelementptr %npk.exec, ptr %ex, i32 0, i32 3
  %p_npk_park_pending = getelementptr %npk.exec, ptr %ex, i32 0, i32 4
  store i64 %at, ptr %p_npk_park_at
  store i32 1, ptr %p_npk_park_pending
  ret void
}

define i64 @npk_park_take() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_park_at = getelementptr %npk.exec, ptr %ex, i32 0, i32 3
  %p_npk_park_pending = getelementptr %npk.exec, ptr %ex, i32 0, i32 4
  %p = load i32, ptr %p_npk_park_pending
  %none = icmp eq i32 %p, 0
  br i1 %none, label %no, label %yes
yes:
  store i32 0, ptr %p_npk_park_pending
  %at = load i64, ptr %p_npk_park_at
  ret i64 %at
no:
  ret i64 0
}

; A woken task's wind-up word, noted at the wake (D-177). The setter and the
; unwinding it triggers land with the join deadline (1.1.8 stage C); today
; this records that the poll HAPPENED, which is what keeps the emitted poll
; from being dead code the optimiser may delete.
define void @npk_windup_note(i32 %w) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_windup_seen = getelementptr %npk.exec, ptr %ex, i32 0, i32 10
  store i32 %w, ptr %p_npk_windup_seen
  ret void
}

define i32 @npk_frozen_get() {
entry:
  %f = load i32, ptr @npk_frozen
  ret i32 %f
}

; SLEEP UNTIL AN ABSOLUTE MONOTONIC TIMEPOINT (D-176 rule 4): the kernel owns
; the arithmetic, so a wake-and-repark loop cannot accumulate drift.
;
; THE OP WORD IS 9|128 = 137: FUTEX_WAIT_BITSET (9) | FUTEX_PRIVATE_FLAG
; (128). CLOCK_MONOTONIC is the DEFAULT for WAIT_BITSET and
; FUTEX_CLOCK_REALTIME (256) is the opt-in — the first cut used 265, which
; read a monotonic timepoint as a realtime one, put every deadline in 1970,
; and returned instantly. The first sleeping program caught it.
define void @npk_park_sleep(i64 %at) {
entry:
  %ex = call ptr @npk_exec()
  ; THE REACTOR-ARMED WAIT (B-3a, 1.1.12a). Once any io_ready has run on
  ; this executor, the idle wait is epoll_pwait over the interest set — the
  ; registered descriptors plus the eventfd every cross-thread waker writes
  ; — bounded by the same deadline the futex wait took. The clear-then-
  ; recheck protocol is untouched: a waker between the caller's re-check and
  ; this wait leaves the eventfd readable, and the wait returns immediately.
  %evp0 = getelementptr %npk.exec, ptr %ex, i32 0, i32 12
  %evfd0 = load i32, ptr %evp0
  %armed = icmp ne i32 %evfd0, 0
  br i1 %armed, label %epoll, label %futex
futex:
  %p_npk_park_word = getelementptr %npk.exec, ptr %ex, i32 0, i32 5
  %ts = alloca [2 x i64], align 16
  %sec = sdiv i64 %at, 1000000000
  %rem = srem i64 %at, 1000000000
  %sp = getelementptr [2 x i64], ptr %ts, i64 0, i64 0
  store i64 %sec, ptr %sp
  %np = getelementptr [2 x i64], ptr %ts, i64 0, i64 1
  store i64 %rem, ptr %np
  %tp = ptrtoint ptr %ts to i64
  %wp = ptrtoint ptr %p_npk_park_word to i64
  ; futex(word, FUTEX_WAIT_BITSET|PRIVATE, expected 0, &abs_timeout, NULL, ~0)
  %r = call i64 @npk_sys6(i64 202, i64 %wp, i64 137, i64 0, i64 %tp,
                          i64 0, i64 -1)
  ret void
epoll:
  ; the futex wait re-checks its word IN the kernel (expected 0); this wait
  ; must do the same by hand, or a rouse that landed between the caller's
  ; re-check and here -- with the ping skipped on a not-yet-visible evfd --
  ; sleeps through a due task until the deadline. Left set for the caller's
  ; own clear-and-rescan, exactly as an EAGAIN futex leaves it.
  %pwq = getelementptr %npk.exec, ptr %ex, i32 0, i32 5
  %pwv = load atomic i32, ptr %pwq seq_cst, align 4
  %pwbusy = icmp ne i32 %pwv, 0
  br i1 %pwbusy, label %out, label %epgo
epgo:
  %epp = getelementptr %npk.exec, ptr %ex, i32 0, i32 11
  %epfd = load i32, ptr %epp
  %now = call i64 @npk_mono_now()
  %span = sub i64 %at, %now
  %neg = icmp slt i64 %span, 0
  %span2 = select i1 %neg, i64 0, i64 %span
  ; nanoseconds to milliseconds, rounding UP so a 1ns wait is not a busy spin
  %msu = add i64 %span2, 999999
  %ms = sdiv i64 %msu, 1000000
  %cap = icmp sgt i64 %ms, 2147483647
  %ms2 = select i1 %cap, i64 2147483647, i64 %ms
  %evbuf = alloca [192 x i8], align 8
  %ebp = ptrtoint ptr %evbuf to i64
  %epi = sext i32 %epfd to i64
  ; epoll_pwait(epfd, events, 16, timeout_ms, NULL, 8)
  %n = call i64 @npk_sys6(i64 281, i64 %epi, i64 %ebp, i64 16, i64 %ms2,
                          i64 0, i64 8)
  %none = icmp sle i64 %n, 0
  br i1 %none, label %out, label %deliver
deliver:
  br label %dloop
dloop:
  %i = phi i64 [ 0, %deliver ], [ %i2, %dnext ]
  %done = icmp sge i64 %i, %n
  br i1 %done, label %out, label %done1
done1:
  ; the packed epoll_event: [ u32 events | u64 data ] at 12-byte stride
  %off = mul i64 %i, 12
  %da = add i64 %off, 4
  %dp0 = getelementptr i8, ptr %evbuf, i64 %da
  %data = load i64, ptr %dp0, align 4
  %isev = icmp eq i64 %data, 0
  br i1 %isev, label %drain, label %due
drain:
  ; the eventfd's counter resets on read; the wake it carried is spent
  %tmp8 = alloca i64, align 8
  %t8 = ptrtoint ptr %tmp8 to i64
  %evi = sext i32 %evfd0 to i64
  %dr = call i64 @npk_sys6(i64 0, i64 %evi, i64 %t8, i64 8, i64 0, i64 0, i64 0)
  br label %dnext
due:
  ; the event's payload is the waiting FRAME: due it, the sweep runs it
  %frp = inttoptr i64 %data to ptr
  %wa = getelementptr %npk.hdr, ptr %frp, i32 0, i32 8
  store atomic i64 1, ptr %wa seq_cst, align 8
  br label %dnext
dnext:
  %i2 = add i64 %i, 1
  br label %dloop
out:
  ret void
}

; REGISTER A DESCRIPTOR WITH THE REACTOR (B-3a, 1.1.12a) — one-shot: the
; event fires once and disarms, the woken task re-tries its syscall, and a
; wait that expires leaves the registration armed, which is harmless — a
; late fire dues whatever frame owns the slot then, a spurious wake the
; model already tolerates everywhere, and closing the fd removes it from
; every epoll set by kernel rule. The first registration creates the epoll
; set and the eventfd and arms the executor's idle wait.
define void @npk_io_register(i32 %fd, i32 %ev) {
entry:
  %ex = call ptr @npk_exec()
  %ctp0 = getelementptr %npk.exec, ptr %ex, i32 0, i32 13
  %fr = load ptr, ptr %ctp0
  %epp = getelementptr %npk.exec, ptr %ex, i32 0, i32 11
  %epfd0 = load i32, ptr %epp
  %have = icmp ne i32 %epfd0, 0
  br i1 %have, label %arm, label %create
create:
  ; epoll_create1(EPOLL_CLOEXEC)
  %ep = call i64 @npk_sys6(i64 291, i64 524288, i64 0, i64 0, i64 0, i64 0, i64 0)
  %epbad = icmp slt i64 %ep, 0
  br i1 %epbad, label %trap, label %mkev
mkev:
  ; eventfd2(0, EFD_CLOEXEC|EFD_NONBLOCK)
  %ev2 = call i64 @npk_sys6(i64 290, i64 0, i64 526336, i64 0, i64 0, i64 0, i64 0)
  %evbad = icmp slt i64 %ev2, 0
  br i1 %evbad, label %trap, label %addev
addev:
  %ep32 = trunc i64 %ep to i32
  store i32 %ep32, ptr %epp
  %evp = getelementptr %npk.exec, ptr %ex, i32 0, i32 12
  %ev32 = trunc i64 %ev2 to i32
  ; RELEASE: a rouser that sees evfd nonzero must also see the descriptor
  ; it names fully created; rousers load it acquire.
  store atomic i32 %ev32, ptr %evp release, align 4
  ; the eventfd rides the set with data 0 — the drain marker
  %eev = alloca [12 x i8], align 4
  store i32 1, ptr %eev, align 4
  %edp = getelementptr i8, ptr %eev, i64 4
  store i64 0, ptr %edp, align 4
  %eevp = ptrtoint ptr %eev to i64
  ; epoll_ctl(epfd, ADD, evfd, &ev)
  %ar = call i64 @npk_sys6(i64 233, i64 %ep, i64 1, i64 %ev2, i64 %eevp, i64 0, i64 0)
  %arbad = icmp slt i64 %ar, 0
  br i1 %arbad, label %trap, label %arm
trap:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
arm:
  %epfd1 = load i32, ptr %epp
  %epl = sext i32 %epfd1 to i64
  %fdl = sext i32 %fd to i64
  ; EPOLLONESHOT on top of the caller's interest
  %evones = or i32 %ev, 1073741824
  %tev = alloca [12 x i8], align 4
  store i32 %evones, ptr %tev, align 4
  %tdp = getelementptr i8, ptr %tev, i64 4
  %fri = ptrtoint ptr %fr to i64
  store i64 %fri, ptr %tdp, align 4
  %tevp = ptrtoint ptr %tev to i64
  ; try ADD; -EEXIST means it was seen before -- MOD rearms the one-shot
  %r1 = call i64 @npk_sys6(i64 233, i64 %epl, i64 1, i64 %fdl, i64 %tevp, i64 0, i64 0)
  %exists = icmp eq i64 %r1, -17
  br i1 %exists, label %mod, label %ck1
mod:
  %r2 = call i64 @npk_sys6(i64 233, i64 %epl, i64 3, i64 %fdl, i64 %tevp, i64 0, i64 0)
  %bad2 = icmp slt i64 %r2, 0
  br i1 %bad2, label %duenow, label %out
ck1:
  %bad1 = icmp slt i64 %r1, 0
  br i1 %bad1, label %duenow, label %out
duenow:
  ; THE KERNEL DECLINED TO WATCH THIS DESCRIPTOR -- EPERM for one epoll
  ; cannot poll (a regular file, always ready by definition), EBADF for one
  ; that does not exist. Neither is the reactor's error to report: the task
  ; is due NOW, resumes before its deadline, and the caller's re-tried
  ; syscall answers with the same errno as a proper `Result` -- the caller
  ; handles its own mistake where it can (D-179's posture), and a file
  ; behind `io_ready` reads instead of trapping. Only creating the reactor
  ; itself still traps above: no descriptor, no caller, no `Result` to ride.
  %dnp = getelementptr %npk.hdr, ptr %fr, i32 0, i32 8
  store atomic i64 1, ptr %dnp seq_cst, align 8
  br label %out
out:
  ret void
}

; CLOSE AN OWNED DESCRIPTOR AT ITS DROP (D-185, 1.1.12b). The verdict is
; deliberately not observable here -- a drop has no Result to carry it, and
; on Linux the descriptor is gone whatever close returns. A caller that must
; see the verdict releases first: `close(release_fd(move o))`. The epoll
; interest set drops the descriptor by kernel rule when its last copy
; closes, so no unwatch is needed.
define void @npk_ofd_close(i32 %fd) {
entry:
  %fdl = sext i32 %fd to i64
  %r = call i64 @npk_sys6(i64 3, i64 %fdl, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

; DROP A DESCRIPTOR FROM THE REACTOR (B-3a, 1.1.12a). `io_ready` defers
; this so a registration lives exactly as long as its wait -- the one-shot's
; payload is the waiting FRAME, and a fire after that frame is freed would
; write into freed memory. ENOENT (never watched, or closed -- the kernel
; removed it) is the no-op answer, not an error; with no reactor armed there
; is nothing to remove.
define void @npk_io_unwatch(i32 %fd) {
entry:
  %ex = call ptr @npk_exec()
  %epp = getelementptr %npk.exec, ptr %ex, i32 0, i32 11
  %epfd = load i32, ptr %epp
  %none = icmp eq i32 %epfd, 0
  br i1 %none, label %out, label %del
del:
  %epl = sext i32 %epfd to i64
  %fdl = sext i32 %fd to i64
  ; epoll_ctl(epfd, EPOLL_CTL_DEL = 2, fd, NULL)
  %r = call i64 @npk_sys6(i64 233, i64 %epl, i64 2, i64 %fdl, i64 0, i64 0, i64 0)
  br label %out
out:
  ret void
}

; --- the monotonic clock (D-176, 1.1.3) ------------------------------------
; CLOCK_MONOTONIC nanoseconds since an arbitrary epoch -- the deadline
; substrate's one clock. Wall clocks are excluded from the deadline path:
; NTP steps them, and a deadline that moves with the wall silently voids
; D-056's containment. clock_gettime(1, valid-ptr) cannot fail on Linux;
; the impossible branch still traps (D-061's posture: "cannot fail" is a
; claim, and claims are checked).
; A fresh failure: the chain restarts at its origin site.
define void @npk_chain_reset(i32 %site) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_chain = getelementptr %npk.exec, ptr %ex, i32 0, i32 8
  %p_npk_chain_n = getelementptr %npk.exec, ptr %ex, i32 0, i32 9
  store i32 1, ptr %p_npk_chain_n
  store i32 %site, ptr %p_npk_chain
  ret void
}

; One propagation hop. The first eight sites stay; the depth keeps counting.
define void @npk_chain_push(i32 %site) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_chain = getelementptr %npk.exec, ptr %ex, i32 0, i32 8
  %p_npk_chain_n = getelementptr %npk.exec, ptr %ex, i32 0, i32 9
  %d = load i32, ptr %p_npk_chain_n
  %in = icmp ult i32 %d, 8
  br i1 %in, label %keep, label %count
keep:
  %slot = getelementptr [8 x i32], ptr %p_npk_chain, i32 0, i32 %d
  store i32 %site, ptr %slot
  br label %count
count:
  %d2 = add i32 %d, 1
  store i32 %d2, ptr %p_npk_chain_n
  ret void
}

define i32 @npk_chain_depth() {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_chain = getelementptr %npk.exec, ptr %ex, i32 0, i32 8
  %p_npk_chain_n = getelementptr %npk.exec, ptr %ex, i32 0, i32 9
  %d = load i32, ptr %p_npk_chain_n
  ret i32 %d
}

; The i-th kept site, oldest first; 0 outside the kept range.
define i32 @npk_chain_site(i32 %i) {
entry:
  %ex = call ptr @npk_exec()
  %p_npk_chain = getelementptr %npk.exec, ptr %ex, i32 0, i32 8
  %p_npk_chain_n = getelementptr %npk.exec, ptr %ex, i32 0, i32 9
  %neg = icmp slt i32 %i, 0
  br i1 %neg, label %oob, label %lo
lo:
  %d = load i32, ptr %p_npk_chain_n
  %cap = icmp ult i32 %d, 8
  %kept = select i1 %cap, i32 %d, i32 8
  %in = icmp ult i32 %i, %kept
  br i1 %in, label %ok, label %oob
ok:
  %slot = getelementptr [8 x i32], ptr %p_npk_chain, i32 0, i32 %i
  %v = load i32, ptr %slot
  ret i32 %v
oob:
  ret i32 0
}

define i64 @npk_mono_now() {
entry:
  %ts = alloca [2 x i64], align 16
  %p = ptrtoint ptr %ts to i64
  %r = call i64 @npk_sys6(i64 228, i64 1, i64 %p, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp ne i64 %r, 0
  br i1 %bad, label %impossible, label %ok
impossible:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
ok:
  %sp = getelementptr [2 x i64], ptr %ts, i64 0, i64 0
  %s = load i64, ptr %sp
  %np = getelementptr [2 x i64], ptr %ts, i64 0, i64 1
  %n = load i64, ptr %np
  %m = mul i64 %s, 1000000000
  %t = add i64 %m, %n
  ret i64 %t
}

; ---------------------------------------------------------------------------
; The trap route (D-142, cycle 0.9.0): how a RUNTIME FAULT becomes a controlled
; shutdown. Emitted guards (division by zero, INT_MIN/-1 — and every guard a
; later rung adds) call @npk_trap with a code from the D-141 space's runtime
; region below E_EOF:
;
;   -4097  DIV_BY_ZERO       integer / or % with a zero divisor (D-007)
;   -4098  INT_MIN_OVERFLOW  INT_MIN / -1 or INT_MIN % -1 (no defined value;
;                            D-008 refused inventing one, so it traps)
;   -4110  INT_OVERFLOW      a plain-integer + - * overflowed its width (D-210,
;                            1.4.2b). The DEFAULT integer is the checked one:
;                            wrapping was the Therac-255->0 shape sitting under
;                            the type nobody has to opt into. `tbb` remains the
;                            saturate-to-ERR family; bit operations are
;                            unchanged, being bit operations
;   -4099  OUT_OF_BOUNDS     a slice/array index past the end, or a range view
;                            that does not fit its source (D-070; 0.9.2)
;   -4100  TBB_ERR           an ERR value reached a bare comparison or a
;                            checked cast out of tbb (D-008 SS5, D-144; 0.9.5)
;   -4101  BAD_STEP          a counted loop's step, not a literal, evaluated
;                            to zero or negative at run time (D-022; 0.9.7)
;   -4102  HEAP_INTEGRITY    double-free, foreign/misaligned/null pointer to
;                            dalloc/ralloc, corrupted header or torn guard,
;                            or a UAF caught by a freed slot's magic (0.10.0)
;   -4103  HEAP_OOM          mmap failed; the trap path allocates nothing,
;                            which is the allocator's own C-3 obligation
;   -4104  HEAP_BAD_REQUEST  negative size, calloc count*size overflow,
;                            ralloc(p, 0), or a non-power-of-two alignment
;   -4106  STALE_HANDLE      NOT A TRAP -- the code an arena get/put/free
;                            RETURNS in Result.error when the handle's
;                            generation no longer matches (D-152); staleness
;                            is a condition the program handles, not a
;                            defect the runtime ends
;   -4107  DEADLINE_EXCEEDED time ran out (D-176): as a Result error it is the
;                            catchable timeout every deadline API returns; as a
;                            trap code it is a JOIN giving up on a task that
;                            outlived its mandatory deadline (D-062/D-083)
;   -4105  HEAP_LEAK         exit reached with live `wild` memory -- the
;                            K-semantics rule (CONTROL_REFERENCE 4.6) made
;                            real at 0.10.1; failsafe may clean up with
;                            wild_release_all() and exit positive
;   -4109  DRIVER_LEAK       exit 0 reached with a driver still registered
;                            (D-188, 1.1.13a) -- the D-151 rule extended to
;                            the driver registry: a clean exit never abandons
;                            a supervised process. kill_all has already run
;                            on this trap's own path, so the driver is dead
;                            before failsafe reports it
;
; The route is trap -> the program's own `failsafe` -> exit with its return.
; A trap RAISED WHILE FAILSAFE IS RUNNING -- failsafe itself double-freeing,
; say -- exits 70 directly rather than recursing into failsafe forever; the
; same flag is what lets failsafe's own exit skip the leak check (the check
; runs once, at the program's exit, never at failsafe's).
; Every program defines `failsafe` (D-013, mandatory), so @npk_failsafe always
; resolves at link. D-014 requires failsafe to return POSITIVE; until 1.3
; injects and verifies that `ensures`, the runtime refuses to report success
; after a fault: a nonpositive return exits 70, the floor's own
; runtime-violation code.
; ---------------------------------------------------------------------------

declare i32 @npk_failsafe(i32)

@npk_in_failsafe = internal global i32 0

; THE ORIGIN CHAIN (D-179, 1.1.6): the sites an in-flight error has passed,
; oldest first. One per thread when the executor arrives (1.1.8); one per
; process until then — at most one error propagates at a time either way.
; The ring keeps the FIRST eight stamps (the origin end is the diagnostic
; end) and the depth counts every hop. Site 0 is reserved for the runtime
; itself. Allocator-free, failure-path-only: the success path never touches
; these words.

define void @npk_trap(i32 %code) noreturn {
  ; D-063: A TRAP IS A WHOLE-PROGRAM EVENT. From here no coroutine is resumed
  ; on any thread — the run loop checks this before every resume, so a trap
  ; inside a task cannot be followed by a sibling running against unknown
  ; state. Frames freeze exactly as they are; nothing is destroyed.
  store i32 1, ptr @npk_frozen
  ; CHAIN-NEUTRAL (D-179): `?!` pushes its site and hands over an error whose
  ; chain must survive; guards and the runtime's own callers reset first.
  %in = load i32, ptr @npk_in_failsafe
  %re = icmp ne i32 %in, 0
  br i1 %re, label %hard, label %run
hard:
  ; re-entry: failsafe trapped. There is no second handler to hand the
  ; situation to, so this is the one uncatchable stop: exit 70 directly.
  ; EXIT_GROUP (231), NOT EXIT (60) — see `npk_exit`.
  %x = call i64 @npk_sys6(i64 231, i64 70, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
run:
  store i32 1, ptr @npk_in_failsafe
  ; DRIVERS DIE BEFORE FAILSAFE RUNS (1.1.13a; D-149 over D-055): the
  ; registry walk is the runtime's own act, not the program's — safing is
  ; mechanism, not policy (D-013), and an uncontrolled driver DURING
  ; failsafe is the hazard class the architecture exists for. SIGKILL via
  ; pidfd only; reaping and cleanup are nobody's business on this path.
  call void @npk_driver_kill_all()
  %r = call i32 @npk_failsafe(i32 %code)
  %bad = icmp sle i32 %r, 0
  %code2 = select i1 %bad, i32 70, i32 %r
  call void @npk_exit(i32 %code2)
  unreachable
}

; NPK_HEAP_STATS, the report (1.5.1b step 0). Allocation-free and
; heap-free by construction -- a stack buffer, four globals and one
; `write(2)` -- because it runs at exit, after a trap's failsafe as well as
; after a clean `exit`, and after the program may have released its heap
; wholesale: a heap the trap was ABOUT must not be asked for memory on the
; way out (a second trap in failsafe is the uncatchable exit 70), and a
; released heap must not be read (the first version of this walked the
; environment slice here and faulted in the compiler's own build). The
; counters are read without the heap mutex: a thread that trapped inside
; the allocator still holds it, and this line is a diagnostic, never a
; verdict.
;
; The line: `heap: allocated=<n> peak_live=<n> count=<n>`, decimal, one
; newline, on fd 2. Whether it prints was decided at `_start` (`npk_hs_arm`).

@npk_hs_t0 = internal constant [16 x i8] c"heap: allocated="
@npk_hs_t1 = internal constant [11 x i8] c" peak_live="
@npk_hs_t2 = internal constant [7 x i8] c" count="

; Append %n bytes of %s to the line at %pos; the new position.
define internal i64 @npk_hs_put_str(ptr %line, i64 %pos, ptr %s, i64 %n) {
entry:
  br label %loop
loop:
  %k = phi i64 [ 0, %entry ], [ %k1, %body ]
  %more = icmp ult i64 %k, %n
  br i1 %more, label %body, label %done
body:
  %sp = getelementptr i8, ptr %s, i64 %k
  %c = load i8, ptr %sp
  %p = add i64 %pos, %k
  %tp = getelementptr i8, ptr %line, i64 %p
  store i8 %c, ptr %tp
  %k1 = add i64 %k, 1
  br label %loop
done:
  %end = add i64 %pos, %n
  ret i64 %end
}

; Append %v in decimal to the line at %pos; the new position. The digits are
; formed backwards in a 24-byte scratch (enough for any i64 read unsigned)
; and copied forward.
define internal i64 @npk_hs_put_dec(ptr %line, i64 %pos, i64 %v) {
entry:
  %tmp = alloca [24 x i8]
  br label %loop
loop:
  %i = phi i64 [ 24, %entry ], [ %i1, %loop ]
  %u = phi i64 [ %v, %entry ], [ %q, %loop ]
  %i1 = sub i64 %i, 1
  %q = udiv i64 %u, 10
  %m = urem i64 %u, 10
  %d = trunc i64 %m to i8
  %ch = add i8 %d, 48
  %dp = getelementptr [24 x i8], ptr %tmp, i64 0, i64 %i1
  store i8 %ch, ptr %dp
  %more = icmp ne i64 %q, 0
  br i1 %more, label %loop, label %copy
copy:
  %j = phi i64 [ %i1, %loop ], [ %j1, %copy ]
  %p = phi i64 [ %pos, %loop ], [ %p1, %copy ]
  %sp = getelementptr [24 x i8], ptr %tmp, i64 0, i64 %j
  %c = load i8, ptr %sp
  %tp = getelementptr i8, ptr %line, i64 %p
  store i8 %c, ptr %tp
  %j1 = add i64 %j, 1
  %p1 = add i64 %p, 1
  %left = icmp ult i64 %j1, 24
  br i1 %left, label %copy, label %done
done:
  ret i64 %p1
}

define internal void @npk_hs_report() {
entry:
  %line = alloca [128 x i8]
  %on = load i64, ptr @npk_hs_on
  %off = icmp eq i64 %on, 0
  br i1 %off, label %done, label %print
print:
  %p0 = call i64 @npk_hs_put_str(ptr %line, i64 0, ptr @npk_hs_t0, i64 16)
  %a = load i64, ptr @npk_hs_allocated
  %p1 = call i64 @npk_hs_put_dec(ptr %line, i64 %p0, i64 %a)
  %p2 = call i64 @npk_hs_put_str(ptr %line, i64 %p1, ptr @npk_hs_t1, i64 11)
  %pk = load i64, ptr @npk_hs_peak
  %p3 = call i64 @npk_hs_put_dec(ptr %line, i64 %p2, i64 %pk)
  %p4 = call i64 @npk_hs_put_str(ptr %line, i64 %p3, ptr @npk_hs_t2, i64 7)
  %cn = load i64, ptr @npk_hs_count
  %p5 = call i64 @npk_hs_put_dec(ptr %line, i64 %p4, i64 %cn)
  %nlp = getelementptr i8, ptr %line, i64 %p5
  store i8 10, ptr %nlp
  %len = add i64 %p5, 1
  %la = ptrtoint ptr %line to i64
  %w = call i64 @npk_sys6(i64 1, i64 2, i64 %la, i64 %len, i64 0, i64 0, i64 0)
  br label %done
done:
  ret void
}

define void @npk_exit(i32 %code) noreturn {
  ; THE K-SEMANTICS EXIT RULE (0.10.1, D-151): a SUCCESSFUL exit -- code 0,
  ; exactly as CONTROL_REFERENCE 4.6 scopes it -- requires the <wild-live>
  ; set empty. Non-empty routes to failsafe (-4105), which may clean up
  ; (wild_release_all) and exit -- and THAT exit passes, because the
  ; in-failsafe flag is set. A FAILURE exit is already a report of
  ; abnormality and keeps its code: hijacking it with a leak trap would
  ; destroy the error it was raising, and error paths carry no cleanup
  ; obligation (the same reasoning as defer-does-not-run-on-trap, D-014).
  ; The count walks preallocated state only.
  %in = load i32, ptr @npk_in_failsafe
  %skip = icmp ne i32 %in, 0
  %fail = icmp ne i32 %code, 0
  %pass = or i1 %skip, %fail
  br i1 %pass, label %leave, label %check
check:
  ; THE DRIVER REGISTRY FIRST (D-188, 1.1.13a): a clean exit with a live
  ; driver is a program that never decided its driver's fate — graver than
  ; leaked memory, because the abandoned thing is a supervised PROCESS. The
  ; trap route it takes runs kill_all before failsafe, so the driver is
  ; dead before the report is made.
  %dlive = call i64 @npk_driver_live_count()
  %dleak = icmp ne i64 %dlive, 0
  br i1 %dleak, label %dtrap, label %wcheck
dtrap:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4109)
  unreachable
wcheck:
  %live = call i64 @npk_wild_live_count()
  %leaks = icmp ne i64 %live, 0
  br i1 %leaks, label %trap, label %leave
trap:
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4105)
  unreachable
leave:
  ; EXIT_GROUP (231), NOT EXIT (60). `exit` ends the CALLING THREAD; the
  ; process continues with whatever else is running, and its final status
  ; becomes whichever thread happens to finish last. With one thread the two
  ; are indistinguishable, which is why this stood from the first runtime
  ; until threads landed (1.1.9) and nothing noticed: a two-thread program
  ; then exited 100, or 0, or 70, depending on scheduling — 27 runs in 200.
  ;
  ; It is not a flaky exit code. `exit` from `main` or `failsafe` is the
  ; CONTROLLED SHUTDOWN (D-013/D-014): the whole point is that the program
  ; stops. A `main` that returns while worker threads keep running has not
  ; shut down at all — it has abandoned them, and with actuators live that is
  ; the uncontrolled stop the safety case exists to prevent. The status the
  ; parent reads being wrong is the mildest of its consequences.
  ; NPK_HEAP_STATS reports HERE, exactly once per process: this block is the
  ; one every exit leaves by, and a leak-trap's failsafe exit re-enters above
  ; and reaches it once (1.5.1b step 0).
  call void @npk_hs_report()
  %c = sext i32 %code to i64
  %r = call i64 @npk_sys6(i64 231, i64 %c, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

; ---------------------------------------------------------------------------
; fmod / fmodf (0.9.4, D-143): `frem` on flt64/flt32 lowers to these libcalls,
; which libm normally provides -- the floor provides them as hand-written IR.
;
; THE RESULT IS EXACT, which is what makes a short implementation correct:
; align |b| up to |a| by exact power-of-two doublings, subtract (Sterbenz: the
; subtrahend is within [|a|/2, |a|], so the subtraction is exact), repeat. No
; rounding ever occurs, so this IS IEEE fmod, not an approximation of it.
; Specials per IEEE: NaN in, |a| infinite, or b zero -> NaN; |b| infinite ->
; a unchanged; the result carries a's sign (llvm.copysign; the fabs/copysign
; intrinsics lower inline -- no symbol survives to the object).
; ---------------------------------------------------------------------------

declare double @llvm.fabs.f64(double)
declare float @llvm.fabs.f32(float)
declare double @llvm.copysign.f64(double, double)
declare float @llvm.copysign.f32(float, float)

define double @fmod(double %a, double %b) {
entry:
  %ab0 = call double @llvm.fabs.f64(double %a)
  %bb = call double @llvm.fabs.f64(double %b)
  %anan = fcmp uno double %a, %a
  %bnan = fcmp uno double %b, %b
  %bzero = fcmp oeq double %bb, 0.0
  %ainf = fcmp oeq double %ab0, 0x7FF0000000000000
  %n1 = or i1 %anan, %bnan
  %n2 = or i1 %n1, %bzero
  %bad = or i1 %n2, %ainf
  br i1 %bad, label %retnan, label %chk
retnan:
  ret double 0x7FF8000000000000
chk:
  %binf = fcmp oeq double %bb, 0x7FF0000000000000
  br i1 %binf, label %reta, label %outer
reta:
  ret double %a
outer:
  %ab = phi double [ %ab0, %chk ], [ %abn, %step ]
  %small = fcmp olt double %ab, %bb
  br i1 %small, label %fin, label %scale
scale:
  %c = phi double [ %bb, %outer ], [ %c2, %grow ]
  %c2 = fmul double %c, 2.0
  %fits = fcmp ole double %c2, %ab
  br i1 %fits, label %grow, label %step
grow:
  br label %scale
step:
  %abn = fsub double %ab, %c
  br label %outer
fin:
  %r = call double @llvm.copysign.f64(double %ab, double %a)
  ret double %r
}

define float @fmodf(float %a, float %b) {
entry:
  %ab0 = call float @llvm.fabs.f32(float %a)
  %bb = call float @llvm.fabs.f32(float %b)
  %anan = fcmp uno float %a, %a
  %bnan = fcmp uno float %b, %b
  %bzero = fcmp oeq float %bb, 0.0
  %ainf = fcmp oeq float %ab0, 0x7FF0000000000000
  %n1 = or i1 %anan, %bnan
  %n2 = or i1 %n1, %bzero
  %bad = or i1 %n2, %ainf
  br i1 %bad, label %retnan, label %chk
retnan:
  ret float 0x7FF8000000000000
chk:
  %binf = fcmp oeq float %bb, 0x7FF0000000000000
  br i1 %binf, label %reta, label %outer
reta:
  ret float %a
outer:
  %ab = phi float [ %ab0, %chk ], [ %abn, %step ]
  %small = fcmp olt float %ab, %bb
  br i1 %small, label %fin, label %scale
scale:
  %c = phi float [ %bb, %outer ], [ %c2, %grow ]
  %c2 = fmul float %c, 2.0
  %fits = fcmp ole float %c2, %ab
  br i1 %fits, label %grow, label %step
grow:
  br label %scale
step:
  %abn = fsub float %ab, %c
  br label %outer
fin:
  %r = call float @llvm.copysign.f32(float %ab, float %a)
  ret float %r
}

; ---------------------------------------------------------------------------
; The i128 division family (0.9.3, D-011 SS2): the ONE libcall class llc emits
; for wide integers -- sdiv/udiv/srem/urem at exactly 128 bits call these four
; symbols (measured on this toolchain; division at 256 bits and beyond expands
; inline, all the way to an executed udiv i4096 probe). compiler-rt provides
; them in C; the floor provides them as hand-written IR under the same TCB
; discipline as everything else here. Shift-subtract long division, 128
; iterations, dependency-free.
;
; A zero divisor never reaches these from emitted code -- the D-142 guard traps
; first -- but the symbols stay TOTAL: b == 0 yields an all-ones quotient and
; r == a from the loop's own arithmetic, a defined result, never UB. INT128_MIN
; negates to itself under two's complement, and its unsigned reading 2^127 is
; exactly what the unsigned core needs, so the sign wrappers are total too.
; ---------------------------------------------------------------------------

define internal { i128, i128 } @npk_udivmod128(i128 %a, i128 %b) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 127, %entry ], [ %inext, %next ]
  %q = phi i128 [ 0, %entry ], [ %q2, %next ]
  %r = phi i128 [ 0, %entry ], [ %r2, %next ]
  %r1 = shl i128 %r, 1
  %iw = zext i32 %i to i128
  %ab = lshr i128 %a, %iw
  %ab1 = and i128 %ab, 1
  %rb = or i128 %r1, %ab1
  %ge = icmp uge i128 %rb, %b
  %rs = sub i128 %rb, %b
  %r2 = select i1 %ge, i128 %rs, i128 %rb
  %qb = shl i128 1, %iw
  %qs = or i128 %q, %qb
  %q2 = select i1 %ge, i128 %qs, i128 %q
  %done = icmp eq i32 %i, 0
  %inext = sub i32 %i, 1
  br i1 %done, label %out, label %next
next:
  br label %loop
out:
  %p0 = insertvalue { i128, i128 } zeroinitializer, i128 %q2, 0
  %p1 = insertvalue { i128, i128 } %p0, i128 %r2, 1
  ret { i128, i128 } %p1
}

define i128 @__udivti3(i128 %a, i128 %b) {
  %qr = call { i128, i128 } @npk_udivmod128(i128 %a, i128 %b)
  %q = extractvalue { i128, i128 } %qr, 0
  ret i128 %q
}

define i128 @__umodti3(i128 %a, i128 %b) {
  %qr = call { i128, i128 } @npk_udivmod128(i128 %a, i128 %b)
  %r = extractvalue { i128, i128 } %qr, 1
  ret i128 %r
}

define i128 @__divti3(i128 %a, i128 %b) {
  %an = icmp slt i128 %a, 0
  %na = sub i128 0, %a
  %aa = select i1 %an, i128 %na, i128 %a
  %bn = icmp slt i128 %b, 0
  %nb = sub i128 0, %b
  %ba = select i1 %bn, i128 %nb, i128 %b
  %qr = call { i128, i128 } @npk_udivmod128(i128 %aa, i128 %ba)
  %q = extractvalue { i128, i128 } %qr, 0
  %sx = xor i1 %an, %bn
  %nq = sub i128 0, %q
  %qq = select i1 %sx, i128 %nq, i128 %q
  ret i128 %qq
}

; C truncated-division semantics, which is what srem lowers against: the
; remainder carries the DIVIDEND's sign.
define i128 @__modti3(i128 %a, i128 %b) {
  %an = icmp slt i128 %a, 0
  %na = sub i128 0, %a
  %aa = select i1 %an, i128 %na, i128 %a
  %bn = icmp slt i128 %b, 0
  %nb = sub i128 0, %b
  %ba = select i1 %bn, i128 %nb, i128 %b
  %qr = call { i128, i128 } @npk_udivmod128(i128 %aa, i128 %ba)
  %r = extractvalue { i128, i128 } %qr, 1
  %nr = sub i128 0, %r
  %rr = select i1 %an, i128 %nr, i128 %r
  ret i128 %rr
}

; ---------------------------------------------------------------------------
; The fd quartet (D-141): open / close / read / write, each ONE syscall,
; faithfully -- the floor is the syscall surface (D-051), so a short write is
; returned, not retried. Discipline lives above it in ordinary Nitpick
; (lib/nio.npk): write-all loops, line endings (D-050), buffering (D-076).
;
; An errored Result carries a zeroed value slot. For `open` that zero is i32 0,
; which reads as stdin -- inert, because reading a tainted `Result.value` is
; refused at compile time; the zero is defence in depth, not the contract.
; ---------------------------------------------------------------------------

define { i32, i32 } @npk_open({ ptr, i64 } %path, i64 %flags, i64 %mode) {
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD = -100, path, flags, mode)
  %r = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 %flags, i64 %mode, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  %f = trunc i64 %r to i32
  %o0 = insertvalue { i32, i32 } zeroinitializer, i32 %f, 0
  %o1 = insertvalue { i32, i32 } %o0, i32 0, 1
  ret { i32, i32 } %o1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i32, i32 } zeroinitializer, i32 0, 0
  %e1 = insertvalue { i32, i32 } %e0, i32 %c, 1
  ret { i32, i32 } %e1
}

define { i32 } @npk_close(i32 %fd) {
  %f = sext i32 %fd to i64
  %r = call i64 @npk_sys6(i64 3, i64 %f, i64 0, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  ret { i32 } zeroinitializer
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i32 } zeroinitializer, i32 %c, 0
  ret { i32 } %e0
}

define { i64, i32 } @npk_read(i32 %fd, ptr %buf, i64 %cap) {
entry:
  ; Zero asked is zero delivered, NOT end-of-input: E_EOF must mean the stream
  ; ended, never that the caller handed over an empty buffer.
  %none = icmp eq i64 %cap, 0
  br i1 %none, label %zero, label %go
zero:
  ret { i64, i32 } zeroinitializer
go:
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 0, i64 %f, i64 %p, i64 %cap, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %sift
sift:
  %eof = icmp eq i64 %r, 0
  br i1 %eof, label %ateof, label %ok
ateof:
  ; E_EOF = -4096 (D-141): end-of-input is an error code, never a sentinel in
  ; the value channel (D-075) -- at the floor exactly as it will be in the
  ; Stream trait above it.
  %z0 = insertvalue { i64, i32 } zeroinitializer, i64 0, 0
  %z1 = insertvalue { i64, i32 } %z0, i32 -4096, 1
  ret { i64, i32 } %z1
ok:
  %k0 = insertvalue { i64, i32 } zeroinitializer, i64 %r, 0
  %k1 = insertvalue { i64, i32 } %k0, i32 0, 1
  ret { i64, i32 } %k1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i64, i32 } zeroinitializer, i64 0, 0
  %e1 = insertvalue { i64, i32 } %e0, i32 %c, 1
  ret { i64, i32 } %e1
}

define { i64, i32 } @npk_write(i32 %fd, ptr %buf, i64 %len) {
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 1, i64 %f, i64 %p, i64 %len, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  %k0 = insertvalue { i64, i32 } zeroinitializer, i64 %r, 0
  %k1 = insertvalue { i64, i32 } %k0, i32 0, 1
  ret { i64, i32 } %k1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i64, i32 } zeroinitializer, i64 0, 0
  %e1 = insertvalue { i64, i32 } %e0, i32 %c, 1
  ret { i64, i32 } %e1
}

; ---------------------------------------------------------------------------
; string -> cstring, and reading a file by path.
;
; `to_cstring` FAILS ON AN INTERIOR NUL, and that is the entire point of D-049.
; A Nitpick `string` is {ptr, len, cap} and length-carrying, so an embedded 0u8
; is just a byte. Converting such a string by copying up to the first NUL
; silently truncates it, and the caller has no indication -- so a path that was
; validated and a path that was opened become DIFFERENT PATHS.
;
;     attacker supplies:   "avatar.png\0.sh"
;     validator sees:      len 14, suffix ".sh"
;     kernel sees:         "avatar.png"
;
; That is the poison-NUL class, and it is why `read_file` takes a cstring rather
; than accepting a string and terminating it itself. The conversion is explicit,
; fallible, and the caller handles the failure like any other.
;
; Error codes (D-141): the error slot carries NEGATIVE system codes -- the
; kernel's own return, exactly as the syscall delivered it, so a caller can
; tell ENOENT (-2) from EACCES (-13) without a second mechanism. Conditions
; the floor detects itself reuse the kernel's vocabulary (-22 EINVAL for the
; interior NUL here, -34 ERANGE for a slice out of range); the one condition
; errno has no word for, end-of-input, is E_EOF = -4096 -- the first code past
; the kernel's own error space (errno stops at 4095), so it can never collide.
; Positive codes are the program's (`fail`), and 0 is ok.
; ---------------------------------------------------------------------------

define { { ptr, i64 }, i32 } @npk_to_cstring({ ptr, i64, i64 } %s) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %n = extractvalue { ptr, i64, i64 } %s, 1
  br label %scan

scan:                                     ; preds = %entry, %step
  %i = phi i64 [ 0, %entry ], [ %i1, %step ]
  %done = icmp uge i64 %i, %n
  br i1 %done, label %copy, label %check

check:                                    ; preds = %scan
  %cp = getelementptr i8, ptr %p, i64 %i
  %ch = load i8, ptr %cp
  %isnul = icmp eq i8 %ch, 0
  br i1 %isnul, label %interior, label %step

step:                                     ; preds = %check
  %i1 = add i64 %i, 1
  br label %scan

copy:                                     ; preds = %scan
  ; One byte more than the length, for the terminator the kernel needs.
  %sz = add i64 %n, 1
  %buf = call ptr @npk_alloc_internal(i64 %sz)
  call ptr @memcpy(ptr %buf, ptr %p, i64 %n)
  %end = getelementptr i8, ptr %buf, i64 %n
  store i8 0, ptr %end
  %c0 = insertvalue { ptr, i64 } zeroinitializer, ptr %buf, 0
  %c1 = insertvalue { ptr, i64 } %c0, i64 %n, 1
  %r0 = insertvalue { { ptr, i64 }, i32 } zeroinitializer, { ptr, i64 } %c1, 0
  %r1 = insertvalue { { ptr, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64 }, i32 } %r1

interior:                                 ; preds = %check
  %e0 = insertvalue { ptr, i64 } zeroinitializer, ptr null, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 0, 1
  %q0 = insertvalue { { ptr, i64 }, i32 } zeroinitializer, { ptr, i64 } %e1, 0
  %q1 = insertvalue { { ptr, i64 }, i32 } %q0, i32 -22, 1
  ret { { ptr, i64 }, i32 } %q1
}

; Can this path be opened for reading?
;
; Ambiguity-is-an-error (BUILD_REFERENCE section 3) means the resolver must probe
; EVERY dependency root before deciding, not stop at the first hit -- a resolver
; that returns early cannot report the second candidate. Probing by full read
; would be O(file size) per root, and most probes miss.
;
; It tests READABILITY rather than existence, which is the question actually
; being asked: a file that exists and cannot be opened is not a candidate.
define i8 @npk_path_exists({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %no, label %yes

yes:
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret i8 1

no:
  ret i8 0
}

; Write a whole buffer to a path, replacing what was there -- read_file's
; mirror, and the routine `npkc -o` stands on (0.8.3). openat/write/close live
; INSIDE it as one auditable body rather than as three exposed symbols; the
; fd-granular primitives arrive with D-050's line discipline, where a held-open
; descriptor is the point.
;
; A SHORT WRITE IS NOT FAILURE. write(2) may take fewer bytes than offered --
; signals, pipes, quotas -- so the loop advances by what was ACCEPTED and only a
; negative return is an error. Treating a short write as success is the classic
; way to truncate a file and report victory; the error code travels out as the
; Result's tbb32.
; Byte equality over two strings -- the primitive the comptime folder mirrors
; (fold_string_builtin) and the one string predicate worth a runtime symbol:
; everything nlibc builds (contains, starts_with, index_of) is ordinary Nitpick
; over byte access, but equality is called from generated code paths where a
; call beats an inlined loop for auditability.
define i8 @npk_string_equals({ ptr, i64, i64 } %a, { ptr, i64, i64 } %b) {
entry:
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  %same = icmp eq i64 %al, %bl
  br i1 %same, label %scan, label %no

scan:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  br label %loop

loop:
  %i = phi i64 [ 0, %scan ], [ %i2, %next ]
  %done = icmp eq i64 %i, %al
  br i1 %done, label %yes, label %cmp

cmp:
  %pa = getelementptr i8, ptr %ap, i64 %i
  %pb = getelementptr i8, ptr %bp, i64 %i
  %ca = load i8, ptr %pa
  %cb = load i8, ptr %pb
  %eq = icmp eq i8 %ca, %cb
  br i1 %eq, label %next, label %no

next:
  %i2 = add i64 %i, 1
  br label %loop

yes:
  ret i8 1

no:
  ret i8 0
}

define { i32 } @npk_write_file({ ptr, i64 } %path, { ptr, i64, i64 } %data) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_WRONLY|O_CREAT|O_TRUNC, 0644). 577 = 1|64|512.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 577, i64 420, i64 0, i64 0)
  %obad = icmp slt i64 %fd, 0
  br i1 %obad, label %openfail, label %wstart

wstart:
  %base = extractvalue { ptr, i64, i64 } %data, 0
  %total = extractvalue { ptr, i64, i64 } %data, 1
  br label %wloop

wloop:
  %off = phi i64 [ 0, %wstart ], [ %off2, %wnext ]
  %left = sub i64 %total, %off
  %done = icmp eq i64 %left, 0
  br i1 %done, label %wclose, label %wone

wone:
  %at = getelementptr i8, ptr %base, i64 %off
  %ati = ptrtoint ptr %at to i64
  %n = call i64 @npk_sys6(i64 1, i64 %fd, i64 %ati, i64 %left, i64 0, i64 0, i64 0)
  %wbad = icmp slt i64 %n, 0
  br i1 %wbad, label %writefail, label %wnext

wnext:
  %off2 = add i64 %off, %n
  br label %wloop

wclose:
  %c = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %cbad = icmp slt i64 %c, 0
  br i1 %cbad, label %closefail, label %ok

ok:
  ret { i32 } zeroinitializer

openfail:
  %oec = trunc i64 %fd to i32
  %or0 = insertvalue { i32 } zeroinitializer, i32 %oec, 0
  ret { i32 } %or0

writefail:
  ; close best-effort: the write's errno is the story, not the close's.
  %ce = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %wec = trunc i64 %n to i32
  %wr0 = insertvalue { i32 } zeroinitializer, i32 %wec, 0
  ret { i32 } %wr0

closefail:
  ; A FAILED CLOSE IS A FAILED WRITE. Buffered-at-the-kernel errors surface
  ; here, and reporting success past one is reporting bytes that may not exist.
  %lec = trunc i64 %c to i32
  %lr0 = insertvalue { i32 } zeroinitializer, i32 %lec, 0
  ret { i32 } %lr0
}

define { { ptr, i64, i64 }, i32 } @npk_read_file({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_RDONLY, 0). AT_FDCWD is -100.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %openfail, label %start

start:                                    ; preds = %entry
  %buf0 = call ptr @npk_alloc_internal(i64 65536)
  br label %loop

loop:                                     ; preds = %start, %iter
  %buf = phi ptr [ %buf0, %start ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %start ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %start ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc_internal(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 %fd, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %rbad = icmp slt i64 %n, 0
  br i1 %rbad, label %readfail, label %check

check:                                    ; preds = %read
  ; A SHORT READ IS NOT END OF FILE. Treating it as one is the classic way to
  ; truncate a file silently, and here it would mean parsing a prefix of a module
  ; and reporting success.
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %o0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %s2, 0
  %o1 = insertvalue { { ptr, i64, i64 }, i32 } %o0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %o1

readfail:                                 ; preds = %read
  ; Close before returning. The process is about to report an error, not exit,
  ; and a driver that reports many missing files must not leak a descriptor each
  ; time it succeeds partway.
  %cr2 = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %rerr32 = trunc i64 %n to i32
  br label %fail

openfail:                                 ; preds = %entry
  %oerr32 = trunc i64 %fd to i32
  br label %fail

fail:                                     ; preds = %readfail, %openfail
  %code = phi i32 [ %rerr32, %readfail ], [ %oerr32, %openfail ]
  %f0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr null, 0
  %f1 = insertvalue { ptr, i64, i64 } %f0, i64 0, 1
  %f2 = insertvalue { ptr, i64, i64 } %f1, i64 0, 2
  %g0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %f2, 0
  %g1 = insertvalue { { ptr, i64, i64 }, i32 } %g0, i32 %code, 1
  ret { { ptr, i64, i64 }, i32 } %g1
}

; ---------------------------------------------------------------------------
; Reading standard input, whole.
;
; The frontend needs to run on REAL FILES for the rejection suite to mean what
; D-085 says it means -- every file there must PARSE and be refused later, and
; only the real parser can demonstrate that. Reading fd 0 rather than opening a
; path is deliberate: it needs no argv (which `_start` does not pass, and which
; would raise the separate question of what `main`'s signature is), and it does
; not pre-commit the file-opening API that cycle 0.3's module loader will want.
;
; SUBSET_1 section 1.6 has listed open/read/close in the runtime floor since 0.0.4
; while none of them existed, because nothing had needed one yet. This is the
; first, and the entry there now says which.
;
; Grows by doubling and reads until read(2) returns 0. A short read is NOT
; end-of-file -- treating it as one is the classic way to silently truncate a
; file, and here it would mean parsing a prefix of a test and reporting success.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_read_stdin() {
entry:
  %buf0 = call ptr @npk_alloc_internal(i64 65536)
  br label %loop

loop:                                     ; preds = %entry, %iter
  %buf = phi ptr [ %buf0, %entry ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %entry ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %entry ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  ; Growing copies into a fresh block; since 0.10.0 the abandoned one is
  ; ralloc's to free, not a permanent loss.
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc_internal(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 0, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %failed = icmp slt i64 %n, 0
  br i1 %failed, label %err, label %check

check:                                    ; preds = %read
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1

err:
  ; An errored Result carries a zeroed value, so a caller that unwraps without
  ; checking gets an empty string rather than a pointer into nothing.
  %e0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr null, 0
  %e1 = insertvalue { ptr, i64, i64 } %e0, i64 0, 1
  %e2 = insertvalue { ptr, i64, i64 } %e1, i64 0, 2
  %q0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %e2, 0
  %qc = trunc i64 %n to i32
  %q1 = insertvalue { { ptr, i64, i64 }, i32 } %q0, i32 %qc, 1
  ret { { ptr, i64, i64 }, i32 } %q1
}

; ---------------------------------------------------------------------------
; THE HEAP (0.10.0, D-150). Size-classed slab chunks + a large-block mmap path,
; with a real dalloc. This replaces the seed-era bump floor; the seed still
; BUILDS the compiler (C-13), but everything it builds now links a runtime that
; actually frees.
;
; SAFETY SHAPE, stated once and obeyed everywhere below:
;
;   1. OUT-OF-BAND METADATA. The allocator's control state -- chunk bitmaps,
;      the chunk table, the large table -- never lives inside user-reachable
;      payload. The prototype's free-list-in-payload design was consulted and
;      REJECTED: a use-after-free there corrupts the allocator's own control
;      data, and detection is probabilistic. Here a UAF can only touch payload
;      and canaries; the bitmap says what is free, deterministically.
;   2. VALIDATE BEFORE DEREFERENCE. dalloc/ralloc prove a pointer lies inside
;      memory THIS allocator mapped (sorted chunk table / large table, binary
;      search) before reading a single byte through it. A garbage pointer is a
;      trap, never a wild load -- the allocator itself must not be the thing
;      that segfaults.
;   3. EVERY FAILURE IS A TRAP, NEVER UB. Double-free, foreign/misaligned
;      pointer, corrupted header, torn guard: npk_trap(-4102) -> failsafe.
;      OOM: npk_trap(-4103). Bad request (negative size, calloc overflow,
;      ralloc(p,0), bad alignment): npk_trap(-4104). The trap path allocates
;      nothing (the C-3 obligation): npk_trap -> failsafe runs on preallocated
;      state, because OOM handling runs when allocation just failed.
;
; BLOCK SHAPE. Every block: [ size i64 | magic i64 | payload... ]. The 16-byte
; header keeps payloads 16-aligned (the floor's 0.7.3 discipline, still what
; bounds ralloc's copy). Magic words are secret-keyed and ADDRESS-keyed:
;   live block (runtime-internal / managed-regime storage: string bodies,
;                argv, file buffers -- OUTSIDE the <wild-live> set, their RAII
;                lands with the managed lowering):
;                 secret ^ blockaddr ^ K_LIVE
;   live block (WILD regime -- the alloc/calloc/ralloc/aalloc builtins; what
;                the exit-time leak check counts, D-151):
;                 secret ^ blockaddr ^ K_LIVEW
;   freed slot:   secret ^ blockaddr ^ K_FREED
;   large block:  secret ^ blockaddr ^ K_LARGE
;   chunk header: secret ^ chunkaddr ^ K_CHUNK
;   guards:       secret ^ guardaddr ^ K_GUARD  (two words per guard)
; The secret comes from getrandom(2) at first allocation, so a forged or
; replayed magic does not survive across runs. In a slab, the NEXT block's
; header is the overrun canary for its neighbour; the last slot is closed by a
; 16-byte tail guard, checked on every free in the chunk. Large blocks carry
; their own 16-byte footer guard.
;
; DOUBLE-FREE is deterministic, not probabilistic: the chunk bitmap holds one
; bit per slot (set = free), so the second dalloc of a slot finds its bit
; already set and traps. This is defense in depth -- D-119 already refuses a
; double-free of a TRACKED binding at compile time; this catches pointers the
; static analysis cannot follow (#wild_ptr fabrications, stored wild aliases).
; A chunk WATERMARK (highest slot ever handed out) additionally traps a free
; of a never-allocated slot, and a recycled slot's header must still carry its
; FREED magic when popped -- a UAF that scribbled a freed block's header traps
; on the next allocation from that slot.
;
; SIZE CLASSES (chunk = 64 KiB, 64 KiB-aligned so block->chunk is one AND):
;   class  = payload bytes; stride = class + 16 header
;   layout = [64B chunk header][bitmap W x i64][pad to 16][slots][16B guard]
; The tables below are compiled-in constants; the generator formula is
;   slots = max S with align16(64 + 8*ceil(S/64)) + S*(class+16) + 16 <= 65536
; and each row was computed exactly (see meta/roadmap/0.10/0.10.0.md).
;
; CHUNK HEADER (64 bytes): +0 magic  +8 class-index  +16 slot-count
;   +24 free-count  +32 scan-hint(word)  +40 next  +48 prev  +56 watermark.
; Chunks sit on per-class PARTIAL/FULL doubly-linked lists (O(1) transitions);
; the sorted chunk table answers "is this address one of my chunks" in
; O(log chunks) with no dereference.
;
; SINGLE-THREADED AT THIS RUNG, deliberately: programs have one thread until
; 1.1, and the lock discipline lands with 1.1's executor work (stated in
; MEMORY_REFERENCE; the shared_arena subcycle 0.10.4 is the concurrent piece).
; ---------------------------------------------------------------------------

@npk_hsec = internal global i64 0
@npk_cls_size = internal constant [14 x i64] [i64 16, i64 32, i64 48, i64 64, i64 96, i64 128, i64 192, i64 256, i64 384, i64 512, i64 768, i64 1024, i64 1536, i64 2048]
@npk_cls_slots = internal constant [14 x i64] [i64 2037, i64 1360, i64 1020, i64 816, i64 583, i64 454, i64 314, i64 240, i64 163, i64 123, i64 83, i64 62, i64 42, i64 31]
@npk_cls_bmw = internal constant [14 x i64] [i64 32, i64 22, i64 16, i64 13, i64 10, i64 8, i64 5, i64 4, i64 3, i64 2, i64 2, i64 1, i64 1, i64 1]
@npk_cls_data = internal constant [14 x i64] [i64 320, i64 240, i64 192, i64 176, i64 144, i64 128, i64 112, i64 96, i64 96, i64 80, i64 80, i64 80, i64 80, i64 80]
@npk_cls_guard = internal constant [14 x i64] [i64 65504, i64 65520, i64 65472, i64 65456, i64 65440, i64 65504, i64 65424, i64 65376, i64 65296, i64 65024, i64 65152, i64 64560, i64 65264, i64 64064]
@npk_cls_part = internal global [14 x i64] zeroinitializer
@npk_cls_full = internal global [14 x i64] zeroinitializer
; QUARANTINE (1.2.3 debug instrument): when set, a freed small chunk is
; poisoned and stamped but NEVER returned to the bitmap -- every stale read
; hits 0xAA forever and every stale free hits the header-magic trap, instead
; of depending on reuse timing. Costs memory (no reuse); for defect hunts.
@npk_quarantine = internal global i64 0
; THE HEAP MUTEX (D-183, 1.2.5b). The allocator's bookkeeping — class heads,
; bitmaps, the chunk table, the large table — was single-threaded by an
; invariant the channel rung enforced: owning data never crossed a thread, so
; every thread freed only what it allocated. Owning channel elements END that
; invariant (a body allocated on the sender's thread is dropped on the
; receiver's), so the bookkeeping takes one futex mutex. Uncontended cost is
; an atomic exchange each way; the poison loop and the syscall-bearing large
; paths hold it longer, which is correctness buying its keep first
; (performance is measured after, per the standing order).
@npk_heap_mx = internal global i32 0

@npk_chtab = internal global i64 0
@npk_chtab_cap = internal global i64 0
@npk_chtab_len = internal global i64 0
@npk_lgtab = internal global i64 0
@npk_lgtab_cap = internal global i64 0
@npk_lgtab_len = internal global i64 0

; --- the three traps (D-141 region: -4102 integrity, -4103 oom, -4104 request)

define internal void @npk_heap_bad() noreturn {
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4102)
  unreachable
}

define internal void @npk_heap_oom() noreturn {
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4103)
  unreachable
}

define internal void @npk_heap_badreq() noreturn {
  call void @npk_chain_reset(i32 0)
  call void @npk_trap(i32 -4104)
  unreachable
}

; --- magic formulas, one definition per role so no site can drift -----------

define internal i64 @npk_m_chunk(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -4059465807380287133
  ret i64 %m
}

define internal i64 @npk_m_live(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 5885026092677834074
  ret i64 %m
}

define internal i64 @npk_m_freed(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -581298601873003291
  ret i64 %m
}

define internal i64 @npk_m_large(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 1884440546999092433
  ret i64 %m
}

define internal i64 @npk_m_livew(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 8639445676566075373
  ret i64 %m
}

define internal i64 @npk_m_largew(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 4436545153374750491
  ret i64 %m
}

define internal i64 @npk_m_guard(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 7651035258233467253
  ret i64 %m
}

; --- mmap-or-trap. All allocator memory arrives through here. ---------------

define internal i64 @npk_hmap(i64 %len) {
  %m = call i64 @npk_sys6(i64 9, i64 0, i64 %len, i64 3, i64 34, i64 -1, i64 0)
  %bad = icmp ugt i64 %m, -4096
  br i1 %bad, label %oom, label %ok
oom:
  call void @npk_heap_oom()
  unreachable
ok:
  ret i64 %m
}

define internal void @npk_hunmap(i64 %a, i64 %len) {
  %r = call i64 @npk_sys6(i64 11, i64 %a, i64 %len, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp ne i64 %r, 0
  br i1 %bad, label %trap, label %ok
trap:
  ; munmap failing means the table and the kernel disagree about what is
  ; mapped -- an integrity failure, not an OOM.
  call void @npk_heap_bad()
  unreachable
ok:
  ret void
}

; --- init: the secret, then the two tables. Idempotent, called lazily. ------

define internal void @npk_heap_init() {
  %s0 = load i64, ptr @npk_hsec
  %live = icmp ne i64 %s0, 0
  br i1 %live, label %done, label %seed
seed:
  ; getrandom(&npk_hsec, 8, 0)
  %addr = ptrtoint ptr @npk_hsec to i64
  %r = call i64 @npk_sys6(i64 318, i64 %addr, i64 8, i64 0, i64 0, i64 0, i64 0)
  %short = icmp ne i64 %r, 8
  br i1 %short, label %trap, label %nz
trap:
  call void @npk_heap_bad()
  unreachable
nz:
  ; zero is the uninitialized sentinel, so a zero draw takes a fixed odd
  ; constant instead -- weaker keying for one run in 2^64, never a stall.
  %s1 = load i64, ptr @npk_hsec
  %z = icmp eq i64 %s1, 0
  br i1 %z, label %fix, label %tabs
fix:
  store i64 -7046029254386353131, ptr @npk_hsec
  br label %tabs
tabs:
  %ct = call i64 @npk_hmap(i64 4096)
  store i64 %ct, ptr @npk_chtab
  store i64 512, ptr @npk_chtab_cap
  store i64 0, ptr @npk_chtab_len
  %lt = call i64 @npk_hmap(i64 4096)
  store i64 %lt, ptr @npk_lgtab
  store i64 128, ptr @npk_lgtab_cap
  store i64 0, ptr @npk_lgtab_len
  br label %done
done:
  ret void
}

; --- the sorted chunk table: membership before any dereference --------------

define internal i64 @npk_chtab_find(i64 %a) {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %head
head:
  %lo = phi i64 [ 0, %entry ], [ %lo2, %step ]
  %hi = phi i64 [ %len, %entry ], [ %hi2, %step ]
  %more = icmp ult i64 %lo, %hi
  br i1 %more, label %probe, label %miss
probe:
  %sum = add i64 %lo, %hi
  %mid = lshr i64 %sum, 1
  %ea = add i64 %tab, %mid
  %eoff = shl i64 %mid, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %v = load i64, ptr %epp
  %eq = icmp eq i64 %v, %a
  br i1 %eq, label %hit, label %cmp
cmp:
  %lt = icmp ult i64 %v, %a
  %lo3 = add i64 %mid, 1
  br label %step
step:
  %lo2 = select i1 %lt, i64 %lo3, i64 %lo
  %hi2 = select i1 %lt, i64 %hi, i64 %mid
  br label %head
hit:
  ret i64 %mid
miss:
  ret i64 -1
}

define internal void @npk_chtab_insert(i64 %a) {
entry:
  %len = load i64, ptr @npk_chtab_len
  %cap = load i64, ptr @npk_chtab_cap
  %full = icmp eq i64 %len, %cap
  br i1 %full, label %grow, label %place
grow:
  %tab0 = load i64, ptr @npk_chtab
  %ncap = shl i64 %cap, 1
  %nbytes = shl i64 %ncap, 3
  %nt = call i64 @npk_hmap(i64 %nbytes)
  %obytes = shl i64 %cap, 3
  %ntp = inttoptr i64 %nt to ptr
  %otp = inttoptr i64 %tab0 to ptr
  %cbytes = shl i64 %len, 3
  call void @llvm.memcpy.p0.p0.i64(ptr %ntp, ptr %otp, i64 %cbytes, i1 false)
  call void @npk_hunmap(i64 %tab0, i64 %obytes)
  store i64 %nt, ptr @npk_chtab
  store i64 %ncap, ptr @npk_chtab_cap
  br label %place
place:
  %tab = load i64, ptr @npk_chtab
  br label %shift
shift:
  ; walk down from the end, moving greater entries up one slot
  %pos = phi i64 [ %len, %place ], [ %pos2, %move ]
  %atz = icmp eq i64 %pos, 0
  br i1 %atz, label %store, label %look
look:
  %pm1 = add i64 %pos, -1
  %poff = shl i64 %pm1, 3
  %pp = add i64 %tab, %poff
  %ppp = inttoptr i64 %pp to ptr
  %pv = load i64, ptr %ppp
  %gt = icmp ugt i64 %pv, %a
  br i1 %gt, label %move, label %store
move:
  %doff = shl i64 %pos, 3
  %dp = add i64 %tab, %doff
  %dpp = inttoptr i64 %dp to ptr
  store i64 %pv, ptr %dpp
  %pos2 = add i64 %pos, -1
  br label %shift
store:
  %soff = shl i64 %pos, 3
  %sp = add i64 %tab, %soff
  %spp = inttoptr i64 %sp to ptr
  store i64 %a, ptr %spp
  %nlen = add i64 %len, 1
  store i64 %nlen, ptr @npk_chtab_len
  ret void
}

; --- the large table: 32-byte entries { ptr, base, mapsize, size }, sorted --

define internal i64 @npk_lg_entry(i64 %idx) {
  %tab = load i64, ptr @npk_lgtab
  %off = shl i64 %idx, 5
  %e = add i64 %tab, %off
  ret i64 %e
}

define internal i64 @npk_lg_find(i64 %p) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  br label %head
head:
  %lo = phi i64 [ 0, %entry ], [ %lo2, %step ]
  %hi = phi i64 [ %len, %entry ], [ %hi2, %step ]
  %more = icmp ult i64 %lo, %hi
  br i1 %more, label %probe, label %miss
probe:
  %sum = add i64 %lo, %hi
  %mid = lshr i64 %sum, 1
  %e = call i64 @npk_lg_entry(i64 %mid)
  %ep = inttoptr i64 %e to ptr
  %v = load i64, ptr %ep
  %eq = icmp eq i64 %v, %p
  br i1 %eq, label %hit, label %cmp
cmp:
  %lt = icmp ult i64 %v, %p
  %lo3 = add i64 %mid, 1
  br label %step
step:
  %lo2 = select i1 %lt, i64 %lo3, i64 %lo
  %hi2 = select i1 %lt, i64 %hi, i64 %mid
  br label %head
hit:
  ret i64 %mid
miss:
  ret i64 -1
}

define internal void @npk_lg_insert(i64 %p, i64 %base, i64 %msz, i64 %sz) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  %cap = load i64, ptr @npk_lgtab_cap
  %full = icmp eq i64 %len, %cap
  br i1 %full, label %grow, label %place
grow:
  %tab0 = load i64, ptr @npk_lgtab
  %ncap = shl i64 %cap, 1
  %nbytes = shl i64 %ncap, 5
  %nt = call i64 @npk_hmap(i64 %nbytes)
  %obytes = shl i64 %cap, 5
  %cbytes = shl i64 %len, 5
  %ntp = inttoptr i64 %nt to ptr
  %otp = inttoptr i64 %tab0 to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %ntp, ptr %otp, i64 %cbytes, i1 false)
  call void @npk_hunmap(i64 %tab0, i64 %obytes)
  store i64 %nt, ptr @npk_lgtab
  store i64 %ncap, ptr @npk_lgtab_cap
  br label %place
place:
  br label %shift
shift:
  %pos = phi i64 [ %len, %place ], [ %pos2, %move ]
  %atz = icmp eq i64 %pos, 0
  br i1 %atz, label %write, label %look
look:
  %pm1 = add i64 %pos, -1
  %se = call i64 @npk_lg_entry(i64 %pm1)
  %sep = inttoptr i64 %se to ptr
  %sv = load i64, ptr %sep
  %gt = icmp ugt i64 %sv, %p
  br i1 %gt, label %move, label %write
move:
  %de = call i64 @npk_lg_entry(i64 %pos)
  %dep = inttoptr i64 %de to ptr
  %sp1 = inttoptr i64 %se to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %dep, ptr %sp1, i64 32, i1 false)
  %pos2 = add i64 %pos, -1
  br label %shift
write:
  %e = call i64 @npk_lg_entry(i64 %pos)
  %f0 = inttoptr i64 %e to ptr
  store i64 %p, ptr %f0
  %e1 = add i64 %e, 8
  %f1 = inttoptr i64 %e1 to ptr
  store i64 %base, ptr %f1
  %e2 = add i64 %e, 16
  %f2 = inttoptr i64 %e2 to ptr
  store i64 %msz, ptr %f2
  %e3 = add i64 %e, 24
  %f3 = inttoptr i64 %e3 to ptr
  store i64 %sz, ptr %f3
  %nlen = add i64 %len, 1
  store i64 %nlen, ptr @npk_lgtab_len
  ret void
}

define internal void @npk_lg_remove(i64 %idx) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  br label %head
head:
  %i = phi i64 [ %idx, %entry ], [ %i2, %body ]
  %i1 = add i64 %i, 1
  %more = icmp ult i64 %i1, %len
  br i1 %more, label %body, label %done
body:
  %de = call i64 @npk_lg_entry(i64 %i)
  %se = call i64 @npk_lg_entry(i64 %i1)
  %dep = inttoptr i64 %de to ptr
  %sep = inttoptr i64 %se to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %dep, ptr %sep, i64 32, i1 false)
  %i2 = add i64 %i1, 0
  br label %head
done:
  %nlen = add i64 %len, -1
  store i64 %nlen, ptr @npk_lgtab_len
  ret void
}

; --- chunk lists ------------------------------------------------------------

define internal void @npk_ch_push(ptr %hp, i64 %ch) {
  %h = load i64, ptr %hp
  %nx = add i64 %ch, 40
  %nxp = inttoptr i64 %nx to ptr
  store i64 %h, ptr %nxp
  %pv = add i64 %ch, 48
  %pvp = inttoptr i64 %pv to ptr
  store i64 0, ptr %pvp
  %some = icmp ne i64 %h, 0
  br i1 %some, label %link, label %sethead
link:
  %hpv = add i64 %h, 48
  %hpvp = inttoptr i64 %hpv to ptr
  store i64 %ch, ptr %hpvp
  br label %sethead
sethead:
  store i64 %ch, ptr %hp
  ret void
}

define internal void @npk_ch_unlink(ptr %hp, i64 %ch) {
  %nxa = add i64 %ch, 40
  %nxap = inttoptr i64 %nxa to ptr
  %nx = load i64, ptr %nxap
  %pva = add i64 %ch, 48
  %pvap = inttoptr i64 %pva to ptr
  %pv = load i64, ptr %pvap
  %isfirst = icmp eq i64 %pv, 0
  br i1 %isfirst, label %head, label %mid
head:
  store i64 %nx, ptr %hp
  br label %fixnext
mid:
  %pnx = add i64 %pv, 40
  %pnxp = inttoptr i64 %pnx to ptr
  store i64 %nx, ptr %pnxp
  br label %fixnext
fixnext:
  %some = icmp ne i64 %nx, 0
  br i1 %some, label %fix, label %clear
fix:
  %npv = add i64 %nx, 48
  %npvp = inttoptr i64 %npv to ptr
  store i64 %pv, ptr %npvp
  br label %clear
clear:
  store i64 0, ptr %nxap
  store i64 0, ptr %pvap
  ret void
}

; --- a fresh chunk: 64 KiB, 64 KiB-aligned by over-map-and-trim -------------

define internal i64 @npk_chunk_new(i64 %ci) {
entry:
  %raw = call i64 @npk_hmap(i64 131072)
  %r1 = add i64 %raw, 65535
  %al = and i64 %r1, -65536
  %pre = sub i64 %al, %raw
  %haspre = icmp ne i64 %pre, 0
  br i1 %haspre, label %trimpre, label %posttrim
trimpre:
  call void @npk_hunmap(i64 %raw, i64 %pre)
  br label %posttrim
posttrim:
  %post = sub i64 65536, %pre
  %haspost = icmp ne i64 %post, 0
  br i1 %haspost, label %trimpost, label %init
trimpost:
  %pend = add i64 %al, 65536
  call void @npk_hunmap(i64 %pend, i64 %post)
  br label %init
init:
  %m = call i64 @npk_m_chunk(i64 %al)
  %mp = inttoptr i64 %al to ptr
  store i64 %m, ptr %mp
  %cia = add i64 %al, 8
  %cip = inttoptr i64 %cia to ptr
  store i64 %ci, ptr %cip
  %sgep = getelementptr [14 x i64], ptr @npk_cls_slots, i64 0, i64 %ci
  %S = load i64, ptr %sgep
  %sca = add i64 %al, 16
  %scp = inttoptr i64 %sca to ptr
  store i64 %S, ptr %scp
  %fca = add i64 %al, 24
  %fcp = inttoptr i64 %fca to ptr
  store i64 %S, ptr %fcp
  %ha = add i64 %al, 32
  %hp = inttoptr i64 %ha to ptr
  store i64 0, ptr %hp
  %nxa = add i64 %al, 40
  %nxp = inttoptr i64 %nxa to ptr
  store i64 0, ptr %nxp
  %pva = add i64 %al, 48
  %pvp = inttoptr i64 %pva to ptr
  store i64 0, ptr %pvp
  %wma = add i64 %al, 56
  %wmp = inttoptr i64 %wma to ptr
  store i64 0, ptr %wmp
  %wgep = getelementptr [14 x i64], ptr @npk_cls_bmw, i64 0, i64 %ci
  %W = load i64, ptr %wgep
  br label %bmhead
bmhead:
  ; word w holds a set bit per free slot: full words of -1, then the remainder
  %w = phi i64 [ 0, %init ], [ %w2, %bmstore ]
  %morew = icmp ult i64 %w, %W
  br i1 %morew, label %bmbody, label %guard
bmbody:
  %before = shl i64 %w, 6
  %left = sub i64 %S, %before
  %ge64 = icmp uge i64 %left, 64
  br i1 %ge64, label %fullw, label %remw
fullw:
  br label %bmstore
remw:
  %one = shl i64 1, %left
  %rem = add i64 %one, -1
  br label %bmstore
bmstore:
  %val = phi i64 [ -1, %fullw ], [ %rem, %remw ]
  %woff = shl i64 %w, 3
  %wa0 = add i64 %al, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  store i64 %val, ptr %wp
  %w2 = add i64 %w, 1
  br label %bmhead
guard:
  %ggep = getelementptr [14 x i64], ptr @npk_cls_guard, i64 0, i64 %ci
  %go = load i64, ptr %ggep
  %ga = add i64 %al, %go
  %g0 = call i64 @npk_m_guard(i64 %ga)
  %gp0 = inttoptr i64 %ga to ptr
  store i64 %g0, ptr %gp0
  %ga1 = add i64 %ga, 8
  %g1 = call i64 @npk_m_guard(i64 %ga1)
  %gp1 = inttoptr i64 %ga1 to ptr
  store i64 %g1, ptr %gp1
  call void @npk_chtab_insert(i64 %al)
  ret i64 %al
}

; --- guard and header checks, shared ----------------------------------------

define internal void @npk_chunk_guard_check(i64 %ch, i64 %ci) {
  %ggep = getelementptr [14 x i64], ptr @npk_cls_guard, i64 0, i64 %ci
  %go = load i64, ptr %ggep
  %ga = add i64 %ch, %go
  %w0 = call i64 @npk_m_guard(i64 %ga)
  %gp0 = inttoptr i64 %ga to ptr
  %v0 = load i64, ptr %gp0
  %ok0 = icmp eq i64 %v0, %w0
  br i1 %ok0, label %second, label %trap
second:
  %ga1 = add i64 %ga, 8
  %w1 = call i64 @npk_m_guard(i64 %ga1)
  %gp1 = inttoptr i64 %ga1 to ptr
  %v1 = load i64, ptr %gp1
  %ok1 = icmp eq i64 %v1, %w1
  br i1 %ok1, label %done, label %trap
trap:
  call void @npk_heap_bad()
  unreachable
done:
  ret void
}

; Validate a small-block pointer end to end WITHOUT mutating: proves p is a
; live slot of one of our chunks and returns the chunk address. Traps on any
; violation. Callers re-derive slot arithmetic; validation lives only here.
define internal i64 @npk_small_check(i64 %ip) {
entry:
  %ch = and i64 %ip, -65536
  %idx = call i64 @npk_chtab_find(i64 %ch)
  %miss = icmp slt i64 %idx, 0
  br i1 %miss, label %trap, label %magic
magic:
  %want = call i64 @npk_m_chunk(i64 %ch)
  %mp = inttoptr i64 %ch to ptr
  %have = load i64, ptr %mp
  %mok = icmp eq i64 %have, %want
  br i1 %mok, label %klass, label %trap
klass:
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %cibad = icmp uge i64 %ci, 14
  br i1 %cibad, label %trap, label %geom
geom:
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %sgep = getelementptr [14 x i64], ptr @npk_cls_slots, i64 0, i64 %ci
  %S = load i64, ptr %sgep
  %b = add i64 %ip, -16
  %dstart = add i64 %ch, %data
  %off = sub i64 %b, %dstart
  %neg = icmp slt i64 %off, 0
  br i1 %neg, label %trap, label %shape
shape:
  %rem = urem i64 %off, %stride
  %misfit = icmp ne i64 %rem, 0
  br i1 %misfit, label %trap, label %range
range:
  %slot = udiv i64 %off, %stride
  %oob = icmp uge i64 %slot, %S
  br i1 %oob, label %trap, label %wmk
wmk:
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  %virgin = icmp uge i64 %slot, %wm
  br i1 %virgin, label %trap, label %bmap
bmap:
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %freebit = and i64 %word, %mask
  %isfree = icmp ne i64 %freebit, 0
  br i1 %isfree, label %trap, label %hdr
hdr:
  %hmi = call i64 @npk_m_live(i64 %b)
  %hmw = call i64 @npk_m_livew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %hv = load i64, ptr %hmp
  %oki = icmp eq i64 %hv, %hmi
  %okw = icmp eq i64 %hv, %hmw
  %hok = or i1 %oki, %okw
  br i1 %hok, label %tail, label %trap
tail:
  call void @npk_chunk_guard_check(i64 %ch, i64 %ci)
  ret i64 %ch
trap:
  call void @npk_heap_bad()
  unreachable
}

; --- the small path ---------------------------------------------------------

define internal ptr @npk_small_alloc(i64 %n, i64 %ci, i64 %wild) {
entry:
  %hp = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %ci
  %h0 = load i64, ptr %hp
  %none = icmp eq i64 %h0, 0
  br i1 %none, label %fresh, label %have
fresh:
  %nc = call i64 @npk_chunk_new(i64 %ci)
  call void @npk_ch_push(ptr %hp, i64 %nc)
  br label %have
have:
  %ch = load i64, ptr %hp
  %wgep = getelementptr [14 x i64], ptr @npk_cls_bmw, i64 0, i64 %ci
  %W = load i64, ptr %wgep
  %ha = add i64 %ch, 32
  %hip = inttoptr i64 %ha to ptr
  %hint = load i64, ptr %hip
  br label %scan
scan:
  %w = phi i64 [ %hint, %have ], [ %w2, %next ]
  %inw = icmp ult i64 %w, %W
  br i1 %inw, label %look, label %broken
look:
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %nz = icmp ne i64 %word, 0
  br i1 %nz, label %take, label %next
next:
  %w2 = add i64 %w, 1
  br label %scan
broken:
  ; a partial chunk with no set bit is a bookkeeping violation
  call void @npk_heap_bad()
  unreachable
take:
  %bit = call i64 @llvm.cttz.i64(i64 %word, i1 true)
  %shifted = shl i64 %w, 6
  %slot = add i64 %shifted, %bit
  %wm1 = add i64 %word, -1
  %nword = and i64 %word, %wm1
  store i64 %nword, ptr %wp
  ; hint: this word if it still has bits, else the next
  %emptied = icmp eq i64 %nword, 0
  %w3 = add i64 %w, 1
  %nhint = select i1 %emptied, i64 %w3, i64 %w
  store i64 %nhint, ptr %hip
  %fca = add i64 %ch, 24
  %fcp = inttoptr i64 %fca to ptr
  %free = load i64, ptr %fcp
  %nf = add i64 %free, -1
  store i64 %nf, ptr %fcp
  %isfull = icmp eq i64 %nf, 0
  br i1 %isfull, label %tofull, label %geom
tofull:
  %fhp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %ci
  call void @npk_ch_unlink(ptr %hp, i64 %ch)
  call void @npk_ch_push(ptr %fhp, i64 %ch)
  br label %geom
geom:
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %soff = mul i64 %slot, %stride
  %d0 = add i64 %ch, %data
  %b = add i64 %d0, %soff
  ; watermark: below it the slot was used before and must still carry its
  ; FREED magic -- a UAF that scribbled a freed block traps here, on reuse
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  %virgin = icmp uge i64 %slot, %wm
  br i1 %virgin, label %mark, label %verify
mark:
  %nwm = add i64 %slot, 1
  store i64 %nwm, ptr %wmp
  br label %stamp
verify:
  %fm = call i64 @npk_m_freed(i64 %b)
  %fma = add i64 %b, 8
  %fmp = inttoptr i64 %fma to ptr
  %fv = load i64, ptr %fmp
  %fok = icmp eq i64 %fv, %fm
  br i1 %fok, label %stamp, label %poisoned
poisoned:
  call void @npk_heap_bad()
  unreachable
stamp:
  %szp = inttoptr i64 %b to ptr
  store i64 %n, ptr %szp
  %lmi = call i64 @npk_m_live(i64 %b)
  %lmw = call i64 @npk_m_livew(i64 %b)
  %iswild = icmp ne i64 %wild, 0
  %lm = select i1 %iswild, i64 %lmw, i64 %lmi
  %lma = add i64 %b, 8
  %lmp = inttoptr i64 %lma to ptr
  store i64 %lm, ptr %lmp
  %u = add i64 %b, 16
  %p = inttoptr i64 %u to ptr
  ret ptr %p
}

define internal void @npk_small_free(i64 %ip) {
entry:
  %ch = call i64 @npk_small_check(i64 %ip)
  %hsb = add i64 %ip, -16
  %hsp = inttoptr i64 %hsb to ptr
  %hsn = load i64, ptr %hsp
  call void @npk_hs_note_free(i64 %hsn)
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %b = add i64 %ip, -16
  %dstart = add i64 %ch, %data
  %off = sub i64 %b, %dstart
  %slot = udiv i64 %off, %stride
  ; POISON THE FREED PAYLOAD (D-183, 1.2.3 — an instrument that stays). A
  ; body freed while a second header still points at it is invisible until
  ; the allocator happens to REUSE the chunk, which makes the defect rare,
  ; schedule-shaped, and unreproducible — the class the stress marker exists
  ; for, one layer down. 0xAA in every freed byte makes the very first stale
  ; read produce loud deterministic garbage instead: stage 2's one-in-242k-
  ; lines corruption becomes visible at every site, every run. Cost: a short
  ; store loop per free, on the free path only.
  %pz = inttoptr i64 %ip to ptr
  br label %poison
poison:
  %pi = phi i64 [ 0, %entry ], [ %pin, %poison_step ]
  %pdone = icmp uge i64 %pi, %cls
  br i1 %pdone, label %poisoned, label %poison_step
poison_step:
  %pp = getelementptr i8, ptr %pz, i64 %pi
  store i8 -86, ptr %pp
  %pin = add i64 %pi, 1
  br label %poison
poisoned:
  ; stamp FREED, then flip the bit
  %fm = call i64 @npk_m_freed(i64 %b)
  %fma = add i64 %b, 8
  %fmp = inttoptr i64 %fma to ptr
  store i64 %fm, ptr %fmp
  %qv = load i64, ptr @npk_quarantine
  %qon = icmp ne i64 %qv, 0
  br i1 %qon, label %done, label %reclaim
reclaim:
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %nword = or i64 %word, %mask
  store i64 %nword, ptr %wp
  %fca = add i64 %ch, 24
  %fcp = inttoptr i64 %fca to ptr
  %free = load i64, ptr %fcp
  %nf = add i64 %free, 1
  store i64 %nf, ptr %fcp
  %ha = add i64 %ch, 32
  %hip = inttoptr i64 %ha to ptr
  %hint = load i64, ptr %hip
  %lower = icmp ult i64 %w, %hint
  %nhint = select i1 %lower, i64 %w, i64 %hint
  store i64 %nhint, ptr %hip
  %wasfull = icmp eq i64 %free, 0
  br i1 %wasfull, label %topart, label %done
topart:
  %fhp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %ci
  %php = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %ci
  call void @npk_ch_unlink(ptr %fhp, i64 %ch)
  call void @npk_ch_push(ptr %php, i64 %ch)
  br label %done
done:
  ret void
}

; --- the large path ---------------------------------------------------------

define internal ptr @npk_large_new(i64 %n, i64 %align, i64 %wild) {
entry:
  %r15 = add i64 %n, 15
  %rn = and i64 %r15, -16
  %wide = icmp ugt i64 %align, 16
  %extra = select i1 %wide, i64 %align, i64 0
  %n32 = add i64 %rn, 32
  %need = add i64 %n32, %extra
  %n4095 = add i64 %need, 4095
  %msz = and i64 %n4095, -4096
  %base = call i64 @npk_hmap(i64 %msz)
  %p0 = add i64 %base, 16
  %am1 = add i64 %align, -1
  %pa0 = add i64 %p0, %am1
  %nega = sub i64 0, %align
  %pa = and i64 %pa0, %nega
  ; the contract, asserted where it is produced: the payload meets the
  ; requested alignment and the block fits its mapping
  %alcheck = and i64 %pa, %am1
  %misaligned = icmp ne i64 %alcheck, 0
  %endp = add i64 %pa, %rn
  %endg = add i64 %endp, 16
  %mapend = add i64 %base, %msz
  %overrun = icmp ugt i64 %endg, %mapend
  %broken = or i1 %misaligned, %overrun
  br i1 %broken, label %selftrap, label %fit
selftrap:
  call void @npk_heap_bad()
  unreachable
fit:
  %b = add i64 %pa, -16
  %szp = inttoptr i64 %b to ptr
  store i64 %n, ptr %szp
  %lmi = call i64 @npk_m_large(i64 %b)
  %lmw = call i64 @npk_m_largew(i64 %b)
  %iswild = icmp ne i64 %wild, 0
  %lm = select i1 %iswild, i64 %lmw, i64 %lmi
  %lma = add i64 %b, 8
  %lmp = inttoptr i64 %lma to ptr
  store i64 %lm, ptr %lmp
  %fa = add i64 %pa, %rn
  %g0 = call i64 @npk_m_guard(i64 %fa)
  %gp0 = inttoptr i64 %fa to ptr
  store i64 %g0, ptr %gp0
  %fa1 = add i64 %fa, 8
  %g1 = call i64 @npk_m_guard(i64 %fa1)
  %gp1 = inttoptr i64 %fa1 to ptr
  store i64 %g1, ptr %gp1
  call void @npk_lg_insert(i64 %pa, i64 %base, i64 %msz, i64 %n)
  %p = inttoptr i64 %pa to ptr
  ret ptr %p
}

; Validate a large block's header and footer against its table entry. Traps on
; violation. %e is the entry address.
define internal void @npk_large_check(i64 %ip, i64 %e) {
entry:
  %b = add i64 %ip, -16
  %wanti = call i64 @npk_m_large(i64 %b)
  %wantw = call i64 @npk_m_largew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %have = load i64, ptr %hmp
  %oki = icmp eq i64 %have, %wanti
  %okw = icmp eq i64 %have, %wantw
  %hok = or i1 %oki, %okw
  br i1 %hok, label %foot, label %trap
foot:
  %sza = add i64 %e, 24
  %szp = inttoptr i64 %sza to ptr
  %sz = load i64, ptr %szp
  %r15 = add i64 %sz, 15
  %rn = and i64 %r15, -16
  %fa = add i64 %ip, %rn
  %w0 = call i64 @npk_m_guard(i64 %fa)
  %fp0 = inttoptr i64 %fa to ptr
  %v0 = load i64, ptr %fp0
  %ok0 = icmp eq i64 %v0, %w0
  br i1 %ok0, label %foot2, label %trap
foot2:
  %fa1 = add i64 %fa, 8
  %w1 = call i64 @npk_m_guard(i64 %fa1)
  %fp1 = inttoptr i64 %fa1 to ptr
  %v1 = load i64, ptr %fp1
  %ok1 = icmp eq i64 %v1, %w1
  br i1 %ok1, label %done, label %trap
trap:
  call void @npk_heap_bad()
  unreachable
done:
  ret void
}

; --- NPK_HEAP_STATS (1.5.1b step 0): the allocator's own numbers ------------
;
; A debug instrument that stays, beside the 0xAA poisoning and the quarantine.
; Four words, kept under the heap mutex like the rest of the bookkeeping:
; the bytes REQUESTED in total, the bytes live now, the peak of live, and the
; count of allocations. `npk_exit` prints them as one line on fd 2 when the
; environment carries `NPK_HEAP_STATS` (`npk_hs_report` below) and is silent
; otherwise, so a program's behaviour never depends on the variable -- the
; line is data for a harness stage, never a verdict inside the program.
;
; WHY THE ALLOCATOR'S NUMBER AND NOT THE PROCESS'S: resident memory is the
; kernel's answer and moves with page reuse and load; a wall-clock is a
; measurement of the machine. These counters are a function of the program's
; own allocation sequence, so the same input gives the same numbers on every
; machine and every run -- what makes "the fix must be a number" checkable.
; The cost on the hot path is the adds and one compare, inside the lock the
; path already holds.
;
; SIZES ARE THE REQUESTED SIZES the headers already carry (a block's `n` at
; payload-16, a large entry's at e+24), never the rounded class sizes, so an
; `alloc(1)` counts as 1 and a resize counts its difference. A free reads its
; size AFTER the header has been validated (`npk_small_check`,
; `npk_large_check`), never before: a bogus pointer must still reach the
; heap-integrity trap it reaches today, not a fault on the read.

@npk_hs_allocated = internal global i64 0
@npk_hs_live      = internal global i64 0
@npk_hs_peak      = internal global i64 0
@npk_hs_count     = internal global i64 0

define internal void @npk_hs_note_alloc(i64 %n) {
entry:
  %a = load i64, ptr @npk_hs_allocated
  %a1 = add i64 %a, %n
  store i64 %a1, ptr @npk_hs_allocated
  %c = load i64, ptr @npk_hs_count
  %c1 = add i64 %c, 1
  store i64 %c1, ptr @npk_hs_count
  %l = load i64, ptr @npk_hs_live
  %l1 = add i64 %l, %n
  store i64 %l1, ptr @npk_hs_live
  %p = load i64, ptr @npk_hs_peak
  %up = icmp ugt i64 %l1, %p
  %p1 = select i1 %up, i64 %l1, i64 %p
  store i64 %p1, ptr @npk_hs_peak
  ret void
}

define internal void @npk_hs_note_free(i64 %n) {
entry:
  %l = load i64, ptr @npk_hs_live
  %l1 = sub i64 %l, %n
  store i64 %l1, ptr @npk_hs_live
  ret void
}

; An in-place resize: live moves by the difference; growth is new bytes
; requested and may set a new peak; a shrink requests nothing.
define internal void @npk_hs_note_resize(i64 %old, i64 %new) {
entry:
  %d = sub i64 %new, %old
  %l = load i64, ptr @npk_hs_live
  %l1 = add i64 %l, %d
  store i64 %l1, ptr @npk_hs_live
  %grew = icmp sgt i64 %d, 0
  br i1 %grew, label %grow, label %done
grow:
  %a = load i64, ptr @npk_hs_allocated
  %a1 = add i64 %a, %d
  store i64 %a1, ptr @npk_hs_allocated
  %p = load i64, ptr @npk_hs_peak
  %up = icmp ugt i64 %l1, %p
  %p1 = select i1 %up, i64 %l1, i64 %p
  store i64 %p1, ptr @npk_hs_peak
  br label %done
done:
  ret void
}

; --- the four builtins, plus aalloc -----------------------------------------

define internal ptr @npk_alloc_impl(i64 %n, i64 %wild) {
entry:
  call void @npk_mx_lock(ptr @npk_heap_mx)
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %init, label %sized
init:
  call void @npk_heap_init()
  br label %sized
sized:
  %neg = icmp slt i64 %n, 0
  br i1 %neg, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  ; alloc(0) is a real, unique, freeable 16-byte block (D-150) -- a trap here
  ; would make every `alloc(count * elem)` with a legal zero count a landmine
  %z = icmp eq i64 %n, 0
  %n1 = select i1 %z, i64 16, i64 %n
  %r15 = add i64 %n1, 15
  %rn = and i64 %r15, -16
  br label %pick
pick:
  %ci = phi i64 [ 0, %norm ], [ %ci2, %bigger ]
  %inrange = icmp ult i64 %ci, 14
  br i1 %inrange, label %try, label %large
try:
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %fits = icmp ule i64 %rn, %cls
  br i1 %fits, label %small, label %bigger
bigger:
  %ci2 = add i64 %ci, 1
  br label %pick
small:
  %sp = call ptr @npk_small_alloc(i64 %n1, i64 %ci, i64 %wild)
  call void @npk_hs_note_alloc(i64 %n1)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret ptr %sp
large:
  %lp = call ptr @npk_large_new(i64 %n1, i64 16, i64 %wild)
  call void @npk_hs_note_alloc(i64 %n1)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret ptr %lp
}

; The WILD entry -- the alloc builtin. What this hands out is in the
; <wild-live> set until dalloc'd, and the exit check counts it (D-151).
define ptr @npk_alloc(i64 %n) {
  %p = call ptr @npk_alloc_impl(i64 %n, i64 1)
  ret ptr %p
}

; The MANAGED entry -- program-owned storage the GENERATED drops release
; (D-183, 1.2.4): today the `dyn` cell a coercion moves its concrete value
; into. Not in <wild-live> -- scope exit frees it, the way a string body's
; drop already works -- and dalloc takes it back like any managed body.
define ptr @npk_alloc_managed(i64 %n) {
  %p = call ptr @npk_alloc_impl(i64 %n, i64 0)
  ret ptr %p
}

; buffer_new (D-200/S23, 1.3.7): the MANAGED owning byte cell -- a zeroed
; body with len = cap = n, whose scope-exit drop (the string's cap-gated
; body, shared because the layout is) frees it. n <= 0 is the empty,
; non-owning buffer {null, 0, 0} -- cap 0 is the not-mine bit. Allocation
; failure traps inside the allocator (HeapOom -> failsafe), so the Result
; is always the ok arm -- `never fails` on the frontend side.
define { { ptr, i64, i64 }, i32 } @npk_buffer_new(i64 %n) {
entry:
  %pos = icmp sgt i64 %n, 0
  br i1 %pos, label %mk, label %empty
mk:
  %p = call ptr @npk_alloc_impl(i64 %n, i64 0)
  call ptr @memset(ptr %p, i32 0, i64 %n)
  %b0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %p, 0
  %b1 = insertvalue { ptr, i64, i64 } %b0, i64 %n, 1
  %b2 = insertvalue { ptr, i64, i64 } %b1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %b2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
empty:
  %e0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr null, 0
  %e1 = insertvalue { ptr, i64, i64 } %e0, i64 0, 1
  %e2 = insertvalue { ptr, i64, i64 } %e1, i64 0, 2
  %s0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %e2, 0
  %s1 = insertvalue { { ptr, i64, i64 }, i32 } %s0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %s1
}

; The INTERNAL entry -- runtime-owned storage (string bodies, argv, file
; buffers). Managed-regime: not in <wild-live>; its RAII lands with the
; managed lowering, and wild_release_all still reclaims it wholesale.
define internal ptr @npk_alloc_internal(i64 %n) {
  %p = call ptr @npk_alloc_impl(i64 %n, i64 0)
  ret ptr %p
}

define ptr @npk_calloc(i64 %count, i64 %size) {
entry:
  %negc = icmp slt i64 %count, 0
  %negs = icmp slt i64 %size, 0
  %neg = or i1 %negc, %negs
  br i1 %neg, label %badreq, label %mul
mul:
  ; the multiply is CHECKED (0.10.0): a wrap here is an undersized allocation
  ; wearing a plausible size
  %wo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %count, i64 %size)
  %prod = extractvalue { i64, i1 } %wo, 0
  %ovf = extractvalue { i64, i1 } %wo, 1
  br i1 %ovf, label %badreq, label %ok
badreq:
  call void @npk_heap_badreq()
  unreachable
ok:
  %p = call ptr @npk_alloc(i64 %prod)
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 %prod, i1 false)
  ret ptr %p
}

define ptr @npk_ralloc(ptr %old, i64 %n) {
entry:
  %isnull = icmp eq ptr %old, null
  br i1 %isnull, label %plain, label %sized
plain:
  %f = call ptr @npk_alloc(i64 %n)
  ret ptr %f
sized:
  ; ralloc(p, 0) is REFUSED (D-150): freeing is spelled dalloc, and C's
  ; realloc(p, 0) is the exact implementation-defined footgun this is not
  %neg = icmp slt i64 %n, 1
  br i1 %neg, label %badreq, label %checks
badreq:
  call void @npk_heap_badreq()
  unreachable
checks:
  %ip = ptrtoint ptr %old to i64
  %misal = and i64 %ip, 15
  %crooked = icmp ne i64 %misal, 0
  br i1 %crooked, label %bad, label %virginheap
bad:
  call void @npk_heap_bad()
  unreachable
virginheap:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %bad, label %route
route:
  call void @npk_mx_lock(ptr @npk_heap_mx)
  %lgidx = call i64 @npk_lg_find(i64 %ip)
  %islarge = icmp sge i64 %lgidx, 0
  br i1 %islarge, label %lg, label %sm
lg:
  %e = call i64 @npk_lg_entry(i64 %lgidx)
  call void @npk_large_check(i64 %ip, i64 %e)
  %basea = add i64 %e, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %e, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  %osza = add i64 %e, 24
  %oszp = inttoptr i64 %osza to ptr
  %osz = load i64, ptr %oszp
  %r15 = add i64 %n, 15
  %rn = and i64 %r15, -16
  %inblock = sub i64 %ip, %base
  %needend = add i64 %inblock, %rn
  %needall = add i64 %needend, 16
  %fitsin = icmp ule i64 %needall, %msz
  br i1 %fitsin, label %inplace, label %move
inplace:
  %b = add i64 %ip, -16
  %szslot = inttoptr i64 %b to ptr
  store i64 %n, ptr %szslot
  store i64 %n, ptr %oszp
  call void @npk_hs_note_resize(i64 %osz, i64 %n)
  %fa = add i64 %ip, %rn
  %g0 = call i64 @npk_m_guard(i64 %fa)
  %gp0 = inttoptr i64 %fa to ptr
  store i64 %g0, ptr %gp0
  %fa1 = add i64 %fa, 8
  %g1 = call i64 @npk_m_guard(i64 %fa1)
  %gp1 = inttoptr i64 %fa1 to ptr
  store i64 %g1, ptr %gp1
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret ptr %old
move:
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ; the regime travels with the data: a wild block moves to a wild block
  %mb = add i64 %ip, -16
  %mw = call i64 @npk_m_largew(i64 %mb)
  %mha = add i64 %mb, 8
  %mhp = inttoptr i64 %mha to ptr
  %mhv = load i64, ptr %mhp
  %mwild = icmp eq i64 %mhv, %mw
  %mrole = select i1 %mwild, i64 1, i64 0
  %np = call ptr @npk_alloc_impl(i64 %n, i64 %mrole)
  %smaller = icmp ult i64 %osz, %n
  %cnt = select i1 %smaller, i64 %osz, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %np, ptr %old, i64 %cnt, i1 false)
  call void @npk_dalloc(ptr %old)
  ret ptr %np
sm:
  %ch = call i64 @npk_small_check(i64 %ip)
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %b2 = add i64 %ip, -16
  %oszp2 = inttoptr i64 %b2 to ptr
  %osz2 = load i64, ptr %oszp2
  %r15b = add i64 %n, 15
  %rnb = and i64 %r15b, -16
  %fitscls = icmp ule i64 %rnb, %cls
  br i1 %fitscls, label %inplace2, label %move2
inplace2:
  store i64 %n, ptr %oszp2
  call void @npk_hs_note_resize(i64 %osz2, i64 %n)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret ptr %old
move2:
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  %mb2 = add i64 %ip, -16
  %mw2 = call i64 @npk_m_livew(i64 %mb2)
  %mha2 = add i64 %mb2, 8
  %mhp2 = inttoptr i64 %mha2 to ptr
  %mhv2 = load i64, ptr %mhp2
  %mwild2 = icmp eq i64 %mhv2, %mw2
  %mrole2 = select i1 %mwild2, i64 1, i64 0
  %np2 = call ptr @npk_alloc_impl(i64 %n, i64 %mrole2)
  %smaller2 = icmp ult i64 %osz2, %n
  %cnt2 = select i1 %smaller2, i64 %osz2, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %np2, ptr %old, i64 %cnt2, i1 false)
  call void @npk_dalloc(ptr %old)
  ret ptr %np2
}

define void @npk_dalloc(ptr %p) {
entry:
  ; dalloc(NULL) is a TRAP (D-150): alloc never returns null, so a null here
  ; is a program in a state its author did not intend -- explicit over
  ; implicit, and unlike C there is no cleanup idiom that needs free(NULL)
  %ip = ptrtoint ptr %p to i64
  %isz = icmp eq i64 %ip, 0
  br i1 %isz, label %bad, label %aligned
bad:
  call void @npk_heap_bad()
  unreachable
aligned:
  %misal = and i64 %ip, 15
  %crooked = icmp ne i64 %misal, 0
  br i1 %crooked, label %bad, label %virginheap
virginheap:
  ; a free before the first allocation cannot name our memory
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %bad, label %route
route:
  call void @npk_mx_lock(ptr @npk_heap_mx)
  %lgidx = call i64 @npk_lg_find(i64 %ip)
  %islarge = icmp sge i64 %lgidx, 0
  br i1 %islarge, label %lg, label %sm
lg:
  %e = call i64 @npk_lg_entry(i64 %lgidx)
  call void @npk_large_check(i64 %ip, i64 %e)
  %basea = add i64 %e, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %e, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  %dsza = add i64 %e, 24
  %dszp = inttoptr i64 %dsza to ptr
  %dsz = load i64, ptr %dszp
  call void @npk_hs_note_free(i64 %dsz)
  call void @npk_lg_remove(i64 %lgidx)
  call void @npk_hunmap(i64 %base, i64 %msz)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret void
sm:
  call void @npk_small_free(i64 %ip)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret void
}

define ptr @npk_aalloc(i64 %n, i64 %align) {
entry:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %init, label %checks
init:
  call void @npk_heap_init()
  br label %checks
checks:
  %negn = icmp slt i64 %n, 0
  br i1 %negn, label %badreq, label %alignck
alignck:
  %zeroa = icmp sle i64 %align, 0
  br i1 %zeroa, label %badreq, label %pow2
pow2:
  %am1 = add i64 %align, -1
  %mix = and i64 %align, %am1
  %notpow = icmp ne i64 %mix, 0
  br i1 %notpow, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  %z = icmp eq i64 %n, 0
  %n1 = select i1 %z, i64 16, i64 %n
  %smalla = icmp ule i64 %align, 16
  br i1 %smalla, label %plain, label %wide
plain:
  ; every ordinary block is 16-aligned already
  %p = call ptr @npk_alloc(i64 %n1)
  ret ptr %p
wide:
  ; UNDER THE HEAP MUTEX (1.5.1b step 0). This path called `npk_large_new` --
  ; which inserts into the large table -- with no lock since the mutex arrived
  ; at 1.2.5b: `npk_alloc_impl` locks around the same call, and this one did
  ; not. Two threads asking for an over-aligned block could race the table.
  ; Found while placing the accounting, which needs the lock too.
  call void @npk_mx_lock(ptr @npk_heap_mx)
  %q = call ptr @npk_large_new(i64 %n1, i64 %align, i64 1)
  call void @npk_hs_note_alloc(i64 %n1)
  call void @npk_mx_unlock(ptr @npk_heap_mx)
  ret ptr %q
}

; --- the <wild-live> registry view (0.10.1, D-151) --------------------------
;
; The chunk bitmaps and the large table ARE the live-set; these two walk it.
; Both are allocation-free and walk only preallocated state, so they are safe
; from failsafe in a degraded process -- the C-3 discipline.

define i64 @npk_wild_live_count() {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %chead
chead:
  %i = phi i64 [ 0, %entry ], [ %i2, %cnext ]
  %acc = phi i64 [ 0, %entry ], [ %acc2, %cnext ]
  %more = icmp ult i64 %i, %len
  br i1 %more, label %cbody, label %larges
cbody:
  %eoff = shl i64 %i, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %ch = load i64, ptr %epp
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  br label %shead
shead:
  ; every slot below the watermark: live iff its free bit is CLEAR, wild iff
  ; its header carries the wild role
  %slot = phi i64 [ 0, %cbody ], [ %slot2, %snext ]
  %sacc = phi i64 [ %acc, %cbody ], [ %sacc2, %snext ]
  %smore = icmp ult i64 %slot, %wm
  br i1 %smore, label %sbody, label %cdone
sbody:
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %fb = and i64 %word, %mask
  %isfree = icmp ne i64 %fb, 0
  br i1 %isfree, label %snext0, label %livecheck
livecheck:
  %soff = mul i64 %slot, %stride
  %d0 = add i64 %ch, %data
  %b = add i64 %d0, %soff
  %wantw = call i64 @npk_m_livew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %hv = load i64, ptr %hmp
  %isw = icmp eq i64 %hv, %wantw
  %inc = select i1 %isw, i64 1, i64 0
  br label %scount
snext0:
  br label %scount
scount:
  %add = phi i64 [ %inc, %livecheck ], [ 0, %snext0 ]
  br label %snext
snext:
  %sacc2 = add i64 %sacc, %add
  %slot2 = add i64 %slot, 1
  br label %shead
cdone:
  br label %cnext
cnext:
  %acc2 = add i64 %sacc, 0
  %i2 = add i64 %i, 1
  br label %chead
larges:
  %ltab = load i64, ptr @npk_lgtab
  %llen = load i64, ptr @npk_lgtab_len
  br label %lhead
lhead:
  %j = phi i64 [ 0, %larges ], [ %j2, %lnext ]
  %lacc = phi i64 [ %acc, %larges ], [ %lacc2, %lnext ]
  %lmore = icmp ult i64 %j, %llen
  br i1 %lmore, label %lbody, label %out
lbody:
  %leoff = shl i64 %j, 5
  %lep = add i64 %ltab, %leoff
  %lepp = inttoptr i64 %lep to ptr
  %p = load i64, ptr %lepp
  %lb = add i64 %p, -16
  %lwant = call i64 @npk_m_largew(i64 %lb)
  %lhma = add i64 %lb, 8
  %lhmp = inttoptr i64 %lhma to ptr
  %lhv = load i64, ptr %lhmp
  %lisw = icmp eq i64 %lhv, %lwant
  %linc = select i1 %lisw, i64 1, i64 0
  br label %lnext
lnext:
  %lacc2 = add i64 %lacc, %linc
  %j2 = add i64 %j, 1
  br label %lhead
out:
  ; wildx pages are wild-role too (0.10.5): a live executable page at exit is
  ; a leak the K-semantics rule names, same as any other wild allocation
  %wxp = load i64, ptr @npk_wildx_live
  %total = add i64 %lacc, %wxp
  ret i64 %total
}

; Releases the WHOLE heap -- every chunk and every large mapping, both
; regimes -- and leaves the allocator usable. This is failsafe's controlled
; cleanup: after it, only exit; anything still pointing into the heap points
; at unmapped pages.
define void @npk_wild_release_all() {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %chead
chead:
  %i = phi i64 [ 0, %entry ], [ %i2, %cbody ]
  %more = icmp ult i64 %i, %len
  br i1 %more, label %cbody, label %clists
cbody:
  %eoff = shl i64 %i, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %ch = load i64, ptr %epp
  call void @npk_hunmap(i64 %ch, i64 65536)
  %i2 = add i64 %i, 1
  br label %chead
clists:
  store i64 0, ptr @npk_chtab_len
  br label %zhead
zhead:
  %k = phi i64 [ 0, %clists ], [ %k2, %zbody ]
  %zmore = icmp ult i64 %k, 14
  br i1 %zmore, label %zbody, label %larges
zbody:
  %pp = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %k
  store i64 0, ptr %pp
  %fp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %k
  store i64 0, ptr %fp
  %k2 = add i64 %k, 1
  br label %zhead
larges:
  %ltab = load i64, ptr @npk_lgtab
  %llen = load i64, ptr @npk_lgtab_len
  br label %lhead
lhead:
  %j = phi i64 [ 0, %larges ], [ %j2, %lbody ]
  %lmore = icmp ult i64 %j, %llen
  br i1 %lmore, label %lbody, label %done
lbody:
  %leoff = shl i64 %j, 5
  %lep = add i64 %ltab, %leoff
  %lepp = inttoptr i64 %lep to ptr
  %basea = add i64 %lep, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %lep, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  call void @npk_hunmap(i64 %base, i64 %msz)
  %j2 = add i64 %j, 1
  br label %lhead
done:
  store i64 0, ptr @npk_lgtab_len
  store i64 0, ptr @npk_hs_live
  ret void
}

; ---------------------------------------------------------------------------
; THE ARENA (0.10.2, D-152). arena<T> = { slab ptr, gens ptr, cap, top,
; free_head }, stride-erased: the compiler computes the element stride
; (size rounded to alignment, floored at 8 so a free slot holds the freelist
; link) and passes it to every call. Generations are i32 with a PARITY
; DISCIPLINE: live slots hold EVEN generations, freed slots ODD -- a handle
; only ever carries the even generation `alloc` issued, so a forged or stale
; handle can never name a freed slot: `at` demands exact equality, and the
; freed slot's generation is odd. Reuse bumps odd back to even (+1), so every
; retired handle to the slot mismatches by at least 2. A slot whose
; generation reaches 0xFFFFFFFE is RETIRED rather than reused -- the counter
; never wraps. The slab and generation array are WILD-role heap blocks: an
; un-destroyed arena is a countable leak (D-151), which is the D-003 story
; told mechanically -- drop the arena wholesale, or the exit check names it.
; Single-threaded by contract (D-017); shared_arena is 0.10.4's.
; ---------------------------------------------------------------------------

define { ptr, ptr, i64, i64, i64 } @npk_arena_make(i64 %stride, i64 %cap0) {
entry:
  %neg = icmp slt i64 %cap0, 0
  br i1 %neg, label %badreq, label %floor
badreq:
  call void @npk_heap_badreq()
  unreachable
floor:
  %small = icmp slt i64 %cap0, 8
  %cap = select i1 %small, i64 8, i64 %cap0
  %bo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %cap, i64 %stride)
  %bytes = extractvalue { i64, i1 } %bo, 0
  %ovf = extractvalue { i64, i1 } %bo, 1
  br i1 %ovf, label %badreq, label %mkslab
mkslab:
  ; MANAGED, NOT WILD (D-183, 1.2.5c): the arena binding's scope-exit drop
  ; destroys these now, so they are the managed regime's storage — like a
  ; string body — and the D-151 exit check keeps covering only what stays
  ; manual. They were wild while `destroy` was the only reclaimer.
  %slab = call ptr @npk_alloc_impl(i64 %bytes, i64 0)
  %gb = shl i64 %cap, 2
  %gens = call ptr @npk_alloc_impl(i64 %gb, i64 0)
  call void @llvm.memset.p0.i64(ptr %gens, i8 0, i64 %gb, i1 false)
  %r0 = insertvalue { ptr, ptr, i64, i64, i64 } zeroinitializer, ptr %slab, 0
  %r1 = insertvalue { ptr, ptr, i64, i64, i64 } %r0, ptr %gens, 1
  %r2 = insertvalue { ptr, ptr, i64, i64, i64 } %r1, i64 %cap, 2
  %r3 = insertvalue { ptr, ptr, i64, i64, i64 } %r2, i64 0, 3
  %r4 = insertvalue { ptr, ptr, i64, i64, i64 } %r3, i64 -1, 4
  ret { ptr, ptr, i64, i64, i64 } %r4
}

define { i64, i32 } @npk_arena_alloc(ptr %a, i64 %stride) {
entry:
  %ai = ptrtoint ptr %a to i64
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  %fh = load i64, ptr %fhp
  %none = icmp eq i64 %fh, -1
  br i1 %none, label %bump, label %pop
pop:
  ; the freed slot's first word is the next-free link
  %slaba = inttoptr i64 %ai to ptr
  %slab0 = load ptr, ptr %slaba
  %si = ptrtoint ptr %slab0 to i64
  %soff = mul i64 %fh, %stride
  %sa = add i64 %si, %soff
  %sp = inttoptr i64 %sa to ptr
  %nxt = load i64, ptr %sp
  store i64 %nxt, ptr %fhp
  br label %issue
bump:
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %capa = add i64 %ai, 16
  %capp = inttoptr i64 %capa to ptr
  %cap = load i64, ptr %capp
  %room = icmp ult i64 %top, %cap
  br i1 %room, label %take, label %grow
grow:
  ; double both stores; ralloc validates, preserves the wild role, and frees
  ; the old blocks -- the single-threaded contract is what makes the
  ; relocation safe (D-017)
  %ncap = shl i64 %cap, 1
  %nbo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %ncap, i64 %stride)
  %nbytes = extractvalue { i64, i1 } %nbo, 0
  %novf = extractvalue { i64, i1 } %nbo, 1
  br i1 %novf, label %badreq, label %doslab
badreq:
  call void @npk_heap_badreq()
  unreachable
doslab:
  %slabaa = inttoptr i64 %ai to ptr
  %oslab = load ptr, ptr %slabaa
  %nslab = call ptr @npk_ralloc(ptr %oslab, i64 %nbytes)
  store ptr %nslab, ptr %slabaa
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %ogens = load ptr, ptr %gensp
  %ngb = shl i64 %ncap, 2
  %ngens = call ptr @npk_ralloc(ptr %ogens, i64 %ngb)
  store ptr %ngens, ptr %gensp
  ; the new half of the generation array must start at zero
  %gi = ptrtoint ptr %ngens to i64
  %oldgb = shl i64 %cap, 2
  %za = add i64 %gi, %oldgb
  %zp = inttoptr i64 %za to ptr
  %zn = sub i64 %ngb, %oldgb
  call void @llvm.memset.p0.i64(ptr %zp, i8 0, i64 %zn, i1 false)
  store i64 %ncap, ptr %capp
  br label %take
take:
  %top2 = load i64, ptr %topp
  %ntop = add i64 %top2, 1
  store i64 %ntop, ptr %topp
  br label %issue2
issue2:
  br label %issue
issue:
  %idx = phi i64 [ %fh, %pop ], [ %top2, %issue2 ]
  ; a virgin slot (generation zero) is promoted to 2, so no arena<T> handle
  ; ever carries generation zero -- that value is shared_arena's constant,
  ; and the split is what lets a shared get refuse a wandering arena handle
  ; (D-154). A reused or reset slot holds an ODD generation; bump it live.
  %gensa2 = add i64 %ai, 8
  %gensp2 = inttoptr i64 %gensa2 to ptr
  %gens2 = load ptr, ptr %gensp2
  %gi2 = ptrtoint ptr %gens2 to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi2, %goff
  %gp = inttoptr i64 %ga to ptr
  %g0 = load i32, ptr %gp
  %virgin = icmp eq i32 %g0, 0
  %g = select i1 %virgin, i32 2, i32 %g0
  %odd = and i32 %g, 1
  %isodd = icmp ne i32 %odd, 0
  %g2 = add i32 %g, 1
  %gv = select i1 %isodd, i32 %g2, i32 %g
  store i32 %gv, ptr %gp
  %h0 = insertvalue { i64, i32 } zeroinitializer, i64 %idx, 0
  %h1 = insertvalue { i64, i32 } %h0, i32 %gv, 1
  ret { i64, i32 } %h1
}

define ptr @npk_arena_at(ptr %a, i64 %stride, i64 %idx, i32 %gen) {
entry:
  %ai = ptrtoint ptr %a to i64
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %oob = icmp uge i64 %idx, %top
  br i1 %oob, label %stale, label %genck
genck:
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %match = icmp eq i32 %g, %gen
  br i1 %match, label %live, label %stale
live:
  %slaba = inttoptr i64 %ai to ptr
  %slab = load ptr, ptr %slaba
  %si = ptrtoint ptr %slab to i64
  %soff = mul i64 %idx, %stride
  %sa = add i64 %si, %soff
  %sp = inttoptr i64 %sa to ptr
  ret ptr %sp
stale:
  ret ptr null
}

define i32 @npk_arena_free(ptr %a, i64 %stride, i64 %idx, i32 %gen) {
entry:
  %p = call ptr @npk_arena_at(ptr %a, i64 %stride, i64 %idx, i32 %gen)
  %bad = icmp eq ptr %p, null
  br i1 %bad, label %stale, label %retire
retire:
  %ai = ptrtoint ptr %a to i64
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  ; retire at the cap: a slot at 0xFFFFFFFE never re-enters the freelist
  %cap = icmp ult i32 %g2, -2
  br i1 %cap, label %push, label %done
push:
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  %fh = load i64, ptr %fhp
  %sp2 = ptrtoint ptr %p to i64
  %spp = inttoptr i64 %sp2 to ptr
  store i64 %fh, ptr %spp
  store i64 %idx, ptr %fhp
  br label %done
done:
  ret i32 0
stale:
  ret i32 1
}

define void @npk_arena_reset(ptr %a, i64 %stride) {
entry:
  %ai = ptrtoint ptr %a to i64
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %next ]
  %more = icmp ult i64 %i, %top
  br i1 %more, label %body, label %clear
body:
  %goff = shl i64 %i, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %odd = and i32 %g, 1
  %isodd = icmp ne i32 %odd, 0
  br i1 %isodd, label %next, label %bump
bump:
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  br label %next
next:
  %i2 = add i64 %i, 1
  br label %head
clear:
  store i64 0, ptr %topp
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  store i64 -1, ptr %fhp
  ret void
}

; ---------------------------------------------------------------------------
; THE wildx W^X STATE MACHINE (0.10.5, D-035/D-155). Executable memory for
; the JIT, contained so it cannot corrupt host memory safety -- the guarantee
; D-035 says wildx actually delivers (the CONTENTS are unverifiable, outside
; Z3 and every static analysis like the FFI barrier; the CONTAINER is not).
;
; THE INVARIANT IS STRUCTURAL, not checked: a page is RW at alloc and RX after
; seal, never W+X at once. seal is a ONE-WAY mprotect; the analysis
; (bindings.npk) refuses any write after it and any execute before it, so the
; transition cannot run backwards in a valid program.
;
; A wildx region is three pages: [ PROT_NONE guard | RW/RX code | PROT_NONE
; guard ]. The guards turn an over/underrun into a fault instead of silent
; corruption of a neighbour. ASLR is the kernel's own mmap randomisation
; (addr NULL) -- we do not place the page, the kernel does. A 16-byte header
; in the code page holds the mapping size and a secret-keyed magic, so free
; validates the pointer is one we handed out. Pages are WILD-role: the count
; below feeds wild_live_count, so an unfreed page traps at exit (D-151).
;
; header (in the code page, 16 bytes before the returned pointer):
;   +0 mapsize (the whole three-page span)  +8 magic
; ---------------------------------------------------------------------------

@npk_wildx_live = internal global i64 0

define internal i64 @npk_m_wildx(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 4358112156723783278
  ret i64 %m
}

define ptr @npk_wildx_alloc(i64 %size) {
entry:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %init, label %sized
init:
  call void @npk_heap_init()
  br label %sized
sized:
  %neg = icmp slt i64 %size, 0
  br i1 %neg, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  ; header + payload rounded to a page, plus a guard page each side
  %need = add i64 %size, 16
  %n4095 = add i64 %need, 4095
  %codep = and i64 %n4095, -4096
  %map = add i64 %codep, 8192
  ; PROT_NONE (0) so the whole span starts unreachable; the middle is opened RW
  %base = call i64 @npk_sys6(i64 9, i64 0, i64 %map, i64 0, i64 34, i64 -1, i64 0)
  %bad = icmp ugt i64 %base, -4096
  br i1 %bad, label %oom, label %open
oom:
  call void @npk_heap_oom()
  unreachable
open:
  %code = add i64 %base, 4096
  ; mprotect(code, codep, PROT_READ|PROT_WRITE = 3)
  %r = call i64 @npk_sys6(i64 10, i64 %code, i64 %codep, i64 3, i64 0, i64 0, i64 0)
  %rbad = icmp ne i64 %r, 0
  br i1 %rbad, label %pfail, label %stamp
pfail:
  call void @npk_heap_bad()
  unreachable
stamp:
  %szp = inttoptr i64 %code to ptr
  store i64 %map, ptr %szp
  %ma = add i64 %code, 8
  %mp = inttoptr i64 %ma to ptr
  %magic = call i64 @npk_m_wildx(i64 %code)
  store i64 %magic, ptr %mp
  %live = load i64, ptr @npk_wildx_live
  %live2 = add i64 %live, 1
  store i64 %live2, ptr @npk_wildx_live
  %u = add i64 %code, 16
  %up = inttoptr i64 %u to ptr
  ret ptr %up
}

; validate a wildx pointer against its header; returns the code-page base
define internal i64 @npk_wildx_check(i64 %up) {
entry:
  %isz = icmp eq i64 %up, 0
  br i1 %isz, label %bad, label %aligned
bad:
  call void @npk_heap_bad()
  unreachable
aligned:
  %code = add i64 %up, -16
  %ma = add i64 %code, 8
  %mp = inttoptr i64 %ma to ptr
  %have = load i64, ptr %mp
  %want = call i64 @npk_m_wildx(i64 %code)
  %ok = icmp eq i64 %have, %want
  br i1 %ok, label %good, label %bad
good:
  ret i64 %code
}

define void @npk_wildx_seal(ptr %p) {
entry:
  %up = ptrtoint ptr %p to i64
  %code = call i64 @npk_wildx_check(i64 %up)
  %szp = inttoptr i64 %code to ptr
  %map = load i64, ptr %szp
  %codep = sub i64 %map, 8192
  ; the ONE-WAY transition: mprotect(code, codep, PROT_READ|PROT_EXEC = 5).
  ; The page is never PROT_WRITE|PROT_EXEC -- W^X holds structurally.
  %r = call i64 @npk_sys6(i64 10, i64 %code, i64 %codep, i64 5, i64 0, i64 0, i64 0)
  %bad = icmp ne i64 %r, 0
  br i1 %bad, label %fail, label %done
fail:
  call void @npk_heap_bad()
  unreachable
done:
  ret void
}

define i64 @npk_wildx_call(ptr %p, i64 %arg) {
entry:
  ; the analysis has proven the page is sealed (RX) before this runs; the
  ; contents are outside verification by construction (D-035), so this is the
  ; boundary -- an ordinary indirect call into code we do not model
  %r = call i64 %p(i64 %arg)
  ret i64 %r
}

define void @npk_wildx_free(ptr %p) {
entry:
  %up = ptrtoint ptr %p to i64
  %code = call i64 @npk_wildx_check(i64 %up)
  %szp = inttoptr i64 %code to ptr
  %map = load i64, ptr %szp
  %base = sub i64 %code, 4096
  call void @npk_hunmap(i64 %base, i64 %map)
  %live = load i64, ptr @npk_wildx_live
  %live2 = sub i64 %live, 1
  store i64 %live2, ptr @npk_wildx_live
  ret void
}

; ---------------------------------------------------------------------------
; THE SHARED ARENA (0.10.4, D-154). The concurrent arena with the SMALLER
; contract (D-017): alloc, get, destroy -- no free, no reset, no put. Slots
; are written once, by alloc, before the handle escapes, and never reused or
; rewritten while the arena is live: that is the entire concurrency story --
; no generation traffic, no freelist, no epochs, no hazard pointers.
;
; STORAGE NEVER MOVES. Chunks tile the index space [0, cap): growth RESERVES
; a capacity range first (one atomic fetch_add on cap -- racing installers
; get DISJOINT ranges and can never collide), builds the chunk against that
; base, and publishes it with a CAS push onto the chunk list. A bumped index
; inside a reserved-but-unlinked range spins in the slot walk until its
; installer links -- bounded by the installer's own progress; 1.1's
; concurrency review owns preemption liveness. All cross-thread state is
; SeqCst (D-016). Geometric chunk sizes: each new chunk carries the arena's
; current capacity in slots, capped at 65536 -- a large arena is a few
; chunks, never thousands.
;
; The handle's generation field is CONSTANT ZERO here (nothing frees, so
; nothing increments) -- checked compiler-side, which also refuses an
; arena<T> handle wandering over: those carry even generations >= 2.
;
; struct SharedArena (48 bytes, a wild heap block; the surface value is a
; POINTER to it -- shareable by reference is the contract):
;   +0  chunk_head  atomic: newest chunk (CAS push target)
;   +8  top         atomic: next slot index (fetch_add)
;   +16 cap         atomic: reserved capacity (fetch_add on growth)
;   +24 stride      element stride, fixed at make
;   +32,+40 reserved
; chunk: [ next i64 | base i64 | nslots i64 | pad i64 ] then slots.
; ---------------------------------------------------------------------------

define ptr @npk_sarena_make(i64 %stride, i64 %cap0) {
entry:
  %neg = icmp slt i64 %cap0, 0
  br i1 %neg, label %badreq, label %floor
badreq:
  call void @npk_heap_badreq()
  unreachable
floor:
  %small = icmp slt i64 %cap0, 64
  %first = select i1 %small, i64 64, i64 %cap0
  %bo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %first, i64 %stride)
  %bytes0 = extractvalue { i64, i1 } %bo, 0
  %ovf = extractvalue { i64, i1 } %bo, 1
  br i1 %ovf, label %badreq, label %mk
mk:
  %bytes = add i64 %bytes0, 32
  %ch = call ptr @npk_alloc_impl(i64 %bytes, i64 1)
  %ci = ptrtoint ptr %ch to i64
  %np = inttoptr i64 %ci to ptr
  store i64 0, ptr %np
  %ba = add i64 %ci, 8
  %bp = inttoptr i64 %ba to ptr
  store i64 0, ptr %bp
  %na = add i64 %ci, 16
  %nsp = inttoptr i64 %na to ptr
  store i64 %first, ptr %nsp
  %sa = call ptr @npk_alloc_impl(i64 48, i64 1)
  %si = ptrtoint ptr %sa to i64
  %hp = inttoptr i64 %si to ptr
  store atomic i64 %ci, ptr %hp seq_cst, align 8
  %ta = add i64 %si, 8
  %tp = inttoptr i64 %ta to ptr
  store atomic i64 0, ptr %tp seq_cst, align 8
  %ca = add i64 %si, 16
  %cp = inttoptr i64 %ca to ptr
  store atomic i64 %first, ptr %cp seq_cst, align 8
  %sta = add i64 %si, 24
  %stp = inttoptr i64 %sta to ptr
  store i64 %stride, ptr %stp
  ret ptr %sa
}

define i64 @npk_sarena_bump(ptr %sa, i64 %stride) {
entry:
  %si = ptrtoint ptr %sa to i64
  %ta = add i64 %si, 8
  %tp = inttoptr i64 %ta to ptr
  %idx = atomicrmw add ptr %tp, i64 1 seq_cst
  %ca = add i64 %si, 16
  %cp = inttoptr i64 %ca to ptr
  br label %check
check:
  %cap = load atomic i64, ptr %cp seq_cst, align 8
  %fits = icmp ult i64 %idx, %cap
  br i1 %fits, label %done, label %grow
grow:
  ; geometric: the new chunk carries the current capacity, capped at 65536
  %big = icmp ugt i64 %cap, 65536
  %newn = select i1 %big, i64 65536, i64 %cap
  %bo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %newn, i64 %stride)
  %bytes0 = extractvalue { i64, i1 } %bo, 0
  %ovf = extractvalue { i64, i1 } %bo, 1
  br i1 %ovf, label %badreq, label %reserve
badreq:
  call void @npk_heap_badreq()
  unreachable
reserve:
  ; the reservation IS the race arbiter: disjoint ranges by construction
  %base = atomicrmw add ptr %cp, i64 %newn seq_cst
  %bytes = add i64 %bytes0, 32
  %ch = call ptr @npk_alloc_impl(i64 %bytes, i64 1)
  %ci = ptrtoint ptr %ch to i64
  %ba = add i64 %ci, 8
  %bp = inttoptr i64 %ba to ptr
  store i64 %base, ptr %bp
  %na = add i64 %ci, 16
  %nsp = inttoptr i64 %na to ptr
  store i64 %newn, ptr %nsp
  %hp = inttoptr i64 %si to ptr
  br label %push
push:
  %old = load atomic i64, ptr %hp seq_cst, align 8
  %np = inttoptr i64 %ci to ptr
  store i64 %old, ptr %np
  %pair = cmpxchg ptr %hp, i64 %old, i64 %ci seq_cst seq_cst
  %okc = extractvalue { i64, i1 } %pair, 1
  br i1 %okc, label %check, label %push
done:
  ret i64 %idx
}

define ptr @npk_sarena_slot(ptr %sa, i64 %stride, i64 %idx) {
entry:
  %si = ptrtoint ptr %sa to i64
  %ta = add i64 %si, 8
  %tp = inttoptr i64 %ta to ptr
  %top = load atomic i64, ptr %tp seq_cst, align 8
  %oob = icmp uge i64 %idx, %top
  br i1 %oob, label %stale, label %walk
stale:
  ret ptr null
walk:
  %hp = inttoptr i64 %si to ptr
  %head = load atomic i64, ptr %hp seq_cst, align 8
  br label %chead
chead:
  %c = phi i64 [ %head, %walk ], [ %nxt, %cnext ]
  %end = icmp eq i64 %c, 0
  ; a bumped index whose chunk is not yet linked spins on the walk; the
  ; installer's own progress bounds the wait
  br i1 %end, label %walk, label %cbody
cbody:
  %ba = add i64 %c, 8
  %bp = inttoptr i64 %ba to ptr
  %base = load i64, ptr %bp
  %na = add i64 %c, 16
  %nsp = inttoptr i64 %na to ptr
  %n = load i64, ptr %nsp
  %lo = icmp uge i64 %idx, %base
  %top2 = add i64 %base, %n
  %hi = icmp ult i64 %idx, %top2
  %in = and i1 %lo, %hi
  br i1 %in, label %found, label %cnext
cnext:
  %np2 = inttoptr i64 %c to ptr
  %nxt = load i64, ptr %np2
  br label %chead
found:
  %rel = sub i64 %idx, %base
  %off = mul i64 %rel, %stride
  %data = add i64 %c, 32
  %addr = add i64 %data, %off
  %p = inttoptr i64 %addr to ptr
  ret ptr %p
}

define void @npk_sarena_destroy(ptr %sa) {
entry:
  %si = ptrtoint ptr %sa to i64
  %hp = inttoptr i64 %si to ptr
  %head = load atomic i64, ptr %hp seq_cst, align 8
  br label %walk
walk:
  %c = phi i64 [ %head, %entry ], [ %nxt, %freec ]
  %done = icmp eq i64 %c, 0
  br i1 %done, label %self, label %freec
freec:
  %np = inttoptr i64 %c to ptr
  %nxt = load i64, ptr %np
  %cp = inttoptr i64 %c to ptr
  call void @npk_dalloc(ptr %cp)
  br label %walk
self:
  call void @npk_dalloc(ptr %sa)
  ret void
}

; ---------------------------------------------------------------------------
; THE EXECUTOR FRAME ALLOCATOR (0.10.3, D-153). NOT arena<T>, deliberately:
; the surface arena is a fixed-slot allocator handing out generation-checked
; INDICES, and a coroutine frame is a per-function, variably-sized block that
; @llvm.coro.begin needs as a RAW POINTER. Conflating them is the mistake the
; concurrency audit caught (total_audit B-1); this family is the allocator
; D-034 actually means, built where the heap lives and consumed by 1.1's
; coroutine lowering, which is its ONLY intended caller -- no surface type,
; no keyword, no builtin.
;
; SHAPE. One allocator per executor; tasks are pinned (D-032), so the whole
; path is single-threaded and ZERO-ATOMIC -- that is D-034's rationale for
; pinning. Chunks of 64 KiB bump-allocate frames; a completed task's frame
; returns to a free list BUCKETED BY EXACT SIZE, which fits coroutines
; precisely: a program has one frame size per async function, recurring many
; times. Frames larger than a chunk take a dedicated heap block (flag bit in
; the header). `drain` retires every frame at once by resetting the bump and
; the buckets, KEEPING the chunks -- the executor's steady state allocates
; from memory it already owns. Frame headers carry the size and a
; secret-keyed state magic: freeing a frame twice, or freeing a pointer that
; is not a live frame, traps -4102 like every other heap-integrity failure.
;
; The executor struct and its chunks are WILD-role heap blocks: an
; un-destroyed executor is a countable leak the D-151 exit check names.
;
; struct FrameExec (64 bytes, a wild heap block):
;   +0  chunk_head   first chunk (chunk: [ next i64 | pad | frames... ])
;   +8  cur_chunk    the chunk being bumped
;   +16 cur_off      bump offset within cur_chunk (starts at 16)
;   +24 bsizes       ptr to i64[cap]  bucket sizes (exact rounded frame size)
;   +32 bheads       ptr to i64[cap]  bucket free-list heads (frame addrs)
;   +40 bcount
;   +48 bcap
;   +56 reserved
; frame block: [ size i64 | state i64 ] then payload, 16-aligned; state is
; secret^addr^K_LIVE or ^K_FREE, with bit 0 of the SIZE word marking a
; dedicated (oversize) block.
; ---------------------------------------------------------------------------

define internal i64 @npk_m_flive(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -602391508442037685
  ret i64 %m
}

define internal i64 @npk_m_ffree(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 999621991244018563
  ret i64 %m
}

define ptr @npk_frame_exec_new() {
entry:
  %fe = call ptr @npk_alloc_impl(i64 64, i64 1)
  %fi = ptrtoint ptr %fe to i64
  ; one 64 KiB chunk up front; the steady state never maps again
  %ch = call ptr @npk_alloc_impl(i64 65536, i64 1)
  %ci = ptrtoint ptr %ch to i64
  %np = inttoptr i64 %ci to ptr
  store i64 0, ptr %np
  %hp = inttoptr i64 %fi to ptr
  store i64 %ci, ptr %hp
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %ci, ptr %ccp
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  store i64 16, ptr %cop
  ; eight buckets to start; ralloc grows the pair in step
  %bs = call ptr @npk_alloc_impl(i64 64, i64 1)
  %bh = call ptr @npk_alloc_impl(i64 64, i64 1)
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  store ptr %bs, ptr %bsp
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  store ptr %bh, ptr %bhp
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  store i64 0, ptr %bcp
  %bpa = add i64 %fi, 48
  %bpp = inttoptr i64 %bpa to ptr
  store i64 8, ptr %bpp
  ret ptr %fe
}

; find the bucket for an exact rounded size; -1 when absent
define internal i64 @npk_frame_bucket(i64 %fi, i64 %rn) {
entry:
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  %bc = load i64, ptr %bcp
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %bs = load ptr, ptr %bsp
  %bi = ptrtoint ptr %bs to i64
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %next ]
  %more = icmp ult i64 %i, %bc
  br i1 %more, label %look, label %miss
look:
  %ea = shl i64 %i, 3
  %ep = add i64 %bi, %ea
  %epp = inttoptr i64 %ep to ptr
  %v = load i64, ptr %epp
  %hit = icmp eq i64 %v, %rn
  br i1 %hit, label %found, label %next
next:
  %i2 = add i64 %i, 1
  br label %head
found:
  ret i64 %i
miss:
  ret i64 -1
}

define ptr @npk_frame_alloc(ptr %fe, i64 %size, i64 %align) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %negsz = icmp slt i64 %size, 0
  br i1 %negsz, label %badreq, label %alck
alck:
  ; coroutine frames align at or below the heap's own sixteen
  %zeroa = icmp sle i64 %align, 0
  %bigal = icmp sgt i64 %align, 16
  %bad = or i1 %zeroa, %bigal
  br i1 %bad, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  %z = icmp eq i64 %size, 0
  %n1 = select i1 %z, i64 16, i64 %size
  %r15 = add i64 %n1, 15
  %rn = and i64 %r15, -16
  ; exact-size bucket first: the common steady state
  %bkt = call i64 @npk_frame_bucket(i64 %fi, i64 %rn)
  %none = icmp slt i64 %bkt, 0
  br i1 %none, label %fresh, label %check
check:
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %bh = load ptr, ptr %bhp
  %hi = ptrtoint ptr %bh to i64
  %ha = shl i64 %bkt, 3
  %hp = add i64 %hi, %ha
  %hpp = inttoptr i64 %hp to ptr
  %head = load i64, ptr %hpp
  %empty = icmp eq i64 %head, 0
  br i1 %empty, label %fresh, label %pop
pop:
  ; the freed frame's payload first word is the next link
  %pa = add i64 %head, 16
  %pp = inttoptr i64 %pa to ptr
  %nxt = load i64, ptr %pp
  store i64 %nxt, ptr %hpp
  ; verify the freed magic, then stamp live
  %sma = add i64 %head, 8
  %smp = inttoptr i64 %sma to ptr
  %sv = load i64, ptr %smp
  %want = call i64 @npk_m_ffree(i64 %head)
  %ok = icmp eq i64 %sv, %want
  br i1 %ok, label %stamp, label %corrupt
corrupt:
  call void @npk_heap_bad()
  unreachable
stamp:
  %lm = call i64 @npk_m_flive(i64 %head)
  store i64 %lm, ptr %smp
  %u = add i64 %head, 16
  %uptr = inttoptr i64 %u to ptr
  ret ptr %uptr
fresh:
  ; oversize takes a dedicated heap block, flagged in the size word
  %need = add i64 %rn, 16
  %big = icmp ugt i64 %need, 65520
  br i1 %big, label %dedicated, label %bump
dedicated:
  %db = call ptr @npk_alloc_impl(i64 %need, i64 1)
  %di = ptrtoint ptr %db to i64
  %dsz = or i64 %rn, 1
  %dsp = inttoptr i64 %di to ptr
  store i64 %dsz, ptr %dsp
  %dma = add i64 %di, 8
  %dmp = inttoptr i64 %dma to ptr
  %dm = call i64 @npk_m_flive(i64 %di)
  store i64 %dm, ptr %dmp
  %du = add i64 %di, 16
  %dup = inttoptr i64 %du to ptr
  ret ptr %dup
bump:
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  %off = load i64, ptr %cop
  %end = add i64 %off, %need
  %fits = icmp ule i64 %end, 65536
  br i1 %fits, label %take, label %newchunk
newchunk:
  %nc = call ptr @npk_alloc_impl(i64 65536, i64 1)
  %nci = ptrtoint ptr %nc to i64
  ; push onto the chunk list, make it current
  %hpx = inttoptr i64 %fi to ptr
  %old = load i64, ptr %hpx
  %ncp = inttoptr i64 %nci to ptr
  store i64 %old, ptr %ncp
  store i64 %nci, ptr %hpx
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %nci, ptr %ccp
  store i64 16, ptr %cop
  br label %take
take:
  %cca2 = add i64 %fi, 8
  %ccp2 = inttoptr i64 %cca2 to ptr
  %cur = load i64, ptr %ccp2
  %off2 = load i64, ptr %cop
  %b = add i64 %cur, %off2
  %end2 = add i64 %off2, %need
  store i64 %end2, ptr %cop
  %bsp2 = inttoptr i64 %b to ptr
  store i64 %rn, ptr %bsp2
  %bma = add i64 %b, 8
  %bmp = inttoptr i64 %bma to ptr
  %bm = call i64 @npk_m_flive(i64 %b)
  store i64 %bm, ptr %bmp
  %bu = add i64 %b, 16
  %bup = inttoptr i64 %bu to ptr
  ret ptr %bup
}

define void @npk_frame_free(ptr %fe, ptr %frame) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %pi = ptrtoint ptr %frame to i64
  %isz = icmp eq i64 %pi, 0
  br i1 %isz, label %bad, label %aligned
bad:
  call void @npk_heap_bad()
  unreachable
aligned:
  %mis = and i64 %pi, 15
  %crooked = icmp ne i64 %mis, 0
  br i1 %crooked, label %bad, label %hdr
hdr:
  %b = add i64 %pi, -16
  %sma = add i64 %b, 8
  %smp = inttoptr i64 %sma to ptr
  %sv = load i64, ptr %smp
  %want = call i64 @npk_m_flive(i64 %b)
  %ok = icmp eq i64 %sv, %want
  br i1 %ok, label %route, label %bad
route:
  %szp = inttoptr i64 %b to ptr
  %szw = load i64, ptr %szp
  %ded = and i64 %szw, 1
  %isded = icmp ne i64 %ded, 0
  br i1 %isded, label %dedic, label %bucket
dedic:
  ; a dedicated block goes back to the heap whole
  %dbp = inttoptr i64 %b to ptr
  call void @npk_dalloc(ptr %dbp)
  ret void
bucket:
  %fm = call i64 @npk_m_ffree(i64 %b)
  store i64 %fm, ptr %smp
  %bkt = call i64 @npk_frame_bucket(i64 %fi, i64 %szw)
  %none = icmp slt i64 %bkt, 0
  br i1 %none, label %newbucket, label %push
newbucket:
  ; grow the parallel arrays when full, then append the size
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  %bc = load i64, ptr %bcp
  %bpa = add i64 %fi, 48
  %bpp = inttoptr i64 %bpa to ptr
  %bcap = load i64, ptr %bpp
  %full = icmp eq i64 %bc, %bcap
  br i1 %full, label %grow, label %append
grow:
  %ncap = shl i64 %bcap, 1
  %nbytes = shl i64 %ncap, 3
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %obs = load ptr, ptr %bsp
  %nbs = call ptr @npk_ralloc(ptr %obs, i64 %nbytes)
  store ptr %nbs, ptr %bsp
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %obh = load ptr, ptr %bhp
  %nbh = call ptr @npk_ralloc(ptr %obh, i64 %nbytes)
  store ptr %nbh, ptr %bhp
  store i64 %ncap, ptr %bpp
  br label %append
append:
  %bc2 = load i64, ptr %bcp
  %bsa2 = add i64 %fi, 24
  %bsp2 = inttoptr i64 %bsa2 to ptr
  %bs2 = load ptr, ptr %bsp2
  %si = ptrtoint ptr %bs2 to i64
  %sa = shl i64 %bc2, 3
  %sp = add i64 %si, %sa
  %spp = inttoptr i64 %sp to ptr
  store i64 %szw, ptr %spp
  %bha2 = add i64 %fi, 32
  %bhp2 = inttoptr i64 %bha2 to ptr
  %bh2 = load ptr, ptr %bhp2
  %hi2 = ptrtoint ptr %bh2 to i64
  %ha2 = add i64 %hi2, %sa
  %hpp2 = inttoptr i64 %ha2 to ptr
  store i64 0, ptr %hpp2
  %bc3 = add i64 %bc2, 1
  store i64 %bc3, ptr %bcp
  %bkt2 = add i64 %bc2, 0
  br label %push2
push:
  br label %push2
push2:
  %slot = phi i64 [ %bkt, %push ], [ %bkt2, %append ]
  %bha3 = add i64 %fi, 32
  %bhp3 = inttoptr i64 %bha3 to ptr
  %bh3 = load ptr, ptr %bhp3
  %hi3 = ptrtoint ptr %bh3 to i64
  %ha3 = shl i64 %slot, 3
  %hp3 = add i64 %hi3, %ha3
  %hpp3 = inttoptr i64 %hp3 to ptr
  %old = load i64, ptr %hpp3
  %la = add i64 %b, 16
  %lp = inttoptr i64 %la to ptr
  store i64 %old, ptr %lp
  store i64 %b, ptr %hpp3
  ret void
}

define void @npk_frame_drain(ptr %fe) {
entry:
  ; every frame dies at once: reset the bump to the FIRST chunk and empty
  ; the buckets; the chunks stay -- the steady state owns its memory
  %fi = ptrtoint ptr %fe to i64
  %hp = inttoptr i64 %fi to ptr
  %head = load i64, ptr %hp
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %head, ptr %ccp
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  store i64 16, ptr %cop
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  store i64 0, ptr %bcp
  ret void
}

define void @npk_frame_exec_destroy(ptr %fe) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %hp = inttoptr i64 %fi to ptr
  %head = load i64, ptr %hp
  br label %walk
walk:
  %c = phi i64 [ %head, %entry ], [ %nxt, %freec ]
  %done = icmp eq i64 %c, 0
  br i1 %done, label %arrays, label %freec
freec:
  %np = inttoptr i64 %c to ptr
  %nxt = load i64, ptr %np
  %cp = inttoptr i64 %c to ptr
  call void @npk_dalloc(ptr %cp)
  br label %walk
arrays:
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %bs = load ptr, ptr %bsp
  call void @npk_dalloc(ptr %bs)
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %bh = load ptr, ptr %bhp
  call void @npk_dalloc(ptr %bh)
  call void @npk_dalloc(ptr %fe)
  ret void
}

define void @npk_arena_destroy(ptr %a) {
entry:
  %ai = ptrtoint ptr %a to i64
  %slaba = inttoptr i64 %ai to ptr
  %slab = load ptr, ptr %slaba
  %has = icmp ne ptr %slab, null
  br i1 %has, label %freeslab, label %gens
freeslab:
  call void @npk_dalloc(ptr %slab)
  br label %gens
gens:
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %garr = load ptr, ptr %gensp
  %hasg = icmp ne ptr %garr, null
  br i1 %hasg, label %freegens, label %zero
freegens:
  call void @npk_dalloc(ptr %garr)
  br label %zero
zero:
  store ptr null, ptr %slaba
  store ptr null, ptr %gensp
  %capa = add i64 %ai, 16
  %capp = inttoptr i64 %capa to ptr
  store i64 0, ptr %capp
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  store i64 0, ptr %topp
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  store i64 -1, ptr %fhp
  ret void
}

; ---------------------------------------------------------------------------
; The symbols LLVM emits calls to. With no libc these must exist.
; ---------------------------------------------------------------------------

define ptr @memcpy(ptr %dst, ptr %src, i64 %n) {
entry:
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %sp = getelementptr i8, ptr %src, i64 %i
  %dp = getelementptr i8, ptr %dst, i64 %i
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memset(ptr %dst, i32 %c, i64 %n) {
entry:
  %b = trunc i32 %c to i8
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %dp = getelementptr i8, ptr %dst, i64 %i
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memmove(ptr %dst, ptr %src, i64 %n) {
entry:
  ; A real move, not a memcpy alias (0.10.0): when dst overlaps src from
  ; above, a forward copy reads bytes it has already overwritten.
  %di = ptrtoint ptr %dst to i64
  %si = ptrtoint ptr %src to i64
  %down = icmp ult i64 %di, %si
  br i1 %down, label %fwd, label %bwd
fwd:
  %r = call ptr @memcpy(ptr %dst, ptr %src, i64 %n)
  ret ptr %r
bwd:
  br label %bhead
bhead:
  %i = phi i64 [ %n, %bwd ], [ %i2, %bbody ]
  %more = icmp ugt i64 %i, 0
  br i1 %more, label %bbody, label %bdone
bbody:
  %i2 = add i64 %i, -1
  %sp = getelementptr i8, ptr %src, i64 %i2
  %dp = getelementptr i8, ptr %dst, i64 %i2
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  br label %bhead
bdone:
  ret ptr %dst
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare i64 @llvm.cttz.i64(i64, i1)
declare { i64, i1 } @llvm.umul.with.overflow.i64(i64, i64)

; ---------------------------------------------------------------------------
; Strings.
;
; string is {ptr, i64 len, i64 cap} and these return Result<string>, which is
; { {ptr,i64,i64}, i32 } -- the error field is tbb32, zero meaning success
; (D-069). The seed's checker types them exactly this way; the two must agree or
; llc rejects the caller, which is how the mismatch was caught the first time.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_string_concat({ ptr, i64, i64 } %a,
                                                     { ptr, i64, i64 } %b) {
entry:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  ; QUARANTINE TRIPWIRE (debug): a source beginning with two poison bytes is a
  ; freed body -- trap at the READ so the backtrace names the reader.
  %qtw = load i64, ptr @npk_quarantine
  %qtwon = icmp ne i64 %qtw, 0
  br i1 %qtwon, label %qchk_a, label %qok
qchk_a:
  %qa2 = icmp sgt i64 %al, 1
  br i1 %qa2, label %qld_a, label %qchk_b
qld_a:
  %qa0 = load i8, ptr %ap
  %qa1p = getelementptr i8, ptr %ap, i64 1
  %qa1 = load i8, ptr %qa1p
  %qab = icmp eq i8 %qa0, -86
  %qab1 = icmp eq i8 %qa1, -86
  %qhit_a = and i1 %qab, %qab1
  br i1 %qhit_a, label %qtrap, label %qchk_b
qchk_b:
  %qb2 = icmp sgt i64 %bl, 1
  br i1 %qb2, label %qld_b, label %qok
qld_b:
  %qb0 = load i8, ptr %bp
  %qb1p = getelementptr i8, ptr %bp, i64 1
  %qb1 = load i8, ptr %qb1p
  %qbb = icmp eq i8 %qb0, -86
  %qbb1 = icmp eq i8 %qb1, -86
  %qhit_b = and i1 %qbb, %qbb1
  br i1 %qhit_b, label %qtrap, label %qok
qtrap:
  call void @npk_heap_bad()
  unreachable
qok:
  %n = add i64 %al, %bl
  %p = call ptr @npk_alloc_internal(i64 %n)
  call ptr @memcpy(ptr %p, ptr %ap, i64 %al)
  %tail = getelementptr i8, ptr %p, i64 %al
  call ptr @memcpy(ptr %tail, ptr %bp, i64 %bl)
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_int_to_string(i64 %v) {
entry:
  ; Fill a 24-byte buffer from the END, then return a view of the filled tail.
  ; No reversal step, and 24 bytes is enough for -9223372036854775808.
  %buf = call ptr @npk_alloc_internal(i64 24)
  %isneg = icmp slt i64 %v, 0
  %neg = sub i64 0, %v
  %u0 = select i1 %isneg, i64 %neg, i64 %v
  %zero = icmp eq i64 %u0, 0
  br i1 %zero, label %just_zero, label %digits

just_zero:
  %z = getelementptr i8, ptr %buf, i64 23
  store i8 48, ptr %z
  br label %sign

digits:
  %i = phi i64 [ 24, %entry ], [ %i2, %digits ]
  %u = phi i64 [ %u0, %entry ], [ %u2, %digits ]
  %i2 = sub i64 %i, 1
  %q = udiv i64 %u, 10
  %m = urem i64 %u, 10
  %d = trunc i64 %m to i8
  %ch = add i8 %d, 48
  %dp = getelementptr i8, ptr %buf, i64 %i2
  store i8 %ch, ptr %dp
  %u2 = udiv i64 %u, 10
  %more = icmp ugt i64 %q, 0
  br i1 %more, label %digits, label %after

after:
  br label %sign

sign:
  %start0 = phi i64 [ 23, %just_zero ], [ %i2, %after ]
  br i1 %isneg, label %put_sign, label %build

put_sign:
  %sp = sub i64 %start0, 1
  %spp = getelementptr i8, ptr %buf, i64 %sp
  store i8 45, ptr %spp
  br label %build

build:
  %start = phi i64 [ %start0, %sign ], [ %sp, %put_sign ]
  %len = sub i64 24, %start
  ; THE HEADER POINTS AT THE ALLOCATION'S BASE (D-183). The digits were built
  ; from the buffer's end, and the header used to carry `buf+start` with
  ; `cap = len` — an INTERIOR pointer claiming ownership. The drop handed that
  ; to `dalloc`, which correctly refused an address it never issued (-4102).
  ; A short forward copy re-homes the digits at the base (dst below src, so
  ; the overlap is safe), and the capacity is the allocation's true 24.
  br label %rehome
rehome:
  %ri = phi i64 [ 0, %build ], [ %rin, %rehome_step ]
  %rdone = icmp uge i64 %ri, %len
  br i1 %rdone, label %homed, label %rehome_step
rehome_step:
  %rsi = add i64 %start, %ri
  %rsp2 = getelementptr i8, ptr %buf, i64 %rsi
  %rb = load i8, ptr %rsp2
  %rdp = getelementptr i8, ptr %buf, i64 %ri
  store i8 %rb, ptr %rdp
  %rin = add i64 %ri, 1
  br label %rehome
homed:
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %buf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 24, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_string_slice({ ptr, i64, i64 } %s,
                                                    i64 %start, i64 %end) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %l = extractvalue { ptr, i64, i64 } %s, 1
  ; Bounds are CHECKED, not assumed. A slice is a view, and a view past the end
  ; of its backing store is the defect the whole type exists to prevent (D-070).
  %b1 = icmp ult i64 %end, %start
  %b2 = icmp ugt i64 %end, %l
  %bad = or i1 %b1, %b2
  br i1 %bad, label %err, label %ok

err:
  %e0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer,
                    { ptr, i64, i64 } zeroinitializer, 0
  %e1 = insertvalue { { ptr, i64, i64 }, i32 } %e0, i32 -34, 1
  ret { { ptr, i64, i64 }, i32 } %e1

ok:
  %np = getelementptr i8, ptr %p, i64 %start
  %n = sub i64 %end, %start
  ; AN OWNED COPY, NOT A VIEW (D-186, user-settled after 1.1.12b's find).
  ; The view this returned shared its body with the source, and `x =
  ; string_slice(x, lo, hi)` — three ordinary tokens — freed that body out
  ; from under the result: a silent use-after-free the type system cannot
  ; see, because view and owner share the type `string` and the ownership
  ; bit is runtime state. The copy costs one allocation and deletes the
  ; whole class. `string_from_bytes` below stays the explicit view
  ; primitive over a buffer the CALLER owns — a different contract, stated
  ; there. An empty slice allocates nothing: len 0 is never dereferenced,
  ; and cap 0 gives the drop nothing to free.
  %none = icmp eq i64 %n, 0
  br i1 %none, label %empty, label %copy
empty:
  %ez0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %np, 0
  %ez1 = insertvalue { ptr, i64, i64 } %ez0, i64 0, 1
  %ez2 = insertvalue { ptr, i64, i64 } %ez1, i64 0, 2
  %ezr0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %ez2, 0
  %ezr1 = insertvalue { { ptr, i64, i64 }, i32 } %ezr0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %ezr1
copy:
  ; THE INTERNAL ENTRY, like every string body (concat's precedent): a
  ; string is managed-regime storage whose drop frees it — it must NOT
  ; enter <wild-live>. The first cut said @npk_alloc, and a SLICED string
  ; alive at `exit 0` — where drops deliberately do not run (D-183,
  ; 1.2.3's amendment) — tripped a phantom WildLeak; found by the Bridge's
  ; stderr-tail test, the first to hold one there.
  %body = call ptr @npk_alloc_internal(i64 %n)
  call void @llvm.memcpy.p0.p0.i64(ptr %body, ptr %np, i64 %n, i1 false)
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %body, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } zeroinitializer, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

; The dual of .ptr / .len: wrap a buffer the caller already owns. Used by the
; lexer to build a decoded string literal, where the decoded bytes are not a
; slice of the source.
define { ptr, i64, i64 } @npk_string_from_bytes(ptr %p, i64 %n) {
  ; CAP 0 FOR THE SAME REASON AS THE SLICE ABOVE: the buffer belongs to the
  ; caller — a writer's sink, the lexer's decode buffer — and this header is a
  ; borrowed view of it. `irw_text` builds one over the IR writer's LIVE
  ; buffer; with `cap = n` the local holding it dropped the compiler's own
  ; output stream from under it.
  %s0 = insertvalue { ptr, i64, i64 } zeroinitializer, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 0, 2
  ret { ptr, i64, i64 } %s2
}
