# Cycle 0.9 — Full type lowering

**Phase B.** The type system stops being checked-only and starts running. 0.4
built the checker for every type; 0.7 lowered subset 1; this cycle lowers **the
rest of the type system** — the wide integers, floats, `tbb`, character widths,
ranges, function values, richer enums, and the full `Result`/struct/enum/slice/
array shapes — so that programs using the real type vocabulary compile and run.

It **opens with a safety repair**, because two constructs currently compile to
unsafe nothing (LIVE-1, LIVE-2), and closes with the borrow-analysis repairs the
hands-on review confirmed. Two standing instruments are added early, in the house
tradition of diffing the compiler against the thing that describes it.

## Goal

By the end of 0.9, a program may use every non-generic, non-async type in
`TYPE_REFERENCE.md` and either (a) compile, link, and run with correct arithmetic
and correct traps, or (b) be refused with `NITPICK-RUNG-001` naming the cycle that
enables it. **There is no third outcome** — no construct that parses and then
silently miscompiles. Closing that third outcome, which exists today in four
places, is as much this cycle's job as the new lowering is.

## Decisions in

Settle before the subcycle that needs them (see `../OPEN_DECISIONS.md`):

- **LIVE-1, LIVE-2** — no decision, immediate work; **0.9.0**.
- **The LBIM-vs-native wide-integer contradiction** (total_audit Theme D) — resolved
  in-plan here toward native `iN ≤ 256` per D-011's measurements; **0.9.3** states it
  and TYPE_REFERENCE §4 is corrected in the same subcycle.
- **Float div-by-zero** (trap vs IEEE) and **the `flt256`/`flt512` question** — a
  one-line decision each, taken at **0.9.4**.
- **`?`-on-tbb** (docs promise, checker refuses) — decided at **0.9.5** (proposed:
  the docs are wrong; `?` stays Result-only, D-099's one-wrapper rule holds).
- **C-13** (seed-retirement / builder-switch) is a *constraint on this cycle's
  source edits*, not a subcycle: everything `src/` adopts here must stay
  seed-compilable until 1.2 switches the builder. See "Watch for".

## The subcycles

| # | Topic |
|---|---|
| 0.9.0 | **The safety repair** — rung refusals for `limit`/`requires`/`ensures`/`invariant`; the div-by-zero guard (or a refusal); `INT_MIN/-1` |
| 0.9.1 | **Two instruments** — `check_kinds_lowered_or_refused`, `check_decisions_current` |
| 0.9.2 | **`Result`, structs, enums, slices, arrays** — the full aggregate lowering above subset 1 |
| 0.9.3 | **Wide integers** — native `i128`/`i256`, the `__divti3` runtime family, the alignment correction |
| 0.9.4 | **Floats** — `flt32/64/128`, the float runtime symbols, div-by-zero policy, the `flt256+` question |
| 0.9.5 | **`tbb` arithmetic** — saturation-to-ERR, sticky propagation, the cast sentinel checks, `pick ERR:` |
| 0.9.6 | **The remaining scalars & forms** — char16/32, ranges, function values, enum-tag casts, ternary `is` |
| 0.9.7 | **The control-flow and floor tail** — `for`/`loop`/`till`/`when`, the `pick` tail, `?`/`?!`, `sys`, `#ptr_add`, `write_all`'s graduation |
| 0.9.8 | **Borrow-analysis repairs & doc-sync** — the confirmed soundness holes, and the Theme-F pass |
| 0.9.9 | **The leading-digit rule (D-147) and ranged literals (D-148)** — added mid-cycle at the user's request after letter-leading balanced literals kept colliding with variable names; the probe work then surfaced that no stage checked a literal's value against its type, fixed in the same subcycle |

The table is the plan as of 0.9.0; a subcycle is inserted rather than stretched if
lowering a family turns up its own class of hole (0.4 and 0.6 both did).

## Watch for

- **The seed must keep compiling `src/` (C-13).** This cycle lowers `tbb`, floats,
  and wide integers — but the *compiler's own source* may not start using them until
  the builder switches at 1.2, because the seed lowers only subset 1. New lowering is
  exercised by **test programs** (`tests/backend/programs/`), not by adopting the
  feature in `src/`. This is the discipline the 0.8 README named; C-13 makes it
  normative.
- **Every new lowering is TCB.** Runtime symbols added here (`__divti3` family, float
  libcall replacements) get the D-015 hand-written-IR discipline and the three-way
  signature diff, and each must clear the D-011 undefined-symbol scan — which will
  *fail the build by name* the first time an unprovided `sdiv i128` libcall is
  emitted (0.8.2 built that check precisely so this cycle cannot regress silently).
- **A trap is correct output.** Div-by-zero, `INT_MIN/-1`, tbb-ERR-at-a-branch, and
  out-of-bounds all lower to a `failsafe` call, not to a refusal and not to UB. The
  test for each is an executed program whose exit code is the trap code — the
  strongest instrument the backend has (0.7.7).
- **The instruments come before the bulk lowering** (0.9.1 before 0.9.2), so that
  the diff is watching while the cycle's largest additions land — the ordering 0.6
  wished it had.
