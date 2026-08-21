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
*Security constraint: Every allocation has a hidden 8-byte CRC32 header. Double-frees and corruption immediately trigger the failsafe.*

| Built-in | Signature | Description |
|---|---|---|
| `alloc` | `int64:size → wild int8->` | Allocates `size` uninitialized bytes. Triggers failsafe on OOM. |
| `calloc` | `(int64:count, int64:size) → wild int8->` | Allocates `count*size` zero-initialized bytes. Prefer this over `alloc` + `memset`. |
| `ralloc` | `(wild any->:ptr, int64:new_size) → wild int8->` | Resizes an allocation. The old pointer is invalid after calling. (Preferred over `realloc`). |
| `dalloc` | `wild any->:ptr → void` | Deallocates a pointer. Safe no-op if `NULL`. Triggers failsafe on double-free. (Preferred over `free`). |
| `mcpy` | `(wild int8->:dst, wild int8->:src, int64:n) → wild int8->` | Copies `n` bytes from `src` to `dst`. **NO overlap allowed**. Maps to `llvm.memcpy`. |
| `mmov` | `(wild int8->:dst, wild int8->:src, int64:n) → wild int8->` | Copies `n` bytes from `src` to `dst`. **Overlap-SAFE**. Maps to `llvm.memmove`. |
| `memset` | `(wild int8->:dst, int64:val, int64:n) → wild int8->` | Fills `n` bytes at `dst` with the byte value `val` (low 8 bits). Maps to `llvm.memset`. |

<!-- builtins:end -->

*(Note: **`malloc` and `free` are not builtins and are not aliases.** They are C
functions, reachable only by declaring them in an `extern` block like any other C
function. The four above are the native allocator; an earlier draft of this line
called `free` and `realloc` "legacy aliases", which was carried over from the
prototype's C/C++ era and was never true of the native API — the prototype's own
type checker knows exactly `alloc`, `calloc`, `ralloc` and `dalloc`, and every
`malloc` in the specification appears inside `extern "libc" { … }`. See D-119.)*

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

| Built-in | Signature | Notes |
|---|---|---|
| `string_concat` | `(string, string) → Result<string>` | The one string operation the compiler is built out of (591 call sites); also comptime-folds. |
| `int_to_string` | `int64 → Result<string>` | Decimal rendering. |
| `string_slice` | `(string, int64:lo, int64:hi) → Result<string>` | Byte-indexed, half-open. |
| `string_from_bytes` | `(wild int8->:ptr, int64:len) → string` | Wraps existing bytes; never fails. |
| `to_cstring` | `string → Result<cstring>` | NUL-terminated copy (D-049). |
| `read_file` | `cstring → Result<string>` | Whole file. |
| `read_stdin` | `() → Result<string>` | Whole stream. |
| `path_exists` | `cstring → bool` | Never fails: absence is an answer, not an error. |
| `write_file` | `(cstring, string) → Result<NIL>` | Whole buffer to a path, replacing what was there — `read_file`'s mirror (0.8.3). A short kernel write is retried; a failed kernel close is a failed write. |
| `open` | `(cstring, int64:flags, int64:mode) → Result<fd>` | One openat at AT_FDCWD. Raw kernel flag numbers — the floor is the syscall surface (D-051); named modes live in the library tier. |
| `close` | `fd → Result<NIL>` | A failed close is reported, never swallowed. |
| `read` | `(fd, ptr, int64:cap) → Result<int64>` | ONE kernel read; bytes delivered. Zero asked is zero delivered; end-of-input is the error code E_EOF, never a zero in the value channel (D-075). |
| `write` | `(fd, ptr, int64:len) → Result<int64>` | ONE kernel write; bytes taken, short counts included — the write-all loop is the library's. Replaced 0.8.3's write_raw (D-141). |
| `write_all` | `(fd, ptr, int64:len) → Result<NIL>` | The retry loop over the single write. In IR only because pointer stepping is a 0.9 rung; graduates to the library tier when it lands. |

Error slots across the floor carry the kernel's own negative codes, exactly as
the syscall returned them (ENOENT is −2). Conditions the floor detects itself
reuse that vocabulary — an interior NUL is −22, a slice out of range −34 — and
end-of-input is E_EOF = −4096, the first code past the kernel's error space
(errno stops at 4095), so no collision is possible. Runtime trap codes continue
that region: −4097 DIV_BY_ZERO and −4098 INT_MIN_OVERFLOW reach `failsafe`
through the runtime's trap route rather than through any Result. Positive codes
belong to programs. The full statements are D-141 and D-142.

## 2c. Comptime-foldable string builtins

The three string names the compiler EVALUATES during `comptime` folding
(`fold_string_builtin`, 0.6.4) — magic by definition, so they stay builtins:

| Built-in | Signature | Notes |
|---|---|---|
| `string_equals` | `(string, string) → bool` | Folds at comptime; `nlibc` provides the runtime body. |
| `string_byte_length` | `string → int64` | Folds at comptime. |
| `string_is_empty` | `string → bool` | Folds at comptime. |

(`string_concat` also folds, and is already floor.)

## 3. Syscalls

Direct access to operating system syscalls. 

| Built-in | Return | Description |
|---|---|---|
| `sys(CONST, ..*int64[])` | `Result<int64>` | The only syscall form. Reaches any OS syscall; returns `tbb32` error codes (negative for system errors). |

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
| `#wild_ptr<T>(addr)` | `wild T` | Constructs a pointer from an integer address. **Legal only in `wild` context** — the single suspension of the general prohibition on integer→pointer casting (D-019). Exists because the allocator must turn an `mmap` result into a `wild int8->`. |
| `#wild_slice<T>(ptr, len)` | `T[]` | Constructs a slice from a raw pointer and an element count. **Legal only in `wild` context** (D-070). Deliberately parallel to `#wild_ptr` — an extent the compiler cannot verify is exactly as privileged as an address it cannot verify. This is how a slice is obtained across the FFI boundary, since slices are not C-compatible and do not cross an `extern` signature. |
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
