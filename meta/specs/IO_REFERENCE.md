# Nitpick Streams and I/O

Written rather than adopted — the carried-over spec set has no I/O chapter, and
`PRE_PLANNING_REVIEW.md` §4 records it as missing and needed by the driver and by
diagnostics.

Grounded on D-042 (`fd` is a distinct type), D-049 (`cstring`), D-050 (line
endings are a stream property), D-051 (`Path` above `nlibc`), D-054 (`Path`),
D-062 (lexical lifetime), D-063 (a trap is a whole-program event), D-070 (slices),
D-071 (blocking is task suspension), and D-074 … D-076.

> **`stream` was a reserved word that named nothing.** It sat in
> `LEXICAL_REFERENCE.md`'s `BuiltinType` list with no definition anywhere, and
> `TYPE_REFERENCE.md` skips from §23 to §25 where it presumably belonged. D-074
> returns it to userland along with `process`, `pipe`, `debug`, and `log`. None of
> what follows needs language syntax.

---

## 1. The model

**I/O is a trait.** `Stream` describes what every readable or writable thing can
do; files, pipes, sockets, and memory buffers are stdlib types implementing it.

```nitpick
trait:Reader = {
    async func:read  = int64(Self:self, uint8[]:dest, Duration:deadline);
};

trait:Writer = {
    async func:write = int64(Self:self, fixed uint8[]:src, Duration:deadline);
    async func:flush = NIL(Self:self, Duration:deadline);
};
```

Three properties, each load-bearing:

- **Every operation is `async`** (D-071). A read that parked the OS thread would
  stall every sibling task pinned to that executor — a driver reading source files
  while diagnostics stream out is exactly that shape. The executor's readiness
  mechanism is `io_uring` or `epoll` **through raw syscalls**, so no dependency
  question arises: `nlibc` *is* the syscall surface.
- **Every operation returns `Result<T>`** and carries a **deadline** (D-056).
  There is no unbounded read.
- **Buffers are slices** (D-070), so a read cannot be told a length that disagrees
  with the storage behind it.

### 1.1 Why a trait

The compiler's **diagnostics must be capturable**. Writing them through
`dyn Writer` means production writes to `stderr` and the test harness writes to a
memory buffer, through one code path. A concrete stream type would need either a
second mechanism for that or an internal tag selecting behaviour at runtime —
the mode-field pattern D-072 rejected for channels.

Object safety holds: every method takes `self`, none returns `Self`, none has
comptime type parameters (`TRAITS_REFERENCE.md` §4.2).

---

## 2. End-of-input is an error code, never a value

```nitpick
Result<int64>:n = await src.read(dest, deadline);
```

`read` returns the number of bytes placed in `dest`. **End of input is the error
code `E_EOF`.** No operation returns a sentinel, and no operation requires a
follow-up call to learn what its return value meant.

> `libn`'s buffered layer returns `FILE_EOF = -1` for **both** end-of-file and
> error, so a caller must consult `feof` / `ferror` to find out which happened —
> the C design, inherited whole.
>
> This is the same defect the project has now removed three times: `Result`
> encoded its error state twice (D-069), a channel `recv` returned `0` for a
> closed channel and a received zero alike (D-072), and here one sentinel covers
> EOF and error together.
>
> **One rule:** end-of-input is an error code, exactly as a closed channel is. It
> exists so the caller is forced to handle a condition it could not otherwise
> distinguish.

---

## 3. Text streams and byte streams

Line-ending policy belongs to the stream (D-050), and the two disciplines are
**different types** rather than a mode flag on one type.

| | `ByteReader` / `ByteWriter` | `TextReader` / `TextWriter` |
|---|---|---|
| Translation | **none, ever** | on |
| Read | bytes as they are | `\r\n`, `\n`, and lone `\r` all yield `\n` |
| Write | bytes as they are | `\n`, unless opened requesting otherwise |
| Unit | `uint8[]` | `string`, by line or by span |

There is no "text mode by accident": a byte stream never translates, and a text
stream is a different type you had to name.

**The line-ending choice is a creation parameter held in the writer**, not a
comptime parameter. Putting it in the type sounds tidier until every function
accepting a writer has to become generic over it for no benefit.

```nitpick
TextWriter:w = await TextWriter.create(sink, LineEnding.Lf)?;
TextWriter:w = await TextWriter.create(sink, LineEnding.CrLf)?;   // greppable opt-in
```

---

## 4. Buffering

Fixed, and **never inferred from whether the output is a terminal** (D-076).

| Stream | Buffering |
|---|---|
| `stdin` | fully buffered |
| `stdout` | line buffered, always |
| `stderr` | **unbuffered, always** |

