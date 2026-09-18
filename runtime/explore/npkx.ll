; npkx.ll -- THE SCHEDULE EXPLORER'S SHIM (1.5.7 steps 1-3; D-212, D-298, D-300, D-301).
;
; Hand-written LLVM IR (D-203's form; D-298 says why it cannot be Nitpick: mutable
; module state, and code that runs inside the floor's critical sections and may
; not allocate, trap or call the floor). TEST INFRASTRUCTURE: linked into
; EXPLORED test binaries only -- never into anything the artifact is -- beside
; the explored floor that `npkg/explore.npk` writes from `runtime/npkrt.ll`.
; This module is never transformed; it stands under the floor's own belts (the
; `undef` ban, D-173 and the stack rule, the zero-dependency scan).
;
; ONE THREAD RUNS AT A TIME -- the baton. Every synchronization step of the
; transformed floor calls in here first (`npkx_point` before an atomic step
; line, `npkx_sys6` in place of the syscall trampoline); the scheduler decides
; who takes the next step, and everyone else sits in a REAL private futex wait
; on its slot's grant word. Only what BLOCKS is virtual: futex WAIT/WAKE (virtual
; queues, longest-blocked first), `epoll_pwait` (a real poll with timeout 0
; under the baton, re-probed only after another thread stepped), and
; `clock_gettime` (a VIRTUAL CLOCK: +1 us per read, a jump to the earliest
; pending deadline when nobody can step), and -- X-13, found by this port --
; the ADDRESS SPACE: an anonymous mapping with no hint is placed at a bump
; pointer, 64 KiB-aligned, because the floor's control flow depends on the
; alignment of `mmap`'s answer and a step count once depended on it. Everything
; else is the real syscall after a scheduling point. A seed names a schedule:
; decisions depend on the seed, slot indices and step counts -- never on tids,
; addresses or wall time -- so the same seed gives the same step count and the
; same schedule hash on every run and every machine.
;
; THE BEHAVIOURAL REFERENCE is the planning prototype's C shim
; (meta/roadmap/1.5/tools/explore_prototype/npkx.c, D-303): this file is held
; to it schedule hash for schedule hash (`hashcmp.sh` beside it), so every
; decision below is the prototype's decision in the prototype's order.
;
; Parameters, by environment (read once from /proc/self/environ at the first call):
;   NPKX_SEED (default 1)     NPKX_POLICY (0 PCT, 1 uniform random walk)
;   NPKX_D (PCT depth, 3)     NPKX_K (the step estimate the change points fall in, 2000)
;   NPKX_BUDGET (50,000,000 steps)    NPKX_ORACLE (1; 0 turns the quiescence oracles off)
;   NPKX_TRACE (1: the seed, steps, hash, clock and thread count at exit; 2: every step as `me what`)
;
; Verdicts end the process with `exit_group(97)` after printing the seed, the
; step count and every slot's state: DEADLOCK (nobody can step, no deadline
; pending, a thread lives), STEP BUDGET (a livelock the fairness rule did not
; break), MMAP (no deterministic address in 64 tries). THE SIGNALS ARE VIRTUAL
; (step 2): `rt_sigaction` is remembered, `tgkill` marks its target and makes a
; virtually blocked one runnable, and the handler runs in the target's own
; context at its next grant -- so the trap route (D-291's stop walk) is explored
; like everything else. THE QUIESCENCE ORACLES (step 3; D-301): before virtual
; time may jump and before a DEADLOCK is declared, LOST-FUTEX-WAKE (a virtual
; waiter's word no longer holds the value it waited on) and LOST-WAKE (a
; blocked thread's executor holds a task stamped DUE on its sleeper list) are
; read off the REAL state -- a lost wakeup here is lateness an exit code cannot
; see; NPKX_ORACLE=0 turns them off, for comparison only.

target triple = "x86_64-unknown-linux-gnu"

declare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)
declare ptr @npk_exec()
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

; --- the slots ---------------------------------------------------------------
;
;   0 grant (i32, atomic: the baton)   1 state   2 tid   3 prio (unsigned)
;   4 addr (B_FUTEX: the word)         5 deadline (virtual ns; -1 none)
;   6 reason   7 exit_tid   8 blocked_seq   9 polled_at   10 pending_sig
;   11 last_site   12 exec (the executor at the block)   13 waitval   14 pad
;
; states:  0 FREE  1 RUNNING  2 READY  3 B_FUTEX  4 B_EPOLL  5 ENDED
; reasons: 0 NONE  1 WOKEN  2 TIMEOUT  3 EXITWAIT  4 PROBE  5 SIGNAL

%npkx.slot = type { i32, i32, i64, i64, i64, i64, i32, i32, i64, i64, i32, i32, i64, i32, i32 }

@npkx_slots = internal global [64 x %npkx.slot] zeroinitializer
@npkx_nslots = internal global i32 0
@npkx_registered = internal global i32 0
@npkx_expected = internal global i32 0
@npkx_dying = internal global i32 0
@npkx_inited = internal global i32 0
@npkx_rng = internal global i64 1
@npkx_seed = internal global i64 1
@npkx_steps = internal global i64 0
@npkx_seq = internal global i64 0
@npkx_hash = internal global i64 1469598103934665603
@npkx_vnow = internal global i64 1000000000
@npkx_policy = internal global i32 0
@npkx_depth = internal global i32 3
@npkx_trace = internal global i32 0
@npkx_kest = internal global i64 2000
@npkx_budget = internal global i64 50000000
@npkx_change = internal global [16 x i64] zeroinitializer
@npkx_nchange = internal global i32 0
@npkx_ended_tids = internal global [64 x i64] zeroinitializer
@npkx_nended = internal global i32 0
@npkx_last_runner = internal global i32 -1
@npkx_run_len = internal global i64 0
@npkx_demote = internal global i64 1048576
@npkx_exit_ts = internal global { i64, i64 } { i64 0, i64 50000000 }

@npkx_key_seed = internal constant [9 x i8] c"NPKX_SEED"
@npkx_key_policy = internal constant [11 x i8] c"NPKX_POLICY"
@npkx_key_d = internal constant [6 x i8] c"NPKX_D"
@npkx_key_k = internal constant [6 x i8] c"NPKX_K"
@npkx_key_trace = internal constant [10 x i8] c"NPKX_TRACE"
@npkx_key_budget = internal constant [11 x i8] c"NPKX_BUDGET"

@npkx_s_pre = internal constant [6 x i8] c"npkx: "
@npkx_s_seed = internal constant [6 x i8] c" seed="
@npkx_s_steps = internal constant [7 x i8] c" steps="
@npkx_s_hash = internal constant [6 x i8] c" hash="
@npkx_s_vnow = internal constant [6 x i8] c" vnow="
@npkx_s_threads = internal constant [9 x i8] c" threads="
@npkx_s_nl = internal constant [1 x i8] c"\0A"
@npkx_s_sp = internal constant [1 x i8] c" "
@npkx_s_slot = internal constant [7 x i8] c"  slot "
@npkx_s_state = internal constant [7 x i8] c" state="
@npkx_s_addr = internal constant [6 x i8] c" addr="
@npkx_s_dead = internal constant [10 x i8] c" deadline="
@npkx_s_prio = internal constant [6 x i8] c" prio="
@npkx_s_last = internal constant [6 x i8] c" last="
@npkx_s_pend = internal constant [6 x i8] c" pend="
@npkx_s_deadlock = internal constant [8 x i8] c"DEADLOCK"
@npkx_s_budget = internal constant [23 x i8] c"STEP BUDGET (livelock?)"
@npkx_s_mmap = internal constant [34 x i8] c"MMAP (no deterministic address)   "
@npkx_map_next = internal global i64 17592186044416
; the virtual signals (step 2): the handler `rt_sigaction` installed, per signal number
@npkx_sig_handler = internal global [65 x ptr] zeroinitializer

; --- the slot fields, by (slot, field) ---------------------------------------
;
; A struct index must be a constant in a GEP, so a field is reached through
; its byte offset in the slot (88 bytes), from this table in field order.

@npkx_off = internal constant [15 x i64] [i64 0, i64 4, i64 8, i64 16, i64 24, i64 32, i64 40, i64 44, i64 48, i64 56, i64 64, i64 68, i64 72, i64 80, i64 84]

define internal ptr @npkx_f(i32 %t, i32 %k) {
entry:
  %op = getelementptr [15 x i64], ptr @npkx_off, i64 0, i32 %k
  %off = load i64, ptr %op
  %t64 = zext i32 %t to i64
  %base = mul i64 %t64, 88
  %at = add i64 %base, %off
  %p = getelementptr i8, ptr @npkx_slots, i64 %at
  ret ptr %p
}

define internal i32 @npkx_ld32(i32 %t, i32 %k) {
entry:
  %p = call ptr @npkx_f(i32 %t, i32 %k)
  %v = load i32, ptr %p
  ret i32 %v
}

define internal i64 @npkx_ld64(i32 %t, i32 %k) {
entry:
  %p = call ptr @npkx_f(i32 %t, i32 %k)
  %v = load i64, ptr %p
  ret i64 %v
}

define internal void @npkx_st32(i32 %t, i32 %k, i32 %v) {
entry:
  %p = call ptr @npkx_f(i32 %t, i32 %k)
  store i32 %v, ptr %p
  ret void
}

define internal void @npkx_st64(i32 %t, i32 %k, i64 %v) {
entry:
  %p = call ptr @npkx_f(i32 %t, i32 %k)
  store i64 %v, ptr %p
  ret void
}

; --- output ------------------------------------------------------------------

define internal void @npkx_put(ptr %s, i64 %n) {
entry:
  %sa = ptrtoint ptr %s to i64
  %r = call i64 @npk_sys6(i64 1, i64 2, i64 %sa, i64 %n, i64 0, i64 0, i64 0)
  ret void
}

; an unsigned decimal
define internal void @npkx_putn(i64 %v) {
entry:
  %buf = alloca [24 x i8], align 1
  call void @llvm.memset.p0.i64(ptr %buf, i8 0, i64 24, i1 false)
  %z = icmp eq i64 %v, 0
  br i1 %z, label %zero, label %digits

zero:
  %zp = getelementptr [24 x i8], ptr %buf, i64 0, i64 23
  store i8 48, ptr %zp
  call void @npkx_put(ptr %zp, i64 1)
  ret void

digits:
  %i = phi i64 [ 24, %entry ], [ %i1, %digits ]
  %cur = phi i64 [ %v, %entry ], [ %q, %digits ]
  %i1 = sub i64 %i, 1
  %q = udiv i64 %cur, 10
  %rm = urem i64 %cur, 10
  %d = trunc i64 %rm to i8
  %c = add i8 %d, 48
  %dp = getelementptr [24 x i8], ptr %buf, i64 0, i64 %i1
  store i8 %c, ptr %dp
  %more = icmp ne i64 %q, 0
  br i1 %more, label %digits, label %done

done:
  %len = sub i64 24, %i1
  call void @npkx_put(ptr %dp, i64 %len)
  ret void
}

; --- the environment ---------------------------------------------------------
;
; Read from /proc/self/environ ONCE, at init -- not through the floor's
; `npk_environ`: the shim's first call comes from `npk_start` before the floor
; has stored its environ slice, and a parameter read from an empty slice is a
; default that looks like a choice.

@npkx_envbuf = internal global [65536 x i8] zeroinitializer
@npkx_envlen = internal global i64 0
@npkx_envpath = internal constant [19 x i8] c"/proc/self/environ\00"

define internal void @npkx_env_read() {
entry:
  %pa = ptrtoint ptr @npkx_envpath to i64
  ; openat(AT_FDCWD, path, O_RDONLY)
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %pa, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %ret, label %read

read:
  %ba = ptrtoint ptr @npkx_envbuf to i64
  %n = call i64 @npk_sys6(i64 0, i64 %fd, i64 %ba, i64 65535, i64 0, i64 0, i64 0)
  %c = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %neg = icmp slt i64 %n, 0
  %len = select i1 %neg, i64 0, i64 %n
  store i64 %len, ptr @npkx_envlen
  br label %ret

ret:
  ret void
}

; `KEY=digits` among the NUL-separated entries of the buffer, else the default.
define internal i64 @npkx_env_u64(ptr %key, i64 %klen, i64 %dflt) {
entry:
  %len = load i64, ptr @npkx_envlen
  br label %loop

loop:
  %i = phi i64 [ 0, %entry ], [ %i1, %next ]
  %room = add i64 %i, %klen
  %done = icmp uge i64 %room, %len
  br i1 %done, label %notfound, label %atstart

; an entry starts at 0 or after a NUL
atstart:
  %first = icmp eq i64 %i, 0
  br i1 %first, label %cmp, label %prev

prev:
  %im1 = sub i64 %i, 1
  %pp = getelementptr [65536 x i8], ptr @npkx_envbuf, i64 0, i64 %im1
  %pc = load i8, ptr %pp
  %isnul = icmp eq i8 %pc, 0
  br i1 %isnul, label %cmp, label %next

cmp:
  %j = phi i64 [ 0, %atstart ], [ 0, %prev ], [ %j1, %cmpnext ]
  %jdone = icmp uge i64 %j, %klen
  br i1 %jdone, label %eq, label %cmpbyte

cmpbyte:
  %ij = add i64 %i, %j
  %ap = getelementptr [65536 x i8], ptr @npkx_envbuf, i64 0, i64 %ij
  %a = load i8, ptr %ap
  %bp = getelementptr i8, ptr %key, i64 %j
  %b = load i8, ptr %bp
  %same = icmp eq i8 %a, %b
  br i1 %same, label %cmpnext, label %next

cmpnext:
  %j1 = add i64 %j, 1
  br label %cmp

eq:
  %eqp = getelementptr [65536 x i8], ptr @npkx_envbuf, i64 0, i64 %room
  %eqc = load i8, ptr %eqp
  %iseq = icmp eq i8 %eqc, 61
  br i1 %iseq, label %parse, label %next

parse:
  %pstart = add i64 %room, 1
  br label %ploop

ploop:
  %p = phi i64 [ %pstart, %parse ], [ %p1, %pacc ]
  %val = phi i64 [ 0, %parse ], [ %val1, %pacc ]
  %pend = icmp uge i64 %p, %len
  br i1 %pend, label %found, label %pbyte

pbyte:
  %cp = getelementptr [65536 x i8], ptr @npkx_envbuf, i64 0, i64 %p
  %c = load i8, ptr %cp
  %ge0 = icmp uge i8 %c, 48
  %le9 = icmp ule i8 %c, 57
  %dig = and i1 %ge0, %le9
  br i1 %dig, label %pacc, label %found

pacc:
  %cz = zext i8 %c to i64
  %cd = sub i64 %cz, 48
  %v10 = mul i64 %val, 10
  %val1 = add i64 %v10, %cd
  %p1 = add i64 %p, 1
  br label %ploop

found:
  ret i64 %val

next:
  %i1 = add i64 %i, 1
  br label %loop

notfound:
  ret i64 %dflt
}

; --- the random walk ---------------------------------------------------------

; xorshift64 over the seed's state (the prototype's `next_rand`).
define internal i64 @npkx_next_rand() {
entry:
  %r0 = load i64, ptr @npkx_rng
  %s13 = shl i64 %r0, 13
  %r1 = xor i64 %r0, %s13
  %s7 = lshr i64 %r1, 7
  %r2 = xor i64 %r1, %s7
  %s17 = shl i64 %r2, 17
  %r3 = xor i64 %r2, %s17
  store i64 %r3, ptr @npkx_rng
  ret i64 %r3
}

