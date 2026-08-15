# `tests/modules/rejection/` — refused by the LOADER

Every file here is a whole program that **loads, or tries to, and is refused**.
`tools/resolve_check.npk` runs it and the harness asserts the codes.

## Why this is a separate suite from `tests/rejection/`

`tests/rejection/` holds files that **parse cleanly and are refused later**, by
the checker, with `NITPICK-RUNG-001` — that is D-085's rule, and the whole point
of that suite is that the parser never restricts.

The failures here happen **earlier**. A file that names a module which does not
exist, or a symbol that is private, never reaches a checker at all. Running both
kinds through one tool would make the two sorts of "correctly refused"
indistinguishable, and telling them apart is exactly what `tests/rejection/`
exists to demonstrate.

## What each file covers

| File | Code |
|---|---|
| `duplicate.npk` | `NITPICK-RESOLVE-001` — two declarations, one name |
| `unknown_name.npk` | `NITPICK-RESOLVE-002` — a name that is nowhere |
| `private_name.npk` | `NITPICK-RESOLVE-003` — importing a private symbol |
| `mod_ambiguous.npk` | `NITPICK-RESOLVE-004` — `name.npk` **and** `name/mod.npk` |
| `no_such_file.npk` | `NITPICK-RESOLVE-005` — a `use` naming nothing |
| `global_cycle.npk` | `NITPICK-RESOLVE-006` — a `const` initialised from itself |
| `no_such_name.npk` | `NITPICK-RESOLVE-007` — importing a name the module lacks |
| `shadowed.npk` | `NITPICK-RESOLVE-008` — a wildcard colliding with a local |

Eight codes, eight files. A code with no test is a diagnostic nobody has ever
seen produced.
