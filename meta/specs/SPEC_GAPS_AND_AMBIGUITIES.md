# Specification Gaps & Ambiguities Report

> # ⚠️ SUPERSEDED — HISTORICAL RECORD ONLY
>
> **Do not treat any resolution in this document as authoritative.**
> It was written for the `nitpick-next` experiment, and several of its
> `[RESOLVED]` blocks are **wrong**. `DECISIONS.md` replaces it in full.
>
> Retained because the *questions* it raises are good ones and the record of what
> was believed at the time is useful. The *answers* are not.
>
> | § | Its answer | Actual decision |
> |---|---|---|
> | 1 | "Nitpick **uses a Garbage Collector**" (hybrid generational) | **D-003** — no collector. Static ownership + arenas with `Handle<T>`. `gc` removed entirely. |
> | 1 | "dangling pointers are impossible because the GC retains all reachable objects" | **D-004** — borrows are second-class; they pass down the call stack and never up. |
> | 1 | "`wild` … is 'pinned' so the GC doesn't move it" | **D-020** — pinning is obsolete; nothing relocates memory implicitly. |
> | 2 | "`Result<T>` error field is a `tbb` type large enough" | **D-005** — confirmed `tbb32`, with `is_error` strictly `bool`. |
> | 3 | "Nitpick does **NOT** support object-oriented method calls… `.` is strictly for struct field access" | **D-006** — **wrong**. UFCS was never removed. Arenas and `atomic<T>` both use method syntax throughout the specs. |
> | 3 | "`dim` (e.g. `dim256<Joules>`)" for dimensional analysis | Retained — but `TYPE_REFERENCE` §2.3.3 and `FORMAL_DRAFT` 2.3.3 still say `fix256`. Rename still owed. |
> | 4 | Division by zero unaddressed; arithmetic uses sticky ERR | **D-007** — type-directed: `tbb` degrades, plain types trap. **D-008** specifies the ERR encoding. |
> | 5 | Module resolution "deferred to Cycle 0.4.0" | **Resolved** — `FORMAL_DRAFT` ch. 14 was adopted as `MODULE_REFERENCE.md`. |
>
> The §3 UFCS error is the instructive one: it was asserted confidently while
> contradicted by arena and atomic code sitting in the same repository. Treat
> every remaining claim here with matching suspicion.

**Target:** `nitpick-next` specifications
**Goal:** Identify all ambiguities, missing details, and contradictions that would block an implementation team from building the compiler without external clarification.

Before we can modularize the implementation plan, the following core architectural questions must be resolved. 

---

## 1. The Memory Management Model (CRITICAL)
With the removal of the borrow checker, the exact mechanism for memory lifecycle management is highly ambiguous across the specs.

*   **What frees memory?** `BUILTIN_REFERENCE.md` mentions `wild` pointers are "untracked by the garbage collector," implying a GC exists. However, `next.md` (from the bootstrap roadmap) mentions implementing a `nodrop` attribute to "suppress auto-cleanup," which strongly implies deterministic RAII/Scope-based destruction (like C++). 
    *   *Question:* Are we using a Tracing GC, ARC (Automatic Reference Counting), or Scope-based RAII? 
*   **Dangling Pointers:** If we use RAII/Scope-based destruction but have no borrow checker, what stops a developer from taking the address of a local variable (`@val`), passing that pointer (`int32->`) out of scope, and causing a use-after-free?
*   **String Allocation:** `string:c = a + b;` allocates a new buffer. Who is responsible for calling `dalloc` on this buffer?
*   **`wild` vs `wildx`:** The safety architecture mentions both `wild` and `wildx` as manual memory types, but `wildx` is never formally defined in the type reference. What is the distinction?

