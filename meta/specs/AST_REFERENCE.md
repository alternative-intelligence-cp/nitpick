# Nitpick Abstract Syntax Tree

Rebuilt from the reconciled grammar — `LEXICAL_REFERENCE.md`, `OP_REFERENCE.md`,
`CONTROL_REFERENCE.md`, `TYPE_REFERENCE.md`, `TRAITS_REFERENCE.md`,
`CONCURRENCY_REFERENCE.md`, `MODULE_REFERENCE.md`, `VERIFICATION_REFERENCE.md`,
`MEMORY_REFERENCE.md` — and D-001 … D-048.

Because the compiler is built **full-frontend-first**, this must encapsulate the
entire grammar from day one: the backend advances rung by rung, but the AST does
not get to change underneath it. The previous revision did not meet that bar and
said so; this one is intended to.

## Conventions

**One naming style: `PascalCase` node names with named fields.** The previous
revision mixed `IfStmt`-style nodes with `WHEN_STMT`-style nodes carrying
positional `.a` / `.b` / `.c` operand slots. Positional slots are removed —
`stmt.then_block` cannot be confused with `stmt.end_block`, `stmt.c` can. For a
compiler under formal verification, a field name that states its meaning is worth
more than the brevity.

Every node carries `span: SourceSpan`. Omitted from the listings below.

---

# 1. Declarations

Top-level items. A source file parses to `ModuleDecl`.

| Node | Fields | Notes |
|---|---|---|
| `ModuleDecl` | `name`, `visibility`, `items: Decl[]` | file scope or `mod name { … }` |
| `ImportDecl` | `path`, `kind`, `names: Ident[]`, `alias` | `kind` ∈ wildcard / single / selective / namespace |
| `FunctionDecl` | `name`, `visibility`, `modifiers`, `generics: GenericParam[]`, `params: ParamDecl[]`, `return_type: TypeNode`, `contracts: ContractNode[]`, `body: BlockStmt?` | see §1.1 |
| `StructDecl` | `name`, `visibility`, `generics`, `fields: FieldDecl[]`, `attributes` | |
| `EnumDecl` | `name`, `visibility`, `generics`, `variants: EnumVariant[]` | variants may carry payloads |
| `TraitDecl` | `name`, `visibility`, `generics`, `supertraits: TypeNode[]`, `items: TraitItem[]` | supertraits combine with **`&`** (D-029) |
| `ImplDecl` | `target: TypeNode \| GenericParam`, `trait: TypeNode?`, `items` | **`impl:Type`** or **`impl:Type:Trait`** — type always first, no connector (D-031) |
| `TypeDecl` | `name`, `visibility`, `items` | the **namespace** construct, `Type:Name = { … }` (D-028) |
| `RuleDecl` | `name`, `subject_type`, `body: Expr`, `refines: Ident[]` | `Rules<int32>:r = { $ > 0i32 }`; `refines` holds `limit<Other>` composition |
| `MacroDecl` | `name`, `params`, `body` | invoked as **`#name(args)`** (D-046) |
| `ExternBlock` | `library`, `items: ExternFn[]` | |
| `OpaqueDecl` | `name` | `opaque struct:OpHandle;` — **`extern`-block item only**, carries no fields, no value semantics (D-066) |
| `GlobalDecl` | `name`, `visibility`, `qualifiers`, `type`, `init` | `pub const int32:MAX = 100i32;` |

## 1.1 Function declaration detail

```
FunctionDecl
  name         : Ident
  visibility   : pub | private
  modifiers    : { inline, noinline, comptime, async }
  generics     : GenericParam[]        // after the name (D-030)
  params       : ParamDecl[]
  variadic     : VariadicSpec?         // see below
  return_type  : TypeNode              // the SUCCESS type; Result<T> is implicit
  contracts    : ContractNode[]        // requires / ensures
  body         : BlockStmt?            // absent in trait declarations
```

```
GenericParam
  name       : Ident
  kind       : Type | ComptimeValue     // D-064
  value_type : TypeNode?                // ComptimeValue only — comptime int32:LEVEL
  bounds     : TypeNode[]               // Type only — combined with & (D-029)
```

- **Two kinds of parameter.** A bare `<T>` is a type parameter and may carry
  bounds; `<comptime int32:LEVEL>` is a compile-time value parameter and carries a
  `value_type` instead. The `comptime` marker is what keeps the two readable
  apart, since otherwise the introduced name would sit on opposite sides of the
  same colon in the two forms (D-064).
