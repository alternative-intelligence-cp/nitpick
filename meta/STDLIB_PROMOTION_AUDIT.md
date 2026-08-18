# Standard-library promotion audit

**What is actually C-free, what was actually promoted, and what `nlibc` still has
to absorb.** Measured against the prototype and `REPOS/ARCHIVE/` as they stand
today; nothing was modified, and nothing in either may be.

Written because the state was assumed rather than known. The assumption this
started from — "`nlibc` is `extern` declarations all the way down" — was wrong on
the facts, and correcting it changed an open question's urgency.

---

## 1. `nlibc` has no implementation

`REPOS/nlibc` holds **zero `.npk` files**. It is a licence, a manifest, empty
`src/` and `tests/`, and a `meta/` directory of real planning work:
`PORT_PLAN.md`, `VARIADIC_COLLAPSE.md`, `SYSCALL_LAYER_REMOVAL.md`,
`FORMAT_LOWERING.md`, `EXEC_FAMILY.md`, and two ledgers.

`PORT_PLAN.md` says so in its own first line: **"No code ported yet."**

### What that corrects

`nlibc` is the port *target* for `ARCHIVE/libn`, and it is planned to have **very
little `extern` surface at all**, because the port deletes most of what would
need one:

| Family | Before | After | Replaced by |
|---|---:|---:|---|
| `printf` / `scanf` | 126 | **0** | `&{ }` interpolation, functions returning `string` (D-053) |
| `sysN` / `sys_fullN` | 10 | **0** | the `sys` builtin (D-047, D-048) — already in this compiler |
| `execlN` / `execlpN` | 17 | **0** | array literals |

Public functions go 608 → 450, parameters 1,710 → ~825, and
`VARIADIC_COLLAPSE.md` contemplates that **none of the variadic families survive
as variadics.**

**So the open question about a C variadic tail (`NITPICK-TYPE-023`) is not
Phase-B-blocking.** It was recorded as blocking on the assumption that `nlibc`
would be FFI-shaped. It is not. The question is still open and still worth
settling — it is just not on the critical path.

---

## 2. Every archived library is C-free

All seventeen, measured by `extern:"…"` blocks in `.npk` sources:

| Library | `.npk` files | `extern` blocks | stray C/C++ |
|---|---:|---:|---|
| `libn` | 334 | **0** | 1 — `_workspace_cleanup/dump_tokens.cpp`, scratch |
| `ncrypto` | 129 | **0** | 3 — `stub.c`, `libncrypto.so`, a test `.c` |
| `nregx` | 41 | **0** | 0 |
| `njson` | 25 | **0** | 0 |
| `nstr` | 5 | **0** | 0 |
| `nsync` | 5 | **0** | 0 |
| `ntoml` | 4 | **0** | 0 |
| `nbase64`, `nfs`, `nrand`, `nthread`, `ntime`, `nvec` | 3 each | **0** | 0 |
| `nmath`, `nsocket`, `nstr-builder`, `nurl` | 2 each | **0** | 0 |

**The C-free claim holds.** The stray files are build residue and scratch, not a
dependency layer — `dump_tokens.cpp` sits in a directory literally named
`_workspace_cleanup`, and `ncrypto`'s are a stub and a test harness beside a built
`.so`.

Worth noting for expectations: several archives are **two or three files**
(`nmath`, `nsocket`, `nurl`, `nstr-builder`). Those are early or thin, not
finished libraries. `libn`, `ncrypto`, `nregx` and `njson` are the substantial
ones.

---

## 3. Promotion was partial, and two subdirectories are husks

Promoted libraries became **subdirectories** of the prototype's `stdlib/`. None of
the nine contains a single `extern` block — **the promotion discipline held where
it happened.** What did not hold is completeness:

| `stdlib/` subdir | `.npk` | Archive counterpart | `.npk` | State |
|---|---:|---|---:|---|
| `regex` | 20 | `nregx` | 41 | partial |
| `fs` | 2 | `nfs` | 3 | near-complete |
| `base64` | 1 | `nbase64` | 3 | partial |
| `time` | 1 | `ntime` | 3 | partial |
| `url` | 1 | `nurl` | 2 | partial |
| `atomic`, `traits` | 1 each | — | — | no archive counterpart |
| **`crypto`** | **0** | `ncrypto` | 129 | **husk — `.o` files only** |
| **`sys`** | **0** | — | — | **husk — `.o` files only, plus `all.npk.bak`** |

