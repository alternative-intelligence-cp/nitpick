# Review of driver_architecture_plan_v2.md — Findings

Fourth-pass review (Gemini Flash → Gemini Pro → Opus 4.6 → this), checked against
the nitpick-native repo at cycle **0.8.4**: all 140 settled decisions in
`DECISIONS.md`, the reference spec set, the **real parser source**, and the current
backend. Kernel-level claims were validated by running code on the deployment
kernel ([poc/](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/), 18/18 pass).

**Verdict:** v2's architecture is sound and is in fact *mandated* by a settled
decision it never cites (D-055). But it contained **three safety holes that defeat
the architecture's stated purpose**, one violated D-055 requirement, roughly a
dozen spec/syntax errors that would fail compilation or review, and a
false-sharing layout on the hot path. All fixed in
[driver_architecture_plan_v3.md](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/driver_architecture_plan_v3.md).

---

## A. Safety-critical (each defeats "the driver cannot crash/compromise the runtime")

### A1. Unsealed shared memory lets the driver SIGBUS the Nitpick process — CRITICAL
v2 creates the memfd, maps it, and hands the fd to the driver with **no seals**.
The driver (buggy or compromised) calls `ftruncate(shm_fd, 0)`; the next Nitpick
access to the mapping raises **SIGBUS** — an unroutable fault inside the verified
process, arriving through the data plane. This is the exact class of event the
entire architecture exists to prevent, and it was reachable by one syscall from
the untrusted side.
**Fix:** `MFD_ALLOW_SEALING` at creation; `F_ADD_SEALS(F_SEAL_SHRINK | F_SEAL_GROW |
F_SEAL_SEAL)` before the fd leaves the process (v3 §4.3). *Both* the attack
(SIGBUS without seals) and the fix (EPERM with them) are demonstrated in poc test 1.

### A2. SIGPIPE on the control plane kills the runtime after any driver crash — CRITICAL
v2 writes control packets with plain `write()`/`sys(SYS_WRITE)`-shaped calls. When
the driver has died — the *expected* failure this design handles — the next write
to the socketpair raises **SIGPIPE, whose default disposition terminates the
process**. The crash-containment path itself crashed the runtime.
**Fix:** every control-plane send goes through `send`/`sendmsg` with
**`MSG_NOSIGNAL`**, turning the dead peer into `EPIPE` — a value (v3 §4.2).
Demonstrated in poc test 4 (plain write ⇒ fatal SIGPIPE; MSG_NOSIGNAL ⇒ EPIPE).

### A3. Shared memory and control packets were treated as trusted — CRITICAL
v2 reads `tail` (driver-written), packet `length` fields, and (implicitly) result
descriptors with no validation. A hostile `length = 0xFFFFFFFF` walks the Bridge
into a 4GB read; a hostile offset/len in a completion record becomes an
out-of-bounds Nitpick-side pointer — memory unsafety in the verified process,
authored by the unverified one.
**Fix:** the **[UNTRUSTED]** discipline (v3 §1, §6.2, §7.3): every driver-written
value is validated (range, seq, overflow-safe bounds) before use; any violation
kills the driver and poisons the Bridge. This plus A1's seal is what actually
closes the memory-safety boundary end-to-end.

### A4. `SIGKILL`-by-pid in the failsafe registry can kill an innocent process — HIGH
v2's `driver_registry_kill_all()` sends `SYS_KILL` to a stored **pid**. If the
driver already died and the pid was recycled, `failsafe` SIGKILLs an unrelated
process — at the worst possible moment, during emergency shutdown.
**Fix:** spawn with `CLONE_PIDFD`; the registry stores the **pidfd**;
`pidfd_send_signal` returns `ESRCH` after reap and can never hit a recycled pid
(v3 §5.4, §8; poc test 2). The pidfd also gives *pollable* death notification,
which v2 had no mechanism for at all — dispatch can watch the reply and the
child's life as two fds under one readiness API.

### A5. Nothing stops an orphaned driver when the runtime is SIGKILLed — HIGH
v2's coverage: `defer` (normal exit) + registry (trap). If the Nitpick process is
SIGKILLed or `failsafe` itself dies, **no in-process mechanism runs** and a GPU/
actuator server keeps running headless.
**Fix:** `PR_SET_PDEATHSIG(SIGKILL)` in the child (kernel kills the driver when
the runtime dies — poc test 3) plus a protocol obligation that the driver **must
exit on control-socket EOF** (covers the PDEATHSIG thread-exit residue). v3 §9
gives the full orphan matrix; every row now has a mechanism.

### A6. D-055's "no partial results" requirement was unaddressed — HIGH
D-055 (which v2 never cites): *"a half-finished GPU computation is exactly that
drift, arriving through the door the safety architecture was built to close.
Dispatch is all-or-nothing, and a retry is only permitted where the operation is
idempotent."* v2 had no sequence numbers, no publication rule, no poisoning — a
timed-out dispatch's stale result could be read by the next one.
**Fix:** seq-stamped dispatches, `WORK_COMPLETE` as the only publication point,
fault ⇒ Bridge poisoned ⇒ region discarded at restart, idempotency-gated retry as
caller policy (v3 §4.4, §6.3).

