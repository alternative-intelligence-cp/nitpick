# poc — kernel mechanism validation for the driver architecture

Research/validation code for
[driver_architecture_plan_v3.md](../driver_architecture_plan_v3.md).
**Not** part of any Nitpick artifact; plain C, outside the TCB, exists only to
prove the plan's kernel-level claims on the actual deployment kernel.

```
gcc -O2 -Wall -o kernel_mechanisms_test kernel_mechanisms_test.c
./kernel_mechanisms_test
```

Last run: **18/18 pass** on `7.0.0-28-generic` (2026-08-20).

| # | Validates | Plan section |
|---|---|---|
| 1 | memfd seals stop a hostile `ftruncate` (EPERM); without seals the same attack **SIGBUSes** the mapping process | v3 §4.3 step 2 (finding A1) |
| 2 | pidfd: pollable death, `pidfd_send_signal`, `waitid(P_PIDFD)` reap, **ESRCH after reap** (no pid-reuse race) | v3 §5, §8 (finding A4) |
| 3 | `PR_SET_PDEATHSIG(SIGKILL)`: the driver dies when the runtime is SIGKILLed — the case `failsafe` can never cover | v3 §9 (finding A5) |
| 4 | `send(MSG_NOSIGNAL)` to a dead peer ⇒ `EPIPE`; plain `write()` ⇒ **fatal SIGPIPE** | v3 §4.2 (finding A2) |
| 5 | cross-process SPSC ring over sealed memfd with C11 acquire/release atomics — zero corruption; latency: socketpair RTT ≈ 7.5 µs, futex-in-shm RTT ≈ 6.5 µs, ring ≈ 19 M items/s | v3 §6, §14 |

The measured numbers in v3 §14 come from test 5 on this machine; re-run after
kernel upgrades or on new hardware before revisiting the performance section.