- **Not to be confused with `type:T`**, which is an ordinary `ParamDecl` in a
  `comptime` function, legal nowhere else, and produces no specialization
  (D-064 §5).

- **`return_type` is the success type.** Every function returns `Result<T>`
  implicitly, except `main` and `failsafe`. The AST stores the declared type; the
  wrapping is a semantic-phase concern, not a syntactic one.
- **`extern` is not a modifier here** — FFI functions live in `ExternBlock`,
  because they carry error contracts that ordinary functions do not (D-002).

### `VariadicSpec`

```
VariadicSpec
  elem_type : TypeNode     // ..*T[] — a typed slice
```

**One form: homogeneous.** `..*T[]:name` is a typed slice, and a variadic call
lowers to building one.

The format-directed form — a bare `..*` following a `fmt` parameter — was
**removed by D-053** along with the `fmt` type itself. Formatting is ordinary
functions returning `string`, spliced by `&{ }` interpolation, so no signature
needs a format string and there is no specifier language to check.

The surviving consumer is the `sys` builtin, `sys(CONST, ..*int64[])`.

## 1.2 `ExternFn`

```
ExternFn
  name         : Ident
  params       : ParamDecl[]
  variadic     : bool
  return_type  : TypeNode
  failure      : FailureContract       // REQUIRED — omission is a compile error
```

```
FailureContract = FailsOn { predicate: Expr, source: ErrnoSource? }
                | NeverFails
```

`never fails` is a required, greppable assertion rather than a default, so that
"this C function is infallible" is a claim a reviewer can audit (D-002).

## 1.3 Trait items

| Node | Fields |
|---|---|
| `TraitMethod` | `signature: FunctionDecl`, `default_body: BlockStmt?` |
| `AssocTypeDecl` | `name`, `default: TypeNode?` — **`assoc:Item;`** (D-028) |

`assoc` rather than `Type`, which declares a namespace. `Type:Foo = { … }` inside
a trait body was ambiguous between an associated type bound to an anonymous
struct and a nested namespace; `assoc` is not.

---

# 2. Statements

| Node | Fields |
|---|---|
| `BlockStmt` | `stmts: Stmt[]` — introduces a scope |
| `VarDeclStmt` | `qualifiers`, `limit: LimitNode?`, `type`, `name`, `init: Expr?`, `attributes` |
| `AssignStmt` | `target: Expr`, `op`, `value: Expr` |
| `ExprStmt` | `expr` |
| `IfStmt` | `cond`, `then_block`, `else_branch: IfStmt \| BlockStmt \| none` |
| `PickStmt` | `selector: Expr`, `arms: PickArm[]` |
| `WhileStmt` | `label: Ident?`, `cond`, `invariants: InvariantNode[]`, `body` |
| `ForStmt` | `label`, `binding: ParamDecl`, `iterable: Expr`, `invariants`, `body` |
| `LoopStmt` | `label`, `start`, `limit`, `step`, `invariants`, `body` |
| `TillStmt` | `label`, `limit`, `step`, `invariants`, `body` |
| `WhenStmt` | `label`, `cond`, `body`, `then_block: BlockStmt?`, `end_block: BlockStmt?` |
| `BreakStmt` | `label: Ident?` |
| `ContinueStmt` | `label: Ident?` |
| `PassStmt` | `value: Expr?` |
| `FailStmt` | `error: Expr` |
| `ReturnStmt` | `result: Expr` — the literal `Result{…}` form only |
| `ExitStmt` | `code: Expr` — legal only in `main` / `failsafe` |
| `TrapStmt` | `error: Expr` — `!!! errCode;` |
| `DeferStmt` | `body: BlockStmt` |
| `DiscardStmt` | `expr` — `discard(e)` / `_~ e` |
| `ProveStmt` | `condition: Expr` — **compile-time** obligation |
| `AssertStaticStmt` | `condition: Expr` |

### Notes carrying decisions

- **`ForStmt.binding` is a full `ParamDecl` with a required type.** `for (int64:i in 1..3)`
  only; the C three-clause form and untyped bindings are both rejected (D-023).
- **`LoopStmt` has `start`/`limit`/`step`; `TillStmt` has `limit`/`step`.** Both
  are **counted**, exposing the counter as `$`. `loop` infers direction from the
  bounds, so `step` must be positive — a negative or zero step is a compile error
  (D-022). Neither has an `end` block; the previous revision gave them one.
