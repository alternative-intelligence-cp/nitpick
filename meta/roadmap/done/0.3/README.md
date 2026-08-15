# Cycle 0.3 — Modules, symbols, visibility

`MODULE_REFERENCE.md` and `BUILD_REFERENCE.md` §3, written in Nitpick.

Cycle 0.2 produced a parser that turns **one file** into an AST. This cycle turns
**a program** into a symbol table: it finds the files, loads them, collects every
declaration, and binds every name to the thing it names.

## Where the frontend stands, and what changes

Until now the compiler has never opened a file. `tools/parse_check.npk` reads
stdin because that needed no path handling and no argv, and that was the honest
scope at the time. A module loader cannot dodge either.

So this cycle starts by giving the frontend the two things it has never had —
**a way to name a file and a way to read one** — and only then builds the graph
on top.

## The obligations D-086 already placed on this cycle

A `use` cycle among modules is **legal**. That decision was forced by 0.2.6, and
it landed three requirements here:

1. **Collect every declaration in every module in the graph before resolving any
   body.** The same two-phase load that already lets a function refer to one
   declared below it in the same file, now applied across the graph.
2. **Report a cycle only when it is genuinely unresolvable** — a struct whose
   size depends on itself other than through a pointer, a `const` whose
   initialiser depends on itself. The diagnostic names the members in the order
   they refer to each other. "Circular import" is not something a reader can act
   on.
3. **Never make resolution order-dependent.** The same module graph must produce
   the same program regardless of which member the loader entered first.

The third is the one to design for rather than test for afterwards. It is a
**reproducibility** requirement before it is a convenience: a build that depends
on entry order differs between the stage-1 and stage-2 compilers, and the
bootstrap fixpoint check (D-085) would then fail for a reason that has nothing to
do with correctness. Chasing that would be a genuinely awful week.

## Two decisions this cycle had to settle first — both now settled

### How a Nitpick program receives its command-line arguments — **D-089**

**Nothing in the spec set said.** `main` is written `func:main = int32()`
everywhere it appears, with no parameters and no alternative form.

This plan recommended that `main` keep that signature and that arguments come
from a runtime function. **That recommendation was wrong, and the way it was
wrong is the lesson.** The spec set had *lost* the parameter form; the prototype
has it, and so does the origin of the `_~` operator, which exists because `main`'s
unused `argc` and `argv` produced warnings with no placeholder to silence them.

The settled form is `func:main = int32(cstring[]:argv)` — one parameter, always,
with no `argc` because a slice carries its length. **Absence from `meta/specs/` is
not evidence of absence**: the set was assembled partly from a verbal retelling
and has dropped real things silently. Check the prototype.

### The file-reading primitive

`read_stdin` was added in 0.2.7 and is not enough. Reading a file by path is the
first thing the loader does.

**Recommendation: one `read_file(path) → Result<string>` in the runtime floor**,
not separate `open`/`read`/`close`. `SUBSET_1.md`'s governing principle is *push
complexity into the compiler, away from the seed* — the seed is the least-audited
artifact in the chain, and three primitives with an fd protocol between them is
more of it than one call that returns bytes. The real I/O model is
`IO_REFERENCE.md`'s `Stream` trait (D-075) and arrives with `nlibc` in 0.8.

## Subcycles

| | Topic |
|---|---|
| **0.3.0** | Reading a file — the two decisions above, `read_file`, and a source manager that loads by path |
| **0.3.1** | The symbol table — what a symbol is, scopes, and the two-phase collect |
| **0.3.2** | Path resolution — the three `use` forms, dependency roots, ambiguity-is-an-error |
| **0.3.3** | The module graph — transitive load, legal cycles, and order independence |
| **0.3.4** | Visibility and imports — `pub`, the four import kinds, non-transitivity, `pub use` |
| **0.3.5** | Name resolution — binding every identifier to the symbol it names |
| **0.3.6** | Diagnostics, the suites, and closing the cycle |

## What the cycle produced

`src/frontend/` gained `symbols.npk`, `paths.npk`, `resolve_path.npk`,
`module_graph.npk`, `resolve.npk`, `resolve_codes.npk` and a generated
`builtins.npk`; `tools/resolve_check.npk` runs the whole frontend on a real
program. `tests/modules/` is the first multi-file fixture directory — every
suite before it was single-file, which a module system cannot be tested from.

**Three passes, and the order is the architecture:**

1. **Load and collect.** Each module parsed and its declarations recorded, needing
   nothing from any other module. This is what lets the graph contain a cycle.
2. **Bind imports, to a fixed point.** Not one pass: `pub use` means a module's
   exports can depend on another's, and with cycles legal there is no dependency
   order to visit in. The fixed point is what makes the result independent of
   which module was entered first.
3. **Resolve bodies.** Every identifier bound, against a table that is already
   complete.

Each stage needs the previous one finished *for every module*. That is D-086
working, rather than being worked around.

## What "done" looks like

A driver that takes a root `.npk` file, loads its whole module graph, and reports
every unresolved name, every private access, and every ambiguous path — with
spans. It still emits nothing; that is Phase B.

## The habit carried over from 0.2

**Diff the two lists mechanically.** Sixteen defects surfaced in cycle 0.2 and
almost none announced itself: a kind nothing constructs is never an error, and a
rejection suite aimed at the wrong parser passes cheerfully. `check_kinds_reachable`
and `tests/grammar/` now run on every harness invocation because of it.

The equivalent question here was asked early and answered: **every
`IdentifierExpr` is bound, is a builtin, or produced a diagnostic — no fourth
outcome.** That is `resolve_audit`, and it guards the same class as
`check_kinds_reachable`: if the resolver's walk forgets a construct, the names
inside it are never visited, stay unbound, and nothing reports it until the type
checker asks what one of them means.

**It was verified rather than assumed.** Blinding the walk to `when` statements
makes the audit fire while every other test in the suite still passes — which is
exactly the failure mode it exists for. A check nobody has seen fail is a check
nobody should trust.
