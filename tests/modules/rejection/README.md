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
| `owned_names.npk` | `NITPICK-RESOLVE-001` — a name the compiler (`Error`) or the prelude owns, declared by the program: at module scope, as an associated type, as a generic parameter (D-239) |
| `limit_names.npk` | `NITPICK-RESOLVE-002` and `NITPICK-RESOLVE-011` — a `limit<name>` naming nothing (on a local, on a parameter, in a `Rules` refinement) and one naming a function or a struct instead of a `Rules` block (D-220, 1.5.1) |
| `rule_cycle.npk` | `NITPICK-RESOLVE-006` — a `Rules` block refining itself, directly or through a chain (D-220, 1.5.1) |
| `header_missing.npk` | `NITPICK-RESOLVE-012` — a file with no header: its first declaration is not `mod:<basename>;` (D-248, 1.5.1b) |
| `header_mismatch.npk` (+ `header_sibling.npk`) | `NITPICK-RESOLVE-012` — a header naming a SIBLING that exists: refused at the header, and the sibling is NOT loaded (the workbench's DEF-2; D-248) |
| `entry_in_module.npk` (+ `entry_lib.npk`) | `NITPICK-RESOLVE-013` — `main`/`failsafe` declared in an imported module, and in an inline module of the root: entry points are the root's top level alone (D-248) |

Ten codes, fourteen files (RESOLVE-001 has two: the plain duplicate, and the
owned name — one code, because each is "this name already means something
here"; two of the fourteen are fixtures another file imports). A code with no
test is a diagnostic nobody has ever seen produced.
