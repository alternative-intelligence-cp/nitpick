# Nitpick Lexical Grammar

Adopted from `FORMAL_DRAFT/01_lexical_analysis.md` with corrections applied for
D-001…D-024. See `GRAMMAR_ADOPTION_CONFLICTS.md` for what changed and why.

Grammar is written in **W3C EBNF** (the XML 1.0 dialect), chosen for clean Unicode
character-class support. Lexical rules begin with an uppercase letter; the tokens
they produce are the terminal symbols of the syntactic grammar.

---

## 1. Source Text

Nitpick source is a sequence of Unicode code points encoded in UTF-8. Characters
not forming part of a valid token are a lexical error.

```ebnf
SourceCharacter ::= #x0000 - #x10FFFF
```

## 2. Whitespace and Comments

Discarded by the lexer; they separate tokens and do not appear in the token
stream. Block comments do **not** nest.

```ebnf
WhiteSpace   ::= [ \t\r\n]+
Comment      ::= LineComment | BlockComment
LineComment  ::= "//" (SourceCharacter - "\n")*
BlockComment ::= "/*" (SourceCharacter - "*/")* "*/"
```

## 3. Identifiers

ASCII-bounded, beginning with a letter or underscore.

```ebnf
Identifier      ::= IdentifierStart IdentifierPart*
IdentifierStart ::= [a-zA-Z_]
IdentifierPart  ::= [a-zA-Z0-9_]
```

> The lexer resolves `_?`, `_!`, and `_~` as distinct operators **before**
> evaluating identifiers that begin with an underscore.

## 4. Keywords

```ebnf
Keyword ::= MemoryQualifier | MemoryOrdering | ControlFlow | AsyncKeyword
          | ModuleKeyword | TypeKeyword | VerificationKeyword | BuiltinHelper

MemoryQualifier     ::= "wild" | "wildx" | "stack" | "defer"

MemoryOrdering      ::= "relaxed" | "acquire" | "release" | "acq_rel" | "seq_cst"

ControlFlow         ::= "if" | "else" | "while" | "for" | "loop" | "till"
                      | "when" | "then" | "end" | "pick" | "fall" | "where"
                      | "give" | "break" | "continue" | "return" | "pass"
                      | "fail" | "exit" | "raw" | "drop" | "nodrop"
                      | "defaults" | "discard" | "move" | "relay"

VerificationKeyword ::= "prove" | "assert_static" | "requires" | "ensures"
                      | "acquires"
                      | "invariant" | "fails" | "on" | "with" | "never"

; the contract position after a parameter list (D-163, D-181):
;   Contracts ::= ( "requires" Expr | "ensures" Expr
;                 | "acquires" ["<="] Expr | "never" "fails"
;                 | "joins" Expr )*
; `joins <const Duration>` states a THREAD's join deadline where its executor
; is created (D-083/D-181) — one greppable, reviewable place per thread.
; `never fails` is also legal after a function TYPE's parameter list.

AsyncKeyword        ::= "async" | "await" | "thread" | "joins"

ModuleKeyword       ::= "use" | "mod" | "pub" | "extern" | "cfg" | "as"
                      | "comptime" | "inline" | "noinline" | "macro" | "derive"

TypeKeyword         ::= "struct" | "enum" | "assoc" | "opaque" | "error"
                      | "trait" | "impl" | "Self"
                      | "Rules" | "limit" | "const" | "fixed" | BuiltinType

BuiltinType         ::= "int1" | "int2" | "int4" | "int8" | "int16" | "int32"
                      | "int64" | "int128" | "int256" | "int512" | "int1024"
                      | "int2048" | "int4096"
                      | "uint1" | "uint2" | "uint4" | "uint8" | "uint16"
                      | "uint32" | "uint64" | "uint128" | "uint256" | "uint512"
                      | "uint1024" | "uint2048" | "uint4096"
                      | "tbb8" | "tbb16" | "tbb32" | "tbb64" | "tbb128" | "tbb256"
                      | "frac8" | "frac16" | "frac32" | "frac64"
                      | "tfp32" | "tfp64" | "tfp128" | "tfp256" | "dim256"
                      | "flt32" | "flt64" | "flt128" | "flt256" | "flt512"
                      | "bool" | "char8" | "char16" | "char32" | "string"
                      | "cstring"
                      | "fd" | "pid" | "tid" | "uid" | "gid"
                      | "dyn" | "any" | "Result" | "Optional"
                      | "Handle" | "arena" | "shared_arena" | "atomic" | "Future"
                      | "Channel"
                      | "simd" | "complex" | "array" | "func"
                      | "trit" | "tryte" | "nit" | "nyte"
                      | "buffer" | "NIL"

BuiltinHelper       ::= "is" | "in" | "is_err"
```

