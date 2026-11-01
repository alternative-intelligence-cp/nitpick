# tests/parked/def413-flow -- programs refused by DEF-413's closed rule, waiting for the flow relaxation

Parked at landing 176 (2026-10-31; DEF-413, management_11's ruling of 21:50). A plain address a binding holds
(`string->:p = @x;`) now conflicts with a `move` of its pointee anywhere in the holder's block (BORROW-013,
`tests/analysis/rejection/held_addr.npk`): the rule is lexical and judged by the root, the closed choice. These
programs are DEF-159's idiom (landing 105) -- `x` moved out past a held `@x`, a new value stored back through
the pointer, the scope exit dropping it -- legal and measured exit 0 on every landing from 105 to 175, and
refused by the closed rule although every one of them WRITES the pointee whole before reading it again.

| file | was | returns when |
|---|---|---|
| `held_addr_store.npk` | `tests/backend/programs/` (the six roads) | the flow relaxation after landing 180 (OPEN_DECISIONS, DEF-413's row): a holder written whole before its next read is no conflict |
| `held_addr_once.npk`, `held_addr_churn.npk` | `tests/backend/programs/` (the once/churn pair) | with the above; `held_addr.toml` goes back to `tests/cost/` with them (its paths are the original ones) |
| `deref_store_vacate.npk` | case 6 of `tests/backend/programs/deref_store_drop.npk` (`case_vacate`) | with the above, back into that file as case 6 |

Nothing lists `tests/parked/`: no `[[test]]` of `nitpick.toml` names it, and the cost units are read from
`tests/cost/` alone. The files are kept whole (header, `mod:`, `main`, `failsafe`) so that `git mv` restores
them; each still compiles and runs exit 0 under landing 175's compiler (`wt/54b/.internal/base/npkc`).
