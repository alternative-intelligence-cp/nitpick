# Nitpick Memory System Specification

Nitpick’s memory model enforces strict lifecycle constraints but offers flexible paradigms—from scope-managed RAII allocations to absolute raw manual management (`wild`) tailored for systems programming and JIT engines.

**There is no garbage collector** (D-003). Lifetimes are determined statically: by
scope for managed and `stack` bindings, by generation-counted `Handle<T>` for
arena contents, and manually for `wild`/`wildx`. Nothing relocates memory
implicitly, and there are no collection pauses.

## 1. Allocation Contexts & Modifiers

Variables in Nitpick exist in one of several allocation states. You can use contextual keywords (like `stack`, `wild`) immediately before the type declaration to explicitly control their residency.

> **`NULL` is a `wild` pointer's value (D-355, 1.6.1e landing 119).** A plain `T->`
> always points at something: `NULL` is legal in a `wild` pointer context alone --
> a `wild`/`wildx` binding, parameter, field or return, a store into or a comparison
> against a wild place, the builtins' `wild any->` parameters -- and `NITPICK-TYPE-096`
> anywhere else a pointer or an `Optional` is expected. A pointer that may be absent
> is the Optional `T->?`, tested with `== NIL` and read with `?? @d` (TYPE_REFERENCE
> §11.1, §27).

### 1.1 Default Managed Memory (Implicit RAII/Scope-based)

> **In a generic body a bare `T` is treated as owning (D-264, 1.5.2f):** the
> body is checked once for every instantiation, so a copy of a `T` place is
> refused (TYPE-046) unless spelled `move(...)` or `.clone()`; a lending
> `pick` binds a `T` payload as a VIEW in place (D-266, 1.5.2h) and a consuming
> one moves it. The `move` of a scalar is its copy; the rule costs nothing
> where nothing owns.
If no keyword is provided, the allocation is tracked and managed. The compiler drops the binding when its scope exits — after the scope's joins and `defer`s, before its channel reclaims (D-183, D-207) — and at no earlier point; a value's last textual use does not shorten its life. *(The sentence here said "or at its NLL last-use point" until 1.5.1b; the compiler never did that.)*
```nitpick
int32:x = 42i32;           // Automatically managed on stack
```

### 1.1a Temporaries (D-246, 1.5.1b step 4)