define internal void @npkx_init() {
entry:
  store i32 1, ptr @npkx_inited
  call void @npkx_env_read()
  %seed = call i64 @npkx_env_u64(ptr @npkx_key_seed, i64 9, i64 1)
  store i64 %seed, ptr @npkx_seed
  %policy = call i64 @npkx_env_u64(ptr @npkx_key_policy, i64 11, i64 0)
  %policy32 = trunc i64 %policy to i32
  store i32 %policy32, ptr @npkx_policy
  %depth = call i64 @npkx_env_u64(ptr @npkx_key_d, i64 6, i64 3)
  %depth32 = trunc i64 %depth to i32
  store i32 %depth32, ptr @npkx_depth
  %kest = call i64 @npkx_env_u64(ptr @npkx_key_k, i64 6, i64 2000)
  store i64 %kest, ptr @npkx_kest
  %trace = call i64 @npkx_env_u64(ptr @npkx_key_trace, i64 10, i64 0)
  %trace32 = trunc i64 %trace to i32
  store i32 %trace32, ptr @npkx_trace
  %budget = call i64 @npkx_env_u64(ptr @npkx_key_budget, i64 11, i64 50000000)
  store i64 %budget, ptr @npkx_budget
  %oracle = call i64 @npkx_env_u64(ptr @npkx_key_oracle, i64 11, i64 1)
  %oracle32 = trunc i64 %oracle to i32
  store i32 %oracle32, ptr @npkx_oracle
  ; rng = seed * 0x9E3779B97F4A7C15 + 0x1234567, never 0
  %m = mul i64 %seed, -7046029254386353131
  %r = add i64 %m, 19088743
  %rz = icmp eq i64 %r, 0
  %r1 = select i1 %rz, i64 1, i64 %r
  store i64 %r1, ptr @npkx_rng
  br label %warm

warm:
  %w = phi i32 [ 0, %entry ], [ %w1, %warm ]
  %dummy = call i64 @npkx_next_rand()
  %w1 = add i32 %w, 1
  %wdone = icmp eq i32 %w1, 8
  br i1 %wdone, label %depthcap, label %warm

depthcap:
  %d0 = load i32, ptr @npkx_depth
  %big = icmp sgt i32 %d0, 16
  %d1 = select i1 %big, i32 16, i32 %d0
  store i32 %d1, ptr @npkx_depth
  %gt1 = icmp sgt i32 %d1, 1
  %dm1 = sub i32 %d1, 1
  %nch = select i1 %gt1, i32 %dm1, i32 0
  store i32 %nch, ptr @npkx_nchange
  %k0 = load i64, ptr @npkx_kest
  %kz = icmp eq i64 %k0, 0
  %k1 = select i1 %kz, i64 1, i64 %k0
  br label %chg

chg:
  %ci = phi i32 [ 0, %depthcap ], [ %ci1, %chgset ]
  %cdone = icmp sge i32 %ci, %nch
  br i1 %cdone, label %ret, label %chgset

chgset:
  %rv = call i64 @npkx_next_rand()
  %rm = urem i64 %rv, %k1
  %cv = add i64 %rm, 1
  %cp = getelementptr [16 x i64], ptr @npkx_change, i64 0, i32 %ci
  store i64 %cv, ptr %cp
  %ci1 = add i32 %ci, 1
  br label %chg

ret:
  ret void
}

