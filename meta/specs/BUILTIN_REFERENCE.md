# Nitpick Built-in Reference

Nitpick provides a set of compiler intrinsics (built-ins) that are available globally without needing to `use` any external modules. These built-ins map directly to LLVM instructions or safe runtime shims.

> **Note on Data Structures:** Built-in collections (like stacks, lists, and hash tables) from early language prototypes have been moved to the Standard Library. You must explicitly import them via the `collections` module.

---

## 1. Memory Management Built-ins (NitpickAlloc)

These intrinsics directly interface with the `NitpickAlloc` slab/VM allocator. They all return `wild` pointers (untracked by the garbage collector).
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

*(Note: `free` and `realloc` are preserved as legacy aliases for `dalloc` and `ralloc` respectively, but `dalloc`/`ralloc` are the preferred Nitpick vernacular).*

---

## 2. String Built-ins

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

## 3. Syscalls

Direct access to operating system syscalls. 

| Built-in | Return | Description |
|---|---|---|
| `sys(CONST, args...)` | `Result<int64>` | Safe-tier syscall. Restricted to curated whitelist. Returns `tbb32` error codes (negative for system errors). |
| `sys!!(CONST, args...)` | `Result<int64>` | Full-tier syscall. Allows any OS syscall. Returns `tbb32` error codes. |
| `sys!!!(expr, args...)` | `int64` | Raw-tier syscall. Allows arbitrary expression evaluation as the syscall number and returns a bare integer (bypasses `Result` wrapping). |

---

## 4. Compiler Macros

| Macro | Return | Description |
|---|---|---|
| `#size_of<T>` | `int64` | Returns the size in bytes of the type `T` at compile time. |

---

## 5. Inline Assembly

Nitpick supports direct inline assembly for `x86_64` and `aarch64` targets.

| Built-in | Return | Description |
|---|---|---|
| `asm!!<T>(arch, code, constraints, args)` | `Result<T>` | Executes assembly and wraps the output in `Result<T>`. Negative integer returns are implicitly treated as errors. |
| `asm!!!<T>(arch, code, constraints, args)`| `T` | Executes assembly and returns the bare type `T` without wrapping or checking. |

**Example:**
```nitpick
// Executing x86_64 assembly, returning a Result<int32>
Result<int32>:val = asm!!<int32>(
    "x86_64", 
    "mov %1, %0\nadd $1, %0", 
    "=r,r", 
    input_var
);
```