An owning value that no place takes — a call's result passed straight to another
call, a literal built and bound to nothing, the operand of a comparison — is a
**temporary**, and it is dropped **when its statement ends**. The rule has one
shape everywhere: the value is registered where it is produced, taken by the
place that keeps it (a binding's initialiser, a `move` parameter, a struct or
array literal's slot, a `pass`), and dropped flag-guarded at the statement's end
if nothing took it. The same drop runs on every path out of the statement — a
`fail`, a `relay`, a `?!` trap, a `break` — and a temporary that feeds a loop's
or an `if`'s condition dies with the condition. Under `await` the temporaries
of the awaiting statement live in the frame, since the statement spans a
suspension. `tests/backend/programs/temp_*.npk` and the `cost` stage's
`temporaries` probe (the nested form held to 4× the bound form's peak,
measured ×3.0) pin it.

### 1.1b `List<T>` (D-247, 1.5.1b step 5)

> **Its buffer is managed storage (D-263, 1.5.2e).** `list_init` and
> `list_reserve` allocate through `alloc_managed`, the heap's UNTRACKED entry
> (the same one a `dyn` cell and a channel's ring use), and `ralloc` keeps a
> block's role, so a grown list stays managed and the generated drop frees it
> with `dalloc`. Until 1.5.2e the buffer was `alloc`'s, the tracked entry, and a
> `List` alive in `main` at `exit 0` — where `exit` runs joins and defers and no
> drops — was the one managed value D-151's exit check counted, exit 94
> `WildLeak`. `alloc_managed` is the PRELUDE's own (TYPE-054 elsewhere): a
> hand-written `wild` container relies on D-151's count as its enforcement of an
> unpaired free, and the count stays for every `wild` block.

`List<T>` — `{ items: wild T->; count; cap }`, the compiler's own growable
collection — is **compiler-known and owning**: its generated drop releases the
`count` elements through `T`'s drop where `T` owns and hands the block back, a
vacant List (`cap == 0`, D-225) owns nothing, and the type is move-only under
TYPE-046/047 like every owning type — pass it as a plain argument, consume it
with a `move T:p` parameter, never copy it binding to binding. It is declared
in the PRELUDE with its functions (`list_init`, `list_push`, `list_reserve`)
since 1.5.1b step 5b, through the bridging refresh the seed README describes;
until then it lived in `src/frontend/list.npk`.

**Its fields are the prelude's own** (D-313, D-314; 1.5.8b step 1b):
`{ hidden wild T->:items; sealed int64:count; sealed int64:cap; }`. Outside the
prelude, `items` is not touched (TYPE-080) and `count` and `cap` are read but
not written (TYPE-079). Until then any program could scribble on them:
- `a.cap = 100` let pushes run past the block into another list (DEF-73);
- `a.items[i] = …` wrote past `count` unchecked (DEF-74).

A list is used through two mechanisms:
- **`l[i]` is the element**, bounds-checked against `count` with the slice's
  guard (`OutOfBounds`). It is a place: read it, write it, compound it, claim it
  (`$$i`, `$$m`), take its address, or `move` out of it. **Assigning over an
  owning element drops the old value**, as a managed array's element does; the
  element is live or vacant after a move (S-26), never unowned.
  - `l[lo...hi]` is a checked `T[]` view of elements `lo…hi−1`. It borrows the
    list (D-249).
  - A list behind a pointer is `(<-p)[i]`. `p[i]` on a pointer to an array, a
    slice or a `List` is refused (TYPE-082), because it reads as pointer
    arithmetic.
- **The checked operations** change a list:
  - `list_push`, `list_reserve`;
  - `list_pop` (the last element, moved out);
  - `list_get(l, i)` (the element BY VALUE, for a `T: Copy` — D-327, 1.6.1c; `i` in `[0, count)`);
  - `list_truncate(l, n)` (drops `n…count−1`);
  - `list_clear`;
  - `list_insert(l, i, v)` (`i` in `[0, count]`);
  - `list_remove(l, i)` (order kept);
  - `list_swap_remove(l, i)` (the last element moved into the hole).

  An index outside the list is `OutOfBounds`, whatever spells it. An element
  that leaves the list is moved to the caller, or into a temporary its statement
  drops (D-246).

### 1.1c What a `pass` or a `move` transfers (D-183 as corrected at 1.5.1b step 5)

`pass` moves the returned value out implicitly, and `move(place)` explicitly;
in both the ROOT binding of the place stops owning what it held (D-065's
whole-binding rule: `pass h.name` invalidates `h`). **A value whose type does
not drop transfers nothing**: `pass h.n` over an `int64` field, or `move(h.n)`,
copies the number out and leaves every owning sibling where it was, so `h`'s
drop still runs at scope exit. Until 1.5.1b the emitter cleared the root's
drop flag for the copyable case too, and an owning local returned by one of
its copyable fields leaked its owning fields on every call (DEF-8;
`pass_field.npk`). **One exception (D-251, 1.5.2)**: a `move` or `pass` out
of an OWNING field or element of a `limit<Rules>` binding refuses
(NITPICK-TYPE-063) — the vacant value it would leave (D-254) is a write no
rule can be asked to admit; move the whole binding, or copy the part.

**A moved-out whole binding is VACANT, not stale (DEF-120, 1.6.1d step 3).** The
emitter used to clear the binding's drop flag and leave the old header in the
slot; since a store through a held pointer drops what it finds (below), the slot
holds the type's canonical vacant value after `move(x)` or `pass x`, exactly as
a moved-out field or element does (D-254): `p = @x; string:t = move(x); (<-p) =
v;` frees nothing of `t`'s.

**And the value stored back is dropped (DEF-159, 1.6.1e landing 105).** In that
shape the move had also cleared `x`'s drop flag, so the scope exit skipped the
drop and the `v` written through `p` leaked (192,094 bytes live after two
thousand rounds of three kinds against 190 for one). The flag is the scope-exit
drop's FAST PATH and nothing more now: for a binding whose address the function
has taken (`@x`, `$$i x`/`$$m x`, a pointer-receiver call on it) the drop runs
over the slot unconditionally -- a vacant slot's drop is a no-op, a re-filled
one's is the drop the value was owed -- and only a binding nobody addressed
keeps the one-byte test.

**What reaches a move slot (DEF-366, DEF-367, DEF-368; 1.6.1e landing 170).** An
owning value enters a slot that takes ownership -- an initialiser, an
assignment, `pass`, `give`, a `move` argument, a `move` RECEIVER, a struct or
array literal's element -- only FRESH (a call's or a literal's whole result,
`move(...)`) or by `move(...)` at its place. A copy of a place through an
operator is refused as the place itself is: `is (c) : a : b`, `a ?? d`, `r ?!
E`, `r ?| d`, `relay r`, `a => T` and `a => dyn T` hand back a place's own
value, so write `is (c) : move(a) : move(b)` or `move(o) ?? d`. A part read out
of a value no binding holds is refused too -- `(f()).s` is a part of a statement
temporary that drops whole at the statement's end -- so bind the value first
(`T:t = f(); x = move(t.s);`); a part read through `?.` is unwrapped and bound
first. A method whose receiver is `move Self:self` takes its receiver as a
`move` argument: `(move(x)).m()`, and through a pointer `(move(<-p)).m()`, which
leaves the pointee VACANT (above). A copy of PLAIN DATA through any of these is
harmless and stays legal.

**`pass v` and `return Result{value: v}` are ONE road (DEF-371; 1.6.1e landing 171).** A
`return`'s literal hands its value out of the frame exactly as `pass` hands its operand:
the same transfer (the place vacated, above), the same refusals -- a borrowed value, a
binding the frame was lent or a pointee's owning part (TYPE-047), an owning part of a
`limit<Rules>` binding (TYPE-063), a `fixed` binding (TYPE-084), a `sealed` field outside
its module (TYPE-079) -- and the same mismatch report. Until landing 171 the long form
COPIED a named owner into the `Result`, and the frame's scope exit dropped it again (a
double free, 95/95 at both legs). A `Result` literal anywhere else -- an initialiser, an
argument, a field -- is a move slot like any other: a place there is TYPE-046. A literal
writes ONE field, `value` or `err` (NITPICK-TYPE-101, DEF-373; TYPE_REFERENCE.md §11.2).