### A7. Unbounded waits in teardown — MEDIUM
v2's `defer` did `await send_shutdown(...)` then **flagless `waitpid`** — an
unbounded kernel block inside cleanup (violates the D-056 posture; a hung driver
wedges scope exit, exactly what D-062 says shutdown must never do). Separately,
whether `await` is legal inside `defer` is unspecified.
**Fix:** explicit `bridge_close(deadline)` on the normal path (graceful, bounded,
escalation ladder SHUTDOWN→SIGTERM→SIGKILL, all via pidfd); the `defer` backstop
is synchronous and bounded by construction (SIGKILL first, then reap) (v3 §4.5).

### A8. stderr pipe could deadlock the driver — LOW
v2 chose a pipe (correct) but nothing drained it during operation; a chatty
driver fills 64KB of pipe buffer, its writes block, and the "hang" is
self-inflicted. **Fix:** continuous async drain into a bounded drop-oldest ring
(v3 §10). Also fixed: child fd hygiene (stdin ← /dev/null, minimal constructed
env, CLOEXEC-at-birth for every runtime fd so nothing leaks through exec).

---

## B. Spec-conformance errors (would not compile, or violate settled decisions)

| # | v2 wrote | Authority says |
|---|---|---|
| B1 | `= Result<Bridge>(...)` etc. on **every** signature | **D-091:** `Result<T>` in return position is a compile error; declare the success type — `= Bridge(...)` |
| B2 | `async pub func:` | Real parser (`parse_decl.npk:651`): `pub` is consumed **before** the modifier loop — only `pub async func:` parses |
| B3 | `defer { … };` | CONTROL_REFERENCE / CLAUDE.md: control blocks take **no trailing semicolon**; `defer { … };` is a parse error |
| B4 | `discard await send_shutdown(…)`, `discard sys(…)` | Discarding a `Result` is **`drop(expr)`**; `discard(x)` marks unused bindings, and both require parens. Also note `drop` on an un-awaited async call **spawns a task** (CONCURRENCY_REFERENCE §2.2) — the parenthesized awaited form is not just style |
| B5 | `#wild_ptr<int8>(…)` | D-019/D-020/MEMORY_REFERENCE §1.5: the type argument is the **pointer type** — `#wild_ptr<int8->>(…)` yields `wild int8->`; `<int8>` yields a non-pointer |
| B6 | `b.child_pid =>! int64` | Kernel-id → integer is the **free** direction: `=> int64` (D-042; 0.8.0 recorded the precedent — `=>!` is for *manufacturing* ids like `3i32 =>! fd`) |
| B7 | `atomic<int64>` fields in `Bridge` initialized from `atomic_from_ptr` | A field of type `atomic<T>` declares **inline storage**; whether an *aliased* atomic is storable at all is unspecified (and if it is borrow-like, D-004 forbids storing it). v3 stores only the `wild` base pointer and constructs views per-use |
| B8 | `INIT_REQ`/`INIT_ACK` share opcode `0x0001` | The repo's most-recurred defect class — "a slot that means two things" (seven catalogued instances; house fix is to split). v3: responses set the high bit |
| B9 | `string: driver_path` | **D-051/D-054:** paths are `Path`, never `string` |
| B10 | §7.6: a forgotten `defer` is caught by the D-062 exit-time wild-leak trap | Unsupportable: the shm pointer is *fabricated* via `#wild_ptr` from `mmap`; nothing specifies fabricated pointers enter `<wild-live>`. The registry is the real tracking mechanism; claim withdrawn (v3 §4.5, spec question raised) |
| B11 | `raw await spawn_driver(…)` modeled in the caller example | `relay` is the propagation idiom; `raw` is the audited bypass. In the safety-posture document the example should model the safe form |
| B12 | Missing citation | The whole architecture is settled **D-055** policy; v2 argued from first principles and missed its four binding requirements (one of which it violated — A6) |

## C. Compiler-reality corrections (what exists today, and when this can build)

- **`sys` does not lower until 0.9** — it is variadic, and variadics are 0.9
  machinery (stated in `ir_expr.npk:748`). Atomics lowering is scheduled on **no
  rung by name**; async/executors are **1.1**. Earliest implementable rung for the
  Bridge: **post-1.1**. v2 implied none of this. (v3 §2 stages it, and lists what
  is buildable now: protocol, C driver, conformance tests.)
- **`Duration` is undefined in the entire spec set** — every deadline API uses it;
  no type, no clock, no timer story. Raised as spec question #1.
- **`SYS_WAITPID` does not exist** on x86_64/aarch64 (libc wrapper; primitives are
  `wait4`/`waitid`) and **`SYS_FORK` does not exist on aarch64**. v3 uses
  `clone(SIGCHLD|CLONE_PIDFD)` + `waitid(P_PIDFD)` exclusively.
