# Cycle 0.1 — Lexer

`LEXICAL_REFERENCE.md` in full, written in Nitpick, compiled by the seed.

**Built once, and never rewritten.** The capability ladder makes the frontend a
single build; the backend is what grows rung by rung. So this cycle produces the
*whole* lexer — every keyword, every literal form, every operator — not the
subset the current backend can lower. A construct the backend cannot yet handle
still **lexes**; it is rejected later, in the backend, with `NITPICK-RUNG-001`.

> **The parser never restricts. The backend does.** (D-085)

That rule starts here. A lexer that only recognises what today's rung supports is
a partial grammar by another name, and re-widening it stage by stage is what
ended `nitpick-bootstrap`.

## Subcycles

| | Topic |
|---|---|
| **0.1.0** | Source manager, the intern table, and `#size_of<T>` |
| **0.1.1** | Token representation — kinds, `Token`, `TokenList` |
| **0.1.2** | Whitespace, comments, identifiers, keywords |
| **0.1.3** | Numeric literals — every width, radix, and the balanced forms |
| **0.1.4** | String, character, and template literals |
| **0.1.5** | Operators, and the three interaction rules |
| **0.1.6** | Lexical diagnostics, and the suites |

## Written test-first

Cycle 0.0 built the harness precisely so this cycle could be. Each subcycle adds
its conformance cases and its **rejection** cases together with the code, and the
rejection cases are the ones that pin the specification — the error cases are
where the real definition lives, which is how D-057, D-064 and D-066 were
recovered in the first place.
