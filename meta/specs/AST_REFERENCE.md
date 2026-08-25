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
| `ModuleDecl` | `name`, `visibility`, `items: Decl[]` | file scope, **`mod:name = { … };`**, or **`mod:name;`** for a file (D-088) |
| `ImportDecl` | `path`, `kind`, `names: Ident[]`, `alias` | `kind` ∈ wildcard / single / selective / namespace |
| `FunctionDecl` | `name`, `visibility`, `modifiers`, `generics: GenericParam[]`, `params: ParamDecl[]`, `return_type: TypeNode`, `contracts: (ContractNode \| NeverFails)[]`, `body: BlockStmt?` | see §1.1; the contracts window holds `requires`/`ensures`/`acquires` and the `never fails` marker (D-163) |
| `StructDecl` | `name`, `visibility`, `generics`, `fields: FieldDecl[]`, `attributes` | |
| `EnumDecl` | `name`, `visibility`, `generics`, `variants: EnumVariant[]` | variants may carry payloads |
| `TraitDecl` | `name`, `visibility`, `generics`, `supertraits: TypeNode[]`, `items: TraitItem[]` | supertraits combine with **`&`** (D-029) |
| `ImplDecl` | `target: TypeNode \| GenericParam`, `trait: TypeNode?`, `items` | **`impl:Type`** or **`impl:Type:Trait`** — type always first, no connector (D-031) |
| `RuleDecl` | `name`, `subject_type`, `body: Expr`, `refines: Ident[]` | `Rules<int32>:r = { $ > 0i32 }`; `refines` holds `limit<Other>` composition |
| `MacroDecl` | `name`, `params`, `body_kind`, `body` | invoked as **`#name(args)`** (D-046). The body is declarations, statements, or one expression, and `body_kind` says which — that is what decides where the macro may be invoked (`MACRO_REFERENCE.md` §1) |
| `MacroSplice` | `invocation: BuiltinExpr` | an invocation standing **where a declaration is expected** — module level, a `struct` body, an `impl` body. Expression position needs no node of its own: there the invocation already *is* a `BuiltinExpr`. Expansion replaces it with whatever the body emits |
| `ExternBlock` | `library`, `items: ExternFn[]` | **`extern:"libc" = { … };`** (D-088) — the name slot holds a string because a library name is not an identifier |
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
  contracts    : (ContractNode | NeverFails)[]   // requires / ensures / never fails (D-163)
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

- **`ParamDecl` and `FieldDecl` carry memory qualifiers**, in the same flags slot
  and with the same bits a local declaration uses — `wild int8->:buf` is a
  declaration qualified `wild`, whether it is a local, a global, a field or a
  parameter. Qualifiers are not part of the type (§4), so the type parser refuses
  to consume one: a type that swallowed `wild` would drop it on the floor. Until
  0.7.3 they were read in statement and return position only, so **the compiler's
  own sources did not parse** — sixteen of its files begin a struct or a signature
  this way.
- **`ParamDecl` carries `discarded: bool`** — the `Type:_~name` annotation
  (D-089), marking a parameter the body deliberately does not read. The name is
  kept: `cstring[]:_~argv` still says what the slot is, which a `_` placeholder
  would not. **Reading a discarded parameter is an error**, because an unchecked
  claim is decoration.
- **`main` has a fixed signature: `func:main = int32(cstring[]:argv)`** (D-089).
  One parameter, always — `failsafe` sets the same precedent with `tbb32:err`, so
  entry-point signatures are fixed and `_~` covers the unused case. There is no
  `argc`: a slice carries its length (D-070), and a second copy of that fact is
  the C bug where a loop trusts `argc` past the end of `argv`.
- **`return_type` is the success type.** Every function returns `Result<T>`
  implicitly, except `main` and `failsafe`. The AST stores the declared type; the
  wrapping is a semantic-phase concern, not a syntactic one.
- **`extern` is not a modifier here** — FFI functions live in `ExternBlock`,
  because they carry error contracts that ordinary functions do not (D-002).

