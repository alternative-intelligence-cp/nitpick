# Nitpick-Next Module & FFI Reference

This document establishes the canonical module system, visibility rules, and Foreign Function Interface (FFI) for the `nitpick-next` compiler.

## 1. Modules (`mod`)

Modules allow the organization of code into hierarchical namespaces.

### 1.1 Defining Modules
Modules can be defined inline or exist in external files.

*   **Inline Modules**:
    ```nitpick
    mod:network = {
        pub func:connect = int32() { pass 0i32; };
        func:internal = int32() { pass 1i32; }; // Private
    };
    ```

*   **External File Modules**:
    ```nitpick
    mod:network;
    ```
    The compiler searches for `network.npk` or `network/mod.npk` relative to the declaring file.

*   **Nested Modules**: Modules can be arbitrarily nested (e.g., `mod:core = { mod:math = { … }; };`).
*   **Visibility**: Modules are private by default. Use `pub mod` to expose them to outer scopes.

## 2. Imports (`use`)

The `use` keyword brings symbols from other modules into the current scope.

### 2.1 File-Based Imports
The canonical syntax imports directly from an `.npk` file path:

*   **Wildcard** (All `pub` symbols): `use "path/module.npk".*;`
*   **Single-Name**: `use "path/module.npk".square;`
*   **Selective**: `use "path/module.npk".{square, pi};`
*   **Namespace (Alias)**: `use "path/module.npk" as math;`

### 2.2 Logical Path Imports
```nitpick
use std.math.*;
use std.collections.{HashMap, HashSet};
```

### 2.3 Search Paths & Transitivity
*   **Transitivity**: `use` imports are strictly **not transitive**. Symbols imported into a module are not automatically re-exported. You must explicitly wrap or use `pub use` to expose them.
*   **Search paths.** This heading previously promised search paths and defined
    only transitivity. A `use` path resolves in exactly one way, decided by its
    first character:

    | Form | Resolves against |
    |---|---|
    | `use "./util.npk"`, `use "../x/y.npk"` | the **importing file's** directory |
    | `use "nfs/path.npk"` | the **dependency roots** |
    | `use std.math.*` | the standard library |

    A dependency named `nfs` declared at `../nfs` roots at **`../nfs/src/`**, so
    `use "nfs/path.npk"` is `../nfs/src/path.npk`.

    **An ambiguous path is an error, not a first match.** If two dependencies both
    supply `x/y.npk` the build fails and names both, because resolution order must
    never be something a reader has to know the manifest's declaration order to
    predict. See `BUILD_REFERENCE.md` §3.

### 2.4 Cycles

**A `use` cycle among modules is legal** (D-086). Two modules may import each
other, and so may any longer ring.

`use` names a namespace and imports **no initialisation order**. That is the
whole reason it is safe. A cyclic import is a hazard in languages where importing
a module *runs* it, because a cycle then has to pick a first module and some names
are observably unbound while it runs. Nitpick has no module-level execution: a
module is a set of declarations, globals are compile-time-initialised, and there
is nothing to sequence.

The language itself forces the cases. `pick` is both a statement and an
expression (D-059), so the statement and expression parsers must reach the same
arm-parsing code; a cast holds a type while an array size holds an expression, so
the type and expression parsers each need the other. No ordering of files removes
these, and no third file factors them out that is not the union of the two.

The loader must therefore:

1. **Collect every declaration in every module in the graph before resolving any
   body** — the same two-phase load that already lets a function refer to one
   declared below it in the same file.
2. **Report a cycle only when it is genuinely unresolvable** — a struct whose size
   depends on itself other than through a pointer, or a `const` whose initialiser
   depends on itself. The diagnostic names the members in the order they refer to
   each other; "circular import" is not something a reader can act on.
3. **Never make resolution order-dependent.** The same module graph must produce
   the same program regardless of which member the loader entered first. This is
   a reproducibility requirement before it is a convenience: a build that depends
   on entry order differs between the stage-1 and stage-2 compilers, and the
   bootstrap fixpoint check would then fail for a reason unrelated to
   correctness (D-078, D-085).

Legal is not encouraged. A cycle that exists because two modules each grew a
function belonging in the other is still a decomposition mistake; this says only
that the language does not forbid it, so the fix is moving the function rather
than inventing a file to satisfy a rule.

## 3. Visibility (`pub`)

