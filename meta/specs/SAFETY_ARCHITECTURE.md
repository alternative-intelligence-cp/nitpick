# Nitpick Safety Architecture & Concepts

Nitpick fundamentally rejects undefined behavior (UB), uncontrolled crashes, and implicit failures. To achieve this, the language architecture relies on a **Defense in Depth** strategy. Safety in Nitpick is not a single feature but an interconnected set of layers—spanning from compile-time mathematical proofs down to a mandatory runtime trap handler.

This document outlines how these safety concepts play together, their purposes, and how they dictate the architecture of the language and compiler.

---

## The Three Layers of Safety

Nitpick's safety architecture can be conceptualized in three layers, checked sequentially from compilation down to runtime execution:

1. **Layer 1: Formal Verification (Compile Time)** - Prevent invalid states mathematically.
2. **Layer 2: Safe Runtime Semantics (Runtime)** - Handle expected failures securely without exceptions.
3. **Layer 3: The Mandatory Failsafe (Recovery/Trap)** - The ultimate, un-bypassable safety net.

> **Layer numbering is fixed as above.** `FORMAL_DRAFT` 12.4 calls `failsafe` the
> "Layer-1 safety net" while 12.1 places it at Layer 3, four paragraphs earlier.
> Layer 3 is correct — it is the last line, not the first.

## Target applications

The bar is set by what the language is for: **AGI consciousness substrates
(Nikola)**, robotics, medical devices, aerospace, and nuclear control. In every
one of those, a silent failure or an unexpected core dump is catastrophic rather
than inconvenient — which is why undefined behaviour is rejected outright instead
of merely discouraged.

---

### Layer 1: Formal Verification (Compile Time)

The primary goal of Nitpick is to shift as much error handling as possible to compile-time using the **Z3 SMT Solver**.

*   **`limit<Rules>` & Value Constraints**: Instead of checking bounds manually, developers declare structural constraints (`Rules`) and bind them to types (e.g., `limit<r_positive> int32:x`). The compiler mathematically proves these constraints are never violated. 
*   **Design by Contract (`requires` / `ensures`)**: Functions declare preconditions and postconditions. The Z3 solver verifies that all callers satisfy the `requires` contract and that the function body satisfies the `ensures` contract.
*   **Formal Proofs (`prove` & `assert_static`)**: Developers can force the solver to construct mathematical proofs of arbitrary expressions (`prove(x != 0)`), which accumulate path conditions (like being inside an `if(x > 0)` block).

**Architectural Impact**: The compiler must tightly integrate with an SMT solver (Z3) during the type-checking/semantic analysis phase. Path condition accumulation must be a core part of the AST walk.

---

### Memory safety is a Layer 1 property, not a runtime one

`--verify-memory` covers use-after-free and recursion bounds. The mechanisms that
deliver it are all **compile-time and structural**, with no runtime checks:

| Property | Mechanism |
|---|---|
| no dangling references | **second-class borrows** — a borrow passes down the call stack and never up (D-004) |
| no use-after-free in arenas | **generation-counted `Handle<T>`** — a stale handle fails safely through `Result<T>` rather than dangling |
| no data races | borrows cannot cross a thread spawn or `await` (D-004); tasks do not migrate (D-032); shared arenas never move memory or reuse slots (D-017) |
| leaks are detected, not silent | the **K-semantics `exit` rule** — reaching `exit` with live `wild`/`wildx` memory traps to `failsafe` instead of returning |
| no collector pauses | there is no garbage collector (D-003) |

---

### Layer 2: Safe Runtime Semantics (The `Result<T>` Intercept)

When a state cannot be mathematically proven at compile time (e.g., reading user input or dynamic data), Nitpick falls back to **sticky error propagation**. Exceptions and `catch` blocks are entirely absent from the language.

*   **The `Result<T>` System**: Functions that can fail must return a `Result<T>`. Developers return using `pass value;` or `fail errCode;`.
*   **Forced Handling**: The caller *must* explicitly handle the `Result<T>` using specific operators:
    *   `?` (Safe Fallback): Swallows the error and provides a default.
    *   `?!` (Emphatic Unwrap): Asserts success; traps to Layer 3 (`failsafe()`) if it errors.
    *   `raw` (Explicit Cast): Bypasses safety (acts as a searchable "TOS" keyword).
    *   `drop`: Explicitly discards the result.