**A function with a `move` parameter is not a VALUE (DEF-372; landing 171).** A function
type carries no `move` -- `func int32(string)` is the type of `takes = int32(move
string:s)` too --, so a call through the value handed the argument to a parameter that
drops it while the caller's binding still owned it (95/95). Naming such a function
anywhere but as the callee of its own call -- an initialiser, an argument, a struct
field, `pass takes;`, a module member `m.takes` -- is `NITPICK-TYPE-100` at the name,
for EVERY `move` parameter, plain data included (a known over-refusal: no type test, and
relaxing it later only accepts more programs). Call the function by name, or wrap it in
a function whose parameter is not `move` and name that; a function type that carries
`move` is a grammar question, tied to DEF-286.

### 1.1d The four store shapes, and what each drops (D-186, DEF-120)

Assigning over an owning value releases the old one first, after the right-hand
side is computed. The four shapes:

| Store | The old value | Since |
|---|---|---|
| `x = v` (a whole binding) | dropped, guarded by the binding's flag | D-183 |
| `s.f = v` (an owning field) | dropped unconditionally — a live struct's fields are live or vacant | D-186 |
| `a[i] = v`, `l[i] = v` (a managed array's or `List`'s element) | dropped unconditionally | D-186, D-247 |
| `(<-p) = v` (through a pointer) | dropped when `p` names MANAGED storage; left alone when `p` is `wild` | DEF-120 |

"`p` is wild" is the escape analysis's own wild-provenance reading (D-223,
`src/frontend/analysis/wild_places.npk` — one predicate for the analysis and the
emitter): a `wild`-qualified binding, parameter or field, an element of one, an
`=>! wild T->` cast, `#ptr_add`/`#wild_slice`, an allocator's or a
wild-returning function's result. Manual storage holds no value until the
author writes one, and the closed direction for a wrong free is no free. A plain
`T->` names a live managed value by the language's contract — a binding's, a
field's, an element's, a vacant one after a move — and a `wild` block cast to a
plain pointer by `=>! T->` is the author's assertion of exactly that: spell the
cast `=>! wild T->` for storage that holds no value yet.

> *(2026-10-16, D-361, 1.6.1e landing 121.)* The crossing is CHECKED now: a value of
> wild provenance (the list above) written into a plain pointer binding, parameter,
> field or return without the `=>! T->` is `NITPICK-TYPE-097` at the value -- the
> assertion is made, or the slot is declared `wild`. A dereference in place, a `wild`
> slot and an `=>! wild` cast take the value as it is (TYPE_REFERENCE §27).

### 1.2 `stack`
Forces explicit allocation onto the hardware call stack. Extremely fast (just a pointer bump), with memory reclaimed exactly at the scope's exit.
```nitpick
stack int32:counter = 0i32;
```

### 1.3 `wild` (Unmanaged / Manual Memory)
Declares the memory as completely unmanaged. `wild` pointers **must** be manually freed. They explicitly bypass RAII tracking. If you fail to free a `wild` pointer, the compiler will abort with a memory leak error on `exit`. A `wild` value stays in the manual regime until the author crosses it out by `v =>! T->` (D-361, 1.6.1e landing 121): into a plain pointer binding, parameter, field or return it is `NITPICK-TYPE-097` otherwise.
```nitpick
wild int8->:buffer = alloc(1024i64);
```

### 1.4 `wildx` (Executable Memory)
Reserved for JIT compilers. Provides executable memory adhering to W⊕X (Write XOR Execute) security protocols with ASLR and guard pages.
```nitpick
wildx uint8->:code = wildx_alloc(4096i64);
```

### 1.5 Constructing a pointer from an address

Casting an integer to a pointer is illegal in ordinary code. The prohibition is
suspended by exactly one construct, legal only in `wild` context (D-019):

```nitpick
wild int8->:page = #wild_ptr<int8->>(addr);
```

This exists because the allocator itself must turn an `mmap` result into a
`wild int8->`. Outside that layer it should essentially never appear.

## 2. Managing `wild` Memory

Because `wild` memory escapes automatic lifecycle tracking, it introduces the risk of leaks and use-after-free conditions. Nitpick uses static analysis to prevent leaks at compile time.

### 2.1 The `defer` Block
The canonical method for cleaning up `wild` memory is using a `defer` block immediately following the allocation. This guarantees the free runs on every normal exit path (early `return`, `pass`, `fail`, `relay`, or `exit`).

> **`defer` does not run on a trap.** `!!!`, `?!` and `??!` transfer control directly to
> `failsafe` without unwinding (D-014). At trap time the state of the system is
> unknown, so no cleanup code runs before the handler that understands the
> situation gets control. `failsafe` receives the allocation registry intact and
> performs whatever cleanup is appropriate.
>
> **[1.5.6 step 2, D-292.]** `failsafe` allocates from a preallocated REGION,
> never from the heap: once the trap route names a holder, every allocation
> bumps from one mebibyte of `.bss` (16-aligned, zero, never freed), frees are
> no-ops, a reallocation copies into the region, and exhaustion is `HeapOom`
> inside `failsafe` — the re-entry rule's exit 70. The heap's mutex may be
> held forever by a thread the stop parked inside the allocator (D-291), and a
> `failsafe` that waited for it would hang with no deadline. The mebibyte is
> the bound a `failsafe` body lives within (TCB.md §5): growing a string by
> repeated concatenation is quadratic in a region that frees nothing, so a
> diagnostic is built in few, large steps.

