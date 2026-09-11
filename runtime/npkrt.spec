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
; `trap` and `trap_code`; a global by its name without `@` as its ADDRESS,
; its contents read through `mem`; `(free j Int)` a free symbol of the
; section; `(load8 M a)`, `(load16 M a)`, `(load32 M a)`, `(load64 M a)` the
; little-endian reassembly of the bytes at `a` in memory M; `(xor64 a b)`,
; `(and64 a b)`, `(or64 a b)` the 64-bit bit operations. A name SMT-LIB owns
; (`abs`, `mod`, `store`...) or the writer uses is spelled `|r:name|`.
;
; Clauses: `(requires P)` -- a hypothesis the floor's callers keep;
; `(ensures P)` -- one row, on the non-trapping paths; `(ensures-trap P)` --
; one row, `trap` iff P (absent: a symbol with trap sites claims `not trap`);
; `(frame (lo len) ...)` -- one row: outside every range memory is unchanged
; (absent: memory is unchanged everywhere, a row); `(loop LABEL (invariant
; I) | (unroll N))` with `(inst SYM e)` -- an extra instantiation of the
; invariant hypothesis at e; `(summary)` -- callers assume this section
; instead of inlining the body; `(residue "why")` -- what the profile does
; not decide, copied into TCB.md §5; `(boundary "what")` -- a syscall-class
; symbol's promise at the kernel boundary. Addresses wrap at 2^64 exactly as
; the IR's do, so a symbol that walks a range states the range is in the
; address space (`(<= (+ p n) 18446744073709551616)`) as a requires.
;
; THE SINGLE-INSTANCE RULE. An invariant's universal is written over the
; section's free symbols; the writer asserts the hypothesis at those symbols
; and demands the conclusion at the same symbols. A step that needs the
; hypothesis at another point names it: memcpy's step reads the source byte
; at `src + i`, which the frame conjunct covers only at `x`, so `(inst x (+
; src i))` instantiates it there.

; --- the byte helpers LLVM calls behind a program's back ---------------------

(symbol @memcpy
  (free j Int) (free x Int)
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
  (free j Int) (free x Int)
  (requires (<= (+ dst n) 18446744073709551616))
  (loop head
    (invariant (and (<= 0 i) (<= i n)
                    (=> (and (<= 0 j) (< j i)) (= (load8 mem2 (+ dst j)) (mod c 256)))
                    (=> (or (< x dst) (>= x (+ dst i))) (= (load8 mem2 x) (load8 mem x))))))
  (ensures (=> (and (<= 0 j) (< j n)) (= (load8 mem2 (+ dst j)) (mod c 256))))
  (ensures (= result dst))
  (frame (dst n)))

(symbol @npk_zero
  (free j Int) (free x Int)
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
  (free j Int) (free x Int)
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