; --- identity ----------------------------------------------------------------

define internal i64 @npkx_gettid() {
entry:
  %t = call i64 @npk_sys6(i64 186, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret i64 %t
}

; the caller's slot; the main thread claims one at its first point
define internal i32 @npkx_self() {
entry:
  %tid = call i64 @npkx_gettid()
  %n = load i32, ptr @npkx_nslots
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %next ]
  %done = icmp sge i32 %i, %n
  br i1 %done, label %claim, label %check

check:
  %stid = call i64 @npkx_ld64(i32 %i, i32 2)
  %same = icmp eq i64 %stid, %tid
  br i1 %same, label %live, label %next

live:
  %st = call i32 @npkx_ld32(i32 %i, i32 1)
  %ended = icmp eq i32 %st, 5
  br i1 %ended, label %next, label %found

found:
  ret i32 %i

next:
  %i1 = add i32 %i, 1
  br label %loop

claim:
  %n1 = add i32 %n, 1
  store i32 %n1, ptr @npkx_nslots
  call void @npkx_st64(i32 %n, i32 2, i64 %tid)
  call void @npkx_st32(i32 %n, i32 1, i32 1)
  %rv = call i64 @npkx_next_rand()
  %r8 = shl i64 %rv, 8
  %d = load i32, ptr @npkx_depth
  %d64 = sext i32 %d to i64
  %pr0 = add i64 %r8, %d64
  %pr = add i64 %pr0, 64
  call void @npkx_st64(i32 %n, i32 3, i64 %pr)
  call void @npkx_st64(i32 %n, i32 5, i64 -1)
  ret i32 %n
}

; --- the baton ---------------------------------------------------------------

define internal void @npkx_wait_grant(i32 %me) {
entry:
  %gp = call ptr @npkx_f(i32 %me, i32 0)
  %ga = ptrtoint ptr %gp to i64
  br label %loop

loop:
  %g = load atomic i32, ptr %gp seq_cst, align 4
  %granted = icmp ne i32 %g, 0
  br i1 %granted, label %take, label %wait

wait:
  ; FUTEX_WAIT | FUTEX_PRIVATE_FLAG (128): sleep while the grant word is 0
  %r = call i64 @npk_sys6(i64 202, i64 %ga, i64 128, i64 0, i64 0, i64 0, i64 0)
  br label %loop

take:
  store atomic i32 0, ptr %gp seq_cst, align 4
  ret void
}

define internal void @npkx_grant(i32 %t) {
entry:
  %gp = call ptr @npkx_f(i32 %t, i32 0)
  %ga = ptrtoint ptr %gp to i64
  store atomic i32 1, ptr %gp seq_cst, align 4
  ; FUTEX_WAKE | FUTEX_PRIVATE_FLAG (129), one waiter
  %r = call i64 @npk_sys6(i64 202, i64 %ga, i64 129, i64 1, i64 0, i64 0, i64 0)
  ret void
}

; --- the verdict -------------------------------------------------------------

define internal void @npkx_die(ptr %why, i64 %wlen) noreturn {
entry:
  call void @npkx_put(ptr @npkx_s_pre, i64 6)
  call void @npkx_put(ptr %why, i64 %wlen)
  call void @npkx_put(ptr @npkx_s_seed, i64 6)
  %seed = load i64, ptr @npkx_seed
  call void @npkx_putn(i64 %seed)
  call void @npkx_put(ptr @npkx_s_steps, i64 7)
  %steps = load i64, ptr @npkx_steps
  call void @npkx_putn(i64 %steps)
  call void @npkx_put(ptr @npkx_s_nl, i64 1)
  %n = load i32, ptr @npkx_nslots
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %slot ]
  %done = icmp sge i32 %i, %n
  br i1 %done, label %exit, label %slot

slot:
  call void @npkx_put(ptr @npkx_s_slot, i64 7)
  %iz = zext i32 %i to i64
  call void @npkx_putn(i64 %iz)
  call void @npkx_put(ptr @npkx_s_state, i64 7)
  %st = call i32 @npkx_ld32(i32 %i, i32 1)
  %stz = zext i32 %st to i64
  call void @npkx_putn(i64 %stz)
  call void @npkx_put(ptr @npkx_s_addr, i64 6)
  %ad = call i64 @npkx_ld64(i32 %i, i32 4)
  call void @npkx_putn(i64 %ad)
  call void @npkx_put(ptr @npkx_s_dead, i64 10)
  %dl = call i64 @npkx_ld64(i32 %i, i32 5)
  call void @npkx_putn(i64 %dl)
  call void @npkx_put(ptr @npkx_s_prio, i64 6)
  %pr = call i64 @npkx_ld64(i32 %i, i32 3)
  call void @npkx_putn(i64 %pr)
  call void @npkx_put(ptr @npkx_s_last, i64 6)
  %ls = call i32 @npkx_ld32(i32 %i, i32 11)
  %lsz = zext i32 %ls to i64
  call void @npkx_putn(i64 %lsz)
  call void @npkx_put(ptr @npkx_s_pend, i64 6)
  %pg = call i32 @npkx_ld32(i32 %i, i32 10)
  %pgz = zext i32 %pg to i64
  call void @npkx_putn(i64 %pgz)
  call void @npkx_put(ptr @npkx_s_nl, i64 1)
  %i1 = add i32 %i, 1
  br label %loop