> **[RESOLVED]** 
> - **Memory Model:** Nitpick **uses a Garbage Collector**. Specifically, it uses a **hybrid generational GC** (Nursery / Old Generation) with safepoints, shadow stack root scanning, and a card table for write barriers. The borrow checker was removed precisely because memory safety is managed by the GC by default.
> - **Dangling Pointers:** In standard (safe) code, dangling pointers are impossible because the GC retains all reachable objects. 
> - **String Allocation:** Standard strings are allocated on the GC heap and automatically cleaned up when unreachable.
> - **`wild` vs `wildx`:** `wild` and `wildx` are explicit safety opt-outs. When you declare `wild`, you manually manage memory (which is "pinned" so the GC doesn't move it during FFI interop). `wildx` is specifically for executable memory (e.g., JIT compilation) and carries security/injection risks. `nodrop()` prevents RAII bindings on specific low-level constructs like scoped `wildx` allocs.

## 2. Error Handling & `Result<T>`
*   **Error Code Size:** `TYPE_REFERENCE.md` defines the `Result<T>` struct as using a `tbb8` for the `error` field. A `tbb8` only holds values up to 127. This is insufficient for OS syscalls (POSIX errnos go above 133), HTTP status codes (e.g., 404, 500), or complex application error spaces. Should this be a `tbb32`?
*   **Safe Casts vs Failsafe:** `OP_REFERENCE.md` states the safe cast operator `=>` "Triggers failsafe if data is lost." Triggering the global trap (Layer 3) for a cast seems like a violation of the `Result<T>` paradigm. Shouldn't `=>` return a `Result<T>` so the user can handle data loss gracefully via `?` or `?!`?

> **[RESOLVED]**
> - **Error Code Size:** The `Result<T>` error field is a `tbb` type large enough for system error spaces (e.g., `tbb32`). The `is_error` sentinel is strictly a `bool` type (not `i8`) to enforce pedantic type-safety (no arithmetic on booleans).
> - **Safe Casts:** The normal safe cast operator `=>` does NOT trap at runtime; instead, it causes a **compile-time error** if there is a risk of data loss. The developer must explicitly use the unchecked cast `=>!` to opt-out of the safety check and intentionally accept the data loss.

## 3. Type System Contradictions
*   **`fix` vs `tfp` Naming:** In `TYPE_REFERENCE.md`, Section 5 introduces Twisted Fixed-Point types (`tfp32`, `tfp64`, etc.). However, the immediate subsection on Dimensional Analysis uses the syntax `fix256<Joules>`. Should `fix` be renamed to `tfp` for consistency?
*   **Methods vs Functions (UFCS):** `BUILTIN_REFERENCE.md` defines string operations as standard functions (e.g., `string_trim(s)`), while `OP_REFERENCE.md` mentions `.` is used for "unified member access (automatically dereferences)". Does Nitpick support object-oriented method calls like `s.trim()`, or is it strictly `string_trim(s)` / pipeline `s |> string_trim()`?
*   **Generics Syntax:** `OP_REFERENCE.md` mentions the turbofish `::<T>` for generic function calls, but there is no specification for how generic types or functions are *declared*.

> **[RESOLVED]**
> - **Naming:** `tfp` is the standard fixed-width type without dimensional analysis. The specific type that supports dimensional analysis is `dim` (e.g., `dim256<Joules>`).
> - **UFCS:** Nitpick does NOT support object-oriented method calls (no `s.trim()`). All operations are standard function calls `string_trim(s)` or `Vec.len(v)`. The `.` operator is strictly for struct field access.
> - **Generics Syntax:** Declarations use the standard bracket syntax next to the name: `struct:Name<T> = { ... }` or `func:my_func<T> = ...`. (The turbofish `::<T>` is used for invocations).
>
> **Extended by D-064.** This resolved declaration syntax only. The turbofish is
> now the *only* expression-position form rather than one of two, generic bodies
> are checked at their definition against their bounds rather than per
> instantiation, `comptime` value parameters are added, and monomorphization is
> bounded at depth 64 with reversible mangling. Note the UFCS resolution directly
> above is **wrong** — D-006 retains UFCS and method-call syntax; that line is an
> error introduced when this document was authored.

## 4. Control Flow & Syntax
*   **Exhaustiveness in `pick`:** Does the compiler enforce that a `pick` (match) block covers all possible values, requiring a `(*)` default case if not?
*   **Pointer Operators:** `OP_REFERENCE.md` lists `->` as "Pointer To / Member Access" (e.g., `p->field`), but immediately below it lists `.` as "Unified member access" that "automatically dereferences if pointer". If `.` handles pointer dereferencing automatically, should `->` be restricted *only* to type declarations (`int32->`) to avoid syntax redundancy?
*   **Power Operator (`**`):** This is listed for arithmetic. Given that power operations on integers can overflow instantly, does this operator automatically promote to a larger type, or does it return a `Result<T>`?

> **[RESOLVED]**
> - **Exhaustiveness:** Yes, `pick` must be exhaustive.
> - **Pointer Operators:** The `->` syntax is strictly for **type declarations** (e.g., `int32->:ptr`). It is NOT used for member access. The `.` operator handles all struct field accesses and automatically dereferences if the variable is a pointer.
> - **Power Operator & Math:** Arithmetic operators (like `**`, `+`, `*`) do NOT return `Result<T>` to avoid making expressions unreadable. Instead, the safe types (like `tbb`) enter a "sticky ERR" state upon overflow. If a developer uses normal types (like `int32`), they accept the risk of standard overflow/underflow.

## 5. Modules & Visibility (Missing Spec)
*   The specs reference modules (e.g., "import via the `collections` module") and visibility modifiers (`pub func:main`), but there is no document explaining how the module system, namespace resolution, or file-to-module mapping works. This is a critical gap for the compiler's frontend parser.

> **[RESOLVED]**
> - **Module Resolution:** Modules are used for namespaces. Only declarations marked as `pub` are exported from a module. The exact import syntax (`use path/to/mod.*` vs `use path/to/mod.somefunc`) will be formally implemented and mapped to the filesystem during the Module Loader cycle (Cycle 0.4.0).