- **`#ptr_add<T>` is not in BUILTIN_REFERENCE's generated builtin markers** — as
  specified today the resolver would not admit the name; element-vs-byte offset
  semantics also unsettled.
- v2 §2.1's framing "the spec only shows `atomic<i32>`" was half-right: D-033 and
  CONCURRENCY_REFERENCE §4.1 already show `atomic<int64>` as legal — it is a
  **lowering** gap, not a spec gap, and correctly belongs in 1.1's scope.
- Child-path constraint v2 missed: between clone and execve the child may hold a
  stale allocator lock from another thread — the child path must be
  **allocation-free**, with argv/envp `cstring`s prepared before the clone.

## D. Performance findings (measured on the deployment kernel — poc test 5)

- **False sharing:** v2 put `head` (Bridge-written) and `tail` (driver-written) 8
  bytes apart — one cache line ping-ponging between processes on every ring op.
  v3 gives each contended field its own line (256-byte header).
- **Dispatch floor measured: ~7.5 µs socketpair RTT** — one CUDA-launch magnitude,
  proportionate for D-055's workload, not for fine-grained calls. The amortization
  is **batching** (N descriptors per notify), which the v3 descriptor-ring layout
  makes a payload change, not a protocol change.
- **Futex-in-shm measured at ~6.5 µs parked** — *not* meaningfully cheaper than the
  socket when the driver sleeps; its real win is **doorbell suppression** (zero
  syscalls while the driver is busy draining). Reserved header line for it; ship
  the socket first. This corrects the folk assumption that futex doorbells are an
  order-of-magnitude win — measure before committing complexity.
- **Ring/payload conflation:** v2 streamed bulk payloads through the ring (forcing
  wrap-splitting of tensors and >4GB ring indices, plus general modulo on a
  non-power-of-two capacity). v3 splits a small power-of-two **descriptor ring**
  (mask, not modulo) from a flat **bulk region** addressed by validated
  `{offset,len}`.
- **SeqCst-vs-acquire/release** (v2's analysis): confirmed correct and now
  quantified — a store fence against a 7.5 µs floor; D-016 costs nothing here.
- **"Zero-copy" claim tightened:** true for CPU serialization; a GPU H2D copy
  remains. v3 stages: `cudaHostRegister` of the mapping at INIT (pinned DMA, v1)
  → reverse fd-passing of a dma-buf/CUDA-IPC allocation (deletes the copy, gated
  by `abi_version`).
- **Result ABI:** all Bridge signatures return ≤12-byte success types — register
  path, clear of D-084's 13–16-byte sret cliff.
- **Spawn cost:** clone's page-table copy on a Nikola-sized address space is
  milliseconds — per driver *lifetime*, acceptable; `clone3(CLONE_VM|CLONE_VFORK)`
  noted as the measured-later optimization, not the starting point.

## E. What v2 got right (kept, some with sharper justification)

- Out-of-process isolation with socketpair control + memfd data plane — and it
  matches settled policy (D-055) exactly.
- **No `failsafe` from the Bridge; every failure is a `Result`** the caller routes.
- D-042 typing throughout (`pid`/`fd` fields, not `int32`) — v2's §3.1 rationale
  was correct and survives verbatim.
- The **driver registry** as a preallocated, failsafe-reachable structure, with
  kill-don't-cleanup semantics on the trap path — the right two-layer shape
  (structural `defer`/close + containment registry). v3 only hardens the kill
  mechanism (pidfd) and the entry lifecycle (registered-before-usable,
  removed-last).
- SeqCst(Nitpick) ↔ acquire/release(C) pairing analysis — correct.
- `atomic_from_ptr` assumed pointer-taking — confirmed by D-033 (v2's §2.2 open
  question is now closed).
- Restart = full recreation, never region reuse.
- stderr via pipe over tempfile.
- One shm region for v1, multi-region as a versioned extension.

## F. The C reference driver (in the original conversation) needs one real fix

Its `volatile ShmHeader *` access to `head`/`tail` is not synchronization — mixed
with the Bridge's true atomics it is a C11 data race (UB), the intermittent-
corruption class D-016 exists to exclude. The reference must use
`atomic_load_explicit/atomic_store_explicit` with acquire/release (v3 §7.5.4).
Its exit-on-EOF behavior, already present, is promoted from accident to protocol
obligation. Add: `fstat` the received fd; validate magic/version before ACK.

---

### Disposition

- **v3 plan:** [driver_architecture_plan_v3.md](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/driver_architecture_plan_v3.md) — supersedes v2.
- **Kernel validation:** [poc/kernel_mechanisms_test.c](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/kernel_mechanisms_test.c) — 18/18 on 7.0.0-28-generic.
- **Eight spec/compiler questions** raised to the nitpick-native effort: v3 §15.
  None block protocol work; all should be settled before the Bridge lands (≥1.1).
- **Nothing in `/home/randy/Workspace/REPOS/nitpick-native` was modified.**
