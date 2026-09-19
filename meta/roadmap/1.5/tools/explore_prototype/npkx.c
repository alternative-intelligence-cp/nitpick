/* PROTOTYPE shim for 1.5.7's planning -- outside every gate, never committed, never linked into anything that ships.
 * (The zero-dependency rule governs the artifact; the real shim is hand-written IR or Nitpick. This exists to MEASURE.)
 *
 * One thread runs at a time (the baton). Every synchronization step of the transformed floor calls in here first;
 * the scheduler (PCT, or a uniform random walk) decides who takes the next step. futex WAIT/WAKE, epoll_pwait and
 * clock_gettime are virtual; everything else passes through. A seed names a schedule.
 *
 * env: NPKX_SEED (u64, default 1)  NPKX_POLICY (0 = PCT, 1 = random walk)  NPKX_D (PCT depth, default 3)
 *      NPKX_K (PCT step estimate, default 2000)  NPKX_TRACE (1: print steps and the schedule's hash at exit)
 */
/* SPLIT-STACK AWARE, AS THE FLOOR AND THE IR SHIM ARE (D-305, 1.5.8 step 2): the explored program's functions carry
 * LLVM's split-stack prologue and call in here; without these two notes ld.lld rewrites those callers onto the slow path
 * (a trap: `__morestack` is the trap route) or refuses the link. */
__asm__(".section .note.GNU-split-stack,\"\",@progbits\n.previous\n"
        ".section .note.GNU-no-split-stack,\"\",@progbits\n.previous\n");
typedef unsigned long u64;
typedef long i64;
typedef unsigned int u32;
typedef int i32;
typedef unsigned char u8;

static inline i64 sc6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
    register i64 rax __asm__("rax") = n;
    register i64 rdi __asm__("rdi") = a;
    register i64 rsi __asm__("rsi") = b;
    register i64 rdx __asm__("rdx") = c;
    register i64 r10 __asm__("r10") = d;
    register i64 r8 __asm__("r8") = e;
    register i64 r9 __asm__("r9") = f;
    __asm__ volatile("syscall" : "+r"(rax) : "r"(rdi), "r"(rsi), "r"(rdx), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return rax;
}

#define SYS_read 0
#define SYS_write 1
#define SYS_open 2
#define SYS_close 3
#define SYS_gettid 186
#define SYS_futex 202
#define SYS_clock_gettime 228
#define SYS_exit_group 231
#define SYS_epoll_pwait 281
#define SYS_exit 60
#define SYS_munmap 11

#define MAXT 64
enum { FREE = 0, RUNNING, READY, B_FUTEX, B_EPOLL, ENDED, HELD };   /* HELD: 1.5.8 step 2c */
enum { R_NONE = 0, R_WOKEN, R_TIMEOUT, R_EXITWAIT, R_PROBE, R_SIGNAL };

struct slot {
    volatile i32 grant;
    i32 state;
    i64 tid;
    u64 prio;
    u64 addr;       /* B_FUTEX: the word */
    i64 deadline;   /* virtual ns; -1 = none */
    i32 reason;
    i32 exit_tid;   /* R_EXITWAIT: the tid the kernel will clear */
    u64 blocked_seq;
    u64 polled_at;
    i32 pending_sig;
    u64 exec;
    u32 waitval;
    u64 ctid;       /* the CHILD_CLEARTID word the kernel clears at the thread's exit (X-19) */
};

static struct slot S[MAXT];
static volatile i32 nslots;
static volatile i32 registered;
static i32 expected;
static volatile i32 dying;
static i32 inited;
static u64 rng = 1, seed = 1, steps, seq, hash = 1469598103934665603UL;
static i64 vnow = 1000000000L;
static i32 policy, depth = 3, trace;
static u64 kest = 2000, budget = 50000000UL;
static i32 last_site[MAXT];
static u64 change[16];
static i32 nchange;
static i64 preempt_at[4];
static i64 hold_at[4];                       /* HELD SITES (1.5.8 step 2c, DEF-67): -1 none */
static i32 held_by[4] = {-1, -1, -1, -1};    /* the slot held at each, -1 none */
static i64 ended_tids[MAXT];
static void (*sig_handler[65])(i32, void *, void *);
extern void *npk_exec(void);
static i32 oracle = 1;
static u64 map_next = 17592186044416UL;   /* 16 TiB: below every randomized mapping, above the static image */
static i32 nended;

