# The `raw`/`drop` licence worklist (D-163, measured at 1.1.0)

The first REAL numbers, replacing the plan's grep heuristic (262 `raw` /
741 `drop` / ~868 clauses). Produced by `check <root> --raw-report` — the real
frontend, so UFCS and imports resolve correctly — over `src/main.npk`, the
three `tools/` roots, and every runnable file in `tests/accept`,
`tests/conformance`, `tests/backend/programs`, and `tests/frontend`.
`check_raw_licensed` reprints the `src/` and `tests/` numbers on every full
harness run until they reach zero.

A SITE is one `raw` or `drop` whose operand's resolved callee does not declare
`never fails`. Marking ONE callee licenses every site that calls it — the
~8,900 `src/` sites concentrate onto far fewer callees (the table accessors
alone are hundreds of sites each) — so 1.1.1's work is measured in callees,
tracked here in sites.

| Tree | may-fail sites | builtin sites | other |
|---|---|---|---|
| src | 8921 | 100 | 0 |
| lib | 11 | 1 | 0 |
| prelude | 54 | 17 | 0 |
| tools | 22 | 3 | 0 |
| tests | 989 | 117 | 5 |
| derive-generated | 7 | 0 | 0 |
| **total** | **10004** | **238** | **5** |

- **builtin** sites are licensed by the BUILTIN table's `fails` column
  (`builtin_never_fails`, generated from `BUILTIN_REFERENCE.md`), not by a
  declaration — 21 of the 33 bare-name builtins are `never fails`.
- **other** is a site whose operand is not a resolvable call (an indirect call
  through a function value, or a non-call operand) — resolved by hand in the
  sweep.
- The `tools/` roots each import all of `src/`, so their in-harness runs are
  skipped (three ~45s runs for three files' own sites); their numbers here are
  the one-off 1.1.0 measurement.

## `src/` by file (the 1.1.1 sweep, descending)

| File | may-fail sites |
|---|---|
| `src/backend/ir/ir_expr.npk` | 1343 |
| `src/backend/ir/ir_stmt.npk` | 551 |
| `src/frontend/macro/expand.npk` | 537 |
| `src/frontend/type_expr.npk` | 499 |
| `src/frontend/resolve_type.npk` | 474 |
| `src/frontend/type_access.npk` | 410 |
| `src/frontend/parse_decl.npk` | 378 |
| `src/frontend/analysis/escape.npk` | 376 |
| `src/frontend/analysis/bindings.npk` | 306 |
| `src/frontend/type_trait.npk` | 268 |
| `src/frontend/type_stmt.npk` | 250 |
| `src/frontend/resolve.npk` | 241 |
| `src/backend/emit_program.npk` | 227 |
| `src/frontend/lexer.npk` | 197 |
| `src/frontend/analysis/locks.npk` | 195 |
| `src/frontend/analysis/exhaust.npk` | 191 |
| `src/backend/ir/ir_types.npk` | 188 |
| `src/frontend/module_graph.npk` | 153 |
| `src/frontend/keywords.npk` | 153 |
| `src/frontend/types.npk` | 138 |
| `src/frontend/macro/derive.npk` | 133 |
| `src/frontend/parse_expr.npk` | 131 |
| `src/frontend/builtin_types.npk` | 124 |
| `src/frontend/type_layout.npk` | 121 |
| `src/driver/pipeline.npk` | 118 |
| `src/frontend/parse_stmt.npk` | 114 |
| `src/frontend/type_cast.npk` | 112 |
| `src/frontend/num_width.npk` | 81 |
| `src/frontend/type_generic.npk` | 80 |
| `src/backend/ir/ir_runtime.npk` | 78 |
| `src/frontend/operators.npk` | 70 |
| `src/frontend/parse_pick.npk` | 68 |
| `src/frontend/type_names.npk` | 63 |
| `src/frontend/ast.npk` | 59 |
| `src/frontend/builtins.npk` | 58 |
| `src/frontend/symbols.npk` | 55 |
| `src/frontend/type_decl.npk` | 54 |
| `src/backend/ir/ir_func.npk` | 37 |
| `src/frontend/parse_type.npk` | 35 |
| `src/frontend/parse_decorate.npk` | 35 |
| `src/frontend/escapes.npk` | 30 |
| `src/frontend/numeric.npk` | 28 |
| `src/main.npk` | 27 |
| `src/backend/ir/ir_writer.npk` | 27 |
| `src/frontend/parser.npk` | 27 |
| `src/frontend/paths.npk` | 20 |
| `src/frontend/resolve_path.npk` | 17 |
| `src/frontend/diagnostics.npk` | 16 |
| `src/frontend/prelude.npk` | 8 |
| `src/frontend/expr_types.npk` | 7 |
| `src/frontend/token.npk` | 6 |
| `src/frontend/source.npk` | 4 |
| `src/frontend/intern.npk` | 3 |

## `tests/` by file (the 1.1.2 sweep, descending, top 25)

| File | may-fail sites |
|---|---|
| `tests/frontend/parse_stmts.npk` | 228 |
| `tests/frontend/parse_types.npk` | 148 |
| `tests/frontend/parse_recovery.npk` | 89 |
| `tests/frontend/types.npk` | 58 |
| `tests/frontend/parser_core.npk` | 57 |
| `tests/frontend/lexer_diagnostics.npk` | 53 |
| `tests/frontend/lexer_operators.npk` | 45 |
| `tests/frontend/lexer_numeric.npk` | 37 |
| `tests/frontend/lexer_idents.npk` | 29 |
| `tests/frontend/source_intern.npk` | 24 |
| `tests/backend/programs/dyn_slots.npk` | 22 |
| `tests/backend/programs/generic_list.npk` | 16 |
| `tests/backend/programs/string_lib.npk` | 14 |
| `tests/accept/lock_levels.npk` | 13 |
| `tests/backend/programs/line_discipline.npk` | 12 |
| `tests/accept/deriving.npk` | 10 |
| `tests/accept/folding.npk` | 10 |
| `tests/accept/expansion.npk` | 9 |
| `tests/accept/borrows.npk` | 8 |
| `tests/backend/programs/dyn_vtable.npk` | 8 |
| `tests/backend/programs/impl_symbols.npk` | 7 |
| `tests/backend/programs/derive_hash.npk` | 6 |
| `tests/backend/programs/optional.npk` | 6 |
| `tests/backend/programs/type_group.npk` | 5 |
| `tests/backend/programs/defer_order.npk` | 4 |

Owned by **1.1.1** (`src/`, `lib/`, `tools/`, the prelude) and **1.1.2**
(`tests/`, then the flip to refusal). The instrument retires when both sweeps
hold zero and `TYPE_RAW_UNLICENSED` lands (1.1.2).
