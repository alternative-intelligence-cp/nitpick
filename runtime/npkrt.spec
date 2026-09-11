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
  (word %npk.tls 4 stated "the join's futex word: written 0 by the creating thread before the clone, then by the kernel (PARENT_SETTID writes the tid, CHILD_CLEARTID zeroes it at exit), read by npk_thread_join with an atomic load and by the kernel's futex compare (DEF-48)")

  ; --- globals -----------------------------------------------------------------
  (word @npk_ch_tab publish ch-open-lock)  ; the table pointer: a release store under the open lock, acquire loads by readers
  (word @npk_ch_n publish ch-open-lock)    ; the count, published after the pointer
  (word @npk_ch_cap lock ch-open-lock)
  (word @npk_ch_fstk lock ch-open-lock)
  (word @npk_ch_fn lock ch-open-lock)
  (word @npk_ch_fcap lock ch-open-lock)
  (word @npk_frozen atomic)                ; D-063's flag: written by the trapping thread, read by every executor (DEF-46)
  (word @npk_in_failsafe atomic)           ; the failsafe holder's word; its arbitration is DEF-47's (step 1)
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