Across the nine subdirectories there are **61 `.o` files against 27 `.npk`**.
`stdlib/crypto` contains `asymmetric/ecc.o`, `asymmetric/rsa.o`, `util/bytes.o`
and a Python fix-up script — and no sources. `stdlib/sys` is the same shape with
`all.npk.bak` beside it.

**So `ncrypto` was not promoted in any usable sense.** Its 129 source files are in
the archive; what reached `stdlib/crypto` was compiled output that has since lost
its sources. The same is true of `sys`.

**Eleven archived libraries have no `stdlib/` counterpart at all**: `libn`,
`njson`, `nmath`, `nrand`, `nsocket`, `nstr`, `nstr-builder`, `nsync`, `nthread`,
`ntoml`, `nvec`. So *archived* did not reliably mean *promoted* — the archive is
better read as "finished enough to stop working on in place" than as "shipped".

---

## 4. The top level is the original C/C++ standard library

Eight files still link C shims, and they are the pre-Nitpick stdlib the project
started with:

| File | Links |
|---|---|
| `arena.npk`, `pool_alloc.npk` | `libnitpick_libc_mem` |
| `condvar.npk`, `mutex.npk`, `shm.npk` | `libnitpick_libc_thread` |
| `net.npk` | `libnitpick_libc_net` |
| `string.npk` | `libnitpick_libc_string` |
| `hexstream.npk` | `libnitpick_libc_hexstream` |

Five shim libraries, four concerns: memory, threading, networking, strings — plus
`hexstream`.

**None of these is a porting target.** They are what had to exist to get going, and
every one is superseded by work that already exists or is planned:

| Concern | Superseded by | Status |
|---|---|---|
| memory | `libn/src/mem/` (5 files, 46 fns) | in `nlibc`'s port plan |
| strings | `libn/src/str/` (11 files, 138 fns), `ARCHIVE/nstr` | in the port plan |
| threading | `ARCHIVE/nsync`, `ARCHIVE/nthread` | **and D-056 redesigns the API anyway** — `Mutex<T, LEVEL>`, no untimed wait |
| networking | `ARCHIVE/nsocket` (2 files — thin) | needs building, not porting |
| `hexstream` | — | **drop it** |

### `hexstream` specifically

It is not a standard-library concern. It belongs in an OS-facing or stream library
if it is revived, and **no operating system currently supports what it needs** —
so a core-language dependency on it is a dependency on something that cannot work.
The idea is worth keeping; the placement is not. If a good implementation exists
later and OS support arrives, it can be promoted then, which is what the promotion
path is for.

---

## 5. What this means for the work

**For `nlibc`:** the port plan targets `ARCHIVE/libn/src` — 58 files, 17,668 lines,
610 public functions across `syscall/`, `mem/`, `str/`, `io/`, `proc/`, `fs/`. That
is unaffected by anything above; the audit confirms the source is C-free and the
plan's deletions are what remove the FFI surface.

**For the concurrency stdlib (`OPEN_DECISIONS.md` §3):** `ARCHIVE/nsync` and
`ARCHIVE/nthread` exist and are C-free, but **D-056 changes the mutex API to
`Mutex<T, LEVEL>` and removes the untimed `CondVar.wait`**, so those are
references rather than ports. Cycle 0.5.6's analysis is ready for them: a
primitive participates by writing `acquires N`, and needs nothing else.

**For `ncrypto`:** 129 C-free source files in the archive, and a `stdlib/crypto`
that is empty of sources. If crypto is wanted, it is a **port from the archive**,
not a promotion that already happened.

**For the big switch:** the top-level C shim layer is the clearest example of what
`meta/SWITCH.md` means by restructuring before moving. None of it should travel.

---

## What was not checked, and what is known about it anyway

- **Whether the archived sources still compile** against the current language.
  **They almost certainly do not.** The language has changed substantially in
  important areas — things dropped, added and tweaked — so even where the syntax
  still looks close the resulting behaviour may not be. That is why the ledgers in
  `nlibc/meta/` exist: the port is a rewrite in places, not a copy.
- **Edge-case audit status per library.** These were **left mid-audit
  deliberately.** They were generally close, and the outstanding findings were
  implementation edge cases surfaced by external audits rather than C dependencies
  — but perfecting them stopped being worth it once the port to this compiler was
  in view, because everything would have to change for that anyway.

  So an unfinished audit here is a decision, not neglect, and a bug found in an
  archived library is expected rather than a discovery.

**The planning consequence: assume per-library work, never a bulk port.** Each one
gets assessed individually when its turn comes — what state it is in, and what has
to change — and this document exists so that assessment starts from measured facts
rather than from memory.
