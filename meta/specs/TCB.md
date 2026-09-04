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
| z3 4.16.0, the workbench build of tag `z3-4.16.0` | `[verify]` `z3-sha256` (the binary's hash), `z3-version`, `z3-options` (D-218.1/D-218.2) | an EVIDENCE tool: a solver defect is a wrong verdict, and a wrong `discharged` elides a guard. Mitigations: the pin (one build, one hash), the profile (a verdict is a function of the obligation, the build and the budget), the committed manifest (a verdict that moves is a red run), `--explain`'s unsat cores on request; proof certificates are D-040's opt-in for certification runs |
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

The disposition column is 1.5.6's to fill per symbol; the class defaults are
written in the meantime so no row is silent.

<!-- BEGIN floor-table -->
| symbol | class | disposition |
|---|---|---|
| `@npk_clone_exec` | asm | the volatile bottom (inline asm): TRUSTED, documented; no proof |
| `@npk_exec` | asm | the volatile bottom (inline asm): TRUSTED, documented; no proof |
| `@npk_sys6` | asm | the volatile bottom (inline asm): TRUSTED, documented; no proof |
| `@npk_ch_at` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_get` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_open` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_ch_wake_one` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_driver_kill_all` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_driver_live_count` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_driver_retire` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_io_register` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_mx_lock` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_mx_unlock` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_park_sleep` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_bump` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_destroy` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_make` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sarena_slot` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_earliest` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_push` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_sl_wake_due` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_step` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_task_done` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_windup_all` | atomic | a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor) |
| `@npk_aalloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_alloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_alloc_impl` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_alloc_internal` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_alloc_managed` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_arena_alloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_arena_destroy` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_arena_make` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_barrier_arrive` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_barrier_cancel` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_barrier_poll` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_buffer_new` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_calloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_close` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_closed` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_lock` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_reclaim` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_recv_wait` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_send_wait` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_try_recv` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_try_send` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_unlock` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_wait_link` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_wait_unlink` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ch_wake_all` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chain_depth` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chain_push` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chain_reset` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chain_site` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chtab_insert` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chunk_guard_check` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_chunk_new` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_close` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_cstr_slice` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_cv_begin` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_cv_broadcast` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_cv_done` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_cv_signal` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_dalloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_exit` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_frame_alloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_frame_exec_destroy` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_frame_exec_new` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_frame_free` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_guard_release` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_hardware_concurrency` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_heap_bad` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_heap_badreq` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_heap_init` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_heap_oom` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_hmap` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_hs_report` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_hunmap` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_int_to_string` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_io_unwatch` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_join_deadline` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_large_check` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_large_new` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_lg_insert` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_mono_now` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_mutex_acquire_wait` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ofd_close` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_open` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_park_take` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_park_until` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_path_exists` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_ralloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_read` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_read_file` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_read_stdin` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_rq_pop` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_rq_push` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_run_until` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_rw_read_wait` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_rw_release_read` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_rw_write_wait` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_small_alloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_small_check` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_small_free` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_start` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_string_concat` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_string_slice` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_thread_entry` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_thread_exit` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_thread_join` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_thread_start` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_tls_boot` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_to_cstring` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_trap` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_wild_release_all` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_wildx_alloc` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_wildx_check` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_wildx_free` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_wildx_seal` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_windup_grace` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_windup_note` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_write` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@npk_write_file` | syscall | specified at the syscall boundary (1.5.6); the kernel is trusted |
| `@__divti3` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@__modti3` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@__udivti3` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@__umodti3` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@fmod` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@fmodf` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@memcpy` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@memmove` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@memset` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_arena_at` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_arena_free` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_arena_reset` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_ch_push` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_ch_unlink` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_chtab_find` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_environ` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_frame_bucket` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_frame_drain` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_frozen_get` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_arm` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_note_alloc` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_note_free` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_note_resize` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_put_dec` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_hs_put_str` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_lg_entry` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_lg_find` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_lg_remove` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_chunk` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_ffree` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_flive` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_freed` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_guard` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_large` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_largew` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_live` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_livew` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_m_wildx` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_string_equals` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_string_from_bytes` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_udivmod128` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_wild_live_count` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_wildx_call` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
| `@npk_zero` | pure | pure IR: Z3-specified at 1.5.6 where feasible |
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

Nothing else is trusted. In particular nothing in `src/`, `lib/` or the prelude
is exempt from the checks that bind a user program (D-205's switch put the
compiler's own source under them), and no C, C++, Rust, Python or third-party
code is in any artifact (the zero-dependency rule, enforced by the closed-world
link and the undefined-symbol scan on every object).
