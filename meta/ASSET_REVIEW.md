# Reusable Asset Review

Assessment of existing code that might be carried into `nitpick-native`, measured
against the decisions in `meta/specs/DECISIONS.md`.

---

## `libn` — `REPOS/ARCHIVE/libn`

**Verdict: port it, do not restart.** It is in better shape than expected and the
expensive parts are exactly the parts D-011 says must exist.

### What is actually there

The repository is noisy — 676 Python fix-up scripts, 159 loose `.npk` files in
the root, plus `scratch/` and `_workspace_cleanup/`. **The real library is
`src/`: 58 `.npk` files, 17,668 lines.** Everything else is porting debris and
can be ignored.

```
src/mem/     alloc.npk (739)  memutil.npk (474)  memcpy.npk (221)
             memset.npk (184) mmap.npk (158)
src/io/      + io/bio/ (13 files — buffered IO, FILE)
src/str/     (11 files)
src/proc/    (7)   src/syscall/ (4)   src/time/ (4)
src/fs/      (2)   src/math/ (1)
```

### The allocator question — answered

**`libn` implements its own allocator; it does not call C's `malloc`.**
`mem/alloc.npk` is a size-class **slab allocator layered on raw `mmap`**:
per-class freelists, slab pages carved into slots, a direct-`mmap` path for large
objects, guard-page support, and header-tracked allocation sizes.

`libn_mem_malloc`, `mem_calloc`, `mem_realloc`, `mem_free` are its **public API
surface**, implemented on `libn_mmap` — not imports. Earlier grep hits for
"malloc" were these definitions, not calls into libc.

### Zero C dependency surface

Every `extern` match inside `src/` is in a **comment**. There are no `extern`
blocks in the real library. It reaches the kernel through its own syscall layer
(`src/syscall/`), not through libc.

Note this is the *opposite* of `REPOS/nitpick-libc`, which contains a full
**`musl-1.2.6`** tree (1,626 `.c` files, 874 `.h`) plus prebuilt `.a` shims. That
repository is C and is precisely what the zero-dependency rule excludes. Do not
confuse the two — the Nitpick-native code is in `ARCHIVE/libn`.

### It already covers part of D-011

D-011 requires Nitpick-native implementations of the symbols LLVM emits behind
our back. `libn` already provides two of the five:

| D-011 symbol | Status in `libn` |
|---|---|
| `memcpy` | `src/mem/memcpy.npk` — 221 lines |
| `memset` | `src/mem/memset.npk` — 184 lines |
| `__divti3` / `__udivti3` / `__modti3` | absent — must be written |

### Compatibility with the settled decisions

Measured across all 58 `src/` files:

| Construct | Files | Impact |
|---|---|---|
| `gc` | **0** | D-003 costs `libn` nothing — no collector usage to remove |
| `wild` / `wildx` | 0 | see concern 3 |
| `$$i` / `$$m` borrows | **0** | D-004 has no existing violations to fix |
| `defer` / `nodrop` | 0 | — |
| `arena<` / `Handle<` | 0 | not used; opportunity rather than problem |
| `unknown` | 1 | trivial audit |
| `ok(` | 0 | — |
| `Result<` | 38 | already on the `Result` discipline |
| `pass` / `fail` | 48 / 37 | ditto |
| `raw` | 35 | audit — each is a deliberate safety bypass |
| typed pointers (`->`) | 41 files, 988 uses | already typed, not raw addresses internally |

**The headline: zero usage of everything D-003 removed.** The GC decision, which
was the most disruptive call made today, requires no changes to `libn` at all.
Bitwise-on-`tbb` (banned by D-008) turned up **no violations** either.

### Porting concerns, in order of size

1. **API boundary returns raw `int64` addresses.** 514 of ~560 public functions
   are declared `= int64(...)`, including the allocator. Internally the code uses
   typed pointers (`uint8->:p`, `StrBuf->:s`, `FILE->:f`), but addresses cross the
   API as bare integers. Under the new design the allocator should hand back
   `wild int8->` so the leak-check and escape rules can see it. **This is the one
   genuinely mechanical, repo-wide change**, and it should be decided before any
   porting starts.
2. **`wild` annotations are absent.** Memory from the allocator *is* unmanaged by
   nature; under D-003/D-004 it must be marked `wild` so leak-checking at `exit`
   and the second-class borrow rules apply. Related to concern 1 — both are the
   same underlying gap.