exit:
  %r = call i64 @npk_sys6(i64 231, i64 97, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

; --- the scheduler -----------------------------------------------------------

; READY, or a blocked epoll waiter worth a probe because someone stepped since
define internal i1 @npkx_eligible(i32 %t) {
entry:
  %st = call i32 @npkx_ld32(i32 %t, i32 1)
  %ready = icmp eq i32 %st, 2
  br i1 %ready, label %yes, label %epoll

epoll:
  %isep = icmp eq i32 %st, 4
  br i1 %isep, label %probe, label %no

probe:
  %steps = load i64, ptr @npkx_steps
  %pa = call i64 @npkx_ld64(i32 %t, i32 9)
  %since = icmp ugt i64 %steps, %pa
  br i1 %since, label %yes, label %no

yes:
  ret i1 true

no:
  ret i1 false
}

; who takes the next step; -1 when nobody can
define internal i32 @npkx_pick() {
entry:
  %n = load i32, ptr @npkx_nslots
  %policy = load i32, ptr @npkx_policy
  %random = icmp eq i32 %policy, 1
  br i1 %random, label %rcount, label %ploop

; the uniform walk: count the eligible, draw one, find it again
rcount:
  %ri = phi i32 [ 0, %entry ], [ %ri1, %rnext ]
  %rc = phi i32 [ 0, %entry ], [ %rc1, %rnext ]
  %rdone = icmp sge i32 %ri, %n
  br i1 %rdone, label %rdraw, label %rcheck

rcheck:
  %re = call i1 @npkx_eligible(i32 %ri)
  %rinc = zext i1 %re to i32
  %rc1 = add i32 %rc, %rinc
  br label %rnext

rnext:
  %ri1 = add i32 %ri, 1
  br label %rcount

rdraw:
  %none = icmp eq i32 %rc, 0
  br i1 %none, label %rnone, label %rpick

rnone:
  ret i32 -1

rpick:
  %rv = call i64 @npkx_next_rand()
  %rc64 = zext i32 %rc to i64
  %rk = urem i64 %rv, %rc64
  br label %rfind

rfind:
  %fi = phi i32 [ 0, %rpick ], [ %fi1, %fnext ]
  %fk = phi i64 [ 0, %rpick ], [ %fk1, %fnext ]
  %fe = call i1 @npkx_eligible(i32 %fi)
  br i1 %fe, label %fhit, label %fnext

fhit:
  %isit = icmp eq i64 %fk, %rk
  br i1 %isit, label %ffound, label %fnext

ffound:
  ret i32 %fi

fnext:
  %finc = zext i1 %fe to i64
  %fk1 = add i64 %fk, %finc
  %fi1 = add i32 %fi, 1
  br label %rfind

; PCT: the highest priority among the eligible, the lowest slot on a tie
ploop:
  %pi = phi i32 [ 0, %entry ], [ %pi1, %pnext ]
  %best = phi i32 [ -1, %entry ], [ %best1, %pnext ]
  %bestpr = phi i64 [ 0, %entry ], [ %bestpr1, %pnext ]
  %pdone = icmp sge i32 %pi, %n
  br i1 %pdone, label %pret, label %pcheck

pcheck:
  %pe = call i1 @npkx_eligible(i32 %pi)
  br i1 %pe, label %pcmp, label %pnext

pcmp:
  %pr = call i64 @npkx_ld64(i32 %pi, i32 3)
  %nobest = icmp eq i32 %best, -1
  %higher = icmp ugt i64 %pr, %bestpr
  %take = or i1 %nobest, %higher
  br label %pnext

pnext:
  %ptake = phi i1 [ false, %pcheck ], [ %take, %pcmp ]
  %pprio = phi i64 [ 0, %pcheck ], [ %pr, %pcmp ]
  %best1 = select i1 %ptake, i32 %pi, i32 %best
  %bestpr1 = select i1 %ptake, i64 %pprio, i64 %bestpr
  %pi1 = add i32 %pi, 1
  br label %ploop

pret:
  ret i32 %best
}

; --- the quiescence oracles (step 3; D-301) -----------------------------------
;
; A lost wakeup in this runtime is LATENESS: every wait carries a deadline
; (D-071), so an executor that misses its wake sleeps until the next deadline
; and the program still exits right, late -- and virtual time hides the
; lateness completely. So the state is read at QUIESCENCE, before virtual time
; may jump and before a DEADLOCK is declared:
;   LOST-FUTEX-WAKE -- a virtual waiter's word no longer holds the value it
;     waited on: every futex protocol of the floor is "wait while the word is
;     v; whoever changes it wakes", so a changed word and a sleeping waiter is
;     a wake somebody owed and nobody sent;
;   LOST-WAKE -- a blocked thread's executor holds, on its sleeper list, a
;     frame stamped DUE (`wake_at` = 1): the models' wake-before-sleep,
;     absorbed-notification and spent-marker, read off the REAL executor.
; The three offsets below are the floor's struct layouts (`%npk.exec` field 2,
; `%npk.hdr` fields 7 and 8), held to `runtime/npkrt.ll` by a belt in both
; runners (`explore-oracle-offsets`). NPKX_ORACLE=0 turns the oracles off, for
; comparison only.
@npkx_off_sl_head = internal constant i64 16
@npkx_off_qnext = internal constant i64 48
@npkx_off_wake_at = internal constant i64 56
@npkx_oracle = internal global i32 1
@npkx_key_oracle = internal constant [11 x i8] c"NPKX_ORACLE"
@npkx_s_lostfutex = internal constant [60 x i8] c"LOST-FUTEX-WAKE (a waiter's word changed and nobody woke it)"
@npkx_s_lostwake = internal constant [52 x i8] c"LOST-WAKE (an executor sleeps on a task stamped due)"

define internal void @npkx_lost_wake_oracle() {
entry:
  %on = load i32, ptr @npkx_oracle
  %off = icmp eq i32 %on, 0
  br i1 %off, label %ret, label %futex

futex:
  %n = load i32, ptr @npkx_nslots
  br label %floop

floop:
  %fi = phi i32 [ 0, %futex ], [ %fi1, %fnext ]
  %fdone = icmp sge i32 %fi, %n
  br i1 %fdone, label %wake, label %fcheck

fcheck:
  %fst = call i32 @npkx_ld32(i32 %fi, i32 1)
  %fisf = icmp eq i32 %fst, 3
  br i1 %fisf, label %fword, label %fnext

fword:
  %fad = call i64 @npkx_ld64(i32 %fi, i32 4)
  %fap = inttoptr i64 %fad to ptr
  %fcur = load atomic i32, ptr %fap seq_cst, align 4
  %fwant = call i32 @npkx_ld32(i32 %fi, i32 13)
  %fchanged = icmp ne i32 %fcur, %fwant
  br i1 %fchanged, label %lostfutex, label %fnext

lostfutex:
  call void @npkx_die(ptr @npkx_s_lostfutex, i64 60)
  unreachable

fnext:
  %fi1 = add i32 %fi, 1
  br label %floop

wake:
  br label %wloop

wloop:
  %wi = phi i32 [ 0, %wake ], [ %wi1, %wnext ]
  %wdone = icmp sge i32 %wi, %n
  br i1 %wdone, label %ret, label %wcheck

wcheck:
  %wst = call i32 @npkx_ld32(i32 %wi, i32 1)
  %wisf = icmp eq i32 %wst, 3
  %wise = icmp eq i32 %wst, 4
  %wblocked = or i1 %wisf, %wise
  br i1 %wblocked, label %wexec, label %wnext

wexec:
  %wex = call i64 @npkx_ld64(i32 %wi, i32 12)
  %wnoex = icmp eq i64 %wex, 0
  br i1 %wnoex, label %wnext, label %whead

whead:
  %shoff = load i64, ptr @npkx_off_sl_head
  %headat = add i64 %wex, %shoff
  %headp = inttoptr i64 %headat to ptr
  %f0 = load i64, ptr %headp
  %qoff = load i64, ptr @npkx_off_qnext
  %waoff = load i64, ptr @npkx_off_wake_at
  br label %walk

walk:
  %f = phi i64 [ %f0, %whead ], [ %fn, %wstep ]
  %cnt = phi i64 [ 0, %whead ], [ %cnt1, %wstep ]
  %fnull = icmp eq i64 %f, 0
  %toofar = icmp uge i64 %cnt, 100000
  %stop = or i1 %fnull, %toofar
  br i1 %stop, label %wnext, label %wframe

wframe:
  %waat = add i64 %f, %waoff
  %wap = inttoptr i64 %waat to ptr
  %wa = load i64, ptr %wap
  %due = icmp eq i64 %wa, 1
  br i1 %due, label %lostwake, label %wstep

lostwake:
  call void @npkx_die(ptr @npkx_s_lostwake, i64 52)
  unreachable

wstep:
  %qat = add i64 %f, %qoff
  %qp = inttoptr i64 %qat to ptr
  %fn = load i64, ptr %qp
  %cnt1 = add i64 %cnt, 1
  br label %walk

wnext:
  %wi1 = add i32 %wi, 1
  br label %wloop

ret:
  ret void
}

