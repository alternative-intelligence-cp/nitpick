# Cycle 0.2 — AST and parser

`AST_REFERENCE.md` in full, and a recursive-descent parser over the token stream
cycle 0.1 produces.

**This is the largest cycle in the frontend, and the one the whole bootstrap
strategy exists to protect.** `CLAUDE.md`: the frontend is built once, in full,
because rewriting the parser at every bootstrap stage is the failure mode the
predecessors hit repeatedly. So the parser accepts the **whole grammar** from the
first commit — generics, traits, `async`, contracts, macros — even though no
backend rung can lower most of it.

> **The parser never restricts. The backend does.** (D-085)

`tests/rejection/` has been enforcing that since cycle 0.0 against the seed. From
here it enforces it against the real parser, which is what it was written for.

## Subcycles

| | Topic |
|---|---|
| **0.2.0** | AST representation — node storage, typed handles, spans |
| **0.2.1** | Parser core — cursor, expectation, and the recovery strategy |
| **0.2.2** | Types — including generic arguments and the `>>` split |
| **0.2.3** | Expressions — the 19-level precedence table |
| **0.2.4** | Statements — every form in `AST_REFERENCE.md` §2 |
| **0.2.5** | Declarations — plus contracts and attributes |
| **0.2.6** | `pick` patterns, guards, and destructuring |
| **0.2.7** | Parser diagnostics, recovery, and the suites |
| **0.2.8** | The type grammar's unreachable corners — added by 0.2.4 |

## What the AST must be

`SUBSET_1.md` §1.2 and `CLAUDE.md` together fix the shape:

- **Tagged enums over composable structs**, not base/derived nodes — Nitpick has
  no inheritance, and this is the transferable technique the predecessor
  documented.
- **Index-based, never pointer-based.** A node refers to another by an index into
  a node array. That is easier to verify than a pointer graph, it survives array
  growth, and it is what lets an enum payload stay one machine word.
- **Every node carries a `Span`** (`AST_REFERENCE.md` "Conventions").

## The one thing to get right early

`tokenlist_split_shr` gets its caller in **0.2.2**. The lexer emits `>>` whole
because a type position is not lexically detectable; the parser splits it when it
needs a `>` to close a type-argument list. That is the mechanism D-064 bought by
confining type-argument context to a type position or `::<` and nowhere else —
no feedback channel, no speculative parse.

## Why 0.2.8 exists

It was not in the original plan. **0.2.4 compared `AST_REFERENCE.md`'s node
tables against the kind tables generated from them**, and the comparison found
four nodes the parser could not build, could not reach, or built wrongly —
including two the document declares only in prose and one, `FuncType`, that no
document in the set gives a spelling for.

Two lessons are worth carrying into later cycles. First, **the generator reads
tables and the document also says things in prose**; anything declared outside a
table is invisible to it, which is how `FallStmt`, `GiveStmt` and
`IdentifierExpr` went missing. Second, and more useful: **diff the two lists
mechanically.** Every one of the four was found in a few seconds by a script
comparing names in the spec against variants in the generated enum, and none of
them would have surfaced from reading either file on its own.
