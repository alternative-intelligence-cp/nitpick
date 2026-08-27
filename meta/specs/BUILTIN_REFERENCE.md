# Nitpick Built-in Reference

Nitpick provides a set of compiler intrinsics (built-ins) that are available globally without needing to `use` any external modules. These built-ins map directly to LLVM instructions or safe runtime shims.

> **Note on Data Structures:** Built-in collections (like stacks, lists, and hash tables) from early language prototypes have been moved to the Standard Library. You must explicitly import them via the `collections` module.

---

> **What is a builtin, exactly.** The regions between
> `<!-- builtins:begin -->` and `<!-- builtins:end -->` markers define the
> BARE-NAME BUILTIN set — the names the resolver admits with no declaration and
> no import, generated into `src/frontend/builtins.npk` by `gen_tables.py`.
> That set is deliberately small (0.8.4): the **runtime floor** (§1, §2b), the
> **`sys` builtin** (§3, D-047/D-048), and the **comptime-foldable string
> names** (§2c) — magic because the compiler itself evaluates them. Everything
> else this reference documents — the string library, formatting, the memory
> helpers — is `nlibc`'s API: ordinary Nitpick functions, written over the
> floor, imported like any other module. The first draft of the generator
> scavenged every code-shaped token in the file, so `close(2)` in a sentence
> about POSIX became a "builtin" nobody could call; the markers are the fix
> that cannot un-fix itself.

<!-- builtins:begin -->

## 1. Memory Management Built-ins (NitpickAlloc)

These intrinsics directly interface with the `NitpickAlloc` slab/VM allocator. They all return `wild` pointers — unmanaged memory outside RAII tracking, which the programmer must free. There is no garbage collector (D-003).
*Security constraint: every allocation carries a hidden 16-byte header — size plus a secret-keyed magic word, **not a CRC** (the "8-byte CRC32" an earlier revision claimed was wrong three ways — see `MEMORY_REFERENCE.md` §3). Double-free, corruption, and a foreign or misaligned pointer trap to `failsafe` with `-4102`; OOM with `-4103`; a malformed request (negative size, checked `calloc` multiply overflow, `ralloc(p, 0)`, bad alignment) with `-4104`. Double-free of a tracked binding is already a compile-time error (D-119); the runtime check covers what the analysis cannot follow.*

| Built-in | Signature | Description | Fails |
|---|---|---|---|
| `alloc` | `int64:size → wild int8->` | Allocates `size` uninitialized bytes, 16-aligned. `alloc(0)` is a real, unique, freeable block (D-150). Triggers failsafe on OOM. | **never fails** (traps on misuse) |
| `aalloc` | `int64:size, int64:align → wild int8->` | Allocates with the requested power-of-two alignment (0.10.0); `align <= 16` is the ordinary path. | **never fails** (traps on misuse) |
| `calloc` | `(int64:count, int64:size) → wild int8->` | Allocates `count*size` zero-initialized bytes. Prefer this over `alloc` + `memset`. | **never fails** (traps on misuse) |
| `ralloc` | `(wild any->:ptr, int64:new_size) → wild int8->` | Resizes an allocation; grows in place where the class or mapping allows, else allocates, copies (bounded by the OLD size), and frees. The old pointer is invalid after calling. `ralloc(NULL, n)` is a fresh allocation; `ralloc(p, 0)` traps (`-4104`, D-150) — freeing is spelled `dalloc`. | **never fails** (traps on misuse) |
| `dalloc` | `wild any->:ptr → void` | Deallocates a pointer, really (0.10.0): the slot is recycled. `dalloc(NULL)` **traps** (`-4102`, D-150) — `alloc` never returns null, so a null here is a state the author did not intend; there is no C-style free(NULL) cleanup idiom to serve. Double-free and foreign pointers trap deterministically. | **never fails** (traps on misuse) |
| `mcpy` | `(wild int8->:dst, wild int8->:src, int64:n) → wild int8->` | Copies `n` bytes from `src` to `dst`. **NO overlap allowed**. Maps to `llvm.memcpy`. | **never fails** (traps on misuse) |
| `mmov` | `(wild int8->:dst, wild int8->:src, int64:n) → wild int8->` | Copies `n` bytes from `src` to `dst`. **Overlap-SAFE**. Maps to `llvm.memmove`. | **never fails** (traps on misuse) |
| `memset` | `(wild int8->:dst, int64:val, int64:n) → wild int8->` | Fills `n` bytes at `dst` with the byte value `val` (low 8 bits). Maps to `llvm.memset`. | **never fails** (traps on misuse) |