; nobody can step: move virtual time to the earliest deadline; -1 when none
define internal i32 @npkx_advance() {
entry:
  call void @npkx_lost_wake_oracle()
  %n = load i32, ptr @npkx_nslots
  br label %loop

loop:
  %i = phi i32 [ 0, %entry ], [ %i1, %next ]
  %best = phi i32 [ -1, %entry ], [ %best1, %next ]
  %bestdl = phi i64 [ 0, %entry ], [ %bestdl1, %next ]
  %done = icmp sge i32 %i, %n
  br i1 %done, label %decide, label %check

check:
  %st = call i32 @npkx_ld32(i32 %i, i32 1)
  %isf = icmp eq i32 %st, 3
  %ise = icmp eq i32 %st, 4
  %blocked = or i1 %isf, %ise
  br i1 %blocked, label %dl, label %next

dl:
  %d = call i64 @npkx_ld64(i32 %i, i32 5)
  %has = icmp sge i64 %d, 0
  br i1 %has, label %cmp, label %next

cmp:
  %nobest = icmp eq i32 %best, -1
  %earlier = icmp slt i64 %d, %bestdl
  %take = or i1 %nobest, %earlier
  br label %next

next:
  %ptake = phi i1 [ false, %check ], [ false, %dl ], [ %take, %cmp ]
  %pdl = phi i64 [ 0, %check ], [ 0, %dl ], [ %d, %cmp ]
  %best1 = select i1 %ptake, i32 %i, i32 %best
  %bestdl1 = select i1 %ptake, i64 %pdl, i64 %bestdl
  %i1 = add i32 %i, 1
  br label %loop

decide:
  %none = icmp eq i32 %best, -1
  br i1 %none, label %ret, label %jump

jump:
  %vnow = load i64, ptr @npkx_vnow
  %later = icmp sgt i64 %bestdl, %vnow
  %vnow1 = select i1 %later, i64 %bestdl, i64 %vnow
  store i64 %vnow1, ptr @npkx_vnow
  call void @npkx_st32(i32 %best, i32 1, i32 2)
  call void @npkx_st32(i32 %best, i32 6, i32 2)
  br label %ret

ret:
  %r = phi i32 [ -1, %decide ], [ %best, %jump ]
  ret i32 %r
}

; give the baton away (or keep it); `me` < 0: the caller is leaving for good
define internal void @npkx_resched(i32 %me) {
entry:
  %next0 = call i32 @npkx_pick()
  %nonext = icmp slt i32 %next0, 0
  br i1 %nonext, label %adv, label %have

adv:
  %next1 = call i32 @npkx_advance()
  %still = icmp slt i32 %next1, 0
  br i1 %still, label %stuck, label %have

stuck:
  %n = load i32, ptr @npkx_nslots
  br label %count

count:
  %i = phi i32 [ 0, %stuck ], [ %i1, %count ]
  %live = phi i32 [ 0, %stuck ], [ %live1, %count ]
  %cdone = icmp sge i32 %i, %n
  %st = call i32 @npkx_ld32(i32 %i, i32 1)
  %alive = icmp ne i32 %st, 5
  %inc0 = zext i1 %alive to i32
  %inc = select i1 %cdone, i32 0, i32 %inc0
  %live1 = add i32 %live, %inc
  %i1 = add i32 %i, 1
  br i1 %cdone, label %counted, label %count

counted:
  %leaving = icmp slt i32 %me, 0
  %nolive = icmp eq i32 %live, 0
  %quiet = and i1 %leaving, %nolive
  br i1 %quiet, label %ret, label %deadlock

deadlock:
  call void @npkx_lost_wake_oracle()
  call void @npkx_die(ptr @npkx_s_deadlock, i64 8)
  unreachable

have:
  %next = phi i32 [ %next0, %entry ], [ %next1, %adv ]
  %nst = call i32 @npkx_ld32(i32 %next, i32 1)
  %isep = icmp eq i32 %nst, 4
  br i1 %isep, label %probe, label %handoff

probe:
  call void @npkx_st32(i32 %next, i32 6, i32 4)
  br label %handoff

handoff:
  %same = icmp eq i32 %next, %me
  br i1 %same, label %ret, label %give

give:
  call void @npkx_grant(i32 %next)
  %leaving2 = icmp slt i32 %me, 0
  br i1 %leaving2, label %ret, label %wait

wait:
  call void @npkx_wait_grant(i32 %me)
  br label %ret

ret:
  ret void
}

; FAIRNESS: strict priorities starve everyone behind a thread that never
; blocks. After 4,096 consecutive steps by one thread while another could
; step, it drops below everything -- below every undemoted thread and below
; every EARLIER demotion (the bands must stay ordered: initial > change-point
; > demotion).
define internal void @npkx_count_step(i32 %me, i64 %what) {
entry:
  %lr = load i32, ptr @npkx_last_runner
  %samerun = icmp eq i32 %lr, %me
  %rl = load i64, ptr @npkx_run_len
  %rl1 = add i64 %rl, 1
  %rlnew = select i1 %samerun, i64 %rl1, i64 0
  store i32 %me, ptr @npkx_last_runner
  store i64 %rlnew, ptr @npkx_run_len
  %policy = load i32, ptr @npkx_policy
  %pct = icmp eq i32 %policy, 0
  %long = icmp ugt i64 %rlnew, 4096
  %fair = and i1 %pct, %long
  br i1 %fair, label %others, label %record

others:
  %n = load i32, ptr @npkx_nslots
  br label %oloop

oloop:
  %oi = phi i32 [ 0, %others ], [ %oi1, %onext ]
  %other = phi i1 [ false, %others ], [ %other1, %onext ]
  %odone = icmp sge i32 %oi, %n
  br i1 %odone, label %odecide, label %ocheck

ocheck:
  %notme = icmp ne i32 %oi, %me
  %ost = call i32 @npkx_ld32(i32 %oi, i32 1)
  %oready = icmp eq i32 %ost, 2
  %oep = icmp eq i32 %ost, 4
  %ocan = or i1 %oready, %oep
  %ohit = and i1 %notme, %ocan
  br label %onext

onext:
  %other1 = or i1 %other, %ohit
  %oi1 = add i32 %oi, 1
  br label %oloop

odecide:
  br i1 %other, label %demote, label %record

demote:
  %dm = load i64, ptr @npkx_demote
  %dm1 = sub i64 %dm, 1
  store i64 %dm1, ptr @npkx_demote
  call void @npkx_st64(i32 %me, i32 3, i64 %dm1)
  store i64 0, ptr @npkx_run_len
  br label %record

record:
  %me64v = zext i32 %me to i64
  %site32 = trunc i64 %what to i32
  call void @npkx_st32(i32 %me, i32 11, i32 %site32)
  %steps = load i64, ptr @npkx_steps
  %steps1 = add i64 %steps, 1
  store i64 %steps1, ptr @npkx_steps
  ; NPKX_TRACE=2: every step as `me what`, for diffing two schedules
  %tr = load i32, ptr @npkx_trace
  %verbose = icmp sgt i32 %tr, 1
  br i1 %verbose, label %stepline, label %hash

stepline:
  call void @npkx_putn(i64 %me64v)
  call void @npkx_put(ptr @npkx_s_sp, i64 1)
  call void @npkx_putn(i64 %what)
  call void @npkx_put(ptr @npkx_s_nl, i64 1)
  br label %hash

hash:
  %h = load i64, ptr @npkx_hash
  %mesh = shl i64 %me64v, 32
  %ev = or i64 %mesh, %what
  %hx = xor i64 %h, %ev
  %h1 = mul i64 %hx, 1099511628211
  store i64 %h1, ptr @npkx_hash
  br i1 %pct, label %chg, label %budget

; a change point: below every initial priority, ABOVE every fairness demotion
chg:
  %nch = load i32, ptr @npkx_nchange
  br label %cloop

cloop:
  %ci = phi i32 [ 0, %chg ], [ %ci1, %cnext ]
  %cdone = icmp sge i32 %ci, %nch
  br i1 %cdone, label %budget, label %ccheck

ccheck:
  %cp = getelementptr [16 x i64], ptr @npkx_change, i64 0, i32 %ci
  %cv = load i64, ptr %cp
  %at = icmp eq i64 %steps1, %cv
  br i1 %at, label %cset, label %cnext

cset:
  %rank = sub i32 %nch, %ci
  %rank64 = zext i32 %rank to i64
  %cprio = add i64 1073741824, %rank64
  call void @npkx_st64(i32 %me, i32 3, i64 %cprio)
  br label %cnext

cnext:
  %ci1 = add i32 %ci, 1
  br label %cloop

budget:
  %b = load i64, ptr @npkx_budget
  %over = icmp ugt i64 %steps1, %b
  br i1 %over, label %livelock, label %ret

livelock:
  call void @npkx_die(ptr @npkx_s_budget, i64 23)
  unreachable

ret:
  ret void
}