> **`vec2`, `vec3`, `vec9`, `matrix`, `tmatrix`, `tensor` and `ttensor` were
> keywords and are not** (D-135). They are **library types** now, built on
> `simd<T, N>`, and a library cannot declare a type whose name is a keyword — so
> keeping them reserved would have made the library that defines them unwritable.
>
> `simd` stays, because it is the primitive the rest are built from. So do `trit`,
> `tryte`, `nit` and `nyte`: no hardware implements balanced ternary, every
> operation on one is emulation, and the compiler is the only place that can be done
> well. The argument for keeping those primitive does not transfer to the vectors,
> which is why the two groups were decided separately.

### Corrections applied

| Change | Reason |
|---|---|
| `gc` removed from `MemoryQualifier` | D-003 — no collector |
| `fails`, `on`, `with`, `never` added | D-002 — FFI error contracts. `with` binds the error source (`with errno`) and was missed in the first pass. |
| `is_err` added | D-008 — non-trapping ERR test |
| **`ok` removed** | D-097 — it tested user-writable `unknown`, which D-007 removed. `is_err` is the non-trapping test for `tbb`; `.is_error` covers `Result`. |
| `discard` added to `ControlFlow` | it is a statement keyword and was absent |
| `tbb128`, `tbb256` added | `TYPE_REFERENCE.md` §6 defines them |
| `fix256` → `dim256` | D-036 — `dim256` has dimensional analysis; `fix256` is its obsolete name. Also removes the `fix` / `fixed` near-miss, `fixed` being the immutability qualifier. |
| `tfp128`, `tfp256` added | D-036 — `TYPE_REFERENCE.md` §5 defines four widths; only two were listed |
| `char8/16/32` added | `TYPE_REFERENCE.md` §2 — semantically distinct from `uint8` |
| `Handle`, `arena`, `shared_arena`, `atomic`, `Future`, `Optional`, `simd`, `complex` added | all specified in `TYPE_REFERENCE.md`; all were missing |
| **35 `a*` collection keywords removed** | D-041 — `astack`, `alist`, `ahash`, `astringlist` and their operations are not language builtins; collections belong in a library. Returns 35 reserved words to userland and removes the last major `aria` naming artifact from the language surface. |
| `fd`, `pid`, `tid`, `uid`, `gid` added | D-042 — kernel identifiers are distinct types permitting comparison but not arithmetic. Combined with `Result<T>`, an `fd` is always valid: POSIX's `-1` goes to `Result.err` and is not representable. |
| `assoc` added | D-028 — declares an associated type |
| **`Type` removed** | D-088 — the namespace construct it named is gone, `mod` having done that job all along and done it better (it can name a *file*, joins the module graph, and is what `use` imports from). A reserved word naming nothing costs a user an identifier and gives a reader a keyword they cannot look up — the same reasoning as D-041's 35 collection keywords and D-074's five. |
| **`stream`, `process`, `pipe`, `debug`, `log` removed** | D-074 — all five were reserved and **defined nowhere**; `TYPE_REFERENCE` skips §24, which is where `stream` presumably went. A reserved word that names nothing costs userland an identifier and gives a reader a keyword they cannot look up. Same reasoning as D-041's 35 collection keywords. The I/O model is `IO_REFERENCE.md` and needs no language syntax. |
| **`binary` removed** | D-074 — `{ptr, i64 length}` is *identical* to a slice (D-070), with identical non-owning behaviour and sub-ranging. Its remaining distinction, immutability, is a **binding** property in Nitpick, not a type property, so an immutable byte view is `fixed uint8[]`. Redundant twice over. `buffer` is retained: a slice cannot own. |
| **`move` moved** from `MemoryQualifier` to `ControlFlow` | D-065 — it is not a qualifier. `move(place)` is a keyword operator with a parenthesized operand, the same shape as `comptime(expr)`, and it belongs beside the other ownership keywords `drop` and `nodrop`. |
| **`relay` and `_^` added** | D-080 — the language had **no way to propagate an error**. With every function returning `Result<T>`, propagation is the most common operation there is, and its absence pushed callers toward `raw` (bypasses the discipline), `?!` (escalates a recoverable error to shutdown), or `?` with a default (silent success). `relay` forwards the code verbatim and runs `defer`. |
| `Self` added | D-030 — used six times in `FORMAL_DRAFT` 13 but never declared a keyword |
| `NIL` added to `BuiltinType` | It is a type as well as a value — `func:reset = NIL(Ast->:a)` — and was listed only among the sentinels, so the type parser refused a spelling the compiler's own sources use on nearly every mutating function (0.2.5) |
| `cstring` added to `BuiltinType` | D-049 — `AST_REFERENCE.md` §4 declares a `CStringType` node and `TYPE_REFERENCE.md` §3.2.1 writes `cstring:cs = "Hello";`, but the production never listed it, so `cstring` lexed as an identifier, the node was unreachable, and a user type of that name would have silently shadowed the builtin (0.2.8) |
| — | `for` is **not** duplicated: it is one reserved token already in `ControlFlow`, used in two grammatical positions (see below) |

