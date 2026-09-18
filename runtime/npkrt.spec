; runtime/npkrt.spec -- the floor's specifications (1.5.6; D-288, D-289, D-290).
;
; This file describes `runtime/npkrt.ll` and is held to it by the belts of
; both runners: a name here that the floor lacks, a word the floor accesses
; that this file does not classify, a classification the floor's text
; contradicts, is a red run. SMT-LIB2 syntax throughout; `;` starts a comment.
;
; SECTIONS. `(shared ...)` -- the shared-state classification (D-290; landed
; 1.5.6 step 0) -- is this file's first section. The per-symbol
; specifications `(symbol @name ...)` (contracts, invariants, bounds, trap
; outcomes, frames, boundary promises; D-288) land at steps 3 and 4.
;
; THE SHARED-STATE RULE (D-290). Every word two threads can reach is either
; an ATOMIC access or a PLAIN access that a named happens-before edge orders.
; A `(word ...)` entry classifies one word -- a field of one of the four
; shared struct types (`%npk.hdr`, the frame header; `%npk.exec`, the
; executor; `%npk.chan`, a channel; `%npk.tls`, a thread's trampoline
; block), or a `global` -- as one of:
;
;   atomic               every access is atomic (`monotonic` at least); a
;                        plain access is a red run
;   mutex                a futex mutex word: never loaded or stored, only
;                        handed to npk_mx_lock/npk_mx_unlock
;   lock <L>             accessed only where lock L is held: a lock(L) call
;                        dominates the access and an unlock(L) post-dominates
;                        it in the same function, or the function is
;                        (under-lock @fn L) -- its callers hold L, and every
;                        call of it is checked to -- or (exempt @fn L "why")
;   publish <L>          written under L with a release (or seq_cst) store
;                        and read acquire elsewhere; a plain access only under L
;   owner-only           the executor's own thread and no other -- the
;                        reviewer's statement, listed in TCB.md SS5
;   born-before-publish  written where the frame, executor or block is born,
;                        before it is published (linked under a lock, pushed
;                        on a queue, handed to the clone) -- the reviewer's
;                        statement, listed in TCB.md SS5
;   once-before-threads  written at start or at the first allocation, before
;                        any second thread exists, never again -- the
;                        reviewer's statement, listed in TCB.md SS5
;   stated "why"         a word whose accesses follow a discipline the classes
;                        above do not name; the sentence is the classification
;                        and TCB.md SS5 carries it verbatim
;
; The locks: `chan.lock` is a channel's own word (slot 8) taken through
; npk_ch_lock/npk_ch_unlock; `ch-open-lock` and `heap-mx` are the two global
; mutex words taken through npk_mx_lock/npk_mx_unlock by name; `cell` is a
; synchronisation primitive's own mutex word, taken through npk_mx_lock by
; pointer; `waiter-list` names the discipline of the intrusive waiter lists
; threaded through a frame's `chan_next`: the list head's owner's mutex --
; a channel's `chan.lock` or a primitive's `cell` -- is held by every caller.
;
; Words of the four struct types that the FLOOR never touches (a frame's
; `state`, `result`, `join_head`, `awaitee`, `thread_tls`) belong to the
; emitted code's discipline: the spawning thread's, joined lexically (D-062,
; D-083); they are not classified here because the belt lists the floor's
; accesses, and a classification nothing accesses is refused as stale.

(shared
  (lock chan.lock (word %npk.chan 8) (by @npk_ch_lock @npk_ch_unlock))
  (lock ch-open-lock (word @npk_ch_open_lock) (by @npk_mx_lock @npk_mx_unlock))
  (lock heap-mx (word @npk_heap_mx) (by @npk_mx_lock @npk_mx_unlock))
  (lock cell (by @npk_mx_lock @npk_mx_unlock))

  ; --- %npk.hdr: the frame header the executor sees ----------------------------
  (word %npk.hdr 0 born-before-publish)    ; resume_fn: stamped at birth (ir_func.npk), read by npk_step on the owner
  (word %npk.hdr 2 atomic)                 ; windup: the joiner's release store, the owner's acquire load (DEF-44)
  (word %npk.hdr 5 owner-only)             ; sibling: the join list, walked by the spawning thread that built it
  (word %npk.hdr 7 owner-only)             ; qnext: the owner executor's ready queue and sleeper list
  (word %npk.hdr 8 atomic)                 ; wake_at: seq_cst everywhere -- the wake protocol's word
  (word %npk.hdr 10 lock waiter-list)      ; chan_next: the waiter lists, under the list head's owner's mutex
  (word %npk.hdr 11 born-before-publish)   ; owner: stamped at birth; a thread's root re-stamped by npk_thread_start before the clone (DEF-49)

  ; --- %npk.exec: one executor per thread ---------------------------------------
  (word %npk.exec 0 owner-only)            ; rq_head
  (word %npk.exec 1 owner-only)            ; rq_tail
  (word %npk.exec 2 owner-only)            ; sl_head
  (word %npk.exec 3 owner-only)            ; park_at
  (word %npk.exec 4 owner-only)            ; park_pending
  (word %npk.exec 5 atomic)                ; park_word: the futex word wakers set (seq_cst)
  (word %npk.exec 6 born-before-publish)   ; join_ns: written by npk_thread_start before the clone
  (word %npk.exec 7 born-before-publish)   ; grace_ns: likewise
  (word %npk.exec 8 owner-only)            ; chain
  (word %npk.exec 9 owner-only)            ; chain_n
  (word %npk.exec 10 owner-only)           ; windup_seen
  (word %npk.exec 11 owner-only)           ; epfd: created and read by the owner
  (word %npk.exec 12 atomic)               ; evfd: a release store at creation, acquire loads by rousers, the owner's own read monotonic
  (word %npk.exec 13 owner-only)           ; cur_task

  ; --- %npk.chan: a channel, under its own futex mutex --------------------------
  (word %npk.chan 0 lock chan.lock)        ; buf
  (word %npk.chan 1 lock chan.lock)        ; cap
  (word %npk.chan 2 lock chan.lock)        ; elem_size
  (word %npk.chan 3 lock chan.lock)        ; head
  (word %npk.chan 4 lock chan.lock)        ; tail
  (word %npk.chan 5 lock chan.lock)        ; count
  (word %npk.chan 6 atomic)                ; gen: read outside the lock by npk_ch_get, moved under it (DEF-45)
  (word %npk.chan 7 lock chan.lock)        ; closed
  (word %npk.chan 8 mutex)                 ; lock: the futex word itself
  (word %npk.chan 9 lock chan.lock)        ; recv_waiters: the list head; its list is threaded through chan_next
  (word %npk.chan 10 lock chan.lock)       ; send_waiters

  ; --- %npk.tls: a thread's trampoline block ------------------------------------
  (word %npk.tls 0 born-before-publish)    ; self
  (word %npk.tls 1 born-before-publish)    ; exec
  (word %npk.tls 2 born-before-publish)    ; root: read by npk_thread_entry on the child, after the clone
  (word %npk.tls 3 born-before-publish)    ; resume
  (word %npk.tls 4 stated "the join's futex word and the stop's target: written 0 by the creating thread before the clone -- or the pid, for the main thread at boot -- then by the kernel (PARENT_SETTID writes the tid, CHILD_CLEARTID zeroes it at exit), read by npk_thread_join with an atomic load, by the stop walk with an atomic load (D-291) and by the kernel's futex compare (DEF-48)")

  ; --- globals -----------------------------------------------------------------
  (word @npk_ch_tab publish ch-open-lock)  ; the table pointer: a release store under the open lock, acquire loads by readers
  (word @npk_ch_n publish ch-open-lock)    ; the count, published after the pointer
  (word @npk_ch_cap lock ch-open-lock)
  (word @npk_ch_fstk lock ch-open-lock)
  (word @npk_ch_fn lock ch-open-lock)
  (word @npk_ch_fcap lock ch-open-lock)
  (word @npk_frozen atomic)                ; D-063's flag: written by the trapping thread, read by every executor (DEF-46)
  (word @npk_in_failsafe atomic)           ; the failsafe holder: claimed by cmpxchg, read seq_cst (D-291)
  (word @npk_stopped atomic)               ; threads parked in the stop handler: atomicrmw by the handler, seq_cst reads by the winner's wait (D-291)
  (word @npk_pid once-before-threads "recorded at boot by getpid, read by the stop walk")
  ; @npk_fs_region is reached only through address arithmetic (its address
  ; taken by npk_fs_alloc, never loaded or stored by name), so no access is
  ; listed; its bytes are the holder's alone, after every other thread is parked.
  (word @npk_fs_bump stated "the failsafe region's bump offset (D-292): read and written only by the failsafe holder, after every other thread is parked (D-291)")
  (word @npk_thread_reg stated "sixty-four slots of two i64 words: the state word (+0) is atomic -- claimed by cmpxchg acq_rel, published release before the clone, read acquire by the stop walk and the retire; the tls word (+1) is written before the release publish and read after an acquire load of the state (D-291)")
  ; @npk_stop_word is the park-forever futex word: its address is handed to
  ; the futex syscall and it is written by nobody, so no access is listed.
  (word @npk_hsec once-before-threads "the heap secret: drawn once at the first allocation, which precedes every thread (npk_thread_start allocates its executor under the heap mutex before it clones), never rewritten -- npk_heap_init re-checks it and returns")
  (word @npk_chtab lock heap-mx)
  (word @npk_chtab_cap lock heap-mx)
  (word @npk_chtab_len lock heap-mx)
  (word @npk_lgtab lock heap-mx)
  (word @npk_lgtab_cap lock heap-mx)
  (word @npk_lgtab_len lock heap-mx)
  (word @npk_cls_part lock heap-mx)
  (word @npk_cls_full lock heap-mx)
  (word @npk_hs_allocated lock heap-mx)
  (word @npk_hs_live lock heap-mx)
  (word @npk_hs_peak lock heap-mx)
  (word @npk_hs_count lock heap-mx)
  (word @npk_hs_on once-before-threads "set by npk_hs_arm at _start from the environment, before main runs")
  (word @npk_environ_slice once-before-threads "written at _start")
  ; @npk_main_exec, the main thread's executor block, is reached only through
  ; the TLS pointer npk_exec reads, so its words are the %npk.exec rows above.
  (word @npk_quarantine once-before-threads "a debug instrument that ships 0 and is written by nobody")
  (word @npk_wildx_live atomic)            ; the executable-page count: read-modify-writes across threads (DEF-50)
  (word @npk_driver_reg stated "sixteen slots of four i32 words: the state word (+0) is atomic -- claimed by cmpxchg acq_rel, published release, read acquire by the walkers; pid (+1) is diagnostic, written by the spawning thread after the clone and read by nobody; pidfd (+2) is prefilled -1 before the release publish and then written by the kernel during the clone (CLONE_PIDFD), read after an acquire load of the state; the fourth word is padding")

  ; --- functions whose callers hold the lock --------------------------------------
  (under-lock @npk_ch_wait_link waiter-list)
  (under-lock @npk_ch_wait_unlink waiter-list)
  (under-lock @npk_ch_wake_one waiter-list)
  (under-lock @npk_ch_wake_all waiter-list)
  (under-lock @npk_small_alloc heap-mx)
  (under-lock @npk_small_free heap-mx)
  (under-lock @npk_small_check heap-mx)
  (under-lock @npk_chunk_new heap-mx)
  (under-lock @npk_chunk_guard_check heap-mx)
  (under-lock @npk_large_new heap-mx)
  (under-lock @npk_large_check heap-mx)
  (under-lock @npk_chtab_find heap-mx)
  (under-lock @npk_chtab_insert heap-mx)
  (under-lock @npk_lg_entry heap-mx)
  (under-lock @npk_lg_find heap-mx)
  (under-lock @npk_lg_insert heap-mx)
  (under-lock @npk_lg_remove heap-mx)
  (under-lock @npk_ch_push heap-mx)
  (under-lock @npk_ch_unlink heap-mx)
  (under-lock @npk_hs_note_alloc heap-mx)
  (under-lock @npk_hs_note_free heap-mx)
  (under-lock @npk_hs_note_resize heap-mx)
  (under-lock @npk_heap_init heap-mx)

  ; --- the exemptions, each with its reason (TCB.md SS5 carries them) --------------
  (exempt @npk_ch_open chan.lock "a fresh channel's fields are written before its slot is published by the count's release store, so no reader can reach them; a reused slot's fields are written under its own lock")
  (exempt @npk_wild_live_count heap-mx "runs at a successful exit after every thread the program spawned is joined (threads are lexical, D-083), and in the trap route's exit only once the process is stopping")
  (exempt @npk_wild_release_all heap-mx "the controlled shutdown's release, called from failsafe or before main's exit, when every thread is joined or stopped")
  (exempt @npk_hs_report heap-mx "the NPK_HEAP_STATS line at exit: read without the mutex by design -- a thread that trapped inside the allocator still holds it, and the line is a diagnostic, never a verdict")
)