3. **Legacy type names.** `fix256_t` and `tfp64_t` appear in return types. Per
   `SPEC_GAPS` §3 the fixed-point family was renamed (`tfp` plain, `dim` for
   dimensional analysis), so these need updating.
4. **`tbb` audit against D-008.** `tbb` appears across 35 files. No bitwise
   violations, but cast sites need review against D-008 §6 — width changes and
   `tbb`↔plain-integer conversions are no longer straight casts.
5. **D-002 does not apply.** No `extern` blocks means no FFI error contracts to
   write. Good news.

### Recommendation

Port `src/` selectively, ignoring the scratch/debris. Take the allocator, `mmap`
layer, `memcpy`/`memset`, and the syscall layer essentially intact — that is the
hard-won part and it is already dependency-free. Decide the pointer-type
convention (concern 1) first, since it touches nearly every public signature and
is far cheaper to do once, mechanically, than to discover mid-port.

---

## `nitpick-posix` — `REPOS/nitpick-posix`

**Verdict: reference only. Do not treat as a base.**

Roughly 30 coreutils directories exist, but **only `ls` (3 files), `sort` (2),
`cal` (2), `yes` (1), and `crypto` (5) contain any `.npk` source.** The remainder
are empty scaffolding. Real content is ~14 utility files plus 18 vendored and 17
test files.

**It does not reference `libn` anywhere** — zero files. The intended
"posix utilities built on `libn`" layering was never actually wired up, so there
is no integration to inherit, only individual utilities to consult.

Worth revisiting once `libn` is ported and the layering can be done properly.

---

## Also present, not yet assessed

`REPOS/ARCHIVE/` holds 18 further libraries that may be worth the same treatment
later: `nstr`, `nstr-builder`, `nfs`, `nmath`, `nrand`, `nregx`, `nsocket`,
`nsync`, `nthread`, `ntime`, `njson`, `ntoml`, `nurl`, `nvec`, `nbase64`,
`ncrypto`. Several are named in `npkc-native`'s manifest as dependencies, so they
were live parts of the ecosystem.


---

## The prototype standard library — `REPOS/nitpick/stdlib`

**This is where `libn` was promoted, and it is the largest reusable asset found
so far: 85 `.npk` files, ~20,700 lines.**

It was overlooked initially because `nitpick-libc` was mistaken for the C-free
libc effort. It is not — `nitpick-libc` was the *earliest* experiment, built on a
musl tree while ideas were still being tested, and was all but deprecated within
the prototype itself. `libn` came later to remove that dependency, and this
directory is where it ended up.

### Concurrency modules

| Module | Lines | C surface |
|---|---|---|
| `thread.npk` | 111 | **none** — raw syscalls only |
| `thread_pool.npk` | — | **none** |
| `mutex.npk` | 121 | **none** — futex-based |
| `rwlock.npk` | 126 | **none** |
| `condvar.npk` | 85 | **none** |
| `channel.npk` | 350 | **none** |
| `actor.npk` | 111 | **none** |
| `atomic.npk` | 181 | ⚠️ DEPRECATED — `extern "nitpick_runtime"` → `atomic_shim.cpp` |
| `barrier.npk` | 34 | ⚠️ DEPRECATED — `npk_shim_barrier_*` |
| `lockfree.npk` | — | ⚠️ DEPRECATED — `npk_shim_lfqueue_*` |

Seven of ten are already C-free. The three that are not are **marked deprecated
in their own source**, so the project had identified them independently.

`atomic.npk` should not be ported at all — it is superseded by the language-level
`atomic<T>`, which emits native LLVM atomic IR with no shim
(`TYPE_REFERENCE.md` §13). `barrier` and `lockfree` need native reimplementation.

`libn` itself supplies the primitives these sit on: its syscall layer already
wraps `futex` (12 uses), `clone` (5), `gettid`, `tkill`, and `set_robust_list`.

### Not yet assessed

The other ~75 files include `arena.npk`, `allocator.npk`, `collections.npk`,
`buffer.npk`, `binary.npk`, `complex.npk`, `crypto/`, `base64/`, `jit.npk`, and
compiler-support modules (`borrow_checker.npk`, `closure_analyzer.npk`,
`definite_assignment.npk`, `const_evaluator.npk`, `async_analyzer.npk`,
`diagnostics.npk`). Several of those last ones are **frontend analyses written in
Nitpick** and may be directly relevant to building the new compiler.

This directory should get the same treatment `libn/src` received before any
further porting decisions are made.