## 5. Operators and Punctuation

```ebnf
Operator ::= "+" | "-" | "*" | "/" | "%" | "++" | "--"
           | "=" | "+=" | "-=" | "*=" | "/=" | "%="
           | "&=" | "|=" | "^=" | "<<=" | ">>="
           | "==" | "!=" | "<" | "<=" | ">" | ">=" | "<=>"
           | "&&" | "||" | "!"
           | "&" | "|" | "^" | "~" | "<<" | ">>"
           | "->" | "<-" | "=>" | "=>!"
           | "@" | "$" | "$$i" | "$$m"
           | "?" | "?." | "??" | "?!" | "?|" | "_?" | "_!" | "_~" | "_^"
           | "!!!" | "|>" | "<|" | ".." | "..." | "..*" | "..^"

CompilerSigil ::= "#"

Punctuation   ::= "(" | ")" | "{" | "}" | "[" | "]" | "." | "," | ":" | ";" | "`"
```

> **`=>!` added** — chapter 01 listed only `=>`, but `=>` and `=>!` are the *only*
> two cast forms (D-021).
>
> **`#` is not a value operator.** It is the **compiler-directive sigil**:
> `#name<T>(...)` for builtins, `#[name]` for attributes (D-020). It was formerly
> the pin operator, which is obsolete without a collector. The lexer must
> distinguish `#` followed by `[` (attribute) from `#` followed by an identifier
> (builtin).
>
> **Direction is semantic** in this operator set: `->` points *to* a target, `<-`
> brings a value *back*, `=>` goes *from* one type *to* another.

### 5.1 The range and spread family

These four share a dot prefix because they are one family, not four unrelated
symbols:

| Token | Meaning | Where it appears |
|---|---|---|
| `..` | inclusive range `[a, b]` | expression |
| `...` | exclusive range `[a, b)` | expression |
| `..*` | variadic rest marker — **collects** arguments | declaration site |
| `..^` | spread — **expands** a collection into arguments | call site |

`..*` and `..^` are inverses. Confirmed against the prototype:
`parser.cpp:2582` (`// Check for spread operator: ..^expr`) and
`expr.cpp:248`; the token also exists in `npkc-native/src/frontend/token.npk`.

This is why `FORMAL_DRAFT` 04 §4.2 names precedence level 8 "Range / **Spread**".

### 5.2 `>>` and nested generics

`>>` is the right-shift operator **and** the closing bracket pair of a nested
generic — `Handle<Node<int64>>`. The lexer must split `>>` into two `>` tokens
when the parser is in a type-argument context.

This is a known interaction rather than an accident, and it is stated here so it
is not rediscovered during implementation (D-030).

**A type-argument context is opened by a type position or by `::<`, and nowhere
else** (D-064). Explicit type arguments in expression position are always written
with the turbofish, so there is no case in which the lexer must guess: `>>`
splits inside a type-argument list and is a right-shift everywhere outside one.
No lookahead and no speculative parse are required.