*   **The Contract Intercept**: This is where Layer 1 meets Layer 2. If a function has a `requires` contract that cannot be statically proven, Nitpick automatically forces the function to return a `Result<T>`. If the caller violates the contract at runtime, the function immediately intercepts and returns a `Result` error rather than crashing.

**Architectural Impact**: `Result<T>` is heavily intertwined with the type system and ABI. The compiler must automatically inject runtime bounds/contract checks and wrap return types implicitly when verification is deferred to runtime.

#### All three Layer 2 mechanisms

Layer 2 is not only `Result<T>`. Three distinct mechanisms live here, and which
one applies is determined by **type**, never by context (D-007):

| Mechanism | Applies to | Behaviour |
|---|---|---|
| **`Result<T>`** | every function except `main` / `failsafe` | errors are values the caller is forced to handle |
| **`tbb` sticky ERR** | `tbb8/16/32/64` arithmetic | undefined operations yield ERR, which propagates through data and **traps at control flow** — a tainted value can never steer a branch (D-008) |
| **`unknown` taint** | `Result.value` after `fail()` | compiler-assigned; cleared by `ok()` or by checking `is_error` |

`unknown` is **not** user-writable and is **not** the general degradation
mechanism it was in the prototype. Division by zero on a plain `int32` traps to
`failsafe`; on a `tbb` it yields ERR and execution continues. See
`TYPE_REFERENCE.md` §6 and §27.

`tbb` sticky ERR is what actually delivers **fail-operational** behaviour, and it
is the mechanism the robotics path depends on.

---

### Layer 3: The Mandatory Failsafe

The ultimate safety net. If an unrecoverable state is reached, execution is trapped.

*   **The `failsafe` function**: Every Nitpick **executable** must define a `func:failsafe = int32(tbb32:err)`. It acts as the global trap handler.
*   **Exactly one per program, supplied by the end user** (D-013). Libraries do **not** define one — there is no chaining, no ordering question, and no second claimant on `exit`. Libraries manage their own resources with `defer`, which is per-scope and composes; `failsafe` is whole-program emergency *policy*.
*   **Triggers**: It is invoked via explicit developer commands (`!!! errCode`), emphatic unwraps (`?! errCode`), comparison or branching on a `tbb` ERR value, or critical unrecoverable runtime panics (e.g., Out Of Memory).
*   **`defer` does not run on a trap** (D-014). `!!!` and `?!` transfer control directly, without unwinding. At trap time the state of the system is unknown — including how degraded it is — so no cleanup runs before the handler that understands the situation gets control. `failsafe` receives the allocation registry intact.
*   **It must not assume a healthy system.** The trap may fire in an arbitrarily degraded state: allocation, file access, and hardware may all be unavailable. Programs driving robotics should **preallocate whatever the shutdown path needs** before the fault, and have it standing ready.
*   **Enforced requirements**: it must exist; its body must not be empty; it must return a **positive** value — reaching `failsafe` means something failed, so returning `0` is a contradiction; and it **may not be `async`** (D-063), because a handler that could suspend could be starved by the executor it is shutting down. The positive-return rule is implemented as a compiler-injected `ensures result > 0i32` contract, verified by Z3 through the existing Design-by-Contract machinery.
*   **A trap is a whole-program event** (D-063). No coroutine is resumed on any thread, no `defer` runs anywhere, and no frame is destroyed — frames freeze in place, since `coro.destroy()` would execute exactly the cleanup the previous point forbids. Every other thread's executor stops **before** `failsafe` gets control, so the handler cannot be racing a sibling task driving the same actuator back the other way. `failsafe` itself runs on the trapping thread as a plain call, never scheduled.
*   **Async adds no new safing requirement.** Because `defer` already does not run on a trap in synchronous code, a function holding an actuator open already could not rely on it. Safing belongs to `failsafe`, reached through state **preallocated before the fault** — and it must be reachable without traversing a task frame, because frames are frozen and unreadable as live objects at that point.
*   **K-Semantics on `exit`**: The failsafe and `main` are the only places allowed to call `exit`. Nitpick enforces that no unchecked manual memory (`wild`/`wildx`) is active upon a **successful** `exit`, and — per D-062 — that no task frames are live either. If a leak or a live task exists during a successful exit attempt, it traps back into the failsafe (`-4105`, 0.10.1/D-151) to enforce cleanup; a failure exit keeps its code. `failsafe` may clean up with `wild_release_all()` and exit positive.
*   **OOM routes to `failsafe` (`-4103`) through a path that allocates nothing.** This is the allocator's own obligation, stated at the boundary where the condition originates (total_audit C-3): OOM handling runs when allocation just failed, so the failsafe-reachable state — the registries, the trap route — is preallocated, the D-014 discipline applied to the allocator specifically.