; =============================================================================
; THE SYMBOLS' SPECIFICATIONS (D-288; 1.5.6 step 3). One `(symbol @name ...)`
; section per specified define. Names: a parameter by its IR name without `%`
; (an aggregate parameter's fields as `a.0`, `a.1`...); a register likewise
; (a loop's header phi in an invariant; a value on a path in an ensures);
; `result` (an aggregate's members `result.0`...); `mem` and `mem2`, the
; memory before and after (at a loop header, `mem2` is the memory THERE);
; `trap` and `trap_code`; `exec` and `tls`, the per-thread constants
; (`npk_exec()`, `npk_tls_self()`); a global by its name without `@` as its
; ADDRESS, its contents read through `mem`; `(free j Int)` a free symbol of
; the section, `(free x Addr)` one that ranges over the address space
; [0, 2^64) -- the universal an invariant's frame conjunct or the frame row
; states is over addresses, and the free `x` is the frame row's point when
; the section declares one; `(load8 M a)`, `(load16 M a)`, `(load32 M a)`,
; `(load64 M a)` the little-endian reassembly of the bytes at `a` (wrapped at
; 2^64, as the IR's addresses are) in memory M; `(s8 x)`, `(s32 x)`,
; `(s64 x)`, `(s128 x)` the signed reading of a word; `(xor64 a b)`,
; `(and64 a b)`, `(or64 a b)` the 64-bit bit operations. A name SMT-LIB owns
; (`abs`, `mod`, `store`...) or the writer uses is spelled `|r:name|`.
; Clauses: `(requires P)` -- a hypothesis the floor's callers keep, a row at
; every `(summary)` call; `(objects (lo len) ...)` -- the caller's objects:
; each range lies in the address space and the listed ranges are pairwise
; disjoint (a null or an empty range is no object), hypotheses here and rows
; at a summary call, and the ranges an `(ensures-fresh LEN)` callee's block
; is disjoint from; `(ensures P)` -- one row, on the non-trapping paths;
; `(ensures-trap P)` -- one row, `trap` iff P (absent: a symbol with trap
; sites claims `not trap`; `(ensures-trap true)` ends every path that reaches
; a call of the symbol); `(ensures-fresh LEN)` -- a summary allocator's
; promise: `result` is a block of LEN bytes disjoint from the caller's
; objects and from every earlier fresh block, the objects unchanged;
; `(frame (lo len) ...)` -- one row: outside every range memory is unchanged
; (absent: memory is unchanged everywhere, a row); with the atom `objects`
; (`(frame objects)`, `(frame (lo len) objects)`) the claim is over the
; caller's declared objects only -- a symbol that allocates changes the heap's
; own words and its fresh blocks, which no caller can name, and promises the
; objects it was handed (none declared: nothing claimed, no row); at a
; `(summary)` call the atom names the CALLER's objects, which the callee
; leaves unchanged outside its ranges; `(loop LABEL (objects (lo len) ...)
; ...)` -- ranges that are objects on every visit of the header (the loop's
; live buffer): their facts join the invariant (proven at init, preserved),
; an allocation inside the loop is disjoint from them and a call's `objects`
; frame keeps them; `(loop LABEL (invariant
; I) | (unroll N) | (unroll N exact))` with `(inst SYM e [SYM e]...)` -- an
; extra instantiation of the invariant hypothesis at e (several pairs
; substitute together); an unrolled loop's rows are marked `[<=N]`, and
; `exact` adds the row that proves the bound; `(summary)` -- callers assume
; this section instead of inlining the body; `(residue "why")` -- what the
; profile does not decide, copied into TCB.md §5; `(boundary "what")` -- a
; syscall-class symbol's promise at the kernel boundary, assumed at its
; `(summary)` calls, never translated. Addresses wrap at 2^64 exactly as the
; IR's do, so a symbol that walks a range states the range is in the address
; space -- `(objects (p n))`, or `(<= (+ p n) 18446744073709551616)` as a
; requires where a weaker condition than disjointness is the true
; precondition (memcpy's forward copy).
;
; THE SINGLE-INSTANCE RULE. An invariant's universal is written over the
; section's free symbols; the writer asserts the hypothesis at those symbols
; and demands the conclusion at the same symbols. A step that needs the
; hypothesis at another point names it: memcpy's step reads the source byte
; at `src + i`, which the frame conjunct covers only at `x`, so `(inst x (+
; src i))` instantiates it there.

; --- the byte helpers LLVM calls behind a program's back ---------------------

(symbol @memcpy
  (free j Int) (free x Addr)
  ; a forward copy is exact when the destination lies below the source or
  ; wholly past it; the intrinsic (llvm.memcpy) requires the stronger
  ; disjointness, and its call sites are held to that by the translator
  (requires (or (<= dst src) (<= (+ src n) dst)))
  (requires (<= (+ dst n) 18446744073709551616))
  (requires (<= (+ src n) 18446744073709551616))
  (loop head
    (invariant (and (<= 0 i) (<= i n)
                    (=> (and (<= 0 j) (< j i)) (= (load8 mem2 (+ dst j)) (load8 mem (+ src j))))
                    (=> (or (< x dst) (>= x (+ dst i))) (= (load8 mem2 x) (load8 mem x)))))
    (inst x (+ src i)))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ dst j)) (load8 mem (+ src j)))))
  (ensures (= result dst))
  (frame (dst n)))

(symbol @memset
  (free j Int) (free x Addr)
  (requires (<= (+ dst n) 18446744073709551616))
  (loop head
    (invariant (and (<= 0 i) (<= i n)
                    (=> (and (<= 0 j) (< j i)) (= (load8 mem2 (+ dst j)) (mod c 256)))
                    (=> (or (< x dst) (>= x (+ dst i))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ dst j)) (mod c 256))))
  (ensures (= result dst))
  (frame (dst n)))

(symbol @npk_zero
  (free j Int) (free x Addr)
  (requires (<= (+ p n) 18446744073709551616))
  (loop loop
    (invariant (and (<= 0 i) (<= i n)
                    (=> (and (<= 0 j) (< j i)) (= (load8 mem2 (+ p j)) 0))
                    (=> (or (< x p) (>= x (+ p i))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ p j)) 0)))
  (frame (p n)))

; --- the string helpers ---------------------------------------------------------

(symbol @npk_string_equals
  (free j Int)
  (requires (<= (+ a.0 a.1) 18446744073709551616))
  (requires (<= (+ b.0 b.1) 18446744073709551616))
  (loop loop
    (invariant (and (<= 0 i) (<= i a.1) (= a.1 b.1)
                    (=> (and (<= 0 j) (< j i)) (= (load8 mem (+ a.0 j)) (load8 mem (+ b.0 j)))))))
  (ensures (or (= result 0) (= result 1)))
  (ensures (=> (= result 1) (and (= a.1 b.1) (=> (and (<= 0 j) (< j a.1)) (= (load8 mem (+ a.0 j)) (load8 mem (+ b.0 j)))))))
  ; `i` is the loop's counter where the mismatch was found
  (ensures (=> (= result 0) (or (not (= a.1 b.1)) (and (<= 0 i) (< i a.1) (not (= (load8 mem (+ a.0 i)) (load8 mem (+ b.0 i)))))))))

(symbol @npk_string_from_bytes
  (ensures (= result.0 p))
  (ensures (= result.1 n))
  (ensures (= result.2 0)))

(symbol @npk_environ
  (ensures (= result.0 (load64 mem npk_environ_slice)))
  (ensures (= result.1 (load64 mem (+ npk_environ_slice 8)))))

(symbol @npk_frozen_get
  (ensures (= result (load32 mem npk_frozen))))

; --- the heap's mixing functions: the formulas ------------------------------------

(symbol @npk_m_chunk  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 14387278266329264483))))
(symbol @npk_m_live   (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 5885026092677834074))))
(symbol @npk_m_freed  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 17865445471836548325))))
(symbol @npk_m_large  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 1884440546999092433))))
(symbol @npk_m_livew  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 8639445676566075373))))
(symbol @npk_m_largew (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 4436545153374750491))))
(symbol @npk_m_guard  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 7651035258233467253))))
(symbol @npk_m_wildx  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 4358112156723783278))))
(symbol @npk_m_flive  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 17844352565267513931))))
(symbol @npk_m_ffree  (ensures (= result (xor64 (xor64 (load64 mem npk_hsec) a) 999621991244018563))))

(symbol @memmove
  (free j Int) (free x Addr)
  (requires (<= (+ dst n) 18446744073709551616))
  (requires (<= (+ src n) 18446744073709551616))
  ; the forward path is memcpy's body inlined (its loop `head` under its own
  ; invariant); the backward loop copies from the top down
  (loop bhead
    (invariant (and (<= 0 i) (<= i n)
                    (=> (and (<= i j) (< j n)) (= (load8 mem2 (+ dst j)) (load8 mem (+ src j))))
                    (=> (or (< x (+ dst i)) (>= x (+ dst n))) (= (load8 mem2 x) (load8 mem x)))))
    (inst x (+ src (- i 1))))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ dst j)) (load8 mem (+ src j)))))
  (ensures (= result dst))
  (frame (dst n)))

; --- the 128-bit division core and its four wrappers --------------------------------

(symbol @npk_udivmod128
  (summary)
  ; the restoring loop: the remainder stays below the divisor, the one fact a
  ; caller needs of it, by invariant (its step decides at 3,093 rlimit)
  (loop loop (invariant (=> (not (= b 0)) (< r b))))
  (ensures (=> (not (= b 0)) (< result.1 b)))
  (residue "the division identity a = q*b + r is not decided under the profile: it needs 2^i, which the IR does not compute, and this writer invents no ghost variable (D-288); the b = 0 answer (q all ones, r = a) and the b = 1 answer (q = a, r = 0) are not decided either -- with the loop unwound 127 times (its bound proven exact in 0.03 s) every row over the whole computation exhausts the rlimit (q all ones unknown after 54 s, b = 1 after 149 s, r = a after 643 s, and even r < b after 652 s, which the invariant's step decides at 3,093), and the multiplier's equivalence is unknown in QF_BV at every width (measured 2026-09-11)"))

(symbol @__udivti3
  (ensures (= result q)))

(symbol @__umodti3
  (ensures (= result r))
  (ensures (=> (not (= b 0)) (< result b))))

(symbol @__divti3
  ; |a| / |b| by the core, then the sign rule: negative iff exactly one operand is
  (ensures (=> (= (< (s128 a) 0) (< (s128 b) 0)) (= result q)))
  (ensures (=> (not (= (< (s128 a) 0) (< (s128 b) 0))) (= result (mod (- 0 q) 340282366920938463463374607431768211456)))))

(symbol @__modti3
  ; the remainder takes the dividend's sign
  (ensures (=> (not (< (s128 a) 0)) (= result r)))
  (ensures (=> (< (s128 a) 0) (= result (mod (- 0 r) 340282366920938463463374607431768211456))))
  (ensures (=> (not (= b 0)) (< r ba))))

; --- fmod: the special values decided, the reduction residue ---------------------------
;
; The two reduction loops carry the trivial invariant: the profile decides
; nothing about their result -- `|result| < |b|` is `unknown` with both loops
; unwound 2 times (6.9 s) and 8 times (12.5 s double, 7.3 s single) -- so the
; loops are cut and the claim is residue, while the special values, decided on
; the paths before the loops, are rows.

(symbol @fmod
  (loop outer (invariant true))
  (loop scale (invariant true))
  (ensures (=> (or (fp.isNaN a) (fp.isNaN b) (fp.isZero b) (fp.isInfinite a)) (fp.isNaN result)))
  (ensures (=> (and (not (fp.isNaN a)) (not (fp.isNaN b)) (not (fp.isZero b)) (not (fp.isInfinite a)) (fp.isInfinite b)) (= result a)))
  (residue "the reduction's result is not decided under the profile: |result| < |b| on the finite path is unknown with the loops unwound 2 and 8 times (z3's floating-point theory bit-blasts every fmul and fsub of the chain; measured 2026-09-11); the special values are rows"))