### Arenas, wild tracking, and W^X memory

Type-directed arena constructors (D-152, D-154), the wild-allocation counters the
controlled `exit`/`failsafe` path uses (D-062, D-151), and the W^X executable
memory the JIT is built on (`wildx`, W^X invariant — a page is never writable and
executable at once).

| Built-in | Signature | Description | Fails |
|---|---|---|---|
| `arena_make` | `int64:cap → arena<T>` | A bump/slab arena for `T`, its element stride taken from the annotation the call is given (D-152) — type-directed, so the element type is never written as an argument. | **never fails** (traps on misuse) |
| `shared_arena_make` | `int64:cap → shared_arena<T>` | The atomically-shared arena (D-154), likewise type-directed. | **never fails** (traps on misuse) |
| `atomic_from_ptr` | `wild T-> → atomic<T>` | Aliases existing memory AS an atomic (D-033), rather than allocating storage (which `atomic<T>:x;` does) — the element type comes from the annotation. **`wild`-context only** (D-187): it fabricates an atomic view over an address the type system did not allocate, exactly as privileged as the `#wild_ptr` that produced the address. The result is a wild local used in place — never stored (a stored aliased atomic is meaningless: the storage is the pointee's, not the struct's). | **never fails** (traps on misuse) |
| `wild_live_count` | `() → int64` | How many `wild` allocations are live — what the controlled `exit` checks so a leak traps rather than passing silently (D-062). | **never fails** (traps on misuse) |
| `wild_release_all` | `() → NIL` | Releases every live `wild` allocation at once; the cleanup `failsafe` may run before exiting positive (D-151). | **never fails** (traps on misuse) |
| `wildx_alloc` | `int64:size → wildx int8->` | Allocates writable-not-executable pages for the JIT to fill. | **never fails** (traps on misuse) |
| `wildx_seal` | `wildx int8->:ptr → NIL` | Flips the pages to executable-not-writable — the one W^X transition, never the reverse. | **never fails** (traps on misuse) |
| `wildx_call` | `(wildx int8->:ptr, int64:arg) → int64` | Calls into sealed executable memory. | **never fails** (traps on misuse) |
| `wildx_free` | `wildx int8->:ptr → NIL` | Releases W^X pages. | **never fails** (traps on misuse) |

<!-- builtins:end -->

*(Note: **`malloc` and `free` are not builtins and are not aliases.** They are C
functions, and since D-149 they are not reachable at all — in-process FFI does
not exist, so there is no `extern "libc"` to declare them in. The natives above are
the WHOLE allocator API — five natives since `aalloc` joined at 0.10.0
(D-150). An earlier draft of this line called `free` and
`realloc` "legacy aliases", carried over from the prototype's C/C++ era and
never true of the native API — the prototype's own type checker knows exactly
`alloc`, `calloc`, `ralloc` and `dalloc`. See D-119.)*

---

## 2. String Functions — `nlibc`'s tier, not builtins

> Everything in this section arrives as ordinary Nitpick functions in `nlibc`,
> written over the floor (§2b). They were listed as builtins while the
> prototype's design was carried over; 0.8.4 measured the truth — the compiler
> calls none of them, nothing could lower them, and the port plan
> (`STDLIB_PROMOTION_AUDIT.md`) already routes them through `nlibc` as code.
> The three exceptions the compiler DOES evaluate are in §2c.

These are fast compiler intrinsics for interacting with the `string` type.

