# The trusted computing base

Drafted at 1.5.0 (2026-09-03) under D-218.11, in r8's terms (the digest is
`meta/roadmap/research/digests/r5r8-digest.md`; read its reliability notes
before citing further). This is the document an auditor reads first: what
the evidence campaign PROVES, what it TESTS, and what it must TAKE ON TRUST —
enumerated, never implied. It is finalized at 1.5.6, when the floor's rows
get their per-symbol dispositions; the membership table below is GENERATED
from `runtime/npkrt.ll` and held to it by the harness
(`check_tcb_floor_current`), so it cannot go stale without a red run.

## 1. The claim

**Verified middle-end, validated floor.** Verification establishes
properties of the Nitpick source and of the LLVM IR the compiler emits — the
artifact of record (D-067). It does not establish that `llc` translated that
IR faithfully, that `ld.lld` linked it faithfully, or that the kernel does
what its ABI says. Those are trusted components, named below. What sits
between the source and the IR is the compiler, and the compiler is the thing
that rebuilds itself byte-identically from a committed snapshot (D-202,
D-203) and, since 1.5.0, rebuilds itself byte-identically as a VERIFIED
build too.

## 2. The evidence, by leg (D-233)

| Leg | Evidence class | Instrument | Where it lands |
|---|---|---|---|
| B | per-obligation proof — every D-218 obligation decided by z3 under the determinism profile, the verdicts committed as `nitpick.obligations`, a discharged guard elided through `llvm.assume` | `npkg verify`, the harness's `verify` stages | 1.5.0 (the D-007 division pair); the rest of the catalogue through 1.5.8 |
| A | whole-program runtime-error absence over the emitted IR by abstract interpretation | Clam/Crab or IKOS, chosen at 1.6.0's gate | 1.6 |
| C | optimizer integrity — the pinned `opt -O2` pipeline does not remove a guarantee | Alive2 beside the opt-O2 harness leg, which stays as the end-to-end net | 1.6 |

The harness's standing instruments are TESTS on the boundaries the legs do
not prove: the `selfhost` fixpoint, the `repro` reproducibility legs, the
opt-O2 re-run of every program, the `absent-fact` flip, the parity of two
runners, the `undef` ban.

## 3. Trusted components, enumerated