(symbol @fmodf
  (loop outer (invariant true))
  (loop scale (invariant true))
  (ensures (=> (or (fp.isNaN a) (fp.isNaN b) (fp.isZero b) (fp.isInfinite a)) (fp.isNaN result)))
  (ensures (=> (and (not (fp.isNaN a)) (not (fp.isNaN b)) (not (fp.isZero b)) (not (fp.isInfinite a)) (fp.isInfinite b)) (= result a)))
  (residue "as fmod's: the reduction's result is not decided under the profile (unknown with the loops unwound 2 and 8 times, measured 2026-09-11); the special values are rows"))

; =============================================================================
; STEP 4 -- the allocator's helpers and checks, the arenas, the frame arena,
; the heap-stats writers, the chain, the executor's words, and the trap route.
; The struct offsets a spec names are the x86_64 layout of the floor's own
; types: `%npk.exec` -- rq_head 0, rq_tail 8, sl_head 16, park_at 24,
; park_pending 32, join_ns 40, grace_ns 48, chain 56 (eight i32), chain_n 88,
; windup_seen 92, epfd 96, evfd 100, cur_task 104; `%npk.hdr` -- resume_fn 0,
; state 8, windup 12, result 16, join_head 24, sibling 32, awaitee 40, qnext
; 48, wake_at 56, thread_tls 64, chan_next 72, owner 80. A chunk header keeps
; its class index at +8, its free count at +24, its hint at +32, its list
; links at +40 (next) and +48 (prev), its watermark at +56, its bitmap at +64.
; An arena's block is slab 0, gens 8, cap 16, top 24, free-head 32; a frame
; arena's is head 0, cur 8, cur-off 16, buckets 24, bucket-count 40.

; --- the executor's words (through `exec`, the per-thread constant) --------------

(symbol @npk_chain_reset
  (ensures (= (load32 mem2 (+ exec 56)) site))
  (ensures (= (load32 mem2 (+ exec 88)) 1))
  (frame ((+ exec 56) 4) ((+ exec 88) 4)))

(symbol @npk_chain_push
  ; the ring keeps eight sites; the depth counts every push
  (ensures (= (load32 mem2 (+ exec 88)) (mod (+ d 1) 4294967296)))
  (ensures (=> (< d 8) (= (load32 mem2 (+ exec 56 (* 4 d))) site)))
  (frame ((+ exec 56) 36)))

(symbol @npk_chain_depth
  (ensures (= result (load32 mem (+ exec 88)))))

(symbol @npk_chain_site
  (ensures (=> (< (s32 i) 0) (= result 0)))
  (ensures (=> (and (>= (s32 i) 0) (>= i (ite (< d 8) d 8))) (= result 0)))
  (ensures (=> (and (>= (s32 i) 0) (< i (ite (< d 8) d 8))) (= result (load32 mem (+ exec 56 (* 4 i)))))))

(symbol @npk_park_until
  (ensures (= (load64 mem2 (+ exec 24)) at))
  (ensures (= (load32 mem2 (+ exec 32)) 1))
  (frame ((+ exec 24) 12)))

(symbol @npk_park_take
  (ensures (=> (= (load32 mem (+ exec 32)) 0) (= result 0)))
  (ensures (=> (not (= (load32 mem (+ exec 32)) 0)) (= result (load64 mem (+ exec 24)))))
  (ensures (= (load32 mem2 (+ exec 32)) 0))
  (frame ((+ exec 32) 4)))

(symbol @npk_windup_grace (ensures (= result (load64 mem (+ exec 48)))))
(symbol @npk_join_deadline (ensures (= result (load64 mem (+ exec 40)))))
(symbol @npk_windup_note
  (ensures (= (load32 mem2 (+ exec 92)) w))
  (frame ((+ exec 92) 4)))
(symbol @npk_task_done
  (ensures (= result (ite (= (load64 mem (+ f 56)) 18446744073709551615) 1 0))))