### Length & Inspection
*   `string_length(str)`: Byte length of the string.
*   `string_byte_length(str)`: Explicit byte-length alias for Unicode clarity.
*   `string_char_count(str)`: UTF-8 codepoint count.
*   `string_is_empty(str)`: True if length is 0.
*   `string_is_valid_utf8(str)`: True if the string is well-formed UTF-8.

### Comparison & Searching
*   `string_equals(a, b)`: Byte-equal comparison.
*   `string_contains(str, needle)`: True if `needle` is found in `str`.
*   `string_starts_with(str, prefix)`: True if `str` starts with `prefix`.
*   `string_ends_with(str, suffix)`: True if `str` ends with `suffix`.
*   `string_index_of(str, needle)`: First byte index of `needle` (-1 if not found).
*   `string_last_index_of(str, needle)`: Last byte index of `needle` (-1 if not found).

### Manipulation
*   `string_concat(a, b)`: Concatenate two strings.
*   `string_substring(str, start, end)`: Returns a byte-indexed substring `[start, end)`.
*   `string_count(str, needle)`: Count non-overlapping occurrences.
*   `string_replace(str, needle, replacement)`: Replace all non-overlapping occurrences.
*   `string_repeat(str, n)`: Repeat the string `n` times.

### Formatting & Padding
*   `string_trim(str)`: Trim ASCII whitespace from both ends.
*   `string_trim_start(str)` / `string_trim_end(str)`: Trim leading or trailing whitespace.
*   `string_to_upper(str)` / `string_to_lower(str)`: ASCII uppercase/lowercase conversion.
*   `string_pad_left(str, len, char)` / `string_pad_right(str, len, char)`: Pad to total length using `char`.

### Conversion
*   `string_from_int(val)` / `string_to_int(str)`: Base 10 integer parsing/formatting.
*   `string_from_int_hex(val)`: Formats an integer to a hex string (without prefix).
*   `string_from_char(byte)`: Converts a single byte to a string.
*   `string_format_float(val, precision)`: Formats a `flt64` to a string with exact precision.

---

<!-- builtins:begin -->

## 2b. The Runtime Floor (bootstrap tier)

The functions `bootstrap/runtime/npkrt.ll` defines and every backend rung can
call — the set the compiler itself is written against until `nlibc` (cycle 0.8)
grows the library tier above it. They are ordinary bare-name builtins: declared in
no module, resolved by the compiler, callable everywhere. Three copies of this
signature set exist by necessity — the runtime defines, the seed declares for
stage 1, `src/backend/ir/ir_runtime.npk` declares for stage 2 — and
`check_runtime_sigs_agree` diffs all three on every harness run.

