# Open decisions and unwritten specs

The work queue as of D-061. Every item here blocks something concrete; the
"blocks" column says what. Nothing in this file is optional and nothing in it is
deferrable — per the standing constraint, anything going into the language has to
be in before the Astrée trial starts, because re-verification is unaffordable.

Ordering rationale is in §4. Items move to `meta/roadmap/done/` as they close,
with the deciding D-number recorded.

---

## 1. Decisions

### Safety-critical

| # | Item | Blocks | Source |
|---|---|---|---|
| 1 | **Task cancellation** — what happens to a pinned executor's arena of live task frames when a task is cancelled or the executor shuts down while frames are live. Interacts with the K-semantics `exit` rule. | concurrency spec; executor implementation | `CONCURRENCY_REFERENCE.md` §6 |
| 2 | **`async` + `failsafe`** — D-014 says a trap runs no `defer` and transfers directly to `failsafe`. What becomes of *other* in-flight tasks on that executor, and of their frames, is undefined. | concurrency spec; `failsafe` lowering | `CONCURRENCY_REFERENCE.md` §6 |

Item 2 is the uncontrolled-shutdown scenario `failsafe` exists to prevent: a trap
inside a suspended task with sibling tasks live on the same executor. Nothing
currently says what happens, which is why these two lead the queue.

### Frontend-blocking

The bootstrap strategy builds the frontend **once, in full**. Anything that
changes the parser, the AST, or the builtin-type table has to be settled before
that work starts, or the strategy fails in exactly the way it was designed to
avoid.

| # | Item | Blocks | Source |
|---|---|---|---|
| 3 | **Generics** — nested generics, implicit call syntax, the `Type` keyword, monomorphization strategy. Declaration syntax alone is resolved. D-056's `Mutex<T, LEVEL>` already commits to a const-generic parameter, so this is load-bearing today. | parser, AST, type checker, and every `<T>` type in the stdlib | `PRE_PLANNING_REVIEW.md` §4; `SPEC_GAPS` §3 |
| 4 | **`move` memory qualifier** — present in the `MemoryQualifier` production, specified nowhere. | lexer/parser; ownership rules; channel `send` | `LEXICAL_REFERENCE.md` open items |
| 5 | **`opaque` declaration form** — `struct \| opaque struct` versus the standalone `opaque:DatabaseHandle;`. Two spellings for one concept; blueprint philosophy says pick one. | parser | conflict 49 |
| 6 | **LBIM sticky ERR** — `int1024`…`int4096` are specified to propagate a sticky ERR sentinel, which makes them twisted types under D-036's own definition while wearing an `int` name. Three ways out are laid out in Part R. | builtin-type table; `ncrypto` port (34,925 lines) | Part R |

### ABI and lowering

| # | Item | Blocks | Source |
|---|---|---|---|
| 7 | **Fat vs thin pointers** — `FULL_specs` §15.1.3 says `int8->` carries bounds metadata; `TYPE_REFERENCE` §10 says `wild` and borrow pointers lower identically and the distinction is a type-checker matter. Interacts with D-004: if borrows are second-class and cannot escape, much of what fat pointers buy is already static and free. | all pointer lowering; FFI boundary; `--verify-memory`'s claim | Part W |
| 8 | **`tbb32` ERR encoding** — how the sticky ERR state lives inside `Result`'s 4-byte error field. A bare `i32` either loses the encoding or implies an undocumented reserved bit pattern. | `Result` lowering | D-005 follow-up |

### Policy

| # | Item | Blocks | Source |
|---|---|---|---|
| 9 | **LLVM / Z3 dependency boundary** — ch. 00b exempts "the LLVM IR generator and the Z3 SMT Subsystem" from the zero-dependency rule. Invoked as subprocesses over hand-written IR and SMT-LIB2 text (nothing in the TCB at runtime, which is what D-011 and D-015 already assume) is a very different claim from linking `libLLVM` and `libz3`. Never stated either way. | the zero-dependency claim as an auditor will read it | Part X |
| 10 | **Runtime `limit<Rules>` violation** — without `--verify`, constraints are "enforced dynamically at runtime", but not what a violation *does*. Presumably a trap to `failsafe`; it has to say so. | `limit` lowering | N5 |

---

## 2. Specs to write

| Spec | Why it is needed | Depends on |
|---|---|---|
| **Channels, actors, thread pools** | Real implementations exist with no specification. The concurrency model is not fully described until they are covered. | decisions 1, 2, 3, 4 |
| **Streams / IO** | Needed for the driver and for diagnostics. D-050 (line endings are a stream property) and D-051 (`Path` above `nlibc`) already constrain it. | decision 3 |
| **Build system** | Needed early for the bootstrap ladder. | — |

---

## 3. Carried, not blocking

- **Port the concurrency stdlib in dependency order.** `mutex`, `rwlock`, and
  `condvar` are genuinely C-free and go first — but D-056 changes the `mutex`
  API, so the port is against `Mutex<T, LEVEL>`, not the old `int64` handle.
  `thread`, `thread_pool`, `channel`, and `actor` wait on dropping `atomic.npk`
  and replacing `core.npk`/`string.npk` with `nstr`. `barrier` and `lockfree`
  need native reimplementation to drop their C shims. **`atomic.npk` should not
  be ported at all** — it is superseded by the language-level `atomic<T>`.
- **Confirm Astrée's accepted input format with AbsInt**, well before the trial
  clock starts. The trial is a single non-renewable 30 days.

---

## 4. Ordering

1. **Decisions 1 and 2 together.** They are one question asked twice — what owns
   a task frame, and who tears it down — and answering either alone risks an
   answer the other contradicts. Safety-critical, and they close
   `CONCURRENCY_REFERENCE.md` §6 apart from the port.
2. **Decision 3, generics.** The largest item and the one the whole frontend
   waits on. Everything spelled `<T>` in the stdlib inherits its answer.
3. **Decisions 4, 5, 10, 9.** Contained decisions, batched — each is small on its
   own and they touch unrelated parts of the language.
4. **Decisions 6 and 8.** Both are the same shape of question — where a sticky
   ERR lives inside a representation — so they are decided together.
5. **Decision 7, pointers.** ABI, and the most expensive to revisit.
6. **The three specs**, concurrency last since it consumes 1, 2, 3, and 4.

This is more than one day's work. That is expected and is not a reason to
compress it.
