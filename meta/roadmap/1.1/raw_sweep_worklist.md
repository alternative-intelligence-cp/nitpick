# The `raw`/`drop` licence worklist (D-163; measured 1.1.0, src swept 1.1.1)

**1.1.1 is done: `src/`, `lib/`, `tools/`, and the prelude hold ZERO unlicensed
sites** (from 8,997 + 118 measured at 1.1.0). `check_raw_licensed` continues to
print the `tests/` numbers on every full run; driving THOSE to zero and flipping
`TYPE_RAW_UNLICENSED` on is 1.1.2's job, after which the instrument retires.

| Tree | may-fail sites | builtin sites | other |
|---|---|---|---|
| src | **0** | **0** | 0 |
| lib | **0** | **0** | 0 |
| tools | **0** | **0** | 0 |
| prelude | **0** | **0** | 0 |
| tests | 289 | 6 | 14 |
| derive-generated | 2 | 0 | 0 |

The `other` sites are `raw`/`drop` over non-call operands or function values in
test fixtures — resolved by hand in 1.1.2's sweep, like 1.1.1's were.

## `tests/` worst files (1.1.2's sweep, from the harness's report)

| File | may-fail sites |
|---|---|
| `tests/frontend/lexer_operators.npk` | 29 |
| `tests/backend/programs/dyn_slots.npk` | 22 |
| `tests/frontend/lexer_numeric.npk` | 18 |
| `tests/backend/programs/generic_list.npk` | 16 |
| `tests/accept/lock_levels.npk` | 13 |

(The per-file remainder prints on every harness run.)

## How 1.1.1 spent the 9,115

- **~1,900 callees marked `never fails`** — the fixpoint proposer + the family
  knots, every claim audited by the checker (`TYPE-037`/`041`) on every round.
- **44 invariant accessors trap-converted** (`fail` → `!!!`): an out-of-range
  table index is a controlled stop with the table's defect code, never a
  Result that `raw` used to swallow into "continue on node 0".
- **~3,300 `relay` → `raw` conversions** where the callee is marked — the
  error branch provably dead, `raw` the checked zero-cost unwrap D-163 makes it.
- **21 proven slices** through `slice_proven` (one greppable trap code,
  `5tbb32`); **4 re-derivations** of checker-established facts trap as
  `?! 25tbb32`.
- **The reasoned `?| NIL` swallows**: ONLY the best-effort diagnostic writers
  (a broken stderr cannot be reported on stderr; the exit code carries the
  verdict) — each with the reason in a comment at the site.
- **`string_concat` and `int_to_string` reclassified never-fails** by IR-body
  audit (their error slot is written 0 on every path; OOM traps) — rule 9's
  own instruction, unlocking the single largest site family.
