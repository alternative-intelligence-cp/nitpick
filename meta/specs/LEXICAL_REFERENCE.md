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
                      | "fail" | "exit" | "raw" | "drop" | "nodrop" | "ok"
                      | "defaults" | "discard" | "move"

VerificationKeyword ::= "prove" | "assert_static" | "requires" | "ensures"
                      | "invariant" | "fails" | "on" | "with" | "never"

AsyncKeyword        ::= "async" | "await"

ModuleKeyword       ::= "use" | "mod" | "pub" | "extern" | "cfg" | "as"
                      | "comptime" | "inline" | "noinline" | "macro" | "derive"

TypeKeyword         ::= "struct" | "enum" | "Type" | "assoc" | "opaque"
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
                      | "fd" | "pid" | "tid" | "uid" | "gid"
                      | "dyn" | "any" | "Result" | "Optional"
                      | "Handle" | "arena" | "shared_arena" | "atomic" | "Future"
                      | "simd" | "complex" | "array" | "func"
                      | "trit" | "tryte" | "nit" | "nyte"
                      | "vec2" | "vec3" | "vec9" | "matrix" | "tmatrix"
                      | "tensor" | "ttensor" | "binary" | "buffer" | "stream"
                      | "process" | "pipe" | "debug" | "log"

BuiltinHelper       ::= "is" | "in" | "is_err"
```

### Corrections applied

| Change | Reason |
|---|---|
| `gc` removed from `MemoryQualifier` | D-003 — no collector |
| `fails`, `on`, `with`, `never` added | D-002 — FFI error contracts. `with` binds the error source (`with errno`) and was missed in the first pass. |
| `is_err` added | D-008 — non-trapping ERR test |
| `discard` added to `ControlFlow` | it is a statement keyword and was absent |
| `tbb128`, `tbb256` added | `TYPE_REFERENCE.md` §6 defines them |
| `fix256` → `dim256` | D-036 — `dim256` has dimensional analysis; `fix256` is its obsolete name. Also removes the `fix` / `fixed` near-miss, `fixed` being the immutability qualifier. |
| `tfp128`, `tfp256` added | D-036 — `TYPE_REFERENCE.md` §5 defines four widths; only two were listed |
| `char8/16/32` added | `TYPE_REFERENCE.md` §2 — semantically distinct from `uint8` |
| `Handle`, `arena`, `shared_arena`, `atomic`, `Future`, `Optional`, `simd`, `complex` added | all specified in `TYPE_REFERENCE.md`; all were missing |
| **35 `a*` collection keywords removed** | D-041 — `astack`, `alist`, `ahash`, `astringlist` and their operations are not language builtins; collections belong in a library. Returns 35 reserved words to userland and removes the last major `aria` naming artifact from the language surface. |
| `fd`, `pid`, `tid`, `uid`, `gid` added | D-042 — kernel identifiers are distinct types permitting comparison but not arithmetic. Combined with `Result<T>`, an `fd` is always valid: POSIX's `-1` goes to `Result.error` and is not representable. |
| `assoc` added | D-028 — declares an associated type; `Type` is namespace-only |
| **`move` moved** from `MemoryQualifier` to `ControlFlow` | D-065 — it is not a qualifier. `move(place)` is a keyword operator with a parenthesized operand, the same shape as `comptime(expr)`, and it belongs beside the other ownership keywords `drop` and `nodrop`. |
| `Self` added | D-030 — used six times in `FORMAL_DRAFT` 13 but never declared a keyword |
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
           | "?" | "?." | "??" | "?!" | "?|" | "_?" | "_!" | "_~"
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
**suffix**, uniform across all bases.

```ebnf
NumericLiteral ::= IntegerLiteral | FloatLiteral

IntegerLiteral ::= (DecimalLiteral | HexLiteral | BinaryLiteral
                   | OctalLiteral | TernaryLiteral | NonaryLiteral) TypeSuffix?

DecimalLiteral ::= [0-9] ([0-9_]* [0-9])?
HexLiteral     ::= [0-9a-fA-F] ([0-9a-fA-F_]* [0-9a-fA-F])? "hex"
BinaryLiteral  ::= [01] ([01_]* [01])? "bin"
OctalLiteral   ::= [0-7] ([0-7_]* [0-7])? "oct"

/* Balanced ternary: T/t denotes -1 */
TernaryLiteral ::= [01Tt] ([01Tt_]* [01Tt])? ("t" | "ter" | "tri")

/* Balanced nonary: A..D / a..d denote -1..-4 */
NonaryLiteral  ::= [0-4a-dA-D] ([0-4a-dA-D_]* [0-4a-dA-D])? ("non" | "n")

FloatLiteral   ::= DecimalLiteral "." DecimalLiteral Exponent? TypeSuffix?
Exponent       ::= [eE] [+-]? DecimalLiteral

TypeSuffix     ::= "u1" | "u2" | "u4" | "u8" | "u16" | "u32" | "u64" | "u128"
                 | "u256" | "u512" | "u1024" | "u2048" | "u4096"
                 | "i1" | "i2" | "i4" | "i8" | "i16" | "i32" | "i64" | "i128"
                 | "i256" | "i512" | "i1024" | "i2048" | "i4096"
                 | "tbb8" | "tbb16" | "tbb32" | "tbb64" | "tbb128" | "tbb256"
                 | "f32" | "f64" | "f128" | "f256" | "f512"
                 | "tfp32" | "tfp64" | "tfp128" | "tfp256" | "dim256"
                 | "char8" | "char16" | "char32"
```

> **Legacy C-style prefixes** (`0x`, `0b`, `0o`, `0n`) are retained solely for C
> FFI compatibility and are discouraged in native Nitpick code. The suffix forms
> are canonical — one rule, applied to every base.
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