- **`WhenStmt.then_block` runs when the body executed at least once, *including*
  after a `break`. `end_block` runs only when the condition was false initially**
  (D-027). Exactly one fires. `break` must lower to `then`, not `end`.
- **`ProveStmt` is a compile-time proof obligation**, not a runtime assertion.
- **`DeferStmt` does not run on a trap** (D-014) — a lowering property, but noted
  here because it is easy to assume otherwise.

## 2.1 `PickArm`

```
PickArm
  label     : Ident?              // target of `fall label;`
  pattern   : PickPattern
  guard     : Expr?               // `where (a > b)`
  body      : BlockStmt
```

```
PickPattern = Value(Expr)                    // (200)
            | Range(lo, hi, inclusive)       // (500..599)
            | StructDestructure(type, binds) // (MouseClick { x, y })
            | EnumDestructure(path, binds)   // (Net.Disconnect(reason))
            | ErrPattern                     // ERR:
            | Wildcard                       // (*)
```

- **`ErrPattern` matches the `tbb` error sentinel.** A `pick` on a `tbb` selector
  **requires** an explicit `ERR:` arm — `Wildcard` may not absorb it, or a
  tainted value steers a branch (D-008 §5.1).
- **There is no `Unreachable` pattern.** `FORMAL_DRAFT` 05 §5.6.3's `(!)` marker
  is removed by D-061: it would let the required `ERR:` arm be elided, which is
  the author asserting a `tbb` cannot be ERR — the least safe assumption the type
  admits. An arm believed unreachable is written normally with `#unreachable()`
  as its body, which traps.
- **There is no `MacroPattern`.** Removed by D-057 rather than respelled: macros
  expand to a fixed point before semantic analysis, so no macro invocation
  survives to be matched.
- `pick` must be exhaustive. A `pick` whose arms `give` is an **expression**
  (D-059, D-060) and must additionally agree on one type across all arms.

## 2.2 Statement-level control transfers

`FallStmt { target: Ident }` and `GiveStmt { value: Expr }` are legal only inside
a `PickArm` body. `give` yields a value when `pick` is used as an expression.

---

# 3. Expressions

## 3.1 Literals

| Node | Notes |
|---|---|
| `IntLiteral` | value, base (dec/hex/bin/oct/ternary/nonary), `type_suffix?` — **suffix-form bases** (`FFhex`, `1T0t`, `2An`) |
| `FloatLiteral` | value, exponent, `type_suffix?` |
| `CharLiteral` | `char8` / `char16` / `char32` — **not an integer** (D-005) |
| `StringLiteral` | escape-processed |
| `RawStringLiteral` | `r"…"` — no escape processing (D-024) |
| `BlockStringLiteral` | `"""…"""` — newlines preserved (D-024) |
| `BoolLiteral` | |
| `SentinelLiteral` | `NULL`, `NIL`, `ERR` — **not `unknown`**, which is compiler-assigned |
| `TemplateLiteral` | `parts: (TemplatePart \| Interpolation)[]` |

## 3.2 Operators

| Node | Fields | Covers |
|---|---|---|
| `BinaryExpr` | `op`, `lhs`, `rhs` | all of `+ - * / % == != < <= > >= <=> && \|\| & \| ^ << >>` — one node, discriminated by `op` |
| `UnaryExpr` | `op`, `operand` | `!` `~` `-` |
| `PostfixExpr` | `op`, `operand` | `++` `--` |
| `AddressOfExpr` | `operand` | `@x` — yields a **second-class borrow**, not a pointer (D-004) |
| `DerefExpr` | `operand` | `<-ptr` |
| `BorrowExpr` | `mutable: bool`, `operand` | `$$i` / `$$m` |
| `PipeExpr` | `direction`, `value`, `callee` | `\|>` / `<\|` |
| `RangeExpr` | `lo`, `hi`, `inclusive` | `..` / `...` |
| `SpreadExpr` | `operand` | **`..^`** — expands a collection at a call site (D-026) |
| `TernaryExpr` | `cond`, `then_expr`, `else_expr` | `is (c) : a : b` |

## 3.3 Access and calls