### 5.3 The two meanings of `!`

`!` is **lexically** disambiguated by position — a reader can tell which meaning
applies from the token alone, without knowing what surrounds it (D-046):

| Position | Meaning | Forms |
|---|---|---|
| **leading** | negation | `!x`, `!=` |
| **trailing or repeated** | unchecked / emphatic | `?!`, `=>!`, `_!`, `!!!` |

**`!!` no longer exists.** After D-001 removed `sys!!!` and `asm!!!`, the tier
marker distinguished nothing: `asm` is now the only assembly form, and the
full-tier syscall is spelled **`sys_full`** — a word that states the tier as
clearly as `!!` and matches `libn`'s existing `sys_safe` / `sys_full` naming.

**Macro invocation is `#name(args)`**, not `name!(args)` (D-046). That was the
one genuine collision — `foo!(x)` is indistinguishable from an emphatic
operation and carries no positional cue. A macro is a compile-time construct
addressed to the compiler, so it belongs under the `#` sigil alongside
`#size_of<T>` and `#[derive(…)]`.

## 6. Literals

### 6.1 Sentinels and Booleans

```ebnf
BooleanLiteral  ::= "true" | "false"
SentinelLiteral ::= "NULL" | "NIL" | "ERR"
```

> **`unknown` is not a literal.** It is a compiler-assigned taint on
> `Result.value` after `fail()`, not something the programmer writes
> (`TYPE_REFERENCE.md` §27). Chapter 01 listed it as a `SentinelLiteral`; that is
> the prototype's broader `unknown`, which was narrowed.
>
> `ERR` **is** writable — it is the `tbb` sentinel and appears as a `pick` match
> label (D-008 §5.1).

### 6.2 Numeric Literals

Underscores are permitted for readability and ignored. Base is given by a
**suffix**, uniform across all bases. Every numeric literal **begins with a
decimal digit** `0`–`9`, and no identifier ever does (D-147); a value whose
leading significant digit is a letter takes a value-neutral leading zero --
`0FFhex`, `0Tt`, `0an`.

```ebnf
NumericLiteral ::= IntegerLiteral | FloatLiteral

IntegerLiteral ::= (DecimalLiteral | HexLiteral | BinaryLiteral
                   | OctalLiteral | TernaryLiteral | NonaryLiteral) TypeSuffix?

DecimalLiteral ::= [0-9] ([0-9_]* [0-9])?
HexLiteral     ::= [0-9] ([0-9a-fA-F_]* [0-9a-fA-F])? "hex"
BinaryLiteral  ::= [01] ([01_]* [01])? "bin"
OctalLiteral   ::= [0-7] ([0-7_]* [0-7])? "oct"

/* Balanced ternary: T/t denotes -1 */
TernaryLiteral ::= [01] ([01Tt_]* [01Tt])? ("t" | "ter" | "tri")

/* Balanced nonary: A..D / a..d denote -1..-4 */
NonaryLiteral  ::= [0-4] ([0-4a-dA-D_]* [0-4a-dA-D])? ("non" | "n")

FloatLiteral   ::= DecimalLiteral "." DecimalLiteral Exponent? TypeSuffix?
Exponent       ::= [eE] [+-]? DecimalLiteral

TypeSuffix     ::= "u1" | "u2" | "u4" | "u8" | "u16" | "u32" | "u64" | "u128"
                 | "u256" | "u512" | "u1024" | "u2048" | "u4096"
                 | "i1" | "i2" | "i4" | "i8" | "i16" | "i32" | "i64" | "i128"
                 | "i256" | "i512" | "i1024" | "i2048" | "i4096"
                 | "tbb8" | "tbb16" | "tbb32" | "tbb64" | "tbb128" | "tbb256"
                 | "f32" | "f64" | "f128"
                 | "tfp32" | "tfp64" | "tfp128" | "tfp256" | "dim256"
                 | "char8" | "char16" | "char32"
```