```nitpick
wild int8->:buf = alloc(16i64);
defer { dalloc(buf); }
```

### 2.2 `nodrop`
The `nodrop` keyword acts as a per-binding RAII opt-out. It prevents an outer initializer from being hooked into the auto-drop tracker. Useful if you want to allocate something normally tracked but explicitly assume manual `wild` lifecycle control over it.
```nitpick
wild int8->:manual_buf = nodrop alloc(16i64);
```

### 2.3 Transferring ownership — `move`

`move(place)` transfers ownership out of a binding and invalidates the source
(D-065). It is what makes single-owner discipline checkable on `wild` memory, and
so what prevents double-free:

```nitpick
wild int8->:buffer = malloc(100i64);
wild int8->:moved  = move(buffer);

free(buffer);   // NITPICK-019 — use after move, and separately
                // "cannot free moved variable"
```

- **Ownership moves only where `move` is written.** Passing an owning value to a
  function borrows it (D-004); there are no implicit moves, because an implicit
  one would change ownership with nothing visible at the point it happens.
- **A moved-from binding is invalid, not "valid but unspecified."** Any read is
  an error. It may be **reinitialized by assignment**, after which it is live
  again — ordinary definite-assignment analysis. A `fixed` binding cannot be,
  since it cannot be assigned at all.
- `move` is **not** a memory qualifier, despite older grammar listing it as one.
  It is a keyword operator with a parenthesized operand, the same shape as
  `comptime(expr)`.
