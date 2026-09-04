# tests/cost — the `cost` stage's units

One `.toml` per unit, each read by BOTH runners (the harness's `stage_cost`,
`npkg/suites.npk`'s `run_cost`) and judged by the allocator's own numbers under
`NPK_HEAP_STATS` — bytes requested, the peak of bytes live, allocations — never
by the clock. The schema (`kind`, `recipe`/`program`/`entry`, `n`, `scale`,
`bound`, `ceiling`, `expect`, `until`) and what each kind measures are
BUILD_REFERENCE §7.1's `cost` row; the defects the units exist for are
OPEN_DECISIONS §2f (DEF-1's three axes, D-183's temporaries, the compiler's
own build); the plan is `meta/roadmap/1.5/1.5.1b.md` §2.

A unit with `expect = "fail"` is a NEGATIVE CONTROL: its bound must be
violated, which proves the instrument bites before the fix exists. The commit
that makes the bound hold removes those two lines in the same change — until
then the unit fails, loudly, the day it starts passing.