### `VariadicSpec`

```
VariadicSpec
  slice_type : TypeNode    // ..*T[] — the whole T[], a slice {ptr, i64 len} (D-070)
```

**One form: homogeneous.** `..*T[]:name` is a typed slice, and a variadic call
lowers to building one.

> **The slot holds `T[]`, not `T`.** `..*T[]:rest` parses `T[]` with the ordinary
> type parser, so the node carries the slice the parameter is bound to inside the
> body; the element each trailing argument is checked against is that slice's
> element. It was called `elem_type`, which reads as the `T` and is what a caller
> would use it as (D-100).

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

**`FailsOn` and `NeverFails` are separate node kinds**, not one node with a flag,
following `PickPattern` — whose six arms are six kinds. The difference is not
cosmetic: "was a contract written, and which one" is then answerable from the
node's kind, where a shared kind plus a boolean makes an unwritten contract and a
`never fails` contract the same node until someone reads the right slot. D-002
exists to make the infallibility claim explicit, and an encoding that can be
misread by ignoring a field undoes it.

## 1.3 Trait items

| Node | Fields |
|---|---|
| `AssocTypeDecl` | `name`, `default: TypeNode?` — **`assoc:Item;`** (D-028) |

> **`TraitMethod` is removed. A method in a trait body is an ordinary
> `FunctionDecl`.** It was specified as `{ signature: FunctionDecl, default_body:
> BlockStmt? }` — and a `FunctionDecl` already carries `body: BlockStmt?`, whose
> §1.1 note reads "absent in trait declarations". So a default body had two
> places to live, and a reader had to know which one a given producer used. One
> fact, one slot: an absent body is a declaration, a present one is a default
> implementation, and that is the whole distinction the wrapper was carrying.

`assoc` rather than `Type`. D-028 moved associated types off `Type` because
`Type:Foo = { … };` inside a trait body was ambiguous between an associated type
bound to an anonymous struct and a nested namespace. **D-088 then removed the
namespace construct entirely** — `mod` already did that job and does it better —
so `Type` is no longer a keyword at all and the ambiguity cannot recur.

---

# 2. Statements

| Node | Fields |
|---|---|
| `BlockStmt` | `stmts: Stmt[]` — introduces a scope |
| `VarDeclStmt` | `qualifiers`, `limit: LimitNode?`, `type`, `name`, `init: Expr?`, `attributes` |
| `AssignStmt` | `target: Expr`, `op`, `value: Expr` |
| `ExprStmt` | `expr` | must have type `NIL`, and the value-less forms are a CLOSED list (D-163 rule 6): `drop f();` / `relay f();` / `f() ?! c;` / `f() ?\| NIL;` — a bare call discards a `Result` (`TYPE-039`) |
| `IfStmt` | `cond`, `then_block`, `else_branch: IfStmt \| BlockStmt \| none` |
| `PickStmt` | `selector: Expr`, `arms: PickArm[]` |
| `WhileStmt` | `label: Ident?`, `cond`, `invariants: InvariantNode[]`, `body` |
| `ForStmt` | `label`, `binding: ParamDecl`, `iterable: Expr`, `invariants`, `body` |
| `LoopStmt` | `label`, `start`, `limit`, `step`, `invariants`, `body` |
| `TillStmt` | `label`, `limit`, `step`, `invariants`, `body` |
| `WhenStmt` | `label`, `cond`, `invariants`, `body`, `then_block: BlockStmt?`, `end_block: BlockStmt?` |
| `BreakStmt` | `label: Ident?` |
| `ContinueStmt` | `label: Ident?` |
| `PassStmt` | `value: Expr?` |
| `FailStmt` | `error: Expr` |
| `ReturnStmt` | `result: Expr` — the literal `Result{…}` form only |
| `ExitStmt` | `code: Expr` — legal only in `main` / `failsafe` |
| `TrapStmt` | `error: Expr` — `!!! errCode;` |
| `DeferStmt` | `body: BlockStmt` |
| `DiscardStmt` | `expr` — `discard(e)` / `_~ e`. The **statement** form; the declaration-site form is `ParamDecl.discarded` (§1.1, D-089) |
| `ProveStmt` | `condition: Expr` — **compile-time** obligation |
| `AssertStaticStmt` | `condition: Expr` |
| `FallStmt` | `target: Ident` — `fall label;`, legal only in a `PickArm` body (§2.2) |
| `GiveStmt` | `value: Expr` — `give e;`, legal only in a `PickArm` body (§2.2) |

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
- **`WhenStmt` carries `invariants` like every other loop.** This table omitted
  them while `VERIFICATION_REFERENCE.md` §4 lists `when` among the constructs
  that take an `invariant` clause — a straight contradiction, resolved in favour
  of `VERIFICATION_REFERENCE`. `when` is a `while` that tracks how it terminated;
  a loop form that could not state an invariant would be an exception to remember
  for no reason anyone could give.
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