| Built-in | Signature | Notes | Fails |
|---|---|---|---|
| `string_concat` | `(string, string) → Result<string>` | The one string operation the compiler is built out of (591 call sites); also comptime-folds. | **never fails** (audited at 1.1.1: the IR body only ever writes error 0 — OOM traps, D-150) |
| `int_to_string` | `int64 → Result<string>` | Decimal rendering. | **never fails** (audited at 1.1.1: one return, error always 0; OOM traps) |
| `string_slice` | `(string, int64:lo, int64:hi) → Result<string>` | Byte-indexed, half-open — **an OWNED COPY** (D-186): a view here made `x = string_slice(x, …)` a silent use-after-free. An empty slice allocates nothing. `string_bytes`/`string_from_bytes` are the explicit view primitives. | `Result` — may fail |
| `string_bytes` | `string → uint8[]` | **The string→slice bridge** (D-185, 1.1.12c): the bytes as a borrowed VIEW — same pointer, same length, no copy. The slice is a borrow (D-070), so everything that stops a borrow escaping stops this one. | **never fails** |
| `string_from_bytes` | `(wild int8->:ptr, int64:len) → string` | Wraps existing bytes; never fails. | **never fails** (traps on misuse) |
| `to_cstring` | `string → Result<cstring>` | NUL-terminated copy (D-049). | `Result` — may fail |
| `read_file` | `cstring → Result<string>` | Whole file. | `Result` — may fail |
| `read_stdin` | `() → Result<string>` | Whole stream. | `Result` — may fail |
| `path_exists` | `cstring → bool` | Never fails: absence is an answer, not an error. | **never fails** (traps on misuse) |
| `mono_now` | `() → int64` | `CLOCK_MONOTONIC` nanoseconds since an arbitrary epoch (D-176) — the deadline substrate's one clock. The impossible-failure branch traps (D-061). | **never fails** (traps on misuse) |
| `channel` | `() → Result<Channel<T, LEVEL, CAP>>` | **The channel constructor** (D-072/D-182): reads its element, lock level and capacity from the ANNOTATION, exactly as `arena_make()` reads its element — the parameters live in the type, so the call takes none. Allocates, so it returns a `Result`. | may fail (OOM) |
| `mutex` | `() → Result<Mutex<T, LEVEL>>` | **The mutex constructor** (D-056, 1.1.11): reads its element and lock level from the ANNOTATION, exactly as `channel()` does. The element moves in if it owns. Allocates the immortal cell, so it returns a `Result`. | may fail (OOM) |
| `rwlock` | `() → Result<RwLock<T, LEVEL>>` | **The reader-writer lock constructor** (D-056, 1.1.11b): as `mutex()`, one writer or many readers. | may fail (OOM) |
| `condvar` | `() → Result<CondVar<LEVEL>>` | **The condition-variable constructor** (D-056, 1.1.11b): no element — it pairs with a `Mutex` at each `timedwait`. | may fail (OOM) |
| `barrier` | `() → Result<Barrier<N, LEVEL>>` | **The barrier constructor** (D-056, 1.1.11b): N arrivals per generation, read from the annotation. | may fail (OOM) |
| `suspend_until` | `(int64) → NIL` | **The suspension primitive** (D-071, 1.1.8): parks the TASK until an absolute monotonic timepoint and lets the executor run something else. Lowers INLINE — the park request goes to the executor, the state word advances, and the machine returns SUSPENDED — so it is legal only inside an `async` function. Everything that blocks in the language is built from it, which is what makes "blocking is always task suspension" true rather than aspirational. | **never fails** |
| `suspend_io` | `(int32, int32, int64) → NIL` | **The I/O suspension primitive** (B-3a, 1.1.12a): registers `(fd, epoll-events)` one-shot in the executor's reactor and parks the task until readiness or the absolute deadline — the caller re-tries its syscall to learn which. Lowers INLINE like `suspend_until`; `io_ready` in the prelude is its `Result`-shaped face. | **never fails** |
| `io_unwatch` | `(int32) → NIL` | **Removes a descriptor from the executor's reactor** (B-3a, 1.1.12a): `io_ready` defers it so the registration lives exactly as long as the wait — a one-shot left armed past its frame would fire into freed memory. Removing an unwatched descriptor is a no-op, not an error. | **never fails** |
| `own_fd` | `fd → OwnedFd` | **Takes ownership of a descriptor** (D-185, 1.1.12b): from here the value's scope closes it — the generated drop is the close, which is what IO_REFERENCE §6's "there is no `close` in the surface" lowers to. Move-only like every owner. | **never fails** |
| `release_fd` | `move OwnedFd → fd` | **The inverse** (D-185): consumes the owner and returns the bare number, so a caller that must observe close's verdict can say `close(release_fd(move o))` — the move defuses the drop, so no double close is spellable. | **never fails** |
| `chain_depth` | `() → int32` | How many sites the in-flight error's origin chain has passed (D-179, 1.1.6) — the depth keeps counting past the eight the ring keeps. | **never fails** |
| `chain_site` | `(int32) → int32` | The i-th kept site id, oldest first; 0 outside the kept range (D-179). | **never fails** |
| `site_line` | `(int32) → int32` | The source line a site id names, from the per-program table; 0 for the runtime's reserved site 0 (D-179). | **never fails** |
| `site_path` | `(int32) → string` | The source path a site id names; empty for site 0 (D-179). | **never fails** |
| `write_file` | `(cstring, string) → Result<NIL>` | Whole buffer to a path, replacing what was there — `read_file`'s mirror (0.8.3). A short kernel write is retried; a failed kernel close is a failed write. | `Result` — may fail |
| `open` | `(cstring, int64:flags, int64:mode) → Result<fd>` | One openat at AT_FDCWD. Raw kernel flag numbers — the floor is the syscall surface (D-051); named modes live in the library tier. | `Result` — may fail |
| `close` | `fd → Result<NIL>` | A failed close is reported, never swallowed. | `Result` — may fail |
| `read` | `(fd, ptr, int64:cap) → Result<int64>` | ONE kernel read; bytes delivered. Zero asked is zero delivered; end-of-input is the error code E_EOF, never a zero in the value channel (D-075). | `Result` — may fail |
| `write` | `(fd, ptr, int64:len) → Result<int64>` | ONE kernel write; bytes taken, short counts included — the write-all loop is the library's. Replaced 0.8.3's write_raw (D-141). | `Result` — may fail |