**Architectural Impact**: The standard library and runtime must establish a global trap mechanism before invoking `main`. Memory allocations (specifically `wild`/`wildx`) must be tracked globally (e.g., in a `<wildx-states>` map) to validate the `exit` condition.

---

## Escape Hatches (Opt-in Unsafe)

For low-level systems programming, safety checks can be intentionally bypassed, but they are highly visible and grep-able:

*   **`raw`** / `_!`: unwrap a `Result<T>`'s value. **D-163 (settled; the contract landed 1.1.0) makes this a CHECKED, zero-cost unwrap, not a bypass**: `raw` is licensed only on a call whose callee is declared `never fails`, so it proves the check redundant rather than skipping it, and leaves the Escape-Hatches list. The contract exists and is checked since 1.1.0; the refusal of an unlicensed `raw` flips at 1.1.2 (after the sweeps), until when `raw` still behaves as the unchecked bypass described here (D-001).
*   **`wild` / `wildx`**: Unchecked, unbounded manual memory pointers.
*   **`#wild_ptr<T>(addr)`**: Constructs a pointer from an integer address; legal only in `wild` context (D-019).
*   **`=>!`**: Unchecked cast, opting out of the compile-time data-loss check.
### `--extra-picky`

Optional but strongly recommended. Rules default to hard compilation errors and
are parameterizable: `--extra-picky=warn-<rule>` downgrades one to a warning,
`--extra-picky=no-<rule>` disables it.

| Rule | Effect |
|---|---|
| `literal-suffixes` | every integer literal must carry an explicit bit-size suffix (`42i32`, not `42`), eliminating sizing ambiguity |
| `shadow` | bans inner scopes redefining outer names, ignoring macro-generated hygiene names |
| `wild` | rejects `wild`/`wildx` on declarations, parameters, and return types, keeping high-level code away from unchecked pointers |
| **`require-tbb`** *(new, D-007)* | requires `tbb` arithmetic in designated real-time code, making the fail-operational path a **compile-time guarantee** rather than a convention |
| **`no-failsafe-alloc`** *(new, D-014)* | rejects allocation inside `failsafe`, partially enforcing the preallocation discipline |
| **`no-sys`** *(new, D-048)* | rejects direct `sys` calls in high-level application code, the way `wild` rejects manual memory. Syscalls belong in `nlibc`; application code reaches the kernel through the typed API |
| **`no-wildx`** *(new, D-035)* | rejects runtime code generation independently of `no-wild`. Manual memory and JIT are very different risks and should not share one switch |

> **`explicit-widening` was removed from this table (D-092, corrected).** It read
> "bans implicit widening; all widenings use an explicit cast", and a rule can
> only be *optional* if the default permits what it bans — which would have made
> implicit widening the default. It is not. Widening always requires a cast, so
> the rule has nothing left to switch off.
>
> The misreading is worth recording because the table invites it. **Every rule
> here adds pedantry beyond what safety requires; none of them gates a safety
> property.** `shadow` bans shadowing, which is confusing rather than unsafe.
> `wild` and `no-wildx` ban constructs that are already explicit and greppable.
> `literal-suffixes` requires a suffix where the width could be inferred safely.
> Reading any row as "the default permits the unsafe thing" inverts the sentence
> immediately below this table.

Every escape hatch is explicit, named, and greppable. That is the standing shape
of a Nitpick guarantee: absolute by default, suspended only through a construct an
auditor can search for.