> **D-148 — the literal envelope.** A numeric literal's value lies in the
> signed 64-bit envelope, verified EXACTLY at scan time (`NITPICK-LEX-004`),
> and must fit its type — suffixed or contextual — verified at the literal
> (`NITPICK-TYPE-031`). Values outside the envelope are **constructed, not
> spelled**: `uint64` above 2⁶³−1 (`0u64 - 1u64` is the maximum), a signed
> width's most negative value in decimal (spell it in a balanced base:
> `0b4bni8` is −128), and the wide integers. One rule for every extreme.
>
> **D-147 — the leading-digit rule.** The token class is decided by the first
> character alone: a literal begins `0`–`9`, an identifier never does. `FFhex`,
> `an`, `ban`, `tt` are ordinary identifiers; the values they used to spell are
> written `0FFhex`, `0an`, `0ban`, `0tt`. Before the rule, the letter-digit
> bases made whole English words into numbers, decided by suffix-stripping from
> the right — a collision that repeatedly cost edit-build-fail cycles.
>
> The **legacy C-style prefixes** (`0x`, `0b`, `0o`, `0n`), previously retained
> for C FFI compatibility, were **removed** by the same decision. Nitpick never
> parses C headers, and two spellings for one literal is what the blueprint
> philosophy refuses. `0xFF` is a bad-digit error at the `x`
> (`NITPICK-LEX-003`).
>
> **Ternary/nonary use the suffix form**, not the `0t…` / `0n…` prefix form shown
> in `FORMAL_DRAFT` 02 §2.4. The two chapters disagreed; the suffix form wins for
> consistency with every other base.
>
> **LBIM literals**: `int2048` and `int4096` have no direct source literal. They
> are instantiated by parsing, e.g. `parse_uint2048("1.5e308")`.

### 6.3 String and Character Literals

```ebnf
StringLiteral      ::= '"' StringCharacter* '"'
StringCharacter    ::= (SourceCharacter - ('"' | "\")) | EscapeSequence
EscapeSequence     ::= "\" ("n" | "r" | "t" | "\" | '"' | "'" | "0"
                            | "x" HexDigit HexDigit
                            | "u" "{" HexDigit+ "}")

RawStringLiteral   ::= "r" '"' (SourceCharacter - '"')* '"'
BlockStringLiteral ::= '"""' (SourceCharacter - '"""')* '"""'

CharacterLiteral   ::= "'" ((SourceCharacter - ("'" | "\")) | EscapeSequence) "'"
```

> **Raw and block strings are retained** (D-024). `FORMAL_DRAFT` 01 omitted them
> on the grounds that the v0.61.82 parser rejected them — a statement about the
> prototype's implementation, not a design decision. `OP_REFERENCE.md` §9 lists
> both as current, and raw strings matter for regex patterns and paths.
>
> `RawStringLiteral` performs **no** escape processing. `BlockStringLiteral`
> preserves newlines and indentation verbatim.

### 6.4 Template Literals

Backtick-delimited, with `&{ … }` interpolation. The lexer decomposes a template
into `TEMPLATE_START`, `TEMPLATE_PART`, `INTERP_START` (`&{`), `INTERP_END` (`}`),
and `TEMPLATE_END`; the embedded expressions are parsed by the syntactic grammar.

```ebnf
TemplateLiteral ::= "`" TemplateContent "`"
TemplateContent ::= (SourceCharacter - ("`" | "&{"))* (Interpolation TemplateContent)?
Interpolation   ::= "&{" /* syntactic expression */ "}"
```

---

## Open items

- ~~**`dim256` rename is not yet applied project-wide.**~~ — **applied.** D-036
  settles `tfp` and `dim` as distinct types with `fix256` obsolete; no live spec
  in `meta/specs/` says `fix256` any longer. `FORMAL_DRAFT` still does, and is
  read-only reference — recorded in `PROTOTYPE_DELTA.md` §4.
- ~~**`move` is listed as a memory qualifier but is not specified anywhere.**~~ —
  **settled by D-065: it is not a qualifier.** `move(place)` is a keyword operator
  with a parenthesized operand, exactly the shape `comptime(expr)` already has. It
  is removed from `MemoryQualifier`, which leaves the four the memory model
  actually has. Ownership transfers only where `move` is written — never
  implicitly — and the moved-from binding is invalid until reinitialized, not
  "valid but unspecified".
- ~~`for` occupies two grammatical roles~~ — **closed by D-031.** `impl` now takes
  no connector (`impl:Message:Serializable`), so `for` reverts to the loop keyword
  alone and has exactly one meaning.