(symbol @npk_rq_push
  ; the frame's qnext cleared, the frame appended: the tail's qnext (or the
  ; head, of an empty queue) names it, and the tail is it
  (requires (not (= f 0)))
  ; WHY THE THREE ARE APART (1.5.6c step 2, lead E-1; the rows need f/exec and f/tail -- measured, pair by pair):
  ; f vs the TAIL -- a double push would make them one frame and the body would write `f.qnext = f`, a cycle the
  ;   executor spins in -- cannot happen: A FRAME IS IN AT MOST ONE OF {a run queue, a sleeper list, running} (the
  ;   two lists thread through the SAME word, `qnext` at +48), and only its owning executor's thread moves it
  ;   (D-032: a task never migrates), so no interleaving is involved. The four callers, read: npk_sl_wake_due
  ;   DETACHES the whole sleeper list first and pushes each due frame of it once (a frame reaches the sleeper list
  ;   only after npk_rq_pop took it off the queue); npk_thread_entry and the emitter's `main` shim push a root
  ;   once onto an EMPTY queue (the tail is null: no object); emit_spawn pushes the child frame it has just carved.
  ;   npk_windup_all does not push (it stamps and rouses; the owner's sweep pushes); an awaited child is driven
  ;   inline and is never queued.
  ; f and the tail vs exec -- a frame is frame-arena memory, the executor block is the thread's own mapping.
  ; WHO KEEPS IT: npk_sl_wake_due and npk_thread_entry are not translated and emitted code calls this symbol
  ;   (exported): nothing proves the invariant at any call. It is argued here and is what 1.5.7's explorer can
  ;   carry as an executable assertion ("f is on no run queue").
  (objects (f 56) (exec 16) ((load64 mem (+ exec 8)) 56))
  (ensures (= (load64 mem2 (+ f 48)) 0))
  (ensures (= (load64 mem2 (+ exec 8)) f))
  (ensures (=> (= t 0) (= (load64 mem2 exec) f)))
  (ensures (=> (not (= t 0)) (= (load64 mem2 (+ t 48)) f)))
  (frame ((+ f 48) 8) (exec 16) ((+ t 48) 8)))

(symbol @npk_rq_pop
  ; WHY THE TWO ARE APART (1.5.6c step 2; the rows need the pair): the head frame is frame-arena memory, the
  ; executor block is the thread's own mapping -- two allocations, whatever the caller does. WHO KEEPS IT:
  ; npk_step, the one caller, is not translated; nothing to keep beyond "the head is a frame".
  (objects (exec 16) ((load64 mem exec) 56))
  (ensures (=> (= h 0) (= result 0)))
  (ensures (=> (not (= h 0)) (and (= result h) (= (load64 mem2 exec) nx) (= (load64 mem2 (+ h 48)) 0))))
  (ensures (=> (and (not (= h 0)) (= nx 0)) (= (load64 mem2 (+ exec 8)) 0)))
  (frame (exec 16) ((+ h 48) 8)))

(symbol @npk_ch_wait_link
  ; the current task is linked at the head of the list at hp
  ; WHY THE THREE ARE APART (1.5.6c step 2; the rows need hp vs the task's frame): hp is a waiter-list head inside
  ; a primitive's cell -- a channel's slot in the table, a Mutex/RwLock/CondVar/Barrier cell wherever its binding
  ; lives. The one place it can lie INSIDE a frame is a frame-resident primitive (a lock declared in an `async`
  ; body), and there the layout separates them: a frame is its 88-byte header and THEN its slots, so hp >= frame +
  ; 88 while the object is the header's first 80 bytes (through `wlink` at +72; `owner` at +80 is outside it).
  ; True of the task's own frame and of any other. The executor block is the thread's own mapping. WHO KEEPS IT:
  ; the seven wait entries, none translated -- the argument is the layout's, which the emitter fixes
  ; (`fnem_slot_gep` indexes slots from field 12).
  (objects (hp 8) (exec 112) ((load64 mem (+ exec 104)) 80))
  (requires (not (= (load64 mem (+ exec 104)) 0)))
  (ensures (= (load64 mem2 (+ fr 72)) (load64 mem hp)))
  (ensures (= (load64 mem2 hp) fr))
  (frame (hp 8) ((+ fr 72) 8)))

(symbol @npk_ch_wait_unlink
  ; the walk past the first link is the list's reachability, which no
  ; first-order clause states; the head case is decided, the rest residue
  (loop loop (unroll 8))
  ; the head and the first nine waiters -- as far as nine copies walk -- are objects (frames, null at the end); the current task is one of them
  ; WHY THEY ARE APART (1.5.6c step 2; the rows stand on it -- with every pair fact deleted the head row is refuted):
  ; THE WAITERS PAIRWISE DISTINCT -- a list with a frame on it twice would be a cycle. A task is linked AT MOST
  ;   ONCE: every wait entry UNLINKS the current task on entry (it is re-entered at every resume) before it may
  ;   LINK it, a task waits on one primitive at a time, and the list is changed only under the primitive's own lock
  ;   (the channel's lock, the futex mutex). The current task is NOT declared beside the nine -- it is one of them
  ;   when it is linked at all, and this symbol is called, routinely, when it is not.
  ; A LIST SHORTER THAN NINE: past the null the "next waiter" is `load64` at address 72, memory the body never
  ;   reads (the walk stops at null). The hypothesis constrains the MODEL's bytes there and no caller's: every real
  ;   state extends to a model that satisfies it (those bytes zero: every later range null, no object).
  ; A WAITER vs hp, vs exec: as npk_ch_wait_link's -- a header's first 80 bytes against a cell that is never in a
  ;   header, and against the thread's own mapping.
  ; WHO KEEPS IT: the eight callers, none translated. "fr is not already linked" is what 1.5.7's explorer can carry.
  (objects (hp 8) (exec 112) ((load64 mem hp) 80) ((load64 mem (+ (load64 mem hp) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 80) ((load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 80))
  (requires (not (= hp 0)))
  (requires (not (= (load64 mem (+ exec 104)) 0)))
  (ensures (=> (= (load64 mem hp) fr) (and (= (load64 mem2 hp) (load64 mem (+ fr 72))) (= (load64 mem2 (+ fr 72)) 0))))
  ; the bounded walk writes the head, the current task's link, or the link of one of the first nine waiters
  (frame (hp 8) ((+ (load64 mem (+ exec 104)) 72) 8) ((+ (load64 mem hp) 72) 8) ((+ (load64 mem (+ (load64 mem hp) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 72) 8) ((+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem (+ (load64 mem hp) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 72)) 72) 8))
  (residue "an unlink from deeper in the waiter list than the head is not decided: the walk is bounded at eight links and the list's reachability is no first-order clause; the head case is a row"))

; --- the heap-stats counters and their writers ---------------------------------

(symbol @npk_hs_note_alloc
  (ensures (= (load64 mem2 npk_hs_allocated) (mod (+ (load64 mem npk_hs_allocated) n) 18446744073709551616)))
  (ensures (= (load64 mem2 npk_hs_count) (mod (+ (load64 mem npk_hs_count) 1) 18446744073709551616)))
  (ensures (= (load64 mem2 npk_hs_live) (mod (+ (load64 mem npk_hs_live) n) 18446744073709551616)))
  (ensures (= (load64 mem2 npk_hs_peak) (ite (> (mod (+ (load64 mem npk_hs_live) n) 18446744073709551616) (load64 mem npk_hs_peak)) (mod (+ (load64 mem npk_hs_live) n) 18446744073709551616) (load64 mem npk_hs_peak))))
  (frame (npk_hs_allocated 8) (npk_hs_count 8) (npk_hs_live 8) (npk_hs_peak 8)))

(symbol @npk_hs_note_free
  (ensures (= (load64 mem2 npk_hs_live) (mod (- (load64 mem npk_hs_live) n) 18446744073709551616)))
  (frame (npk_hs_live 8)))

(symbol @npk_hs_note_resize
  (ensures (= (load64 mem2 npk_hs_live) (mod (+ (load64 mem npk_hs_live) (- new old)) 18446744073709551616)))
  (ensures (=> (> (s64 d) 0) (= (load64 mem2 npk_hs_allocated) (mod (+ (load64 mem npk_hs_allocated) d) 18446744073709551616))))
  (ensures (=> (not (> (s64 d) 0)) (= (load64 mem2 npk_hs_allocated) (load64 mem npk_hs_allocated))))
  (ensures (=> (> (s64 d) 0) (= (load64 mem2 npk_hs_peak) (ite (> l1 (load64 mem npk_hs_peak)) l1 (load64 mem npk_hs_peak)))))
  (frame (npk_hs_allocated 8) (npk_hs_live 8) (npk_hs_peak 8)))

(symbol @npk_hs_put_str
  (free j Int) (free x Addr)
  (requires (<= (+ line pos n) 18446744073709551616))
  (requires (<= (+ s n) 18446744073709551616))
  (requires (or (<= (+ s n) (+ line pos)) (<= (+ line pos n) s)))
  (loop loop
    (invariant (and (<= 0 k) (<= k n)
                    (=> (and (<= 0 j) (< j k)) (= (load8 mem2 (+ line pos j)) (load8 mem (+ s j))))
                    (=> (or (< x (+ line pos)) (>= x (+ line pos k))) (= (load8 mem2 x) (load8 mem x)))))
    (inst x (+ s k)))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ line pos j)) (load8 mem (+ s j)))))
  (ensures (= result (mod (+ pos n) 18446744073709551616)))
  (frame ((+ line pos) n)))

(symbol @npk_hs_arm
  ; the flag is set, or untouched, and nothing else moves; WHICH entry arms
  ; it is an existential over the environment's entries, bounded here
  (loop scan (unroll 64))
  (loop cmp (unroll 14 exact))
  (ensures (or (= (load64 mem2 npk_hs_on) (load64 mem npk_hs_on)) (= (load64 mem2 npk_hs_on) 1)))
  (frame (npk_hs_on 8))
  (residue "which environment entry arms the flag is not decided: a match is an existential over the entries, and the scan is bounded at 64 entries (its rows read [<=64])"))

; --- the arenas and the frame arena ---------------------------------------------------

(symbol @npk_arena_at
  (requires (not (= a 0)))
  (requires (=> (> (load64 mem (+ a 24)) 0) (not (= (load64 mem (+ a 8)) 0))))
  ; the slot answers iff its index is below the top and its generation matches
  ; WHY THE ARENA VALUE AND ITS GENERATION TABLE ARE APART (1.5.6c step 2; the three arena sections share the
  ; clause -- this one's rows do not need the pair, npk_arena_free's and npk_arena_reset's do): `a` is the arena
  ; VALUE, the 40 bytes `{slab, gens, cap, top, free_head}` the program's binding holds; `gens` is a block
  ; npk_arena_make takes from the allocator and never hands out, so no binding can lie inside it. And `a` is in
  ; no SLAB either, its own included: an arena is an owning type and an owning element is refused at `arena_make`
  ; (D-183), so an arena value is never an arena element. WHO KEEPS IT: emitted code, the only caller (exported)
  ; -- nothing proves it at the call; the argument is the type checker's refusal plus the allocator's freshness.
  (objects (a 40) ((load64 mem (+ a 8)) (* 4 (load64 mem (+ a 24)))))
  (ensures (=> (>= idx (load64 mem (+ a 24))) (= result 0)))
  (ensures (=> (and (< idx (load64 mem (+ a 24))) (not (= (load32 mem (+ (load64 mem (+ a 8)) (* 4 idx))) gen))) (= result 0)))
  (ensures (=> (and (< idx (load64 mem (+ a 24))) (= (load32 mem (+ (load64 mem (+ a 8)) (* 4 idx))) gen))
               (= result (mod (+ (load64 mem a) (* idx stride)) 18446744073709551616)))))

(symbol @npk_arena_free
  (requires (not (= a 0)))
  (requires (=> (> (load64 mem (+ a 24)) 0) (not (= (load64 mem (+ a 8)) 0))))
  ; a live slot's generation moves on and, below the retirement cap, the
  ; slot joins the free list; a stale handle answers 1 and changes nothing
  ; THE PAIR: npk_arena_at's argument (the rows here need it). THE TWO `requires` BELOW ARE APARTNESS TOO, of the
  ; same kind and with the same keeper (1.5.6c step 2): the freed SLOT's first word -- where the free list's link
  ; is written -- lies outside the generation table and outside the arena value. The slab and `gens` are two
  ; blocks of the allocator's, and the value is in no slab (above).
  (objects (a 40) ((load64 mem (+ a 8)) (* 4 (load64 mem (+ a 24)))))
  (requires (or (<= (+ (mod (+ (load64 mem a) (* idx stride)) 18446744073709551616) 8) (load64 mem (+ a 8)))
                (<= (+ (load64 mem (+ a 8)) (* 4 (load64 mem (+ a 24)))) (mod (+ (load64 mem a) (* idx stride)) 18446744073709551616))))
  (requires (or (<= (+ (mod (+ (load64 mem a) (* idx stride)) 18446744073709551616) 8) a)
                (<= (+ a 40) (mod (+ (load64 mem a) (* idx stride)) 18446744073709551616))))
  (ensures (=> (= p 0) (= result 1)))
  (ensures (=> (not (= p 0)) (and (= result 0) (= (load32 mem2 (+ gens (* 4 idx))) (mod (+ g 1) 4294967296)))))
  (ensures (=> (and (not (= p 0)) (< (mod (+ g 1) 4294967296) 4294967294))
               (and (= (load64 mem2 (+ a 32)) idx) (= (load64 mem2 p) (load64 mem (+ a 32))))))
  (frame ((+ (load64 mem (+ a 8)) (* 4 idx)) 4) ((+ a 32) 8) ((mod (+ (load64 mem a) (* idx stride)) 18446744073709551616) 8)))

(symbol @npk_arena_reset
  (requires (not (= a 0)))
  (requires (=> (> (load64 mem (+ a 24)) 0) (not (= (load64 mem (+ a 8)) 0))))
  (free j Int) (free x Addr)
  ; THE PAIR: npk_arena_at's argument (the rows here need it -- the loop writes the table and then the value's
  ; `top` and `free_head`).
  (objects (a 40) ((load64 mem (+ a 8)) (* 4 (load64 mem (+ a 24)))))
  (loop head
    (invariant (and (<= 0 i) (<= i top)
                    (=> (and (<= 0 j) (< j i)) (= (mod (load32 mem2 (+ gi (* 4 j))) 2) 1))
                    (=> (or (< x gi) (>= x (+ gi (* 4 i)))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (and (<= 0 j) (< j (load64 mem (+ a 24)))) (= (mod (load32 mem2 (+ (load64 mem (+ a 8)) (* 4 j))) 2) 1)))
  (ensures (= (load64 mem2 (+ a 24)) 0))
  (ensures (= (load64 mem2 (+ a 32)) 18446744073709551615))
  (frame ((load64 mem (+ a 8)) (* 4 (load64 mem (+ a 24)))) ((+ a 24) 16)))

(symbol @npk_frame_bucket
  (free j Int)
  (requires (<= (+ (load64 mem (+ fi 24)) (* 8 (load64 mem (+ fi 40)))) 18446744073709551616))
  (loop head
    (invariant (and (<= 0 i) (<= i bc)
                    (=> (and (<= 0 j) (< j i)) (not (= (load64 mem (+ bi (* 8 j))) rn))))))
  (ensures (=> (>= (s64 result) 0) (and (< result (load64 mem (+ fi 40))) (= (load64 mem (+ (load64 mem (+ fi 24)) (* 8 result))) rn))))
  (ensures (=> (< (s64 result) 0) (=> (and (<= 0 j) (< j (load64 mem (+ fi 40)))) (not (= (load64 mem (+ (load64 mem (+ fi 24)) (* 8 j))) rn))))))

(symbol @npk_frame_drain
  (objects (fe 48))
  (ensures (= (load64 mem2 (+ fe 8)) (load64 mem fe)))
  (ensures (= (load64 mem2 (+ fe 16)) 16))
  (ensures (= (load64 mem2 (+ fe 40)) 0))
  (frame ((+ fe 8) 16) ((+ fe 40) 8)))

; --- the heap's tables and lists -------------------------------------------------------

(symbol @npk_lg_entry
  (ensures (= result (mod (+ (load64 mem npk_lgtab) (* 32 idx)) 18446744073709551616))))

(symbol @npk_chtab_find
  (summary)
  (free j Int) (free k Int)
  ; the chunk table: `len` entries of eight bytes at `tab`, strictly ascending
  ; (a binary search over the addresses of the chunks, kept sorted by insert)
  (requires (< (load64 mem npk_chtab_len) 9223372036854775808))
  (requires (<= (+ (load64 mem npk_chtab) (* 8 (load64 mem npk_chtab_len))) 18446744073709551616))
  (requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len)))
                (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))
  (loop head
    (invariant (and (<= 0 lo) (<= lo hi) (<= hi len)
                    (=> (and (<= 0 j) (< j lo)) (< (load64 mem (+ tab (* 8 j))) a))
                    (=> (and (<= hi j) (< j len)) (> (load64 mem (+ tab (* 8 j))) a))
                    (=> (and (<= 0 j) (< j k) (< k len)) (< (load64 mem (+ tab (* 8 j))) (load64 mem (+ tab (* 8 k)))))))
    (inst k mid)
    (inst j mid k j))
  (ensures (=> (>= (s64 result) 0) (and (< result (load64 mem npk_chtab_len)) (= (load64 mem (+ (load64 mem npk_chtab) (* 8 result))) a))))
  (ensures (=> (< (s64 result) 0) (=> (and (<= 0 j) (< j (load64 mem npk_chtab_len))) (not (= (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) a))))))

(symbol @npk_lg_find
  (free j Int) (free k Int)
  ; the large table: `len` entries of thirty-two bytes at `tab`, the first
  ; word of each the block's address, strictly ascending
  (requires (< (load64 mem npk_lgtab_len) 9223372036854775808))
  (requires (<= (+ (load64 mem npk_lgtab) (* 32 (load64 mem npk_lgtab_len))) 18446744073709551616))
  (requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_lgtab_len)))
                (< (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) (load64 mem (+ (load64 mem npk_lgtab) (* 32 k))))))
  (loop head
    (invariant (and (<= 0 lo) (<= lo hi) (<= hi len)
                    (=> (and (<= 0 j) (< j lo)) (< (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) p))
                    (=> (and (<= hi j) (< j len)) (> (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) p))
                    (=> (and (<= 0 j) (< j k) (< k len)) (< (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) (load64 mem (+ (load64 mem npk_lgtab) (* 32 k)))))))
    (inst k mid)
    (inst j mid k j))
  (ensures (=> (>= (s64 result) 0) (and (< result (load64 mem npk_lgtab_len)) (= (load64 mem (+ (load64 mem npk_lgtab) (* 32 result))) p))))
  (ensures (=> (< (s64 result) 0) (=> (and (<= 0 j) (< j (load64 mem npk_lgtab_len))) (not (= (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) p))))))

(symbol @npk_lg_remove
  (free x Addr)
  ; every entry above idx moves down one place, the count drops by one
  (requires (< idx (load64 mem npk_lgtab_len)))
  (requires (not (= (load64 mem npk_lgtab) 0)))
  ; WHY THE TABLE'S BLOCK IS APART FROM THE TWO WORDS THAT NAME IT (1.5.6c step 2; the rows need both pairs, not
  ; the third -- two globals): the block is a MAPPING (npk_hmap, from npk_heap_init and from npk_lg_insert's growth:
  ; `mmap` at address 0 with no MAP_FIXED, as the floor's only other `mmap`, npk_wildx_alloc's, is), and
  ; `npk_lgtab`/`npk_lgtab_len` are words of the image; the kernel places no such
  ; mapping over memory already mapped (the kernel-effect table's `maps` row: the mapping is FRESH). WHO KEEPS IT:
  ; npk_dalloc, not translated -- and nothing a caller does can break it; it is the kernel's promise.
  (objects ((load64 mem npk_lgtab) (* 32 (load64 mem npk_lgtab_len))) (npk_lgtab 8) (npk_lgtab_len 8))
  (loop head
    (invariant (and (<= idx i) (< i len)
                    (=> (and (<= (+ (load64 mem npk_lgtab) (* 32 idx)) x) (< x (+ (load64 mem npk_lgtab) (* 32 i)))) (= (load8 mem2 x) (load8 mem (+ x 32))))
                    (=> (or (< x (+ (load64 mem npk_lgtab) (* 32 idx))) (>= x (+ (load64 mem npk_lgtab) (* 32 i)))) (= (load8 mem2 x) (load8 mem x)))))
    (inst x (+ x 32)))
  (ensures (=> (and (<= (+ (load64 mem npk_lgtab) (* 32 idx)) x) (< x (+ (load64 mem npk_lgtab) (* 32 (- (load64 mem npk_lgtab_len) 1))))) (= (load8 mem2 x) (load8 mem (+ x 32)))))
  (ensures (= (load64 mem2 npk_lgtab_len) (- (load64 mem npk_lgtab_len) 1)))
  (frame ((+ (load64 mem npk_lgtab) (* 32 idx)) (* 32 (- (load64 mem npk_lgtab_len) 1 idx))) (npk_lgtab_len 8)))