static u64 next_rand(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

static void put(const char *s) { i64 n = 0; while (s[n]) n++; sc6(SYS_write, 2, (i64)s, n, 0, 0, 0); }
static void putn(u64 v) { char b[24]; i32 i = 23; b[i] = 0; if (!v) b[--i] = '0'; while (v) { b[--i] = '0' + (v % 10); v /= 10; } put(b + i); }

static u64 env_u64(const char *buf, i64 len, const char *key, u64 dflt) {
    i64 kl = 0; while (key[kl]) kl++;
    for (i64 i = 0; i + kl < len; i++) {
        if ((i == 0 || buf[i - 1] == 0)) {
            i64 j = 0; while (j < kl && buf[i + j] == key[j]) j++;
            if (j == kl && buf[i + j] == '=') {
                u64 v = 0; i64 p = i + kl + 1;
                while (p < len && buf[p] >= '0' && buf[p] <= '9') { v = v * 10 + (u64)(buf[p] - '0'); p++; }
                return v;
            }
        }
    }
    return dflt;
}

static void init(void) {
    static char buf[65536];
    inited = 1;
    i64 fd = sc6(SYS_open, (i64) "/proc/self/environ", 0, 0, 0, 0, 0);
    i64 len = 0;
    if (fd >= 0) { len = sc6(SYS_read, fd, (i64)buf, sizeof buf - 1, 0, 0, 0); sc6(SYS_close, fd, 0, 0, 0, 0, 0); if (len < 0) len = 0; }
    seed = env_u64(buf, len, "NPKX_SEED", 1);
    policy = (i32)env_u64(buf, len, "NPKX_POLICY", 0);
    depth = (i32)env_u64(buf, len, "NPKX_D", 3);
    kest = env_u64(buf, len, "NPKX_K", 2000);
    trace = (i32)env_u64(buf, len, "NPKX_TRACE", 0);
    budget = env_u64(buf, len, "NPKX_BUDGET", 50000000UL);
    oracle = (i32)env_u64(buf, len, "NPKX_ORACLE", 1);
    /* DIRECTED SITES (1.5.7 step 4, X-15, amended into the reference the same day): up to four atomic sites at which
       the arriving thread is demoted below every other, at every arrival -- a change point at a place. -1: none. */
    preempt_at[0] = (i64)env_u64(buf, len, "NPKX_PREEMPT1", (u64)-1);
    preempt_at[1] = (i64)env_u64(buf, len, "NPKX_PREEMPT2", (u64)-1);
    preempt_at[2] = (i64)env_u64(buf, len, "NPKX_PREEMPT3", (u64)-1);
    preempt_at[3] = (i64)env_u64(buf, len, "NPKX_PREEMPT4", (u64)-1);
    /* HELD SITES (1.5.8 step 2c, DEF-67, amended into the reference the same day): up to four atomic sites at which the
       FIRST arrival is held -- not scheduled -- until another thread arrives there and passes, keeping the baton
       through the site's instruction; a held thread is also released when nothing else can step, before virtual time
       may jump. A directed site demotes each arrival below the last and cannot reverse two arrivals at one place. */
    hold_at[0] = (i64)env_u64(buf, len, "NPKX_HOLD1", (u64)-1);
    hold_at[1] = (i64)env_u64(buf, len, "NPKX_HOLD2", (u64)-1);
    hold_at[2] = (i64)env_u64(buf, len, "NPKX_HOLD3", (u64)-1);
    hold_at[3] = (i64)env_u64(buf, len, "NPKX_HOLD4", (u64)-1);
    rng = seed * 0x9E3779B97F4A7C15UL + 0x1234567;
    if (!rng) rng = 1;
    for (i32 i = 0; i < 8; i++) next_rand();
    if (depth > 16) depth = 16;
    nchange = depth > 1 ? depth - 1 : 0;
    for (i32 i = 0; i < nchange; i++) change[i] = 1 + next_rand() % (kest ? kest : 1);
}

static i32 self(void) {
    i64 tid = sc6(SYS_gettid, 0, 0, 0, 0, 0, 0);
    for (i32 i = 0; i < nslots; i++) if (S[i].tid == tid && S[i].state != ENDED) return i;
    /* the main thread, at its first point */
    i32 me = nslots++;
    S[me].tid = tid; S[me].state = RUNNING; S[me].prio = (next_rand() << 8) + (u64)depth + 64; S[me].deadline = -1;
    return me;
}

static void wait_grant(i32 me) {
    while (!S[me].grant) sc6(SYS_futex, (i64)&S[me].grant, 128 /* WAIT|PRIVATE */, 0, 0, 0, 0);
    S[me].grant = 0;
}
static void grant(i32 t) { S[t].grant = 1; sc6(SYS_futex, (i64)&S[t].grant, 129 /* WAKE|PRIVATE */, 1, 0, 0, 0); }

static void die_tail(void);
static void die(const char *why) { put("npkx: "); put(why); die_tail(); }
/* A FALSE CALLER HYPOTHESIS (1.5.7 step 5, D-302, amended into the reference the same day): the explored floor's
   generated entry checkers call this with `<symbol>: <clause>` when a spec clause does not hold at a call. */
void npkx_assumption(const char *msg, i64 len) { dying = 1; put("npkx: ASSUMPTION "); sc6(SYS_write, 2, (i64)msg, len, 0, 0, 0); die_tail(); }
static void die_tail(void) {
    put(" seed="); putn(seed); put(" steps="); putn(steps); put("\n");
    for (i32 i = 0; i < nslots; i++) {
        put("  slot "); putn((u64)i); put(" state="); putn((u64)S[i].state); put(" addr="); putn(S[i].addr);
        put(" deadline="); putn((u64)S[i].deadline); put(" prio="); putn(S[i].prio); put(" last="); putn((u64)(u32)last_site[i]); put(" pend="); putn((u64)S[i].pending_sig); put("\n");
    }
    sc6(SYS_exit_group, 97, 0, 0, 0, 0, 0);
}

static i32 eligible(i32 t) {
    if (S[t].state == READY) return 1;
    if (S[t].state == B_EPOLL && steps > S[t].polled_at) return 1;   /* worth a probe: someone stepped since */
    return 0;
}

/* who takes the next step; -1 when nobody can */
static i32 pick(void) {
    i32 best = -1;
    if (policy == 1) {
        i32 n = 0, c[MAXT];
        for (i32 t = 0; t < nslots; t++) if (eligible(t)) c[n++] = t;
        if (!n) return -1;
        return c[next_rand() % (u64)n];
    }
    for (i32 t = 0; t < nslots; t++) if (eligible(t) && (best < 0 || S[t].prio > S[best].prio)) best = t;
    return best;
}

/* nobody can step: move virtual time to the earliest deadline, or report the deadlock */
static void lost_wake_oracle(void) {
    /* LOST-FUTEX-WAKE: a virtual waiter whose word no longer holds the value it waited on -- whoever changed it owed a wake */
    for (i32 t = 0; t < nslots; t++)
        if (S[t].state == B_FUTEX && *(volatile u32 *)S[t].addr != S[t].waitval) die("LOST FUTEX WAKE (a waiter's word changed and nobody woke it)");
    for (i32 t = 0; t < nslots; t++) {
        if ((S[t].state != B_FUTEX && S[t].state != B_EPOLL) || !S[t].exec) continue;
        u64 f = *(u64 *)(S[t].exec + 16);                  /* %npk.exec field 2: the sleeper list's head */
        for (i32 n = 0; f && n < 100000; n++) {
            if (*(i64 *)(f + 56) == 1) die("LOST WAKE (an executor sleeps on a task stamped due)");   /* %npk.hdr field 8 */
            f = *(u64 *)(f + 48);                          /* field 7: qnext */
        }
    }
}

static i32 advance(void) {
    i32 best = -1;
    if (oracle) lost_wake_oracle();
    for (i32 t = 0; t < nslots; t++)
        if ((S[t].state == B_FUTEX || S[t].state == B_EPOLL) && S[t].deadline >= 0 && (best < 0 || S[t].deadline < S[best].deadline)) best = t;
    if (best < 0) return -1;
    if (S[best].deadline > vnow) vnow = S[best].deadline;
    S[best].state = READY; S[best].reason = R_TIMEOUT;
    return best;
}

/* nobody can step: a held thread is released first (1.5.8 step 2c), the lowest held site first */
static i32 release_held(void) {
    for (i32 i = 0; i < 4; i++) if (held_by[i] >= 0) { i32 t = held_by[i]; S[t].state = READY; held_by[i] = -1; return t; }
    return -1;
}

/* give the baton away (or keep it). `me` < 0: the caller is leaving for good and does not wait. */
static void resched(i32 me) {
    for (;;) {
        i32 next = pick();
        if (next < 0) next = release_held();
        if (next < 0) next = advance();
        if (next < 0) {
            i32 live = 0;
            for (i32 t = 0; t < nslots; t++) if (S[t].state != ENDED) live++;
            if (me < 0 && live == 0) return;
            if (oracle) lost_wake_oracle();
            die("DEADLOCK");
        }
        if (S[next].state == B_EPOLL) S[next].reason = R_PROBE;
        if (next == me) { return; }
        grant(next);
        if (me < 0) return;
        wait_grant(me);
        return;
    }
}

static void settle_ended(void);
static void run_pending(i32 me) {
    settle_ended();
    if (S[me].pending_sig) { i32 sg = S[me].pending_sig; S[me].pending_sig = 0; S[me].state = RUNNING; if (sig_handler[sg]) sig_handler[sg](sg, 0, 0); }
}

/* FAIRNESS: strict priorities starve everyone behind a thread that never blocks (a loop "until another thread
 * stops me"). After FAIR consecutive steps by one thread while another could step, it drops below everything. */
static i32 last_runner = -1;
static u64 run_len, demote = 1UL << 20;
#define FAIR 4096

static u64 fair_extra;
static void count_step(i32 me, u64 what) {
    if (me == last_runner) run_len++; else { last_runner = me; run_len = 0; }
    /* THE FAIRNESS BOUND IS JITTERED (1.5.7 step 4, X-16, amended into the reference the same day): a fixed bound
       resonates with a periodic thread, which then rests at the same phase on every slice; 0..63 more steps, drawn
       from the seed's stream when the bound is reached. */
    if (run_len == FAIR) fair_extra = next_rand() & 63;
    if (policy == 0 && run_len > FAIR + fair_extra) {
        i32 other = 0;
        for (i32 t = 0; t < nslots; t++) if (t != me && (S[t].state == READY || S[t].state == B_EPOLL)) other = 1;
        if (other) { S[me].prio = --demote; run_len = 0; }   /* below every undemoted thread, and below every EARLIER demotion */
    }
    last_site[me] = (i32)what;
    steps++;
    hash = (hash ^ (((u64)me << 32) | what)) * 1099511628211UL;
    for (i32 i = 0; i < 4; i++) if (preempt_at[i] == (i64)what) { S[me].prio = --demote; run_len = 0; break; }   /* a directed site (X-15) */
    if (policy == 0) for (i32 i = 0; i < nchange; i++) if (steps == change[i]) S[me].prio = (1UL << 30) + (u64)(nchange - i);   /* below every initial priority, ABOVE every fairness demotion */
    if (steps > budget) die("STEP BUDGET (livelock?)");
}

/* EXECUTED HYPOTHESES (the idea of 1.5.7 SS2.4, two of them by hand): the spec's `(objects ...)` apartness, checked at
 * the call. small_free: the chunk, its two list neighbours and the class's partial head, 65536 each -- under the OLD
 * clause (unconditional) and the NEW one (`apart-when` the chunk was full). rq_push: f vs the tail, f vs exec. */
static u64 sf_calls, sf_old_viol, sf_new_viol, rq_calls, rq_viol;
static i32 apart(u64 a, u64 la, u64 b, u64 lb) { return !a || !b || !la || !lb || a + la <= b || b + lb <= a; }
void npkx_chk_small_free(u64 ip, u64 *cls_part) {
    u64 ch = ip - (ip % 65536), n = *(u64 *)(ch + 40), p = *(u64 *)(ch + 48), cls = *(u64 *)(ch + 8);
    u64 h = cls < 14 ? cls_part[cls] : 0, full = *(u64 *)(ch + 24) == 0;
    u64 r[4] = {ch, n, p, h};
    i32 bad = 0;
    for (i32 i = 0; i < 4; i++) for (i32 j = 0; j < i; j++) if (!apart(r[i], 65536, r[j], 65536)) bad = 1;
    sf_calls++;
    if (bad) { sf_old_viol++; if (full) sf_new_viol++; }
}
void npkx_chk_rq_push(u64 f) {
    u64 ex = (u64)npk_exec(), tail = ex ? *(u64 *)(ex + 8) : 0;
    rq_calls++;
    if (!apart(f, 56, tail, 56) || !apart(f, 56, ex, 16) || !apart(tail, 56, ex, 16)) rq_viol++;
}

void npkx_trap(void) { /* the trap route is explored like everything else (virtual signals) */ }

void npkx_point(i32 site) {
    if (dying) return;
    if (!inited) init();
    i32 me = self();
    count_step(me, (u64)(u32)site);
    for (i32 i = 0; i < 4; i++) if (hold_at[i] == (i64)(u32)site) {   /* a held site (1.5.8 step 2c) */
        if (held_by[i] < 0) { held_by[i] = me; S[me].state = HELD; resched(me); S[me].state = RUNNING; run_pending(me); return; }
        S[held_by[i]].state = READY; held_by[i] = -1;   /* the pass: this thread keeps the baton through the instruction */
        return;
    }
    S[me].state = READY;
    resched(me);
    S[me].state = RUNNING;
    run_pending(me);
}

void npkx_prespawn(void) { if (!inited) init(); (void)self(); expected = registered + 1; }

/* THE SETTLED END (1.5.7 step 5, X-19, amended into the reference the same day): the kernel clears a thread's
   CHILD_CLEARTID word after its last virtual step, racing whoever runs next, and npk_thread_join reads that word
   before it waits -- a step count depended on the race. The clone's ctid word is kept per slot; an ending thread
   queues it, and the next holder of the baton waits, really, until the kernel has cleared it. */
static i64 pend_tid[MAXT]; static u64 pend_addr[MAXT]; static i32 npend;
static void real_exit_wait(u64 addr, u32 tid);
static void settle_ended(void) { for (i32 i = 0; i < npend; i++) real_exit_wait(pend_addr[i], (u32)pend_tid[i]); npend = 0; }
void npkx_spawned(i64 tid, i64 ctid) {
    if (dying || tid <= 0) return;
    while (registered < expected) sc6(SYS_futex, (i64)&registered, 128, (i64)(expected - 1), 0, 0, 0);
    for (i32 t = 0; t < nslots; t++) if (S[t].tid == tid) { S[t].ctid = (u64)ctid; break; }
}

void npkx_begin(void) {
    /* the parent waits in npkx_spawned and holds the baton: this thread alone touches the shim */
    i32 me = nslots++;
    S[me].tid = sc6(SYS_gettid, 0, 0, 0, 0, 0, 0);
    S[me].state = READY; S[me].deadline = -1; S[me].grant = 0;
    S[me].prio = (next_rand() << 8) + (u64)depth + 64;
    registered++;
    sc6(SYS_futex, (i64)&registered, 129, 64, 0, 0, 0);
    wait_grant(me);
    S[me].state = RUNNING;
    run_pending(me);
}

void npkx_end(void) {
    if (dying) return;
    i32 me = self();
    i64 tid = S[me].tid;
    S[me].state = ENDED;
    ended_tids[nended++] = tid;
    if (S[me].ctid && npend < MAXT) { pend_tid[npend] = tid; pend_addr[npend] = S[me].ctid; npend++; }   /* X-19 */
    for (i32 t = 0; t < nslots; t++)
        if (S[t].state == B_FUTEX && *(volatile u32 *)S[t].addr == (u32)tid) { S[t].state = READY; S[t].reason = R_EXITWAIT; S[t].exit_tid = (i32)tid; }
    resched(-1);
}

static i64 rel_ns(i64 tsp) { if (!tsp) return -1; i64 *ts = (i64 *)tsp; return ts[0] * 1000000000L + ts[1]; }

static void real_exit_wait(u64 addr, u32 tid) {
    struct { i64 s, n; } ts = {0, 50000000};
    while (*(volatile u32 *)addr == tid) sc6(SYS_futex, (i64)addr, 0 /* WAIT, shared: the kernel's CLEARTID wake is */, (i64)tid, (i64)&ts, 0, 0);
}

i64 npkx_sys6(i64 n, i64 a, i64 b, i64 c, i64 d, i64 e, i64 f) {
    if (dying) return sc6(n, a, b, c, d, e, f);
    if (!inited) init();
    if (n == SYS_exit_group) {
        if (trace) { put("npkx-hyp: small_free calls="); putn(sf_calls); put(" OLD-clause-violated="); putn(sf_old_viol); put(" NEW-clause-violated="); putn(sf_new_viol);
                     put(" rq_push calls="); putn(rq_calls); put(" violated="); putn(rq_viol); put("\n"); }
        if (trace) { put("npkx: seed="); putn(seed); put(" steps="); putn(steps); put(" hash="); putn(hash); put(" vnow="); putn((u64)vnow); put("\n"); }
        return sc6(n, a, b, c, d, e, f);
    }
    i32 me = self();
    if (n == SYS_exit) { npkx_end(); return sc6(n, a, b, c, d, e, f); }
    count_step(me, 0x80000000UL | (u64)n);
    S[me].state = READY;
    resched(me);
    S[me].state = RUNNING;
    run_pending(me);
    if (n == 13 /* rt_sigaction */) { if (a > 0 && a < 65 && b) sig_handler[a] = *(void (**)(i32, void *, void *))b; return sc6(n, a, b, c, d, e, f); }
    if (n == 234 /* tgkill */) {
        for (i32 t = 0; t < nslots; t++) if (S[t].tid == b && S[t].state != ENDED) {
            S[t].pending_sig = (i32)c;
            if (S[t].state == B_FUTEX || S[t].state == B_EPOLL) { S[t].state = READY; S[t].reason = R_SIGNAL; }   /* a signal interrupts a blocking call */
            return 0;
        }
        return -3;   /* ESRCH */
    }
    if (n == SYS_clock_gettime) { i64 *ts = (i64 *)b; vnow += 1000; ts[0] = vnow / 1000000000L; ts[1] = vnow % 1000000000L; return 0; }
    /* THE VIRTUAL ADDRESS SPACE (1.5.7 step 1's finding, amended into the reference the same day): the floor's
     * control flow depends on the addresses the kernel hands out (`npk_chunk_new` over-maps and trims, and whether
     * its pre-trim munmap happens depends on the alignment of mmap's answer), so a step count that includes syscalls
     * depended on an address and two runs of one seed differed by one step under load. An anonymous private mapping
     * with no hint is placed at a bump pointer, 64 KiB-aligned, with MAP_FIXED_NOREPLACE; a collision moves the
     * pointer by 1 GiB and retries. Hinted, fixed or file-backed mappings pass through. */
    if (n == 9 && a == 0 && (d & 0x20) && !(d & 0x10)) {
        for (i32 tries = 0; tries < 64; tries++) {
            i64 hint = (i64)map_next;
            i64 r = sc6(n, hint, b, c, d | 0x100000, e, f);
            if (r == hint) { map_next = ((u64)hint + (u64)b + 65535UL) & ~65535UL; return r; }
            if ((u64)r <= (u64)-4096L) sc6(SYS_munmap, r, b, 0, 0, 0, 0);
            map_next = (u64)hint + (1UL << 30);
        }
        die("MMAP (no deterministic address)");
    }
    if (n == SYS_futex) {
        i32 op = (i32)(b & 0x7f);
        if (op == 0 || op == 9) {                       /* WAIT, WAIT_BITSET (absolute deadline) */
            u32 cur = *(volatile u32 *)a;
            if (cur != (u32)c) return -11;              /* EAGAIN */
            for (i32 i = 0; i < nended; i++) if ((u32)ended_tids[i] == cur) { real_exit_wait((u64)a, cur); return 0; }
            i64 rel = rel_ns(d);
            i64 dl = rel < 0 ? -1 : (op == 9 ? rel : vnow + rel);
            /* THE KERNEL NEVER SLEEPS PAST AN EXPIRED DEADLINE (1.5.7 step 4's finding, X-14, amended into the
               reference the same day): a wait whose absolute timeout is already due returns ETIMEDOUT at once --
               it arms an hrtimer that has already expired -- and never waits for anything else. Blocking it
               virtually until quiescence made a floor that reads a due stamp as a 1 ns deadline into a LOST-WAKE. */
            if (dl >= 0 && dl <= vnow) return -110;
            S[me].exec = (u64)npk_exec();
            S[me].waitval = (u32)c;
            S[me].state = B_FUTEX; S[me].addr = (u64)a; S[me].reason = R_NONE; S[me].blocked_seq = ++seq;
            S[me].deadline = dl;
            resched(me);
            S[me].state = RUNNING; S[me].deadline = -1;
            run_pending(me);
            if (S[me].reason == R_EXITWAIT) { real_exit_wait((u64)a, (u32)S[me].exit_tid); return 0; }
            return S[me].reason == R_TIMEOUT ? -110 : 0;
        }
        if (op == 1 || op == 10) {                      /* WAKE: the longest-blocked first */
            i64 woke = 0;
            while (woke < c) {
                i32 best = -1;
                for (i32 t = 0; t < nslots; t++)
                    if (S[t].state == B_FUTEX && S[t].addr == (u64)a && (best < 0 || S[t].blocked_seq < S[best].blocked_seq)) best = t;
                if (best < 0) break;
                S[best].state = READY; S[best].reason = R_WOKEN; woke++;
            }
            return woke;
        }
        return sc6(n, a, b, c, d, e, f);
    }
    if (n == SYS_epoll_pwait) {
        i64 until = d < 0 ? -1 : vnow + d * 1000000L;   /* the virtual deadline, fixed once */
        for (;;) {
            i64 r = sc6(n, a, b, c, 0, e, f);           /* a poll: never blocks */
            if (r != 0 || d == 0) return r;
            S[me].exec = (u64)npk_exec();
            S[me].state = B_EPOLL; S[me].polled_at = steps; S[me].reason = R_NONE; S[me].deadline = until;
            resched(me);
            S[me].state = RUNNING; S[me].deadline = -1;
            run_pending(me);
            if (S[me].reason == R_TIMEOUT) return sc6(n, a, b, c, 0, e, f);
        }
    }
    return sc6(n, a, b, c, d, e, f);
}