; --- the virtual signals (step 2) --------------------------------------------

; A pending signal runs its handler NOW, in this thread's own context, at the
; grant it was made runnable for; the handler's own steps are explored like
; any others. The stop handler (D-291) never returns: it parks forever on a
; word nothing writes, a virtual wait with no deadline.
define internal void @npkx_run_pending(i32 %me) {
entry:
  %sg = call i32 @npkx_ld32(i32 %me, i32 10)
  %none = icmp eq i32 %sg, 0
  br i1 %none, label %ret, label %fire

fire:
  call void @npkx_st32(i32 %me, i32 10, i32 0)
  call void @npkx_st32(i32 %me, i32 1, i32 1)
  %hp = getelementptr [65 x ptr], ptr @npkx_sig_handler, i64 0, i32 %sg
  %h = load ptr, ptr %hp
  %noh = icmp eq ptr %h, null
  br i1 %noh, label %ret, label %handle

handle:
  call void %h(i32 %sg, ptr null, ptr null)
  br label %ret

ret:
  ret void
}

; --- the hooks the transformed floor calls -----------------------------------

define void @npkx_point(i32 %site) {
entry:
  %dying = load i32, ptr @npkx_dying
  %d = icmp ne i32 %dying, 0
  br i1 %d, label %ret, label %go

go:
  %in = load i32, ptr @npkx_inited
  %uninit = icmp eq i32 %in, 0
  br i1 %uninit, label %init, label %step

init:
  call void @npkx_init()
  br label %step

step:
  %me = call i32 @npkx_self()
  %sz = zext i32 %site to i64
  call void @npkx_count_step(i32 %me, i64 %sz)
  call void @npkx_st32(i32 %me, i32 1, i32 2)
  call void @npkx_resched(i32 %me)
  call void @npkx_st32(i32 %me, i32 1, i32 1)
  call void @npkx_run_pending(i32 %me)
  br label %ret

ret:
  ret void
}

define void @npkx_prespawn() {
entry:
  %in = load i32, ptr @npkx_inited
  %uninit = icmp eq i32 %in, 0
  br i1 %uninit, label %init, label %go

init:
  call void @npkx_init()
  br label %go

go:
  %me = call i32 @npkx_self()
  %reg = load atomic i32, ptr @npkx_registered seq_cst, align 4
  %exp = add i32 %reg, 1
  store i32 %exp, ptr @npkx_expected
  ret void
}

; the parent waits for the child to register and hand the baton back
define void @npkx_spawned(i64 %tid) {
entry:
  %dying = load i32, ptr @npkx_dying
  %d = icmp ne i32 %dying, 0
  %bad = icmp sle i64 %tid, 0
  %skip = or i1 %d, %bad
  br i1 %skip, label %ret, label %loop

loop:
  %reg = load atomic i32, ptr @npkx_registered seq_cst, align 4
  %exp = load i32, ptr @npkx_expected
  %behind = icmp slt i32 %reg, %exp
  br i1 %behind, label %wait, label %ret

wait:
  %ra = ptrtoint ptr @npkx_registered to i64
  %expm1 = sub i32 %exp, 1
  %expm1z = zext i32 %expm1 to i64
  %r = call i64 @npk_sys6(i64 202, i64 %ra, i64 128, i64 %expm1z, i64 0, i64 0, i64 0)
  br label %loop

ret:
  ret void
}

; the child's first act: a slot, registration, then it waits for the baton
define void @npkx_begin() {
entry:
  %n = load i32, ptr @npkx_nslots
  %n1 = add i32 %n, 1
  store i32 %n1, ptr @npkx_nslots
  %tid = call i64 @npkx_gettid()
  call void @npkx_st64(i32 %n, i32 2, i64 %tid)
  call void @npkx_st32(i32 %n, i32 1, i32 2)
  call void @npkx_st64(i32 %n, i32 5, i64 -1)
  %gp = call ptr @npkx_f(i32 %n, i32 0)
  store atomic i32 0, ptr %gp seq_cst, align 4
  %rv = call i64 @npkx_next_rand()
  %r8 = shl i64 %rv, 8
  %d = load i32, ptr @npkx_depth
  %d64 = sext i32 %d to i64
  %pr0 = add i64 %r8, %d64
  %pr = add i64 %pr0, 64
  call void @npkx_st64(i32 %n, i32 3, i64 %pr)
  %reg = load atomic i32, ptr @npkx_registered seq_cst, align 4
  %reg1 = add i32 %reg, 1
  store atomic i32 %reg1, ptr @npkx_registered seq_cst, align 4
  %ra = ptrtoint ptr @npkx_registered to i64
  %w = call i64 @npk_sys6(i64 202, i64 %ra, i64 129, i64 64, i64 0, i64 0, i64 0)
  call void @npkx_wait_grant(i32 %n)
  call void @npkx_st32(i32 %n, i32 1, i32 1)
  call void @npkx_run_pending(i32 %n)
  ret void
}

; the thread's last act: whoever waits on its tid word will see the kernel clear it
define void @npkx_end() {
entry:
  %dying = load i32, ptr @npkx_dying
  %d = icmp ne i32 %dying, 0
  br i1 %d, label %ret, label %go

go:
  %me = call i32 @npkx_self()
  %tid = call i64 @npkx_ld64(i32 %me, i32 2)
  call void @npkx_st32(i32 %me, i32 1, i32 5)
  %ne = load i32, ptr @npkx_nended
  %ep = getelementptr [64 x i64], ptr @npkx_ended_tids, i64 0, i32 %ne
  store i64 %tid, ptr %ep
  %ne1 = add i32 %ne, 1
  store i32 %ne1, ptr @npkx_nended
  %tid32 = trunc i64 %tid to i32
  %n = load i32, ptr @npkx_nslots
  br label %loop

loop:
  %i = phi i32 [ 0, %go ], [ %i1, %next ]
  %done = icmp sge i32 %i, %n
  br i1 %done, label %leave, label %check

check:
  %st = call i32 @npkx_ld32(i32 %i, i32 1)
  %isf = icmp eq i32 %st, 3
  br i1 %isf, label %word, label %next

word:
  %ad = call i64 @npkx_ld64(i32 %i, i32 4)
  %ap = inttoptr i64 %ad to ptr
  %wv = load atomic i32, ptr %ap seq_cst, align 4
  %ontid = icmp eq i32 %wv, %tid32
  br i1 %ontid, label %rouse, label %next

rouse:
  call void @npkx_st32(i32 %i, i32 1, i32 2)
  call void @npkx_st32(i32 %i, i32 6, i32 3)
  call void @npkx_st32(i32 %i, i32 7, i32 %tid32)
  br label %next

next:
  %i1 = add i32 %i, 1
  br label %loop

leave:
  call void @npkx_resched(i32 -1)
  br label %ret

ret:
  ret void
}

; --- the virtual syscalls ----------------------------------------------------

define internal i64 @npkx_rel_ns(i64 %tsp) {
entry:
  %none = icmp eq i64 %tsp, 0
  br i1 %none, label %ret, label %read

read:
  %p = inttoptr i64 %tsp to ptr
  %s = load i64, ptr %p
  %np = getelementptr i64, ptr %p, i64 1
  %ns = load i64, ptr %np
  %s9 = mul i64 %s, 1000000000
  %v = add i64 %s9, %ns
  br label %ret

ret:
  %r = phi i64 [ -1, %entry ], [ %v, %read ]
  ret i64 %r
}

; a joiner blocked on the tid of a thread that has ENDED: the kernel's CLEARTID
; write and its SHARED wake are invisible to the virtual queues -- a real wait
define internal void @npkx_real_exit_wait(i64 %addr, i32 %tid) {
entry:
  %ap = inttoptr i64 %addr to ptr
  %tsa = ptrtoint ptr @npkx_exit_ts to i64
  %tz = zext i32 %tid to i64
  br label %loop

loop:
  %w = load atomic i32, ptr %ap seq_cst, align 4
  %still = icmp eq i32 %w, %tid
  br i1 %still, label %wait, label %ret

wait:
  %r = call i64 @npk_sys6(i64 202, i64 %addr, i64 0, i64 %tz, i64 %tsa, i64 0, i64 0)
  br label %loop

ret:
  ret void
}