- **A claim is exclusive or shared, and `@` claims nothing** (D-286, 1.5.5).
  `$$m place` excludes every other access of an overlapping place while it
  lives, `$$i place` admits readers and excludes writers, and `@place` is a
  plain address with no claim. A claim's life is lexical (the call it is an
  argument of, or the scope of the pointer local holding it); a static
  overlap is refused (`NITPICK-BORROW-013`), a computed-index overlap is
  guarded at run time (`BorrowOverlap`) and proven away by the verified build.
  **A view's rights are its TYPE's** (D-348 (ii), D-350, D-351; 1.6.1e landing 103):
  `fixed T[]` is the read-only view, produced by a `fixed` binding of slice type,
  by ranging `fixed` storage, by reading a slice off a `fixed` struct and by
  `string_bytes`; a plain `T[]` converts to it and it converts back to nothing,
  so no road -- an argument, a field, a return, a `List` element, a trait method's
  parameter -- lets a write reach bytes a `fixed` promised (TYPE_REFERENCE §9.2.1,
  §26). **A view's root is frozen while the view is live** (D-325, 1.6.1 step 0): a
  view of a binding — a view-maker's result (`string_bytes`, `string_from_bytes`),
  a range view (`l[lo...hi]`), the result of a call whose own body views what
  it was handed (`@d`, or a `string` passed by value), a view moved through a
  call or pushed into a `List` — is a shared party on its root from the holder's
  declaration to the end of its block, or for its call when it is a call's
  argument, and every write-capable access of what it views refuses
  (`NITPICK-BORROW-015`): an assignment into it, `@root`/`$$m root` handed to a
  callee that writes through it, a pointer-receiver call, a `move` out of it,
  `list_push` on the viewed list. A pointer LOCAL's VALUE handed to a callee is the same access of what it points at as `@place` (DEF-275, landing 140): the callee's written paths are judged against the views rooted at the pointer's name, so `Holder->:b = @hd; … string_bytes(b.items[0i64]); … clobber(b)` refuses as `clobber(@hd)` does; a callee that writes another field, or reads, stands. Reads, `$$i`, a disjoint field, a disjoint
  numeral range, a callee that writes nothing through what it is handed, and a
  pointer root's own reassignment stay. A struct field viewing another field of
  its own struct freezes that field; a view of the very storage a store
  overwrites refuses at the store (`NITPICK-BORROW-002`). End the view's block
  before writing its root, or make the view a copy (`.clone()`,
  `string_concat("", …)`).
  **A spawn lends its sanctioned crossing until the block's join** (DEF-148,
  1.6.1e step 1): `drop f(@s)` with `s` a `shared_arena<T>` or one of the four
  locks (D-180's five kinds) is a lent party on `s` from the spawn to the end of
  the enclosing block, whose exit joins the task (D-207), and every
  write-capable access of `s` before the join refuses (`NITPICK-BORROW-016`):
  `destroy()`, an assignment over it, a `move` out, `$$m`, `@s` to a callee
  whose summary stores over the pointee, and the spawned task's own body if it
  stores over what it was lent. The kind's own concurrent operations (`alloc`/
  `get`; `acquire`/`read`/`write`/`timedwait`/`signal`/`broadcast`/`arrive`),
  `$$i`, a second spawn and a helper that only allocates stay. Destroy it after
  the block, or join first.
  A by-value parameter's frame storage — its address, an inline array's range —
  cannot travel up (`NITPICK-BORROW-001`, DEF-109, 1.6.1 step 0: it is the callee's
  own slot), while a view of the heap bytes a `string`, `List` or slice parameter
  references may, and the caller's freeze then holds the argument; a temporary
  handed to a callee that views what it is given is a view of a temporary
  (`NITPICK-BORROW-012`).
  **What a call stores is read off the callee's own body** (D-325's summaries):
  a callee the analysis can see connects only what it stores — a direct call, a
  method, a trait's method as the UNION of every impl's body (and its default's),
  which is what a `dyn` may hold or a bound may name — while a callee it cannot
  see (a function value) is read by D-223's shape rule, every destination that
  could hold the borrow. **What a call's RESULT holds is read the same way** (D-326,
  1.6.1b): a result holds a borrow of an argument exactly when the callee's summary
  says it may view or carry what that argument handed it — a callee that BUILDS its
  result (a fresh string, a copy of a view, a method's rendering) hands back nothing of
  the frame, and the caller may return it — and by the shape rule alone for a callee
  the analysis cannot see (a `dyn` method, a function value); a bare builtin's result
  is the reference's `Views` column's (D-249). A `dyn` holder and a bare type parameter are read in the
  closed direction (DEF-113, 1.6.1 step 0): either may hold any pointer. A
  BY-VALUE parameter that a call stores a borrow of this frame's storage into —
  a `move`, a `move dyn`, a copyable struct — HOLDS it from that call (DEF-114)
  and cannot travel up; a LENT `dyn`'s cell is the caller's (the one owning-by-cell
  kind a loan may write, through its methods), so a borrow stored into it refuses
  at the call (`NITPICK-BORROW-002`, DEF-115).
  **A holder this frame owns is bound by its block** (D-004 rule 3 as DEF-42 reads it, 1.5.5
  step 0; asked of a callee's store since DEF-252, 1.6.1e landing 116): a borrow of a local
  declared inside a block, stored into a binding declared outside that block -- by an
  assignment, a struct literal, a copied holder, a call's result, or a CALLEE whose matrix
  stores its argument into the binding's pointee (`reset(@c, string_bytes(t))`; through an
  explicit `<-` too, `(<-a)[0i64] = Cursor{ src: src, … }` -- DEF-272, landing 138) -- refuses
  at the store or at the call (`NITPICK-BORROW-002`): the holder outlives the storage the
  view names (a block-scoped `string`'s heap body is freed at the block's end; the callee's
  form compiled and read the freed body until landing 116). A by-value parameter holder is
  bound by the body's block (it dies with the frame, as the body's own locals do); a
  destination reached through a POINTER -- `@(<-p)`, `@lp.f`, `@((<-lp)[i])`, `@s.next.f`
  (DEF-272) -- refuses as the direct `(<-p) = …` does, since a holder's block bounds the
  pointer and not what it points at (a pointer PARAMETER's pointee outlives the frame and has
  its own sentence). Declare the holder inside the block, store a copy, or pass the borrow down.
  **A moved-out OWNING value carries what its place holds, not its root's identity**
  (DEF-125, 1.6.1d step 4): `move(self.v)` of a `string` — a `cstring`, a `buffer`, an
  `OwnedFd`, a struct or an enum of such, a `List` of such: a type holding no FOREIGN
  pointer (no raw pointer, slice, `dyn` or `any` inside) — carries the refs recorded at
  `self.v` (a view stored there earlier in the body) and nothing of `self`, because the
  moved bytes are the heap body the value owns and moving it out transfers that body;
  the return seam's implicit move (`pass self.v`) reads the same. So a swap of two owned
  strings through a lent `dyn`'s method is accepted, and a field that holds a view of
  frame storage, moved out and returned, is still `NITPICK-BORROW-001`; a struct with a
  raw pointer field keeps the conservative reading.
  **An enum's payload carries an address as a struct's field does** (DEF-160, 1.6.1d
  step 4): the layout records the pointer-bearing bit of an enum from its payloads, the
  escape analysis reads it, and the constructor `E.Some(@local)` is a call whose
  arguments' refs are the value's — returned from its frame, held in a struct that is, or
  stored through a call into an outliving pointee, it refuses (`NITPICK-BORROW-001`,
  `NITPICK-BORROW-002`) exactly as the struct literal `S{ p: @local }` always has.
  A plain by-value parameter of an OWNING type is a loan and is read-only (D-004,
  D-266; `NITPICK-TYPE-085`, 1.6.0 step 3g): no assignment to it or into it, no
  `@`/`$$i`/`$$m`, no pointer-receiver call, no stateful operation — a callee that
  changes an owning value takes it as `move T:p`, one that reads the whole of it
  takes `.clone()`; a copyable parameter is a copy and keeps every write.
  A `fixed` binding has no address (D-287, `NITPICK-TYPE-071`), and one that owns
  cannot be moved out of — `move(...)` or the implicit move at `pass`, of the
  binding or any part of it — since the vacant value a move leaves behind is a
  write to immutable storage (`NITPICK-TYPE-084`, 1.6.0 step 3f; `.clone()` is
  the reading), and no PART of one is written after its declaration — an element,
  a plain field, a compound assignment, a stateful operation on a part, a `fixed`
  local's element, anything under a `fixed` field (`NITPICK-TYPE-086`, 1.6.0
  step 4b; the whole binding's second assignment and a `fixed` field's own stay
  `NITPICK-ASSIGN-002`): the pointer
  type carries no mutability, so an address of an immutable would be a write
  path no rule sees.

## 3. Allocation Built-ins (NitpickAlloc)