(symbol @npk_ch_push
  ; the chunk becomes the head of the list at hp: its next the old head, its
  ; prev 0, the old head's prev the chunk
  (requires (not (= ch 0)))
  ; WHY THE THREE ARE APART (1.5.6c step 2; the rows need ch vs hp and ch vs the old head): hp is a cell of a
  ; global table (`npk_cls_part`/`npk_cls_full`) and a chunk is a mapping -- the kernel's promise, as
  ; npk_lg_remove's. THE CHUNK vs THE OLD HEAD: a chunk is pushed only when it is on NO list -- fresh from
  ; npk_chunk_new (npk_small_alloc's `fresh`), or just taken off the OTHER list by npk_ch_unlink (partial -> full
  ; in npk_small_alloc's `tofull`, full -> partial in npk_small_free's `topart`) -- and A CHUNK IS ON AT MOST ONE
  ; LIST, so the list's head is another chunk, or null.
  ; WHO KEEPS IT: npk_small_free inlines this body, so there its own rows cover it under ITS hypotheses
  ; (`apart-when` the chunk was full); npk_small_alloc is not translated and nothing proves it at its calls.
  ; "a chunk is on at most one list" is leg A's (D-233) and what 1.5.7's explorer can assert.
  (objects (hp 8) (ch 56) ((load64 mem hp) 56))
  (ensures (= (load64 mem2 hp) ch))
  (ensures (= (load64 mem2 (+ ch 40)) (load64 mem hp)))
  (ensures (= (load64 mem2 (+ ch 48)) 0))
  (ensures (=> (not (= (load64 mem hp) 0)) (= (load64 mem2 (+ (load64 mem hp) 48)) ch)))
  (frame (hp 8) ((+ ch 40) 16) ((+ (load64 mem hp) 48) 8)))

(symbol @npk_ch_unlink
  ; the chunk leaves the list at hp: its prev's next (or the head) becomes its
  ; next, its next's prev becomes its prev, its own links are cleared
  (requires (not (= ch 0)))
  ; WHY THE FOUR ARE APART (1.5.6c step 2; the rows need five of the six pairs): hp against a chunk as
  ; npk_ch_push's. THE CHUNK vs ITS NEIGHBOURS, AND THE NEIGHBOURS vs EACH OTHER: the lists are LINEAR and
  ; null-terminated (npk_ch_push sets `prev` 0 and links at the head; nothing links a tail to a head), so a chunk
  ; is not its own neighbour and its `next` and `prev` are two other chunks, or null -- a chunk on no list has
  ; both null, and no object. WHO KEEPS IT: as npk_ch_push's.
  (objects (hp 8) (ch 56) ((load64 mem (+ ch 40)) 56) ((load64 mem (+ ch 48)) 56))
  (ensures (=> (= (load64 mem (+ ch 48)) 0) (= (load64 mem2 hp) (load64 mem (+ ch 40)))))
  (ensures (=> (not (= (load64 mem (+ ch 48)) 0)) (= (load64 mem2 (+ (load64 mem (+ ch 48)) 40)) (load64 mem (+ ch 40)))))
  (ensures (=> (not (= (load64 mem (+ ch 40)) 0)) (= (load64 mem2 (+ (load64 mem (+ ch 40)) 48)) (load64 mem (+ ch 48)))))
  (ensures (= (load64 mem2 (+ ch 40)) 0))
  (ensures (= (load64 mem2 (+ ch 48)) 0))
  (frame (hp 8) ((+ ch 40) 16) ((+ (load64 mem (+ ch 48)) 40) 8) ((+ (load64 mem (+ ch 40)) 48) 8)))

(symbol @npk_small_free
  (free j Int) (free k Int) (free x Addr)
  ; a validated small block returns to its chunk: the payload poisoned (0xAA,
  ; D-183's instrument), the header stamped freed, the live counter down by
  ; the block's size; off quarantine, the slot's bit set in the chunk's
  ; bitmap, the free count up, the hint lowered to the word, and a chunk that
  ; was full moves from the class's full list to its partial list
  ; (npk_ch_unlink and npk_ch_push inlined: their stores are cheaper to the
  ; solver than their frames as templates); the validation is
  ; npk_small_check's summary, whose own rows prove it
  (requires (not (= (- ip (mod ip 65536)) 0)))
  ; the validation's own preconditions (the chunk table sorted, in space -- leg A's facts), restated here so its call's rows are its
  (requires (< (load64 mem npk_chtab_len) 9223372036854775808))
  (requires (<= (+ (load64 mem npk_chtab) (* 8 (load64 mem npk_chtab_len))) 18446744073709551616))
  (requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len)))
                (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))
  (requires (<= (+ (- ip (mod ip 65536)) 65536) 18446744073709551616))
  ; THE LIST OBJECTS ARE OBJECTS ONLY WHEN THE BODY TOUCHES THEM (1.5.6c step 0, lead E-1). The chunk's two
  ; list neighbours and the class's partial-list head were declared apart from the chunk and from one another
  ; UNCONDITIONALLY -- false for the ordinary free: a chunk that is already partial is ON the partial list, so
  ; when it is that list's head the chunk and the head are one 64 KiB, and when it is second its `prev` is the
  ; head (npk_small_alloc allocates from the partial head, so a LIFO free lands exactly there). For those frees
  ; every row of this section assumed a falsehood and claimed nothing. The body reads the three only in
  ; `topart` -- when the chunk WAS FULL (its free count at entry, the word at +24, is zero) and moves from the
  ; full list to the partial one -- and there the clause is true: a chunk is on one list and the lists are
  ; linear, so its full-list neighbours and the partial head are three other chunks, or null. So each of the
  ; three is `apart-when` that count is zero: in the address space always (a list word is null or a chunk),
  ; set apart only where the body reads it. WHO KEEPS IT: npk_dalloc, the one caller, a boundary symbol --
  ; nothing proves it at the call; that a chunk is on one list and the lists are linear is leg A's (D-233).
  (objects ((- ip (mod ip 65536)) 65536)
           ((load64 mem (+ (- ip (mod ip 65536)) 40)) 65536 apart-when (= (load64 mem (+ (- ip (mod ip 65536)) 24)) 0))
           ((load64 mem (+ (- ip (mod ip 65536)) 48)) 65536 apart-when (= (load64 mem (+ (- ip (mod ip 65536)) 24)) 0))
           ((load64 mem (+ npk_cls_part (* 8 (load64 mem (+ (- ip (mod ip 65536)) 8))))) 65536 apart-when (= (load64 mem (+ (- ip (mod ip 65536)) 24)) 0))
           (npk_cls_part 112) (npk_cls_full 112) (npk_hs_live 8) (npk_quarantine 8) (npk_hsec 8) (npk_cls_size 112) (npk_cls_data 112))
  (loop poison (invariant (and (<= 0 pi) (<= pi cls)
                    (= (load64 mem2 npk_hs_live) (mod (- (load64 mem npk_hs_live) hsn) 18446744073709551616))
                    (=> (and (<= 0 j) (< j pi)) (= (load8 mem2 (+ ip j)) 170))
                    (=> (and (or (< x ip) (>= x (+ ip pi))) (or (< x npk_hs_live) (>= x (+ npk_hs_live 8)))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (and (<= 0 j) (< j cls)) (= (load8 mem2 (+ ip j)) 170)))
  ; the stamp is the freed mix the body computes (`fm`, npk_m_freed's formula in its own section)
  (ensures (= (load64 mem2 (- ip 8)) fm))
  (ensures (= (load64 mem2 npk_hs_live) (mod (- (load64 mem npk_hs_live) (load64 mem (- ip 16))) 18446744073709551616)))
  (ensures (=> (= (load64 mem npk_quarantine) 0)
               (and (= (load64 mem2 wa) (or64 (load64 mem wa) mask))
                    (= (load64 mem2 (+ ch 24)) (mod (+ (load64 mem (+ ch 24)) 1) 18446744073709551616))
                    (= (load64 mem2 (+ ch 32)) (ite (< w (load64 mem (+ ch 32))) w (load64 mem (+ ch 32)))))))
  (ensures (=> (and (= (load64 mem npk_quarantine) 0) (= (load64 mem (+ ch 24)) 0))
               (and (= (load64 mem2 (+ npk_cls_part (* 8 ci))) ch)
                    (= (load64 mem2 (+ ch 40)) (load64 mem (+ npk_cls_part (* 8 ci))))
                    (= (load64 mem2 (+ ch 48)) 0))))
  (frame (ip cls) ((- ip 8) 8) (npk_hs_live 8) (wa 8) ((+ ch 24) 16) ((+ ch 40) 16)
         ((+ npk_cls_full (* 8 ci)) 8) ((+ npk_cls_part (* 8 ci)) 8)
         ((+ (load64 mem (+ ch 40)) 48) 8) ((+ (load64 mem (+ ch 48)) 40) 8)
         ((+ (load64 mem (+ npk_cls_part (* 8 ci))) 48) 8))
  (residue "the five ensures and the frame are not decided under the profile (unknown at the budget; at ten times it two answer unknown and four do not answer within forty minutes): each claim over the state after the poison loop must separate its address from the tail's stores -- the stamp, the counter, the bitmap word, the chunk's counters and the list words -- through the class table's geometry, a fourteen-way case over the class index the solver does not finish; the validation's four preconditions, the poison loop's two rows and the no-trap row discharge"))

(symbol @npk_hs_put_dec
  ; the digits of v, most significant first, at line[pos..pos+K) with K its decimal
  ; length; the two loops unwound in full (twenty digits cover 2^64, the bounds
  ; proven exact) and ONE ROW PER LENGTH: with the length fixed the exit copy, the
  ; copy loop's trip count and every address are numerals, and the row's twenty
  ; digit claims decide by evaluation where one claim over an unknown length did not
  (loop loop (unroll 19 exact))
  (loop copy (unroll 19 exact))
  (objects ((+ line pos) 20))
  ; the position after the digits is a word (the twenty-digit row is refutable at pos = 2^64 - 20 otherwise: the result wraps to 0)
  (requires (< (+ pos 20) 18446744073709551616))
  (ensures (=> (< v 10) (and (= result (+ pos 1)) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod v 10))))))
  (ensures (=> (and (>= v 10) (< v 100)) (and (= result (+ pos 2)) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10) 10))))))
  (ensures (=> (and (>= v 100) (< v 1000)) (and (= result (+ pos 3)) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100) 10))))))
  (ensures (=> (and (>= v 1000) (< v 10000)) (and (= result (+ pos 4)) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000) 10))))))
  (ensures (=> (and (>= v 10000) (< v 100000)) (and (= result (+ pos 5)) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000) 10))))))
  (ensures (=> (and (>= v 100000) (< v 1000000)) (and (= result (+ pos 6)) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100000) 10))))))
  (ensures (=> (and (>= v 1000000) (< v 10000000)) (and (= result (+ pos 7)) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000000) 10))))))
  (ensures (=> (and (>= v 10000000) (< v 100000000)) (and (= result (+ pos 8)) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000000) 10))))))
  (ensures (=> (and (>= v 100000000) (< v 1000000000)) (and (= result (+ pos 9)) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100000000) 10))))))
  (ensures (=> (and (>= v 1000000000) (< v 10000000000)) (and (= result (+ pos 10)) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000000000) 10))))))
  (ensures (=> (and (>= v 10000000000) (< v 100000000000)) (and (= result (+ pos 11)) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000000000) 10))))))
  (ensures (=> (and (>= v 100000000000) (< v 1000000000000)) (and (= result (+ pos 12)) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100000000000) 10))))))
  (ensures (=> (and (>= v 1000000000000) (< v 10000000000000)) (and (= result (+ pos 13)) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000000000000) 10))))))
  (ensures (=> (and (>= v 10000000000000) (< v 100000000000000)) (and (= result (+ pos 14)) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000000000000) 10))))))
  (ensures (=> (and (>= v 100000000000000) (< v 1000000000000000)) (and (= result (+ pos 15)) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100000000000000) 10))))))
  (ensures (=> (and (>= v 1000000000000000) (< v 10000000000000000)) (and (= result (+ pos 16)) (= (load8 mem2 (+ line pos 15)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000000000000000) 10))))))
  (ensures (=> (and (>= v 10000000000000000) (< v 100000000000000000)) (and (= result (+ pos 17)) (= (load8 mem2 (+ line pos 16)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 15)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000000000000000) 10))))))
  (ensures (=> (and (>= v 100000000000000000) (< v 1000000000000000000)) (and (= result (+ pos 18)) (= (load8 mem2 (+ line pos 17)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 16)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 15)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 100000000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 1000000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 10000000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 100000000000000000) 10))))))
  (ensures (=> (and (>= v 1000000000000000000) (< v 10000000000000000000)) (and (= result (+ pos 19)) (= (load8 mem2 (+ line pos 18)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 17)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 16)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 15)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 100000000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 1000000000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 10000000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 100000000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 1000000000000000000) 10))))))
  (ensures (=> (>= v 10000000000000000000) (and (= result (+ pos 20)) (= (load8 mem2 (+ line pos 19)) (+ 48 (mod v 10))) (= (load8 mem2 (+ line pos 18)) (+ 48 (mod (div v 10) 10))) (= (load8 mem2 (+ line pos 17)) (+ 48 (mod (div v 100) 10))) (= (load8 mem2 (+ line pos 16)) (+ 48 (mod (div v 1000) 10))) (= (load8 mem2 (+ line pos 15)) (+ 48 (mod (div v 10000) 10))) (= (load8 mem2 (+ line pos 14)) (+ 48 (mod (div v 100000) 10))) (= (load8 mem2 (+ line pos 13)) (+ 48 (mod (div v 1000000) 10))) (= (load8 mem2 (+ line pos 12)) (+ 48 (mod (div v 10000000) 10))) (= (load8 mem2 (+ line pos 11)) (+ 48 (mod (div v 100000000) 10))) (= (load8 mem2 (+ line pos 10)) (+ 48 (mod (div v 1000000000) 10))) (= (load8 mem2 (+ line pos 9)) (+ 48 (mod (div v 10000000000) 10))) (= (load8 mem2 (+ line pos 8)) (+ 48 (mod (div v 100000000000) 10))) (= (load8 mem2 (+ line pos 7)) (+ 48 (mod (div v 1000000000000) 10))) (= (load8 mem2 (+ line pos 6)) (+ 48 (mod (div v 10000000000000) 10))) (= (load8 mem2 (+ line pos 5)) (+ 48 (mod (div v 100000000000000) 10))) (= (load8 mem2 (+ line pos 4)) (+ 48 (mod (div v 1000000000000000) 10))) (= (load8 mem2 (+ line pos 3)) (+ 48 (mod (div v 10000000000000000) 10))) (= (load8 mem2 (+ line pos 2)) (+ 48 (mod (div v 100000000000000000) 10))) (= (load8 mem2 (+ line pos 1)) (+ 48 (mod (div v 1000000000000000000) 10))) (= (load8 mem2 (+ line pos 0)) (+ 48 (mod (div v 10000000000000000000) 10))))))
  (frame ((+ line pos) 20)))