Error slots across the floor carry the kernel's own negative codes, exactly as
the syscall returned them (ENOENT is −2). Conditions the floor detects itself
reuse that vocabulary — an interior NUL is −22, a slice out of range −34 — and
end-of-input is E_EOF = −4096, the first code past the kernel's error space
(errno stops at 4095), so no collision is possible. Runtime trap codes continue
that region: −4097 DIV_BY_ZERO, −4098 INT_MIN_OVERFLOW, −4099 OUT_OF_BOUNDS
(a slice or array index past the end, D-070), and −4100 TBB_ERR (an ERR value
at a bare comparison or a checked cast out of tbb, D-144) reach `failsafe`
through the runtime's trap route rather than through any Result. Positive codes
belong to programs. The full statements are D-141 and D-142.

## 2c. Comptime-foldable string builtins

The three string names the compiler EVALUATES during `comptime` folding
(`fold_string_builtin`, 0.6.4) — magic by definition, so they stay builtins:

| Built-in | Signature | Notes | Fails |
|---|---|---|---|
| `string_equals` | `(string, string) → bool` | Folds at comptime; `nlibc` provides the runtime body. | **never fails** (traps on misuse) |
| `string_byte_length` | `string → int64` | Folds at comptime. | **never fails** (traps on misuse) |
| `string_is_empty` | `string → bool` | Folds at comptime. | **never fails** (traps on misuse) |

(`string_concat` also folds, and is already floor.)

## 3. Syscalls

Direct access to operating system syscalls. 

| Built-in | Return | Description | Fails |
|---|---|---|---|
| `sys(CONST, ..*int64[])` | `Result<int64>` | The only syscall form. Reaches any OS syscall; returns `tbb32` error codes (negative for system errors). | `Result` — may fail |

> ### There is one syscall form, not three (D-048)
>
> The original three tiers were a danger ladder along two axes — how many
> syscalls are reachable, and whether the result is wrapped:
>
> | Tier | Reachable | Wrapped |
> |---|---|---|
> | `sys` | curated whitelist | **no** |
> | `sys!!` | everything | yes |
> | `sys!!!` | everything | **no** |
>
> The whitelist was small **because `sys` was unwrapped** — restricting the call
> set compensated for the missing error wrapping. D-001 removed `sys!!!` and made
> everything wrapped, so that justification disappeared, leaving two tiers
> separated only by an arbitrary list of which calls are common enough.
>
> Restricting which syscalls a binary may make is **`--seccomp`**'s job: a
> kernel-enforced allowlist that cannot be bypassed by choosing a different
> spelling in source. Argument-level constraints belong in the **typed API** —
> `io_set_nonblocking` and its siblings have no slot for an arbitrary `ioctl`
> request code, which is a stronger guarantee than a primitive that rejects one
> at runtime.
>
> `--extra-picky=no-sys` bans direct syscalls in high-level application code, the
> way `no-wild` bans manual memory.
>
> **`asm!!` is spelled `asm`** (D-046) — it is the only assembly form left.
> `!!` no longer exists in the language.
>
> **`sys!!!` is removed** (D-001). The raw tier returned a bare `int64`, bypassing
> `Result<T>` entirely, and permitted an arbitrary expression as the syscall
> number — an unchecked failure in the place it is most likely to be
> catastrophic. Both remaining tiers are `Result`-wrapped, so the rule "every
> function returns `Result<T>` except `main` and `failsafe`" has no exceptions.
> `raw` / `_!` remains the single explicit, greppable bypass.