| Node | Fields | Notes |
|---|---|---|
| `MemberAccessExpr` | `base`, `field` | **`.` only** — auto-dereferences pointers; `->` is type-position only (D-006) |
| `SafeNavExpr` | `base`, `field` | `?.` |
| `IndexExpr` | `base`, `index` | bounds-checked |
| `CallExpr` | `callee`, `generic_args`, `args`, `turbofish: bool` | `generic_args` may arrive implicitly (`f<int32>(x)`) or via turbofish (`f::<int32>(x)`); `turbofish` records which, since the parser needs lookahead to tell a generic call from a `<` comparison |
| `MethodCallExpr` | `receiver`, `method`, `generic_args`, `args` | UFCS — `p.magnitude()` resolves to `Point_magnitude(p)` (D-006) |
| `BuiltinExpr` | `name`, `generic_args`, `args` | **`#name<T>(…)`** (D-020) — `#size_of<T>`, `#wild_ptr<T>(addr)` |
| `ComptimeExpr` | `expr` | **`comptime(expr)`** — forces compile-time resolution; a compile error if it cannot be resolved |
| `MacroInvocationExpr` | `name`, `args` | **`#name(args)`** (D-046) — expanded before semantic analysis |

### Two kinds of builtin, and why the parser must distinguish them

| Form | Parsed as | Examples |
|---|---|---|
| **`#`-prefixed** | `BuiltinExpr` / `MacroInvocationExpr` | `#size_of<T>`, `#wild_ptr<T>(addr)`, `#derive`, user macros |
| **bare name** | ordinary `CallExpr` | `alloc`, `calloc`, `ralloc`, `dalloc`, `mcpy`, `mmov`, `memset`, `sys`, `asm`, `ok`, `is_err`, the `string_*` intrinsics |

The `#` sigil marks something **the compiler must treat specially** — evaluate at
compile time, permit an otherwise-forbidden construction, or expand before
semantic analysis. The bare-name builtins are ordinary calls that the compiler
happens to provide: they take arguments, return `Result<T>`, and are subject to
the same rules as any other function.

The distinction is syntactic and the parser acts on it directly — a `#` changes
the parse path — so it is recorded here rather than left to the semantic phase.

## 3.4 Result and safety

| Node | Fields | Notes |
|---|---|---|
| `SafeUnwrapExpr` | `expr`, `default` | `e ? d` |
| `NullCoalesceExpr` | `expr`, `default` | `e ?? d` |
| `EmphaticUnwrapExpr` | `expr`, `error_code` | **`e ?! code`** — exactly one `tbb32` argument (D-009) |
| `DefaultsExpr` | `expr`, `fallback` | `?\|` / `defaults` |
| `RawUnwrapExpr` | `expr` | `raw e` / `_! e` |
| `DropExpr` | `expr` | `drop e` / `_? e` |

## 3.5 Casts

| Node | Fields |
|---|---|
| `CastExpr` | `expr`, `target: TypeNode` — `=>`, **compile error if loss is possible** |
| `UncheckedCastExpr` | `expr`, `target` — `=>!`, the sole opt-out |

Only these two. `cast<T>` / `#cast<T>` / `@cast<T>` do not exist (D-021).

## 3.6 Construction and async

| Node | Fields |
|---|---|
| `StructLiteralExpr` | `type`, `fields` |
| `ArrayLiteralExpr` | `elements` |
| `VectorCtorExpr` | `type`, `components` — `vec3(1.0, 2.0, 3.0)` |
| `AwaitExpr` | `operand` — legal only inside `async func` (`NITPICK-040`) |
| `IterationVarExpr` | — `$`, legal only inside `loop` / `till` |
| `DynCastExpr` | `expr`, `traits: TypeNode[]` — `dyn A & B` (D-029) |

**No lambda or closure nodes.** Closures are removed (D-018); function pointers
are ordinary values referenced by `IdentifierExpr`.

---

# 4. Types

| Node | Fields | Notes |
|---|---|---|
| `NamedType` | `name`, `generic_args` | |
| `PointerType` | `pointee` | `T->` — **thin**, one word, no bounds metadata (D-038) |
| `OptionalType` | `inner` | `T?` |
| `ArrayType` | `element`, `size: Expr?` | value type; does not decay |
| `FuncType` | `params`, `return_type` | |
| `DynType` | `traits: TypeNode[]` | `dyn A & B` |
| `CStringType` | — | **`cstring`** — NUL-terminated, `{ptr, len}` (D-049). Inhabited by string literals (checked at compile time) and by `to_cstring` |
| `SelfType` | — | `Self`, valid only in `trait` / `impl` bodies (D-030) |

Qualifiers on `VarDeclStmt`, not on the type node: `stack`, `wild`, `wildx`,
`const`, `fixed`, `borrow_imm`, `borrow_mut`. **`gc` does not exist** (D-003).

### Types the parser must know as builtins

