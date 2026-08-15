# `tests/grammar/` — the whole language, parsed and nothing else

Files here are **never compiled and never run.** They are fed to the real parser
(`tools/parse_check.npk`), which must accept every one of them with no
diagnostics at all.

That freedom is the point. `tests/conformance/` must stay inside subset 1 because
the seed has to lower it, and `tests/rejection/` covers only the constructs
somebody thought to write a rejection test for. Neither can exercise a corner of
the grammar that no backend rung will reach for years — a bounded generic, a
blanket `impl`, `..*T[]` variadics, `#[cfg(…)]` on a local declaration.

> **The parser never restricts. The backend does.** (D-085)

`tests/rejection/` demonstrates the first half of that: constructs outside subset
1 parse and are refused later, by the checker, with a code that names the rung.
This directory demonstrates the other half — that the grammar is **complete**,
including the parts nothing else touches.

## Why it matters more than it looks

The frontend is built **once, in full** (`CLAUDE.md`), because rewriting the
parser at every bootstrap stage is the failure mode the predecessors hit
repeatedly. A gap in the grammar does not announce itself: it waits until a
backend rung needs the construct, and by then the parser has been stable for
months and the change is no longer local.

`whole_grammar.npk` found six such gaps the first time it ran — `a++`, `_~ e;`,
the `ERR` sentinel, `vec3(…)`, a qualified return type, and a `pick` arm guard
position. Every one of them was specified, and none had a test.

## Rules for files here

- **Parse-only.** Nothing here needs to typecheck, and most of it does not.
  `Message`, `Renderable` and `Drawable` are never declared, on purpose.
- **Breadth over realism.** A file here should read like an inventory, not like a
  program. If a construct appears in `AST_REFERENCE.md`, it belongs here.
- **Every construct, including the ones that are removed.** `(!)` and
  `impl:Trait:for:Type` do *not* belong here — they belong in
  `tests/rejection/`, where their diagnostics are asserted.
