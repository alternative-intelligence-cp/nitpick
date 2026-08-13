# Nitpick-Next Module & FFI Reference

This document establishes the canonical module system, visibility rules, and Foreign Function Interface (FFI) for the `nitpick-next` compiler.

## 1. Modules (`mod`)

Modules allow the organization of code into hierarchical namespaces.

### 1.1 Defining Modules
Modules can be defined inline or exist in external files.

*   **Inline Modules**:
    ```nitpick
    mod network {
        pub func:connect = int32() { pass 0i32; };
        func:internal = int32() { pass 1i32; }; // Private
    }
    ```

*   **External File Modules**:
    ```nitpick
    mod network;
    ```
    The compiler searches for `network.npk` or `network/mod.npk` relative to the declaring file.

*   **Nested Modules**: Modules can be arbitrarily nested (e.g., `mod core { mod math { ... } }`).
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

## 3. Visibility (`pub`)

Nitpick uses a strict binary visibility model: **Public** or **Private**.

*   **Private (Default)**: Symbols are accessible only within the same module/file. Intra-module access to private symbols is always permitted.
*   **Public (`pub`)**: Prefix declarations with `pub` to export them.
    ```nitpick
    pub func:compute = int32() { ... };
    pub struct:Point = { ... };
    pub const int32:MAX = 100i32;
    pub mod utils { ... }
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

## 5. Foreign Function Interface (`extern`)

`extern` blocks are used for C FFI bindings. 

### 5.1 Extern Syntax

```nitpick
extern "libc" {
    func:printf = int32(any->:fmt);
    func:malloc = wild any->(int64:size);
}
```

### 5.2 Mandatory `Result<T>` Wrapping and error contracts
> **⚠️ CRITICAL DEVIATION FROM PROTOTYPE:** In the original prototype, `extern` functions returned bare values. Now **ALL functions, including `extern` bindings, return `Result<T>`**.

When calling C FFI functions via `extern`, the compiler automatically wraps the C return value in a `Result<T>`. This ensures consistency across the language: you never have to guess whether a function call needs error handling or `raw`.

**A failing C call must produce an errored `Result`, never a silent success.**
C has no universal failure convention — it is per-function — so the mapping
cannot be inferred from the type. Every `extern` declaration therefore states its
own failure condition, and **omitting it is a compile error** (D-002):

```nitpick
extern "libc" {
    func:open   = int32(int8->:path, int32:flags)  fails on result < 0i32 with errno;
    func:malloc = wild any->(int64:size)           fails on result == NULL;
    func:strlen = int64(int8->:s)                  never fails;
}
```

*   `fails on <expr>` — predicate over `result` marking the call as failed.
*   `with errno` — optional source of the error code; without it a generic FFI error code is used.
*   `never fails` — an explicit, greppable assertion that the function cannot fail. **Required rather than implied**, so that "this C function is infallible" is a documented claim a reviewer can audit rather than an unstated default.

Predicates are unparenthesized, matching `requires` / `ensures` rather than the
parenthesized conditions of `if` / `while`. The keyword is `fails on`, not
`fails when` — `when` is the state-tracked loop construct and must not acquire a
second meaning.

If you do not care about the error from an `extern` function, append `raw` or use the `_!` prefix to unwrap the value directly. The optimizer strips the wrapper at compile time, guaranteeing **zero runtime overhead**.

```nitpick
int32:n = raw printf("Hello"); 
// OR
int32:x = _! printf("World");
```

### 5.3 ABI Pointers
*   **Strings**: Do not pass native `string` types directly to C functions expecting `char*`. Use the `as_cstring(string)` builtin to generate a null-terminated `char8[]` array, and pass a pointer to that array instead.
*   **Pointers**:
    *   `int32->`: Scalar pointer (`int32_t*`).
    *   `MyStruct->`: Struct pointer (`struct MyStruct*`).
    *   `any->`: Erased/Opaque pointer (`void*`).