> **The allocation CEILING (D-308 §7; landed 1.5.8b step 6c).** A request above
> 2^47 bytes (140,737,488,355,328 — the x86-64 user address space, so no block
> can be larger) is a bad request at every entry of the allocator: `alloc`,
> `alloc_managed`, `calloc`, `ralloc`, `aalloc` and the floor's own internal
> entry all trap `HeapBadRequest`, by ONE unsigned compare in each of the
> allocator's three entries (`npk_alloc_impl`, the failsafe region's
> `npk_fs_alloc`, and `npk_aalloc`, whose over-aligned path reaches the large
> allocator without passing the core) — a compare that reads a negative size as
> a huge one, so it is the sign check as well. Exactly the ceiling is legal: a
> request the kernel then refuses is `HeapOom`, not a bad request. The promise
> `n <= 2^47` is the floor spec's on `npk_alloc_internal`, consumed as a fact by
> every translated caller's rows and accepted at TCB.md §5 (10); it is what
> bounds every built-in length (TYPE_REFERENCE §9.2.1).

Nitpick provides raw, slab-backed compiler intrinsics for dynamic sizing. All return `wild int8->` and must be handled appropriately.
*   **`alloc(size)`**: Allocate `size` uninitialized bytes.
*   **`calloc(count, size)`**: Allocate zero-initialized memory.
*   **`ralloc(ptr, new_size)`**: Resize the allocation. Old pointer becomes invalid.
*   **`dalloc(ptr)`**: Explicitly deallocate.

There are **no aliases**: `malloc`/`free`/`realloc` are C names, and since
D-149 there is no in-process C to reach them through. An earlier revision of
this list called `realloc` and `free` "retained legacy aliases" — that was
struck by D-119 in `BUILTIN_REFERENCE.md` and is corrected here too. The fifth
native, **`aalloc(size, align)`**, serves alignments above the default sixteen
(0.10.0).

Every heap allocation carries a hidden 16-byte header — **the block's size and
a secret-keyed, address-keyed magic word** — and the allocator detects
double-free and header corruption and routes them to `failsafe`
(`-4102`), out-of-memory to `failsafe` (`-4103`), and a malformed request
(negative size, `calloc` count×size overflow — the multiply is CHECKED —
`ralloc(p, 0)`, a non-power-of-two alignment) to `failsafe` (`-4104`).
*Not a CRC*: an earlier revision claimed "a hidden 8-byte CRC32 header", which
was wrong three ways (a CRC32 is 4 bytes; no allocator in the project's
history uses one; a CRC over payload is an O(size) cost per operation nobody
signed up for). The real scheme is canaries: in a slab the next block's header
is its neighbour's overrun canary and a tail guard closes the chunk; large
blocks carry their own footer guard; freed slots keep a distinct FREED magic
that is re-verified when the slot is handed out again. Double-free of a
**tracked** binding is additionally a compile-time error (D-119); the runtime
check covers pointers the static analysis cannot follow. Allocator control
state (bitmaps, the chunk and large-block tables) lives out of band where no
payload overrun can reach it, and `dalloc`/`ralloc` prove a pointer lies
inside allocator-owned memory before dereferencing anything — a garbage
pointer is a trap, never a wild load. The heap is single-threaded at this
rung; the lock discipline lands with 1.1's executor work.

### The `<wild-live>` registry and the exit-time check (0.10.1, D-151)