Nitpick uses a strict binary visibility model: **Public** or **Private**.

*   **Private (Default)**: Symbols are accessible only within the same module/file. Intra-module access to private symbols is always permitted.
*   **Public (`pub`)**: Prefix declarations with `pub` to export them.
    ```nitpick
    pub func:compute = int32() { ... };
    pub struct:Point = { ... };
    pub const int32:MAX = 100i32;
    pub mod:utils = { ... };
    ```

## 4. Functions (`func:`)

### 4.1 Strict Syntax
To simplify the parser and enforce readability, `nitpick-next` strictly requires the canonical syntax. Legacy C-style/Rust-style `func name() -> type` is banned.

```nitpick
func:add = int32(int32:a, int32:b) {
    pass (a + b);
};
```
*   **Return Type**: The type declared (e.g., `int32`) is the *success* type. The compiler automatically wraps this in a `Result<int32>` at the semantic level.

## 5. Driver Interfaces (`extern`)

**In-process FFI does not exist in Nitpick (D-149).** All foreign code runs
in a separate, supervised **driver process** (D-055's architecture, detailed
in `meta/roadmap/audit-0.8-close/driver_architecture_plan_v3.md`), and an
`extern` block declares the INTERFACE to one. The word means what it says:
outside the process. The FFI barrier and the process boundary are the same
line — past it, a segfault, a hang, or a scribbled heap in the foreign code
arrives in the Nitpick process as **a value** (a closed socket, a reaped
child, an errored `Result<T>`), never as an uninterceptable fault.

### 5.1 Syntax

```nitpick
extern:"cuda_driver" = {
    opaque struct:KernelHandle;
    func:load_kernel = KernelHandle(int8[]:image);
    func:dispatch    = NIL(KernelHandle:k, int8[]:args);
};
```

The string names the driver; the functions are its methods. The compiler
lowers each method to a **Bridge stub** — marshal into the sealed
shared-memory ring, dispatch with a mandatory deadline, unmarshal or return
the error (lowering lands at 1.1). An `opaque struct` declared here is a
**typed wire handle**: an opaque value with a generation counter, minted by
the driver, type-safe on this side, dead after a driver restart (D-066 as
narrowed by D-149).

### 5.2 `Result<T>` and the wire's failure convention

All driver methods return `Result<T>` like every other function. There are
**no per-method error contracts**: the wire has a universal failure
convention — every dispatch returns status plus payload — so timeouts,
driver death, and protocol violations arrive as uniform negative codes in
the D-141 error space. (D-002's `fails on` / `with errno` / `never fails`
contracts existed because in-process C had no such convention; they are not
written anymore. The grammar remains parsed and is refused by the checker
with D-149 named.)

### 5.3 The wire vocabulary is closed

Fixed-width scalars, POD structs of them, sized byte payloads, and typed
handles. Payloads are **copied out of shared memory before validation** —
the shared region is an I/O device whose every byte is untrusted input, not
memory. Nothing address-shaped crosses in either direction: no pointers, no
slices-as-views, no `void*` (which is now valid nowhere in the language).

### 5.4 What makes a driver valid

Two layers. At **connect**: a handshake carrying magic, protocol version,
and an **interface hash derived from the `extern` block's signatures** — a
driver built against a stale interface is refused before any call. In
**Nitpick**: the generated stub implements the `Driver` trait with D-055's
obligations (deadline on every dispatch, no partial results, supervised
child, `failsafe`-reachable registry). The driver side is built against the
**C SDK header** — the wire protocol and ring layout as a C header plus a
reference event loop; the contract is the protocol, not a language binding,
so SDKs in other languages are alternative implementations of the same
wire.

```nitpick
int32:n = raw some_query(name); 
// OR
int32:x = _! some_query(name);
```

### 5.3 ABI Pointers
*   **Strings**: Do not pass native `string` types directly to C functions expecting `char*` — a `string` is `{ptr, len, cap}` and is **not** NUL-terminated. Use **`cstring`** (D-049): a string literal converts at compile time, and `to_cstring(s)` converts a runtime `string`, failing if it contains an interior NUL. Do not use `char8[]` for this — an ordinary char array carries no termination guarantee.
*   **Pointers**:
    *   `int32->`: Scalar pointer (`int32_t*`).
    *   `MyStruct->`: Struct pointer (`struct MyStruct*`).
    *   `any->`: Erased/Opaque pointer (`void*`).
