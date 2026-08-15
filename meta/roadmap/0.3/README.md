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

## Two decisions this cycle must settle first

Both are named in 0.3.0 and neither can be worked around.

### How a Nitpick program receives its command-line arguments

**Nothing in the spec set says.** `main` is written `func:main = int32()`
everywhere it appears — `VERIFICATION_REFERENCE.md`, `CONCURRENCY_REFERENCE.md`,
`TYPE_REFERENCE.md` §715 — with no parameters and no alternative form. A compiler
driver needs to know what to compile.

**Recommendation: `main` keeps its signature, and arguments come from a runtime
function.** `main` is already special — a bare `int32`, no `Result`, `exit` only
— and giving it a second, optional-parameter form makes it two shapes for one
thing, which is exactly what D-088 just finished removing elsewhere. Command-line
arguments are process state like the environment and the working directory; they
belong to `nlibc`, not to the language.

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

## What "done" looks like

A driver that takes a root `.npk` file, loads its whole module graph, and reports
every unresolved name, every private access, and every ambiguous path — with
spans. It still emits nothing; that is Phase B.

## The habit carried over from 0.2

**Diff the two lists mechanically.** Sixteen defects surfaced in cycle 0.2 and
almost none announced itself: a kind nothing constructs is never an error, and a
rejection suite aimed at the wrong parser passes cheerfully. `check_kinds_reachable`
and `tests/grammar/` now run on every harness invocation because of it.

The equivalent question here is worth asking early, since the answer is not
obvious: **what is the mechanical check that a symbol table is complete?** A
plausible one is that every `Ident` token in every parsed file resolves to
exactly one symbol or to exactly one diagnostic — no third outcome, and nothing
silently unbound. If that holds, it belongs in the harness beside the other two.