> **What the check counts, and what it does not (D-263, 1.5.2e).** Every block
> from `alloc`, `aalloc`, `calloc` and a `ralloc` of one — the `wild` regime,
> whose unpaired free is the author's — and never managed storage: a string's
> body, a `dyn` cell, a channel's ring, and since 1.5.2e a `List<T>`'s buffer
> (`alloc_managed`, the prelude's own entry). `exit` runs joins and defers and
> no drops (D-183's amendment), so an owning local of `main` is never dropped
> by a program that exits; managed storage is the kernel's at exit, and the
> check says nothing about it.

The allocator's own tables ARE the live-set — no second bookkeeping
structure exists to drift from the first. A **wild** allocation is what the
`alloc`/`calloc`/`ralloc`/`aalloc` builtins hand out, stamped with a
wild-role magic; **runtime-internal storage** (string bodies, argv, file
buffers) is managed-regime, stamped with the internal role, and is NOT in
`<wild-live>` — its RAII arrives with the managed lowering, and until then
it is reclaimed wholesale by `wild_release_all()` or process exit.

- **`wild_live_count()`** walks the chunk bitmaps (slots below each
  watermark whose free bit is clear, counting wild-role headers) and the
  large table. Allocation-free, preallocated state only: safe from
  `failsafe` in a degraded process.
- **The exit-time check**: a SUCCESSFUL exit (code 0 — CONTROL_REFERENCE
  §4.6's "successful" scoping) with a non-empty set routes to `failsafe`
  with `-4105`. A failure exit keeps its code — hijacking an error report
  with a leak trap would destroy the error, and error paths carry no
  cleanup obligation (the defer-does-not-run-on-trap reasoning, D-014).
  `failsafe` may call **`wild_release_all()`** — followed by `exit` and by
  nothing else (TYPE-062, 1.5.1b step 5: a `main` that released and then
  RETURNED ran its scope-exit drops over unmapped memory) — drops every chunk and
  large mapping, both regimes, leaving the allocator usable — and exit
  positive; its own exit passes because the in-failsafe flag is set. The
  same flag makes a trap RAISED INSIDE failsafe exit 70 directly instead
  of recursing.

**One registry mechanism, three clients** (the audit's unification ask):
the sorted fixed-stride table — mmap-grown, preallocated initial capacity,
binary-searched, allocation-free to read, `failsafe`-walkable — is the
mechanism, and the allocation tables (8-byte chunk entries, 32-byte large
entries) are its first client. The **stream registry** (IO_REFERENCE §10)
and the **driver registry** (D-149's Bridge) are specified as the next two
clients of the same shape, not re-inventions: fixed-stride entries, sorted
by key, walked by `failsafe` in registration order at shutdown.

## 4. `Handle<T>` and Arena Allocators

Arenas with `Handle<T>` are the primary mechanism for **graph-shaped and cyclic
data**, as well as for performance-critical structures (physics nodes, ECS). With
no collector, they are how Nitpick handles reference cycles: allocate the graph
in an arena and drop the arena wholesale, so individual nodes are never freed and
cycles among them are irrelevant.

When an object is allocated in an arena, the system issues a `Handle<T>` containing a generation counter. If the memory slot is reallocated or freed, the generation counter increments. Attempting to use the old handle immediately fails safely by returning an `ERR` through the `Result<T>` system, completely eliminating silent use-after-free catastrophes.

Handles are **indices, not pointers**, which is what makes them safe across arena
growth and what lets them express cyclic references without a collector.

### 4.1 `Handle<T>` Memory Layout
The `Handle<T>` arena pointer lowers into a 16-byte aligned struct (`%Handle = type { i64, i32 }` in LLVM IR):
- **Bytes [0-7]**: `uint64:index` (Slot Index in the Arena)
- **Bytes [8-11]**: `uint32:generation` (Generation Counter for stale detection)
- **Bytes [12-15]**: Padding (for 16-byte alignment)

### 4.2 The operation set, and chained member access

Creation is the **`arena_make(cap)`** builtin, type-directed by the
annotation the way an unsuffixed literal is (D-092). An earlier revision
wrote `arena<int64>.alloc(1000)` — one name for two different operations
(creating the arena on the type, allocating a slot on the value), which the
blueprint philosophy refuses; D-152 split them.

```nitpick
arena<int64>:my_arena = arena_make(1000i64);
Handle<int64>:h = my_arena.alloc();
drop my_arena.put(h, 41i64);             // write through the handle
int64:val = my_arena.get(h) ? 0i64;      // read a COPY, with a default
drop my_arena.free(h);
my_arena.destroy();
```

The set is `alloc() -> Handle<T>`, `get(h) -> Result<T>`,
`put(h, v) -> Result<NIL>`, `free(h) -> Result<NIL>`, `reset() -> NIL`,
`destroy() -> NIL` (D-017 as amended by D-152). `get` returns the element
**by value**: a borrow-returning `get` would be a returned borrow, which
D-004 refuses everywhere — mutation is spelled `put`. A stale handle fails
`get`/`put`/`free` with **`-4106` in `Result.err`**, never a trap.
`destroy` CONSUMES the arena (a compile-time move, like `dalloc`), and an
un-destroyed arena is a wild-role leak the exit-time check names (D-151). Its
receiver is storage the function OWNS — a binding, or a field or element of
one reached through no pointer — never a pointer's pointee or a temporary
(`NITPICK-TYPE-091`, DEF-157, 1.6.1e step 1: through a pointer there is no
flag to clear, and the owner freed the arena again); a destroyed FIELD is
vacated as a moved-out field is (D-254), so the aggregate stays live and drops
its siblings (DEF-158).

> Note the operator: `?` takes a **fallback value**; `?!` takes a **failsafe error
> code** and traps (D-009). An earlier revision of this section wrote
> `my_arena.get(h) ?! 0i64`, which would call `failsafe(0)` on a stale handle
> rather than yielding `0`.

`.` handles all member access and auto-dereferences pointers (D-006). Chained
access through arenas embedded in structs works as ordinary member-place
addressing:

```nitpick
struct:App = { arena<int64>:my_arena; };
App:app = App{ my_arena: arena_make(16i64) };
Handle<int64>:h = app.my_arena.alloc();
```

### 4.2b The executor frame allocator is NOT `arena<T>` (D-153)

D-034's "each thread's executor owns an arena from which it allocates task
frames" names the arena *philosophy* — batch lifetime, drop-cheap — not the
surface type. The surface `arena<T>` is a **fixed-slot** allocator handing
out generation-checked **indices**; a coroutine frame is a **per-function,
variably-sized** block that `@llvm.coro.begin` needs as a **raw pointer**.
Conflating the two is the mistake the concurrency audit caught
(total_audit B-1), and this section exists so nobody repeats it.

The executor frame allocator (0.10.3) is runtime-internal — no keyword, no
builtin; its only caller is 1.1's coroutine lowering (C-7) — with a fixed
five-call interface: `npk_frame_exec_new` / `npk_frame_alloc(size, align)`
/ `npk_frame_free` / `npk_frame_drain` / `npk_frame_exec_destroy`. Tasks
are pinned (D-032), so the whole path is single-threaded and zero-atomic —
that is D-034's rationale for pinning. Chunked bump allocation; completed
frames return to a free list bucketed by **exact size**, which fits
coroutines precisely (one frame size per async function, recurring);
oversize frames take dedicated heap blocks; `drain` retires every frame at
once while keeping the chunks. The executor and its chunks are wild-role
heap blocks: an un-destroyed executor is a countable leak (D-151). Frame
alignment is capped at 16, the heap's own.

### 4.3 `shared_arena<T>` — arenas across threads

`arena<T>` is **single-threaded**. Sharing one across threads is unsound: growth
may relocate slots, so a pointer obtained from `get()` can dangle when another
thread's `alloc()` triggers reallocation — a hazard no amount of atomic
generation-counting fixes.

For concurrent use, `shared_arena<T>` is a distinct type with a deliberately
smaller contract (D-017):

| | `arena<T>` | `shared_arena<T>` |
|---|---|---|
| Threading | single-threaded | multi-threaded |
| Operations | `alloc`, `get`, `free`, `reset`, `destroy` | **`alloc`, `get`, `destroy` only** |
| Per-slot `free` | yes | **no** |
| Storage | may reallocate on growth | **chunked, never moves** |
| Cost | zero | one atomic bump per allocation |

Dropping per-slot `free` is what makes concurrency safe without epochs, hazard
pointers, or reference counting: slots are never reused while the arena is live,
so generations never increment during concurrent access and there is no freelist
to contend on. Allocation is a single atomic bump into non-moving chunked
storage.

`destroy` requires that no thread still holds handles — this is ownership, not
synchronization: the owner destroys the arena after joining.

**As built (0.10.4, D-154):** creation is `shared_arena_make(cap)`,
type-directed like `arena_make`; the surface value is ONE POINTER to the
runtime structure — shareable by reference is the contract, so the value is
the reference. **`alloc(v)` carries the value**, because there is no `put`:
the slot is written once, before its handle escapes, and is immutable after —
which is what makes concurrent `get` race-free with no per-slot machinery
(handle transfer between threads is the synchronizing edge; 1.1's channels
are SeqCst, D-016). `get` COPIES, exactly as `arena<T>`'s does and for the
same reason (a borrow return would violate D-004; D-152's argument applies
unchanged — the "likely yes" this section once carried is decided NO). Growth
RESERVES a capacity range with one atomic `fetch_add` — racing installers get
disjoint ranges and cannot collide — then publishes the chunk with a CAS
push; chunk sizes are geometric (each new chunk carries the arena's current
capacity in slots, capped at 65536), so a large arena is a few chunks, never
thousands. Shared handles carry generation ZERO constantly (nothing frees,
nothing increments), and `arena<T>` issues generations starting at 2 — so a
single-threaded arena's handle wandering into a shared `get` is refused as
stale (`-4106`), not read. `destroy` consumes the binding at compile time
(`MOVE-002`), and an un-destroyed shared arena is a wild-role leak the exit
check names (D-151).

## 5. The stacks (D-305; landed 1.5.8, 2026-09-19)

**Every stack a program runs on is the floor's, and its budget is the
program's, not the shell's.** `ulimit -s` and `RLIMIT_STACK` do not size any
of them. Before 1.5.8 the main thread ran on the stack the kernel handed it,
and an overflow was SIGSEGV with no `failsafe` (DEF-59: the compiler compiling
itself died under `ulimit -s 2048`). Each stack is ONE anonymous mapping,
lowest address first:

| region | size | main thread | spawned thread | `failsafe` |
|---|---|---|---|---|
| guard (`PROT_NONE`) | 4 KiB | yes | yes | yes |
| signal stack | 64 KiB | yes | yes | — |
| guard (`PROT_NONE`) | 4 KiB | yes | yes | — |
| reserve (below the limit word) | 64 KiB | yes | yes | yes |
| usable | — | 8 MiB | 2 MiB | 1 MiB |

**The check is in every function the compiler emits.** Each `define` carries
LLVM's split-stack prologue, which compares the stack pointer less the
function's exact frame against the thread's LIMIT WORD (`%fs:0x70`, in the
thread's TLS block) before the frame exists. Crossing it enters the trap route
as `StackExhausted` (4118), which every `failsafe` names because every program
can reach it. A frame larger than a page is refused at the prologue, where it
used to step over a thread's one guard page into whatever lay below (DEF-60). The
floor's own functions carry no prologue. They fit in the reserve, and a belt
proves it (VERIFICATION_REFERENCE §9.5).

**`failsafe` runs on a stack of its own**, mapped at startup, because the trap
route allocates nothing (D-153's rule). So a `failsafe` entered by
`StackExhausted` has its whole 1 MiB, and an overflow inside `failsafe` meets
the re-entry rule and exits 70. **Signals run on the thread's signal stack**
(SA_ONSTACK): the stop signal (D-291), the four machine faults and SIGPIPE
(D-307). So no signal frame lands in the reserve, and a fault caused by the
stack itself still has a stack to be handled on.

**A spawned thread's stack is released at its join** (1.5.8 step 1b, DEF-65):
the joiner unmaps it once the kernel has cleared the thread's tid word. Before
that, every joined thread kept its mapping and every page it touched until the
process ended. **A thread's TLS block and executor come from per-slot pools**
(step 3b, DEF-66): sixty-four entries each, one per thread-registry slot,
reborn for each thread in the slot and never freed. That is what lets a stale
stop walk or a stale waker read memory that exists, and a program spawn and
join threads without limit. The 65th LIVE thread is refused at its start, as
the registry always did (D-055's posture).

**A `stack` binding (§1.2) lives in the frame of the function that declares
it**, so it counts against that function's frame, and so against the check
above. A large `stack` array is a large frame, and a large frame is refused at
its prologue rather than jumping a guard.