| Component | Pinned by | Why it is trusted rather than proven |
|---|---|---|
| LLVM 20.1.2 `llc`, `ld.lld` (and `opt` on the -O2 path) | `nitpick.toml` `[toolchain]`, an exact patch release, every invocation built from its flag lists (D-204) | the translation of IR to machine code is outside the verified boundary (D-067); every verified toolchain short of CompCert has one. Leg C (1.6) validates the optimizer's passes; the opt-O2 leg tests the whole; `llc` itself is not validated |
| the Linux kernel's syscall ABI (x86_64) | the floor's one trampoline, `@npk_sys6`, and the `module asm` clone | the boundary at which a value becomes a syscall argument; the floor's `syscall` rows below are specified AT this boundary (1.5.6), never past it |
| z3 4.16.0, the workbench build of tag `z3-4.16.0` | `[verify]` `z3-sha256` (the binary's hash), `z3-version`, `z3-options` (D-218.1/D-218.2) | an EVIDENCE tool: a solver defect is a wrong verdict, and a wrong `discharged` elides a guard. Mitigations: the pin (one build, one hash), the profile (a verdict is a function of the obligation, the build and the budget; the Diophantine sub-solver is off since 1.5.6 step 4 — S-71 — because its undo at `(pop)` was the one place the pinned build did not return), the committed manifest (a verdict that moves is a red run), `--explain`'s unsat cores on request; proof certificates are D-040's opt-in for certification runs |
| the leg-A analyzer and Alive2 (1.6) | commit hash, built on the workbench | the same doctrine as z3's (D-233): pinned, auditable, verdicts committed, a new alarm on an unchanged tree a stop sign |
| the floor's volatile bottom | this file's table, class `asm` | inline assembly and the clone: the seL4 precedent — handwritten assembly and volatile accesses are documented as the bottom of the TCB, not proven |

## 4. The floor, enumerated

`runtime/npkrt.ll` is hand-written LLVM IR, permanent (D-203), linked into
every artifact. Every `define` in it is one row below, classified by what its
body does — the classifier is `bootstrap/harness/harness.py`'s
`_floor_classes`, and `check_tcb_floor_current` fails the run when this table
and the floor disagree:

- **asm** — the body contains inline assembly: the syscall trampoline and
  the clone-and-exec. The volatile bottom. Trusted, documented, never proven.
- **atomic** — the body performs an atomic operation: the channel, mutex,
  park, scheduler, shared-arena and driver-registry paths. Modelled as
  PRIMITIVES at 1.5.6 (the r6 verdict: model the primitive, never the whole
  executor; BPOR-style bounds if a model spins), never proven whole.
- **syscall** — the body reaches the trampoline transitively and performs no
  atomic operation: the allocator's mmap path, the file and descriptor
  surface, the traps, the executor's waits. Specified at the syscall boundary
  at 1.5.6: what the floor promises given what the kernel promises.
- **pure** — none of the above: arithmetic (`__udivti3`, `fmod`), the memory
  helpers LLVM calls behind the program's back (`memcpy`, `memset`,
  `memmove`), the allocator's bookkeeping, the string helpers. Z3-specified
  at 1.5.6 where feasible (r8 Lesson 2: have Z3 prove the floor's execution
  traces simulate the compiler's memory-model semantics).

The disposition column is GENERATED too (1.5.6 step 3, D-288 §2.10;
`bootstrap/harness/tcb_floor.py --write` rewrites the table, and both
runners' belts hold it): `trusted (inline asm)` for the asm class;
`specified (N discharged, M residue)` from the floor's committed manifest
(`runtime/npkrt.obligations`) for a symbol with a section in
`runtime/npkrt.spec`; the section's `residue (…)` and `boundary (…)`
sentences verbatim; and, for a symbol no section names yet, the class
default — the rows steps 4–6 still owe read exactly as they did before
this column existed, so a reader can tell the specified from the pending.
The `asm` class means inline assembly in the body OR a call of a symbol
`module asm` defines (`npk_thread_start` rides `npk_clone_raw`), over the
code of each body — a comment that said `asm` classified `npk_clone_exec`
until step 3 held the two runners' classifiers to one answer.

<!-- BEGIN floor-table -->
| symbol | class | disposition |
|---|---|---|
| `@__divti3` | pure | specified (3 discharged, 0 residue) |
| `@__modti3` | pure | specified (4 discharged, 0 residue) |
| `@__udivti3` | pure | specified (2 discharged, 0 residue) |
| `@__umodti3` | pure | specified (3 discharged, 0 residue) |
| `@fmod` | pure | specified (7 discharged, 0 residue); residue (the reduction's result is not decided under the profile: |result| < |b| on the finite path is unknown with the loops unwound 2 and 8 times (z3's floating-point theory bit-blasts every fmul and fsub of the chain; measured 2026-09-11); the special values are rows) |
| `@fmodf` | pure | specified (7 discharged, 0 residue); residue (as fmod's: the reduction's result is not decided under the profile (unknown with the loops unwound 2 and 8 times, measured 2026-09-11); the special values are rows) |
| `@memcpy` | pure | specified (5 discharged, 0 residue) |
| `@memmove` | pure | specified (7 discharged, 0 residue) |
| `@memset` | pure | specified (5 discharged, 0 residue) |
| `@npk_aalloc` | syscall | boundary (the aligned entry: a power-of-two alignment or HeapBadRequest; at or below sixteen the ordinary path, above it a mapping through npk_large_new at that alignment under the heap mutex (initialised there, locked -- D-290); during a failsafe the region at the alignment (D-292)) |
| `@npk_alloc_impl` | syscall | boundary (the allocator's core: during a failsafe a bump from the region with no mutex and no tables (D-292); otherwise, under the heap mutex, a slot of a small chunk for n <= the largest class or a fresh mapping through npk_large_new, the header stamped live, the stats counters noted; a request past the size ceiling is HeapBadRequest and a refused mapping HeapOom; alloc(0) is a real block (D-150)) |
| `@npk_alloc_internal` | syscall | boundary (the managed heap's untracked entry: a block of n bytes, 16-aligned, disjoint from every live allocation, its header stamped live, or the heap trap on exhaustion; D-150's header/payload disjointness and validate-before-dereference are the invariants leg A (D-233) carries) |
| `@npk_alloc_managed` | syscall | boundary (the managed entry (D-183): a block of n bytes from npk_alloc_impl with wild = 0 -- the drop walk's storage, never in the wild-live set, freed through dalloc like any managed body) |
| `@npk_alloc` | syscall | boundary (the wild `alloc` entry (D-150, D-151): a block of n bytes from npk_alloc_impl with wild = 1, in the wild-live set until dalloc'd and counted by the exit check; alloc(0) is a real 16-byte block) |
| `@npk_arena_alloc` | syscall | boundary (a slot: the free list's head when one exists (its generation already bumped by the free), else the top slot, the arrays grown by ralloc when the top reaches the cap (the single-threaded contract makes the relocation safe, D-017); the answer is {index, generation} -- the handle's identity) |
| `@npk_arena_at` | pure | specified (4 discharged, 0 residue) |
| `@npk_arena_destroy` | syscall | boundary (returns the arena's slot and generation blocks to the heap (dalloc, when present) and leaves the header vacant: null blocks, cap 0, top 0, free head -1) |
| `@npk_arena_free` | pure | specified (4 discharged, 0 residue) |
| `@npk_arena_make` | syscall | boundary (an arena of cap0 slots of the given stride over three managed heap blocks -- the slots, the generations (every generation 1, live-odd), the free list -- with top 0 and the free head -1 (D-183: managed, the binding's scope-exit drop destroys it)) |
| `@npk_arena_reset` | pure | specified (6 discharged, 0 residue) |
| `@npk_barrier_arrive` | syscall | boundary (a party's arrival under the cell's futex: the count up; the last arrival completes the round -- the generation bumped, the count reset, every parked party woken -- and answers the generation it arrived in) |
| `@npk_barrier_cancel` | syscall | boundary (a timed-out party hands its slot back under the cell's futex: unlinked from the waiter list, and the count down by one unless its round already completed (the generation moved), in which case there is nothing to hand back) |
| `@npk_barrier_poll` | syscall | boundary (the Barrier wait: parked on the cell's waiter list until the generation moves past mygen or the absolute deadline; 0 the round completed, 1 the deadline) |
| `@npk_buffer_new` | syscall | boundary (a zeroed managed block of n bytes as {ptr, n, n} through npk_alloc_managed and npk_zero; n <= 0 is the non-owning empty {null, 0, 0} (cap 0 the not-mine bit); exhaustion traps inside the allocator, so the Result is always the ok arm) |
| `@npk_calloc` | syscall | boundary (count * size bytes, the multiply CHECKED (a wrap is HeapBadRequest, never an undersized block), the payload zeroed) |
| `@npk_ch_at` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_close` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_closed` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_get` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_lock` | syscall | boundary (a channel's own futex mutex (slot 8): the CAS from 0 to 1, else a futex wait on the word until it is 0 again; single-threaded reasoning past the acquire is the shared-state rule's (D-290)) |
| `@npk_ch_open` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_push` | pure | specified (5 discharged, 0 residue) |
| `@npk_ch_reclaim` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_recv_wait` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_send_wait` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_try_recv` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_try_send` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_unlink` | pure | specified (6 discharged, 0 residue) |
| `@npk_ch_unlock` | syscall | boundary (the channel mutex's release: the word stored 0 and one waiter woken (futex wake)) |
| `@npk_ch_wait_link` | syscall | specified (3 discharged, 0 residue) |
| `@npk_ch_wait_unlink` | syscall | specified (2 discharged, 0 residue); residue (an unlink from deeper in the waiter list than the head is not decided: the walk is bounded at eight links and the list's reachability is no first-order clause; the head case is a row) |
| `@npk_ch_wake_all` | syscall | boundary (wakes every frame on the waiter list at hp, one at a time through npk_ch_wake_one, until the list is empty: each due-marked and its executor's eventfd written) |
| `@npk_ch_wake_one` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_chain_depth` | syscall | specified (2 discharged, 0 residue) |
| `@npk_chain_push` | syscall | specified (3 discharged, 0 residue) |
| `@npk_chain_reset` | syscall | specified (3 discharged, 0 residue) |
| `@npk_chain_site` | syscall | specified (4 discharged, 0 residue) |
| `@npk_chtab_find` | pure | specified (5 discharged, 0 residue) |
| `@npk_chtab_insert` | syscall | boundary (inserts a chunk's address into the sorted chunk table, growing the table by a fresh mapping when full and moving the greater entries up one slot; sortedness -- what npk_chtab_find's requires assumes -- is this symbol's construction) |
| `@npk_chunk_guard_check` | syscall | specified (2 discharged, 0 residue) |
| `@npk_chunk_new` | syscall | boundary (a fresh 64 KiB-aligned chunk for class ci: a 128 KiB anonymous mapping trimmed to the aligned 64 KiB (the surplus unmapped), its header (the guard word, the class, the slot and free counts, the hint, the list links, the watermark) and its bitmap written, the chunk registered in the sorted chunk table) |
| `@npk_clone_exec` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_close` | syscall | specified (3 discharged, 0 residue) |
| `@npk_cstr_slice` | syscall | boundary (the bytes of a NUL-terminated string the kernel wrote (argv, envp) as {ptr, len}: a strlen over the kernel's memory, the bytes themselves never copied) |
| `@npk_cv_begin` | syscall | boundary (the CondVar wait's entry: the frame linked on the cv's waiter list under the cv's futex, the guard's mutex released (the ordinary guard release), the frame due at the caller's absolute deadline -- the park itself is the executor's) |
| `@npk_cv_broadcast` | syscall | boundary (wakes every waiter on the cv's list under the cv's futex (npk_ch_wake_all)) |
| `@npk_cv_done` | syscall | boundary (unlinks the frame from the cv's waiter list under the cv's futex, on every completed path -- idempotent like every unlink) |
| `@npk_cv_signal` | syscall | boundary (wakes one waiter on the cv's list under the cv's futex (npk_ch_wake_one)) |
| `@npk_dalloc` | syscall | boundary (the free: a null or misaligned pointer is HeapBadRequest (dalloc(NULL) traps by D-150), a block whose header does not validate is HeapBadRequest, a small block returns to its chunk (npk_small_free) and a large one to the kernel (munmap) under the heap mutex; during a failsafe, after the null and alignment checks, a no-op (D-292)) |
| `@npk_driver_kill_all` | atomic | boundary (sends SIGKILL through the pidfd of every live driver in the registry (pidfd_send_signal: allocation-free, mask-independent, safe against pid reuse); reaping is nobody's business on the trap path; it writes no memory) |
| `@npk_driver_live_count` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_driver_retire` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_environ` | pure | specified (3 discharged, 0 residue) |
| `@npk_exec` | asm | trusted (inline asm) |
| `@npk_exit` | atomic | boundary (the controlled shutdown (D-013/D-014, D-151): a successful exit with live wild storage or a live driver routes to failsafe, a non-holder's exit during a failsafe parks, the heap-stats line is written, and the process ends by exit_group -- the path never returns) |
| `@npk_frame_alloc` | syscall | boundary (a coroutine frame of size bytes at align (at or below the heap's sixteen): the exact-size bucket's free list first (the common steady state), else a bump from the current chunk, else a dedicated heap block whose header carries the dedicated bit; the header stamped frame-live) |
| `@npk_frame_bucket` | pure | specified (5 discharged, 0 residue) |
| `@npk_frame_drain` | pure | specified (4 discharged, 0 residue) |
| `@npk_frame_exec_destroy` | syscall | boundary (unmaps the frame arena's chunks and frees its bucket arrays) |
| `@npk_frame_exec_new` | syscall | boundary (a frame arena for an executor: one 64 KiB chunk mapped up front (the steady state never maps again), eight size buckets over two parallel managed arrays) |
| `@npk_frame_free` | syscall | boundary (returns a frame: a null or misaligned pointer, or a header that does not validate as frame-live, is HeapBadRequest; a dedicated block goes back to the heap whole, a bucketed one is stamped frame-free and pushed on its size bucket's list, a new bucket appended (the parallel arrays grown by ralloc) when its size has none) |
| `@npk_frozen_get` | atomic | specified (2 discharged, 0 residue) |
| `@npk_fs_alloc` | syscall | boundary (a block of n bytes at the requested alignment bumped from the one-mebibyte failsafe region with the heap's [ size | 0 ] header before it, single-threaded by construction since every other thread is parked before failsafe runs (D-291); exhaustion is HeapOom inside failsafe, the re-entry rule's exit 70 (D-292)) |
| `@npk_guard_release` | syscall | boundary (releases an exclusive hold: the state word to free and a wake by the cell's kind (a mutex wakes one waiter, an rwlock's writer release wakes all -- a crowd of readers may proceed together)) |
| `@npk_hardware_concurrency` | syscall | boundary (the number of hardware threads the program may use (D-073): the popcount of the sched_getaffinity mask over 1024 bits, at least 1) |
| `@npk_heap_bad` | syscall | specified (2 discharged, 0 residue) |
| `@npk_heap_badreq` | syscall | specified (2 discharged, 0 residue) |
| `@npk_heap_init` | syscall | boundary (the heap's first initialisation, under the mutex (D-290): the hash secret from getrandom (a zero draw takes a fixed odd constant -- weaker keying for one run in 2^64, never a stall), the large table mapped, the class tables' derived words computed) |
| `@npk_heap_oom` | syscall | specified (2 discharged, 0 residue) |
| `@npk_hmap` | syscall | boundary (an anonymous private read-write mapping of len bytes (mmap), or HeapOom when the kernel refuses (an answer past 2^64 - 4096)) |
| `@npk_hs_arm` | pure | specified (3 discharged, 0 residue); residue (which environment entry arms the flag is not decided: a match is an existential over the entries, and the scan is bounded at 64 entries (its rows read [<=64])) |
| `@npk_hs_note_alloc` | pure | specified (5 discharged, 0 residue) |
| `@npk_hs_note_free` | pure | specified (2 discharged, 0 residue) |
| `@npk_hs_note_resize` | pure | specified (5 discharged, 0 residue) |
| `@npk_hs_put_dec` | pure | specified (23 discharged, 0 residue) |
| `@npk_hs_put_str` | pure | specified (5 discharged, 0 residue) |
| `@npk_hs_report` | syscall | boundary (the NPK_HEAP_STATS line, written to fd 2 exactly once per process at exit when the flag is armed: read without the heap mutex by design (a diagnostic, never a verdict; the shared-state exemption says why)) |
| `@npk_hunmap` | syscall | boundary (munmap of a mapping the heap made; a refusal means the table and the kernel disagree about what is mapped -- an integrity failure, HeapBadRequest, not an OOM) |
| `@npk_in_fs` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_int_to_string` | syscall | specified (7 discharged, 1 residue); residue (the digit bytes are the same computation as npk_hs_put_dec's, whose twenty rows state them; here the length, the capacity, the zero and the sign are rows -- and the sign row is not decided under the profile (unknown at ten times the budget): the sign byte reaches the block's base through the rehome copy's twenty unrolled loads, whose addresses the solver must relate to the sign store's wrapped one) |
| `@npk_io_register` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_io_unwatch` | syscall | boundary (epoll_ctl DEL of the descriptor from the executor's epoll instance, so a later readiness can never write into a freed registration; ENOENT (never watched, or closed -- the kernel removed it) is the no-op answer, and with no reactor armed there is nothing to remove) |
| `@npk_join_deadline` | syscall | specified (2 discharged, 0 residue) |
| `@npk_large_check` | syscall | specified (2 discharged, 0 residue) |
| `@npk_large_new` | syscall | boundary (a large block: a page-rounded anonymous mapping of the header, the payload at the requested alignment and the size, registered in the large table with its base and mapping size; the payload meets the alignment and the block fits its mapping, asserted where they are produced) |
| `@npk_lg_entry` | pure | specified (2 discharged, 0 residue) |
| `@npk_lg_find` | pure | specified (5 discharged, 0 residue) |
| `@npk_lg_insert` | syscall | boundary (inserts a large block's entry (payload, base, mapping size, size) into the large table sorted by payload, growing the table by a fresh mapping (the old one copied and unmapped) when full and moving the greater entries up one slot; sortedness is this symbol's construction) |
| `@npk_lg_remove` | pure | specified (8 discharged, 0 residue) |
| `@npk_m_chunk` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_ffree` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_flive` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_freed` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_guard` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_large` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_largew` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_live` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_livew` | pure | specified (2 discharged, 0 residue) |
| `@npk_m_wildx` | pure | specified (2 discharged, 0 residue) |
| `@npk_mono_now` | syscall | specified (3 discharged, 0 residue) |
| `@npk_mutex_acquire_wait` | syscall | boundary (the Mutex wait (1.1.11): under the cell's own futex, the frame linked on the cell's waiter list and parked until the releaser's wake or the absolute deadline, the acquire re-run when woken (a woken waiter may lose to a barger, which the deadline bounds); the frame unlinked on every completed path; 0 acquired, 1 the deadline) |
| `@npk_mx_lock` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_mx_unlock` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ofd_close` | syscall | boundary (the descriptor's drop: close(2) once, its answer discarded by design (D-185: a drop cannot fail); no memory is touched) |
| `@npk_open` | syscall | specified (3 discharged, 0 residue) |
| `@npk_park_forever` | syscall | boundary (a futex wait on a word nothing writes, re-waited on a spurious return: the stop handler's body and a losing trapper's end; the thread dies with the winner's exit_group; allocation-free by construction (D-291)) |
| `@npk_park_sleep` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_park_take` | syscall | specified (4 discharged, 0 residue) |
| `@npk_park_until` | syscall | specified (3 discharged, 0 residue) |
| `@npk_path_exists` | syscall | specified (3 discharged, 0 residue) |
| `@npk_raise` | syscall | specified (2 discharged, 0 residue) |
| `@npk_ralloc` | syscall | boundary (a block of n bytes holding the old block's bytes up to the smaller size, the old block's role (wild or managed) kept and the old block freed; ralloc(p, 0) is HeapBadRequest (D-150); during a failsafe a fresh region block with the old bytes copied by the header's size and the old block left (D-292)) |
| `@npk_read_file` | syscall | specified (9 discharged, 0 residue); residue (the bytes the kernel wrote are opaque (the kernel-effect table says only where), so the buffer's contents are not claimed; the growth's copy is memcpy's row in its own file) |
| `@npk_read_stdin` | syscall | specified (7 discharged, 0 residue); residue (as npk_read_file's: the kernel's bytes are opaque) |
| `@npk_read` | syscall | specified (5 discharged, 0 residue) |
| `@npk_reg_claim` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_reg_retire` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_rq_pop` | syscall | specified (4 discharged, 0 residue) |
| `@npk_rq_push` | syscall | specified (5 discharged, 0 residue) |
| `@npk_run_until` | syscall | boundary (the executor loop until the target task completes (0) or the absolute deadline dl expires (1; 0 is no deadline): every ready task stepped in queue order, the idle wait epoll_pwait carrying the earliest sleeper's deadline (1.1.12), the waker's due-now stamp consumed at resume) |
| `@npk_rw_read_wait` | syscall | boundary (the RwLock's read acquire: parked on the cell's waiter list while a writer holds it (state 1) or until the absolute deadline; N >= 2 is N-1 readers; 0 acquired, 1 the deadline) |
| `@npk_rw_release_read` | syscall | boundary (a reader's release: the count comes down under the cell's futex, and zero wakes the crowd -- a parked writer is in it) |
| `@npk_rw_write_wait` | syscall | boundary (the RwLock's write acquire: parked on the cell's waiter list while any holder exists or until the absolute deadline; 0 acquired, 1 the deadline) |
| `@npk_sarena_bump` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_destroy` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_make` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_slot` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_earliest` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_push` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_wake_due` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_small_alloc` | syscall | boundary (a slot of a small chunk of class ci under the heap mutex: the first clear bit of a partial chunk from the hint word on (a partial chunk with no clear bit is a bookkeeping violation and traps), a chunk moved to the full list when its last slot goes, a new chunk mapped when no partial one exists; the slot's header stamped by the wild role) |
| `@npk_small_check` | syscall | specified (14 discharged, 0 residue) |
| `@npk_small_free` | syscall | specified (7 discharged, 6 residue); residue (the five ensures and the frame are not decided under the profile (unknown at the budget; at ten times it two answer unknown and four do not answer within forty minutes): each claim over the state after the poison loop must separate its address from the tail's stores -- the stamp, the counter, the bitmap word, the chunk's counters and the list words -- through the class table's geometry, a fourteen-way case over the class index the solver does not finish; the validation's four preconditions, the poison loop's two rows and the no-trap row discharge) |
| `@npk_start` | syscall | boundary (the process entry, called by _start with the initial stack pointer: argv and envp measured, the main thread's TLS block and executor booted (npk_tls_boot), SIGUSR1 armed for the stop signal over the kernel's own sigaction shape (D-291), the driver registry cleared, main run to completion under the executor, then the exit sequence (npk_exit) -- the path never returns) |
| `@npk_step` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_stop_handler` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_stop_others` | atomic | boundary (signals every other registered thread (tgkill) and waits, under the executor's join deadline, for each to park in the stop handler; a thread the kernel did not interrupt in time is proceeded past (TCB.md SS5 item 6); it writes no memory of its own -- the count it waits on is the handlers' atomic word) |
| `@npk_string_concat` | syscall | specified (12 discharged, 0 residue) |
| `@npk_string_equals` | pure | specified (6 discharged, 0 residue) |
| `@npk_string_from_bytes` | pure | specified (4 discharged, 0 residue) |
| `@npk_string_slice` | syscall | specified (10 discharged, 0 residue) |
| `@npk_sys6` | asm | trusted (inline asm) |
| `@npk_task_done` | atomic | specified (2 discharged, 0 residue) |
| `@npk_thread_entry` | syscall | boundary (a spawned thread's first ordinary code, reached by the clone trampoline's real call (a child that continued in IR would read the parent's spilled stack): its TLS and executor booted from the trampoline's block, the task run to completion, the thread ended through npk_thread_exit) |
| `@npk_thread_exit` | syscall | boundary (exit(60) of the calling thread alone: the kernel clears and wakes the CLONE_CHILD_CLEARTID word the joiner waits on (a shared wake, 1.4.4); the path never returns) |
| `@npk_thread_join` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_thread_start` | asm | trusted (inline asm) |
| `@npk_tls_boot` | syscall | boundary (the main thread's TLS block and executor from raw anonymous mappings -- INTERNAL, not npk_alloc, so the runtime's own storage is never a leak at a clean exit (D-151) -- and %fs set by arch_prctl to the block) |
| `@npk_tls_self` | asm | trusted (inline asm) |
| `@npk_to_cstring` | syscall | specified (9 discharged, 0 residue) |
| `@npk_trap` | atomic | specified (2 discharged, 0 residue); residue (the call of npk_failsafe is opaque -- the program's handler is not the floor's (D-014); its result decides the exit status through npk_exit, whose promise is the boundary's) |
| `@npk_udivmod128` | pure | specified (4 discharged, 0 residue); residue (the division identity a = q*b + r is not decided under the profile: it needs 2^i, which the IR does not compute, and this writer invents no ghost variable (D-288); the b = 0 answer (q all ones, r = a) and the b = 1 answer (q = a, r = 0) are not decided either -- with the loop unwound 127 times (its bound proven exact in 0.03 s) every row over the whole computation exhausts the rlimit (q all ones unknown after 54 s, b = 1 after 149 s, r = a after 643 s, and even r < b after 652 s, which the invariant's step decides at 3,093), and the multiplier's equivalence is unknown in QF_BV at every width (measured 2026-09-11)) |
| `@npk_wild_live_count` | atomic | residue (the count of live wild blocks walks every chunk's watermarked slots and the large table -- three nested loops over the heap's structure, whose invariant would restate D-150's whole shape; the properties it rests on (header/payload disjointness, validate-before-dereference over the heap) are leg A's (D-233)) |
| `@npk_wild_release_all` | syscall | boundary (failsafe's controlled cleanup: every chunk and every large mapping returned to the kernel, the tables reset, the allocator left usable; after it only exit may follow (TYPE-062) -- anything still pointing into the heap points at unmapped pages) |
| `@npk_wildx_alloc` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_wildx_call` | pure | residue (an ordinary indirect call into a sealed wildx page: the analysis has proven the page sealed before this runs, and the contents are outside verification by construction (D-035) -- the call's effect on memory and its result are opaque, and no clause claims either) |
| `@npk_wildx_check` | syscall | specified (3 discharged, 0 residue) |
| `@npk_wildx_free` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_wildx_seal` | syscall | boundary (the ONE-WAY transition of a wildx page: mprotect to PROT_READ|PROT_EXEC; the page is never PROT_WRITE|PROT_EXEC, so W^X holds structurally) |
| `@npk_windup_all` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_windup_grace` | syscall | specified (2 discharged, 0 residue) |
| `@npk_windup_note` | syscall | specified (2 discharged, 0 residue) |
| `@npk_write_file` | syscall | specified (7 discharged, 0 residue) |
| `@npk_write` | syscall | specified (3 discharged, 0 residue) |
| `@npk_zero` | pure | specified (4 discharged, 0 residue) |
<!-- END floor-table -->

## 5. What a reader must accept

1. That the LLVM toolchain at the pinned patch release translates the emitted
   IR faithfully. The repro stage shows `llc` is deterministic on our input;
   1.6's leg C validates the optimizer's passes; nothing validates `llc`'s
   instruction selection.
2. That the kernel implements the syscalls the floor issues as documented.
3. That the pinned z3 build decides correctly what it reports as `unsat`. The
   manifest makes a moved verdict visible and fatal; it does not make the
   solver right.
4. That the encoding the compiler writes (`src/backend/smt/`) is sound: every
   hypothesis it asserts is a fact on every execution reaching the site
   (P-8's claim, stated in `smt_encode.npk`'s header) -- and, since 1.5.2,
   that a limited binding's rule holds on every execution past each of its
   write points, because each is guarded by that rule or discharged under
   these same hypotheses (D-251, L-7). 1.5.6 reads this claim against the
   floor's specification; K (VERIFICATION_REFERENCE §6.2) is the
   metatheoretic check on the language the encoding assumes.
5. That the committed snapshot (`bootstrap/seed/stage1.ll`) is what its STAMP
   says: D-085's diverse double-compilation is the Thompson-attack mitigation,
   and the fixpoint re-derives the snapshot from source on every full run.
6. That a thread the trap route signals stops before `failsafe` runs (D-291,
   1.5.6 step 1): the winner waits for every other live thread's handler to
   park, under the executor's join deadline, and proceeds past a thread the
   kernel did not interrupt in time -- a state no user-space signal reaches.
   The thread registry holds 64 threads; the 65th is refused at its start.
7. That a `failsafe` body lives within the failsafe region (D-292, 1.5.6 step
   2): one mebibyte of `.bss`, bumped, never freed, exhausted at the re-entry
   exit 70 -- a diagnostic built by repeated concatenation is quadratic there.
8. That the kernel-effect table (VERIFICATION_REFERENCE §9.2; 1.5.6 step
   4) is what the syscalls the floor issues do: the answer a value or an
   errno in `[-4095, -1]`, `read`/`write`/`getrandom` answering at most the
   count asked and `epoll_pwait` at most the events room, and each number's
   memory effect as the table states it (`read`, `getrandom`,
   `clock_gettime`, `sched_getaffinity`, `rt_sigaction`, `epoll_pwait`
   writing where and how much it says, `mmap` a fresh mapping zero when
   anonymous, `exit`/`exit_group` ending the process, the rest leaving
   memory as it was). A syscall whose effect the table understates is an
   unsound proof; the table is read against the kernel's documentation.
9. That `npk_exec()` and `npk_tls_self()` answer the calling thread's own
   executor and trampoline block -- one object each of its struct's size,
   apart from every other object a symbol touches -- and that a symbol's
   `(objects …)` clause states its callers' discipline: every listed range
   lies in the address space and the listed ranges are pairwise disjoint (a
   null or an empty range is no object). The floor's own callers are held to
   a `(summary)` symbol's objects by rows at each call; a caller outside the
   floor -- the emitted code -- keeps them by D-201's table and the
   allocator's discipline (leg A).
10. That the allocator keeps the promise its summary states (1.5.6 step 4;
    `npk_alloc_internal`, and `npk_dalloc` as the envelope symbols call
    it): a fresh block, its header included, is disjoint from every object
    the caller names, every live buffer its loops carry and every earlier
    block of the caller's, those objects are unchanged by the call, the
    block's header word is the size asked, and a request at or past 2^63 is
    the trap; a free changes the block, its header and the heap's own words
    and nothing the caller names. D-150's header/payload disjointness and
    validate-before-dereference over the whole heap are the invariants leg
    A (D-233) carries; `npk_small_check`'s section states what "validated"
    means (the class index, the slot's position, the stride boundary, the
    watermark, the slot count) and its rows prove it from the class tables.
11. That the class tables `npk_cls_size`/`_slots`/`_bmw`/`_data`/`_guard`
    are the geometry the chunk layout assumes: `64 + 8·bmw ≤ data`, `data +
    slots·(size + 16) ≤ guard` and `guard + 16 ≤ 65536` for every class
    (checked by hand at step 4; a table load reads as the `ite` over the
    index, so a row that needs a class's numbers has them by the case).
    (The remaining acceptances 1.5.6 owes -- the modelling assumption, the
    bounds, the two opaque calls -- land with steps 5 and 6.)

Nothing else is trusted. In particular nothing in `src/`, `lib/` or the prelude
is exempt from the checks that bind a user program (D-205's switch put the
compiler's own source under them), and no C, C++, Rust, Python or third-party
code is in any artifact (the zero-dependency rule, enforced by the closed-world
link and the undefined-symbol scan on every object).