define internal void @npkx_trace_exit() {
entry:
  %t = load i32, ptr @npkx_trace
  %on = icmp ne i32 %t, 0
  br i1 %on, label %print, label %ret

print:
  call void @npkx_put(ptr @npkx_s_pre, i64 6)
  call void @npkx_put(ptr @npkx_s_seed, i64 6)
  %seed = load i64, ptr @npkx_seed
  call void @npkx_putn(i64 %seed)
  call void @npkx_put(ptr @npkx_s_steps, i64 7)
  %steps = load i64, ptr @npkx_steps
  call void @npkx_putn(i64 %steps)
  call void @npkx_put(ptr @npkx_s_hash, i64 6)
  %h = load i64, ptr @npkx_hash
  call void @npkx_putn(i64 %h)
  call void @npkx_put(ptr @npkx_s_vnow, i64 6)
  %v = load i64, ptr @npkx_vnow
  call void @npkx_putn(i64 %v)
  ; the thread count, n in PCT's per-run bound 1/(n*k^(d-1)), which the stage prints
  call void @npkx_put(ptr @npkx_s_threads, i64 9)
  %ns = load i32, ptr @npkx_nslots
  %nsz = zext i32 %ns to i64
  call void @npkx_putn(i64 %nsz)
  call void @npkx_put(ptr @npkx_s_nl, i64 1)
  br label %ret

ret:
  ret void
}