**Both are ordinary statement nodes and now appear in §2's table.** They were
declared here in prose only, so the node-kind table generated from §2 did not
have them and the parser had nothing to build. "Legal only inside a `PickArm`" is
a **semantic** restriction, not a syntactic one: the parser accepts them anywhere
a statement is accepted and the placement check belongs to semantic analysis
(D-085 — the parser never restricts). Writing the restriction into the grammar
would give `fall` outside a `pick` the diagnostic "expected a statement", which
names neither the construct nor the rule it broke.

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
| `MoveExpr` | `place` | **`move(place)`** — transfers ownership and invalidates the source (D-065) |
| **`IsErrExpr`** | `operand` | **`is_err(tbbValue)`** — tests a `tbb` for ERR **without trapping** (D-008, D-096) |
| **`ResultLiteralExpr`** | `value`, `error` | **`Result{value: v, err: e}`** — the only way to construct a `Result`, and the only legal operand of `return` (D-097) |

> **There is no `OptionalLiteralExpr`, deliberately (D-099).** An `Optional` is
> built by writing the value and emptied by writing `NIL`, so there is no
> construct for a node to represent. One was drafted by symmetry with
> `ResultLiteralExpr` and withdrawn — `NIL` had always been the answer.

> **`ResultLiteralExpr` and `IsErrExpr` were missing, and nothing could have
> noticed.** Three constructs the specs require could not be written down,
> because each is spelled with a KEYWORD and the parser only builds these forms
> from an identifier.
>
> - **`Result{value: v, err: e}`** is the language's only constructor and, per
>   `ReturnStmt` in §2, the only legal operand of `return`. `TYPE_REFERENCE.md`
>   §11 gives the full desugar table for `pass`/`fail` over it. The struct-literal
>   path is gated on `TokenKind.Ident`, and `Result` is `KwResult`, so the real
>   parser answered `NITPICK-PARSE-002`.
> - **`is_err(x)`** is the one test guaranteed not to trap: branching on a `tbb`
>   ERR value traps to `failsafe`, so a program needs a way to ask "did this go
>   ERR" that is safe on every value. Its operand is a **`tbb`**, not a `Result`
>   — `OP_REFERENCE.md` §5 is explicit and an earlier reading of this document got
>   it backwards.
> - **`ok(val)`** was removed instead (D-097). It tested user-writable `unknown`,
>   which D-007 had already removed, so it was an operator whose subject no longer
>   existed.
>
> `ResultLiteralExpr` has a FIXED SHAPE — exactly `value` and `error`, in either
> written order — rather than reusing `StructLiteralExpr`. A struct literal's
> window alternates interned NAMES with values, and a keyword has no intern index;
> reusing it would put a token kind where a name index belongs, which is the same
> confusion that made `Result<int32>` report "there is no type named" (D-096).
>
> `IsErrExpr` takes `MoveExpr`'s shape: a keyword operator with a parenthesised
> operand. It is not a call, because 0.5 and 0.7 have to *see* which construct
> carries the no-trap guarantee rather than recover it from a callee's name.
>
> A comment in `resolve.npk` had described `ok` and `is_err` as *bare-name
> builtins* that "resolve to nothing and that is correct". They are not in
> `builtins.npk`, and they are keywords, so nothing about that sentence was true
> — it described a path neither name could reach.

