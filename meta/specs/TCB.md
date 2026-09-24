# The trusted computing base

Drafted at 1.5.0 (2026-09-03) under D-218.11, in r8's terms (the digest is
`meta/roadmap/research/digests/r5r8-digest.md`; read its reliability notes
before citing further). This is the document an auditor reads first: what
the evidence campaign PROVES, what it TESTS, and what it must TAKE ON TRUST —
enumerated, never implied. **It was FINALIZED at 1.5.6 (2026-09-12)**, when
the floor got its specification, its protocol models and its syscall
boundary: THREE marked regions are now generated from the tree and held to
it by both runners — the membership table with each symbol's class and
disposition (§4, `check_tcb_floor_current` / `tcb_floor_current`), the
syscall boundary (§4b, `check_tcb_syscalls_current` / `tcb_syscalls_current`)
and what the evidence does not cover (§4c, `check_tcb_residue_current` /
`tcb_residue_current`). None can go stale without a red run, and 1.6.1's
leg-A analyzer model reads all three: the classes say what kind of symbol it
faces, the syscall table the boundary it must not cross, the residue list
what is still owed.

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
runners, the `undef` ban -- and, since 1.5.7, the SCHEDULE EXPLORER
(VERIFICATION_REFERENCE §10): the real floor and each program's own IR,
transformed, run under a seeded scheduler that chooses every interleaving,
1,000 seeds per concurrency test, with quiescence oracles that see a lost
wakeup an exit code cannot, and the spec's caller hypotheses executed at every
call. Its first floor find was DEF-57.

## 3. Trusted components, enumerated