define i64 @npkx_sys6(i64 %n, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f) {
entry:
  %dying = load i32, ptr @npkx_dying
  %dy = icmp ne i32 %dying, 0
  br i1 %dy, label %passthrough, label %live

live:
  %in = load i32, ptr @npkx_inited
  %uninit = icmp eq i32 %in, 0
  br i1 %uninit, label %init, label %ready

init:
  call void @npkx_init()
  br label %ready

ready:
  %isexitgroup = icmp eq i64 %n, 231
  br i1 %isexitgroup, label %exitgroup, label %ident

exitgroup:
  call void @npkx_trace_exit()
  br label %passthrough

ident:
  %me = call i32 @npkx_self()
  %isexit = icmp eq i64 %n, 60
  br i1 %isexit, label %threadexit, label %step

threadexit:
  call void @npkx_end()
  br label %passthrough

step:
  %what = or i64 %n, 2147483648
  call void @npkx_count_step(i32 %me, i64 %what)
  call void @npkx_st32(i32 %me, i32 1, i32 2)
  call void @npkx_resched(i32 %me)
  call void @npkx_st32(i32 %me, i32 1, i32 1)
  call void @npkx_run_pending(i32 %me)
  %issigaction = icmp eq i64 %n, 13
  br i1 %issigaction, label %sigaction, label %notsigaction

; THE VIRTUAL SIGNALS (step 2). rt_sigaction: the handler is remembered (the
; real action is installed too, and never fires: no real signal is ever sent)
sigaction:
  %sigok0 = icmp sgt i64 %a, 0
  %sigok1 = icmp slt i64 %a, 65
  %sigokp = icmp ne i64 %b, 0
  %sigok2 = and i1 %sigok0, %sigok1
  %sigok = and i1 %sigok2, %sigokp
  br i1 %sigok, label %remember, label %passthrough

remember:
  %actp = inttoptr i64 %b to ptr
  %handler = load ptr, ptr %actp
  %hp = getelementptr [65 x ptr], ptr @npkx_sig_handler, i64 0, i64 %a
  store ptr %handler, ptr %hp
  br label %passthrough

notsigaction:
  %istgkill = icmp eq i64 %n, 234
  br i1 %istgkill, label %tgkill, label %notclock

; tgkill: the target is marked; it runs the handler in its own context at its
; next grant, and a virtually BLOCKED target is made runnable (a signal
; interrupts a blocking call); an unknown tid is ESRCH
tgkill:
  %nk = load i32, ptr @npkx_nslots
  br label %kloop

kloop:
  %ki = phi i32 [ 0, %tgkill ], [ %ki1, %knext ]
  %kdone = icmp sge i32 %ki, %nk
  br i1 %kdone, label %esrch, label %kcheck

kcheck:
  %ktid = call i64 @npkx_ld64(i32 %ki, i32 2)
  %ksame = icmp eq i64 %ktid, %b
  br i1 %ksame, label %klive, label %knext

klive:
  %kst = call i32 @npkx_ld32(i32 %ki, i32 1)
  %kended = icmp eq i32 %kst, 5
  br i1 %kended, label %knext, label %kmark

kmark:
  %sig32 = trunc i64 %c to i32
  call void @npkx_st32(i32 %ki, i32 10, i32 %sig32)
  %kisf = icmp eq i32 %kst, 3
  %kise = icmp eq i32 %kst, 4
  %kblocked = or i1 %kisf, %kise
  br i1 %kblocked, label %kwake, label %kdone2

kwake:
  call void @npkx_st32(i32 %ki, i32 1, i32 2)
  call void @npkx_st32(i32 %ki, i32 6, i32 5)
  br label %kdone2

kdone2:
  ret i64 0

knext:
  %ki1 = add i32 %ki, 1
  br label %kloop

esrch:
  ret i64 -3

notclock:
  %isclock = icmp eq i64 %n, 228
  br i1 %isclock, label %clock, label %notclock1

; the virtual clock: +1 us per read
clock:
  %v0 = load i64, ptr @npkx_vnow
  %v1 = add i64 %v0, 1000
  store i64 %v1, ptr @npkx_vnow
  %tsp = inttoptr i64 %b to ptr
  %secs = sdiv i64 %v1, 1000000000
  %nsecs = srem i64 %v1, 1000000000
  store i64 %secs, ptr %tsp
  %nsp = getelementptr i64, ptr %tsp, i64 1
  store i64 %nsecs, ptr %nsp
  ret i64 0

notclock1:
  %ismmap = icmp eq i64 %n, 9
  br i1 %ismmap, label %mmap, label %notmmap

; THE VIRTUAL ADDRESS SPACE (1.5.7 step 1's finding): the floor's control flow
; depends on the addresses the kernel hands out -- `npk_chunk_new` over-maps
; and trims, and whether its pre-trim `munmap` happens depends on the
; alignment of `mmap`'s answer -- so a step count that includes syscalls
; depended on an address, and two runs of one seed differed by one step
; under load. An anonymous private mapping with no hint is placed by the shim
; instead: at a bump pointer, 64 KiB-aligned, with MAP_FIXED_NOREPLACE, so
; every run of a seed sees the same addresses. A collision (EEXIST, or a
; kernel that answered elsewhere) moves the pointer by 1 GiB and tries again,
; deterministically. Hinted, fixed or file-backed mappings pass through.
mmap:
  %nohint = icmp eq i64 %a, 0
  %anon = and i64 %d, 32
  %isanon = icmp ne i64 %anon, 0
  %fixed = and i64 %d, 16
  %isfixed = icmp ne i64 %fixed, 0
  %ours0 = and i1 %nohint, %isanon
  %notfixed = xor i1 %isfixed, true
  %ours = and i1 %ours0, %notfixed
  br i1 %ours, label %mloop, label %passthrough

mloop:
  %tries = phi i64 [ 0, %mmap ], [ %tries1, %mretry ]
  %hint = load i64, ptr @npkx_map_next
  %flags = or i64 %d, 1048576
  %mr = call i64 @npk_sys6(i64 9, i64 %hint, i64 %b, i64 %c, i64 %flags, i64 %e, i64 %f)
  %mhit = icmp eq i64 %mr, %hint
  br i1 %mhit, label %mplaced, label %mmiss

mplaced:
  %mend = add i64 %hint, %b
  %mend1 = add i64 %mend, 65535
  %mnext = and i64 %mend1, -65536
  store i64 %mnext, ptr @npkx_map_next
  ret i64 %mr

; not placed: an error (EEXIST) leaves nothing to undo; an answer elsewhere is unmapped
mmiss:
  %merr = icmp ugt i64 %mr, -4096
  br i1 %merr, label %mretry, label %munmap

munmap:
  %mu = call i64 @npk_sys6(i64 11, i64 %mr, i64 %b, i64 0, i64 0, i64 0, i64 0)
  br label %mretry

mretry:
  %skip = add i64 %hint, 1073741824
  store i64 %skip, ptr @npkx_map_next
  %tries1 = add i64 %tries, 1
  %giveup = icmp uge i64 %tries1, 64
  br i1 %giveup, label %mdie, label %mloop

mdie:
  call void @npkx_die(ptr @npkx_s_mmap, i64 34)
  unreachable

notmmap:
  %isfutex = icmp eq i64 %n, 202
  br i1 %isfutex, label %futex, label %notfutex

futex:
  %op = and i64 %b, 127
  %iswait = icmp eq i64 %op, 0
  %iswaitb = icmp eq i64 %op, 9
  %anywait = or i1 %iswait, %iswaitb
  br i1 %anywait, label %fwait, label %fwake0

; WAIT / WAIT_BITSET: block virtually while the word holds the expected value
fwait:
  %wp = inttoptr i64 %a to ptr
  %cur = load atomic i32, ptr %wp seq_cst, align 4
  %c32 = trunc i64 %c to i32
  %changed = icmp ne i32 %cur, %c32
  br i1 %changed, label %eagain, label %endedcheck

eagain:
  ret i64 -11

; a word holding the tid of an ENDED thread: the kernel clears it, not us
endedcheck:
  %ne = load i32, ptr @npkx_nended
  br label %eloop

eloop:
  %ei = phi i32 [ 0, %endedcheck ], [ %ei1, %enext ]
  %edone = icmp sge i32 %ei, %ne
  br i1 %edone, label %block, label %echeck

echeck:
  %etp = getelementptr [64 x i64], ptr @npkx_ended_tids, i64 0, i32 %ei
  %et = load i64, ptr %etp
  %et32 = trunc i64 %et to i32
  %ehit = icmp eq i32 %et32, %cur
  br i1 %ehit, label %exitwait0, label %enext

enext:
  %ei1 = add i32 %ei, 1
  br label %eloop

exitwait0:
  call void @npkx_real_exit_wait(i64 %a, i32 %cur)
  ret i64 0

block:
  %rel = call i64 @npkx_rel_ns(i64 %d)
  %ex = call ptr @npk_exec()
  %exi = ptrtoint ptr %ex to i64
  call void @npkx_st64(i32 %me, i32 12, i64 %exi)
  call void @npkx_st32(i32 %me, i32 13, i32 %c32)
  call void @npkx_st32(i32 %me, i32 1, i32 3)
  call void @npkx_st64(i32 %me, i32 4, i64 %a)
  call void @npkx_st32(i32 %me, i32 6, i32 0)
  %sq = load i64, ptr @npkx_seq
  %sq1 = add i64 %sq, 1
  store i64 %sq1, ptr @npkx_seq
  call void @npkx_st64(i32 %me, i32 8, i64 %sq1)
  %norel = icmp slt i64 %rel, 0
  %vnow = load i64, ptr @npkx_vnow
  %vrel = add i64 %vnow, %rel
  %absdl = select i1 %iswaitb, i64 %rel, i64 %vrel
  %dl = select i1 %norel, i64 -1, i64 %absdl
  call void @npkx_st64(i32 %me, i32 5, i64 %dl)
  call void @npkx_resched(i32 %me)
  call void @npkx_st32(i32 %me, i32 1, i32 1)
  call void @npkx_st64(i32 %me, i32 5, i64 -1)
  call void @npkx_run_pending(i32 %me)
  %why = call i32 @npkx_ld32(i32 %me, i32 6)
  %exitwait = icmp eq i32 %why, 3
  br i1 %exitwait, label %exitwait1, label %woken

exitwait1:
  %xt = call i32 @npkx_ld32(i32 %me, i32 7)
  call void @npkx_real_exit_wait(i64 %a, i32 %xt)
  ret i64 0

woken:
  %timedout = icmp eq i32 %why, 2
  %wr = select i1 %timedout, i64 -110, i64 0
  ret i64 %wr

; WAKE / WAKE_BITSET: the longest-blocked waiters on the word first
fwake0:
  %iswake = icmp eq i64 %op, 1
  %iswakeb = icmp eq i64 %op, 10
  %anywake = or i1 %iswake, %iswakeb
  br i1 %anywake, label %fwake, label %passthrough

fwake:
  %n2 = load i32, ptr @npkx_nslots
  br label %wloop

wloop:
  %woke = phi i64 [ 0, %fwake ], [ %woke1, %wfound ]
  %more = icmp slt i64 %woke, %c
  br i1 %more, label %wscan, label %wret

wscan:
  br label %wsloop

wsloop:
  %wi = phi i32 [ 0, %wscan ], [ %wi1, %wsnext ]
  %wbest = phi i32 [ -1, %wscan ], [ %wbest1, %wsnext ]
  %wbseq = phi i64 [ 0, %wscan ], [ %wbseq1, %wsnext ]
  %wsdone = icmp sge i32 %wi, %n2
  br i1 %wsdone, label %wdecide, label %wscheck

wscheck:
  %wst = call i32 @npkx_ld32(i32 %wi, i32 1)
  %wisf = icmp eq i32 %wst, 3
  br i1 %wisf, label %wsaddr, label %wsnext

wsaddr:
  %wad = call i64 @npkx_ld64(i32 %wi, i32 4)
  %wsame = icmp eq i64 %wad, %a
  br i1 %wsame, label %wscmp, label %wsnext

wscmp:
  %wsq = call i64 @npkx_ld64(i32 %wi, i32 8)
  %wnobest = icmp eq i32 %wbest, -1
  %wolder = icmp ult i64 %wsq, %wbseq
  %wtake = or i1 %wnobest, %wolder
  br label %wsnext

wsnext:
  %wptake = phi i1 [ false, %wscheck ], [ false, %wsaddr ], [ %wtake, %wscmp ]
  %wpseq = phi i64 [ 0, %wscheck ], [ 0, %wsaddr ], [ %wsq, %wscmp ]
  %wbest1 = select i1 %wptake, i32 %wi, i32 %wbest
  %wbseq1 = select i1 %wptake, i64 %wpseq, i64 %wbseq
  %wi1 = add i32 %wi, 1
  br label %wsloop

wdecide:
  %wnone = icmp eq i32 %wbest, -1
  br i1 %wnone, label %wret, label %wfound

wfound:
  call void @npkx_st32(i32 %wbest, i32 1, i32 2)
  call void @npkx_st32(i32 %wbest, i32 6, i32 1)
  %woke1 = add i64 %woke, 1
  br label %wloop

wret:
  ret i64 %woke

notfutex:
  %isepoll = icmp eq i64 %n, 281
  br i1 %isepoll, label %epoll, label %passthrough

; epoll_pwait: a real poll with timeout 0 under the baton; a blocked waiter is
; re-probed only after another thread stepped; the virtual deadline is fixed once
epoll:
  %neg = icmp slt i64 %d, 0
  %dms = mul i64 %d, 1000000
  %vnow2 = load i64, ptr @npkx_vnow
  %until0 = add i64 %vnow2, %dms
  %until = select i1 %neg, i64 -1, i64 %until0
  %zero = icmp eq i64 %d, 0
  br label %eploop

eploop:
  %pr = call i64 @npk_sys6(i64 %n, i64 %a, i64 %b, i64 %c, i64 0, i64 %e, i64 %f)
  %got = icmp ne i64 %pr, 0
  %answer = or i1 %got, %zero
  br i1 %answer, label %epret, label %epblock

epret:
  ret i64 %pr

epblock:
  %ex2 = call ptr @npk_exec()
  %exi2 = ptrtoint ptr %ex2 to i64
  call void @npkx_st64(i32 %me, i32 12, i64 %exi2)
  call void @npkx_st32(i32 %me, i32 1, i32 4)
  %steps = load i64, ptr @npkx_steps
  call void @npkx_st64(i32 %me, i32 9, i64 %steps)
  call void @npkx_st32(i32 %me, i32 6, i32 0)
  call void @npkx_st64(i32 %me, i32 5, i64 %until)
  call void @npkx_resched(i32 %me)
  call void @npkx_st32(i32 %me, i32 1, i32 1)
  call void @npkx_st64(i32 %me, i32 5, i64 -1)
  call void @npkx_run_pending(i32 %me)
  %why2 = call i32 @npkx_ld32(i32 %me, i32 6)
  %eptimeout = icmp eq i32 %why2, 2
  br i1 %eptimeout, label %eplast, label %eploop

eplast:
  %lr = call i64 @npk_sys6(i64 %n, i64 %a, i64 %b, i64 %c, i64 0, i64 %e, i64 %f)
  ret i64 %lr

passthrough:
  %r = call i64 @npk_sys6(i64 %n, i64 %a, i64 %b, i64 %c, i64 %d, i64 %e, i64 %f)
  ret i64 %r
}