> **`MoveExpr` was missing.** D-065 settled `move(place)` as a keyword operator
> with a parenthesised operand — the same shape `comptime(expr)` has — and it is
> in `LEXICAL_REFERENCE.md`'s `ControlFlow` keywords and specified in
> `MEMORY_REFERENCE.md` §2.3. It simply never got a node here, which the parser
> found by needing one. Its operand is a **place**, not a value, which is why it
> is not a `UnaryExpr`: no other unary operator constrains what it may be applied
> to.

## 3.3 Access and calls

| Node | Fields | Notes |
|---|---|---|
| `IdentifierExpr` | `name` | a bare name — a variable, a function, a type in expression position |
| `MemberAccessExpr` | `base`, `field` | **`.` only** — auto-dereferences pointers; `->` is type-position only (D-006) |
| `SafeNavExpr` | `base`, `field` | `?.` |
| `IndexExpr` | `base`, `index` | bounds-checked |
| `CallExpr` | `callee`, `generic_args`, `args`, `turbofish: bool` | `generic_args` may arrive implicitly (`f<int32>(x)`) or via turbofish (`f::<int32>(x)`); `turbofish` records which, since the parser needs lookahead to tell a generic call from a `<` comparison |
| `MethodCallExpr` | `receiver`, `method`, `generic_args`, `args` | UFCS — `p.magnitude()` resolves to `Point_magnitude(p)` (D-006). **A member access followed by `(` is this node, not a `CallExpr` over a `MemberAccessExpr`** — UFCS resolution needs the receiver kept apart from the callee, and two shapes for one call would leave every consumer testing for both |
| `BuiltinExpr` | `name`, `generic_args`, `args` | **`#name<T>(…)`** (D-020) — `#size_of<T>`, `#wild_ptr<T>(addr)` |
| `ComptimeExpr` | `expr` | **`comptime(expr)`** — forces compile-time resolution; a compile error if it cannot be resolved |

