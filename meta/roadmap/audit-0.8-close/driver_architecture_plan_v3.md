# Nitpick External Driver Architecture — v3

Verified against the [nitpick-native](file:///home/randy/Workspace/REPOS/nitpick-native) build at cycle **0.8.4** (nlibc floor exists; `sys` lowers at 0.9; async at 1.1). Every syntax form below was checked against the *real parser* (`src/frontend/parse_decl.npk`) and the settled decisions in [DECISIONS.md](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md), not just the reference docs.

> [!NOTE]
> This replaces `driver_architecture_plan_v2.md`. The v2→v3 delta — including three
> safety holes that defeated the architecture's whole purpose — is catalogued in
> [review_v2_findings.md](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/review_v2_findings.md).
> The kernel mechanisms this plan depends on were validated on the deployment
> kernel (7.0.0): see [poc/](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/) — **18/18 checks pass**.

---

> [!IMPORTANT]
> **Scope widened by D-149 (post-0.9):** the architecture below now governs
> **ALL foreign code**, not only GPU/GUI — in-process FFI does not exist in
> Nitpick. `extern` blocks are the driver-interface declaration; stub
> generation is compiler lowering of `DeclExternBlock` (not a macro library);
> D-002's per-function `fails on` contracts are dead, replaced by the wire's
> uniform status. Everything this plan says about the trust model, the two
> channels, deadlines, and supervision applies to every driver.

---

## 0. This is the implementation of D-055

v2 derived the architecture from first principles. It did not need to: **[D-055](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L3479) already settled it.** "Anything requiring GPU access — CUDA included, which Nikola requires — and any complex GUI runs as a separate server process." This plan is the concrete design for D-055's *supervised child server*, so it inherits D-055's four boundary requirements as obligations:

| D-055 requirement | Where this plan delivers it |
|---|---|
| 1. Every dispatch carries a deadline | §4.4, §5.4 |
| 2. **No partial results** — dispatch is all-or-nothing; retry only where idempotent | §6.3 *(new in v3 — v2 had nothing)* |
| 3. The server is a supervised child | §4, §8, §9 |
| 4. GUI failure must never stop control logic — GUI server ≠ GPU server | one Bridge per server; Bridges are independent (§4.1) |

D-055 also narrows what "driver" means for GPU work: kernels are written in **Nitpick**, compiled by npkc through LLVM's NVPTX/AMDGPU backends (`#[gpu_kernel]` is a codegen target, never a callable). The driver process is a *loader and dispatcher* for Nitpick-compiled kernel images — not a place where compute logic lives. §11 aligns the wire protocol with that.

### Why out-of-process (unchanged from v2, confirmed)

Past the FFI barrier the runtime cannot intercept a fault and route it through `failsafe` ([CLAUDE.md L176-178](file:///home/randy/Workspace/REPOS/nitpick-native/CLAUDE.md#L176)). Process isolation turns a vendor-blob segfault into **a value**: a closed socket, a pollable pidfd, an errored `Result<T>`. The driver is explicitly outside the TCB and needs no verification, because its failure is contained and observable.

### The two channels

- **Control plane:** `AF_UNIX` stream `socketpair` (`SOCK_CLOEXEC`). Framing, lifecycle, completion signalling.
- **Data plane:** anonymous shared memory (`memfd_create`, **sealed**, §6), mapped by both sides. Descriptor ring + bulk payload region.

```
+-------------------------------------------------------------------------+
| NITPICK RUNTIME PROCESS (verified TCB)                                  |
|   [User Code] <--> [Bridge] <--> [Driver Registry (failsafe-reachable)] |
+---------|-----------------|---------------------|-----------------------+
          | ctrl socketpair | stderr pipe         | sealed memfd (shm)
+---------|-----------------|---------------------|-----------------------+
| DRIVER PROCESS (outside TCB, disposable)        |                       |
|   [Event Loop] <-> [Descriptor Ring + Bulk Region] <-> [CUDA/Vulkan/GTK]|
+-------------------------------------------------------------------------+
```

---

## 1. The trust model — one sentence that governs every section

> **After `execve`, nothing the driver writes is trusted: every byte read from the
> shared memory and every field of every control packet is untrusted input,
> validated before use; any violation kills the driver and fails the Bridge.**

v2 treated the shared memory as memory. It is an I/O device. The concrete rules this produces are marked **[UNTRUSTED]** throughout.

---

## 2. Where this sits on the roadmap

The Bridge is `nlibc`-tier Nitpick. It cannot be implemented at the current rung, and the plan must say so precisely rather than imply it is next week's work:

| Prerequisite | Rung | Status |
|---|---|---|
| `sys` builtin lowering (variadic) | **0.9** | blocked on variadics; named in `src/backend/ir/ir_expr.npk:748` |
| structs / enums / slices / `Result` full lowering | **0.9** | scheduled |
| `atomic<T>` lowering (`load`/`store`/`compare_exchange` for `int64`, `int32`, `bool`) | **not explicitly scheduled** | 0.9 names types only; atomics belong naturally to **1.1** (concurrency). *Raise this* — the driver needs only the six-method SeqCst set, no new spec work (§3.1) |
| `async`/`await`, executors, task suspension (D-071) | **1.1** | scheduled |
| `Duration` + monotonic clock + executor timers | **nowhere** | *spec gap* — every deadline API in IO_REFERENCE/CONCURRENCY_REFERENCE uses `Duration`, and no document defines it (§3.4) |
| driver registry hook in `failsafe` | runtime work | pairs with the stream registry, itself open ([IO_REFERENCE §10](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/IO_REFERENCE.md#L240)) — build **one** registry mechanism, three clients (§8) |

**What can be built now, before any of that lands:** the C reference driver, the wire-protocol conformance suite, and a C mock-bridge that exercises a real driver — the protocol is language-independent. The [poc/](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/) harness is the seed of that suite.

---

## 3. Compiler / spec prerequisites

### 3.1 `atomic<int64>` — a lowering gap, not a spec gap (v2 corrected)

v2 called this a spec addition. It is not: [D-033](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L1906) and [CONCURRENCY_REFERENCE §4.1](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/CONCURRENCY_REFERENCE.md#L224) already show `atomic<int64>:hits;` as legal. What is missing is **backend lowering for any atomic at all**. The IR is the §13 table widened to `i64` — LLVM-native on x86_64 and aarch64. The ask for cycle 1.1: the six SeqCst methods for `int32`, `int64`, `bool`.

### 3.2 `atomic_from_ptr<T>` — resolved, and a storability question v3 sidesteps

D-033 settles the signature: **it takes a pointer** (`atomic_from_ptr<int32>(hdr_ptr)`), sourced through `#wild_ptr<T>` in `wild` context where the address originates as an integer.

**Open type-system question v2 silently stepped into:** its `Bridge` struct stored `atomic<int64>` *fields* initialized from `atomic_from_ptr` — but an `atomic<T>` field means *inline storage in the struct* (that is what `atomic<int64>:hits;` declares), while `atomic_from_ptr` produces an *alias of foreign storage*. Whether an aliased atomic is a storable value, a borrow (then D-004 forbids storing it), or a construct valid only as a local is **specified nowhere**. v3 does not store aliased atomics: the Bridge holds the `wild` base pointer, and ring functions construct the atomic view locally, in `wild` context, at each use (§6.2). Raise the storability question with the spec effort; nothing here waits on the answer.

### 3.3 `#ptr_add<T>` — settled name, unfinished bookkeeping

D-033 names `#ptr_add<T>(ptr, offset)` as the only pointer-arithmetic form. Two items for the compiler team:

- It is **absent from BUILTIN_REFERENCE's `<!-- builtins:begin -->` marker regions**, which generate the resolver's bare-name builtin set — as specified today the resolver would not admit the name.
- Whether `offset` is in **elements of T or in bytes** is unstated. Every use in this plan is `T = int8`, where the readings agree; the ambiguity must not survive into a plan that offsets an `int64` table.

### 3.4 `Duration`, deadlines, and a monotonic clock — undefined

Every blocking operation carries `Duration:deadline` (D-056) and **no spec defines `Duration`**, its unit, its arithmetic, or the clock that anchors it. The Bridge needs: a `Duration` type; `CLOCK_MONOTONIC` via `sys`; executor timer integration (timerfd or io_uring timeout) so an expired deadline actually resumes the task. This lands naturally with 1.1's executor and should be in that cycle's plan. Until settled, this plan treats `Duration` as an opaque relative timeout.

### 3.5 Syscall constants and portability

The constants module (`nlibc`, 0.9) needs: `SOCKETPAIR`, `MEMFD_CREATE`, `FTRUNCATE`, `FCNTL` (+ `F_ADD_SEALS`, `F_SEAL_SHRINK|GROW|SEAL`), `MMAP`/`MUNMAP`, `SENDMSG`/`RECVMSG` (+ `MSG_NOSIGNAL`, `SCM_RIGHTS`), `EXECVE`, `PRCTL` (+ `PR_SET_PDEATHSIG`, `PR_SET_NO_NEW_PRIVS`), `PIDFD_OPEN`, `PIDFD_SEND_SIGNAL`, `WAITID` (+ `P_PIDFD`), `CLONE`/`CLONE3`, `DUP2`/`DUP3`, `CLOCK_GETTIME`.

Two portability corrections to v2:

- **There is no `waitpid` syscall** on x86_64/aarch64 — it is a libc wrapper. The primitives are `wait4` and `waitid`; this plan uses **`waitid(P_PIDFD, …)`** exclusively (§4.5), which also closes the pid-reuse race.
- **There is no `fork` syscall on aarch64** — only `clone`. Spawning goes through the clone family from the start; `CLONE_PIDFD` then delivers the pidfd atomically with the spawn (§4.3).

Per [D-044](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L2537)/[D-043](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L2517), flags are bitflag types and signals are an enum at the typed layer; conversion to raw `int64` happens only inside these `nlibc` helpers, which is exactly where D-044 confines it.

---

## 4. The Bridge — struct and lifecycle

### 4.1 Type definition

```nitpick
struct:Bridge = {
    pid:            child_pid;      // D-042: kernel ids are distinct types
    fd:             child_pidfd;    // pollable process handle (§4.3) — an fd
    fd:             ctrl;           // control socketpair, our end
    fd:             child_stderr;   // read end of the stderr pipe (§10)
    int64:          shm_size;
    wild int8->:    shm;            // sealed memfd mapping; wild => defer-managed
    uint32:         next_seq;       // dispatch sequence counter (§6.3)
    bool:           closed;         // set by bridge_close; read by the defer backstop
};
```

Corrections captured from v2: return types are written as the **success type** — `Result<…>` in return position is a compile error ([D-091](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L6234)); modifier order is **`pub async func:`** (the parser accepts `pub` first, then the modifier loop — `async pub` does not parse); no aliased atomics stored (§3.2).

**Ownership contract (new, load-bearing):** a `Bridge` is **single-owner, move-only** — the same discipline as `arena<T>`. The data plane is an SPSC ring; one producer is a *type-level* assumption, so two tasks dispatching on one Bridge must be unrepresentable, not discouraged. Concurrent use of one driver is expressed the same way D-017 expresses it for arenas: don't share the Bridge — give the owner a channel. One Bridge per server process; the GPU server and the GUI server are **different Bridges** (D-055 req. 4).

**Error codes** are positive user-space `tbb32` constants (negative is the system range, `0` and ERR unconstructible — [D-069](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L4672)):

```nitpick
pub fixed tbb32:E_DRIVER_SPAWN    = 2001tbb32;
pub fixed tbb32:E_DRIVER_PROTOCOL = 2002tbb32;   // any [UNTRUSTED] violation
pub fixed tbb32:E_DRIVER_FAULT    = 2003tbb32;   // died / EOF / pidfd fired
pub fixed tbb32:E_DRIVER_DEADLINE = 2004tbb32;
pub fixed tbb32:E_RING_FULL       = 2005tbb32;
pub fixed tbb32:E_BRIDGE_POISONED = 2006tbb32;   // used after a fault (§4.4)
```

### 4.2 Safety rules

- **The Bridge never traps.** No `!!!`, no `?!`. Every failure is a `Result<T>` the caller routes — restart, degrade, or escalate. (Unchanged from v2; it is the correct posture.)
- **One syscall form**, `sys(...) → Result<int64>` (D-048). The helpers here are `nlibc`-tier — the `--extra-picky=no-sys` boundary puts them exactly where `wild` code belongs.
- **The registry entry outlives every resource it guards** (§8): it is written before the handshake completes and retired as the *last* step of teardown, so a trap at any intermediate point still finds a killable entry.
- **[UNTRUSTED] on the control plane:** opcode must be from the known set, `length` ≤ `MAX_PACKET` (64 KiB), payload lengths must match the opcode. Violation ⇒ kill, `E_DRIVER_PROTOCOL`.
- **Every control-plane send uses `sendmsg`/`send` with `MSG_NOSIGNAL`.** A plain `write()` to a socketpair whose peer died raises **SIGPIPE, which terminates the Nitpick process by default** — the exact event this architecture exists to prevent, reachable in v2 on every post-crash send. Empirically confirmed in [poc](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/kernel_mechanisms_test.c) test 4.

### 4.3 Spawn

```nitpick
pub async func:spawn_driver = Bridge(
    Path:       driver_path,        // D-051/D-054: paths are Path, never string
    int64:      shm_size,
    Duration:   deadline
) { … };
```

The sequence, with every hardening step stated (each *(sealed)*, *(pidfd)*, *(pdeathsig)* item is validated in the poc):

1. **Control pair:** `socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)`. Every fd the runtime creates is CLOEXEC **at birth** — `SOCK_CLOEXEC`, `MFD_CLOEXEC`, `O_CLOEXEC`, `PIDFD_OPEN` (CLOEXEC by default). "Close unused fds in the child" (v2) is unenumerable; birth-CLOEXEC makes leak-freedom structural: the driver inherits exactly what step 5 deliberately installs.
2. **Shared memory:** `memfd_create(name, MFD_CLOEXEC | MFD_ALLOW_SEALING)`; `ftruncate(shm_size)`; `mmap(PROT_READ|PROT_WRITE, MAP_SHARED)`; then **seal: `fcntl(F_ADD_SEALS, F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL)`**.
   **This seal is the single most important line in the plan.** Without it, the driver calls `ftruncate(shm_fd, 0)` and the next Nitpick access to the mapping raises **SIGBUS — the driver crashes the runtime through the data plane**, defeating the architecture outright. With the seal, the hostile shrink returns `EPERM` and the mapping is stable for its lifetime. Both directions demonstrated in poc test 1. Writes stay possible for both sides (`F_SEAL_WRITE` is *not* set); `F_SEAL_SEAL` stops the driver adding seals of its own.
3. **Header init:** magic, capacity, zeroed indices, version (§6.1) — before the child exists, so the driver never observes a half-written header.
4. **stderr pipe** (`O_CLOEXEC`, read end kept): §10. stdin of the child will be `/dev/null`; stdout policy likewise §10.
5. **Spawn:** `clone(SIGCHLD | CLONE_PIDFD)` — the **pidfd** arrives atomically with the child; no `pidfd_open` race window. Child path — which must be **allocation-free** (argv/envp `cstring`s are prepared *before* the clone; another thread may hold the allocator lock at clone time, and the child has only this thread):
   - `prctl(PR_SET_PDEATHSIG, SIGKILL)` — if the runtime dies by any means, *including SIGKILL where `failsafe` never runs*, the kernel reaps the driver (§9, poc test 3). Then check `getppid()` — if the parent died before the prctl landed, exit.
   - `prctl(PR_SET_NO_NEW_PRIVS, 1)` — unconditional; free; a compromised driver cannot escalate through setuid/fscaps, and it is the precondition for any later seccomp filter (§12).
   - `dup2` the child socket end to **fd 3** (dup2 clears CLOEXEC — the one fd meant to survive exec), `/dev/null` onto 0, pipe ends onto 1/2.
   - optional sandbox setup (§12), then `execve(driver_path, argv, minimal_env)` — a **constructed environment**, never the runtime's inherited one (secrets do not leak into an unverified process).
6. **Handshake:** send `INIT_REQ` carrying `{protocol_version, shm_size}` with the shm fd via `SCM_RIGHTS` (§7.4); `await` `INIT_ACK` **under `deadline`**; verify the driver's version. Failure or timeout ⇒ kill via pidfd, reap, `fail(E_DRIVER_SPAWN)`.
7. **Register** in the driver registry (§8) — *before* returning, and in program order before the Bridge is usable.
8. `pass(b);`

**Spawn-cost note:** `clone` without `CLONE_VM` copies page tables — on a Nikola-sized address space that is milliseconds. It is a per-driver-lifetime cost, not per-dispatch, so plain clone ships first; `clone3(CLONE_VM | CLONE_VFORK | CLONE_PIDFD)` (the posix_spawn technique — child on a scratch stack, straight into execve) is the measured-later optimization. Do not start there; the CLONE_VM child path is delicate and buys nothing until spawn frequency is shown to matter.

### 4.4 Dispatch

```nitpick
pub async func:dispatch = int32(
    Bridge->:   b,
    uint32:     kernel_id,
    Duration:   deadline
) {
    // 1. seq = b.next_seq; b.next_seq advances     (§6.3 — no-partial-results)
    // 2. send EXEC_NOTIFY {seq, kernel_id}          MSG_NOSIGNAL (§4.2)
    // 3. await WORK_COMPLETE under `deadline`, concurrently watching:
    //      - ctrl readability (the reply)
    //      - child_pidfd readability (driver died)   [poc test 2]
    //    Both are fds; both integrate with the 1.1 executor's readiness
    //    mechanism (io_uring/epoll) with no special cases.
    // 4. [UNTRUSTED] reply must be WORK_COMPLETE, length == 8,
    //    seq must equal the one sent. Anything else => kill, E_DRIVER_PROTOCOL.
    // 5. deadline expiry => escalation (§5.4) and E_DRIVER_DEADLINE.
    //    pidfd fired => reap, capture stderr tail (§10), E_DRIVER_FAULT.
    // 6. pass(result_code);
};
```

**A Bridge that has faulted is poisoned.** After any `E_DRIVER_*` failure the ring state is unknown; every subsequent call returns `E_BRIDGE_POISONED` until the caller tears down and respawns (§13.2 — fresh memfd, fresh socketpair, *never* a reused region). This is D-055's no-partial-results rule expressed as a state machine rather than a hope.

Dispatch is **serial per Bridge** in v1 (one in flight). The seq field and the completion-record format (§6.3) are deliberately sufficient for pipelined dispatch later without a wire change (§14).

### 4.5 Teardown — an explicit close, with the defer as backstop

v2 put the *graceful* path inside `defer` — `await send_shutdown(...)` then a **flagless `waitpid`**. Two defects: an unbounded kernel wait inside cleanup violates the D-056 posture (no unbounded wait, and a hung driver would wedge scope exit exactly as D-062 says a shutdown must not); and whether `await` is even legal inside a `defer` block is unspecified (§15). v3 splits the layers:

```nitpick
// The normal path — graceful, bounded, async. Call it; then the defer is a no-op.
pub async func:bridge_close = NIL(Bridge->:b, Duration:deadline) {
    // SHUTDOWN over ctrl (MSG_NOSIGNAL; a dead peer is already-closed, fine)
    // await pidfd readable under `deadline`      — driver exiting voluntarily
    // deadline expiry: pidfd_send_signal(SIGKILL) — guaranteed to terminate
    // waitid(P_PIDFD, WEXITED)                    — bounded: the process is dead
    // munmap(b.shm, b.shm_size); close ctrl, stderr, pidfd
    // driver_registry_remove(...)                 — LAST (§4.2 invariant)
    // b.closed = true
    pass(NIL);
};
```

```nitpick
Bridge:b = relay await spawn_driver(path, size, deadline);
defer { drop(bridge_reap(@b)); }        // backstop only: synchronous, bounded
// … use b …
relay await bridge_close(@b, close_deadline);
```

`bridge_reap` is **synchronous and bounded by construction**: if `b.closed`, return; else `pidfd_send_signal(SIGKILL)` → `waitid(P_PIDFD)` (terminates promptly — the process is SIGKILLed) → munmap/close → registry remove. No await, no graceful attempt — it is the "the caller forgot or errored out early" path, mirroring how `failsafe` treats registry entries (§8): kill, don't negotiate.

Syntax notes vs v2, all verified against [CONTROL_REFERENCE](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/CONTROL_REFERENCE.md) and CLAUDE.md: `defer { … }` takes **no trailing semicolon**; discarding a `Result` is **`drop(expr)`**, not `discard` (which marks unused bindings); `relay` — not `raw` — is how a fallible call propagates in code that models the safety posture; kernel-id → integer casts are the **free** direction (`b.child_pid => int64`; `=>!` is only for *manufacturing* ids, e.g. `3i32 =>! fd` in the child — the 0.8.0 precedent).

**One v2 claim withdrawn (§7.6 there):** that a forgotten `defer` is caught by the D-062 exit-time `wild` leak check. The shm pointer is *fabricated* via `#wild_ptr` from an `mmap` result; whether fabricated pointers enter `<wild-live>` at all is unspecified, and the honest position is that **the driver registry, not the leak checker, is the tracking mechanism** for Bridge resources. Raised as a spec question (§15).

---

## 5. Failure detection — how each failure mode becomes a value

| Failure | Detected by | Latency | Result |
|---|---|---|---|
| driver exits / crashes | **pidfd readable** (poll/io_uring) + ctrl EOF | immediate | `E_DRIVER_FAULT` + exit status via `waitid` + stderr tail (§10) |
| driver hangs | dispatch **deadline** (D-055 req. 1: "a hang is worse than a crash") | bounded by caller | `E_DRIVER_DEADLINE`, then §5.4 escalation |
| driver misbehaves | **[UNTRUSTED]** validation, control or data plane | at the violating packet/record | `E_DRIVER_PROTOCOL`, kill |
| runtime traps | driver registry walked by `failsafe` (§8) | trap path | SIGKILL via pidfd |
| runtime SIGKILLed | **PR_SET_PDEATHSIG** in the driver + driver's mandatory exit-on-EOF (§7.5) | kernel-immediate | driver reaped by init |

### 5.4 The kill escalation ladder

Graceful → forceful, every rung bounded: `SHUTDOWN` packet → deadline → `SIGTERM` via pidfd → short deadline → `SIGKILL` via pidfd → `waitid(P_PIDFD)`. SIGKILL cannot be ignored, so the ladder terminates. All signals travel through the **pidfd**: after the child is reaped a pidfd signal returns `ESRCH` — it can never kill an unrelated process that recycled the pid. (v2's registry `SYS_KILL` on a raw pid carried exactly that reuse race; poc test 2 demonstrates the `ESRCH` guarantee.)

**PDEATHSIG caveat, recorded:** the death signal fires when the spawning *thread* exits, not only the process. Spawn drivers from a long-lived executor thread (thread lifetimes are lexical anyway, D-083), and keep exit-on-EOF as the required backstop — it covers this residue completely.

---

## 6. The data plane

### 6.1 Layout — contended fields get their own cache lines

v2 packed `head` (producer-written) and `tail` (consumer-written) 8 bytes apart — **the same cache line ping-ponging between two processes on every operation**. v3 header, 256 bytes:

```
line 0 (ro after init):  u64 magic 0x4E50_4B44_5256_3033   ("NPKDRV03")
                         u32 abi_version | u32 capacity (POWER OF TWO)
                         u64 bulk_offset | u64 bulk_size | flags
line 1: atomic i64 head        (Bridge-written)   + 56B pad
line 2: atomic i64 tail        (driver-written)   + 56B pad
line 3: atomic u32 doorbell / u32 driver_parked   + 56B pad   (reserved, §14)
0x100:  DESCRIPTOR RING   — capacity × 32B records (§6.3)
bulk_offset: BULK REGION  — tensors / vertex data / command payloads
```

Two v2 conflations split apart:

- **The ring holds fixed 32-byte descriptors; bulk data lives in a flat region** referenced by `{offset, len}`. v2 streamed payloads *through* the ring, which forces wrap-splitting of tensors and motivated >4GB ring indexing. A descriptor ring stays small and power-of-two (index arithmetic is a mask, not v2's general modulo); the bulk region alone grows to tensor scale. `int64` indices stay (free-running, never wrapped), and `atomic<int64>` remains the §3.1 ask.
- **Capacity is a power of two, enforced at `spawn_driver`.**

### 6.2 Ring access on the Nitpick side

The Bridge stores only `wild int8->: shm`. Each ring operation constructs its atomic view **inline, as a local** — `wild` context; the view is neither stored in a struct nor returned from a helper, so the §3.2 storability question never arises:

```nitpick
// inside a ring operation; nlibc-tier wild context
// (#ptr_add's type-argument spelling follows §3.3's pending settlement —
//  <int8-> if it mirrors #wild_ptr, <int8> if it is element-scaled; for
//  byte offsets the two agree in effect)
wild int8->:head_p = #ptr_add<int8>(b.shm, 64i64);
atomic<int64>:head = atomic_from_ptr<int64>(head_p);
int64:h = head.load();
```

Producer ordering: write descriptor bytes and bulk bytes **first**, then SeqCst-store the advanced `head`. SeqCst subsumes the release the consumer's acquire pairs with — v2's ordering analysis was correct and is kept, now with numbers: the extra fence costs nanoseconds against a 7.5µs dispatch floor (§14). Consumer-side (`tail`) loads on the Bridge:

**[UNTRUSTED]** `tail` is driver-written. Before use: `tail` must satisfy `head - capacity ≤ tail ≤ head` (free-running comparison). Completion records read from the ring revalidate `seq`, and any `{offset, len}` the driver wrote back must satisfy `bulk_offset ≤ offset ∧ offset + len ≤ bulk_offset + bulk_size` **checked with overflow-safe arithmetic** before any Nitpick read through it. Violation ⇒ kill, `E_DRIVER_PROTOCOL`. A hostile `tail` or a hostile length must never become a Nitpick-side pointer — this is the memory-safety boundary, and it is why the seal (§4.3) plus these checks together restore the "driver cannot crash the runtime" guarantee end to end.

### 6.3 No partial results (D-055 requirement 2)

- Every dispatch carries a fresh `seq` (§4.4). Every descriptor embeds it; every completion record embeds it back.
- **`WORK_COMPLETE` is the only publication point.** The Bridge never reads result data before the matching completion arrives — there is no "peek at the ring mid-flight" API, so a result that was never signalled complete is unreachable, not merely discouraged.
- On `E_DRIVER_DEADLINE` / `E_DRIVER_FAULT`, in-flight seqs are dead: the Bridge is poisoned (§4.4) and the region is discarded at respawn (§13.2). A late completion for a dead seq can never surface — the region it lived in is gone.
- **Retry is the caller's decision and only for idempotent operations** — the dispatch API carries no automatic retry, and the plan's example wrappers document idempotency as a precondition, matching D-055's wording.

---

## 7. The control plane

### 7.1 Packet header (8 bytes, unchanged from v2)

`u16 opcode | u16 flags | u32 length`, host byte order (same-host protocol; the version field exists for everything else).

### 7.2 Opcodes — request/response are distinct codes

| Code | Name | Dir | Payload |
|---|---|---|---|
| `0x0001` | `INIT_REQ` | B→D | `u32 protocol_version, u64 shm_size` + SCM_RIGHTS fd |
| `0x8001` | `INIT_ACK` | D→B | `u32 protocol_version` |
| `0x0002` | `EXEC_NOTIFY` | B→D | `u32 seq, u32 kernel_id` |
| `0x8002` | `WORK_COMPLETE` | D→B | `u32 seq, u32 result_code` |
| `0x00FF` | `SHUTDOWN` | B→D | none |

v2 gave `INIT_REQ` and `INIT_ACK` **the same code, disambiguated by direction** — one slot meaning two things, which is this repo's most-recurred defect class (seven instances catalogued in [ROADMAP.md](file:///home/randy/Workspace/REPOS/nitpick-native/meta/roadmap/ROADMAP.md); "split *whether* from *which*" is the standing fix). Responses set the high bit. `HEARTBEAT` is dropped from v1: pidfd detects death instantly and deadlines detect hangs at the point that matters — a background ping adds a syscall stream and a second liveness definition for no additional coverage. The code point stays reserved.

On the Nitpick side opcodes are an `enum` (`pick` exhaustiveness over inbound codes, with the `(*)` arm being the kill-on-unknown rule); the tag→`u16` cast at the wire is `=>!` per 0.8.0.

### 7.3 [UNTRUSTED] read-side rules

Fixed-size reads for the header; `length` ≤ 64 KiB and exactly what the opcode demands; unknown opcode from the driver ⇒ kill (the Bridge is not obligated to drain garbage the way the tolerant reference driver does). Short reads/EOF mid-packet ⇒ `E_DRIVER_FAULT`.

### 7.4 SCM_RIGHTS helper

As v2 §5.4: `msghdr`/`cmsghdr` built in `wild` memory inside an `nlibc` helper (`send_fd` / `recv_fd`), the only place those structs exist. All sends `MSG_NOSIGNAL` (§4.2).

### 7.5 Driver obligations (protocol-mandated, enforced by conformance tests)

1. **Exit on control-socket EOF/HUP.** The universal orphan backstop (§5, §9).
2. `fstat` the received fd and use `st_size` (cross-check `INIT_REQ.shm_size`; mismatch ⇒ exit).
3. Validate magic, version, capacity-vs-size before `INIT_ACK`.
4. Access `head`/`tail` with **C11 atomics** (`atomic_load_explicit(memory_order_acquire)` / `store … release`) — the v2 reference's `volatile ShmHeader *` is *not* synchronization; volatile loads against the Bridge's true atomics are a C11 data race (UB) and precisely the intermittent-corruption class D-016 exists to exclude.
5. stderr is the diagnostic channel; stdout is not interpreted.

---

## 8. Driver registry — the failsafe path

Concept unchanged from v2 (it was the right design); mechanics hardened:

```nitpick
// Runtime-internal, PREALLOCATED before main() — failsafe cannot allocate (D-014)
struct:DriverRegistryEntry = {
    atomic<int32>:  state;          // 0 free / 1 claiming / 2 active — CAS-claimed
    pid:            child_pid;      // diagnostic only
    fd:             child_pidfd;    // THE kill handle
};
// fixed capacity (16); spawn_driver fails E_DRIVER_SPAWN when full — bounded, never grows
```

- `driver_registry_kill_all()` — called from `failsafe`: walk entries; for `state == 2`, `sys(PIDFD_SEND_SIGNAL, entry.child_pidfd => int64, SIGKILL, 0, 0)`. **No graceful shutdown, no munmap, no reaping** — safing, not cleanup (v2 had this right). `pidfd_send_signal` is allocation-free, mask-independent, and `ESRCH`-safe against reuse.
- Registration precedes usability; removal is teardown's final step (§4.2). A trap at any intermediate moment finds a live, killable entry.
- **Build one registry mechanism.** The allocation registry exists, the stream registry is an open item told to share its structure ([IO_REFERENCE §10](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/IO_REFERENCE.md#L240)), and this is the third client. Anything whose abandonment is unsafe must be reachable from `failsafe` **without traversing a task frame** (D-063) — a global preallocated table is exactly that shape; one implementation, three tables.

---

## 9. Orphan-coverage matrix — every exit path accounted for

| Runtime's fate | What stops the driver |
|---|---|
| normal scope exit | `bridge_close` (graceful, §4.5) |
| early error return | `defer` backstop `bridge_reap` (SIGKILL, §4.5) |
| trap → `failsafe` | registry `kill_all` via pidfd (§8) |
| `failsafe` itself dies / runtime SIGKILLed | **PDEATHSIG(SIGKILL)** from the kernel (§4.3) |
| PDEATHSIG residue (spawning thread exited early) | driver's mandatory **exit-on-EOF** (§7.5) |

v2 covered the first three. The last two are the cases no in-process mechanism *can* cover — only prearranged kernel behavior and driver-side discipline do — and a GPU/actuator server left running headless is a physical-safety event, not a leak.

---

## 10. Driver stderr

Pipe (v2's preference, confirmed) with the failure mode named: **a full pipe blocks the driver's writes** and manufactures a fake hang. Therefore the Bridge *continuously* drains `child_stderr` from a small async task in the Bridge's owning scope into a preallocated ring (last 8 KiB, drop-oldest), and the tail rides along in every `E_DRIVER_FAULT`/`E_DRIVER_DEADLINE` report. The drain task's lifetime is lexical (D-062) inside the Bridge scope — no detachment question.

Child fd map at exec: `0 ← /dev/null`, `1 ← /dev/null` (v1; a captured-stdout variant is a flag later), `2 ← pipe`, `3 ← ctrl`.

---

## 11. `#[gpu_kernel]` alignment

D-055 fixes the pipeline: kernels are Nitpick source → npkc → NVPTX/AMDGPU → kernel image; **no host-callable symbol exists**; launch is explicit dispatch. The protocol therefore reserves, without implementing in v1:

- `LOAD_MODULE` (B→D): a `{offset, len}` into the bulk region holding a PTX/SPIR-V image; driver returns a module handle. `kernel_id` becomes `{module, entry_index}` against a manifest emitted by npkc alongside the image.
- `EXEC_NOTIFY`'s descriptor gains a launch record: grid/block dims + parameter buffer `{offset, len}` (all **[UNTRUSTED]**-validated by the *driver* in this direction — the driver defends its own address space too, cheaply).

The GPU driver binary stays a generic loader/dispatcher outside the TCB. Compute logic stays in Nitpick, type-checked, per D-055. Nothing in v1's wire format has to change to add this — that is what the `abi_version` and reserved codes are for.

---

## 12. Sandbox (deployment configuration)

- `PR_SET_NO_NEW_PRIVS` — always, unconditional (§4.3).
- **Honesty note that v2 lacked:** a *tight* seccomp allowlist under a GPU userland is close to impractical — libcuda/Mesa need broad `ioctl`/`mmap` surfaces. Realistic menu, per driver class: namespace isolation (`CLONE_NEWNS` with a minimal mount set, `CLONE_NEWNET` empty for compute, `CLONE_NEWPID`), an rlimit set (`RLIMIT_NOFILE`, `AS`, `CORE=0`), and a *denylist*-shaped seccomp filter (block `ptrace`, `process_vm_readv/writev`, keyring, userfaultfd) rather than an allowlist. GUI drivers (GTK4) need a socket to the display server — different profile than compute; another reason they are different Bridges.
- `spawn_driver` takes an optional sandbox config; the mechanism stays out of the language, per v2.

---

## 13. Restart semantics (v2 §8.2, confirmed and sharpened)

1. A faulted Bridge is poisoned (§4.4); teardown then respawn.
2. **Everything is recreated** — memfd, mapping, socketpair, pidfd. A crashed driver's ring state is undefined; reuse is unrepresentable because the old region is unmapped before the new spawn.
3. Registry: old entry retired in teardown, new entry claimed at respawn — never edited in place.
4. Restart policy (backoff, attempt caps, degrade-vs-escalate) belongs to the caller — the Bridge reports, the application decides, matching the "failsafe is policy, libraries are mechanism" split (D-013).

---

## 14. Performance analysis — measured, not asserted

Numbers from [poc/kernel_mechanisms_test.c](file:///home/randy/Workspace/META/NITPICK/research/extern_driver/poc/kernel_mechanisms_test.c) test 5 on the deployment machine (kernel 7.0.0):

| Path | Measured |
|---|---|
| socketpair round-trip (dispatch floor, driver parked) | **~7.5 µs** |
| futex-in-shm round-trip (parked consumer) | **~6.5 µs** |
| SPSC ring, spinning consumer | **19M items/s** (1-byte items; descriptor-sized records will be lower but of the same order) |

What this says about the design:

- **The dispatch floor is one CUDA-launch-magnitude (~5–10 µs).** For D-055's workload — kernel launches, GUI commands — the control-plane cost is proportionate. It is *not* proportionate for fine-grained calls; the amortization story is **batching**: `EXEC_NOTIFY` announces "N descriptors are in the ring", the driver drains until empty, one completion per batch or per item as the caller chooses. The descriptor ring (§6.1) makes this a payload change, not a protocol change.
- **A parked futex is *not* meaningfully cheaper than the socket (6.5 vs 7.5 µs)** — the win of the reserved doorbell line (§6.1) is *suppression*: when the driver is already awake and draining, `driver_parked == 0` and the Bridge skips the notify syscall entirely — the steady-state busy pipeline costs ~0 syscalls/item. That is a v2 (post-measurement) optimization; the socket ships first and stays as the lifecycle channel regardless.
- **SeqCst vs acquire/release:** the delta is a store fence on x86 — nanoseconds against a microsecond floor. D-016 costs nothing that matters here; no intrinsic escape hatch is warranted.
- **False sharing** (§6.1) was the real ring-level cost in v2's layout; fixed by construction now.
- **"Zero-copy" stated honestly:** zero-*serialization* into shm, but a GPU H2D copy still exists on the driver side. Staged path: (a) v1 — driver `cudaHostRegister`s the mapped region once at INIT, making Bridge→GPU transfers true pinned-memory DMA with no staging copy; (b) later — *reverse* fd passing (driver exports a dma-buf / CUDA-IPC allocation to the Bridge), putting the bulk region in DMA-visible memory and deleting the copy. The protocol reserves driver→bridge SCM_RIGHTS for exactly this (`abi_version` gates it).
- **Result ABI:** every signature here returns ≤ 12-byte success types — register-returned per [D-084](file:///home/randy/Workspace/REPOS/nitpick-native/meta/specs/DECISIONS.md#L5650); the 13–16B cliff is not touched.
- **Spawn:** page-table copy cost accepted per-lifetime; `clone3(CLONE_VM|CLONE_VFORK)` only if profiling demands (§4.3).

---

## 15. Questions raised to the spec/compiler effort

1. **`Duration` / clock / executor timers** — undefined anywhere; blocks every deadline API, not just this plan (§3.4). Belongs in 1.1's cycle plan.
2. **Aliased-atomic storability** — is `atomic_from_ptr`'s result a value, a borrow, or local-only? (§3.2)
3. **`#ptr_add<T>`** — add to BUILTIN_REFERENCE's generated builtin set; settle element-vs-byte offset semantics (§3.3).
4. **Atomics lowering** is on no rung by name; propose the six-method set for `int32/int64/bool` in 1.1 (§3.1).
5. **Do `#wild_ptr`-fabricated pointers enter `<wild-live>`?** Decides whether the D-062 exit check sees mmap'd regions at all (§4.5).
6. **May a `defer` block `await`?** v3 no longer needs it (§4.5), but IO_REFERENCE's "scope exit closes streams" implies async work at scope exit and the answer should be written down.
7. **Slices/borrows held across a leaf `await`** — `async func:read = int64(Self, uint8[]:dest, …)` in IO_REFERENCE holds a borrow across its own suspension, which D-004 rule 4 forbids as written. However 1.1 resolves that for the I/O traits also resolves it for `send_fd`/`read_packet` here.
8. **Registry unification** — allocation + stream + driver registries as one preallocated, failsafe-reachable mechanism (§8).

## 16. Validation status

| Claim | Status |
|---|---|
| seals stop hostile shrink; unsealed shrink ⇒ SIGBUS | **demonstrated** (poc 1) |
| pidfd poll/kill/reap; ESRCH after reap | **demonstrated** (poc 2) |
| PDEATHSIG kills driver on runtime SIGKILL | **demonstrated** (poc 3) |
| MSG_NOSIGNAL ⇒ EPIPE; plain write ⇒ fatal SIGPIPE | **demonstrated** (poc 4) |
| cross-process acquire/release ring correctness; latency floor | **demonstrated** (poc 5) |
| Nitpick-side code | **blocked until ≥ 1.1** (§2); protocol + C driver + conformance tests buildable now |