---

<!-- builtins:end -->

## 4. Compiler Macros

`#` is the **compiler-directive sigil** — it marks something addressed to the
compiler rather than the runtime (D-020). Two syntactic positions, one meaning:

| Form | Purpose |
|---|---|
| `#name<T>(...)` | builtin producing a value |
| `#name(...)` | **macro invocation** (D-046) — replaces `name!(args)` |
| `#[name(...)]` | attribute annotating a declaration |

> **`@` is never a builtin prefix.** `@` is the address-of operator and nothing
> else. Forms such as `@sizeof`, `@typeof`, and `@derive` appear in older
> material and are **wrong** — `@cast<T>(x)` reads as "the address of `cast<T>`
> of x". Rewrite any such usage to the `#name<T>(...)` form.
>
> **Except casting**, which has no builtin form at all: `@cast<T>` and
> `@cast_unchecked<T>` become the operators **`=>`** and **`=>!`** (D-021), not
> `#cast<T>`. A cast is an operation on a value, not a directive to the compiler,
> so it does not belong under `#`.

<!-- builtins:begin -->
| Macro | Return | Description |
|---|---|---|
| `#size_of<T>` | `int64` | Returns the size in bytes of the type `T` at compile time. |
| `#wild_ptr<T>(addr)` | `wild T->` | Constructs a pointer from an integer address. **Legal only in `wild` context** — the single suspension of the general prohibition on integer→pointer casting (D-019). Exists because the allocator must turn an `mmap` result into a `wild int8->`. |
| `#wild_slice<T>(ptr, len)` | `T[]` | Constructs a slice from a raw pointer and an element count. **Legal only in `wild` context** (D-070). Deliberately parallel to `#wild_ptr` — an extent the compiler cannot verify is exactly as privileged as an address it cannot verify. This is how a slice is laid over memory the type system did not allocate — a `sys`-returned mapping, a `wild` region — now that nothing address-shaped crosses an `extern` boundary at all (D-149). |
| `#ptr_add<T>(ptr, offset)` | `wild T->` | Pointer arithmetic — the ONLY form (D-033). `offset` is in **ELEMENTS of T** (D-187): it lowers to `getelementptr T`, so `#ptr_add<int64>(p, 1)` advances eight bytes; byte arithmetic is `T = int8`. **Legal only in `wild` context** — pointer arithmetic is the manual regime's, like `#wild_ptr`. |
| `#unreachable()` | *(any)* | Marks a point the program can never reach — the arm of a `pick` a stricter analysis has already excluded (D-061, the replacement for the removed `(!)`). It **traps** through D-142's route (`UNREACHABLE`, −4102) if control ever reaches it, so a wrong exclusion is a controlled stop, not undefined behaviour. Takes no arguments; as an expression it has whatever type its position expects, because it produces no value. |
<!-- builtins:end -->

---

## 5. Inline Assembly

Nitpick supports direct inline assembly for `x86_64` and `aarch64` targets.

| Built-in | Return | Description |
|---|---|---|
| `asm<T>(arch, code, constraints, args)` | `Result<T>` | Executes assembly and wraps the output in `Result<T>`. Negative integer returns are implicitly treated as errors. |

> **`asm!!!` is removed** (D-001), for the same reason as `sys!!!` — raw inline
> assembly returning an unwrapped value is precisely where an unchecked failure
> is most dangerous. The surviving form is spelled **`asm`** (D-046).

**Example:**
```nitpick
// Executing x86_64 assembly, returning a Result<int32>
Result<int32>:val = asm<int32>(
    "x86_64", 
    "mov %1, %0\nadd $1, %0", 
    "=r,r", 
    input_var
);
```