Beyond the scalar families: `fd`, `pid`, `tid`, `uid`, `gid` (D-042); the bitflag
families `oflags`, `prot`, `mflags`, `fmode`, `fcmd`, `advice`, `whence` (D-044);
`Handle`, `arena`, `shared_arena` (D-017), `atomic`, `Future`, `Optional`,
`Result`, `simd`, `complex`, `dim256`, `tfp32/64/128/256`.

---

# 5. Verification nodes

| Node | Fields |
|---|---|
| `ContractNode` | `kind: requires \| ensures`, `condition: Expr` |
| `InvariantNode` | `conditions: Expr[]` — attached to loop statements |
| `LimitNode` | `rule: Ident` — `limit<r_pos>` on a declaration or parameter |

`ensures` may reference the special `result` identifier.

---

# 6. Attributes

```
Attribute { name: Ident, args: Expr[] }      // #[align(16)], #[cfg(...)], #[derive(...)]
```

Attach to declarations. **`#[derive(…)]`**, not `@derive` (D-020).

`#[lexical_drop]` and `#[nll_drop]` are **removed** — they existed to force
deterministic RAII "bypassing standard GC", which is now the only behaviour
(D-003).

---

# 7. Removed from the previous revision

| Node | Why |
|---|---|
| `PinExpr` (`#obj`) | pinning is obsolete without a collector; `#` is now the compiler-directive sigil (D-020) |
| `LAMBDA` / closure capture | closures removed (D-018) |
| `gc` in `memory_modifier` | no collector (D-003) |
| `WHEN_STMT` / `LOOP_STMT` / `PICK_STMT` positional `.a` / `.b` / `.c` | replaced by named fields |
| `end` block on `LOOP_STMT` / `TILL_STMT` | only `when` has one (D-027) |
| `a*` collection builtins | `astack`, `alist`, `ahash`, `astringlist` and 35 operation keywords are not language builtins (D-041) |

---

# 8. Open items

These are gaps in the **grammar**, not in this document — recorded so the parser
work does not silently invent answers.

1. ~~**`MacroPattern` in `pick`**~~ — **removed by D-057**, not respelled. Macros
   expand to a fixed point *before* semantic analysis, so by the time a `pick`
   executes no macro invocation exists to match and the pattern could never fire.
   No test exercises it. AST-fragment matching, if wanted, is a separate feature
   needing its own design.
2. ~~**Macros have no governing specification.**~~ — **settled by D-057.** Two
   documents exist in `nitpick-docs` (neither carried into `meta/specs/`), but
   the real specification was in **49 regression tests**, keyed to decision codes
   `MACRO2-DEC-001…007` and `COMPTIME-006/007`. Hygiene, expansion order,
   declaration emission, and struct/impl splicing are all recovered there and now
   recorded. D-057 additionally flips the hygiene default to defining-scope,
   makes unresolved-in-defining-scope an error rather than a caller-scope
   fallback, and bounds expansion so the fixed-point loop terminates.

3. ~~**`Future<T>` visibility.**~~ — **settled by D-058: internal lowering
   artifact.** `await f()` yields `T` and `drop work()` discards, so no construct
   produces one. No `Future` type node is needed here.
4. ~~**`give` and `pick`-as-expression.**~~ — **settled by D-059: `pick` is
   both, and the arms decide.** Arms containing `give` form an exhaustive,
   single-typed expression-pick usable anywhere an expression is; arms without
   form a statement-pick. Kept because uninitialized variables are a compile
   error, so without it nothing can initialize a variable by matching. Rules
   recovered from `nitpick/TMP/audit037/`; `GiveStmt` is correct as represented.
5. **`comptime` blocks** — **three documents, three answers.**
   `macros_meta_specs.txt` §2 gives a `comptime func:` modifier and a
   `comptime(expr)` forcing form. `FORMAL_DRAFT` 07 §7.2 gives the modifier and a
   `comptime { … };` **block**. This document has only the modifier.

   **Resolved as: modifier + `comptime(expr)`, no block.** The block form is
   incoherent — `FORMAL_DRAFT`'s own example binds `int32[]:baked_data` inside
   `comptime { … }` and then expects to use it, so the construct would have to be
   a block that does **not** introduce a scope, contradicting 05 §5.2 where every
   block does. A brace form that scopes everywhere except here is exactly the
   context-dependent meaning the blueprint philosophy rejects. `comptime(expr)`
   expresses the same thing as an ordinary expression with an ordinary binding.

   `ComptimeExpr` is therefore added to §3; `FunctionDecl.modifiers` already
   carries `comptime`.