| Component | Pinned by | Why it is trusted rather than proven |
|---|---|---|
| LLVM 20.1.2 `llc`, `ld.lld` (and `opt` on the -O2 path) | `nitpick.toml` `[toolchain]`, an exact patch release, every invocation built from its flag lists (D-204) | the translation of IR to machine code is outside the verified boundary (D-067); every verified toolchain short of CompCert has one. Leg C (1.6) validates the optimizer's passes; the opt-O2 leg tests the whole; `llc` itself is not validated |
| the Linux kernel's syscall ABI (x86_64) | the floor's one trampoline, `@npk_sys6`, and the `module asm` clone | the boundary at which a value becomes a syscall argument; the floor's `syscall` rows below are specified AT this boundary (1.5.6), never past it. **§4b enumerates the surface**: which symbol issues which number, and which numbers each reaches on its own paths. What the kernel PROMISES for each number is the kernel-effect table the translator applies (`npkg/floor_smt.npk`, one row per number, listed in VERIFICATION_REFERENCE §9.2); a number the floor issues without a row there fails the run, so the trust is enumerated rather than assumed |
| z3 4.16.0, the workbench build of tag `z3-4.16.0` | `[verify]` `z3-sha256` (the binary's hash), `z3-version`, `z3-options` (D-218.1/D-218.2) | an EVIDENCE tool: a solver defect is a wrong verdict, and a wrong `discharged` elides a guard. Mitigations: the pin (one build, one hash), the profile (a verdict is a function of the obligation, the build and the budget; the Diophantine sub-solver is off since 1.5.6 step 4 — S-71 — because its undo at `(pop)` was the one place the pinned build did not return), the committed manifest (a verdict that moves is a red run), `--explain`'s unsat cores on request; proof certificates are D-040's opt-in for certification runs |
| the leg-A analyzer and Alive2 (1.6) | commit hash, built on the workbench | the same doctrine as z3's (D-233): pinned, auditable, verdicts committed, a new alarm on an unchanged tree a stop sign |
| the floor's volatile bottom | this file's table, class `asm` | inline assembly and the clone: the seL4 precedent — handwritten assembly and volatile accesses are documented as the bottom of the TCB, not proven |

## 4. The floor, enumerated

`runtime/npkrt.ll` is hand-written LLVM IR, permanent (D-203), linked into
every artifact. Every `define` in it is one row below, classified by what its
body does — the classifier is `bootstrap/harness/harness.py`'s
`_floor_classes`, and `check_tcb_floor_current` fails the run when this table
and the floor disagree. **The third column is the symbol's DISPOSITION**,
generated from `runtime/npkrt.spec`, `runtime/models/` and the committed
`runtime/npkrt.obligations`: `specified (N discharged, M residue)` for a
symbol whose section yields rows, `modelled (…)` for one a protocol model's
steps name, `boundary (…)` and `residue (…)` in the spec's own words where a
claim is deliberately not made, and `trusted (inline asm)` for the volatile
bottom. A symbol whose disposition drifts from its rows is a red run:

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
until step 3 held the two runners' classifiers to one answer. Since 1.5.8
step 2b (DEF-64) the table also lists every symbol a `module asm` block
defines, as `trusted (module asm)`. These are the assembler's input: no
translator reads them and no transform rewrites them. The syscall table
below counts their syscalls as well; until that step no census saw them,
and `rt_sigreturn` and the clone trampoline's `clone` and `exit` were
missing from the numbers and from the rows that reach them.

<!-- BEGIN floor-table -->
| symbol | class | disposition |
|---|---|---|
| `@__divti3` | pure | specified (3 discharged, 0 residue) |
| `@__modti3` | pure | specified (4 discharged, 0 residue) |
| `@__morestack_non_split` | asm | trusted (module asm) |
| `@__morestack` | asm | trusted (module asm) |
| `@__udivti3` | pure | specified (2 discharged, 0 residue) |
| `@__umodti3` | pure | specified (3 discharged, 0 residue) |
| `@_start` | asm | trusted (module asm) |
| `@fmod` | pure | specified (7 discharged, 0 residue); residue (the reduction's result is not decided under the profile: |result| < |b| on the finite path is unknown with the loops unwound 2 and 8 times (z3's floating-point theory bit-blasts every fmul and fsub of the chain; measured 2026-09-11); the special values are rows) |
| `@fmodf` | pure | specified (7 discharged, 0 residue); residue (as fmod's: the reduction's result is not decided under the profile (unknown with the loops unwound 2 and 8 times, measured 2026-09-11); the special values are rows) |
| `@memcpy` | pure | specified (5 discharged, 0 residue) |
| `@memmove` | pure | specified (7 discharged, 0 residue) |
| `@memset` | pure | specified (5 discharged, 0 residue) |
| `@npk_aalloc` | syscall | boundary (the aligned entry: a request above 2^47 bytes (D-308's ceiling, checked here as well because the over-aligned path reaches npk_large_new without passing npk_alloc_impl's compare -- 1.5.8b step 6c) or a non-power-of-two alignment is HeapBadRequest; at or below sixteen the ordinary path, above it a mapping through npk_large_new at that alignment under the heap mutex (initialised there, locked -- D-290); during a failsafe the region at the alignment (D-292)) |
| `@npk_alloc_impl` | syscall | boundary (the allocator's core: during a failsafe a bump from the region with no mutex and no tables (D-292); otherwise, under the heap mutex, a slot of a small chunk for n <= the largest class or a fresh mapping through npk_large_new, the header stamped live, the stats counters noted; a request above 2^47 bytes -- D-308's CEILING, the x86-64 user address space, one unsigned compare that reads a negative size as a huge one (1.5.8b step 6c) -- is HeapBadRequest and a refused mapping HeapOom; alloc(0) is a real block (D-150)); modelled (trap-route) |
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
| `@npk_ch_at` | atomic | modelled (channel-table) |
| `@npk_ch_close` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_closed` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_get` | atomic | modelled (channel-table) |
| `@npk_ch_lock` | syscall | boundary (a channel's own futex mutex (slot 8): the CAS from 0 to 1, else a futex wait on the word until it is 0 again; single-threaded reasoning past the acquire is the shared-state rule's (D-290)); modelled (channel-table) |
| `@npk_ch_open` | atomic | modelled (channel-table) |
| `@npk_ch_push` | pure | specified (5 discharged, 0 residue) |
| `@npk_ch_reclaim` | atomic | modelled (channel-table) |
| `@npk_ch_recv_wait` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_send_wait` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_try_recv` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_try_send` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_unlink` | pure | specified (6 discharged, 0 residue) |
| `@npk_ch_unlock` | syscall | boundary (the channel mutex's release: the word stored 0 and one waiter woken (futex wake)); modelled (channel-table) |
| `@npk_ch_wait_link` | syscall | specified (3 discharged, 0 residue); modelled (park-unpark) |
| `@npk_ch_wait_unlink` | syscall | specified (2 discharged, 0 residue); residue (an unlink from deeper in the waiter list than the head is not decided: the walk is bounded at eight links and the list's reachability is no first-order clause; the head case is a row) |
| `@npk_ch_wake_all` | syscall | boundary (wakes every frame on the waiter list at hp, one at a time through npk_ch_wake_one, until the list is empty: each due-marked and its executor's eventfd written) |
| `@npk_ch_wake_one` | atomic | modelled (park-unpark) |
| `@npk_chain_depth` | syscall | specified (2 discharged, 0 residue) |
| `@npk_chain_push` | syscall | specified (3 discharged, 0 residue) |
| `@npk_chain_reset` | syscall | specified (3 discharged, 0 residue) |
| `@npk_chain_site` | syscall | specified (4 discharged, 0 residue) |
| `@npk_chtab_find` | pure | specified (5 discharged, 0 residue) |
| `@npk_chtab_insert` | syscall | boundary (inserts a chunk's address into the sorted chunk table, growing the table by a fresh mapping when full and moving the greater entries up one slot; sortedness -- what npk_chtab_find's requires assumes -- is this symbol's construction) |
| `@npk_chunk_guard_check` | syscall | specified (2 discharged, 0 residue) |
| `@npk_chunk_new` | syscall | boundary (a fresh 64 KiB-aligned chunk for class ci: a 128 KiB anonymous mapping trimmed to the aligned 64 KiB (the surplus unmapped), its header (the guard word, the class, the slot and free counts, the hint, the list links, the watermark) and its bitmap written, the chunk registered in the sorted chunk table) |
| `@npk_clone_exec` | atomic | modelled (driver-registry) |
| `@npk_clone_raw` | asm | trusted (module asm) |
| `@npk_close` | syscall | specified (3 discharged, 0 residue) |
| `@npk_cstr_slice` | syscall | boundary (the bytes of a NUL-terminated string the kernel wrote (argv, envp) as {ptr, len}: a strlen over the kernel's memory, the bytes themselves never copied) |
| `@npk_cv_begin` | syscall | boundary (the CondVar wait's entry: the frame linked on the cv's waiter list under the cv's futex, the guard's mutex released (the ordinary guard release), the frame due at the caller's absolute deadline -- the park itself is the executor's) |
| `@npk_cv_broadcast` | syscall | boundary (wakes every waiter on the cv's list under the cv's futex (npk_ch_wake_all)) |
| `@npk_cv_done` | syscall | boundary (unlinks the frame from the cv's waiter list under the cv's futex, on every completed path -- idempotent like every unlink) |
| `@npk_cv_signal` | syscall | boundary (wakes one waiter on the cv's list under the cv's futex (npk_ch_wake_one)) |
| `@npk_dalloc` | syscall | boundary (the free: a null or misaligned pointer is Unreachable (-4102 through npk_heap_bad, the integrity trap -- DEF-76: this sentence said HeapBadRequest until 1.5.8b step 6c, and the IR never did) (dalloc(NULL) traps by D-150), a block whose header does not validate is Unreachable (-4102 through npk_heap_bad, the integrity trap -- DEF-76: this sentence said HeapBadRequest until 1.5.8b step 6c, and the IR never did), a small block returns to its chunk (npk_small_free) and a large one to the kernel (munmap) under the heap mutex; during a failsafe, after the null and alignment checks, a no-op (D-292)) |
| `@npk_driver_kill_all` | atomic | boundary (sends SIGKILL through the pidfd of every live driver in the registry (pidfd_send_signal: allocation-free, mask-independent, safe against pid reuse); reaping is nobody's business on the trap path; it writes no memory); modelled (driver-registry, trap-route) |
| `@npk_driver_live_count` | atomic | modelled (driver-registry) |
| `@npk_driver_retire` | atomic | modelled (driver-registry) |
| `@npk_environ` | pure | specified (3 discharged, 0 residue) |
| `@npk_exec` | asm | trusted (inline asm) |
| `@npk_exit` | atomic | boundary (the controlled shutdown (D-013/D-014, D-151): a successful exit with live wild storage or a live driver routes to failsafe, a non-holder's exit during a failsafe parks, the heap-stats line is written, and the process ends by exit_group -- the path never returns); modelled (trap-route) |
| `@npk_failsafe_on_stack` | asm | trusted (inline asm) |
| `@npk_fault_arm` | syscall | boundary (THE LAST NET (D-307, 1.5.8 step 3): rt_sigaction for SIGSEGV, SIGBUS, SIGILL and SIGFPE -> npk_fault_handler, with SA_SIGINFO | SA_ONSTACK | SA_RESTORER | SA_NODEFER over the kernel's own sigaction shape and npk_sigreturn as the restorer, and for SIGPIPE -> npk_pipe_handler, which returns, with SA_RESTORER | SA_ONSTACK | SA_RESTART (DEF-68: a write to a pipe with no reader answers EPIPE instead of killing the process; a handler, not SIG_IGN, because an ignored disposition survives execve into every child); the old actions not read; a refusal traps -4102 at startup. What the kernel then promises -- that a synchronous fault in a thread is delivered to that thread, on its registered signal stack, with the signal unblocked because the action says NODEFER -- is the boundary: TCB.md SS5 accepts it by name) |
| `@npk_fault_handler` | syscall | specified (2 discharged, 0 residue); modelled (trap-route) |
| `@npk_frame_alloc` | syscall | boundary (a coroutine frame of size bytes at align (at or below the heap's sixteen): the exact-size bucket's free list first (the common steady state), else a bump from the current chunk, else a dedicated heap block whose header carries the dedicated bit; the header stamped frame-live) |
| `@npk_frame_bucket` | pure | specified (5 discharged, 0 residue) |
| `@npk_frame_drain` | pure | specified (4 discharged, 0 residue) |
| `@npk_frame_exec_destroy` | syscall | boundary (unmaps the frame arena's chunks and frees its bucket arrays) |
| `@npk_frame_exec_new` | syscall | boundary (a frame arena for an executor: one 64 KiB chunk mapped up front (the steady state never maps again), eight size buckets over two parallel managed arrays) |
| `@npk_frame_free` | syscall | boundary (returns a frame: a null or misaligned pointer, or a header that does not validate as frame-live, is Unreachable (-4102 through npk_heap_bad, the integrity trap -- DEF-76: this sentence said HeapBadRequest until 1.5.8b step 6c, and the IR never did); a dedicated block goes back to the heap whole, a bucketed one is stamped frame-free and pushed on its size bucket's list, a new bucket appended (the parallel arrays grown by ralloc) when its size has none) |
| `@npk_frozen_get` | atomic | specified (2 discharged, 0 residue); modelled (trap-route) |
| `@npk_fs_alloc` | syscall | boundary (a block of n bytes at the requested alignment bumped from the one-mebibyte failsafe region with the heap's [ size | 0 ] header before it, single-threaded by construction since every other thread is parked before failsafe runs (D-291); exhaustion is HeapOom inside failsafe, the re-entry rule's exit 70 (D-292)); modelled (trap-route) |
| `@npk_fs_switch_call` | asm | trusted (module asm) |
| `@npk_guard_release` | syscall | boundary (releases an exclusive hold: the state word to free and a wake by the cell's kind (a mutex wakes one waiter, an rwlock's writer release wakes all -- a crowd of readers may proceed together)) |
| `@npk_hardware_concurrency` | syscall | specified (6 discharged, 0 residue) |
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
| `@npk_hs_report` | syscall | boundary (the NPK_HEAP_STATS line, written to fd 2 exactly once per process at exit when the flag is armed: read without the heap mutex by design (a diagnostic, never a verdict; the shared-state exemption says why)); modelled (trap-route) |
| `@npk_hunmap` | syscall | boundary (munmap of a mapping the heap made; a refusal means the table and the kernel disagree about what is mapped -- an integrity failure, Unreachable (-4102 through npk_heap_bad, the integrity trap -- DEF-76: this sentence said HeapBadRequest until 1.5.8b step 6c, and the IR never did), not an OOM) |
| `@npk_in_fs` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_int_to_string` | syscall | specified (7 discharged, 1 residue); residue (the digit bytes are the same computation as npk_hs_put_dec's, whose twenty rows state them; here the length, the capacity, the zero and the sign are rows -- and the sign row is not decided under the profile (unknown at ten times the budget): the sign byte reaches the block's base through the rehome copy's twenty unrolled loads, whose addresses the solver must relate to the sign store's wrapped one) |
| `@npk_io_register` | atomic | modelled (park-unpark, reactor-io) |
| `@npk_io_unwatch` | syscall | boundary (epoll_ctl DEL of the descriptor from the executor's epoll instance, so a later readiness can never write into a freed registration; ENOENT (never watched, or closed -- the kernel removed it) is the no-op answer, and with no reactor armed there is nothing to remove); modelled (reactor-io) |
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
| `@npk_mx_lock` | atomic | modelled (futex-mutex, trap-route) |
| `@npk_mx_unlock` | atomic | modelled (futex-mutex, trap-route) |
| `@npk_ofd_close` | syscall | boundary (the descriptor's drop: close(2) once, its answer discarded by design (D-185: a drop cannot fail); no memory is touched) |
| `@npk_open` | syscall | specified (3 discharged, 0 residue) |
| `@npk_park_forever` | syscall | boundary (a futex wait on a word nothing writes, re-waited on a spurious return: the stop handler's body, a losing trapper's end, and the end of an executor that saw the frozen flag and is not the holder (DEF-57); the thread dies with the winner's exit_group; allocation-free by construction (D-291)); modelled (trap-route) |
| `@npk_park_sleep` | atomic | modelled (park-unpark, reactor-io) |
| `@npk_park_take` | syscall | specified (4 discharged, 0 residue); modelled (park-unpark, trap-route) |
| `@npk_park_until` | syscall | specified (3 discharged, 0 residue); modelled (park-unpark, trap-route) |
| `@npk_path_exists` | syscall | specified (3 discharged, 0 residue) |
| `@npk_pipe_handler` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_raise` | syscall | specified (2 discharged, 0 residue) |
| `@npk_ralloc` | syscall | boundary (a block of n bytes holding the old block's bytes up to the smaller size, the old block's role (wild or managed) kept and the old block freed; ralloc(p, 0) is HeapBadRequest (D-150); during a failsafe a fresh region block with the old bytes copied by the header's size and the old block left (D-292)) |
| `@npk_read_file` | syscall | specified (9 discharged, 0 residue); residue (the bytes the kernel wrote are opaque (the kernel-effect table says only where), so the buffer's contents are not claimed; the growth's copy is memcpy's row in its own file) |
| `@npk_read_stdin` | syscall | specified (7 discharged, 0 residue); residue (as npk_read_file's: the kernel's bytes are opaque) |
| `@npk_read` | syscall | specified (5 discharged, 0 residue) |
| `@npk_reg_claim` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_reg_publish` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_reg_reserve` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_reg_retire` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_rq_pop` | syscall | specified (4 discharged, 0 residue) |
| `@npk_rq_push` | syscall | specified (5 discharged, 0 residue) |
| `@npk_run_until` | syscall | boundary (the executor loop until the target task completes (0) or the absolute deadline dl expires (1; 0 is no deadline): every ready task stepped in queue order, the idle wait epoll_pwait carrying the earliest sleeper's deadline (1.1.12), the waker's due-now stamp consumed at resume) |
| `@npk_rw_read_wait` | syscall | boundary (the RwLock's read acquire: parked on the cell's waiter list while a writer holds it (state 1) or until the absolute deadline; N >= 2 is N-1 readers; 0 acquired, 1 the deadline) |
| `@npk_rw_release_read` | syscall | boundary (a reader's release: the count comes down under the cell's futex, and zero wakes the crowd -- a parked writer is in it) |
| `@npk_rw_write_wait` | syscall | boundary (the RwLock's write acquire: parked on the cell's waiter list while any holder exists or until the absolute deadline; 0 acquired, 1 the deadline) |
| `@npk_sarena_bump` | atomic | modelled (shared-arena) |
| `@npk_sarena_destroy` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_make` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_slot` | atomic | modelled (shared-arena) |
| `@npk_sigreturn` | asm | trusted (module asm) |
| `@npk_sigstack_on` | syscall | boundary (sigaltstack(&{ base, 0, 64 KiB }, NULL) for the calling thread (D-305 (4)); the kernel refuses a stack below the machine's minimum signal frame with ENOMEM, and that refusal traps -4102 at the thread's start -- the assumption that 64 KiB holds this machine's signal frame, checked where it is made) |
| `@npk_sl_earliest` | atomic | modelled (park-unpark) |
| `@npk_sl_push` | atomic | modelled (park-unpark, reactor-io) |
| `@npk_sl_wake_due` | atomic | modelled (park-unpark, reactor-io) |
| `@npk_small_alloc` | syscall | boundary (a slot of a small chunk of class ci under the heap mutex: the first clear bit of a partial chunk from the hint word on (a partial chunk with no clear bit is a bookkeeping violation and traps), a chunk moved to the full list when its last slot goes, a new chunk mapped when no partial one exists; the slot's header stamped by the wild role) |
| `@npk_small_check` | syscall | specified (14 discharged, 0 residue) |
| `@npk_small_free` | syscall | specified (7 discharged, 6 residue); residue (the five ensures and the frame are not decided under the profile (unknown at the budget; at ten times it two answer unknown and four do not answer within forty minutes): each claim over the state after the poison loop must separate its address from the tail's stores -- the stamp, the counter, the bitmap word, the chunk's counters and the list words -- through the class table's geometry, a fourteen-way case over the class index the solver does not finish; the validation's four preconditions, the poison loop's two rows and the no-trap row discharge) |
| `@npk_stack_exhausted` | syscall | specified (2 discharged, 0 residue) |
| `@npk_stack_foreign` | syscall | specified (2 discharged, 0 residue) |
| `@npk_stack_map` | syscall | boundary (one stack of the floor's shape (D-305, 1.5.8 step 2): `usable` bytes behind a PROT_NONE guard page -- with a signal stack and a second guard when `sig` is not 0 -- and the 64 KiB reserve below the limit word, one anonymous mapping through npk_hmap, each guard by mprotect (a guard that cannot be set traps -4102); the five words { base, length, signal stack, limit, top } written to `out`) |
| `@npk_start_main` | syscall | boundary (startup's remainder on the floor's stack (D-305 (3)): argv as npk_start measured it, `main` -- the first emitted function the main thread runs, whose prologue reads the limit word npk_start wrote -- and npk_exit with its answer; never returns) |
| `@npk_start` | asm | trusted (inline asm) |
| `@npk_std_fds` | syscall | boundary (THE STANDARD DESCRIPTORS ARE OPEN (DEF-69, 1.5.8 step 3c): for each of 0, 1, 2 in turn, fcntl(fd, F_GETFD); an open one is left as it is, and a closed one (EBADF) is opened onto /dev/null by openat(AT_FDCWD, `/dev/null`, O_RDWR, 0) -- read-write and inherited, as the three are -- which returns the lowest free number, that one, because every lower one is open by then; any other answer (a failed probe other than EBADF, /dev/null missing or refused, a descriptor table with no room) traps -4102 before `main` runs. So no descriptor the program or the floor creates is ever 0, 1 or 2 unless the program closed one of them itself) |
| `@npk_step` | atomic | modelled (park-unpark, reactor-io, trap-route) |
| `@npk_stop_handler` | atomic | modelled (trap-route) |
| `@npk_stop_others` | atomic | boundary (signals every other registered thread (tgkill) and waits, under the executor's join deadline, for each to park in the stop handler; a thread the kernel did not interrupt in time is proceeded past (TCB.md SS5 item 6); it writes no memory of its own -- the count it waits on is the handlers' atomic word); modelled (trap-route) |
| `@npk_string_concat` | syscall | specified (12 discharged, 0 residue) |
| `@npk_string_equals` | pure | specified (6 discharged, 0 residue) |
| `@npk_string_from_bytes` | pure | specified (4 discharged, 0 residue) |
| `@npk_string_slice` | syscall | specified (10 discharged, 0 residue) |
| `@npk_switch_stack` | asm | trusted (module asm) |
| `@npk_sys6` | asm | trusted (inline asm) |
| `@npk_task_done` | atomic | specified (2 discharged, 0 residue); modelled (park-unpark) |
| `@npk_thread_entry` | syscall | boundary (a spawned thread's first ordinary code, reached by the clone trampoline's real call (a child that continued in IR would read the parent's spilled stack): its signal stack registered first (npk_sigstack_on, from the TLS block's word the parent wrote; D-305 (4)), its TLS and executor booted from the trampoline's block, the task run to completion, the thread ended through npk_thread_exit) |
| `@npk_thread_exit` | syscall | boundary (exit(60) of the calling thread alone: the kernel clears and wakes the CLONE_CHILD_CLEARTID word the joiner waits on (a shared wake, 1.4.4); the path never returns) |
| `@npk_thread_join` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_thread_start` | asm | trusted (inline asm) |
| `@npk_tls_boot` | syscall | boundary (the main thread's TLS block and executor from raw anonymous mappings -- INTERNAL, not npk_alloc, so the runtime's own storage is never a leak at a clean exit (D-151) -- and %fs set by arch_prctl to the block) |
| `@npk_tls_self` | asm | trusted (inline asm) |
| `@npk_to_cstring` | syscall | specified (9 discharged, 0 residue) |
| `@npk_trap` | atomic | specified (2 discharged, 0 residue); residue (the call of npk_failsafe is opaque -- the program's handler is not the floor's (D-014); its result decides the exit status through npk_exit, whose promise is the boundary's); modelled (trap-route) |
| `@npk_udivmod128` | pure | specified (4 discharged, 0 residue); residue (the division identity a = q*b + r is not decided under the profile: it needs 2^i, which the IR does not compute, and this writer invents no ghost variable (D-288); the b = 0 answer (q all ones, r = a) and the b = 1 answer (q = a, r = 0) are not decided either -- with the loop unwound 127 times (its bound proven exact in 0.03 s) every row over the whole computation exhausts the rlimit (q all ones unknown after 54 s, b = 1 after 149 s, r = a after 643 s, and even r < b after 652 s, which the invariant's step decides at 3,093), and the multiplier's equivalence is unknown in QF_BV at every width (measured 2026-09-11)) |
| `@npk_wild_live_count` | atomic | residue (the count of live wild blocks walks every chunk's watermarked slots and the large table -- three nested loops over the heap's structure, whose invariant would restate D-150's whole shape; the properties it rests on (header/payload disjointness, validate-before-dereference over the heap) are leg A's (D-233)) |
| `@npk_wild_release_all` | syscall | boundary (failsafe's controlled cleanup: every chunk and every large mapping returned to the kernel, the tables reset, the allocator left usable; after it only exit may follow (TYPE-062) -- anything still pointing into the heap points at unmapped pages) |
| `@npk_wildx_alloc` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_wildx_call` | pure | residue (an ordinary indirect call into a sealed wildx page: the analysis has proven the page sealed before this runs, and the contents are outside verification by construction (D-035) -- the call's effect on memory and its result are opaque, and no clause claims either) |
| `@npk_wildx_check` | syscall | specified (3 discharged, 0 residue) |
| `@npk_wildx_free` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_wildx_seal` | syscall | boundary (the ONE-WAY transition of a wildx page: mprotect to PROT_READ|PROT_EXEC; the page is never PROT_WRITE|PROT_EXEC, so W^X holds structurally) |
| `@npk_windup_all` | atomic | modelled (park-unpark) |
| `@npk_windup_grace` | syscall | specified (2 discharged, 0 residue) |
| `@npk_windup_note` | syscall | specified (2 discharged, 0 residue) |
| `@npk_write_file` | syscall | specified (7 discharged, 0 residue) |
| `@npk_write` | syscall | specified (3 discharged, 0 residue) |
| `@npk_zero` | pure | specified (4 discharged, 0 residue) |
<!-- END floor-table -->

## 4b. The syscall boundary, enumerated (1.5.6 step 6; D-288 §2.8)

The kernel is trusted (§3), and this is the surface across which it is
trusted: every floor symbol that issues a syscall or reaches one, with the
numbers it issues itself and the numbers it reaches on its OWN paths. Every
symbol can also reach `exit_group` through a trap, which says nothing about
the symbol, so the trap route is named rather than counted. The table is
GENERATED from `runtime/npkrt.ll` (`bootstrap/harness/tcb_floor.py --write`)
and held by `check_tcb_syscalls_current` (harness) and
`tcb_syscalls_current` (npkg): a syscall added to the floor without this
document moving is a stale claim about the one boundary the kernel is
trusted across.

<!-- BEGIN floor-syscalls -->
The floor issues 30 syscall numbers, and no others: **0** read, **1** write, **3** close, **9** mmap, **10** mprotect, **11** munmap, **13** rt_sigaction, **15** rt_sigreturn, **39** getpid, **56** clone, **59** execve, **60** exit, **72** fcntl, **110** getppid, **131** sigaltstack, **157** prctl, **158** arch_prctl, **202** futex, **204** sched_getaffinity, **228** clock_gettime, **231** exit_group, **233** epoll_ctl, **234** tgkill, **257** openat, **281** epoll_pwait, **290** eventfd2, **291** epoll_create1, **292** dup3, **318** getrandom, **424** pidfd_send_signal. Each has one row in the
kernel-effect table -- the `kernel-effects` region of VERIFICATION_REFERENCE §9.2, generated
into `npkg/floor_kernel.npk` -- saying what it does to memory and to the result, and the
belt holds that sentence: a number without a row, or a call site whose option the row does
not speak for, is a finding. The rows that WRITE memory are held to the running kernel by
`tests/backend/programs/kernel_effects.npk`; what no probe reaches is what a reader accepts (§5).

| symbol | class | issues | reaches |
|---|---|---|---|
| `@npk_clone_raw` | asm | 56 clone, 60 exit | 56 clone, 60 exit |
| `@npk_sigreturn` | asm | 15 rt_sigreturn | 15 rt_sigreturn |
| `@__morestack` | asm | -- | the trap route only |
| `@__morestack_non_split` | asm | -- | the trap route only |
| `@_start` | asm | -- | 9 mmap, 10 mprotect, 13 rt_sigaction, 39 getpid, 72 fcntl, 131 sigaltstack, 158 arch_prctl, 257 openat, and the trap route |
| `@npk_cstr_slice` | syscall | -- | 9 mmap, and the trap route |
| `@npk_std_fds` | syscall | 72 fcntl, 257 openat | 72 fcntl, 257 openat, and the trap route |
| `@npk_start` | asm | 13 rt_sigaction | 9 mmap, 10 mprotect, 13 rt_sigaction, 39 getpid, 72 fcntl, 131 sigaltstack, 158 arch_prctl, 257 openat, and the trap route |
| `@npk_start_main` | syscall | -- | 1 write, 202 futex, 231 exit_group, and the trap route |
| `@npk_tls_boot` | syscall | 39 getpid, 158 arch_prctl | 9 mmap, 39 getpid, 158 arch_prctl, and the trap route |
| `@npk_mx_lock` | atomic | 202 futex | 202 futex |
| `@npk_mx_unlock` | atomic | 202 futex | 202 futex |
| `@npk_ch_lock` | syscall | -- | 202 futex |
| `@npk_ch_unlock` | syscall | -- | 202 futex |
| `@npk_ch_wake_one` | atomic | 1 write, 202 futex | 1 write, 202 futex |
| `@npk_ch_wake_all` | syscall | -- | 1 write, 202 futex |
| `@npk_ch_open` | atomic | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_ch_close` | atomic | -- | 1 write, 202 futex |
| `@npk_ch_closed` | atomic | -- | 202 futex |
| `@npk_ch_reclaim` | atomic | -- | 1 write, 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_mutex_acquire_wait` | syscall | -- | 202 futex, 228 clock_gettime, and the trap route |
| `@npk_guard_release` | syscall | -- | 1 write, 202 futex |
| `@npk_rw_release_read` | syscall | -- | 1 write, 202 futex |
| `@npk_rw_read_wait` | syscall | -- | 202 futex, 228 clock_gettime, and the trap route |
| `@npk_rw_write_wait` | syscall | -- | 202 futex, 228 clock_gettime, and the trap route |
| `@npk_cv_begin` | syscall | -- | 1 write, 202 futex |
| `@npk_cv_done` | syscall | -- | 202 futex |
| `@npk_cv_signal` | syscall | -- | 1 write, 202 futex |
| `@npk_cv_broadcast` | syscall | -- | 1 write, 202 futex |
| `@npk_barrier_arrive` | syscall | -- | 1 write, 202 futex |
| `@npk_barrier_poll` | syscall | -- | 202 futex, 228 clock_gettime, and the trap route |
| `@npk_barrier_cancel` | syscall | -- | 202 futex |
| `@npk_ch_recv_wait` | atomic | -- | 1 write, 202 futex, 228 clock_gettime, and the trap route |
| `@npk_ch_send_wait` | atomic | -- | 1 write, 202 futex, 228 clock_gettime, and the trap route |
| `@npk_ch_try_send` | atomic | -- | 1 write, 202 futex |
| `@npk_ch_try_recv` | atomic | -- | 1 write, 202 futex |
| `@npk_stack_map` | syscall | 10 mprotect | 9 mmap, 10 mprotect, and the trap route |
| `@npk_sigstack_on` | syscall | 131 sigaltstack | 131 sigaltstack, and the trap route |
| `@npk_fault_arm` | syscall | 13 rt_sigaction | 13 rt_sigaction, and the trap route |
| `@npk_fault_handler` | syscall | -- | the trap route only |
| `@npk_stack_exhausted` | syscall | -- | the trap route only |
| `@npk_stack_foreign` | syscall | -- | the trap route only |
| `@npk_thread_start` | asm | -- | 9 mmap, 10 mprotect, 56 clone, 60 exit, and the trap route |
| `@npk_thread_entry` | syscall | -- | 0 read, 131 sigaltstack, 202 futex, 228 clock_gettime, 281 epoll_pwait, and the trap route |
| `@npk_thread_exit` | syscall | 60 exit | 60 exit |
| `@npk_thread_join` | atomic | 3 close, 202 futex | 3 close, 11 munmap, 202 futex, 228 clock_gettime, and the trap route |
| `@npk_hardware_concurrency` | syscall | 204 sched_getaffinity | 204 sched_getaffinity |
| `@npk_clone_exec` | atomic | 56 clone, 59 execve, 110 getppid, 157 prctl, 231 exit_group, 292 dup3 | 56 clone, 59 execve, 110 getppid, 157 prctl, 231 exit_group, 292 dup3 |
| `@npk_driver_retire` | atomic | -- | the trap route only |
| `@npk_driver_kill_all` | atomic | 424 pidfd_send_signal | 424 pidfd_send_signal |
| `@npk_step` | atomic | -- | 0 read, 202 futex, 228 clock_gettime, 281 epoll_pwait, and the trap route |
| `@npk_run_until` | syscall | -- | 0 read, 202 futex, 228 clock_gettime, 281 epoll_pwait, and the trap route |
| `@npk_windup_all` | atomic | 1 write, 202 futex | 1 write, 202 futex |
| `@npk_park_sleep` | atomic | 0 read, 202 futex, 281 epoll_pwait | 0 read, 202 futex, 228 clock_gettime, 281 epoll_pwait, and the trap route |
| `@npk_io_register` | atomic | 233 epoll_ctl, 290 eventfd2, 291 epoll_create1 | 233 epoll_ctl, 290 eventfd2, 291 epoll_create1, and the trap route |
| `@npk_ofd_close` | syscall | 3 close | 3 close |
| `@npk_io_unwatch` | syscall | 233 epoll_ctl | 233 epoll_ctl |
| `@npk_mono_now` | syscall | 228 clock_gettime | 228 clock_gettime, and the trap route |
| `@npk_park_forever` | syscall | 202 futex | 202 futex |
| `@npk_stop_handler` | atomic | 202 futex | 202 futex |
| `@npk_stop_others` | atomic | 202 futex, 234 tgkill | 202 futex, 228 clock_gettime, 234 tgkill, and the trap route |
| `@npk_trap` | atomic | 231 exit_group | 1 write, 202 futex, 228 clock_gettime, 231 exit_group, 234 tgkill, 424 pidfd_send_signal |
| `@npk_hs_report` | syscall | 1 write | 1 write |
| `@npk_exit` | atomic | 231 exit_group | 1 write, 202 futex, 231 exit_group, and the trap route |
| `@npk_open` | syscall | 257 openat | 257 openat |
| `@npk_close` | syscall | 3 close | 3 close |
| `@npk_read` | syscall | 0 read | 0 read |
| `@npk_write` | syscall | 1 write | 1 write |
| `@npk_to_cstring` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_path_exists` | syscall | 3 close, 257 openat | 3 close, 257 openat |
| `@npk_write_file` | syscall | 1 write, 3 close, 257 openat | 1 write, 3 close, 257 openat |
| `@npk_read_file` | syscall | 0 read, 3 close, 257 openat | 0 read, 3 close, 9 mmap, 11 munmap, 202 futex, 257 openat, 318 getrandom, and the trap route |
| `@npk_read_stdin` | syscall | 0 read | 0 read, 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_hmap` | syscall | 9 mmap | 9 mmap, and the trap route |
| `@npk_hunmap` | syscall | 11 munmap | 11 munmap, and the trap route |
| `@npk_heap_init` | syscall | 318 getrandom | 9 mmap, 318 getrandom, and the trap route |
| `@npk_chtab_insert` | syscall | -- | 9 mmap, 11 munmap, and the trap route |
| `@npk_lg_insert` | syscall | -- | 9 mmap, 11 munmap, and the trap route |
| `@npk_chunk_new` | syscall | -- | 9 mmap, 11 munmap, and the trap route |
| `@npk_chunk_guard_check` | syscall | -- | the trap route only |
| `@npk_small_check` | syscall | -- | the trap route only |
| `@npk_small_alloc` | syscall | -- | 9 mmap, 11 munmap, and the trap route |
| `@npk_small_free` | syscall | -- | the trap route only |
| `@npk_large_new` | syscall | -- | 9 mmap, 11 munmap, and the trap route |
| `@npk_large_check` | syscall | -- | the trap route only |
| `@npk_fs_alloc` | syscall | -- | the trap route only |
| `@npk_alloc_impl` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_alloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_alloc_managed` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_buffer_new` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_alloc_internal` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_calloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_ralloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_dalloc` | syscall | -- | 11 munmap, 202 futex, and the trap route |
| `@npk_aalloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_wild_release_all` | syscall | -- | 11 munmap, and the trap route |
| `@npk_arena_make` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_arena_alloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_wildx_alloc` | atomic | 9 mmap, 10 mprotect | 9 mmap, 10 mprotect, 202 futex, 318 getrandom, and the trap route |
| `@npk_wildx_check` | syscall | -- | the trap route only |
| `@npk_wildx_seal` | syscall | 10 mprotect | 10 mprotect, and the trap route |
| `@npk_wildx_free` | atomic | -- | 11 munmap, and the trap route |
| `@npk_sarena_make` | atomic | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_sarena_bump` | atomic | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_sarena_destroy` | atomic | -- | 11 munmap, 202 futex, and the trap route |
| `@npk_frame_exec_new` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_frame_alloc` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_frame_free` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_frame_exec_destroy` | syscall | -- | 11 munmap, 202 futex, and the trap route |
| `@npk_arena_destroy` | syscall | -- | 11 munmap, 202 futex, and the trap route |
| `@npk_string_concat` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_int_to_string` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
| `@npk_string_slice` | syscall | -- | 9 mmap, 11 munmap, 202 futex, 318 getrandom, and the trap route |
<!-- END floor-syscalls -->

## 4c. What the floor's evidence does not cover (1.5.6 step 6)

Generated beside the other two, and for the same reason: the list a reader is
likeliest to skip is the one that matters most. `budget` is a row the pinned
z3 did not decide under the profile — the verdict rule keeps it as RESIDUE,
never as a proof — and a `(residue "…")` sentence is a claim the spec
deliberately does not make. Both are here, with the models' standing bounds.

<!-- BEGIN floor-residue -->
**Rows the profile did not decide** (`budget`; the verdict rule keeps them as
residue, never as a proof). 7 of them:

- `@npk_int_to_string` -- 1 row(s); the section's `(residue ...)` sentence says why.
- `@npk_small_free` -- 6 row(s); the section's `(residue ...)` sentence says why.

**Sentences the spec carries**, each a claim NOT made:

- `@npk_udivmod128` -- the division identity a = q*b + r is not decided under the profile: it needs 2^i, which the IR does not compute, and this writer invents no ghost variable (D-288); the b = 0 answer (q all ones, r = a) and the b = 1 answer (q = a, r = 0) are not decided either -- with the loop unwound 127 times (its bound proven exact in 0.03 s) every row over the whole computation exhausts the rlimit (q all ones unknown after 54 s, b = 1 after 149 s, r = a after 643 s, and even r < b after 652 s, which the invariant's step decides at 3,093), and the multiplier's equivalence is unknown in QF_BV at every width (measured 2026-09-11)
- `@fmod` -- the reduction's result is not decided under the profile: |result| < |b| on the finite path is unknown with the loops unwound 2 and 8 times (z3's floating-point theory bit-blasts every fmul and fsub of the chain; measured 2026-09-11); the special values are rows
- `@fmodf` -- as fmod's: the reduction's result is not decided under the profile (unknown with the loops unwound 2 and 8 times, measured 2026-09-11); the special values are rows
- `@npk_ch_wait_unlink` -- an unlink from deeper in the waiter list than the head is not decided: the walk is bounded at eight links and the list's reachability is no first-order clause; the head case is a row
- `@npk_hs_arm` -- which environment entry arms the flag is not decided: a match is an existential over the entries, and the scan is bounded at 64 entries (its rows read [<=64])
- `@npk_small_free` -- the five ensures and the frame are not decided under the profile (unknown at the budget; at ten times it two answer unknown and four do not answer within forty minutes): each claim over the state after the poison loop must separate its address from the tail's stores -- the stamp, the counter, the bitmap word, the chunk's counters and the list words -- through the class table's geometry, a fourteen-way case over the class index the solver does not finish; the validation's four preconditions, the poison loop's two rows and the no-trap row discharge
- `@npk_trap` -- the call of npk_failsafe is opaque -- the program's handler is not the floor's (D-014); its result decides the exit status through npk_exit, whose promise is the boundary's
- `@npk_wild_live_count` -- the count of live wild blocks walks every chunk's watermarked slots and the large table -- three nested loops over the heap's structure, whose invariant would restate D-150's whole shape; the properties it rests on (header/payload disjointness, validate-before-dereference over the heap) are leg A's (D-233)
- `@npk_read_file` -- the bytes the kernel wrote are opaque (the kernel-effect table says only where), so the buffer's contents are not claimed; the growth's copy is memcpy's row in its own file
- `@npk_read_stdin` -- as npk_read_file's: the kernel's bytes are opaque
- `@npk_int_to_string` -- the digit bytes are the same computation as npk_hs_put_dec's, whose twenty rows state them; here the length, the capacity, the zero and the sign are rows -- and the sign row is not decided under the profile (unknown at ten times the budget): the sign byte reaches the block's base through the rehome copy's twenty unrolled loads, whose addresses the solver must relate to the sign store's wrapped one
- `@npk_wildx_call` -- an ordinary indirect call into a sealed wildx page: the analysis has proven the page sealed before this runs, and the contents are outside verification by construction (D-035) -- the call's effect on memory and its result are opaque, and no clause claims either

**The models' residue** (VERIFICATION_REFERENCE SS9.4): every model is read twice --
bounded, by the solver (its rows hold to their own depth and preemption bound and no
further), and exhaustively, by explicit-state search over its whole reachable space
(D-295), which is where the SAFETY claim rests: no bad state is reachable in any model
below, inside its bounds or outside them. LIVENESS is not claimed at all -- that a due
task is eventually run, and that the shared arena's walker stops spinning, need a
fairness assumption neither reading can state.
The models, each with its bounds and its reachable states (and how many of them the bounds reach): `channel-table` (K 12, D 6; 151 states, 141 inside), `driver-registry` (K 10, D 5; 414 states, 398 inside), `futex-mutex` (K 11, D 6; 350 states, 350 inside), `park-unpark` (K 14, D 5; 358 states, 314 inside), `reactor-io` (K 16, D 7; 68 states, 68 inside), `shared-arena` (K 12, D 5; 1086 states, 1086 inside), `trap-route` (K 14, D 6; 852 states, 852 inside).
<!-- END floor-residue -->

## 4d. Who keeps a section's assumptions (1.5.6c step 3; leads E-1 and E-2)

A section's `requires`, `(objects …)` and `(views …)` are HYPOTHESES of its
rows: the rows are decided under them, so for a call that does not keep them
the rows say nothing. A translated caller of a `(summary)` symbol PROVES them,
as rows at the call. A translated caller of any other symbol INLINES it, so the
caller's own rows cover the body in the caller's context, under the caller's
hypotheses. Every other call is checked by NOTHING: a floor caller that is not
translated (a boundary symbol, a symbol with no claims), and EMITTED code —
the emitter's own calls and LLVM's lowering (`llvm.memcpy` becomes a call of
`memcpy`) reach whatever the floor EXPORTS, a `define` that is not `internal`.

The table is generated from the floor and the spec, because the hand-written
account of it was wrong twice in eleven sections (1.5.6c step 2's record) and
because it is the list of what §5's sixteenth acceptance asks for. Where an
assumption is STRUCTURAL — two ranges that cannot coincide — the argument for
it is written in `runtime/npkrt.spec` beside the clause.

<!-- BEGIN floor-callers -->
31 sections have rows AND assume something of their caller. For 2 of them every caller is covered -- no
untranslated floor caller, and the symbol is not exported; 18 have a floor caller no row covers; 15 are
EXPORTED, so emitted code can call them and nothing proves the assumption there. Under the explorer (1.5.7
step 5, D-302) 236 of the 240 hypotheses these sections state are EXECUTED at every call of every explored
schedule by a generated entry checker, untranslated floor callers and emitted code alike; the 4 it cannot
evaluate are listed by name below the table.

| symbol | assumes | a row at the call | inlined into | NOT PROVED: floor callers | NOT PROVED: emitted code | executed at every explored call |
|---|---|---|---|---|---|---|
| `@memcpy` | requires | -- | `@npk_to_cstring` `@npk_read_file` `@npk_read_stdin` `@memmove` `@npk_string_concat` | `@npk_ch_open` `@npk_ch_reclaim` `@npk_ch_recv_wait` `@npk_ch_send_wait` `@npk_ch_try_send` `@npk_ch_try_recv` `@npk_ralloc` | yes | 3 of 3 |
| `@memset` | requires | -- | -- | `@npk_buffer_new` | yes | 1 of 1 |
| `@npk_zero` | requires | -- | -- | `@npk_ch_open` | no | 1 of 1 |
| `@npk_string_equals` | requires | -- | -- | -- | yes | 2 of 2 |
| `@memmove` | requires | -- | -- | -- | yes | 2 of 2 |
| `@npk_rq_push` | requires objects | -- | -- | `@npk_thread_entry` `@npk_sl_wake_due` | yes | 7 of 7 |
| `@npk_rq_pop` | objects | -- | -- | `@npk_step` | yes | 3 of 3 |
| `@npk_ch_wait_link` | requires objects | -- | -- | `@npk_mutex_acquire_wait` `@npk_rw_read_wait` `@npk_rw_write_wait` `@npk_cv_begin` `@npk_barrier_poll` `@npk_ch_recv_wait` `@npk_ch_send_wait` | no | 7 of 7 |
| `@npk_ch_wait_unlink` | requires objects | -- | -- | `@npk_mutex_acquire_wait` `@npk_rw_read_wait` `@npk_rw_write_wait` `@npk_cv_done` `@npk_barrier_poll` `@npk_barrier_cancel` `@npk_ch_recv_wait` `@npk_ch_send_wait` | no | 68 of 68 |
| `@npk_hs_put_str` | requires | -- | -- | `@npk_hs_report` | no | 3 of 3 |
| `@npk_arena_at` | requires objects | -- | `@npk_arena_free` | -- | yes | 5 of 5 |
| `@npk_arena_free` | requires objects | -- | -- | -- | yes | 7 of 7 |
| `@npk_arena_reset` | requires objects | -- | -- | -- | yes | 5 of 5 |
| `@npk_frame_bucket` | requires | -- | -- | `@npk_frame_alloc` `@npk_frame_free` | no | 1 of 1 |
| `@npk_frame_drain` | objects | -- | -- | -- | yes | 1 of 1 |
| `@npk_chtab_find` | requires | `@npk_small_check` | -- | -- | no | 2 of 3 |
| `@npk_lg_find` | requires | -- | -- | `@npk_ralloc` `@npk_dalloc` | no | 2 of 3 |
| `@npk_lg_remove` | requires objects | -- | -- | `@npk_dalloc` | no | 8 of 8 |
| `@npk_ch_push` | requires objects | -- | `@npk_small_free` | `@npk_small_alloc` | no | 7 of 7 |
| `@npk_ch_unlink` | requires objects | -- | `@npk_small_free` | `@npk_small_alloc` | no | 11 of 11 |
| `@npk_small_free` | requires objects | -- | -- | `@npk_dalloc` | no | 70 of 71 |
| `@npk_hs_put_dec` | requires objects | -- | -- | `@npk_hs_report` | no | 2 of 2 |
| `@npk_chunk_guard_check` | requires | `@npk_small_check` | -- | -- | no | 2 of 2 |
| `@npk_small_check` | requires | `@npk_small_free` | -- | `@npk_ralloc` | no | 3 of 4 |
| `@npk_large_check` | requires | -- | -- | `@npk_ralloc` `@npk_dalloc` | no | 1 of 1 |
| `@npk_wildx_check` | requires | -- | -- | `@npk_wildx_seal` `@npk_wildx_free` | no | 1 of 1 |
| `@npk_read` | objects | -- | -- | -- | yes | 1 of 1 |
| `@npk_read_file` | objects | -- | -- | -- | yes | 1 of 1 |
| `@npk_to_cstring` | requires objects | -- | -- | -- | yes | 2 of 2 |
| `@npk_string_concat` | requires views | -- | -- | -- | yes | 5 of 5 |
| `@npk_string_slice` | requires objects | -- | -- | -- | yes | 2 of 2 |

Listed, not checked -- a clause the entry checker cannot evaluate over the entry state:
- `@npk_chtab_find`: `(requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len))) (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))` -- names `j`, which is not the entry state
- `@npk_lg_find`: `(requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_lgtab_len))) (< (load64 mem (+ (load64 mem npk_lgtab) (* 32 j))) (load64 mem (+ (load64 mem npk_lgtab) (* 32 k))))))` -- names `j`, which is not the entry state
- `@npk_small_free`: `(requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len))) (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))` -- names `j`, which is not the entry state
- `@npk_small_check`: `(requires (=> (and (<= 0 j) (< j k) (< k (load64 mem npk_chtab_len))) (< (load64 mem (+ (load64 mem npk_chtab) (* 8 j))) (load64 mem (+ (load64 mem npk_chtab) (* 8 k))))))` -- names `j`, which is not the entry state
<!-- END floor-callers -->

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
8. About the kernel-effect table (VERIFICATION_REFERENCE §9.2's
   `kernel-effects` region -- the ONE authority, generated into
   `npkg/floor_kernel.npk`; D-288 as amended, 1.5.6b), LESS than this item
   asked until then. It used to ask a reader to accept the whole table,
   "read against the kernel's documentation" -- and it named, among the rows
   "writing where and how much it says", the two that measurement then found
   WRONG (`sched_getaffinity`: the requested length, where the raw call writes
   `result` bytes, an over-approximation that hid DEF-52; `rt_sigaction`: the
   8-byte sigset size, where the kernel writes the whole 32-byte action, an
   under-approximation -- the unsound direction -- latent because the floor's
   one call passes a null `oldact`). No recorded verdict rested on either. The
   rows that WRITE memory are now HELD TO THE RUNNING KERNEL on every run, in
   both runners, by `tests/backend/programs/kernel_effects.npk`: each raw
   syscall over a sentinel-filled buffer, the bytes the kernel changed EQUAL to
   the row's on a success and none on a failure, the row's buffer and bound
   columns held with them. What a reader still accepts: the answer shape (a
   value or an errno in `[-4095, -1]`); the rows no probe reaches --
   `exit`/`exit_group` ending the process, the `asm` rows (`clone`, `execve`:
   issued only from inline asm, no effect modelled), the `none` rows for the
   options the floor passes (an option outside a row's set is refused by name,
   because `arch_prctl(ARCH_GET_FS)` and `prctl(PR_GET_NAME)` WRITE user
   memory), and failure paths beyond the ones probed; that the claim is about
   the kernel the suite ran on; and that "no memory effect" speaks of BYTES
   and not of mappings -- a load after `munmap` is modelled as the old bytes,
   so a use-after-unmap is outside the model by construction. A syscall whose
   effect the table understates is an unsound proof.
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
    block's header word is the size asked, and a request above 2^47 -- the
    address space, D-308's CEILING, one unsigned compare in `npk_alloc_impl`,
    `npk_fs_alloc` and `npk_aalloc` that reads a negative size as a huge one
    (1.5.8b step 6c; "at or past 2^63" until then) -- is the trap; a free changes the block, its header and the heap's own words
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
12. That an interleaving of the floor's atomic operations in
    SEQUENTIAL-CONSISTENCY order is the only observable one, for the
    protocols the models cover (1.5.6 step 5; VERIFICATION_REFERENCE §9.4).
    The argument is §2.9's shared-state classification, which this document's
    floor table and `runtime/npkrt.spec`'s `(shared …)` section carry per
    word: every cross-thread datum in a modelled protocol travels through a
    `seq_cst` operation or a `release`/`acquire` pair whose payload is read
    only after the acquire, and x86-TSO plus LLVM's ordering rules make an SC
    interleaving of those operations the only order a thread can observe. The
    classification itself is checked (an access whose word it does not cover
    fails the run); its TRUTH -- that a word marked `owner-only` is touched by
    one thread, that a `born-before-publish` word is written before its
    publish -- is the reader's, stated per word with the edge that orders it.
13. That a MODEL is what was checked, and that LIVENESS was not (1.5.6 step
    5; narrowed at 1.5.6b step 4d, D-295). This acceptance used to ask for
    more: each solver row holds to its own depth and preemption bound
    (`bad:<predicate>:K<k>:D<d>`), so "a defect that needs more steps than
    `K`, or more thread switches than `D`, is outside what was proven" --
    and measured, three of the seven models' depths are smaller than their
    state spaces' diameters. It no longer has to be accepted for SAFETY: every
    model is read a second way, by explicit-state search over its WHOLE
    reachable space with no bound and no solver, a belt in both runners, and
    a bad state reachable anywhere is a red run (the generated paragraph of
    §4c prints each model's reachable states and how many its bounds reach).
    The two readings are independent -- different algorithms, and the first
    two readers a model's MEANING has had -- and the run is green only when
    both hold. What a reader still accepts: that the MODEL says what the
    floor's blocks do (the correspondence belt proves every atomic operation
    and syscall is NAMED by some step, not that a step means what its block
    does -- 1.5.6b step 2 found a deviation, a missing step and a gap by
    reading one model against its code; 1.5.7's explorer drives the real
    blocks -- *[2026-09-18, 1.5.7: it does. Every atomic step and syscall of
    the floor is a scheduling point of every explored schedule, and the
    nineteen model controls were walked against the floor itself, eleven
    becoming explorer controls. It also found where a model said too little:
    `trap-route` had no error code, so DEF-57 -- a failsafe running with the
    wrong error -- satisfied every predicate it had. The model now carries the
    codes, and the correspondence of a step's MEANING to its block is still
    not proven, only exercised]*); that the controls, each of which removes a guarding step and
    must reach its bad state within the bounds, show the predicates are not
    vacuous; and that LIVENESS is not claimed at all -- that a due task is
    eventually run, and that the shared arena's walker stops spinning, need
    a fairness assumption neither reading can state.
14. That two calls are OPAQUE and everything past them is residue:
    `npk_wildx_call`'s indirect call into a sealed executable page (D-035 --
    the analysis has proven the page sealed before it runs, and its contents
    are outside verification by construction) and `npk_failsafe`, the
    program's own handler, which the floor calls and cannot know. Every
    translated path that reaches either carries a `(residue "…")` sentence
    saying so.
15. That the deadlines the trap route waits under are long enough in
    practice, and that a thread which does not stop within one is proceeded
    past (1.5.6 step 1): the stop walk waits for the signalled threads under
    the executor's join deadline (5 s by default), then continues to the
    drivers and `failsafe` regardless. A thread the kernel does not
    interrupt -- one blocked in an uninterruptible wait -- is therefore
    running, in principle, while `failsafe` runs. The alternative is waiting
    forever, which is the failure this whole route exists to prevent.
16. That the floor's CALLERS keep what its sections assume of them (1.5.6c;
    leads E-1 and E-2). A section's `requires`, `(objects …)` and `(views …)`
    — that a range lies in the address space, that it is LIVE memory (which
    is what makes the allocator's fresh block apart from it), that two ranges
    do not coincide — are hypotheses of its rows, and §4d lists, per section,
    the callers for which a row proves them and the callers for which nothing
    does: the floor's untranslated symbols, and emitted code. For the second
    kind the evidence is an ARGUMENT, written in `runtime/npkrt.spec` beside
    the clause — a frame is in at most one of a run queue, a sleeper list and
    running; a task is linked on at most one waiter list, once; a chunk is on
    at most one class list and the lists are linear; a frame's header is never
    a slot; an arena value is never an arena element; the kernel maps nothing
    over a live mapping unless asked — and the emitter's call discipline
    (D-201's table is the program encoder's business). Two such hypotheses
    were FALSE for a legal caller until 1.5.6c and no solver could have said
    so: `npk_string_concat` assumed its two inputs apart (`string_concat(s, s)`)
    and `npk_small_free` assumed a chunk apart from the head of the list it
    was on. They were found by reading. What would TEST the rest rather than
    argue it is 1.5.7's explorer carrying these invariants as executable
    assertions in its floor; until it does, this item is what a reader accepts.
    *[2026-09-18, 1.5.7 step 5 (D-302): it does, for 236 of the 240: each
    section's hypotheses are an entry checker the explored floor calls at
    every call of every explored schedule, and a false one is the verdict
    `ASSUMPTION` (the spec control `unconditional-apartness` plants 1.5.6's
    false clause back and is found on `drop_string`'s 27th step). What a
    reader still accepts: the four hypotheses the checkers LIST by name (§4d;
    each names a free symbol), and every caller in a schedule or a program the
    explorer does not run (item 17).]*

17. That what the schedule explorer ran is what was tested, and nothing
    more (1.5.7; VERIFICATION_REFERENCE §10). WEAK MEMORY is not explored:
    the baton serializes, so every atomic step runs as sequentially
    consistent, and an ordering bug in a release/acquire pair is invisible
    to it (D-290's shared-state belt and the models are what speak to that;
    `// stress:` runs weak memory on real cores and catches what it catches).
    Programs with REAL CHILD PROCESSES are not explored (a virtual clock
    cannot share a child's real time): each says `// explore: no <reason>`,
    a belt holds every `// stress:` program to a marker, and the stage prints
    the list with the reasons on every run -- ten at 1.5.7's close. The clone
    trampoline and the asm bottom run, but are not points. SCHEDULES BEYOND
    THE SEEDS are not claimed: PCT's guarantee is a probability per run,
    1/(n·k^(d−1)) for a depth-d bug, printed per unit, never a proof. And
    LIVENESS is not claimed here either.

18. That the floor's own frames fit in the reserve below every limit word, and
    what that argument leaves out (D-305, 1.5.8 step 2). Every function the
    compiler emits compares its frame against the thread's limit word before
    allocating it, so an emitted overflow is `StackExhausted` through the trap
    route; the floor's functions do not check, and the floor object tells the
    linker it is split-stack-aware (`.note.GNU-split-stack`, beside
    `.note.GNU-no-split-stack`) -- a claim the `floor-stack-reserve` belt holds in
    both runners: the floor's deepest chain of frames, as the pinned `llc` lays
    them out, with one more pass of the trap route and the emitted leaf's slack,
    within a quarter of the 64 KiB reserve (1,712 bytes at 1.5.8 step 2). What a
    reader accepts: that `llc`'s `-stack-size-section` reports the frames it
    emits (item 1); that no signal frame lands in the reserve, because every
    signal the floor handles runs on the thread's signal stack (SA_ONSTACK) and
    `sigaltstack` itself refuses a stack below the machine's minimum; that the
    JIT code `npk_wildx_call` enters checks nothing -- `wildx` is the author's
    opt-out, and a JIT frame larger than the reserve and the guard page below it
    is outside this argument; and that the assembly the switches are written in
    (`npk_switch_stack`, `npk_fs_switch_call`, `__morestack`) does what its
    comments say -- it runs, and is no point of the explorer (item 17).
19. That the kernel delivers a machine fault as it promises, and what still
    stops a program with no `failsafe` (D-307, 1.5.8 step 3). SIGSEGV, SIGBUS,
    SIGILL and SIGFPE -- a fault the language could not prevent: JIT code
    (`wildx`), a floor defect, the hardware -- enter the trap route as
    `MachineFault` through `npk_fault_handler`, on the faulting thread's signal
    stack, and a fault inside the route (a `failsafe` that faults) meets the
    holder's re-entry exit 70 because the actions say SA_NODEFER (measured: a
    fault inside a fault's `failsafe` exits 70, and 132 -- the kernel's kill --
    on a floor whose actions lack the flag). What a reader accepts: that the
    kernel delivers a synchronous fault to the thread that raised it, on its
    registered signal stack, unblocked because the action says NODEFER; and
    that what follows stays UNCONTROLLED, by name. A fault before
    `npk_fault_arm` has run (the first instructions of `npk_start`: its TLS
    boot, the standard descriptors' probe (DEF-69, step 3c), the two stack
    mappings, the signal stack, SIGUSR1's action). A fault
    whose own delivery fails: a signal stack that is itself unusable, which the
    kernel answers by killing the process. SIGKILL and SIGSTOP, which no
    process can catch, and the kernel's out-of-memory killer. Every other
    signal another process can send (SIGTERM, SIGINT, SIGHUP and the rest keep
    their default actions, most of which end the process) -- except SIGPIPE,
    which the floor catches with a handler that returns, so a write to a pipe
    with no reader answers EPIPE as a value where it killed the process
    (DEF-68, found writing this item: a program piped into `head` died with
    exit 141). A hardware
    error the kernel does not turn into one of the four signals. And the
    kernel's own faults. The explorer's shim is not reentrant, so a fault
    inside the shim's own code is reported as the shim's defect (`SHIM FAULT`,
    exit 97; K-13), never explored as a trap.

Nothing else is trusted. In particular nothing in `src/`, `lib/` or the prelude
is exempt from the checks that bind a user program (D-205's switch put the
compiler's own source under them), and no C, C++, Rust, Python or third-party
code is in any artifact (the zero-dependency rule, enforced by the closed-world
link and the undefined-symbol scan on every object).