; --- the validations: pure until they trap -----------------------------------------------

(symbol @npk_chunk_guard_check
  (summary)
  ; the chunk's two guard words at the class's guard offset carry the mix
  (requires (< ci 14))
  (requires (<= (+ ch 65536) 18446744073709551616))
  (ensures-trap (or (not (= (load64 mem (+ ch (load64 mem (+ npk_cls_guard (* 8 ci)))))
                            (xor64 (xor64 (load64 mem npk_hsec) (+ ch (load64 mem (+ npk_cls_guard (* 8 ci))))) 7651035258233467253)))
                    (not (= (load64 mem (+ ch (load64 mem (+ npk_cls_guard (* 8 ci))) 8))
                            (xor64 (xor64 (load64 mem npk_hsec) (+ ch (load64 mem (+ npk_cls_guard (* 8 ci))) 8)) 7651035258233467253))))))

(symbol @npk_small_check
  (summary)
  (free j Int) (free k Int)
  ; a small allocation's address is validated against the whole heap before
  ; anything is dereferenced (D-150): its chunk is in the table, the chunk's
  ; magic and class index hold, the address sits on a slot boundary inside
  ; the class's data region, below the watermark, with its free bit clear, a
  ; live header stamp, and the chunk's guards intact -- else the heap trap
  (requires (< (load64 mem npk_chtab_len) 9223372036854775808))
  (requires (<= (+ (load64 mem npk_chtab) (* 8 (load64 mem npk_chtab_len))) 18446744073709551616))
  (requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len)))
                (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))
  (requires (<= (+ (- ip (mod ip 65536)) 65536) 18446744073709551616))
  (ensures (= result (- ip (mod ip 65536))))
  ; what validated MEANS, for the callers that take this section as a summary
  ; (npk_small_free): the class index below 14, the block's header at or past
  ; the class's data region, on a stride boundary, its slot below the chunk's
  ; watermark and the class's slot count -- the geometry the class tables
  ; fix, so the slot lies inside the chunk past the header and the bitmap
  (ensures (>= ip 16))
  (ensures (< (load64 mem (+ result 8)) 14))
  (ensures (<= (+ result (load64 mem (+ npk_cls_data (* 8 (load64 mem (+ result 8)))))) (- ip 16)))
  ; the slot arithmetic in the floor's own wrapped shape, so the solver reads the
  ; same terms the body computes (the offset and the stride are wrapped words)
  (ensures (= (mod (mod (- (mod (- ip 16) 18446744073709551616) (mod (+ result (load64 mem (+ npk_cls_data (* 8 (load64 mem (+ result 8)))))) 18446744073709551616)) 18446744073709551616) (mod (+ (load64 mem (+ npk_cls_size (* 8 (load64 mem (+ result 8))))) 16) 18446744073709551616)) 0))
  (ensures (< (div (mod (- (mod (- ip 16) 18446744073709551616) (mod (+ result (load64 mem (+ npk_cls_data (* 8 (load64 mem (+ result 8)))))) 18446744073709551616)) 18446744073709551616) (mod (+ (load64 mem (+ npk_cls_size (* 8 (load64 mem (+ result 8))))) 16) 18446744073709551616)) (load64 mem (+ result 56))))
  (ensures (< (div (mod (- (mod (- ip 16) 18446744073709551616) (mod (+ result (load64 mem (+ npk_cls_data (* 8 (load64 mem (+ result 8)))))) 18446744073709551616)) 18446744073709551616) (mod (+ (load64 mem (+ npk_cls_size (* 8 (load64 mem (+ result 8))))) 16) 18446744073709551616)) (load64 mem (+ npk_cls_slots (* 8 (load64 mem (+ result 8)))))))
  (ensures-trap (or miss (not mok) cibad neg misfit oob virgin isfree (not hok))))

(symbol @npk_large_check
  ; the header stamp is live, and the two guard words past the rounded size
  (requires (<= (+ ip (and64 (+ (load64 mem (+ e 24)) 15) 18446744073709551600) 16) 18446744073709551616))
  (ensures-trap (or (not (or (= (load64 mem (- ip 8)) (xor64 (xor64 (load64 mem npk_hsec) (- ip 16)) 1884440546999092433))
                             (= (load64 mem (- ip 8)) (xor64 (xor64 (load64 mem npk_hsec) (- ip 16)) 4436545153374750491))))
                    (not (= (load64 mem (+ ip (and64 (+ (load64 mem (+ e 24)) 15) 18446744073709551600)))
                            (xor64 (xor64 (load64 mem npk_hsec) (+ ip (and64 (+ (load64 mem (+ e 24)) 15) 18446744073709551600))) 7651035258233467253)))
                    (not (= (load64 mem (+ ip (and64 (+ (load64 mem (+ e 24)) 15) 18446744073709551600) 8))
                            (xor64 (xor64 (load64 mem npk_hsec) (+ ip (and64 (+ (load64 mem (+ e 24)) 15) 18446744073709551600) 8)) 7651035258233467253))))))

(symbol @npk_wildx_check
  (requires (<= 16 up))
  (ensures-trap (or (= up 0) (not (= (load64 mem (- up 8)) (xor64 (xor64 (load64 mem npk_hsec) (- up 16)) 4358112156723783278)))))
  (ensures (= result (mod (- up 16) 18446744073709551616))))

; --- the trap route ------------------------------------------------------------------------

(symbol @npk_heap_bad (ensures-trap true))
(symbol @npk_heap_oom (ensures-trap true))
(symbol @npk_heap_badreq (ensures-trap true))
(symbol @npk_raise (ensures-trap true))

(symbol @npk_stop_others
  (summary)
  (boundary "signals every other registered thread (tgkill) and waits, under the executor's join deadline, for each to park in the stop handler; a thread the kernel did not interrupt in time is proceeded past (TCB.md SS5 item 6); it writes no memory of its own -- the count it waits on is the handlers' atomic word")
  (ensures true))

(symbol @npk_driver_kill_all
  (summary)
  (boundary "sends SIGKILL through the pidfd of every live driver in the registry (pidfd_send_signal: allocation-free, mask-independent, safe against pid reuse); reaping is nobody's business on the trap path; it writes no memory")
  (ensures true))

(symbol @npk_exit
  (summary)
  (boundary "the controlled shutdown (D-013/D-014, D-151): a successful exit with live wild storage or a live driver routes to failsafe, a non-holder's exit during a failsafe parks, the heap-stats line is written, and the process ends by exit_group -- the path never returns")
  (ensures-trap true))

(symbol @npk_hs_report
  (boundary "the NPK_HEAP_STATS line, written to fd 2 exactly once per process at exit when the flag is armed: read without the heap mutex by design (a diagnostic, never a verdict; the shared-state exemption says why)"))

(symbol @npk_trap
  ; every path ends: the winner runs failsafe and exits, a loser parks, a
  ; re-entry exits 70 directly
  (ensures-trap true)
  (residue "the call of npk_failsafe is opaque -- the program's handler is not the floor's (D-014); its result decides the exit status through npk_exit, whose promise is the boundary's"))

(symbol @npk_wild_live_count
  (residue "the count of live wild blocks walks every chunk's watermarked slots and the large table -- three nested loops over the heap's structure, whose invariant would restate D-150's whole shape; the properties it rests on (header/payload disjointness, validate-before-dereference over the heap) are leg A's (D-233)"))

