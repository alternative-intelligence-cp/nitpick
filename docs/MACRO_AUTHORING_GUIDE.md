# Macro & Derive Authoring Guide

> v0.8.4 — Aria Compiler Documentation

## Overview

Aria's macro system provides two mechanisms for code generation:

1. **Derive macros** — Auto-generate trait implementations for structs
2. **Attribute macros** — Modify or annotate declarations at compile time

## Derive Macros

### Built-in Derives

Apply derive attributes to structs to auto-generate trait implementations:

```aria
#[derive(Eq, Ord, Clone, ToString, Debug, Hash)]
struct:Point = {
    int32:x;
    int32:y;
};
```

#### Available Traits

| Trait | Method | Signature | Description |
|-------|--------|-----------|-------------|
| `ToString` | `to_string` | `string(Self:self)` | String representation |
| `Debug` | `debug` | `string(Self:self)` | Debug representation with types |
| `Eq` | `eq` | `bool(Self:self, Self:other)` | Field-by-field equality |
| `Ord` | `less_than` | `bool(Self:self, Self:other)` | Lexicographic less-than |
| `Hash` | `hash` | `uint64(Self:self)` | FNV-1a hash |
| `Clone` | `clone` | `Self(Self:self)` | Shallow copy |

### How Derive Works

1. The type checker expands `#[derive(Trait)]` into a synthetic `impl:Trait:for:StructName`
2. The generated impl contains methods that operate field-by-field
3. Synthetic impls are injected into the AST before IR generation
4. IR codegen generates the mangled function (e.g., `Point_less_than`)

### Using Derived Methods

Derived methods are called via UFCS (Uniform Function Call Syntax):

```aria
Point:a = Point{ x: 1, y: 2 };
Point:b = Point{ x: 3, y: 4 };

bool:is_equal = raw a.eq(b);
bool:is_less = raw a.less_than(b);
string:repr = raw a.to_string();
```

Note: Derived methods return `Result<T>`, so use `raw` to unwrap.

## Attribute Macros

### Available Attributes

| Attribute | Target | Description |
|-----------|--------|-------------|
| `#[inline]` | Functions | Hint to inline the function |
| `#[noinline]` | Functions | Prevent inlining |
| `#[align(N)]` | Structs/Vars | Set alignment in bytes |
| `#[derive(...)]` | Structs | Generate trait impls |
| `#[gpu_kernel]` | Functions | Mark as GPU kernel |
| `#[gpu_device]` | Functions | Mark as GPU device function |
| `#[comptime]` | Functions | Evaluate at compile time |