> **`IdentifierExpr` was missing**, exactly as `MoveExpr` was: §3.6 refers to it
> by name ("function pointers are ordinary values referenced by
> `IdentifierExpr`") but no table ever declared it. The parser found it by
> needing it, and the shape it had reached for in the meantime is the reason this
> matters: a bare name became a `MemberAccessExpr` with a **zero base**. That
> makes one encoding carry two meanings — `x` and `(nothing).x` become the same
> node, told apart only by a base that happens to be zero — and every consumer
> walking an access chain then has to know that a zero base silently means "stop,
> this was a name". A name is not an access with a missing operand. It gets its
> own node.

### Two kinds of builtin, and why the parser must distinguish them

| Form | Parsed as | Examples |
|---|---|---|
| **`#`-prefixed** | `BuiltinExpr` | `#size_of<T>`, `#wild_ptr<T>(addr)`, `#derive`, user macros |
| **bare name** | ordinary `CallExpr` | `alloc`, `calloc`, `ralloc`, `dalloc`, `mcpy`, `mmov`, `memset`, `sys`, `asm`, `ok`, `is_err`, the `string_*` intrinsics |

> **`MacroInvocationExpr` is removed; `#name(args)` is a `BuiltinExpr`.** D-046
> settles that "a caller does not need to know whether `#foo(x)` is
> compiler-provided or user-defined; both expand at compile time and both are
> type-checked after expansion." The parser is a caller in exactly that sense: it
> sees `#`, a name, and arguments, and **nothing in the syntax says which kind of
> name it is.** A macro may be imported, so the file being parsed need not
> contain its declaration.
>
> Two node kinds for one syntactic form, discriminated by a fact the parser does
> not have, is a kind that can never be built — and it was one, until 0.2.8's
> reachability check named it. Resolution happens after macro expansion, where
> the answer is actually available.

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
| `DefaultsExpr` | `expr`, `fallback` | `?\|` / `defaults` — **struck (D-167)**; the node is still built so the parser never restricts, and refused by name |
| `RawUnwrapExpr` | `expr` | `raw e` / `_! e` |
| `DropExpr` | `expr` | `drop e` / `_? e` |
| **`RelayExpr`** | `expr` | **`relay e` / `_^ e`** (D-080) — on error, returns the same code from the enclosing function; otherwise yields `.value` |

- **`RelayExpr` is a normal exit path**, so `defer` runs on the error branch —
  unlike `EmphaticUnwrapExpr`, which traps and runs nothing (D-014, amended).
- It is **illegal in `main` and `failsafe`**, the two functions that return a bare
  `int32` and leave through `exit`, so there is no `Result` to relay into.
- These five, plus `await`, occupy **precedence level 2** (D-081); they had no
  level at all before.

## 3.5 Casts

| Node | Fields |
|---|---|
| `CastExpr` | `expr`, `target: TypeNode`, `quals` — `=>`, **compile error if loss is possible** |
| `UncheckedCastExpr` | `expr`, `target`, `quals` — `=>!`, the sole opt-out |

Only these two. `cast<T>` / `#cast<T>` / `@cast<T>` do not exist (D-021).

**A cast target carries a memory qualifier** — `p => wild int8->` — and it is
load-bearing rather than decoration: to LLVM a `wild` pointer and a managed one are
the same word, and the distinction is enforced entirely by the type checker (D-038),
so the cast target is where a program says the result is unmanaged. `quals` is on
`DynCastExpr` too, in the same slot, because a qualifier a node cannot record is one
nothing can refuse.

## 3.6 Construction and async

| Node | Fields |
|---|---|
| `StructLiteralExpr` | `type`, `fields` |
| `ArrayLiteralExpr` | `elements` |
| `VectorCtorExpr` | `type`, `components` — `vec3(1.0, 2.0, 3.0)` |
| `AwaitExpr` | `operand` — legal only inside `async func` (`NITPICK-040`) |
| `IterationVarExpr` | — `$`, legal only inside `loop` / `till` |
| `DynCastExpr` | `expr`, `traits: TypeNode[]`, `quals` — `dyn A & B` (D-029). **A `=>` whose target is a `dyn` type is this node, not a `CastExpr`** — building a fat pointer is not the same operation as a checked scalar conversion, and giving them one node would hide that at every use |
| `PickExpr` | `selector: Expr`, `arms: PickArm[]` — a `pick` whose arms `give` (D-059) |

> **`PickExpr` was missing.** D-059 settled that `pick` is **both** a statement
> and an expression and that the arms decide which, but only `PickStmt` ever got
> a node — so the construct D-059 created could not be built. Since uninitialized
> variables are a compile error, an expression-`pick` is the only way to
> initialize a variable by matching, which is the reason D-059 kept it.
>
> It is a second node rather than one node used in both positions, and the reason
> is **termination**, not taste. `CONTROL_REFERENCE.md`'s standing rule is that
> control-flow blocks take no trailing `;` — `pick (x) { … }` as a statement ends
> at its brace. An expression-`pick` ends wherever its enclosing statement does:
> `int32:v = pick (y) { … };` takes the `;` of the declaration. Representing both
> as one node in expression position would make the semicolon rule depend on what
> the expression happened to contain, which is exactly the context-dependence the
> rest of the grammar avoids.
>
> The two carry identical fields and identical slot layouts, so one set of arm
> accessors reads both; only the kind differs.

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
| `FuncType` | `params`, `return_type`, `never_fails: bool` | D-163 |
| `DynType` | `traits: TypeNode[]` | `dyn A & B` |
| `FuncType` | `params`, `return_type`, `never_fails: bool` | **`func RetType(ParamTypes) [never fails]`** (D-087; D-163) — the same parts, in the same order, as the declaration it is the type of; the contract is part of the type's identity, and a may-fail function cannot fill a `never fails` slot |
| `CStringType` | — | **`cstring`** — NUL-terminated, `{ptr, len}` (D-049). Inhabited by string literals (checked at compile time) and by `to_cstring` |
| `AnyType` | — | **`any`** — the type-erased pointer, C's `void*`. **Only legal under `->`**; bare `any` is a type error |
| `SelfType` | — | `Self`, valid only in `trait` / `impl` bodies (D-030) |
| `ComptimeArg` | `value: Expr` | **`Mutex<Config, 2>`** — a compile-time **value** in a type-argument list (D-064 §2, D-109). Not a type, and it appears nowhere else: a generic argument list is positional and holds both kinds, so every entry has to be one index |
| `Projection` | `base`, `name` (payload) | **`T.Item`** — an associated type projected from a type (D-164). A TYPE node rather than a path because `.` here brings a member of the base toward you, which is what `.` means everywhere else in the language |

Qualifiers on `VarDeclStmt`, not on the type node: `stack`, `wild`, `wildx`,
`const`, `fixed`, `borrow_imm`, `borrow_mut`. **`gc` does not exist** (D-003).

> **Three of these were unreachable and one built the wrong node**, all found by
> 0.2.4's comparison of this table against the generated kinds and fixed in 0.2.8:
>
> - **`FuncType` had no spelling in any document.** `FULL_specs.txt` gives
>   `FuncType ::= "func"` and defers to a chapter that does not cover it, so
>   function pointers were values with no type while §3.6 says they are ordinary
>   values. D-087 settles the spelling.
> - **`cstring` was not in `LEXICAL_REFERENCE.md`'s `BuiltinType`**, so it lexed
>   as an identifier and became a `NamedType`. A user type named `cstring` would
>   have silently shadowed the builtin.
> - **`any` was the mirror image** — the keyword existed and the node did not.
> - **`ArrayType.size` consumed one token and called it an integer literal**
>   whatever it was, so `int32[COUNT]` produced an array sized by `COUNT`'s
>   intern-table slot. Not an error; a plausible wrong number.
>
> The shared trait is worth stating: **none of them failed.** Each produced a
> different program than the source said. A parser that rejects something is a
> parser you argue with; a parser that accepts something and means something else
> is a parser you trust and should not.

**`NIL` is a type as well as a value**, and it is a `NamedType` like any other
builtin — it needs no node of its own. `func:reset = NIL(Ast->:a)` declares the
return type; `pass NIL;` yields the single value that inhabits it. This is not
one word with two meanings: `NIL` is a type with exactly one inhabitant, so the
type and the value are the same thing named once. It was missing from
`LEXICAL_REFERENCE.md`'s `BuiltinType` production, which made the type parser
reject a spelling the compiler's own sources use on nearly every mutating
function.

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
| `NeverFails` | *(no fields)* — the `never fails` contract on an ordinary function, trait method, impl method, `comptime` function, or function type (D-163). Distinct from the Decl-side `NeverFails` that `extern` blocks carry (D-002). |

`ensures` may reference the special `result` identifier.

---

# 6. Attributes

```
Attribute { name: Ident, args: Expr[] }      // #[align(16)], #[cfg(...)], #[derive(...)]
```

Attach to declarations. **`#[derive(…)]`**, not `@derive` (D-020).

`Attribute` **is a node in the declaration array**, alongside `GenericParam`,
`FieldDecl`, `VariadicSpec` and the failure contracts — the array is already
where declaration sub-structures live, and an attribute is one. It had no kind at
all until 0.2.5 needed to build one, because the kind table is generated from the
node tables in §§1–5 and this section is neither one of those nor a table.

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