; --- the allocator's entry, as the envelope symbols assume it ----------------------------
;
; `npk_alloc_internal` is BOUNDARY (its rows are leg A's, D-233) and a
; (summary): a caller assumes a fresh 16-aligned block of n bytes, disjoint
; from every object the caller declares with `(objects ...)` and from every
; other allocation of the caller, those objects unchanged -- the
; allocator's promise, an acceptance TCB.md SS5 carries -- or the trap
; outcome under a condition nobody names (`oom`).

(symbol @npk_alloc_internal
  (summary)
  (free oom Int)
  (boundary "the managed heap's untracked entry: a block of n bytes, 16-aligned, disjoint from every live allocation, its header stamped live, or the heap trap on exhaustion; D-150's header/payload disjointness and validate-before-dereference are the invariants leg A (D-233) carries")
  (ensures-trap (not (= oom 0)))
  (ensures (not (= result 0)))
  (ensures (= (mod result 16) 0))
  (ensures (<= (+ result n) 18446744073709551616))
  (ensures (< n 9223372036854775808))
  (ensures (=> (> n 0) (= (load64 mem2 (- result 16)) n)))
  (ensures-fresh (- result 16) (+ n 16)))

; --- the envelope symbols: the D-141/D-069 discipline at the kernel boundary -------------
;
; A syscall is the uninterpreted `sys`; a negative kernel answer becomes the
; error field with a ZEROED value slot, a non-negative one the value with
; error 0; nothing is retried.

(symbol @npk_open
  (ensures (=> (>= (s64 r) 0) (and (= result.0 (mod r 4294967296)) (= result.1 0))))
  (ensures (=> (< (s64 r) 0) (and (= result.0 0) (= result.1 (mod r 4294967296))))))

(symbol @npk_close
  (ensures (=> (>= (s64 r) 0) (= result.0 0)))
  (ensures (=> (< (s64 r) 0) (= result.0 (mod r 4294967296)))))

(symbol @npk_read
  (free x Addr)
  ; zero asked is zero delivered, never end-of-input; a kernel 0 is E_EOF
  ; (-4096, D-141); a short read is returned; the count never exceeds cap;
  ; only the buffer's first `count` bytes may have moved
  (objects (buf cap))
  (ensures (=> (= cap 0) (and (= result.0 0) (= result.1 0))))
  (ensures (=> (and (not (= cap 0)) (< (s64 r) 0)) (and (= result.0 0) (= result.1 (mod r 4294967296)))))
  (ensures (=> (and (not (= cap 0)) (= r 0)) (and (= result.0 0) (= result.1 4294963200))))
  (ensures (=> (and (not (= cap 0)) (> (s64 r) 0)) (and (= result.0 r) (= result.1 0) (<= r cap))))
  (frame (buf cap)))

(symbol @npk_write
  (ensures (=> (>= (s64 r) 0) (and (= result.0 r) (= result.1 0) (<= r len))))
  (ensures (=> (< (s64 r) 0) (and (= result.0 0) (= result.1 (mod r 4294967296))))))

(symbol @npk_ofd_close
  (boundary "the descriptor's drop: close(2) once, its answer discarded by design (D-185: a drop cannot fail); no memory is touched")
  (frame))

(symbol @npk_mono_now
  ; seconds times a billion plus nanoseconds of the timespec the kernel
  ; wrote; a failing clock_gettime is a heap-shaped trap (-4102) -- the one
  ; impossible answer, kept as a check
  (ensures (= result (mod (+ (* 1000000000 (load64 mem2 ts)) (load64 mem2 (+ ts 8))) 18446744073709551616)))
  (ensures-trap (not (= r 0)))
  (frame (ts 16)))

(symbol @npk_path_exists
  ; openat succeeded and the descriptor was closed, or it did not
  (ensures (=> (>= (s64 fd) 0) (= result 1)))
  (ensures (=> (< (s64 fd) 0) (= result 0))))

(symbol @npk_write_file
  ; every byte is offered until the kernel has taken them all, a failed open,
  ; write or close is the answer's error (a failed close is a failed write)
  (loop wloop (invariant (and (<= 0 off) (<= off total))))
  (ensures (=> (< (s64 fd) 0) (= result.0 (mod fd 4294967296))))
  (ensures (=> (>= (s64 fd) 0) (or (= result.0 0) (= result.0 (mod n 4294967296)) (= result.0 (mod c 4294967296)))))
  (ensures (=> (and (>= (s64 fd) 0) (= result.0 0)) (>= (s64 c) 0)))
  (ensures (=> (and (>= (s64 fd) 0) (= result.0 0)) (= off data.1)))
  (frame))

(symbol @npk_read_file
  (free x Addr)
  (objects (path.0 path.1))
  ; a fresh buffer of at least 64 KiB, doubled while full, filled until the
  ; kernel answers 0; a failed open or read is the error with a zeroed value;
  ; the buffer is a live block below the allocator's ceiling, apart from the
  ; path, whose bytes the loop leaves alone
  (loop loop (objects ((- buf 16) (+ cap 16)))
             (invariant (and (<= len cap) (<= 65536 cap) (< cap 9223372036854775808)
                             (= (load64 mem2 (- buf 16)) cap)
                             (or (<= (+ buf cap) path.0) (<= (+ path.0 path.1) (- buf 16)))
                             (=> (and (<= path.0 x) (< x (+ path.0 path.1))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (< (s64 fd) 0) (and (= result.0.0 0) (= result.0.1 0) (= result.0.2 0) (= result.1 (mod fd 4294967296)))))
  (ensures (=> (and (>= (s64 fd) 0) (< (s64 n) 0)) (and (= result.0.0 0) (= result.0.1 0) (= result.0.2 0) (= result.1 (mod n 4294967296)))))
  (ensures (=> (and (>= (s64 fd) 0) (= n 0)) (and (= result.1 0) (<= result.0.1 result.0.2) (<= 65536 result.0.2) (not (= result.0.0 0)))))
  (frame objects)
  (residue "the bytes the kernel wrote are opaque (the kernel-effect table says only where), so the buffer's contents are not claimed; the growth's copy is memcpy's row in its own file"))

(symbol @npk_read_stdin
  (free x Addr)
  (loop loop (objects ((- buf 16) (+ cap 16))) (invariant (and (<= len cap) (<= 65536 cap) (< cap 9223372036854775808) (= (load64 mem2 (- buf 16)) cap))))
  (ensures (=> (< (s64 n) 0) (and (= result.0.0 0) (= result.0.1 0) (= result.0.2 0) (= result.1 (mod n 4294967296)))))
  (ensures (=> (= n 0) (and (= result.1 0) (<= result.0.1 result.0.2) (<= 65536 result.0.2) (not (= result.0.0 0)))))
  (frame objects)
  (residue "as npk_read_file's: the kernel's bytes are opaque"))

(symbol @npk_to_cstring
  (free j Int) (free x Addr)
  ; a copy with a NUL past the end, refused (-22) when the string holds an
  ; interior NUL -- the kernel would read a shorter path than the program named
  (objects (s.0 s.1))
  (requires (<= (+ s.0 s.1) 18446744073709551616))
  (loop scan (invariant (and (<= 0 i) (<= i s.1) (=> (and (<= 0 j) (< j i)) (not (= (load8 mem (+ s.0 j)) 0))))))
  (ensures (=> (= result.1 0) (and (not (= result.0.0 0)) (= result.0.1 s.1) (= (load8 mem2 (+ result.0.0 s.1)) 0)
                                   (=> (and (<= 0 j) (< j s.1)) (= (load8 mem2 (+ result.0.0 j)) (load8 mem (+ s.0 j)))))))
  (ensures (=> (= result.1 0) (=> (and (<= 0 j) (< j s.1)) (not (= (load8 mem (+ s.0 j)) 0)))))
  (ensures (=> (not (= result.1 0)) (and (= result.1 4294967274) (= result.0.0 0) (= result.0.1 0))))
  (frame objects))

(symbol @npk_string_concat
  (free j Int) (free x Addr)
  ; a's bytes then b's in a fresh block of exactly their total, or a's pointer
  ; with length 0 for an empty result (DEF-25); the quarantine tripwire traps
  ; on a poisoned source
  ; THE TWO INPUTS ARE VIEWS, NOT OBJECTS (1.5.6c step 1, lead E-1): `(objects (a.0 a.1) (b.0 b.1))` asserted
  ; them APART, false for `string_concat(s, s)` -- a legal program, in the tree twice -- and for any two
  ; overlapping views `string_from_bytes` makes; for such a call every row here assumed a falsehood and claimed
  ; nothing. The body only reads them (the frame row proves it: no byte of a view changes) and writes a fresh
  ; block, so the apartness was never needed: with it deleted all twelve rows still discharge. WHO KEEPS THE
  ; REST (in the address space, live): emitted code, the one caller -- nothing proves it at the call.
  (views (a.0 a.1) (b.0 b.1))
  (requires (<= (+ a.0 a.1) 18446744073709551616))
  (requires (<= (+ b.0 b.1) 18446744073709551616))
  (requires (< (+ a.1 b.1) 18446744073709551616))
  (ensures (= result.1 0))
  (ensures (= result.0.1 (+ a.1 b.1)))
  (ensures (=> (= (+ a.1 b.1) 0) (and (= result.0.0 a.0) (= result.0.2 0))))
  (ensures (=> (> (+ a.1 b.1) 0) (and (not (= result.0.0 0)) (= (mod result.0.0 16) 0) (= result.0.2 (+ a.1 b.1)))))
  (ensures (=> (and (> (+ a.1 b.1) 0) (<= 0 j) (< j a.1)) (= (load8 mem2 (+ result.0.0 j)) (load8 mem (+ a.0 j)))))
  (ensures (=> (and (> (+ a.1 b.1) 0) (<= 0 j) (< j b.1)) (= (load8 mem2 (+ result.0.0 a.1 j)) (load8 mem (+ b.0 j)))))
  (ensures-trap (and (not (= (load64 mem npk_quarantine) 0))
                     (or (and (> (s64 a.1) 1) qhit_a) (and (or (<= (s64 a.1) 1) (not qhit_a)) (> (s64 b.1) 1) qhit_b))))
  (frame objects))

(symbol @npk_string_slice
  (free j Int) (free x Addr)
  ; an owned copy of s[start, end) (D-186), refused (-34) when the bounds are
  ; not a prefix-ordered pair inside the string; an empty slice allocates nothing
  (objects (s.0 s.1))
  (requires (<= (+ s.0 s.1) 18446744073709551616))
  (ensures (=> (or (< end start) (> end s.1)) (and (= result.1 4294967262) (= result.0.0 0) (= result.0.1 0) (= result.0.2 0))))
  (ensures (=> (and (<= start end) (<= end s.1)) (and (= result.1 0) (= result.0.1 (- end start)))))
  (ensures (=> (and (<= start end) (<= end s.1) (= end start)) (= result.0.2 0)))
  (ensures (=> (and (<= start end) (<= end s.1) (< start end)) (and (not (= result.0.0 0)) (= result.0.2 (- end start)))))
  (ensures (=> (and (<= start end) (<= end s.1) (< start end) (<= 0 j) (< j (- end start))) (= (load8 mem2 (+ result.0.0 j)) (load8 mem (+ s.0 start j)))))
  (frame objects))

(symbol @npk_int_to_string
  ; the decimal text of v with a leading minus for a negative, in a fresh
  ; 24-byte block re-homed at its base: the length is the digit count plus
  ; the sign, the capacity 24
  (loop digits (unroll 18 exact))
  (loop rehome (unroll 20 exact))
  (ensures (= result.1 0))
  (ensures (= result.0.2 24))
  (ensures (not (= result.0.0 0)))
  (ensures (=> (= v 0) (and (= result.0.1 1) (= (load8 mem2 result.0.0) 48))))
  (ensures (=> (< (s64 v) 0) (= (load8 mem2 result.0.0) 45)))
  (frame objects)
  (residue "the digit bytes are the same computation as npk_hs_put_dec's, whose twenty rows state them; here the length, the capacity, the zero and the sign are rows -- and the sign row is not decided under the profile (unknown at ten times the budget): the sign byte reaches the block's base through the rehome copy's twenty unrolled loads, whose addresses the solver must relate to the sign store's wrapped one"))

; --- the boundary symbols (1.5.6 step 4, item 3) ----------------------------------------
;
; Every other syscall-class symbol: its sentence states what the symbol
; promises given the kernel's promise, and why it has no rows (§2.8 of the
; plan) -- the heap's structure is leg A's (D-233) and a syscall's effect is
; the kernel's. A boundary section is never translated; a `(summary)` one
; among them is assumed at its calls by its clauses.

; the allocator's entries and paths
(symbol @npk_alloc
  (boundary "the wild `alloc` entry (D-150, D-151): a block of n bytes from npk_alloc_impl with wild = 1, in the wild-live set until dalloc'd and counted by the exit check; alloc(0) is a real 16-byte block"))
(symbol @npk_alloc_managed
  (boundary "the managed entry (D-183): a block of n bytes from npk_alloc_impl with wild = 0 -- the drop walk's storage, never in the wild-live set, freed through dalloc like any managed body"))
(symbol @npk_alloc_impl
  (boundary "the allocator's core: during a failsafe a bump from the region with no mutex and no tables (D-292); otherwise, under the heap mutex, a slot of a small chunk for n <= the largest class or a fresh mapping through npk_large_new, the header stamped live, the stats counters noted; a request past the size ceiling is HeapBadRequest and a refused mapping HeapOom; alloc(0) is a real block (D-150)"))
(symbol @npk_aalloc
  (boundary "the aligned entry: a power-of-two alignment or HeapBadRequest; at or below sixteen the ordinary path, above it a mapping through npk_large_new at that alignment under the heap mutex (initialised there, locked -- D-290); during a failsafe the region at the alignment (D-292)"))
(symbol @npk_calloc
  (boundary "count * size bytes, the multiply CHECKED (a wrap is HeapBadRequest, never an undersized block), the payload zeroed"))
(symbol @npk_ralloc
  (boundary "a block of n bytes holding the old block's bytes up to the smaller size, the old block's role (wild or managed) kept and the old block freed; ralloc(p, 0) is HeapBadRequest (D-150); during a failsafe a fresh region block with the old bytes copied by the header's size and the old block left (D-292)"))
(symbol @npk_dalloc
  (summary)
  (free bad Int)
  (boundary "the free: a null or misaligned pointer is HeapBadRequest (dalloc(NULL) traps by D-150), a block whose header does not validate is HeapBadRequest, a small block returns to its chunk (npk_small_free) and a large one to the kernel (munmap) under the heap mutex; during a failsafe, after the null and alignment checks, a no-op (D-292)")
  ; as a summary (the envelope symbols free a superseded buffer): the trap
  ; outcome under a condition nobody names (`bad`), the freed block's bytes
  ; the allocator's (poisoned), the caller's objects untouched
  (ensures-trap (not (= bad 0)))
  (ensures true)
  (frame ((- p 16) (+ (load64 mem (- p 16)) 16)) objects))
(symbol @npk_buffer_new
  (boundary "a zeroed managed block of n bytes as {ptr, n, n} through npk_alloc_managed and npk_zero; n <= 0 is the non-owning empty {null, 0, 0} (cap 0 the not-mine bit); exhaustion traps inside the allocator, so the Result is always the ok arm"))
(symbol @npk_fs_alloc
  (boundary "a block of n bytes at the requested alignment bumped from the one-mebibyte failsafe region with the heap's [ size | 0 ] header before it, single-threaded by construction since every other thread is parked before failsafe runs (D-291); exhaustion is HeapOom inside failsafe, the re-entry rule's exit 70 (D-292)"))
(symbol @npk_heap_init
  (boundary "the heap's first initialisation, under the mutex (D-290): the hash secret from getrandom (a zero draw takes a fixed odd constant -- weaker keying for one run in 2^64, never a stall), the large table mapped, the class tables' derived words computed"))
(symbol @npk_small_alloc
  (boundary "a slot of a small chunk of class ci under the heap mutex: the first clear bit of a partial chunk from the hint word on (a partial chunk with no clear bit is a bookkeeping violation and traps), a chunk moved to the full list when its last slot goes, a new chunk mapped when no partial one exists; the slot's header stamped by the wild role"))
(symbol @npk_chunk_new
  (boundary "a fresh 64 KiB-aligned chunk for class ci: a 128 KiB anonymous mapping trimmed to the aligned 64 KiB (the surplus unmapped), its header (the guard word, the class, the slot and free counts, the hint, the list links, the watermark) and its bitmap written, the chunk registered in the sorted chunk table"))
(symbol @npk_chtab_insert
  (boundary "inserts a chunk's address into the sorted chunk table, growing the table by a fresh mapping when full and moving the greater entries up one slot; sortedness -- what npk_chtab_find's requires assumes -- is this symbol's construction"))
(symbol @npk_large_new
  (boundary "a large block: a page-rounded anonymous mapping of the header, the payload at the requested alignment and the size, registered in the large table with its base and mapping size; the payload meets the alignment and the block fits its mapping, asserted where they are produced"))
(symbol @npk_lg_insert
  (boundary "inserts a large block's entry (payload, base, mapping size, size) into the large table sorted by payload, growing the table by a fresh mapping (the old one copied and unmapped) when full and moving the greater entries up one slot; sortedness is this symbol's construction"))
(symbol @npk_hmap
  (boundary "an anonymous private read-write mapping of len bytes (mmap), or HeapOom when the kernel refuses (an answer past 2^64 - 4096)"))
(symbol @npk_hunmap
  (boundary "munmap of a mapping the heap made; a refusal means the table and the kernel disagree about what is mapped -- an integrity failure, HeapBadRequest, not an OOM"))
(symbol @npk_wild_release_all
  (boundary "failsafe's controlled cleanup: every chunk and every large mapping returned to the kernel, the tables reset, the allocator left usable; after it only exit may follow (TYPE-062) -- anything still pointing into the heap points at unmapped pages"))
(symbol @npk_cstr_slice
  (boundary "the bytes of a NUL-terminated string the kernel wrote (argv, envp) as {ptr, len}: a strlen over the kernel's memory, the bytes themselves never copied"))

; the arenas and the frame arena
(symbol @npk_arena_make
  (boundary "an arena of cap0 slots of the given stride over three managed heap blocks -- the slots, the generations (every generation 1, live-odd), the free list -- with top 0 and the free head -1 (D-183: managed, the binding's scope-exit drop destroys it)"))
(symbol @npk_arena_alloc
  (boundary "a slot: the free list's head when one exists (its generation already bumped by the free), else the top slot, the arrays grown by ralloc when the top reaches the cap (the single-threaded contract makes the relocation safe, D-017); the answer is {index, generation} -- the handle's identity"))
(symbol @npk_arena_destroy
  (boundary "returns the arena's slot and generation blocks to the heap (dalloc, when present) and leaves the header vacant: null blocks, cap 0, top 0, free head -1"))
(symbol @npk_frame_exec_new
  (boundary "a frame arena for an executor: one 64 KiB chunk mapped up front (the steady state never maps again), eight size buckets over two parallel managed arrays"))
(symbol @npk_frame_exec_destroy
  (boundary "unmaps the frame arena's chunks and frees its bucket arrays"))
(symbol @npk_frame_alloc
  (boundary "a coroutine frame of size bytes at align (at or below the heap's sixteen): the exact-size bucket's free list first (the common steady state), else a bump from the current chunk, else a dedicated heap block whose header carries the dedicated bit; the header stamped frame-live"))
(symbol @npk_frame_free
  (boundary "returns a frame: a null or misaligned pointer, or a header that does not validate as frame-live, is HeapBadRequest; a dedicated block goes back to the heap whole, a bucketed one is stamped frame-free and pushed on its size bucket's list, a new bucket appended (the parallel arrays grown by ralloc) when its size has none"))

; the waits and the wakes (the sync primitives over the cell's shape)
(symbol @npk_ch_lock
  (boundary "a channel's own futex mutex (slot 8): the CAS from 0 to 1, else a futex wait on the word until it is 0 again; single-threaded reasoning past the acquire is the shared-state rule's (D-290)"))
(symbol @npk_ch_unlock
  (boundary "the channel mutex's release: the word stored 0 and one waiter woken (futex wake)"))
(symbol @npk_ch_wake_all
  (boundary "wakes every frame on the waiter list at hp, one at a time through npk_ch_wake_one, until the list is empty: each due-marked and its executor's eventfd written"))
(symbol @npk_mutex_acquire_wait
  (boundary "the Mutex wait (1.1.11): under the cell's own futex, the frame linked on the cell's waiter list and parked until the releaser's wake or the absolute deadline, the acquire re-run when woken (a woken waiter may lose to a barger, which the deadline bounds); the frame unlinked on every completed path; 0 acquired, 1 the deadline"))
(symbol @npk_guard_release
  (boundary "releases an exclusive hold: the state word to free and a wake by the cell's kind (a mutex wakes one waiter, an rwlock's writer release wakes all -- a crowd of readers may proceed together)"))
(symbol @npk_rw_read_wait
  (boundary "the RwLock's read acquire: parked on the cell's waiter list while a writer holds it (state 1) or until the absolute deadline; N >= 2 is N-1 readers; 0 acquired, 1 the deadline"))
(symbol @npk_rw_write_wait
  (boundary "the RwLock's write acquire: parked on the cell's waiter list while any holder exists or until the absolute deadline; 0 acquired, 1 the deadline"))
(symbol @npk_rw_release_read
  (boundary "a reader's release: the count comes down under the cell's futex, and zero wakes the crowd -- a parked writer is in it"))
(symbol @npk_cv_begin
  (boundary "the CondVar wait's entry: the frame linked on the cv's waiter list under the cv's futex, the guard's mutex released (the ordinary guard release), the frame due at the caller's absolute deadline -- the park itself is the executor's"))
(symbol @npk_cv_done
  (boundary "unlinks the frame from the cv's waiter list under the cv's futex, on every completed path -- idempotent like every unlink"))
(symbol @npk_cv_signal
  (boundary "wakes one waiter on the cv's list under the cv's futex (npk_ch_wake_one)"))
(symbol @npk_cv_broadcast
  (boundary "wakes every waiter on the cv's list under the cv's futex (npk_ch_wake_all)"))
(symbol @npk_barrier_arrive
  (boundary "a party's arrival under the cell's futex: the count up; the last arrival completes the round -- the generation bumped, the count reset, every parked party woken -- and answers the generation it arrived in"))
(symbol @npk_barrier_poll
  (boundary "the Barrier wait: parked on the cell's waiter list until the generation moves past mygen or the absolute deadline; 0 the round completed, 1 the deadline"))
(symbol @npk_barrier_cancel
  (boundary "a timed-out party hands its slot back under the cell's futex: unlinked from the waiter list, and the count down by one unless its round already completed (the generation moved), in which case there is nothing to hand back"))
(symbol @npk_park_forever
  (boundary "a futex wait on a word nothing writes, re-waited on a spurious return: the stop handler's body, a losing trapper's end, and the end of an executor that saw the frozen flag and is not the holder (DEF-57); the thread dies with the winner's exit_group; allocation-free by construction (D-291)"))

; the threads, the process, the executor loop
(symbol @npk_start
  (boundary "the process entry, called by _start with the initial stack pointer: argv and envp measured, the main thread's TLS block and executor booted (npk_tls_boot), SIGUSR1 armed for the stop signal over the kernel's own sigaction shape (D-291), the driver registry cleared, main run to completion under the executor, then the exit sequence (npk_exit) -- the path never returns"))
(symbol @npk_tls_boot
  (boundary "the main thread's TLS block and executor from raw anonymous mappings -- INTERNAL, not npk_alloc, so the runtime's own storage is never a leak at a clean exit (D-151) -- and %fs set by arch_prctl to the block"))
(symbol @npk_thread_entry
  (boundary "a spawned thread's first ordinary code, reached by the clone trampoline's real call (a child that continued in IR would read the parent's spilled stack): its TLS and executor booted from the trampoline's block, the task run to completion, the thread ended through npk_thread_exit"))
(symbol @npk_thread_exit
  (boundary "exit(60) of the calling thread alone: the kernel clears and wakes the CLONE_CHILD_CLEARTID word the joiner waits on (a shared wake, 1.4.4); the path never returns"))
(symbol @npk_hardware_concurrency
  ; DEF-52 (1.5.6b): the mask is ZEROED before the kernel sees it, because the
  ; raw sched_getaffinity writes only `z` bytes of the 128 and nothing else
  ; zero-fills the rest. The third claim is the one that bites: when the kernel
  ; wrote nothing, every word is zero, the popcounts sum to zero and the answer
  ; is the floor's 1 -- REFUTED on the floor as it stood before the fix, where
  ; the sixteen words were whatever the stack held. `ctpop64` is uninterpreted,
  ; in [0, 64], zero at zero; the sixteen-word walk is unwound exactly.
  (loop loop (unroll 16 exact))
  (ensures (>= result 1))
  (ensures (<= result 1024))
  (ensures (=> (= z 0) (= result 1))))
; (it was a BOUNDARY section until 1.5.6b -- a promise, never proven, which is
; where DEF-52 lived. WHICH bits the kernel sets stays the kernel's answer; how
; many bytes it writes is the kernel-effect table's row 204, measured.)
(symbol @npk_run_until
  (boundary "the executor loop until the target task completes (0) or the absolute deadline dl expires (1; 0 is no deadline): every ready task stepped in queue order, the idle wait epoll_pwait carrying the earliest sleeper's deadline (1.1.12), the waker's due-now stamp consumed at resume"))
(symbol @npk_io_unwatch
  (boundary "epoll_ctl DEL of the descriptor from the executor's epoll instance, so a later readiness can never write into a freed registration; ENOENT (never watched, or closed -- the kernel removed it) is the no-op answer, and with no reactor armed there is nothing to remove"))

; the executable pages
(symbol @npk_wildx_seal
  (boundary "the ONE-WAY transition of a wildx page: mprotect to PROT_READ|PROT_EXEC; the page is never PROT_WRITE|PROT_EXEC, so W^X holds structurally"))
(symbol @npk_wildx_call
  (residue "an ordinary indirect call into a sealed wildx page: the analysis has proven the page sealed before this runs, and the contents are outside verification by construction (D-035) -- the call's effect on memory and its result are opaque, and no clause claims either"))
