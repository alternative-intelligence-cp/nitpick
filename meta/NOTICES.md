# The notices sent to the library listener

The compiler side tells the library side, before a language-visible change lands
and after every landing, what moved: the six ladder rows with the PREVIOUS digests
quoted (a delta alone authenticates only the previous size — D-265), the generated
manifest numbers, the harness's own summary lines verbatim, and the notice number.
The listener keeps its own count on its own board, and its count is the authority;
this file exists so ours can be checked without asking it. The number lived only
in two sessions' heads until 1.5.8b: a duplicate happened once (step 3 was sent as
24 where the board said 34, corrected by the listener), and the text of a notice
does not survive the session that sent it.

## The rule

Every landing notice carries, in this order: the number; the commit and the step;
what it is (the language-visible change first, or "no language change"); the six
ladder rows at the landed commit, each with the previous digest and size when it
moved; the manifest numbers (`nitpick.obligations`, `runtime/npkrt.obligations`)
as the runners print them; the harness block verbatim (`programs`, `verify`,
`floor`, `parity`, `ok N test(s) passed`); and what is next, with whether the
floor moves at it (a re-pin). An ADVANCE notice precedes any landing that
reserves a name (D-239), adds an arm every program must name, changes a refusal,
or moves the floor; it is unnumbered and names the number the landing will take.

## The count

| n | date | commit | landing | note |
|---|---|---|---|---|
| F5 | 2026-09-18 | — | FORECAST: 1.5.8 planned; S-84…S-87 ratified as D-304…D-307 | `decreases`/`unbounded` announced |
| 22 | 2026-09-19 | `cd1ed86` | 1.5.8 step 0 | |
| 23 | 2026-09-19 | `754b510` | 1.5.8 step 1 (`CastRange`, DEF-58/61) | |
| 24 | 2026-09-19 | `d5ad3c9` | 1.5.8 step 1b (DEF-65) | |
| 25 | 2026-09-19 | `f481dab` | 1.5.8 step 2 (the split-stack prologue, D-305) | corrected by 26 (the failsafe count) |
| 26 | 2026-09-19 | `cae3997` | 1.5.8 step 2b (DEF-64) | |
| F6 | 2026-09-19 | — | FORECAST | |
| 27 | 2026-09-19 | `4d5fd77` | 1.5.8 step 2c (DEF-67, `hold-at:`) | pin anchor `4d5fd77` |
| 28 | 2026-09-19 | `b7244d2` | 1.5.8 step 3 (`MachineFault`, D-307) | |
| 29 | 2026-09-19 | `0cf0f78` | 1.5.8 step 3b (DEF-66) | |
| F7 | 2026-09-19 | — | FORECAST | |
| 30 | 2026-09-19 | `6340d5c` | 1.5.8 step 3c (DEF-69) | the floor's last move before 1.5.8b step 6c |
| 31 | 2026-09-19 | `35ad9e1` | 1.5.8 step 4 (the refresh; 1.5.8 closed) | |
| 32 | 2026-09-19 | `f241766` | 1.5.8b step 0 (the plan, D-308…D-314) | documents only |
| 33 | 2026-09-19 | `410d405` | 1.5.8b step 1 (`sealed`/`hidden`) | |
| 34 | 2026-09-19 | `3592de2` | 1.5.8b step 3 (the `overflow` rows) | sent as "24"; the listener corrected the count. Steps 1b and 2 landed without a notice of their own and rode 33 and 34 |
| 35+36 | 2026-09-19 | `0439819`, `3207f72` | 1.5.8b steps 4 and 5 (`+% -% *%`; the `bounds`/`cast-range` rows) | one message; its "since 1.5.4e" for the floor's last move was wrong (`6340d5c` is right — the listener caught it) |
| 37 | 2026-09-20 | `c5ba885` | 1.5.8b step 6 (a field's own `limit<Rules>`) | its "byte-identical emission" sentence contradicted its own ladder (the listener caught it; the plan's sentence was the defect — fixed at step 7) |
| — | 2026-09-23 | — | ADVANCE for step 6b (the reach fix) and for step 6c (the ceiling, `ListLen`, the arms, the floor) | |
| 38 | 2026-09-24 | `14ef02f` | 1.5.8b step 6b (DEF-86: the reach analysis follows calls into the prelude) | the rows 38–40 were written at step 7 (2026-09-23) ahead of their landings; each was sent 2026-09-24 |
| 39 | 2026-09-24 | `3e4b47d` | 1.5.8b step 6c (D-308 §§6–7; the floor moves) | re-pin (`npkrt.o` 72,560 → 72,576 B) |
| 40 | 2026-09-24 | `3156b72` | 1.5.8b step 6d (`intern.npk`'s `*%`; DEF-88) | |
| 41 | 2026-09-24 | `aee4dd9` | 1.5.8b step 7 (the docs and the close; 1.5.8b COMPLETE) | every ladder row unchanged from `3156b72` |
| — | 2026-09-24 | — | ADVANCE for 1.5.8c step 1: `decreases`/`unbounded` become keywords (none in the libraries, measured 2026-09-23), `DecreasesViolated` an arm every program with a `decreases` names, `terminate` guarded; the landing takes the number after 1.5.8c step 0's | sent to `nitpick-libs_s5` (the seat changed hands 2026-09-24) |
| 42 | 2026-09-24 | `68b6e05` | 1.5.8c step 0 (the plan and the codes) | no language change |
| 43 | 2026-09-24 | `f578e6b` | 1.5.8c step 1 (the mechanism: the two keywords, the loop and function clauses, the check, the `terminate` rows; DEF-90) | sent by `nitpick-compiler_s13` (the seat after s12) |
| 44 | 2026-09-24 | `5ea6053` | 1.5.8c step 2 (the one-hop snapshot refresh: the committed builder parses the clauses and carries DEF-90's fix; the builder rows move) | carries the SWEEP RECIPE for the libraries (step 3's tool and idioms; D-316) |
| 45 | 2026-09-24 | | 1.5.8c step 3 (the sweep of our tree: every `while`/`when` clausal, 392 by the tool, 563 by the reading, 72 `unbounded`; the evaluator checks a measure; every failsafe names `(DecreasesViolated)`) | no language change; the record `decreases_read.txt` is the libraries' worked example |
| — | 2026-09-24 | — | ADVANCE for 1.5.8c step 4: TYPE-072's `neither` shape becomes a refusal; a FUNCTION's `decreases` is live (the recursive groups, TYPE-074/075, TYPE-073's 64-bit width for a function measure, the check at every call inside the group, the `terminate` call rows, the `stack-depth` rows derived by both runners); `index.txt` gains two fields and `rows.txt`'s fifth admits `d`; DEF-92 named | sent to `nitpick-libs_s5` by `nitpick-compiler_s13` before the landing |
| 46 | 2026-09-24 | | 1.5.8c step 4 (the refusal, the groups, the function measure, the `stack-depth` rows) | the landing the advance above announced |
| 47 | 2026-09-24 | | 1.5.8c step 4b (DEF-92: the generic-instance interner's index -- the frontend 5.4x faster, every program's emission byte-identical) | no language change; a re-pin gets a faster compiler and nothing else |

The messages themselves are not kept here; each seat keeps the text it sent in its
own `.internal/handoff_*/` scratch for as long as that survives, and this table is
what survives them.
