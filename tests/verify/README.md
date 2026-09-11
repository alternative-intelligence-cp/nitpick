# `tests/verify/` — programs decided by z3, held to what they expect

A file here is compiled by the compiler under test with `--obligations`,
every function file is decided by the pinned z3 under the determinism
profile, and its rows are counted against the file's own header —
module-filtered, EXACTLY:

```
// expect-exit: 7
// expect-obligation: err-exit discharged 3
// expect-obligation: err-exit open 1
// expect-obligation: failsafe-post discharged 8
// expect-obligation: exhaustive checker 1
```

Each `expect-obligation` line names a KIND (VERIFICATION_REFERENCE §7b), a
VERDICT (`discharged`, `open`, `budget`, `unencoded`, `checker`) and a
COUNT, and the multiset of (kind, verdict) over the program's own module
must equal the header's — a row the header does not name fails the unit,
as does a named row that is not there. Then the program is built ELIDED
(each discharged guard replaced by its `llvm.assume`), run at -O0 and
through `opt -O2`, and must exit `expect-exit`; `expect-error:
NITPICK-VERIFY-001` instead says the verified build REFUSES the program (an
undischarged `prove`, D-269) and the unit ends there. Both runners — the
harness's `verify` stage and `npkg test` — judge every file the same way,
and the `parity` stage diffs their verdicts.

Three things every file carries:

- **Its `failsafe`'s rows.** Every `failsafe` has `failsafe-post` rows, one
  per `exit` (D-267), so no program expects nothing; the count is the number
  of `exit`s in the `failsafe`, the arms' and the trailing one.
- **A `checker` row per `pick`** (`exhaustive checker N`, both spellings,
  code lines only) and per `assert_static`.
- **Only the arms the reach analysis demands** — an arm no identity reaches
  is accepted and counts a row all the same. Since 1.5.4b step 4b a float
  `/` or `%` demands no `DivByZero`/`DivOverflow` (DEF-37).

A row's kind, site, role, group, traps and tier are `rows.txt`'s. Read one
by hand with `.internal/quickemit/npkc FILE --obligations D` and then
`z3 smt.random_seed=0 sat.random_seed=0 rlimit=20000000 -smt2 D/NNNN.smt2`
(CLAUDE.md's verification leg); a row with a Real-interval twin has
`D/NNNN.t2.smt2` beside it, named in `D/index.t2.txt`.

## The programs 1.5.4b added (the theories, D-277…D-282)

| program | what it pins |
|---|---|
| `shift_range.npk` | `shift-range` (D-277): an unconstrained amount `open`, one under a path condition and one the folder knows `discharged`; a division by a shifted-then-or'd value under the bounded amount crosses and discharges |
| `divz_masked.npk` | the bit-vector crossing at 32 bits (D-280): `(k & 6) \| 2` as a divisor, the D-007 pair discharged — the first bitwise-shaped discharge |
| `divz_shift_lit.npk` | the Int forms (D-279): `1 << 3` and `(m >> 2) \| 1` as divisors, two pairs discharged |
| `divz_wide_mask.npk` | the low-bits form at 2048 bits: `(x & 255) + 1` discharged where the crossing would exhaust the budget |
| `bit_test.npk` | a single-bit mask in a path condition |
| `flags_word.npk` | a flag family as a word (D-230 under D-279): `O_WRONLY \| O_CREAT`, `=>! int32`, a one-bit mask under `prove` |
| `twisted_cmp.npk` | `err-exit` at a compare (D-278): guarded and summed-then-tested discharged, bare open |
| `twisted_cast.npk` | `err-exit` at casts: leaving under both spellings, entering from a wider integer, narrowing within the family |
| `tfp_mul_range.npk` | a `tfp64` product of two `limit<r_unit>` parameters bounded by z3 (nonlinear), discharging the `=>!` after it |
| `tfp_div_err.npk` | the rowless twisted division: its ERR reaching a compare, open bare and trapping at run time, discharged under `is_err` |
| `dim256_cmp.npk` | `dim256` is `tfp256` to the solver |
| `tern_cmp.npk` | a `tryte` sum against the balanced bound, the Kleene `&`, `tryte => trit` narrowing, `int32 => tryte` entry |
| `limit_tbb.npk` | `limit` over a twisted subject: the write points and the rule's own compare |
| `neg_numeral.npk` | DEF-34 (a negative numeral in a row) and DEF-35 (a negated literal pattern) |
| `req_shift.npk` | DEF-33, both faces: a shift guard inside a `requires`, at the call and at the entry |
| `flt_tier1.npk` | floats, tier 1 (D-281): a bounded quotient's `prove` discharged in QF_FP, tier `fp` |
| `flt_tier2.npk` | tier 2: `prove(#sqrt(a*a + b*b) >= 0.0)` under a rule's bounds — `unknown` in QF_FP, discharged by the Real-interval twin, tier `real` |
| `flt_unbounded.npk` | the same claim without bounds: `budget`, no twin (condition (i)), the verified build refusing the `prove` |
| `flt_open.npk` | `prove(a + b > a)` open at once: NaN, or a non-positive `b` |
| `flt_limit.npk` | `limit` over a float subject, `unencoded` until 1.5.4b |
| `flt_nan_cmp.npk` | `prove(a == a)` over a parameter: open — `fp.eq`'s NaN reading, tier 1 only |
| `flt_entry.npk` | a float entering `tbb` (D-144 as amended): the `err-exit` row over the float term, discharged under a branch's bounds |
| `simd_div_rows.npk` | a `simd` division's any-lane guard as ONE row over the lanes (D-282): discharged under a `limit` on the splat, open on an unconstrained lane |
| `simd_lanes.npk` | lanes through bindings (S-58): a constructed local, a splat, `.sum()`, a lane read and `.len` as divisors; a shift by a literal splat discharged, by `main`'s amount open |
| `simd_div_open.npk` | a divisor whose lanes the program computes: `open`, the guard stays — `divz_unencoded.npk` renamed, since no `unencoded` row remains in the suite |
| `divz_none.npk` | (1.5.0's; re-expected at step 4b) a float and a twisted division: no row, and since DEF-37 no arm |

The earlier programs are indexed by the subcycle that wrote them
(`meta/roadmap/1.5/1.5.0.md` … `1.5.4.md`, each step's "Tests" paragraph).

## The programs 1.5.4e added

| program | what it pins |
|---|---|
| `simd_compound_rows.npk` | the compound form's any-lane rows (D-284, DEF-39): `v /= simd(k)` and `v <<= simd(k)` under a branch's bounds, all three rows discharged and their guards elided |
| `shift_field_compound.npk` | DEF-41: a compound shift through a field records its `shift-range` row (open, the guard kept), and the shift's fact after its site discharges the division's `div-min` beside it |
| `raise_code.npk` | D-285 (DEF-36): a division discharged and elided beside the program's own `?! DivByZero` — a raise, not a guard, to the belts; at run time the raise reaches its arm |
