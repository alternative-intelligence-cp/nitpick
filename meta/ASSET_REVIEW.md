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

**112 files, 20,702 lines — the largest reusable asset in the ecosystem.**

It was overlooked initially because `nitpick-libc` was mistaken for the C-free
libc effort. It is not: `nitpick-libc` was the *earliest* experiment, built on a
musl tree while ideas were still being tested, and was all but deprecated within
the prototype itself. `libn` came later to remove that dependency, and this
directory is where it was promoted.

### Verdict by tier

| Tier | Files | Lines | Disposition |
|---|---|---|---|
| **1 — C-free, portable** | 63 | **11,406** | port; same D-012 signature pass as `libn` |
| **2 — deprecated C shims** | 17 | 2,731 | reimplement natively, or drop as superseded |
| **3 — FFI bindings to the C++ compiler** | 19 | 6,393 | **do not port** — this is what the project exists to eliminate |
| **4 — tests & scratch** | 13 | 284 | assess separately |

**55% of the directory is directly portable.** Another 31% is the FFI bridge to
the C++ backend and must not come across at any price.

### Tier 1 — what is actually there

| Domain | Files | Lines | Notes |
|---|---|---|---|
| **regex engine** | 14 | **2,339** | `regex_compiler` (721), `nfa_compiler` (311), `regex_vm` (297), `regex_cache` (261), `prefix_extractor` (154). **Zero extern anywhere.** |
| **math / physics** | 7 | **2,309** | `quantum` (705), `complex` (566), `math` (372), `number`, `linalg`, `wavemech` |
| **strings & fmt** | 5 | 1,681 | `string_convert` (602), `string` (449), `print_utils`, `fmt`, `string_builder` |
| **system / io** | 11 | 1,720 | `sys` (391), `io`, `net`, `process`, `signal`, `pipe`, `shm`, `ntime`, `nfs`, `nurl` |
| **concurrency** | 7 | 1,068 | `channel` (351), `thread_pool` (157), `rwlock` (126), `mutex` (121), `thread` (111), `actor` (111), `condvar` (85) |
| **memory** | 5 | 498 | `arena` (91), `drop` (143), `pool_alloc` (102), `mem` (95), `handle` (62) |
| **data & misc** | 14 | 1,791 | `binary` (349), `dbug` (289), `base64` (224), `buffer` (216), `hexstream`, `random`, `rules`, `bld`, `pkg` |

**The regex engine is the standout.** ~2,300 lines, entirely C-free, including a
compiler, an NFA builder, a VM, a cache, and a prefix extractor. `nregx` was named
as a dependency of several later libraries (websockets among them), so this
unblocks more than itself.

**`quantum.npk` (705), `complex.npk` (566), and `wavemech.npk` (175)** are the
physics substrate — directly relevant to Nikola, and dependency-free.

### Tier 2 — the C shims

| File | Lines | extern | Disposition |
|---|---|---|---|
| `collections.npk` | 460 | 33 | **rebuild** — containers are foundational and this is a pure shim |
| `json.npk` | 315 | 96 | rebuild |
| `toml.npk` | 309 | 62 | rebuild (needed by `nitpick.toml` parsing) |
| `atomic.npk` + `atomic/int32.npk` | 244 | 11 | **do not port** — superseded by the language-level `atomic<T>`, which emits native LLVM atomic IR with no shim (`TYPE_REFERENCE.md` §13) |
| `lockfree.npk` | 115 | 16 | rebuild |
| `wave.npk` | 563 | 4 | rebuild — small C surface, mostly native |
| `aifs.npk` | 167 | 25 | assess — purpose unclear |
| `barrier.npk` | 34 | 3 | rebuild |
| `lib_hashmap_*` (3), `lib_vec_*` (4) | 352 | 44 | **delete** — see below |

The seven `lib_hashmap_int32_int64` / `lib_vec_int64` style files are **hand
monomorphized containers**, written because generics were not available. With
generics specified (D-030), one generic implementation replaces all seven. They
should not be ported in any form.

All Tier 2 files are **already marked `DEPRECATED` in their own source headers**,
so the project had identified them independently.

### Tier 3 — do not port

`parser.npk` (1,921), `type_checker.npk` (1,806), `lexer.npk` (1,270),
`module_table`, `visibility_checker`, `diagnostics`, `borrow_checker`,
`const_evaluator`, `definite_assignment`, `closure_analyzer`, `exhaustiveness`,
`safety_checker`, `async_analyzer`, `warnings`, and the tooling modules.

**These are not Nitpick implementations of compiler passes.** They are `extern`
declaration blocks binding to the C++ compiler — `lex_new`, `ast_new_node`,
`tc_new` are C++ functions. `borrow_checker.npk` is 49 lines of which 19 are
extern declarations; the file header describes the algorithm and then declares
the C++ entry points that implement it.

This is the same arrangement as `npkc-native/src/backend/ffi_bridge.npk`, and it
is precisely what `nitpick-native` exists to replace. **Correction to an earlier
assessment**: these were previously described here as "frontend analyses written
in Nitpick" that might inform the new compiler. They are not, and they will not.

Their file headers remain useful as *documentation* of what each pass does —
`borrow_checker.npk` states the borrow rules, the wild-memory state machine
(`UNINIT → ALLOC → FREED/MOVED`), and the leak-detection strategy in prose.

### A prototype bug worth carrying forward

`borrow_checker.npk` records: *"Aria wrapper functions removed due to compiler
codegen bug (`wild int8->` NIL comparison + extern call crashes LLVM)."* Worth a
regression test in the new compiler.

### Recommendation

Assess Tier 1 for porting **after** `nlibc`'s core lands, in this order:

1. **regex** — largest self-contained win, unblocks downstream libraries
2. **concurrency** — needed for the D-032 executor model, and `thread`/`mutex`
   already go through raw syscalls that `nlibc` supplies
3. **strings, memory, system/io** — overlaps `libn`; reconcile rather than
   duplicate, since both directories implement string and I/O layers
4. **math / physics** — no dependencies, can happen any time

Note the overlap risk: `libn/src/str/` and `stdlib/string*.npk` both exist, as do
`libn/src/io/` and `stdlib/io.npk`. Deciding which survives is a prerequisite to
porting either, or the work gets done twice.