C decides `stdout`'s buffering by calling `isatty`. Nitpick does not: a default
that varies by circumstance is a defect in the design, and its practical cost is
that output interleaves differently — or vanishes on a crash — depending on
whether the program ran in a terminal or through a pipe, so the configuration
that gets debugged is not the one that ships.

`io_isatty` remains available; it answers a question and changes nothing.

A program wanting throughput on `stdout` wraps it in its own `BufWriter`,
explicitly.

### 4.1 Buffered data is lost on a trap

`defer` does not run on a trap (D-014), and a trap is a whole-program event in
which no task resumes and no cleanup executes (D-063). **No flush is attempted**,
because flushing means running code against state the fault may have corrupted.

Stated rather than mitigated, because the mitigation belongs to the program:

- **`stderr` is unbuffered, so diagnostics written to it survive a trap.** That is
  why it is unbuffered — it is a safety property, not a performance judgement.
- The registry of open streams is reachable from `failsafe`, alongside the
  allocation registry D-014 already hands it. `failsafe` **may** flush; the
  runtime may not decide that for it.
- Anything whose loss is unacceptable goes to an unbuffered stream, on the same
  reasoning that puts actuator safing in `failsafe` rather than in `defer`.

---

## 5. Opening, and the path boundary

```nitpick
Path:p = Path.parse("/etc/hosts")?;
ByteReader:r = await ByteReader.open(p, deadline)?;
```

- **Opening takes a `Path`**, never a `string` (D-051, D-054). `Path` is
  absolute, lexically normalized, and contains no interior NUL.
- **`cstring` appears only at the syscall boundary** (D-049), where the conversion
  rejects interior NULs — the poison-NUL bypass.
- **An `fd` is always valid** (D-042). POSIX's `-1` goes to `Result.error` and is
  not representable, so there is no not-open state inside an open stream. `libn`'s
  `FILE.fd` is an `int64` with `-1` meaning not-open; that state is the absence of
  the stream, not a value of the field.

> ⚠️ **Lexical normalization is not kernel resolution** (D-054). A containment
> check on a normalized path does not survive symlinks; traversal that must stay
> inside a directory uses `openat` with `O_NOFOLLOW`.

---

## 6. Lifetime

**A stream's storage belongs to the scope that opened it** and is closed at scope
exit — the same lexical rule tasks (D-062), channel endpoints (D-072), and borrows
(D-004) follow.

Consequences:

- **There is no `close` in the surface.** Scope exit closes.
- **A stream cannot escape its scope**, so there is no question of which owner
  closes it, and no double-close.
- **A stream cannot be sent through a channel** — it is not an owning value that
  outlives its scope. Hand work to a task within the scope instead; D-062
  guarantees the task finishes first.

---

## 7. Seeking

```nitpick
await s.seek(Whence.Start, offset, deadline)   -> Result<int64>   // new position
```

`Whence` is an enum — `Start`, `Current`, `End` — not an `int64` constant.
Seeking a buffered stream discards the read buffer and flushes the write buffer
first; both are part of the operation rather than the caller's responsibility.

---

## 8. Standard streams

`stdin`, `stdout`, and `stderr` are `TextReader` / `TextWriter` over the three
inherited descriptors, with the buffering in §4.

They are **not** globals that any code may grab. They belong to `main`'s scope and
are passed down like any other stream, which is what makes a program testable:
the harness supplies memory streams and reads back what was written. This is the
same argument as §1.1 and the reason diagnostics take a `dyn Writer`.

---

## 9. What `nlibc` provides, and what sits above it

| Layer | Contents |
|---|---|
| **`nlibc`** | raw syscalls — `open`, `openat`, `read`, `write`, `lseek`, `close`, `fstat`, `pipe2`, `io_uring_*`. `cstring` at the boundary (D-049). No buffering, no line-ending policy, no `Path`. |
| **stdlib** | `Path` (D-051), `Reader` / `Writer`, the text and byte streams, buffering, the standard streams |

The split is the one D-051 drew: `nlibc` is the POSIX surface and `cstring` is
permanently the right type there; everything portable sits above it.

**`printf` and `scanf` are not in either layer** (D-053). Formatting is ordinary
functions returning `string`, spliced by `&{ }` interpolation; a writer takes the
resulting string. There is no format-specifier language, so there is nothing for a
stream to interpret.

---

## 10. Open items

- **Sockets.** The stream traits cover them, but addressing, connection setup, and
  the `Result` mapping for the socket-specific error space are not specified here.
  `ARCHIVE/nsocket` exists and has not been assessed.
- **`io_uring` versus `epoll`.** Both are raw syscalls and either satisfies the
  dependency rule. Which the executor uses — and whether it must support both for
  older kernels — is a runtime decision, not a language one, and needs measuring
  rather than deciding on paper.
- **The stream registry's exact shape**, as handed to `failsafe`. It parallels the
  allocation registry D-014 already specifies, and should share its structure
  rather than invent a second one.
