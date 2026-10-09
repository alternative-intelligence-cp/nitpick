# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Status: PHASE C UNDERWAY — **cycle 1.6 (the LLVM-native analyzer evidence, D-233) IS UNDERWAY: **DEF-224 LANDED 2026-10-09 UNDER D-352 — `failsafe`'S PICK IS FOUND THROUGH BARE BLOCKS, SO A STATEMENT MACRO MAY SUPPLY IT** (landing 109, `1.6.1e.md`'s record; `nitpick-compiler_35`; the library listener's probe02a / PX-010, registered 2026-10-01 at landing 92; the user's ratification 2026-10-09: "I am pretty much fine with recommendations that attempt to strike the best balance of those goals and from the best i can figure what you are proposing does so") — D-179 says `failsafe` "must contain" the pick and `reach_settle` read it as "among the body's own statements", so `#posix_failsafe(e);` (an expansion is a bare block) and a hand-written `{ pick (e) { … } }` were REACH-001; **the pick is found through bare blocks at any depth and never through an `if`, a `when`, a loop or another pick's arm** (`failsafe_pick_in`, reach.npk: a bare block runs unconditionally, so every trap reaches the pick, which is all D-179 asks; REACH-002's arm contract is asked of it where it stands). A REFUSAL REMOVED. `failsafe_pick_macro.npk`, `failsafe_pick_block.npk` (exit 70 through a real trap), `failsafe_cond_pick.npk` (REACH-001); D-352 carries the user's words, D-179 and MACRO_REFERENCE §5 their notes; **S-134 REGISTERED** (a `failsafe` a project importing many libraries can keep, beyond one shared macro: the user's own question, not acted on). The sweep 5,117 files (5 sites vanished: the two REACH-001 of `failsafe_pick_macro.npk` and `failsafe_pick_block.npk`; and the REACH-001 of the listener's `probe02a_failsafe_macro.npk`, `probe02d_caller_body.npk` and `probe02e_block_nested.npk` (nitpick-posix), each passing the pick search now; 8 appeared (the same three probes reaching REACH-002 for `StackExhausted` and `MachineFault`, two sites each -- their arm set predates 1.5.8 -- and the MACRO-009 note under the two macro forms, pointing the report into the macro body)); the emission 568 programs, 566 byte-identical, 0 different, the two new tests refused by the base alone, the libraries' 4,003 — 0 different, 0 newly refused; the manifest 7,070 obligations (6,771 → 6,774 rows, 6,660 shared, ZERO moved, 2 fell). NEXT: DEF-226, DEF-232…DEF-238, DEF-242, DEF-244, DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-202 LANDED 2026-10-09 — A REFERENCE TO AN AGGREGATE MODULE BINDING IS A CONSTANT** (landing 108, `1.6.1e.md`'s record; `nitpick-compiler_35`; found by landing 87's probes, registered 2026-10-01) — `fixed Pt:Q = P;`, `fixed int32[3]:B = A;` and `fixed int32?:O2 = O;` were NITPICK-TYPE-035 in the sentence that lists "another module binding" as legal (the module-constant gate and the global renderer asked only the folder, which holds a scalar's and a string's value and nothing else); **one walk, `fixed_global_sym` (type_resolve.npk), says which `fixed` module binding a name stands for** — the folder's identifier arm, the gate and the renderer read it — **the gate admits a reference the folder cannot hold** (the target's own declaration answers for its initialiser, D-240) **and the renderer follows it to the target's initialiser, in the target's home scope**; the folder's own answer first on both sides, so nothing that compiled before moves. A REFUSAL REMOVED. `module_ref.npk` (twenty-one checks at both legs; a struct, a chain, an array, an `Optional`'s `NIL`, nested aggregates, members of a literal, an inline module's binding); TYPE_REFERENCE's initialiser table and DECISIONS' D-165 carry the note. The sweep 5,114 files (9 sites vanished: the 9 TYPE-035 sites of `module_ref.npk`, one per aggregate reference; 0 appeared); the emission 566 programs, 565 byte-identical, 0 different, the new test refused by the base alone, the libraries' 4,003 — 0 different, 0 newly refused; the manifest 7,067 obligations (6,771 → 6,771 rows, 6,538 shared, ZERO moved, ZERO fell). NEXT: DEF-224, DEF-226, DEF-232…DEF-238, DEF-242, DEF-244, DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-243 LANDED 2026-10-09 — A BUILTIN IS CALLED, NEVER NAMED AS A VALUE** (landing 107, `1.6.1e.md`'s record; built by `nitpick-compiler_34`, landed by `nitpick-compiler_35`; the library seat's O-N40 item 1, registered at landing 99) — `func int64() never fails:f = mono_now;` and `call_it(mono_now)` passed the checker and died in the emitter as EMIT-002 (a builtin is the emitter's intrinsic, with no symbol to hand out; the generated table types a builtin's CALLS alone, D-201, and `type_ident` returned D-240's "refused" 0 for the bare name with nothing reported); **`type_ident` refuses an identifier with no symbol and a builtin's spelling** (`NITPICK-TYPE-054` at the name, in an initialiser, an argument, a struct literal's field, a `pass`, an assignment) **and `type_call` reads a bare builtin callee by its spelling** instead of handing it to the identifier typer (the first form reported every builtin CALL in the prelude; the test's own `mono_now()` control caught it). A REFUSAL ADDED, a loud failure moved from the emitter to the checker. `builtin_as_value.npk` (five sites, the controls); BUILTIN_REFERENCE's D-294 paragraph and DECISIONS carry the note. The sweep 5,113 files (5 sites appeared: the five sites of `builtin_as_value.npk`; 0 vanished); the emission 565 programs, 565 byte-identical, 0 different, the libraries' 4,003 — 0 different, 0 newly refused; the manifest 7,067 obligations (6,771 → 6,771 rows, 6,662 shared, ZERO moved, ZERO fell). NEXT: DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242, DEF-244, DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **D-342's AND D-343's REFUSALS LANDED 2026-10-09 — AN ENUM VALUES EVERY VARIANT OR NONE, AND A MACRO PARAMETER CANNOT NAME WHAT THE BODY EMITS** (landing 106, `1.6.1e.md`'s record; `nitpick-compiler_34`; the two of the user's 2026-10-01 ratifications — "all the recommendations look fine to me. lets go with those." — the checker did not yet make) — `enum:Mixed = { First; Second; Tenth = 10i32; };` compiled with `Tenth` 10 beside positions 0 and 1 (a reader from C or Rust expects 6 of `{ A = 5i32; B; }`'s `B`: the look-alike trap), and `macro:m = (N) { func:N = …; };` emitted a function literally called `N` with the argument dropped; **a mix is `NITPICK-TYPE-093` once, at the first variant whose spelling differs from the first's** (`check_enum_values`; a mixed enum's unvalued variants leave the tag table, D-240) **and a parameter the body declares, used or not, is `NITPICK-MACRO-011` at the declaration** (`check_macro_params`, with its own sentence for the declared-only shape). TWO REFUSALS ADDED. `enum_values.npk` (`Mixed` into the refused section; thirteen sites), `param_misplaced.npk` (the four `*_unused` shapes; twenty sites), `enums_pick.npk` (`Flat` values every variant); the headers regenerated to a fixed point by `.internal/handoff_34/tools/header_from_check.py`. The sweep 5,112 files (1 vanished — a collision report replaced by the mix report — and 7 appeared: the two files' new sites and the listener's `mc0388`, MACRO-011); the emission 565 programs, 565 byte-identical, the libraries' 4,003 — 0 different, 1 newly refused (`mc0388`); the manifest 7,067 obligations (6,771 → 6,771 rows, 6,658 shared, ZERO moved, ZERO fell). NEXT: DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242…DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-159 LANDED 2026-10-09 — A VALUE STORED THROUGH A HELD `@x` AFTER `move(x)` IS DROPPED AT THE SCOPE EXIT** (landing 105, `1.6.1e.md`'s record; `nitpick-compiler_34`; registered 2026-09-26 by 1.6.1e step 1's probes of DEF-120's own shape) — `string->:p = @x; string:t = move(x); (<-p) = v;` is legal and LEAKED `v`: the move cleared `x`'s drop flag, the store put a live `v` into the vacated slot, the scope exit read the flag and dropped nothing (192,094 bytes live after two thousand rounds of three kinds against 190 for one); **the emitter marks a binding whose address it has taken** (`mark_root_taken` from `emit_addr_op` — `@`, `$$i`, `$$m` — the pointer receiver of `emit_method_call` and the awaited receiver of `emit_child_frame`; `FnEmitter.ltaken`) **and drops such a binding's slot without the flag test** (`emit_one_flagged_drop`: a vacant slot's drop is D-225's no-op, since every moved-out or destroyed place is vacant — DEF-120, DEF-158 — and a re-filled slot's is the drop owed); the flag stays the fast path for a binding nobody addressed. NO REFUSAL moves; a LEAK CLOSED. `held_addr_store.npk` (six roads: a local, a `move` parameter, a List, a struct, a kept receiver address, a coroutine), `tests/cost/held_addr.toml` (190 against 190); `meta/roadmap/1.6/tools/dropnorm.py` reads an emission comparison whose drops changed. The sweep 5,112 files (0 appeared, 0 vanished); the emission 565 programs, 518 byte-identical, 47 different in text — every one flag-tests-only (2,972 drops made unconditional), the libraries' 4,003 — 24 different in text (every one flag-tests-only), 0 newly refused; the manifest 7,067 obligations (6,769 → 6,771 rows, 6,675 shared, ZERO moved, ZERO fell). NEXT: D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242…DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-249 LANDED 2026-10-09 — AN `exit` OPERAND FITS `int32`** (landing 104, `1.6.1e.md`'s record; `nitpick-compiler_34`; the library seat's O-N41 from nitpick-time 0.3.2's planner, registered at landing 102) — `exit may(argv.len);` of a fallible `may` passed the checker and `llc` refused the emitted module ("'%t3' defined with type '{ i32, i32 }' but expected 'i32'"), while `int32:v = may(argv.len); exit v;` was TYPE-007: `check_exit` (type_stmt.npk) typed the operand under the `int32` expectation and never held it to it; **it holds the operand to `int32` exactly and reports through the one mismatch reporter at the operand** (not `fits` -- no conversion enters an exit code, and the in-process slot-sites check refused the first form; `NITPICK-TYPE-007`: a `Result<int32>`, an `int64` — "no implicit widening, so write the cast" — a `bool`; in `main` and `failsafe` alike; an entry point whose signature is already refused keeps that one report, D-240; `?!`, `?|` and a `pick` are the spellings). A REFUSAL ADDED, a loud failure moved from `llc` to the checker. `exit_operand.npk` (four sites, four controls; `main_sig_ret.npk` and `failsafe_sig_ret.npk` the D-240 controls); CONTROL_REFERENCE §4.6's note. The sweep 5,109 files (4 sites appeared: the four sites of `exit_operand.npk`; 0 vanished); the emission 562 programs, 562 byte-identical, 0 different, the libraries' 4,003 — 0 different, 0 newly refused; the manifest 7,065 obligations (6,769 → 6,769 rows, 6,662 shared, ZERO moved, ZERO fell). NEXT: DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242…DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **D-348 STEP (ii) LANDED 2026-10-08 — `fixed T[]` IS A TYPE, THE READ-ONLY VIEW** (landing 103, `1.6.1e.md`'s record and §2.10e; `nitpick-compiler_33`; D-350 and D-351, the user's ratification of S-132 and S-133 on 2026-10-08: "your recommendation for those questions looks fine to me."; DEF-246 and DEF-247 FIXED) — a view left its binding by seven roads carrying no rights (DEF-246: `string_bytes` of a `fixed` module string or of a literal written through, `MachineFault`; a `fixed` module array ranged, `MachineFault`; a `fixed` local array ranged, a view copied out of a `fixed` struct, a `fixed` view assigned to a plain binding, silent; an impl of `Writer` dropping the trait's `fixed`, accepted and writing the caller's bytes through the bound): **the type carries them** — `TY_SLICE` with one bit (`tt_slice_fixed`), a plain view converting to it and never back (`fits`; TYPE-007 with the rule, TYPE-032 at a cast), produced by a `fixed` binding of slice type (R1: the parser marks the node, `ast_type_mark_fixed`), a range of `fixed` storage, a slice read off a `fixed` struct and `string_bytes` (always, D-351), refused through by `place_fixed` reading the base's TYPE (TYPE-086, TYPE-071 in any slot), and told apart by id in a trait comparison (TYPE-014); in a bare position `fixed` precedes a slice type only (PARSE-013 otherwise; `stack` on a return and `fixed` on a cast target too — DEF-247's dormant bits deleted); a plain binding of the view is the grouped type `(fixed uint8[]):view`. The tree's seven readers of the bridge and fourteen tests re-spelled; two double reports fixed (`require_place*` stop their callers, D-240). The sweep 5,101 files (88 vanished: the tree's 31 are the base compiler's parse errors on the three new files and the re-spelled `view_escape.npk` (it cannot parse `fixed uint8[]` in a bare position); the libraries' 57 are analysis-stage codes that stand behind a TYPE-007 in the same file now (MOVE-001 31, ASSIGN-002 8, BORROW-001 6, BORROW-015 5, BORROW-013 4, BORROW-012 2, BORROW-009 1); 1,475 appeared: the two rejection files' sites and 1,453 TYPE-007 sites in 1,112 library files, readers of the bridge — nitpick-fuzz 1,013 files, 1,018 sites (src/ 0 files, 0 sites; tests 1,013 files); nitpick-regex 78 files, 400 sites (src/ 10 files, 28 sites; tests 68 files); nitpick-time 21 files, 35 sites (src/ 2 files, 2 sites; tests 19 files)); the emission 562 programs, 561 different in text by the one type-id shift and 561 identical after `tidnorm.py`, the libraries' 3,994 — 659 newly refused (the readers); the manifest 7,065 obligations (6,767 → 6,769 rows, 6,108 shared, ZERO moved, ZERO fell). NEXT: DEF-249 (`exit` of a fallible call reaches `llc`: the library seat's O-N41), DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242…DEF-245, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-248 LANDED 2026-10-08 — A `fixed` PARAMETER IS WRITTEN BY ITS CALLER AND NEVER BY ITS BODY** (landing 102, `1.6.1e.md`'s record; `nitpick-compiler_33`; found by the probes of D-348 step (ii)'s planning) — TYPE_REFERENCE §26 promised ASSIGN-002 for "a parameter the callee may not reassign" and the bindings analysis never asked (its comment said a parameter carries no `fixed`; `param_quals` has carried the bit since 0.7.3, read by `place_fixed` alone): `func:g = int32(fixed int32:n) … { n = 2i32; … }` compiled and answered 2; **the assignment arm reads `param_quals & QUAL_FIXED`** — a whole-binding or compound assignment to a `fixed` parameter is `NITPICK-ASSIGN-002`, a `fixed uint8[]` view parameter's re-pointing included; a plain or `move` parameter keeps DEF-124's re-assignment. A REFUSAL ADDED. `fixed_param_assign.npk` (five sites, the local twin as the control). REGISTERED with it: **DEF-246** (a plain view minted from or copied out of `fixed` storage writes it — six faces, two of them `MachineFault` in safe code: `string_bytes` of a `fixed` module string, a `fixed` module array ranged; three silent; and an impl of `Writer` dropping the trait's `fixed`, accepted — D-348 step (ii) closes all six by construction, **PLANNED at `.internal/handoff_33/D348_II_PLAN.md`, its S-132 (the spelling, R1) and S-133 (`string_bytes` returns the read-only view) SETTLED by the user the same day as D-350 and D-351 — recorded here, with DEF-249 (the library seat's O-N41: `exit` of a fallible call reaches `llc`) — and BUILT as landing 103**) and **DEF-247** (`fixed`/`stack` as return qualifiers parse and mean nothing). The sweep 5,097 files (5 sites appeared: the four parameter sites of `fixed_param_assign.npk` (its fifth, the local twin, is the rule as it stood and the base reports it too); and the fuzzer's own CLAIM program for this rule, nitpick-fuzz's `m11/programs/ty1873.npk` (its header: "claim: A fixed parameter may not be reassigned: ASSIGN-002; expect: refuse; wrong: accepted" -- the corpus had DEF-248 recorded as a wrong acceptance, and it refuses as expected now); 0 vanished); the emission 561 programs, 561 byte-identical, the libraries' 3,995 — 0 different, 1 newly refused; the manifest 7,063 obligations (6,767 → 6,767 rows, 6,583 shared, ZERO moved, ZERO fell). NEXT: D-348 step ii (landing 103, built), DEF-249, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242…DEF-245, DEF-246 and DEF-247 (with step ii), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-164 LANDED 2026-10-08 — ONE MISTAKE, ONE REPORT AT THE LEXER'S AND THE PARSER'S SEAM** (landing 101, `1.6.1e.md`'s record; `nitpick-compiler_32`; found by D-332's count rule over the library listener's corpus on 2026-09-27, four more faces at 1.6.1e step 3 and 3a; D-240) — `pub error:ETimeValue(ValueFault);` was PARSE-001 twice (the recovery resumed at the identifier the broken declaration left behind), a refused integer literal (LEX-003, LEX-004) was followed by PARSE-002 at the Error token, a dangling exponent sign (`1.5e+;`) by PARSE-002 at the `+` left behind, and a refused parameter type (TYPE-001) by TYPE-014; **a declaration's recovery skips to a sync point (`p_recover_decl`), a refused integer literal stays an `IntLit`, a dangling sign before a terminator is the refused literal's tail, and a signature holding a refused type is not compared (`sig_holds_invalid`)** — statements keep `p_recover`, `3.5e-x` keeps its reading. REFUSALS REMOVED BY COUNT, NONE BY KIND. `parse_one_report.npk` (the unit: faces a–d one diagnostic each, the recovery controls), `impl_refused_param.npk`. DEF-245 REGISTERED (a sixth face the sweep found, seven fuzzer programs: a declaration whose header fails is parsed on into its body and the body reported again; not fixed here). The sweep 5,089 files (44 sites vanished and 1 appeared: 43 second reports gone — 2 TYPE-014 in `impl_refused_param.npk`; the libraries' 1 PARSE-002 in `c23_c_hex_prefix_refused.npk`; 1 PARSE-001 in `as0038b.npk`; 1 PARSE-001 in `as0044.npk`; 1 PARSE-001 in `as0575.npk`; 3 PARSE-001 in `cc0151c.npk`; 1 PARSE-002 in `lx0321.npk`; 1 PARSE-002 in `lx0343.npk`; 1 PARSE-002 in `lx0346.npk`; 1 PARSE-002 in `lx0349.npk`; 1 PARSE-001 in `mc0332.npk`; 2 PARSE-001 in `mc0368.npk`; 1 PARSE-001 in `md0166.npk`; 1 PARSE-001 in `md0209.npk`; 1 PARSE-001 in `publib.npk`; 1 PARSE-001 in `md0212.npk`; 3 PARSE-001 in `md0219.npk`; 1 PARSE-001 in `tr0041.npk`; 2 PARSE-001 in `tr0045.npk`; 3 PARSE-001 in `tr0356.npk`; 5 PARSE-001 in `tr0425.npk`; 2 PARSE-001 in `tr0448.npk`; 2 PARSE-001 in `tr0550.npk`; 1 PARSE-002 in `ty1646b.npk`; 2 PARSE-001 in `vf0580.npk`; 1 PARSE-002 in `probe02d_wide_literal_refused.npk`; 1 PARSE-001 in `probe14_error_payload_refused.npk` — and one moved within its line, the fuzzer's `as0100.npk`, a sixth face registered as DEF-245); the emission 561 programs, 561 byte-identical, 0 different, the libraries' 3,988 — 0 different, 0 newly refused; the manifest 7,063 obligations (6,761 → 6,767 rows, 6,588 shared, ZERO moved, ZERO fell). NEXT: D-348 step ii (planned first), DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242, DEF-243, DEF-244, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-165 LANDED 2026-10-08 UNDER D-337 — A STRUCT LITERAL THAT WRITES SEVERAL `sealed` FIELDS FROM OUTSIDE THEIR MODULE IS ONE REPORT** (landing 100, `1.6.1e.md`'s record; `nitpick-compiler_32`; found by D-332's count rule over the library listener's corpus on 2026-09-27, the user's reading of 2026-09-30) — `PatternError{ kind: …, offset: …, span_len: …, detail: … }` written outside its module was FOUR `NITPICK-TYPE-079` at one span (D-313's write form, counted per field); **`type_struct_literal` collects the sealed fields a literal writes and reports ONCE at the literal, naming them all** (`note_sealed_decl` under each field; a literal writing one sealed field keeps the per-field sentence; every other write form stays one report per write, D-337); `sealed_literal.npk`'s five literals (the parent 13 TYPE-079, this compiler 5). REFUSALS REMOVED BY COUNT, NONE BY KIND. The sweep 5,074 files (16 sites vanished, every one a collapsed per-field report — 1 in `list_fields.npk` and 8 in the new `sealed_literal.npk`, 7 in the libraries' `pattern_error_literal.npk` (3 sites fewer), `probe15_civil_literal_bypass.npk` (2 sites fewer), `probe20_instant_literal_refused.npk` (1 site fewer), `probe21_timestamp_literal_refused.npk` (1 site fewer); 0 appeared); the emission 561 programs, 561 byte-identical, 0 different, the libraries' 3,978 — 0 different, 0 newly refused; the manifest 7,057 obligations (6,754 → 6,761 rows, 6,647 shared, ZERO moved, ZERO fell). NEXT: DEF-164, D-348 step ii (planned first), DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242, DEF-243, DEF-244, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **D-349 LANDED 2026-10-08 — THE TOOLCHAIN PIN IS LLVM 20.1.8, THE RELEASE THE DISTRIBUTIONS SERVE, AND THE DEPENDENCY CHAIN MOVED WITH IT** (landing 99, `1.6.1e.md`'s record; `nitpick-compiler_32`; S-131 from the library seat's VM test of INSTALL.md, the user's "we just can't forget the dependency chain beyond just the compiler itself") — measured FIRST with apt.llvm.org's signed noble-20 packages extracted into a user prefix on landing 98's tree: every object (`npkrt.o`, the shim's `npkx.o`, `builder.o`, `npkc.o`) and the emission (`npkc.ll`, 31,443,507 B) BYTE-IDENTICAL to the 20.1.2 ladder's, the fixpoint holding, `float_vectors.py --llvm` the same numbers, and ONE byte of every linked binary moved (the linker's `.comment` string); NIKOS (LLVM static) and Alive2 (`libLLVM.so.20.1` dynamically: it would have drifted under its old digest) rebuilt against 20.1.8 and re-pinned, the gate's eight controls 7 of 8 found in both modes and Alive2's smoke counts as at 1.6.0 — NIKOS's cmake demanding a `clang` of the SAME release beside `llvm-20-dev`, and `engines.sh` now finding its LLVM where PATH's `llc` lives (the runners' rule). THE MACHINE MOVED AS A PREFIX: `apt` could not install the suite system-wide because the 32-bit Mesa drivers hold `libllvm20:i386` and apt.llvm.org ships no i386, so the ten verified packages live in `~/.local/llvm-20.1.8` and the `~/.local/bin` links point there (`/usr/lib/llvm-20` stays 20.1.2 for Mesa); INSTALL.md names the distributions' `llvm-20`, apt.llvm.org's noble suite for Ubuntu 24.04 and the prefix route for the i386 case. The harness green under the prefix (53 of 53; `nitpick.obligations` unmoved: no `src/` change). No refusal, no emission, no computed answer, no floor byte, no snapshot moves; every worktree pinned to 20.1.2 refuses until rebased. DEF-243 and DEF-244 REGISTERED (the library seat's O-N40: a builtin as a function VALUE dies in the emitter; the loader keys a module by a path's SPELLING). NEXT: DEF-165 under D-337, DEF-164 (the library side's re-pin waits on these two), DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242, DEF-243, DEF-244, D-348 step ii (planned first), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-230 LANDED 2026-10-08 UNDER D-348 STEP (i) — A `fixed` SLICE IS READ-ONLY THROUGH IT** (landing 98, `1.6.1e.md`'s record; `nitpick-compiler_32`; the library listener's F-047 in O-N36) — a callee declaring `fixed uint8[]:v` wrote `v[0i64] = 9u8` into its caller's bytes in silence (exit 10 at both legs), because `place_fixed` (type_stmt.npk) stopped at a slice base by D-287's reading ("the storage there is not the binding's own") while D-074 had retired `binary` on the promise that an immutable byte view is `fixed uint8[]`; the user chose the promise (D-348): **the walk goes through a slice base to the view's root** (a pointer and a handle still stop it: an address is not a view), so an element write, a compound, a sub-range's element, a slice held in a `fixed` aggregate or declared a `fixed` field are `NITPICK-TYPE-086` and `@`, `$$i`/`$$m` and a pointer-receiver call on an element are `NITPICK-TYPE-071`, each with the view's own sentence (`place_through_slice`); `fixed_slice_write.npk`'s nine sites; the listener's `s1`/`s2` refused, its controls unchanged, the tree's six `Writer` implementations untouched. A REFUSAL ADDED that no file reaches but the finding's own reproducers; step (ii) — `fixed T[]` as a TYPE — is planned before it is built. The sweep 5,072 files (1 vanished: NITPICK-REACH-001 at the new rejection file's own `failsafe`, a site that exists only because the PARENT admits the file's nine writes and runs its reach analysis over it, which asks the `failsafe` to name what can reach it; this compiler refuses the writes first; 12 appeared: the nine sites of `fixed_slice_write.npk` -- TYPE-086 at its six write forms, TYPE-071 at its three address forms -- and the finding's own reproducers, TYPE-086 each: the listener's `s1_fixed_view_written.npk` and `s2_fixed_param_written.npk` and the fuzzer's `ty1719b.npk`; no other file of the tree, the libraries or the applications moves); the emission 561 programs, 561 byte-identical, 0 different, the libraries' 3,977 — 3 newly refused (`s1_fixed_view_written.npk`, `s2_fixed_param_written.npk`, `ty1719b.npk`); the manifest 7,050 obligations (6,752 → 6,754 rows, 6,645 shared, ZERO moved, ZERO fell). NEXT: D-349's landing (the toolchain pin to 20.1.8 with NIKOS and Alive2 rebuilt and re-measured), then DEF-165 under D-337, DEF-164, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, D-348 step ii (planned first), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-231 LANDED 2026-10-08 UNDER D-347 — A `frac`'S STORED PARTS ARE ITS READABLE PARTS** (landing 97, `1.6.1e.md`'s record; built by `nitpick-compiler_31`, landed by `nitpick-compiler_32`; the fuzzer's `ty1657` in O-N36) — `-(1 3/8)` was stored {−2, 5, 8} (D-198's floor form, the prototype's rule) and printed "-2 5/8", which a reader takes for −2.625, and −3/8 had TWO stored forms ({−1, 5, 8} from `(-1) + 5/8`, {0, −3, 8} from `1/8 - 1/2`: `==` equal, the parts different); the user ruled the print a wrong answer (2026-10-05) and ratified the form (2026-10-08): **whole and num never of opposite signs, |num| < denom, value = whole + num/denom in every case, one form per value; the print shows the sign once** ("-1 3/8", "-3/8"); `npk_frac_norm`'s last block shares the sign, the four `ToString` impls print |num| beside a nonzero whole, `emit_frac_cast`'s integer exit is the whole field, `npk_frac_cmp` needed nothing; `frac_parts.npk`'s thirty-three checks, every frac program of the tree at both legs, `ty1657` exits 0. A COMPUTED ANSWER changes for every negative frac with a fraction (its print, `.whole`, `.num`); no refusal moves. The sweep 5,069 files (5 vanished, 5 appeared: five `note:` lines naming prelude lines that moved); the emission 561 programs, 0 byte-identical — EVERY emission moves through D-179's error-origin site table (every program carries the prelude's whole table, and the frac section gained six guard sites and 27 lines: **DEF-242 REGISTERED**), and with the table canonicalised (`meta/roadmap/1.6/tools/sitenorm.py`) 558 identical and the 3 different the frac programs, the libraries' 3,977 likewise 2,418 identical and 17 frac programs different; the manifest 7,047 obligations (6,752 → 6,752 rows, 6,678 shared, ZERO moved, ZERO fell; the 74 re-keyed a move of type-id names). NEXT: landing 98 (DEF-230 under D-348 step i), D-349's landing (the toolchain pin), then DEF-165 under D-337, DEF-164, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, DEF-242 (the site table), D-348 step ii (planned first), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-229 LANDED 2026-10-08 — THE COMPILER TERMINATES ON AN UNBOUNDED GENERIC INSTANTIATION** (landing 96, `1.6.1e.md`'s record; built by `nitpick-compiler_31`, landed by `nitpick-compiler_32`; the library listener's F-041 in O-N36) — `deep<T>` calling `deep::<Box<T>>(…)` never exited (1.16 GB at 60 s; the fuzzer's 4.4 GB at 300 s): the cap of 64 bounded the resolver's recursion through nested type arguments alone (`NITPICK-TYPE-018`) and the emitter's transitive monomorphization (`fninst_concrete`) consulted no depth; **the direct self-call at a type built from the function's own parameter is TYPE-018 at the call** (`type_generic_call`, `type_mentions_params_of` with the declaration's window: an enclosing impl's parameter is somebody else's), **and an indirect cycle is refused by the emitter's cap** (`type_nest_depth` against `GENERIC_DEPTH`, one number for both halves) **as an emitter refusal by name** (`iv_refused`/`LL_REFUSED`, the third `IrVal` state beside the rung and the defect) in a third of a second; `generic_self_growth.npk` (four sites, four controls), `generic_nesting_bounded.npk`. **D-347, D-348 AND D-349 RECORDED** (the user, 2026-10-08: "your recommendations are fine with me" at 00:33 — the frac form and the `fixed` slice; "i'm fine with moving the pin … we just can't forget the dependency chain beyond just the compiler itself" at 01:15 — the toolchain pin to 20.1.8 with NIKOS and Alive2 rebuilt, its own landing after 98) **AND THE INSTALL README PLACED** (`INSTALL.md` at the root, `README.md`'s pointer; the library seat's VM test on Ubuntu 26.04 folded in: the distribution's `llvm-20` first, apt.llvm.org's dead source and its recovery, RESOLVE-012 for a mod-name mismatch — which this file also had wrong). The sweep 5069 files (0 vanished, 6 appeared: the finding's own `h1_unbounded_instantiation.npk` and the fuzzer's claim program `tr0586.npk` (TRAITS §3.5's claim, 'expect: refuse') -- each a direct self-growing call the parent's CHECKER passed clean, the hang being the emitter's -- and `generic_self_growth.npk`'s four sites); the emission 560 programs, 560 byte-identical, 0 different, the libraries' 3978 — 0 newly refused; the manifest 7047 obligations (6730 → 6752 rows, 6361 shared, ZERO moved, 1 fell -- a MOVE: `ir_types.type_mentions_param`'s 4 discharged `overflow` rows are `types.type_mentions_params_of`'s now; 22 rows added for the new walks). NEXT: landing 97 (DEF-231 under D-347), landing 98 (DEF-230 under D-348 step i), D-349's landing, then DEF-165 under D-337, DEF-164, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, D-348 step ii (planned first), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-228 AND DEF-241 LANDED 2026-10-08 — A TEMPLATE OWNS WHAT IT BUILDS, AND ONLY THAT** (landing 95, `1.6.1e.md`'s record; built by `nitpick-compiler_31`, landed by `nitpick-compiler_32`; the library listener's O-N38 item 1) — the `&{ }` splice handed every `to_string` result and every intermediate concatenation to `npk_string_concat`, which copies both operands and frees neither, and dropped none: 24 bytes per spliced value per evaluation, invisible to D-151 (`tleak` 24,004 → 28 bytes live), and a template whose only item was a string aliased it (two owners of one body, a double free at a function's return); **each piece is the statement's temporary once copied past (D-246), a lone string place or call item is copied, a lone literal is the literal** (a template with nothing to splice is its text, at no cost — the landing's first form copied it too, found reading the sweep: the fuzzer's `op0391` had moved); `template_owner.npk`'s ten roads, `template_splice.toml` (42 against 36) and `template_literal.toml` (0 against 0). The sweep 5065 files (0 vanished, 0 appeared); the emission 559 programs, 553 byte-identical, 6 different (every one holding a template with a splice), the libraries' 3976 — 41 different, each a template with a splice, a derived `ToString`/`Debug` body or an import of one; the manifest 7025 obligations (6730 → 6730 rows, 6636 shared, ZERO moved, ZERO fell; the 94 rows not shared re-keyed, a type-id move, the multisets equal). **D-349 RATIFIED 2026-10-08 01:15** (the toolchain pin moves to the LLVM 20.1 release the distributions serve, 20.1.8, with NIKOS and Alive2 rebuilt and re-measured against it — the user: "we just can't forget the dependency chain beyond just the compiler itself"; recorded by landing 96, its own landing after 98). NEXT: landing 96 (DEF-229; D-347, D-348 and D-349 recorded; INSTALL.md placed), landing 97 (DEF-231 under D-347), landing 98 (DEF-230 under D-348 step i), D-349's landing, then DEF-165 under D-337, DEF-164, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, D-348 step ii (planned first), step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4. Before it: **DEF-227 LANDED 2026-10-08 — A TRAIT OBJECT BUILT BY THE EXPLICIT `=> dyn` CAST IS OWNED ONCE** (landing 94, `1.6.1e.md`'s record; `nitpick-compiler_31`, the first landing after the Fable pause; the library listener's F-037 in O-N36) — `x => dyn T` lowers through `emit_fit`, the one path the implicit coercion takes, and `emit_fit`'s transfer registers the cell as the statement's temporary (D-246); the cast NODE was also on `temp_producer`'s list, so the same cell was registered twice under one name, `fnem_temp_take` takes ONE entry per name, the binding kept one registration and the statement's end freed the other: `dyn Speaks:d = move(l) => dyn Speaks;` then `d.say()` read the allocator's 0xAA poison (170 for 7) at both legs, with and without `move`, over a POD and over an owning struct, and a function that returned freed the cell again — a use after free in safe code; **the cast is not a producer now** (its cell is `emit_fit`'s registration, as the implicit coercion's is). `dyn_cast_owner.npk`: nine roads and two controls, exit 0 at both legs, the parent 95. THE PAUSE'S LOG REGISTERED (`.internal/handoff_31/FINDINGS_LOG.md`, the library seats' O-N35…O-N39): **DEF-228** (the `&{ }` splice leaks its `to_string` temporary: SILENT; landing 95, built and measured — `tleak` 24,004 → 28 bytes live) and **DEF-241** (a template whose ONLY item is a string aliased it, found probing DEF-228: a function's return freed one body twice; landing 95), **DEF-229** (the compiler does not terminate on an unbounded generic instantiation: `deep::<Box<T>>` inside `deep<T>`; landing 96, built — the checker refuses the direct shape at the call, TYPE-018 naming the function and the type, and the emitter's cap at `GENERIC_DEPTH` refuses the indirect cycle in a third of a second), **DEF-230 with S-129** (a write through a `fixed uint8[]` lands — D-074's promise against D-287's reading: the user's), **DEF-231** (a `frac` prints its stored parts, read as another number, and −3/8 has TWO stored forms, `(-1) + 5/8` printing "-1 5/8": the user ruled it a wrong answer on 2026-10-05 and gave the rule — a reader sees the value as it is used, never as stored; D-347 in preparation: whole and num of one sign, |num| < denom, value = whole + num/denom always, one form per value), DEF-232…DEF-238 (the lower-priority compiler rows), DEF-239 (the reference rows the compiler contradicts), DEF-240 (`npkg`'s stage list), S-130 (the performance findings: the floor at -O0, the checked path beside the envelope defeating inlining, the allocator's per-free cost; the floor through `opt -O2` recommended as its own measured landing). The sweep 5,060 files (0 vanished, 0 appeared; 1,418 more files than at 93: the fuzzer's session 9); the emission 554 programs, 553 byte-identical (the new test the one difference), the libraries' 3,976 — 4 different, F-037's own reproducers and M11's `as0453`; the manifest 7,025 obligations (6,730 → 6,730 rows, 6,730 shared, ZERO moved, ZERO fell — it did not move). **THE COMPILER SIDE RESUMED 2026-10-07 20:00 at the user's go.** NEXT, wrong answers first: DEF-231 under D-347, landing 95 (DEF-228, DEF-241), landing 96 (DEF-229), DEF-230 under S-129, then DEF-165 under D-337, DEF-164, DEF-159, D-342's and D-343's refusals, DEF-202, DEF-224, DEF-226, DEF-232…DEF-238, step 3c (D-339), D-341's plan, step 3d (DEF-239 and DEF-240 with it), step 4; the install README (the user's priority of 2026-10-08, drafted from a fresh-clone run) beside them. Before it: **DEF-225 LANDED 2026-10-01 — A BARE `#unreachable();` LEAVES** (landing 93, `1.6.1e.md`'s record; `nitpick-compiler_30`; the library listener's O-N34) — the bindings analysis decides "does this statement leave" (`stmt_exits`: which arm a merge drops) and "may it complete" (`stmt_completes`: FLOW-001) from two lists of statement KINDS, and `#unreachable()` is an expression, so as a statement of its own it fell through: `if (r.is_error) { #unreachable(); }` and then `r.value` was TAINT-001 where the same arm ending in `!!! Unreachable;` or `exit` compiled, and beside it ASSIGN-001 (the binding the other arm assigned), ASSIGN-002 (a `fixed` binding written in the arm), MOVE-001 (a move in the arm) and FLOW-001 (a body ending in it, under a message that says "or a trap") — five refusals of correct programs, one missing row; **one helper (`stmt_is_unreachable`) is read by both predicates**, beside the trap statement's row. ONLY THE BARE STATEMENT: under another expression the statement completes (`unreachable_edge.npk` holds the edge); the encoder's `stmt_falls_through` keeps its own list on purpose; no emission changes (the analysis feeds refusals alone). REGISTERED OPEN: **DEF-226 — the `pick (r.is_error)` form's check is not carried out of the `pick`** (`pick (r.is_error) { (true) { exit 1i32; }, (false) { } }` then `r.value` is TAINT-001 with every leaver; write the `if` form). The sweep 3,642 files (22 sites vanished, 0 appeared: all in the landing's own three test files); the emission 553 programs, 551 byte-identical, the libraries' 2,608 unmoved; the manifest 7,025 obligations (6,730 → 6,730 rows, 6,535 shared, ZERO moved, ZERO fell). (The compiler side paused from this landing until the Fable allowance reset, 2026-10-07 20:00: `nitpick-compiler_31`, on Opus, logged what the library seats sent and acted on none of it; landing 94 registered the log.) Before it: **DEF-183 PART B LANDED 2026-10-01 UNDER D-340 — A MACRO ARGUMENT RESOLVES AT THE INVOCATION SITE, AND THE VERIFIED BUILD NO LONGER PROVES A FACT ABOUT THE WRONG LOCAL (DEF-223)** (landing 92, `1.6.1e.md`'s record; `nitpick-compiler_29`) — measured on the compiler before it, the defect had seven faces where the question named two: `#twice(shared)` beside a module `shared` answered 200 for 10, a caller's local was RESOLVE-002, a local or a `for` binding the BODY declared captured the argument (2 for 8, 3 for 30), a declaration macro's argument was captured by the emitted function's PARAMETER (10 for 105) and by an emitted module's binding, and `comptime` folded the module's binding of the argument's spelling; **the expander marks an argument's root with how far out it was written, as COUNTS in the node's flag word** (`back`, `HYG_SITE`, `roots`; the clone carries them, `instantiate_expr` composes them) **and the resolver steps back through a chain of site frames** (`site_enter`; a count that does not fit is RESOLVE-INTERNAL, never the module's scope); DEF-221 (`#caller` in an inner invocation's argument lost its mark in the clone: 101 for 6) and DEF-222 (`#caller` in an argument outside every macro body compiled: MACRO-008) FIXED, DEF-220 registered and WITHDRAWN by the chain (an ALIAS's target's `#caller` reaches the alias's caller, D-125: kept, and stated in the reference); **DEF-223 FIXED, A SOUNDNESS HOLE OF THE VERIFIED BUILD NEEDING NO MACRO: the SMT encoder resolved identifiers by SPELLING** — a callee's `requires d > lim` over a module `lim` was proven at the call site against the CALLER's local `lim`, and the verified binary divided by zero (107) where the plain build stops with `RequiresViolated` (115); `ident_bind` (a module-scope symbol never reads a local's binding) and `enc_prepare` (a spelling a hygienic identifier uses that the function declares twice is never named). The sweep 3,635 files (30 sites vanished, 6 appeared: our new tests and two nitpick-posix probes of this defect); the emission 551 programs, 548 byte-identical, the libraries' 2,604 unmoved; the manifest 7,025 obligations (6,695 → 6,730 rows, 6,450 shared, ZERO moved, 2 fell (both a MOVE: the one discharged `overflow` row of `clone_expr` and of `clone_stmt` went with the body to `clone_expr_node` and `clone_stmt_node`)). Before it: **DEF-205 LANDED 2026-10-01 UNDER D-346 — A FLOAT LITERAL FITS ITS TYPE, AND THE 15-DIGIT RULE IS LIFTED** (landing 91, `1.6.1e.md`'s record; `nitpick-compiler_27`) — `1.0e39f32` and `1.0e999f64` were infinity and `1.0e-60f32` zero, in silence, on eight roads of eight: a literal that rounds to infinity, or a nonzero one that rounds to zero, is `NITPICK-TYPE-031` at the literal, at both widths (a subnormal result is rounding and stays; an infinity is computed, never written) — one sentence (`float_fit_refusal`, numeric.npk) asked by the checker of every literal it types and, **found by probing after the rule looked complete, by the compile-time evaluator of a literal under a `comptime(…)` operand, which the checker never types** (`comptime(1.0e999f64)` was still infinity; the same road had carried a sixteen-digit `flt32` literal past the 15-digit rule since landing 88); **the emitter holds the promise** (no constant for a literal that does not fit: EMIT-002, never a silent infinity); the 15-digit rule on `flt32` is gone (TYPE-030 keeps `flt128` alone). The sweep 3,625 files (105 NITPICK-TYPE-030 sites vanished, the lifted digit rule in our own tests and in three fuzz programs, and 18 NITPICK-TYPE-031 appeared, all in `float_fit.npk`); the emission 546 programs, 545 byte-identical; the manifest 6,990 obligations (6,702 → 6,695 rows, 6,458 shared, ZERO moved). Before it: **1.6.1e STEP 3b LANDED 2026-10-01 — `comptime` ORDERS STRINGS (D-338), AND S-119…S-126 ARE RATIFIED AS D-339…D-346** (landing 90, `1.6.1e.md` §2.10d and its record; `nitpick-compiler_28`) — the compile-time evaluator holds a payload-less enum variant as a value (`CV_ENUM`: the enum's type and the variant's position), compares two with `==`/`!=`, folds `a.cmp(b)` on two constant strings by the order the run time's `impl:string:Ord` computes (unsigned bytes, then length; `comptime_string_order.npk` asks the evaluator, the machine and the literal answer of twenty pairs), folds `?!` and `?|` through a call it completed, and runs a `pick` — statement and expression form, source order, guards, ranges by the selector's signedness, `fall`, `give` — INSIDE A `comptime func:` CALL AND NOWHERE ELSE (an arm's assignment folded where it stands in a run-time body would never run); an expression arm ends in `give`; an enum value does not leave the evaluator (`comptime(…)` over one TYPE-004 with its own sentence, a module binding TYPE-035 as before: S-127, the user's). The library listener's `mc0309b` answers `run:0`. Two defects its probes found are FIXED with it: DEF-211 (the evaluator's `&&`/`||` evaluated both operands — a division its guard protects was TYPE-004 at compile time where the run time says `false`) and DEF-213 (`fold_const` reset the depth mid-evaluation, so a `comptime func:` recursing through a type's array size stopped the compiler with exit 3: TYPE-025 now). REGISTERED OPEN: DEF-212 (two value arms with one value reach `llc`: "duplicate case value"), DEF-214 (a `comptime func:` is emitted nowhere, and a call of one outside a constant context reaches `llc` undefined — with S-128: is one also a run-time function? recommended no). **THE USER RATIFIED S-119…S-126 on 2026-10-01** ("all the recommendations look fine to me. lets go with those.", in `nitpick-compiler_26`'s session), recorded here as D-339 (the lock floor closed, the prelude's traits undeclared), D-340 (a macro argument resolves at the invocation site), D-341 (`buffer` gets checked indexing and slicing), D-342 (an enum values every variant or none), D-343 (a macro parameter that only names a declaration is MACRO-011), D-344 (a macro body does not name the landing declaration's type parameter), D-345 (a carried family's constant is written, not computed), D-346 (a float literal that rounds to infinity or to zero is refused; the 15-digit rule lifted). The sweep 3,624 files under both checkers (107 sites vanished, 2 appeared, all in the landing's four new test files and `mc0309b`); the emission byte-identical for every program that compiles on both (546 programs, 544 identical, 0 different, the 2 new programs; the libraries' 2,602, none different); the manifest re-recorded (6,997 obligations; 6,680 → 6,702 rows, 6,412 shared, zero moved). Before it: **DEF-209 LANDED 2026-10-01 — A `flt64` LITERAL IS WRITTEN AS ITS BITS: NO DECIMAL A PROGRAM WRITES IS LLVM'S TO CONVERT** (landing 89, `1.6.1e.md`'s record; `nitpick-compiler_27`) — a `flt64` literal was `double <text>` and LLVM's parser decided its bits: right within an exponent of 24,000 and WRONG past it (the parser caps the exponent it reads: `0.<24000 zeros>1e24001`, the number 1.0, was the double 0.1, `1<24001 zeros>.0e-24001` was 10.0, a longer one zero — measured through the whole compiler at both legs; the solver's term was the number written, so a verified build discharged `prove(x == 1.0f64)` over a binary that held 0.1): **`float_const_text` writes `0x` and sixteen hex digits at both widths from `float_round`** (one branch where there were two; `float_literal_body` is gone). Every float program's emitted TEXT moves and no constant's VALUE does but those literals — **proven on two independent legs over every differing emission pair** (the tree's 34, the libraries' 31): the two texts are identical once every floating-point constant is read as its bits (the old decimals by exact rationals), and LLVM makes ONE artifact of both (the `-O0` object, the `opt -O2` text and the `-O2` object byte-identical) in every pair but the 2 that hold a literal past the cap; and every distinct decimal the old emissions held within the cap (593 texts) is read by the pinned LLVM as the nearest double; `float_round`, the one authority at both widths now, agrees with exact rationals on 566,680 adversarial texts. Tests: `float_literal_kat.npk` gains four texts past the cap and six roads (`88aa21d` fails each), `tests/verify/flt64_past_cap.npk` (`88aa21d`'s verified binary exits 1). REGISTERED OPEN: DEF-210 (a refused `#[derive(Copy)]` over an array says `Clone`). TRAITS_REFERENCE's derive section corrected (eight derivable traits; what `Copy` admits — the library seat's note). The sweep 3,612 files (0 vanished, 0 appeared); the emission 544 programs, 510 byte-identical, 34 different in text, 0 refused by `88aa21d` alone; the manifest 6,975 obligations (6,690 → 6,680 rows, 6,586 shared, ZERO moved). Before it: **DEF-203 LANDED 2026-10-01 — A `flt32` LITERAL IS THE NEAREST FLOAT TO THE NUMBER WRITTEN** (landing 88, `1.6.1e.md`'s record; `nitpick-compiler_26`) — a `flt32` literal lowered as `fptrunc double <text> to float`, two roundings, and D-143's "equal to one below sixteen digits" was false (`9.51125303839185e-19f32` compiled to `0x218C5C80`, the nearest float is `0x218C5C7F`; about one written number in 2³⁰, on every road a constant takes): **the compiler converts the decimal itself** — `float_round` (numeric.npk), exact, one rounding to nearest with ties to even, generic in the format, no wide integer, held to exact rationals by 2,072 generated vectors — and **the emitter has one writer of a float constant** (`float_const_text`: a `flt32` is its BITS, `float 0x…`); the solver's term (`fp_literal`) changed in the same commit, so the program and the proof hold one number; a `flt32` module constant compiles; **`flt32` rows are eligible for the Real-interval tier for the first time** (a `flt32` literal had been a `narrow` definition, never a bound); measured for the first time: LLVM's own decimal→double parse is correctly rounded on the pinned toolchain for an exponent within 24,000 (8,890 adversarial texts, none different; `float_literal_kat.npk` holds it on every run) and WRONG past it; **DEF-206** fixed with it (an unsuffixed fraction in a `flt32` or `flt128` slot, bare or under `comptime`, skipped D-143's literal rules: `flt128:x = 1.0;` died as EMIT-002) and **DEF-207** (a float literal with a six-digit exponent stalled a build that writes obligations and a twenty-digit one trapped it: `decimal_to_real` is bounded). REGISTERED OPEN: **DEF-209 — a `flt64` literal still rides LLVM's parser, which caps the exponent at 24,000: `0.<24000 zeros>1e24001`, the number 1.0, is the double 0.1 (a silent wrong answer in a literal of some 24 KB; the next landing writes a `flt64`'s bits too)**, DEF-208 (`tfp_q_decimal` refuses a fixed-point literal by its exponent alone). The sweep 3,611 files (14 sites vanished and 4 appeared, all in the landing's own three test files); the emission 543 programs, 535 byte-identical, 7 different, 0 newly refused, 1 refused by `789ffdc` alone (`flt32_literal.npk`, for its module constants); the manifest 6,985 obligations (6,560 → 6,690 rows, 6,453 shared, ZERO moved). Before it: **DEF-179 LANDED 2026-10-01, WITH THREE SILENT WRONG CONSTANTS ITS PROBES FOUND** (landing 87, `1.6.1e.md`'s record; `nitpick-compiler_25`) — **a module-level constant of every literal family is the number its literal is in a function body**: `fixed flt64:PI = 3.14f64;`, a `tfp`/`dim256` value, a negated one, `= PI`, `comptime(2.5f64)`, a top-level `ERR`/`NIL` and `3f64` compile (the folder's value gains `CV_REAL`, a constant CARRIED as its literal's text and never computed); **DEF-197**: `fixed tfp64:Q = 2tfp64;` was `constant i64 2`, `fixed tbb8:T = 100tbb8 + 100tbb8;` was `i8 200` where the run time answers ERR, `fixed tryte:C = 29524 + 1;` was `i16 29525` — the folder read every twisted literal as an untyped plain integer and the global's renderer wrote it whatever the type (no such binding exists in 40,928 files); the folder computes in the plain integers only (`fold_computes`), and an expression over a carried family at module level is NITPICK-TYPE-035 "written, not computed"; **DEF-198**: `int8:a = comptime(100 + 100);` stored `i8 200` and `comptime(BIG)` over an `int64` binding stored a truncated `i32` — a folded name has its binding's type and an untyped `comptime` value must fit its slot (TYPE-031); **DEF-204**: a `pick` arm over a fixed-point selector never matched (`(2tfp64)` compared its integer payload with the selector's Q value: a silent wrong branch; `pat_q_text`); DEF-199 (`3f64` reached `llc` as `double 3`; `100000000f32` trapped the checker), DEF-200 (a top-level sentinel was refused by a second gate in the bindings analysis, deleted), DEF-201 (three unchecked narrowings: `int8[4294967298]` was an array of two). REGISTERED OPEN: **DEF-203 — a `flt32` literal is two roundings and D-143's 15-digit reason is false (`9.51125303839185e-19f32` is one ulp off the nearest float; a `flt32` module constant waits for the same conversion: TYPE-035 by name)**, DEF-202 (a reference to an aggregate binding), DEF-205 (`1.0e39f32` and `1.0e999f64` are infinity and `1.0e-60f32` is zero, in silence: D-148's "must fit" has no float row), **S-125** (may the compiler compute in the carried families: recommended no) and **S-126** (refuse a float literal that rounds to infinity or zero; the 15-digit rule after DEF-203). The sweep 3,605 files (6 vanished, 17 appeared, all in the landing's own three test files); the emission 540 programs, 538 byte-identical; the manifest 6,855 obligations (6,553 → 6,560 rows, 6,291 shared, ZERO moved; four pairs read as fallen, one renamed function's). Before it: **DEF-192 AND DEF-196 LANDED 2026-10-01** (landing 86, `1.6.1e.md`'s record; `nitpick-compiler_24`) — two silent wrong answers closed: **a TYPE's name in a macro body resolves where the macro was written** (D-057, which the type resolver had never been told: `#size_of<T>()` in a body, invoked inside `func:g<T>` or a method of `impl:<T>:Box<T>`, measured the CALLER's type beside a module `struct:T`, 8 for 4) — it binds a generic parameter only where the body itself declares it, asked of the node's span (`ast_macro_at`, `macro_may_bind`), and a body that compiled by the capture is TYPE-001 ("in the scope this macro was written in"); and **compile-time evaluation keeps a call's variables by the node that declares them** (`FoldEnv` by the symbol's origin: a nested block's local overwrote the outer one — 12 at compile time where the same body answers 11 at run time — a macro body's free name read a `comptime` local, a name inside a type read one where the checker reads the module's binding). The sweep 3,601 files (only the new test's own sites move), the emission 536 of 538 byte-identical and 1,562 of 1,562 library and application programs, the manifest 6,848 obligations (6,536 → 6,553 rows, 6,277 shared, ZERO moved, ZERO fallen). **S-124** registered for the user (may a body name the landing declaration's type parameter: recommended no). Before it: **DEF-193 LANDED 2026-10-01** (landing 85, `1.6.1e.md`'s record; `nitpick-compiler_24`) — **an enum variant's explicit value is CHECKED at its declaration**: `NITPICK-TYPE-093` (new) for a value that is not a bare integer literal (`X = 7i32 + 1i32`, `X = BASE`, `X = -3i32` were each IGNORED and the variant took its position), a value outside `[0, 2^31 − 1]` (truncated before) and a tag another variant already has (two variants compared equal; an unvalued variant's tag is its position) — `check_decl` had no arm for an enum at all; the sweep 3,598 files (only the new test's own sites move), the emission 536 of 536 byte-identical, the manifest 6,831 obligations (6,527 → 6,536 rows, 6,435 shared, ZERO moved, ZERO fallen); what a value MAY be is S-122, the user's. REGISTERED: **DEF-196 — compile-time evaluation keeps a call's variables BY NAME** (a nested block's local overwrites the outer one: 12 at compile time where the same body answers 11 at run time; a macro body's free name reads a `comptime` local; a name inside a type reads one where the checker reads the module's binding), and DEF-192's second face (the invoking `impl`'s generic parameter captures a body's type name too). Before it: **1.6.1e step 3a LANDED 2026-10-01** (`1.6.1e.md` §2.10c and its record; `nitpick-compiler_24`) — **DEF-189 FIXED: macro substitution is TOTAL** (a parameter written in a type, a pattern, an attribute or an emitted declaration's signature is the argument; the clone had copied those four through by id since 0.6.1, and beside a module binding of the parameter's spelling the body silently read the binding — `int32[N]` was the module's `N` whatever was passed): every declaration and pattern of a body is cloned per instantiation, a type exactly where it holds an expression; a parameter standing where a type's NAME stands, or declared by the body AND written in it as an expression, is `NITPICK-MACRO-011` (new) at the macro's declaration; `#caller(name)` in a body's type or pattern is legal; a `pub Rules` a macro emits keeps its `pub`. **And two older silent defects the step's own test found, fixed with it: DEF-191 — a generic call reached the FIRST instance recorded wherever two instances share a signature** (a type parameter that appears only in the body: `bytes_of::<int64>()` answered `bytes_of<int32>`'s 4, since 1.0.2b; the checker records each call's type arguments, `ExprTypes.callee_targs`, and the emitter matches by them) **and DEF-195 — a soundness hole of the VERIFIED build**: a generic `pure never fails` callee was ONE uninterpreted function for every instance, so `width(1i8)` and `width(1i64)` were one term, a `div-zero` row over their difference was discharged and its guard elided where the plain build traps; the function symbol carries the instance now. REGISTERED OPEN: **DEF-192** (a type name in a macro body is captured by the invoking function's generic parameter — a silent wrong answer), **DEF-193** with S-122 (an enum variant's explicit value is honoured only as a bare literal and never checked: an expression is ignored, two variants may share a tag), DEF-194, **S-120** (whose scope a macro ARGUMENT resolves in: DEF-183, whose silent face answers 200 for 10), **S-121** (a `buffer` has no safe typed access), **S-123** (may a parameter name an emitted declaration — MACRO_REFERENCE §10's open question, which the step's first form would have settled by refusing and the sweep caught: the library listener's `mc0388`). The sweep 3,597 files (12 vanished, 18 appeared, every one read), the emission 533 of 536 byte-identical (the 2 different are the two generic-instance tests), the manifest 6,822 obligations (6,420 → 6,527 rows, 6,219 shared, ZERO moved, ZERO fallen). Before it: **1.6.1e step 3 LANDED 2026-10-01** (`1.6.1e.md`'s record; read, designed and integrated by `nitpick-compiler_22` with three implementation agents in three worktrees, reviewed, documented and landed by `nitpick-compiler_23`) — the library listener's F-027 rows (DEF-153), each READ on `5fbaf4a` before it was built (§2.10b: five of the plan's twelve decisions changed and one planned refusal was withdrawn — a spliced `fixed` qualifier TRAVELS to the field, so `mc0085b` is a documentation row): `flt256`/`flt512` are refused by the resolver (TYPE-001); **a float literal is read by its production** — `NITPICK-LEX-009` for a tail that is no `TypeSuffix` (DEF-166), and inside the scan **DEF-174, a silent wrong answer** (a sign after a float was swallowed into the literal: `3.5f64-x` computed 3.5) and DEF-175 (`1_000.5tfp64` folded to 1.0); a fraction with an integer suffix is TYPE-031 (DEF-167); `suspend_until`/`suspend_io`/`io_watch` are TYPE-043 outside `async`, the two parks in `defer` too (DEF-168: `io_watch` in a sync body faulted the machine; DEF-169: a park in `defer` trapped the compiler); a compile-time failure names its call chain; **a channel's `send`/`recv` waits at its LEVEL** (LOCK-001) **and an implementation of a trait method that declares no `acquires` level may acquire nothing** (LOCK-002 — D-056's own two sentences, kept at last; so no impl of a prelude trait may lock or wait), the lock walk reading the checker's call record where it resolved four call forms to nothing; **the expansion walk reaches every expression position** (DEF-170: MACRO-006 43 sites → 0), an alias is passed through at every declaration site (DEF-171), **a macro cycle is refused at its declaration** and the round bound reports the invocation left standing (DEF-172, DEF-173). REGISTERED OPEN: DEF-176…DEF-188 (the rows' neighbours; DEF-181 the lock walk's six holes, with S-119 for the user), **DEF-189 — a macro parameter written in a type or a pattern is never substituted and silently reads a module binding of that spelling (a wrong answer, older than the step, found by its review)**, DEF-190 (a trailing `_` in a numeral). **D-336, D-337 and D-338 RATIFIED** (the user, 2026-09-30: "go with your recommendations on all three"): a frozen compatibility corpus built at the freeze (S-117), one TYPE-079 per struct literal naming every sealed field (DEF-165's reading), and `comptime` orders strings (S-118 — `mc0309b`, step 3b). The sweep 3,591 files under both checkers (75 sites vanished, 91 appeared, every one in our own tests or in ten of the listener's m11 reproducers; no file of the libraries or applications moves); the emission byte-identical for every program that compiles on both (533 programs, 528 identical, 0 different, 0 newly refused, 5 the new tests); the manifest re-recorded (6,714 obligations; 6,291 → 6,420 rows, 5,863 shared, zero moved, zero fallen). NEXT: DEF-189 (its own landing, planned first), then step 3b (D-338), DEF-165 under D-337, DEF-164, DEF-159, step 3c (the lock walk, DEF-181; S-119 with numbers), step 3d (the neighbours), step 4 (DEF-154), 1.6.1f's plan, then 1.6.1 step 2. Before it: **D-332's runner rule LANDED 2026-09-27** (between 1.6.1e steps 2 and 3; `1.6.1e.md`'s record): both runners hold each code's reported site COUNT to its `expect-error` lines (`match_findings`, `counted_sites`, the self-checks' `silent_site`), and its first run read thirteen of the tree's 235 rejection files -- nine headers re-spelled one line per site, and DEF-161 (a refused `..^` to `sys` fit-checked too), DEF-162 (`simd(…)` under a refused annotation reporting it missing) and DEF-163 (`not_constant.npk` expecting a refusal D-222 made legal; the messages naming `const`) FIXED; over the listener's corpus five files would fail on a re-pin (F24) and two shapes are registered OPEN, DEF-164 (the parser's recovery reports `error:E(P);` twice) and DEF-165 (a sealed-field literal reported per field, the reading the user's). Before it: **1.6.1e step 2 LANDED 2026-09-26** (`meta/roadmap/1.6/1.6.1e.md`'s record) — the library listener's F-023…F-026: **DEF-149** (an un-awaited `async` METHOD call is TYPE-043 as a free call is), **DEF-150** (the compiler's three stops: a macro body of both kinds is `NITPICK-MACRO-010`, a declaration's modifiers are read past, and **no tree nests deeper than 256 levels** — `AST_DEPTH_MAX`, measured per declaration by `ast_depth.npk` over the AST's construction log, `NITPICK-PARSE-012` once, a splice landing past it `NITPICK-MACRO-003`, the analyses' depths reading the one constant; the first form counted the parser's descent alone and a 600-term left chain still trapped the folder), **DEF-151** (`--extra-picky=no-wildx` walks the program's modules and reads four spellings; `// npkc-flags:` in a file's header, both runners), **DEF-152** (a builtin scalar with type arguments is TYPE-016); NEXT: 1.6.1e step 3 (DEF-153, the sixteen rows — design, Fable's; its notice is F25), step 4, DEF-159, DEF-164 and DEF-165, then 1.6.1 step 2; and **S-116 SETTLED as D-335** (the user, 2026-09-26: wide strings are IN — `string<char16>`/`<char32>`, byte strings today, are designed and implemented as **1.6.1f** before the freeze, its plan Fable's), with S-117 (a frozen compatibility corpus as the permanent freeze's instrument) registered for the user. Before it: **1.6.1d step 4 LANDED 2026-09-26 — 1.6.1d IS COMPLETE** (`meta/roadmap/1.6/1.6.1d.md`'s record): the library listener's F-009, F-010, F-015, F-016, F-017, O-N29 and O-N32 — **DEF-124** (a `move` parameter re-initialised after a move is readable again: the bindings analysis marks a parameter's assignment), **DEF-125** (a moved-out owning value carries what its place holds, not its root's identity: a swap of two owned strings through a lent `dyn` compiles; a view held in a field and moved out is still BORROW-001), **DEF-126** (a mismatch between two same-named types names their modules: "expected `same_name.Row`, found `rows.Row`"), **DEF-131 under D-330** (`<=>` LOWERS — the operands once, `<` and `>` composed, `-1`/`0`/`1` as `int32`, the folder's fold and the encoder's `ite`; a float or a frac pair `NITPICK-TYPE-088`, a `string` or a `tbb` no ordering at all), **DEF-132** (a negative range-pattern bound lowers through the folder), **DEF-133** (six reference sentences corrected; d7 under **D-331**: a certain constant division is `NITPICK-TYPE-004` wherever the folder decides the pair — a local's initialiser, an argument), **DEF-142** (an arm naming a variant its enum lacks, or another type's name before the dot, is `NITPICK-RESOLVE-002` at the pattern where it was EMIT-002), **DEF-143** (`mod:error;` is PARSE-001 at the keyword, one report) — and **DEF-160, a memory-safety hole the step's own probe found**: an enum payload's address was tracked by nothing (the layout wrote no pointer bit for an enum, and the constructor `E.Some(@local)` — method-call-shaped, no callee — read as an intercepted builtin method whose arguments carry nothing), so `E.Some(@local)` returned from a function compiled since D-261 and the caller read a dead frame (exit 3 at -O0); the layout records the bit from the payloads, the three pointer tables read it, and a constructor carries every argument — the three faces refuse now. The sweep 3,570 files under both checkers (13 vanished, 17 appeared: the listener's two over-restrictions gone, its two constant-division programs TYPE-004 as their `expect` lines say, ty0365b TYPE-008 alone, our new tests' own sites; nothing else of the listener's moves); the emission byte-identical for every program that compiles on both (526 programs, 520 byte-identical, 0 different, 0 newly refused, 6 the new tests); the manifest re-recorded (6,550 obligations; 6,249 → 6,257 rows, 6,025 shared, zero moved, zero fallen). NEXT: 1.6.1e steps 2–4, then D-332's runner rule, then 1.6.1 step 2. **1.6.1d step 3b LANDED 2026-09-26** (`meta/roadmap/1.6/1.6.1d.md`'s record) — **DEF-122 FIXED under D-328 and D-333** (the library listener's F-007: every path conversion leaked `len + 1` bytes since 0.6, a view of storage nothing owned): `cstring` is `{ ptr, len, cap }` — `cap == 0` a body it does not own (a literal in `cstring` position, an `argv`/`environ()` element), `cap > 0` the copy `to_cstring` makes, dropped at its scope's exit through the string's drop body — move-only (TYPE-046 on a binding-to-binding copy; `.clone()` through the prelude's new `impl:cstring:Clone`; `move` to transfer), riding a channel and crossing a spawn as a `string` does; the literal in `cstring` position LIVE (D-049's compile-time form; an interior NUL `NITPICK-TYPE-092`); the `cap` member; the floor's six `cstring` symbols take and build the trio, so the floor's bytes moved and the snapshot was refreshed in TWO hops (the old builder on the old floor, then the new compiler on both; stage2 == stage3, 29,922,613 bytes, `0787c5e2…`); `tests/cost/to_cstring.toml` holds the churn's peak to the once's (10 bytes where 40,000 were live); the two-checker sweep over 3,555 files found nitpick-time's fifteen `cstring` copies in eight test files (TYPE-046 from this landing; the census at the plan had missed them), named in F20 with their spellings; the manifest re-recorded (zero moved, zero fallen), the floor's 388 rows re-recorded (two re-keyed), D-303's sweep 45 of 45. NEXT: 1.6.1d step 4, then 1.6.1e steps 2–4, then D-332's runner rule, then 1.6.1 step 2. **1.6.1e step 1 LANDED 2026-09-26 (`meta/roadmap/1.6/1.6.1e.md`'s record) — the library listener's M11 findings F-018…F-022: DEF-144 (the emitter resolves an identifier by the RESOLVER's symbol, `ident_slot`, so a macro body's free name is emitted as D-057 binds it — it read the caller's local of the same name), DEF-145 (a character literal has a WIDTH: `'\u{…}'` and a source character above U+00FF are `char32`, a `char8` slot refuses the escape, TYPE-007), DEF-146 (the scope's join relays the EARLIEST-SPAWNED child's error, `main`'s join too — S-115 ratified as **D-334**, the user, 2026-09-26: "go with what we have"), DEF-147 (an expired `timedwait` is `DeadlineExceeded` with the guard spent, the mutex not re-acquired; no floor byte), DEF-148 (a spawn LENDS its sanctioned crossing until the block's join: `destroy`, an assignment, a `move`, `$$m` or `@s` to a callee that stores over it is `NITPICK-BORROW-016`; the kind's own concurrent operations stay), and two the step's probes found: DEF-157 (`destroy` through a pointer or of a temporary is `NITPICK-TYPE-091` — it freed the arena the owner freed again, `MachineFault` in safe code) and DEF-158 (`destroy` on a field vacates the field; it cleared the aggregate's flag and leaked the siblings); DEF-159 registered OPEN (a value stored through a held `@x` after `move(x)` leaks). NEXT: 1.6.1d steps 3b and 4, then 1.6.1e steps 2–4. Before it: **1.6.1d step 3 LANDED 2026-09-26 — the leaks: DEF-120 (`(<-p) = v` drops the old pointee when `p` names MANAGED storage — the escape analysis's wild-provenance reading, `wild_places.npk`, is the one predicate; a `wild` binding, parameter, field, an `=>! wild` cast, `#ptr_add`, an allocator's result stay drop-free; with it a moved-out WHOLE binding keeps the vacant value, D-254's rule beside the field's), DEF-121 (THE ARM IS A SCOPE in the emitter: a consuming `pick`'s bindings are dropped at the arm's end and at every exit from it; a coroutine's binding flag is the frame byte at role `50 + ordinal`), and four pick-arm defects the step's probes found — DEF-138 (a `move` of the arm's own binding inside its `where` guard double-freed when the guard failed: `NITPICK-TYPE-089`), DEF-139 (a `fall` into an arm that binds read a payload its pattern never matched: `NITPICK-TYPE-090`), DEF-140 (a `fall` to a label no arm carries was EMIT-002: RESOLVE-002 now), DEF-141 (a consuming `pick` EXPRESSION left its selector on the statement's temporaries, and `give move(x)` handed out a body the statement then freed: exit 95 on every compiler before this one); **D-332** (S-113: both runners hold a rejection file's per-code site COUNT) and **D-333** (S-114: a `cstring` rides a channel and crosses a spawn as a `string` does, under D-328) recorded — the user, 2026-09-26: "go with your recommendations on those two questions. they look fine to me."; the listener's O-N32 registered as DEF-142 (a `pick` arm naming a variant its enum lacks is EMIT-002) and DEF-143 (RESOLVE-012's message drops the keyword), both step 4's; the library listener's M11 findings (nitpick-fuzz F-018…F-028: four silent wrong answers, a use-after-free through a destroyed `shared_arena`, three compiler traps, an accepted un-awaited async method, `--extra-picky=no-wildx` refusing its own prelude, a unit on `tfp64` ignored, two tables) REGISTERED as DEF-144…DEF-154 for a subcycle **1.6.1e**, to be planned execution-grade next, its first step the wrong answers and the memory fault; step 3b (DEF-122 under D-328: the floor's `cstring` layout and a two-floor snapshot refresh, split from step 3 at the floor's seam) and step 4 remain, their order against 1.6.1e's first step the user's; then 1.6.1 step 2.** **1.6.1d step 2 LANDED 2026-09-26 — the loops (D-328…D-331 recorded: the user's four ratifications of the day, "go with your recommendations on all four"): DEF-128 under D-329 (a range value keeps its spelling, `{ lo, hi, inclusive }`; the `for` compares by the element's signedness and tests `last` before it adds — `0u8..255u8` runs 256 times where it ran zero), DEF-129 (a counted loop's operands widened by their own signedness; a `uint64` bound past 2^63 traps `IntOverflow`, an `overflow` row), DEF-130 (`till` ascends, emitter and evaluator), DEF-135 (a twisted ERR bound traps `TbbErr`, an `err-exit` row — it ran zero times in silence), DEF-136 (a wide or non-integer bound is TYPE-068), DEF-137 (a per-process temp path spells its pid fixed-width with constant work, `sys_pid_tag()` — landing 72's red harness read: the pid counter wrapped between two replays of one seed); the listener's twenty F-012/F-013/F-014 programs answer the reference at both legs; NEXT: step 3 (the leaks: DEF-120, DEF-121, DEF-122 under D-328), then step 4 (DEF-124, 125, 126, 131 under D-330, 132, 133 under D-331), then 1.6.1 step 2.** **1.6.1d step 1 LANDED 2026-09-26 — the memory faults of the library listener's fuzz findings (`meta/roadmap/1.6/1.6.1d.md`, planned execution-grade the same day over M9 and M10, DEF-118…DEF-134 registered, four landings by severity): DEF-118 (a consuming `pick`'s binding is in the move rules — the resolver numbers a pattern binding as a local, so a second move or a read after a move is MOVE-001 where it compiled and double-freed or read the free poison), DEF-119 (a `move(<-p)` in a callee is a write of the pointee in its summary, so `@x` to it while a view lives is BORROW-015), DEF-123 (a write THROUGH a `$$i` holder — `(<-p) = v`, `move(<-p)`, `@(<-p)` handed on, a pointer-receiver call on `<-p` — and a `$$i` claim or a pointer holding one handed to a callee whose summary writes through that position are BORROW-013), DEF-127 (a `for` binding ends with its loop in the emitter, where an outer name read the loop's slot and past it), DEF-134 (a by-value receiver method called on a pointer is handed the POINTEE, loaded — it received the pointer's bits and read garbage at -O2; found by the step's own accept file); every one of the sixteen findings reproduced first at both legs, the sweep of 2,449 files under both checkers moving nine sites (eight the listener's reproducers, one the tree's `aliasing.npk` case its comment had promised since 1.5.5 — S-113), the emission comparison 495 of 497 identical; S-109 (`cstring` ownership), S-110 (a range keeps its spelling), S-111 (`<=>` on floats), S-112 (a constant division refused everywhere) and S-113 asked of the user; NEXT: 1.6.1d step 2 (the loops: DEF-128 under S-110, DEF-129, DEF-130), then the leaks, then the rest, then 1.6.1 step 2.** 1.6.0, the bring-up gate, PLANNED execution-grade and its steps 0, 1, 2, 3b, 3c, 3d, 3e, 3f, 3g, 3h, 3, 4, 4b, 5, 5b and 5c LANDED 2026-09-25/26 — 1.6.0 IS COMPLETE** (5c: **DEF-108, D-323** — a function that fell off its end compiled and returned a ZERO value, a fallible one a silent SUCCESS, `main` an exit 0 (the emitter's fall-off return had stood in for a check since the first backend; the library listener's fuzzer found it, the user named the rule); `NITPICK-FLOW-001` refuses every body that can reach its own closing brace, from the bindings analysis's `stmt_completes` — every path ends in `pass`, `fail`, `exit` or a trap, a `NIL` function passes `NIL`; the tree's 906 files hold no fall-through outside the new test; the same commit records **D-324** (S-103…S-105: 1.6.1's NIKOS commits on a `nitpick-port` branch the user merges, the second decider on by default, the stage over every plain emission on every run) and **D-325** (S-106: a VIEW's root is FROZEN for the view's lexical lifetime — DEF-107's fix, measured on the tree first, 1.6.1 step 0's first landing) — the user, 2026-09-26: "both of your recommendations sound fine to me. lets ratify those.") **1.6.1 IS UNDERWAY — step 0 LANDED 2026-09-26** (`meta/roadmap/1.6/1.6.1.md`'s execution record): **DEF-107 FIXED under D-325** — a VIEW is a fourth party kind of D-286's aliasing walk (`PARTY_VIEW`, beside PLAIN/SHARED/EXCLUSIVE), its root frozen from the holder's declaration to its block's end (or for its call), and every write-capable access of what it views is `NITPICK-BORROW-015`; **the provenance summaries** — per function at the escape fixpoint: the VIEW entries (which parameter's bytes the result views, at which path), the PASS bit, the STORE MATRIX (what flows into each pointer parameter's pointee), the MUTATION paths — read off each callee's own body, so a known callee connects only what it stores (the shape rule of D-117/D-223 stays for a callee the analysis cannot see), and a TRAIT's method is the UNION of its impls' (the reach analysis's reading, DEF-86); **seven defects found by its probes, fixed with it**: DEF-109 (a by-value parameter's frame storage travelled up), DEF-110 (the folder's environment handed out a view it then overwrote), DEF-111 (rule B's `can_connect` re-resolved field types on every query — memoised, the checker over `src/npkc.npk` 27.5 s → 9.6 s), DEF-112 (an address or a view pushed into a `List` escaped with it), DEF-113 (a `dyn` holder was never a destination — a `dyn` and a bare `T` are read in the closed direction now), DEF-114 (a by-value parameter could not HOLD a borrow — `pholds`), DEF-115 (a lent `dyn`'s cell, the caller's, received a local's address — `BORROW-002` at the call); the D-223 counterweight (`borrow_pair_plain.npk`) carries both readings of its verdict; the measurement: 1,030 files swept with the rule built (the tree's 799 and the library listener's 231) and not one site moved outside the new tests — the listener's exposure is zero; S-107 (rule A's `holds` marking refined by the same summaries, the listener's O-N27) is the user's; **the NIKOS half of step 0 LANDED 2026-09-26 (D-324)**: the three defects of the step-4 reading are three commits on the user's `nitpick-port` branch (`070247c` the `inttoptr` constraint ADDED, `c712a3e` 128 sampled into the AR integer-alignment table, `db47f9d` the coroutine intrinsics in every switch — pushed to `origin/nitpick-port`, never `main`), the pin moved to `db47f9d` (`engines.sh`, `pins.txt`), and measured under the gate's runner: the compiler's datalayout twin is READ where the pinned build refused it (3,325 functions defined, `main` the one with checks — the executor wall), the controls 7 of 8 in both modes as before, and the intra-mode `inttoptr` effect visible on `uninit` (fewer `unreachable`, more decided); a module that calls `llvm.coro.*` is refused earlier by the importer for its `token` operands, so the coroutine mapping is reached by no legal module today (told to the maintainer in the commit). **Step 0c LANDED 2026-09-26** (the library listener's O-N28, received while step 0's harnesses ran): **DEF-116** — an impl could declare `move` on a parameter its trait lends, or lend one its trait consumes, and the result was a double free or a leak (`same_signature` compares TYPES, which carry no `move`); an impl's parameter is `move` exactly where the trait's is now, in both directions, `NITPICK-TYPE-014` at the parameter naming the direction, because a call through a bound or a `dyn` reads the TRAIT's declaration and spends its argument where it says `move`; **DEF-117** — a refused argument no longer cascades into TYPE-022 at a generic call (D-240); **S-108** for the user (a `never fails` by-value read of a generic container: a prelude `Copy` marker is the recommendation); the listener's DEF-104 exposure correction recorded. **step 1 (E-8) LANDED 2026-09-26** — every module of ours states `target datalayout` and `target triple` (the emitter's first two lines; the floor and the explorer shim by hand), pinned as `[toolchain] triple`/`datalayout` and held by both runners to what the pinned `opt` derives from the triple and to every emission, the floor and the shim (the snapshot refreshed in one hop, `ec29f358…`, 28,872,936 bytes; no object moves for the line, measured; the gate's twins retired, the plain form IS the twin); NEXT: step 2 (transform-free ingestion — the NIKOS importer learns `select`, `switch`, the atomics and constant expressions, the driver reads the emission's own bitcode, the `analyze-input` belt in both runners); and **S-107 and S-108 are RATIFIED as D-326 and D-327** (the user, 2026-09-26, in this session: "the recommendations from earlier you asked about are fine. go with those."), each a subcycle — 1.6.1b (rule A's `holds` marking read off the provenance summaries) and 1.6.1c (the prelude `Copy` marker with a derivable form and a `never fails` `list_get`) — both planned execution-grade 2026-09-26; **1.6.1b LANDED 2026-09-26** (`call_result_holds` in `escape.npk`: a call's result holds a borrow exactly when the callee's summary says it may view or carry the argument, the shape rule kept for an unknown callee; 2,157 files of the tree and the listener's repositories under both checkers, zero sites moved outside the two new tests); **1.6.1c LANDED 2026-09-26** (the prelude marker `Copy`: `T: Copy` licenses a plain copy of a `T` in a generic body, `list_get<T: Copy>` reads a `List<T>` out by value `never fails`, `NITPICK-TYPE-087` at an impl where a copy would be a second owner, `#[derive(Copy)]`; 6,199 rows before and after, 5,685 shared, zero verdicts moved, zero pairs fell; the 514 re-keyed rows a move of type-id names); NEXT: 1.6.1 step 2 (transform-free NIKOS ingestion). (5b: two documents from the library listener's report — D-292's dated note (the runtime does not count the failsafe region's bytes in `NPK_HEAP_STATS`; the 1.5.6 record's departure is what landed) and **DEF-107 registered with S-106 for the user**: a VIEW's root may be written while the view is live and the view then reads freed memory in safe code on both legs — D-249 stops a view escaping, D-286's claims never include a view, D-266 froze a lending pick's selector alone; the recommendation is the freeze of a view's root for the view's lexical lifetime, measured on the tree first) (5: **D-322** — NIKOS IS LEG A'S ENGINE, by D-320's rule 1 on row 2 (seven of eight planted defects found; Clam had no memory result and no `div-zero` check and is DECIDED OUT with its scorecard kept); E-8 settled in the same sentence — the emission states its own data layout, 1.6.1's first step; `[verify.nikos]` the manifest key; **`meta/roadmap/1.6/1.6.1.md` PLANNED execution-grade** — the pinned-tool table, transform-free ingestion before any model, the port item by item with the record's lines, the alarm ledger `nitpick.alarms`, the `analyze` stage — with S-103…S-105 owed to the user before it starts; **NEXT: 1.6.1**) (4b: DEF-106 — a write into a PART of a `fixed` binding compiled and stored into the LLVM `constant` global, a fault at -O0 and a deleted store at -O2; `NITPICK-TYPE-086` refuses every write form into a part through the one helper every write form asks, the whole binding and a `fixed` field's own assignment staying ASSIGN-002 — the library listener's fuzzer's finding) (4: THE READING — each engine's port distance sized by reading its pinned source, file and line, every decisive citation re-read: NIKOS's executor wall is `inliner.hpp:224-229` (an unknown callee assumed to have no side effects; the floor's kernel memory is the one location `AbsoluteZero`, its pointer set TOP, so every resume pointer is TOP) — a call-all-matching fallback 150–250 lines plus the allocator-ENTRY model 100–150 (an `mmap` model is worthless: the allocator rebuilds every pointer from integers), the overflow intrinsics an importer-only model of 150–200 lines (a plain `ar::Call` today, its aggregate result forgotten), `llvm.assume` 25–35 with an optional second decider of each z3 discharge, the signedness guess one line or 45–60 (flagless `add`/`sub`/`mul` read as UNSIGNED, `add i64 %len, -1` as +2^64−1), the entry seeding 40–60, the datalayout size check one token (the integer-alignment table samples five widths), `select`/`switch` refused by the importer so `ikos-pp` transforms at every level; NIKOS ANALYSED EVERY PLAIN FORM UNDER LLVM'S DEFAULT LAYOUT (nothing derives one from the triple; `{ i32, i64 }` at 12 bytes where the binary has 16) — E-8, the emitter writing its own `target datalayout` line, the user's; three NIKOS defects by reading (the `inttoptr` pre-pass constraint never added, the coroutine intrinsics missing from five switches, the five-width table); Clam's cell-mapping abort is sea-dsa's SOUND refusal of our by-value aggregate arguments (its call resolution pairs pointer-typed formals alone; the transform Clam always applies creates the trigger and a raw read would drop the aggregate store in silence), so S-101's demotion has no sound instance and no patch joined the pin (D-321's dated note) — the "with" column an unsound INSTRUMENT outside every gate, which passes the plain forms and dies in Crab's typed regions on the whole form; Clam reads no wrap flag, havocs no memory at an external call, and `--crab-lower-with-overflow-intrinsics` is sound for a TRAPPING guard (verified); `dbm` moves no verdict at 3,800× the time, `-opt=none` adds one and eight sites of covered classes; the scorecard: under D-320's rule 1 Clam is disqualified (no memory result, no `div-zero` check) and NIKOS wins, offered to the user at S-102 — the record in `1.6.0.md`; step 5 follows) (3: THE GATE'S RUNS — `gate_run.py` over step 2's input set, 316 tasks over the three engines, each twice: NIKOS reads every form but the compiler's datalayout twin, finds seven of the eight planted defects in both modes (`uaf` missed: the free is an external callee it does not know), and reaches 12 of 14 functions in the sync program and ONE — `main` — of the async program and of the compiler, because the executor's resume call has no points-to and NIKOS's inliner assumes an unknown callee has no effects (the executor wall, step 4's port item); Clam's eight memory settings abort at sea-dsa's cell mapping on every input and find nothing on the controls, and over the compiler's whole-program form exceed the machine (killed at 129 GiB resident, seven times); its intra mode reached the hour cap on every program-scale input; Alive2's fifteen `incorrect` verdicts are inter-procedural facts, measured through a no-inlining twin, escalated to the user and recorded under D-321 with E-7 (`nonnull`/`dereferenceable` on constructed pointer parameters) for 1.6.2; the datalayout twins move no program verdict and NIKOS refuses the compiler's; two runner defects fixed by running it — the cap left analyzer orphans, and a machine kill read as a Clam result; the record is `1.6.0.md`'s step-3 entry, the report `.internal/gate/runs/report.md`; steps 4 and 5 follow) (3h: DEF-105 — an imported `fixed` binding's declared type resolved in the importer's scope, a table read with the wrong stride past its end; both checker sites resolve in the declaration's home scope now, D-137 — the library listener's finding) (3g: DEF-102 — a plain by-value parameter of an owning type is LENT and now read-only, `NITPICK-TYPE-085`, after a field write in a callee dropped the caller's value; DEF-103 — a keyword as a declared name is PARSE-001; DEF-104 — a lent `T` in a generic body escaped both loan gates, which ask D-264's `type_owns_for_move` now; `T[0]` stated as supported — the library listener's findings) (3f: DEF-99 — a move out of `fixed` storage holding an owning value compiled and faulted, a store into a `constant` global; `NITPICK-TYPE-084` refuses the move as TYPE-046 refuses the copy — the library listener's finding) (3e: DEF-98 — the lexer closed a block string on two quotes where §6.3's grammar closes it on three; the grammar stands — the library listener's finding) (3d: DEF-97 — a generic instance used only inside a generic function had its type definition emitted after its first use, `llc` refusing the module; a late definition goes ahead of every function now — the library listener's finding) (3c: DEF-96 — a two-parameter `main` compiled and its `argc` read a register nobody wrote; `main` is `NITPICK-TYPE-083` unless exactly `int32(cstring[]:argv)`, `failsafe`'s `int32` return under TYPE-044 — the library listener's finding) (3b: DEF-95 — the reach analysis armed `BadStep` for every counted loop where the emitter guards only a computed step, the library listener's finding; a demand removed) (step 1: the three engines and the pinned z3 built at their commits by `meta/roadmap/1.6/tools/engines.sh` into `~/.local/src/1.6/`, every binary digest-pinned in `pins.txt`; Alive2 carries the recorded `rlimit` patch and links the pinned z3 as a SHARED library — a static link collides on `smt::context` — and its smoke on `dyn_slots` is deterministic across runs, its two "incorrect" verdicts being the inlined vtable thunks, Alive2's stated inter-procedural blind spot, not a miscompile) (`meta/roadmap/1.6/1.6.0.md`, measured first: LLVM 18's assembler reads our emission unchanged, so Clam at LLVM 18 ingests it without a port; the IKOS candidate is the user's own port of IKOS to LLVM 20, **NIKOS v2.4.0** at `REPOS/nikos`, recorded nowhere in the tree until now; NIKOS has no model of the 2,810 overflow-intrinsic call sites or of `llvm.assume`, and its inter-procedural walk reaches one function of the compiler because `main` hands its body to the executor; Clam's memory analysis aborts on our by-value aggregate arguments; Alive2's pin is the last commit before its `nocapture` → `captures(none)` change) — S-99…S-101 ratified the day they were asked as **D-319** (NIKOS the IKOS candidate; a transform between the artifact and the analyzer invalidates the evidence), **D-320** (the gate's decision rule, before the numbers) and **D-321** (Alive2's budget a resource limit, its solver the pinned z3 commit, a recorded patch part of a pin); steps 1–5 landed: the pinned builds outside the tree, the inputs and the eight planted controls, the runs twice, the reading, the decision D-322 and the 1.6.1 plan — cycles 1.4 (self-hosting) and **1.5 (verification) COMPLETE** — 1.5 closed 2026-09-25 at 1.5.8d (`meta/roadmap/done/1.5/`: every kind of the obligation catalogue live, the floor specified, modelled, read twice and explored, the snapshot refreshed from the final tree — stage2 == stage3, 28,111,929 bytes, `4029fc70efbe9cd3da26b7fb379b477b5dc9417bba3cb0359d5dd25126a1f337`); **NEXT: cycle 1.6, the LLVM-native analyzer evidence (D-233)** — `meta/roadmap/1.6/README.md` is the map and its opening note says what 1.5 left it, 1.6.0 the bring-up gate (Clam/Crab vs IKOS over three emissions, decided by measurement). The history this sentence carried until the close: 1.5.8d's step 0 LANDED 2026-09-25 ( D-317 — a by-value aggregate's identity and its fields as uninterpreted functions of it in the encoder, 64 of the compiler's 79 by-value `terminate` rows and 139 rows in all newly discharged with no verdict moved and no bound written; D-318 — E-5's bound-call fan-out stands, decided out; DEF-94 — the implicit pointer receiver was never an escape, a soundness hole the step's first probe found and fixed), PLANNED execution-grade 2026-09-24 (`meta/roadmap/done/1.5/1.5.8d.md`: two leads decided before the refresh, the refresh, the docs, the archive); 1.5.8b (the `overflow`/`bounds`/`cast-range` rows, `sealed`/`hidden`, the constants, the wrapping family and field limits; COMPLETE 2026-09-23) and 1.5.8c (`decreases`/`unbounded` with the `terminate` and `stack-depth` rows; COMPLETE 2026-09-24) have landed, the old 1.5.8 having been planned 2026-09-18 as four under D-304…D-307: 1.5.0–1.5.8 have landed (1.5.8, COMPLETE 2026-09-19: the runtime's uncontrolled stops closed — a poisoned float cast, a stack overflow with no `failsafe`, a guard page a frame could jump, the last net for every other fault), the floor itself is specified, modelled, its models read twice, its spec's caller assumptions written down and EXECUTED, every synchronization step of the floor and of each concurrency test run under the schedule explorer (1.5.7), and TCB.md is finalized

The **specification set is complete** — `meta/specs/` holds twenty-one documents and
`DECISIONS.md` records 351 decisions, D-001 through D-351 (this sentence said 240 from 1.4.8c until 1.5.8b's planning, 314 until 1.5.8b step 6c, 316 until 1.5.8d step 0, 318 until 1.6.0 step 0, 321 until 1.6.0 step 5, 322 until 1.6.0 step 5c, which carries D-323, D-324 and D-325, and 325 until 1.6.1 step 1, which carries D-326 and D-327, and 334 until the commit after 1.6.1e step 2, which carries D-335, and 335 until 1.6.1e step 3, which carries D-336, D-337 and D-338, and 338 until 1.6.1e step 3b, which carries D-339…D-346, the user's ratification of S-119…S-126, and 346 until landing 96, which carries D-347, D-348 and D-349 — the frac representation and the `fixed` slice, the user's "your recommendations are fine with me" of 2026-10-08, and the toolchain pin moving to the release the distributions serve with NIKOS and Alive2 rebuilt against it, his "we just can't forget the dependency chain beyond just the compiler itself" of the same night, and 349 until landing 102, which carries D-350 and D-351 — D-348 step (ii)'s spelling, R1, and the string→slice bridge's read-only view, the user's "your recommendation for those questions looks fine to me." of 2026-10-08 in `nitpick-compiler_33`'s session). The **plan is in `meta/roadmap/`**,
organised as numbered cycle folders holding `x.y.z.md` subcycle files; finished
cycles move to `meta/roadmap/done/`. Start at `meta/roadmap/ROADMAP.md`.

**Cycles 0.0–1.0 are done** (`meta/roadmap/done/`): the lexer, the AST and parser,
the module/symbol/visibility passes, the type system, the static analyses, macros
with `comptime` and `#[derive]`, IR emission, `nlibc` and the runtime floor, full
type lowering, the memory allocator, and generics/traits/`dyn`. **`npkc` exists**:
`src/npkc.npk` over `src/driver/pipeline.npk` (the one front-half sequence;
`tools/check.npk` is a thin wrapper over it) and `src/backend/`. The harness runs
**272 real-backend programs** (the count at the 1.5.6b close — this sentence said 172 from cycle 1.0 until then; each also re-run through `opt -O2` + `llc -O2`
since 1.3.8) and, since 1.5.4 retired the rung suite, asserts NO `NITPICK-RUNG-001` rejection (1 until 1.5.4 lowered `prove`/`assert_static`; 4 until 1.5.3 lowered the three contract cases; 6 until 1.5.2 lowered the two `limit<Rules>` cases; 8 until 1.4.7's
OWED-8 moved the two channel-element cases to the type checker as `TYPE-057`), and **stage 1 rebuilds itself byte-identically** — the fixpoint has held
through every cycle since 0.8.

**Cycle 1.1 (async and concurrency) is underway.** Landed: `never fails` checked
everywhere with `raw`/`drop` licensed by it (D-163), the `Duration` clock, coroutine
lowering as hand-written switched-resume state machines (D-177/D-178), the typed
`Error` system with origin chains and an exhaustive `failsafe` (D-179), per-thread
executors, real threads over `clone(2)` (D-181), and `atomic<T>` plus channels,
thread pools and actors (D-182). **Cycle 1.2 — the managed
lowering — was inserted ahead of the rest** (D-183): 1.1.11's `Mutex` hands out a
guard whose release IS scope exit, and until this cycle the default regime
implemented no drops at all. Landed through 1.2.3e: the cap==0 ownership bit,
per-type generated `@"npk.drop.<tid>"` bodies, drop flags with move/`pass`
clearing, the move-only rule for owning types (TYPE-046/047), `move T:p`
consuming parameters, and scope-exit drops LIVE for strings, structs and enums
— the compiler drops its own locals and still rebuilds itself byte-identically.
Debug instruments that stay: 0xAA free-poisoning, and `@npk_quarantine`
(npkrt.ll, ships 0) — a never-reuse mode that makes any use-after-free
deterministic, with a poisoned-source tripwire in `npk_string_concat`, and **`NPK_HEAP_STATS`** (1.5.1b step 0): a program whose environment carries
that name prints `heap: allocated=<n> peak_live=<n> count=<n>` — the
allocator's own words, deterministic for a given input — on fd 2 at exit; the
harness's `cost` stage and `npkg test` read it, and a wall-clock is never a
verdict.
**Cycle 1.2 is COMPLETE** (`meta/roadmap/done/1.2/`): drops are live for
strings, structs, enums, `dyn` (which owns a heap cell and drops through
the vtable's slot 0) and arenas, in sync AND async bodies (frame-resident
flags; the wind-up unwind drops too, and the first test to drive a wind-up
found the rouse and the grace-wait defects). Channels reclaim at their
creating function's exit — `StaleHandle` is reachable, slots reuse under
moved generations — and OWNING elements cross channels under the heap's
first futex mutex, with `move` required at the send. A factory says
`gives` after its parameter list (1.2.6): the caller's exit then owns the
returned channels' reclaim, and creating a channel in an unmarked
channel-returning function refuses. A failed `send` drops its element —
the rule a failing callee already applies to its `move` parameters. Owning elements in ARENAS
refuse at `arena_make` until the generated deep-view family lands (D-183).
Phase C renumbered around the cycle (self-hosting 1.3, verification 1.4,
Astrée 1.5; the map is in `ROADMAP.md`). **1.1.11 is COMPLETE**: `Mutex<T,LEVEL>`/`Guard<T>` (the guard's scope-exit
drop IS the release), `RwLock<T,LEVEL>` (`read` → read-only `RGuard<T>`,
`write` → the same `Guard<T>`), `CondVar<LEVEL>` (`timedwait` lends the
guard and reacquires; a timed-out wait SPENDS it — nulled, not held past
the deadline), and `Barrier<N,LEVEL>` (a timed-out party hands its slot
back). Levels feed the 0.5.6 ordering analysis from the receiver's types;
borrows of the primitives are the sanctioned spawn crossings. **1.1.12 (the
reactor) is underway — a landed (D-184, B-3a closed)**: epoll WITHOUT
timerfd (`epoll_pwait` as the armed executor's idle wait, carrying the
sleeper deadline), an eventfd as the cross-thread wake channel,
`suspend_io` + prelude `io_ready` + deferred `io_unwatch` (a registration
lives exactly as long as its wait), and **the task-identity rule**: awaits
drive children inline, so EVERY waiter registration — channels and locks
included — resolves the executor's `cur_task`, not the frame it was
lowered in; the nested-wait lost-wakeup this fixed was latent everywhere
(`nested_wait.npk` regresses it). `sys` takes pointers (`ptrtoint` at the
trampoline — the one place an address becomes a number). **1.1.12b landed
(D-185)**: `OwnedFd` (TY 39 — the drop IS the close; `own_fd`/`release_fd`;
move-only; a channel refuses it, a spawn MOVE is join-bounded legal),
`Path` + `path_parse` (constructor FUNCTIONS — the language has no static
methods), the `Reader`/`Writer` traits (`Self->` receivers, `within`) and
`ByteReader`/`ByteWriter` as prelude retry loops over `io_ready`
(`WouldBlock`/`Interrupted` are named prelude errnos), `Whence` seek, and
TYPE-048 (an impl keeps its trait's `async` — a sync body driven as a
coroutine is corruption). The awaited-method child frame now seeds `Self->`
receivers by ADDRESS. S-1 was **settled by the user as D-186**:
`string_slice` returns an OWNED COPY (`string_from_bytes` stays the
explicit view primitive), and the fallout closed D-183's field/element
overwrite leak — assigning over an owning FIELD or managed-array ELEMENT
now drops the old value (`overwrite_owned.npk` proves both by descriptor
exhaustion). **1.1.12c
landed**: the text layer as GENERIC WRAPPERS — `TextWriter<W>`,
`LineBufWriter<W>` (buffering is a TYPE, never a mode field), `LineEnding`,
unconditional read translation with the split-`\r\n` pending flag,
`string_bytes`, and `std_in`/`std_out`/`std_err` constructors that each own
a CLOEXEC dup (std descriptors stay blocking — the one stated D-071
exception). Three compiler fixes fell out: template fields must resolve
through `struct_field`'s BOUND walk (three raw sites closed — the drop-body
generator was reporting "no type named `W`" on nested generic instances),
the `Ident { Ident :` struct-literal lookahead reads the fourth token, and
awaited bound-calls in generic bodies substitute before trusting the
recorded template symbol. **1.1.12d landed — 1.1.12 (the reactor and the
I/O surface) is COMPLETE**: async methods behind `dyn` — the method slot
holds the concrete RESUME, a size tail (`@"npk.fsz.<frame>"` globals) tells
the caller how big the frame is, and the await site builds it from the
TRAIT's shape through a per-site prefix type and drives the standard loop.
Object safety: async + `Self->` receiver is safe (by-value `Self` alone
still disqualifies); `Self->` receivers admitted generally — and the sync
thunks' by-value-receiver latent (handing a method its own first bytes as
an address) is fixed. `dyn_stream.npk`: one erased `report(dyn Writer…)`,
two writers, two byte patterns. **1.1.13 (the Bridge, D-149 over D-055) is
underway — stage a is COMPLETE (D-187, D-188)**: `#ptr_add<T>`
element-scaled; `atomic_from_ptr::<T>` fused-only over the turbofish;
`lib/nbridge.npk`'s sealed shm (`shm_create_sealed` — the seal is the
architecture's load-bearing line), SCM_RIGHTS pair, and `spawn_driver` →
`bridge_reap` over `npk_driver_clone_exec` (fork-shape clone, allocation-free
child, PDEATHSIG + NO_NEW_PRIVS + dup3 + execve); the DRIVER REGISTRY is
published BEFORE the clone (CLONE_PIDFD writes the pidfd into the slot),
`npk_driver_kill_all` runs on the TRAP PATH before user `failsafe`, and a
clean exit 0 with a live driver refuses — trap −4109 `DriverLeak` (D-188,
the D-151 exit rule's second registry). The Bridge tier never traps: `?!` is
unwrap-or-TRAP-as (a test-main assert, barred from the Bridge by v3 §4.2 —
an EPIPE schedule caught the misuse), so the library binds-and-fails. The
harness grew `tests/backend/fixtures/` + `// argv:` substitution
(`mock_driver.npk` speaks the wire), and the `// stress:` loop — dead in the
real-backend stage until now — runs there again. **Stage b is COMPLETE
(D-189)**: the §6.1 ring header (power-of-two capacity a spawn parameter),
`io_watch` (register-only, two args) + prelude `io_ready2`, dispatch —
descriptor before the SeqCst head publish, EXEC_NOTIFY, the TRIPLE wait
(ctrl/pidfd/stderr), [UNTRUSTED] tail and reply validation,
kill-on-protocol, KILL-ON-DEADLINE (a hang ends now; the graceful §5.4
ladder is close's alone), poisoning — `bridge_close` (rungs end on
DEATH-OR-BUDGET, never on a wait merely returning; the release phase
synchronous behind `closed`), and the stderr tail WOVEN into the waits
(D-180 bars a borrowing drain task) with `bridge_stderr_tail` the reader.
The work flushed two runtime defects: the executor never CONSUMED the
waker's due-now stamp at resume, so every once-woken task busy-polled all
its later waits (a year latent behind retry loops; one cmpxchg at
`npk_step`'s resume site), and D-186's slice copy allocated from the
wild-TRACKED entry (a sliced string alive at `exit 0` was a phantom
WildLeak). **Stage c is COMPLETE (D-190) — 1.1.13 and the Bridge are
DONE**: `extern` blocks lower to GENERATED stubs (`bridge_stubs.npk`, the
derive mechanism's sibling — source synthesized and spliced before
collection; the block binds nothing, the stubs are ordinary async fns,
zero downstream special cases), methods declared IN FULL (`Bridge->`
first, `Duration` last, the v1 wire vocabulary between; departures refuse
EXTERN-001, written contracts EXTERN-002 — D-002's mandatory-contract
parse rule lifted as D-149 scheduled), the INTERFACE HASH rides INIT_REQ
under D-179's error-identity seed (one derived-identity constant
ecosystem-wide) and a stale driver refuses at the handshake, and
`sdk/npkdrv.h` + harness-built C reference drivers prove the wire end to
end (`extern_c_driver.npk`: echo through the ring, driver-reported
refusal → EDriverError, hostile tail → protocol kill, stale interface →
EDriverSpawn). **CYCLE 1.1 IS COMPLETE** (D-191, `done/1.1/`): user
variadics and `..^` landed at the close (the spec's "a variadic call lowers
to building one" — gather, spread, empty tail, and the awaited gather in a
frame slot; methods may not collect, by refusal), the await-edge rungs
became checker rules (SUSPEND-003: no await in a `where` guard) and
internal belts, and the close-out's first exercised programs found three
latent defects: the collector's name was never typable in a body, a
range-view was not counted as address-taking, and stage D's "lives to the
function's end" extension had been a silent no-op since birth (`fn_end`
read off a block span that covers only the opening — every address-taken
local whose last textual use preceded a later suspension sat on the dying
resume stack). **G-3 settled (the user): the exotic tier is NEW CYCLE 1.3**,
and Phase C renumbered a second time — self-hosting 1.4, verification 1.5,
and 1.6 (which was Astrée until **D-233** re-homed the evidence to the emitted
IR; the cycle numbers are unchanged, its topic is not). Adopting `dyn Writer` in npkc's own diagnostics is 1.4
material. **The 1.1 interlude closed the self-contained backlog**: `sys` is
TYPED (D-192 — `Result<int64>` by D-048's contract, register-shaped
arguments refused by name, zext for unsigned/kernel args, `?|` given `?!`'s
unknown-operand fallback; the S-3 rows), and float/char `ToString` landed
(D-193, §6b — shortest-round-trip Dragon4 over `uint2048` in the PRELUDE,
no wide division by construction, 353 python-generated known-answer
vectors; total UTF-8 with U+FFFD for non-scalars). The bare-builtin
signature table is proposed as P-3 (OPEN_DECISIONS §3), owed before 1.4.
**Cycle 1.3 (the exotic numeric tier) is COMPLETE** (`meta/roadmap/done/1.3/`) — 1.3.0 ratified the
whole surface as D-194…D-200 (the survey corrected the tier's story: `tfp`
is Twisted FIXED Point per D-036/D-037, the prototype's floating tfp_ops is
the superseded design; `dim256` units are exponent vectors with `unit:`
declarations ratified; ternary Kleene logic rides `&`/`|`; frac is
invariant-normalized exact-or-ERR; tensors cap at rank 9 with inline dims).
**1.3.1 landed `simd<T, N>`**: TY_SIMD through every walker (the B-7 sweep
enumerated up front), annotation-directed `simd(…)` with splat, elementwise
ops/compares (verdicts are `simd<bool, N>`), bounds-checked lane places,
`.len`, ordered extract-chain reductions (no intrinsics — nothing can
become a libcall), elementwise casts, and D-007 any-lane division guards
(zero → DivByZero, structural INT_MIN/−1 → DivOverflow). TYPE-029 (the
Tier-1 vector refusal) retired with the ctor rung; `.any` joined `.acquire`
in the keyword-after-dot interning. `simd_basic`/`simd_div0`/`simd_divmin`/
`simd_oob` + `simd_rules.npk` (14 refusals). **1.3.2 landed `tfp*`** —
twisted fixed point on native `i32..i256`, the tbb machinery generalized
to both twisted kinds (compare-trap, add/sub, `%`, negation, ERR, is_err)
plus Q-scaled mul/div through widened intermediates with round-trip
narrowing; literals fold EXACTLY in subset-1 limb arithmetic (the seed
still builds src/ — C-13); the cast matrix with sentinel maps and
trap-on-ERR-exit under both spellings; `.floor()`/`.trunc()`; exact-decimal
`ToString` (uint256 core, 100 python-`fractions` vectors); REACH refined
(twisted division arms no DivByZero; TbbErr armed for both families).
Three D-195 amendments recorded: compare-on-ERR TRAPS (D-008 §5 outranked
the carried spec row), raw bitwise/shifts STRUCK (`ERR << 1` is an ERR
laundry), tbb↔tfp casts impossible. **The flagged tbb asymmetry was then
settled by the user and landed as the D-144 amendment**: leaving tbb, ERR
traps under BOTH spellings (`=>!` acknowledges the VALUE's loss; the
0.9.5 carrier read crossed taint as INT_MIN), and the value crossing is
range-classified like any numeric pair — closing a second hole where
`tbb64 => int8` truncated and `tbb32 => uint32` sign-extended raw under
the CHECKED spelling; the prelude's four `tbbN:Hash` impls now spell
their guarded crossing `=>!`. **1.3.3 landed `dim256<Unit>`** — units as
packed 7-exponent SI vectors on interned `TY_DIM` (unit equality IS
type-id equality; a=256 keeps `type_int_bits` uniform), THE ZERO VECTOR
IS `tfp256` (cancellation yields the bare number, scaling needs no
special case, bare `dim256` refuses — one meaning, one spelling),
`unit:Name = algebra;` in the grammar (`unit` is now a keyword) with the
seven SI base names compiler-known and the derived set as PRELUDE
declarations, single-name annotations, `*`//`/` composing vectors with
`+ - %`/compare demanding equality (both units shown in base-product
form, TYPE-049/050), the one cast boundary (`=> tfp256` drops — ERR
RIDES, the crossing never leaves the family; `tfp256 =>! dim256<U>`
asserts), no `ToString` by design (`&{x => tfp256}` is the spelling),
and erased lowering (the TY_TFP-site sweep at 256 bits). Two found
defects: the `pick` ERR-arm demand had not extended to `tfp` (the
wildcard swallowed the taint — now all three twisted kinds demand it),
and **compound assignment was a second implementation of the operators**
— raw `add`/`sdiv` since 0.9.5, so a twisted `+=` wrapped, `/=` by zero
reached the hardware, and a float `+=` emitted integer `add`; the
arithmetic value core is now extracted (`emit_arith_value`) and BOTH
spellings route through it, with `op=` held to the slot's unit.
**1.3.4 landed the ternary/nonary bases** (`trit`/`tryte`/`nit`/`nyte`,
D-197): ONE `TY_TERN` kind, the binary rung storing the VALUE (balanced
order IS numeric order; the sentinels coincide with tbb's carrier
minimums, so `tbb_min_decimal`/`tbb_divlike` serve a third family; the
prototype's packed-trit LUTs are deliberately not carried — §7's
emulation-as-identity warning), overflow past the BALANCED bound → ERR
(the prototype's trit clamp OVERRULED by D-197's uniform text), `/ %` at
the multi-digit pair only (TYPE-051), the user-ratified Kleene `&`/`|`
on the single digits (min/max, NOT = `0 - x`), `.trit(i)`/`.nit(i)`
digit extraction by the offset trick (bounds-checked, ERR-sticky;
`trit`/`nit` joined the after-dot keyword interning), contextual
literals in any base checked EXACTLY against ±29524-style bounds, the
one-family cast matrix with the D-144-as-amended leaving rule, and four
prelude `ToString` impls. Found at implementation: **the same-text
early-out defeated the tbb cast matrix at SAME-WIDTH crossings** —
`tbb32⇄int32` share `i32`, so the crossings 0.9.5's audit hole was
ABOUT rode through unchecked, passing by coincidence (INT_MIN's bit
pattern IS the sentinel); the tbb intercept now precedes the early-out
as tfp's has since 1.3.2. **1.3.5 landed the `frac*` exact rationals**
(D-198): the tier's first NON-SCALAR family — `{whole: iN, num: iN,
denom: uN}`, five invariants after every operation, exact or ERR. The
algorithms are the PRELUDE's, in Nitpick, in `int256` (one core, the
width's bounds as parameters; the emitter unpacks → calls the
deterministic prelude symbol → repacks-or-ERR — the arithmetic a
verifier reads is source, not hand IR); operators `+ - * /` and
comparisons exactly (TYPE-052 gates `%`, pick selectors, and literals —
`int => frac` is the entry); read-only members `.whole/.num/.denom`;
canonical ERR `{minN, minN, 0}` with the disjunctive `is_err`; casts
widen `=>`/narrow `=>!`-absorbing, `=>! flt64` rounds, `=>! int`
truncates toward zero, ERR traps both spellings on exit, and a float
NEVER enters. Found: D-169's non-scalar compare belt called the first
checker-ADMITTED aggregate comparison a defect (the frac arm now
precedes it), plus two reserved-word collisions (`tid`, `fd`) in new
emitter code that only npkc's own parse catches. **1.3.6 landed
`complex<T>`** (D-199): `{T, T}` over the ratified four elements,
`complex(re, im)` reusing the ctor node (the payload names the
keyword), the arithmetic in per-element PRELUDE cores — tfp bodies ride
the language's own Q operators with the any-component-ERR → BOTH
canonicalization an explicit line, float bodies write SMITH'S division
(the naive denominator overflows at √max) with flt32 computing in
flt32 — equality only (no total order exists; per-component, NaN-aware,
taint-trapping), `is_err`/`ERR` on tfp elements only, NO casts either
direction, methods `.re/.im/.conj/.abs2` everywhere and `.abs` float-only
(`llvm.sqrt` declared in the fixed block), four concrete-instance
prelude `ToString` impls ("3+4i"; ERR pairs render "ERR"), TYPE-053.
**1.3.7 landed the library tier** (D-200): `buffer` minimally as the
MANAGED owning byte cell (`buffer_new(int64)` never fails — zeroed,
`len == cap == n`, `n <= 0` the empty answer; the string's trio, the
string's shared drop body, move-only, `==`-refused; §23's draft verb
family/`buffer_free`/`resize` struck by decision — typed access is
`#ptr_add` + `<-`), `#sqrt` (flt32/64 → `llvm.sqrt.*`, the instruction —
a Newton loop computes a DIFFERENT number), `lib/nvec.npk` (vec2/3/4
over `simd<flt64, N>`, ctor FUNCTIONS, dot/cross/length2/length; vec9 as
nine named `flt64` fields with identity and 3×3 mul) and
`lib/ntensor.npk` (`matrix<T>` `{buffer, rows, cols}`, `tensor<T>` rank
≤ 9 dims INLINE, library bounds `fail`s — `tmatrix`/`ttensor` are
`matrix<tryte>` instances, the twisted ERR proven THROUGH the
container). Found: a checker-typed never-fails builtin's Result envelope
rode out under the bare recorded type — `buffer:b = buffer_new(16i64)`
stored 32 bytes into a 24-byte slot (the generic rt path now extracts at
the site; `buffer_new` is the first of its class); a generic struct
literal in a generic factory is the BARE `matrix{ … }` (the
`TextWriter{}` idiom — `matrix<T>{ … }` does not parse); checker-typed
builtins are called WITHOUT `raw` (`own_fd` precedent).
**1.3.8 closed the cycle**: the tier's lowering pinned in the ir_types
fixture (fifteen `ll_is` assertions, no `// ll:` markers — the seed
refuses every tier type, so the pin is the real compiler asserting its
own text), the last G-3 rung retired to a defect confession, and the
harness grew the **opt-O2 leg**: EVERY real-backend program re-runs
through `opt -O2` + `llc -O2` with the same expected exit (stress loops
included), the zero-dependency scan repeated on the optimised object
(`opt` may MINT libcalls), and a missing `opt` failing loudly. The
prototype's optimiser-removed-guarantee lesson is now a standing
instrument, not a one-off audit — and its FIRST full run caught a real
one: the suspend walk recorded no suspension point for the bare
primitives (`suspend_io`/`suspend_until`), so `io_ready`'s `give_up`
deadline sat in an ALLOCA that re-entry re-created — every run since
1.1.12 passed because the fresh slot happened to hold the OLD BYTES at
-O0, and the first optimised build moved the caller's frame and broke
eleven programs deterministically. The walk now records the park as a
suspension (the await rule: past the arguments), `give_up` lives in the
frame, and both levels answer identically. The second hole in the same
analysis (D-191's `fn_end` was the first).
**CYCLE 1.4 (self-hosting) is COMPLETE** (`meta/roadmap/done/1.4/`; closed 2026-09-02 at 1.4.9). 1.4.0 ratified the whole batch
as **D-201…D-209** (`meta/roadmap/done/1.4/1.4.0.md`): the builtin surface
typed from ONE generated signature table (D-201 — never-fails builtins
type BARE, the 13-arm convention generalized; the emitter's parallel
authority retires; the ~1,700-site `raw` shed rides a transitional
rule), the fixpoint criterion restated (D-202 — the harness has measured
the right thing since 0.8.1; the spec's "byte-identical binaries"
sentence was the defect), the committed bootstrap IR + survival map +
THE FLOOR'S PERMANENT FORM IS HAND-WRITTEN LLVM IR (D-203 — the user:
the goal was removing the C/C++ layer, never LLVM; npkrt.ll re-homes to
`runtime/` at the switch), reproducibility pinned and tested (D-204),
the normative builder rule with the switch at 1.4.6 (D-205 — measured at
open: `src/` is STILL fully subset-1; SUBSET_1 §4's gradual-adoption
story never happened), `npkg` + `npk_spawn` + the closed-world link
(D-206), per-scope joins with the `dyn`-element refusal permanent
(D-207), loop-carried moved-from states (D-208), and the adoption scope
(D-209 — generic collections and `dyn Writer` diagnostics in; mass
`&{ }` re-spell, pipeline async, and `src/` macros OUT by decision). The
user's bug/vuln-statistics research (§6c) ran the same day: eight
deep-research reports live in `meta/roadmap/research/` with
decision-grade digests in `research/digests/`, the coverage
audit in `research/COVERAGE_AUDIT.md` **ratified whole as D-210…D-221**
(overflow TRAPS on plain ints, `const`/`fixed`-only module state, the
consuming `pick (move(v))`, the dyn coercion refusal for
channel-carrying concretes, the nfs safety riders, the schedule-
exploration harness, NIKOS struck from 1.5, and the whole 1.5
verification architecture recorded early — new subcycles 1.4.2b and
1.4.3b scheduled), and the research-informed plans landed — cycle 1.5's
README carries the full proposed verification architecture (solver
determinism profile, encodings, obligation catalogue), 1.6's the
analyzer-evidence plan (rewritten at **D-233**; it was the Astrée handbook and
the C-19 question list until the evidence moved to the emitted IR), and 1.4.2–1.4.8 each have
execution-grade subcycle files written for a fresh executor to follow
without asking. 1.4.1 landed the instruments (B-7's walkers-total check
found and fixed the missing TY_ENUM drop arm, a silent payload leak live
since 1.2, plus the contains-walker laundering holes). **1.4.2 (P-3) is
COMPLETE** — four commits, full harness green at each:
BUILTIN_REFERENCE.md's marked regions are now the ONE signature
authority, parsed strictly (`gen_tables.py` hard-fails on a row it cannot
read) and cross-checking the Signature column's `Result<…>` against the
Fails column; `builtins.npk` gained `builtin_sig_special/_count/_param/
_param_move/_ret` and **`ir_runtime.npk` is GENERATED from the same
rows** — the LLVM ABI derived, with a five-token `**ABI:**` note
(`inline`, `sym=`, `ret=`, `args=`, `envelope`) only where a symbol
departs, and the nine emitter-only symbols in their own §2d region. Every
regular builtin call is TYPED (arity, per-argument fit, `move`, spread
refusal; a `never fails` builtin is the BARE value, a may-fail one a
`Result<T>` — D-201 §4), four bespoke arms retired into the table,
`NITPICK-TYPE-054` carries a builtin's own call rules, and **2,215
`raw`/`relay` plus 242 `drop` came off the tree** in 150 files, with the
seed's three wrapped-flag flips and its extract-at-site in the same
commit. The emitter's parallel authority is gone: `result_ll_value_half`
and its `?|`/`?!` consumers, `emit_raw`'s wrapped/inner fallback,
`drop`'s nine hardcoded builtin names, and the argument coercion's blind
`zext` (the D-192 class — a negative `int32` widened into an enormous
positive `int64`) which now reads signedness off the recorded type.
Found on the way: **five latent unchecked pointer reinterpretations in
`src/`** (`ralloc`/`alloc` bytes bound or cast without `=>!`, invisible
while the callee had no type) and the instrument defect that
`check_runtime_sigs_agree`'s derived-inner cross-check had NEVER RUN — it
read the wrapped flag out of a leaked loop variable. Three of step 3's
four defects were caught by the compiler checking ITSELF, none by a test.
**1.4.2b** (D-210 overflow traps on plain ints, D-211 `fixed`-only module
state), **1.4.2c** (D-222 — `const` retired), **1.4.3** (D-208: the move
analysis learns about PARAMETERS, which is where the hole actually was — 26
findings in `src/`, including a live double free), and **1.4.3b** (D-216: the
consuming `pick (move(v))`) followed. **1.4.4 (D-207) is COMPLETE**:
`join_head` is per SCOPE — by a MARK, since the list is already a LIFO stack,
so a scope's exit joins until the head is back where its entry left it (one
pointer per scope, frame-resident at role 41 in a coroutine because a scope
spans suspensions). The order at every scope exit is now **join → defers →
drops → that scope's channel reclaims**, innermost first; D-183 ran the joins
LAST, which put a spawned child's borrowed `Mutex` behind the mutex's own
drop, and `type_drops`' "by the join discipline nobody can still hold it"
comment was a claim the lowering did not keep. `%npk.join` — one block every
exit branched to, and the reason the join ran last — is gone: the return seam
stores the result BEFORE the unwind and returns after it, so the first-child-
error arbitration reads the same slot it always did (D-136 untouched — the
value is still EVALUATED before the defers). Lifted: `channel()` inside a
LOOP (per-iteration reclaim; `chan_loop.npk` proves it by the first
iteration's endpoint answering `StaleHandle` seven reclaims later) and
`shared_arena` teardown (`TY_SHARED_ARENA` drops for real and left two
walker excuse tables). Riders: `exit` runs joins and defers and NO reclaims
(the drain runs generated drop bodies — the walk D-183's amendment keeps off
the controlled-shutdown path), and `.destroy()` on either arena kind clears
the binding's drop flag, the same mechanism `move` uses. The `dyn`-element
channel refusal is PERMANENT by D-207 and its rung now says so. Owning made
`shared_arena` MOVE-ONLY (TYPE-046 keys on `type_drops`), so a borrow became
the only way to share one — and **D-180 was amended (user-ratified) to make
`shared_arena<T>->` the fifth sanctioned spawn crossing**: its hazard test is
"a mutation the holder cannot see", and a shared arena has no mutation at all
(D-154 writes a slot once, before its handle escapes, and never again), while
the joined-before-freed order this cycle built is what bounds the borrow.
Two finds, neither by a test of the thing that broke: the **D-151 leak check
runs only on `exit 0`** — a program reporting success as 42 checks nothing,
which is how the first `shared_arena_drop.npk` passed against a build with
the drop disabled — and **every thread join had been sleeping its entire
five-second deadline since 1.1.9**. `npk_thread_join` waited on the
CHILD_CLEARTID word with `FUTEX_PRIVATE_FLAG`; the kernel's wake from
`mm_release` is a SHARED wake, which a private waiter never receives, so the
word was cleared promptly, the wake went nowhere, the wait ran to its
timeout, and the reload then reported success — right answer, five seconds
late, every time. One token; `mutex_basic` 5.00s → 0.12s, and the mandatory
deadline can once again tell "finished" from "stuck".
**1.4.5 (D-204) is COMPLETE**: the toolchain is a pinned build INPUT —
`nitpick.toml`'s `[toolchain]` carries the exact patch release (20.1.2 at the
time, 20.1.8 since D-349 on 2026-10-08; not a minor pin: a patch release can
change instruction selection) and the four
flag sets, and every `llc`/`opt`/`ld.lld` invocation is BUILT from those
lists, fifteen call sites and one authority, because a stated flag nothing
consumes is the next stale document. `check_toolchain_pin` asks the tools
themselves (not `llvm-config`, which is a -dev package the build does not
need) and refuses a mismatch loudly; `selfcheck.py` holds its FAILURE path.
A new **`repro` stage** runs the same compiler on the same absolute inputs
from a DIFFERENT working directory and requires the same bytes, then
assembles the compiler's own IR twice and compares the objects — the two
hazard classes the fixpoint cannot see, since IT compares two different
binaries (agreement, not determinism): H1, ASLR and hash-iteration order,
which has no controlling flag and can only be tested, and H9, the build
path leaking into the artifact, which is not hypothetical here because
D-179's site tables embed source paths. Both are clean, measured. Found:
`npkseed.py` never did embed its argv path — `Module` puts the path in a
`path` FIELD while `_path` is the location attribute nothing sets on a
module node, so the ModuleID was `"?"` by accident, one character from the
opposite; it is an explicit constant now.
**1.4.6 (D-203, D-205) is COMPLETE — the builder switched.** The committed
`bootstrap/seed/stage1.ll` is what builds `src/` now; `npkrt.ll` re-homed to
`runtime/`; the Python seed builds nothing. D-205's rule changed meaning with
it: `src/` is no longer bounded by subset 1 but by what the SNAPSHOT can
compile, so a feature enters `src/` only after a snapshot that understands it.
**1.4.7 (adoption, D-209) is at step 3.** Step 1: five copies of the
diagnostic walk became one. Step 2 is COMPLETE — every growable array in `src/`
is a `List<T>` and `ralloc` appears nowhere outside `list.npk`; twenty-two
families, four of which the original enumeration had missed because it keyed on
`ralloc` and one family `alloc`s-and-copies. **D-229 is COMPLETE**: the walk is
generic and borrowing, sorts, and is tested through the capture it exists for;
`impl:Sink:Writer` lives in `diag_writer.npk` beside the walk (REACH is
import-scoped and the impl is async). It is the tree's FIRST impl on a struct
declared in another module, and its first build found that the symbol scheme
had never said which module qualifies a method — definitions and three call
paths disagreed the moment an impl left its trait's module. **The impl's
module, on both ends** (D-156 read as its vtable row already said), with
byte-identical IR for every pre-existing program; `impl_foreign.npk` pins every
shape, and `Trait.default_method(recv)` — refused even in one file — works.
**Step 3 is COMPLETE**: 268 counter loops are `for (intN:i in 0iN...b)` —
`..` is the INCLUSIVE range, `...` the exclusive one — and the 332 that stay
`while` do so by rule: **a `for` captures its bound at entry and a `while`
re-reads it**, so a loop bounded by a container's live count stays a `while`
and the spelling says which (proposed for ratification in 1.4.7.md). Five
match-shaped unwraps became `?|`/`?!`. **OWED-8 is CLOSED**: a type that
cannot be a channel element is `NITPICK-TYPE-057`, the checker's refusal at the
spelling and at the substitution, from one table the backend's belt also reads;
the undecided kinds stay a rung. **OWED-1 is CLOSED**: the one red the
parallel scheme ever produced was a race in the C test fixture (the hostile
tail stored after the completion), reproduced 11 times in 120 under contention
and fixed at its source; the Bridge, the reactor and the optimiser were
exonerated by measurement, and D-228's width calibration is unblocked.
**1.4.7 IS CLOSED (2026-09-01)**: the FNV step took the one copy's `uint128`
spelling (1.4.6's owed item; `bridge_stubs.npk`'s second copy of the trio,
missed by 1.4.2b's collapse, is gone — 175 of 176 backend programs emit
byte-identical IR, the 176th differing by exactly that body), the fixpoint is
declared under D-202, and **the snapshot is refreshed from the adopted tree**
(stage2 == stage3, 15,450,688 bytes). The refresh's dry run found that an
absolute `src/npkc.npk` argument embeds the build path into 1,489 of 1,647
site-table rows — D-078 held by one README line — so the `repro` stage now
refuses an absolute site path in the committed snapshot, and whether the
source manager should record manifest-root-relative paths is **S-7** (the
user's). SUBSET_1 §4 carries its closing edit.
**1.4.7b IS COMPLETE** (`meta/roadmap/done/1.4/1.4.7b.md`): the close's
recommendations were ratified as **D-234** (a `for` captures its bound, a
`while` re-reads it), **D-235** (every kind decided as a channel element:
simd and function values ride, the sync primitives, atomics and arenas refuse
permanently) and **D-236** (manifest-root-relative source paths); D-235 and
D-231 landed, the tfp fold runs in one `uint512`, and D-228's width is
calibrated at 6. D-236's implementation and D-230's `TY_FLAGS` were re-homed
to 1.4.8's open beside the layers they need.
**1.4.8 (`npkg`, D-206) IS UNDERWAY** (`meta/roadmap/done/1.4/1.4.8.md` carries the
execution record and the order). **Part A landed**: the runtime's driver
clone is ONE primitive for every supervised child — `clone_exec`, a ten-word
block with the child's 0/1/2 sources, an optional ctrl fd, and the "every
child-bound fd ≥ 4" rule CHECKED by the runtime (`-EINVAL` before any slot is
claimed) instead of trusted; `spawn_driver` is a caller of it. `environ()` is
a floor builtin (`_start` measures argv and envp with one builder; no syscall
returns the environment and `npkg` needs `PATH`). `lib/nsys.npk` holds the
shared syscall vocabulary (REACH is import-scoped — the tool runner could not
import the Bridge for six constants), `lib/nproc.npk` is the tool runner
(`proc_spawn`/`proc_wait`/`proc_reap`: both pipes captured, every wait
bounded, a deadline kills-reaps-retires), and `proc_tool.npk` proves capture,
the environment passed through, a missing tool's 127 as the CHILD's answer,
and a hung tool killed at a deadline — exiting 0 so D-151/D-188 assert nothing
leaked. The user settled D-230's families and S-8 on 2026-09-02 (the
recommendations as written).
**Steps 2b–6 landed (2026-09-02), validated under D-228's cumulative-prefix protocol after a mid-step-6 UI freeze the recovery lost nothing to.** `range<T>` is spellable (2b, S-8/D-093); the snapshot was refreshed mid-cycle so `src/` and the library it imports may spell the flag families (3); every `open` caller crosses its `oflags`/`fmode` to the floor's word with `=> int32`, `open` itself staying `int64` — the floor is the syscall surface (4); `lib/nfs.npk` is the file-system surface — a sorted listing over `getdents64`, containment answered by OPENING not by string checks, restrictive creation defaults and the at-family (D-213's three riders), with `sys_cwd` and three named errnos in the prelude (5); and **D-236** renders every source path relative to the manifest root the driver finds by walking up from the main file, so the `selfhost` stage's new assertion measures H9 green — zero absolute site rows where 1,479 of 1,637 leaked before (6). Four full harnesses (main plus three cumulative-prefix worktrees) came back 58/58; main is byte-identical to the fully-merged `w456`, and a confirmatory harness on committed main followed. **Part D LANDED (2026-09-02): `npkg/` exists** — twelve modules of Nitpick built by the compiler under test, over the compiler's own path code, list and lexer. `npkg build` runs the README's ladder and produces a compiler BYTE-IDENTICAL to the harness's; `npkg test` builds, runs the runner self-check (§7.1, also `--selfcheck` alone), then every suite the harness runs unit for unit — 908 verdicts on the first full run, every suite count the harness's — with `--only`, `--verdicts PATH`, and `update`/`verify` refusing by name. The undefined-symbol scan reads the object's ELF64 symbol table itself (`npkg/elf.npk`) rather than spawning an unpinned `llvm-readelf`; the harness keeps spawning it and the new **`parity` stage** builds `npkg`, runs `npkg test --verdicts` from the manifest root, diffs the two verdict lists unit for unit and byte-compares `build/npkc` — every per-file harness site now records a verdict. Three things the port found: BUILD_REFERENCE §7.1's "unexpected diagnostics fail a test" is a rule NEITHER runner enforces (subset matching since 0.8; 17 of 131 rejection files carry unasserted extras — **S-9**), nine of those seventeen were `tools/resolve_check.npk` never naming the prelude module (fixed), and the tool runner's capture was quadratic (a `string_concat` per 8 KB read; `npkg test`'s first full run spent 17 of 56 minutes in the kernel — `lib/nproc.npk` accumulates linearly now, and `proc_wait` CONSUMES its `Proc`, which D-004's conservative borrow rule required for the captured text to leave the frame). Whether every suite should be a manifest `[[test]]` entry is **S-10**. Both runners run until `meta/SWITCH.md`; the harness remains the run whose result means the suite is green. **The concluding harness run on the final tree: every stage green, 58/58, and the `parity` stage's first result — 902 verdicts agree between the two runners, npkc byte-identical.** 1.4.8 is closed; S-9 and S-10 were ratified the same day as **D-237** (exact diagnostic matching in both runners) and **D-238** (every suite a `[[test]]` entry with a `stage`, one table both runners read), and land as **1.4.8b** (`meta/roadmap/done/1.4/1.4.8b.md`, execution-grade).
**1.4.8b IS COMPLETE (2026-09-02)**, two commits each under a full harness with
`parity` green. **D-237**: on the error channel the SET of codes a rejection test
reports must EQUAL the set its expectations name — `check_module_rejection`
(harness) and `check_rejection` (npkg) fail every unnamed code by name, the
runner self-check's `unasserted-extra` case proves the rule bites (a negative
control shows the old subset rule accepting it), and 131 of 131 rejection files
pass under it after the eight resolutions. Two of the eight contradicted their
pre-settlement on reading: `definite_assignment.npk` MEANS its `PICK-003` (named,
not wildcarded away), and `assoc.npk`'s `TYPE-014` was a stale-text collision —
its default assoc was named `Error` at 1.0.6, D-179 later made `Error` the
compiler-known type resolved by name ahead of every lookup, and the checker read
the word two ways (the builtin in signature comparison, the trait's assoc in the
object-safety walk); the test spells `Fault`, and whether `Error` joins the names
a program cannot declare — a module-level `struct:Error` is ACCEPTED today where
`struct:Duration` is refused, and an `assoc` shadows a prelude type inside its
trait — is **S-11**; three sites where two rules report one mistake are **S-12**.
**D-238**: every suite is a `[[test]]` entry with a `stage` (`compile` with its
`kind`, `parse`, `resolve`, `check`, `accept`, `fixture`, `program`, `runtime`),
`paths`/`path` and `recursive`; both runners read the one table and refuse an
entry they cannot honour BY NAME before anything runs; the hardcoded loops are
gone from both (the harness dispatches from `run_stage`, npkg from
`run_targets`, each stage a function, each tool built once on first use); the
before/after verdict lists differ in exactly the two recorded ways — the six
duplicated grammar units collapse and the two `nf_twin` twins the old
non-recursive glob missed are swept — and nothing is judged differently.
**1.4.8c IS COMPLETE (2026-09-02)**: S-11 and S-12 ratified the same day as
**D-239** — a name the compiler (`Error`) or the prelude owns cannot be declared
by a program at ANY type-namespace declaration, associated types and generic
parameters included, refused by the loader under RESOLVE-001
(`owned_names.npk`: six shapes; a module-level `struct:Error` had been ACCEPTED
where `struct:Duration` was refused, and an `assoc:Duration` shadowed the
prelude inside its trait) — and **D-240** — one mistake, one report: the old
blanket spelling is recognised by one probe so TYPE-002 never joins TYPE-012,
`drop` over a bare non-`Result` is TYPE-007 alone, and a refused `..^` argument
is not also fit-checked. Refusal-only: the compiler's emission of itself is
unchanged.
**1.4.9 CLOSED THE CYCLE (2026-09-02): self-hosting is declared under D-202.**
The README's refresh, invoked relatively from the tree root, gives stage2 ==
stage3 at 15,631,627 bytes (sha256 `9ce0ec8d3de5b2c83da4a1f11d3f89965728f6cf938f70042ea053eff5defaaf`) from the final tree
(`80784f3`, whose `src/` is 1.4.8c's) — installed as the snapshot with its
STAMP, so the committed builder IS the fixpoint text — and the harness's
`selfhost`, `repro` and `parity` stages are green on the same tree (906
verdicts agreeing between the two runners, `build/npkc` byte-identical). The
close synced the docs (the D-233 doc-sync batch drained, G-2 recorded as
settled by D-231, the width measurement into ORCHESTRATION §4), retired
HANDOFF.md into ROADMAP's and the 1.4 README's "What cycle 1.4 taught" (every
part re-homed; `1.4.9.md` has the map), and archived the folder. **Next: cycle
1.5 (verification)** — its batch D-217…D-221 is ratified,
`meta/roadmap/done/1.5/README.md` is the map (its opening says where to start), and
1.5.0 is the skeleton: the SMT-LIB2 writer, z3 spawned through `lib/nproc.npk`
under the determinism profile, the obligation manifest, `TCB.md` drafted.
**1.5.0 IS COMPLETE (2026-09-03)**: the pipeline is real — obligations as
SMT-LIB2 text, the pinned z3 one process per function under the determinism
profile, `nitpick.obligations` committed (141 rows, 116 discharged), `llvm.assume`
elision, the D-007 division pair proven end to end, the verified compiler
rebuilding itself, the `undef` ban a check, TCB.md drafted.
**1.5.1 IS COMPLETE (2026-09-03) — the verification surface TYPES**
(`meta/roadmap/done/1.5/1.5.1.md`, five steps, each under a full harness). A
`limit<name>` RESOLVES at all three sites (a typo is the identifier's own
RESOLVE-002, a non-`Rules` name RESOLVE-011, a refinement cycle RESOLVE-006,
and the resolved rule is written onto the node); a `Rules` body types over `$`
(the same node as a counted loop's counter: the value under consideration),
every clause a `bool`, and a limited binding's declared type is the rule's
subject BY IDENTITY (TYPE-059 — no widening, no type parameter). Every
proposition is a `bool` — `requires`, `ensures`, each `invariant` conjunct,
`prove`, `assert_static` — and a contract expression admits only what a
proposition can evaluate anywhere (TYPE-060): no `await`, `move`,
`relay`/`?!`/`?|`/`drop`, `pick` expression, store or manufactured view, and a
call only to a NAMED `never fails` `pure` function (`raw f(…)`; a builtin bare
by its Fails and Pure columns) — a function value, a field or a `dyn` is
refused because 1.5.3 encodes a contract call as an uninterpreted function per
KNOWN symbol. **Five decisions landed with it**: D-241 (D-163's contract row
retires — `never fails` may carry `requires`/`ensures`/`limit`, since D-221's
trap route is the channel a never-fails body already admits), D-242 (purity is
DECLARED: `pure` is a contract clause checked in the body — TYPE-061: no
`async`/`thread`, no `move` parameter, no impure/indirect callee, no
shared-state receiver, no `wild`, no owning local, no store the caller sees —
and every builtin row carries a `Pure` column generated into `builtin_pure`;
purity never rides a function type), D-243 (`old(expr)` is a keyword operator
with its own node: the value at the function's entry, in `ensures` and
`invariant`, never nested, only of a copyable value), D-244 (`main`/`failsafe`
carry no contract), D-245 (`result` is a keyword with a leaf node — a
parameter could shadow the identifier it used to be, inside its own
postcondition). **Found**: macro expansion SHARED its verify nodes across
expansions, so the last expansion resolved won — invisible while the backend
refused the family, a miscompile the day 1.5.3 lowered a contract in a
macro-emitted function; `clone_verify` now. S-13 closed (parity fails on any
nonzero `npkg test` exit). No obligation rows moved (141), no rung retired
(1.5.2/1.5.3/1.5.4 own them), no snapshot refresh (frontend only; `src/`
adopts none of it until a snapshot understands it — D-205).
**1.5.1b IS COMPLETE (2026-09-04) — the workbench's findings, fixed before
planned work** (`meta/roadmap/done/1.5/1.5.1b.md`; nine landings, each a
cumulative prefix validated by a full harness, D-228). The floor keeps four
allocator words and prints them under `NPK_HEAP_STATS`, and the `cost` stage
runs in both runners over `tests/cost/` (step 0); every file's first
declaration is its header and the entry points are the root's — `main`/
`failsafe` outside the root refuse, 243 files gained `mod:<basename>;`
(D-248, step 1); a root with `main` and no `failsafe` refuses at `main`,
REACH-003 (step 1b); a view-maker's result BORROWS its operand, by the
reference's `Views` column, and a view of a temporary is BORROW-012 (D-249,
step 2); the builders write into a `Sink`, each byte once, and the three
cost axes hold their bound (step 3); derived comparisons follow the operand's
SPELLING, DERIVE-006 (D-250, step 3b); a unit without `failsafe` declares
`@npk_failsafe` and compiles to an object — the `object` stage in both
runners — and `pub use` re-exports after a plain `use` (step 3c); an owning
value no place takes is a temporary of its statement, dropped when it ends
and on every exit, frame-resident across `await` (D-246, step 4); and
`List<T>` is compiler-known and OWNING, move-only, and lives in the PRELUDE
with its functions since step 5b's bridging refresh (D-247, step 5; S-25 as
recommended, pending the user). **Step 5 found five
defects on the way, all fixed in it**: `pass h.n` cleared the root's drop
flag for a COPYABLE field (an owning local returned by a copyable field leaked
since 1.2.3); every descriptor-exhaustion proof in the suite depended on the
session's soft descriptor limit, so both runners now stand under
`nitpick.toml`'s `[limits] nofile`; a `move` out of a FIELD dropped the
moved-out value at the field's next assignment (the resolver's own constant
folder double-freed, masked by `exit`) — a partial move now leaves the type's
vacant value and the aggregate stays live (S-26); three unit tests released
the heap and then RETURNED from `main` — TYPE-062 now requires `exit` after
`wild_release_all()` (S-27); and the trap route after a release died because
the main thread's TLS block was heap memory — a raw mapping now. The close-out
refreshed the snapshot and set `tests/cost/self.toml`'s ceiling; its first
harness found the diagnostic sort reading slots it had moved out of (DEF-13,
step 5c: byte identity of the fixpoint is not behavioural identity — the
refresh's own harness is the proof, and the seed README says so).
**1.5.2 (`limit<Rules>` live) IS COMPLETE (2026-09-04; `meta/roadmap/done/1.5/
1.5.2.md`, planned and closed the same day, every question ratified the day
it was asked — S-24…S-30 as D-251…D-255, a D-247 note and the README's
1.5.4b row; six landings, each a cumulative prefix under a full harness,
D-228).** **Step 0 was DEF-14**, a soundness defect found by planning: the
1.5.0 encoder gave an address-taken local a stable symbol and withheld only
its definition, so a definition of ANOTHER local in terms of it outlived a
call that wrote through the pointer, z3 discharged a `div-zero` row the
program then defeats, and the elided build died with a floating-point
exception where the plain build reaches `failsafe` — proven end to end on
`8dbef43` with a twelve-line program. An escaped name is never NAMED now
(every read a fresh opaque term, the escape set computed before the
parameter loop); the compiler's own 141 rows, re-decided with both compilers
and joined by ordinal, moved not one verdict. Then: TYPE-063 (a limited
binding has no address — `@`, `$$m`, `$$i` and a move out of an owning
sub-place refuse) and TYPE-064 (a `limit` where no write point exists — a
trait signature, `wild`, `comptime`; `main`/`failsafe` under D-244) with
`LimitViolated` (−4111) armed by REACH, and the runner self-checks' rung
example moved to `prove`; the check in EVERY build — one generated `i8`
predicate per `Rules` (`src/backend/ir/ir_rules.npk`), the check AFTER every
write over the binding's whole value (initialiser, every assignment to it or
any part of it, the callee's entry — sync and coroutine), the site key's
SPACE (expression/statement/declaration), the rung retired; the `limit` rows
encoded with the rule as a HYPOTHESIS on every later version of the binding
— `limit_loop.npk`'s `div-zero` after a loop is the first cross-kind
discharge — `limit-subsume` rows at every direct call of a sync callee (the
checker records a callee only for method calls; a free call's is its
symbol), elision into ONE `llvm.assume` over the rule's range clauses, both
runners kind-aware with the `none` elision word, the self-checks' `wrong-`/
`right-limit`; the caller-side bypass (D-252: a sync function with a limited
parameter is `<symbol>.body` plus its ordinary symbol as the checked entry,
`.body` inside the quotes; a direct call whose row is discharged names the
body; the belt counts defines, tail calls and direct calls in both runners
— over the SYMBOL text, since the code-only text blanks quoted names and the
step's first harness found the belt counting nothing — with the
`bypass-counted`/`-missing`/`-as-value` cases in both self-checks);
the docs. `nitpick.obligations` never moved. **1.5.2b (derived impls over generic subjects) IS COMPLETE
(2026-09-05; `meta/roadmap/done/1.5/1.5.2b.md`, planned, ratified — D-256…D-259 — and
closed the same day; six landings, each a cumulative prefix under a full
harness, D-228).** Step 0: `DECL_THREAD` is 512 and `check_decl_flags_unique`
refuses a shared `DECL_*` value, a value that is not one bit, or a row it
cannot read (L-12). Step 1 (D-256, DEF-15): a family impl APPLIES to an
instance only when its target pattern unifies with it and every bound holds —
ONE unifier (`family_unify`, `type_trait.npk`) read by `find_method`,
`type_implements`, `bind_blanket`, the `dyn`-coercion lookup and the emitter's
`note_family_instance`; the measurement found the parameters had been bound
POSITIONALLY on both sides, so `Pair<U, T>`, `Pair<int32, T>` and
`Box<Pair<T, int32>>` — admitted at their declaration since 1.0.4b — now run;
the call site reports TYPE-017 naming the impl (or the derive), the parameter
and the bound; every overlap report carries a note at the earlier impl. Step 2
(D-257): the prelude's `scalar-impls` GENERATED region — 348 rows in thirteen
families from LEXICAL_REFERENCE's BuiltinType production, `Debug` exactly
where a `ToString` exists, `Hash` for the mechanical rest of the ladder,
`dim256` per DISTINCT unit vector (per NAME the prelude refused itself:
`Hertz`/`Becquerels`, `Grays`/`Sieverts`, `Candela`/`Lumens`, and
`Radians`/`Steradians` ARE `tfp256`), `string`'s `Eq`/`Ord`/`PartialOrd` by
hand, `gen_tables.py --check` and the harness's `check_generated_current`;
found: the tier scalars' method intercepts (`tfp`, `dim256`, the ternary
family, `complex`, `simd`) refused EVERY name outside their fixed sets in the
checker AND the emitter (whose `emit_tfp_method` lowered `to_string` as
`floor`), so the region could exist and never be called — both sides now send
a name outside the type's own set to the impl table through one predicate per
kind. Step 3 (D-258; DEF-16, DEF-17, DEF-18 closed): `dv_class` reaches every
member through the trait being derived — a scalar through the prelude's impl
(`raw`), a named type or parameter through its own (`relay`), a `string`
field through the prelude's four, a `simd`/pointer by `.any()`/address under
`Eq` and copy under `Clone`, the rest refused by name (DERIVE-006 per derive)
— the head synthesized on exactly the parameters the body reaches,
`Clone` member-wise, `Debug` through `debug`; found and fixed: the emitter's
`emit_tostring` could not lower an interpolated bound parameter or a
family-impl `ToString` (EMIT-002 where the checker admitted). Step 4 (D-259):
every generated line goes through one sink recording (subject, trait,
member), `front_run` re-homes every diagnostic in a `<derived-N>` file to the
derive's declaration with the derive and the member named, both runners refuse
a `<derived-` path with `derived-path`/`derived-path-control` in both
self-checks, and D-240 widened: a `raw` over a refused operand is not asked a
second question (six unit cases counted the pair since 1.1.2). **Recorded for
the user (OPEN_DECISIONS §2f)**: DEF-19 — a `pick` over an `Optional` enum
with variant patterns is admitted by the checker and refused by the emitter
(`pick (o ?? Ordering.Equal)` is the suite's spelling); DEF-20 — a GENERIC
ENUM parses and means nothing (no `T` in its payloads resolves, a variant
constructor is the bare enum), never decided in or out. Measured and
recorded, not failures: the compiler's own IR grows 2.2% (every prelude impl
body is emitted whether reached or not) and the frontend over `src/npkc.npk`
takes 14% longer; `nitpick.obligations` never moved. **The two findings were
ratified the same day as D-260 (a `pick` does not select on an `Optional`,
TYPE-065) and D-261 (generic enums are IN, a family as a generic struct is)
and landed as 1.5.2c.** **1.5.2c IS COMPLETE (2026-09-05;
`meta/roadmap/done/1.5/1.5.2c.md`, planned, ratified and closed the same day; three
landings, each a cumulative prefix under a full harness, D-228).** Step 0
(D-260): a `pick`'s selector may not be an `Optional` — TYPE-065 at the
selector, statement and expression form, before the arms are read; `pick (o ??
default)` and `== NIL` are the spellings. Step 1 (D-261): a generic enum is a
family — `bind_instance` is `pub` and binds a struct's OR an enum's window, and
every payload-type read goes through it (the layout per instance, so
`Opt<string>` owns and `Opt<int32>` does not; the pattern bindings; the
constructor's fit; the emitter's `variant_payload_slot_at`, which construction,
`pick` binding and the drop body share); `enum_instance_for` supplies a
constructor's or a bare variant's instance — the expected type, else inferred
from the payload by `unify_into`, else TYPE-022 naming the parameter — built
through `make_instance` and recorded so `check_instantiations` judges it; the
emitter substitutes a generic body's enum at `emit_ctor` and through
`pick_sel_tid` at every `pick` reader. **Found on the way, fixed in step 1**:
the `pick` EXPRESSION form typed NO arm binding — only the statement form
called the binding typer and the lending-form refusal — so `give t.x` over a
bound `t` was accepted unchecked and died as EMIT-002 on a PLAIN enum, a struct
given where an `int32` was expected passed the fit check against nothing, and
an owning payload could be copied out of a lending pick expression (1.4.3b's
hole, open in one of the two spellings); the statement form's whole prelude is
ONE function now, `type_pick_rules`, called by both forms
(`pick_expr_bindings.npk` pins the four rules that became live). Two `pick`
binding sites in `ir_stmt.npk` that resolved the payload node by hand read
`variant_payload_slot` now. Step 2: the docs. `nitpick.obligations` never
moved. **1.5.2d IS COMPLETE (2026-09-05; `meta/roadmap/done/1.5/1.5.2d.md`; the
library workbench's S-38, ratified the same day as D-262; four landings, each
a cumulative prefix under a full harness).** The workbench measured a fixed
per-program cost at the 1.5.2c close — a floor-only program at +388,765 bytes
of IR, 0.10 s → 0.85 s, 21 MB → 102 MB, constant to the byte across its
programs — and the measurement D-262 asked for first put the frontend's share
(88%) in three SCALING DEFECTS, not in the prelude's size: the bindings
analysis allocated one state slot per statement and declaration OF THE WHOLE
PROGRAM for every function and copied it at every branch (57% of the run), and
the type table and the string interner deduplicated by linear scan. Step 1: the
resolver hands each parameter and local a dense per-function ordinal
(`SymbolTable.local_ord`/`stmt_ord`/`fn_slots`), `state_new` is sized by it,
and both tables carry hash indexes — the probe's frontend 0.72 s → 0.07 s and
96 MB → 4.4 MB, the compiler's own build 242 s → 20 s with its peak 13.4 GB →
113 MB; every rejection file reports the same set and every program compiling
no changed source emits byte-identical IR. Step 2 (D-262 §1): AN UNREFERENCED
PRELUDE ITEM IS NOT EMITTED — the writer records each non-generic prelude
function, each prelude impl's methods and vtable and each prelude generic
instance as an item, and `irw_trim_items` keeps an item only if a symbol it
defines is referenced from outside every item or from a kept item, a fixpoint
over the emitted TEXT (a reference is an `@` token, so no kind of use is
enumerated and an unanticipated one keeps its item); the probe's IR 845,283 →
50,561 bytes and 608 → 14 functions, `llc` 0.46 s → 0.02 s, the compiler's own
IR keeps 101 of 685 prelude functions, all referenced. Found on the way: the
first prelude function's item straddled the head-to-tail sink switch; the
prelude's generic instances are recorded by call sites in dropped bodies and are
items too; `src/frontend/prelude.npk` shared the language prelude's qualifier
and is `prelude_names.npk`; the elision cross-check counts a row only for a
function the emission holds. The belt (`check_prelude_trimmed`,
`ir_prelude_trimmed`, self-check cases in both runners) runs on every module the
compiler under test emits and NOT on the snapshot's (the tools, `npkg build`'s
`npkc.ll`, the runner self-check's cases). Step 2b (DEF-21, the workbench's
finding): the undefined-symbol allowlist is the runtime's EXPORTS — the 57
`internal` defines out, the `module asm`'s two `.globl` names in. Step 3: the
docs; `self.toml`'s ceiling tightened to the new peak. Step 4 (the workbench's
O-N17, found during the subcycle): a generic function moving OUT of an indexed
element at an owning `T` links -- `emit_move_out` builds the vacant helper's
symbol from the place's type THROUGH the specialization, and a registered drop
type the emitter cannot lower is EMIT-002 instead of a body silently not
emitted. Found writing its test and recorded as **S-39** (the user): an owning
`List<T>` local alive in `main` at `exit 0` is a `WildLeak` by construction --
`exit` runs joins and defers and no drops (D-183's amendment), and a List's
buffer is the one managed storage D-151 counts. `nitpick.obligations` never
moved. **1.5.2e IS COMPLETE (2026-09-05; `meta/roadmap/done/1.5/1.5.2e.md`; three
landings under full harnesses).** S-39 ratified as **D-263**: the prelude's
`List<T>` stores through `alloc_managed`, the managed heap's untracked entry,
PRELUDE-ONLY by the reference's `**Prelude-only**` marker (generated into
`builtin_prelude_only`; TYPE-054 from any other module, because a hand-written
`wild` container relies on D-151's count as its enforcement of an unpaired
free), `ralloc` keeping a block's role — a `List` alive in `main` at `exit 0`
exits 0 where it exited 94, and the exit path stays free of the drop walk as
D-183 decided. DEF-22 (the workbench's O-N18): `.len` on a fixed-size array
lowers to the constant its type carries. **1.5.2f IS COMPLETE (2026-09-05;
`meta/roadmap/done/1.5/1.5.2f.md`; two landings under full harnesses).** S-40, the
workbench's O-N19, ratified as **D-264**: a bare type parameter — and `Self` in
a trait's default body — is MOVE-ONLY in the body that names it, because a
generic body is checked once for every type it is instantiated at;
`type_owns_for_move` is the one predicate, `require_move_if_owning`, the
lending-pick binding check and the derive generator ask it (TYPE-046 with the
parameter's own reason; DERIVE-006 for a `T` payload under `Eq`/`Ord`/
`PartialOrd`/`Clone`, as for a `string`'s — `Hash`/`ToString`/`Debug` bind
nothing and still generate). Before it, `T:x = s[i]` at an owning `T` compiled,
linked and ran with two owners of one heap body — a use-after-free that exited
0. Seven sites in the tree, all a by-value `T:v` stored into an owning slot,
say `move T:v` and `move(v)` now; nothing else refuses anew. **Left open:
S-41**, a borrowing `pick` binding form — settled the next day as D-266
(1.5.2h). **1.5.2g IS COMPLETE (2026-09-06;
`meta/roadmap/done/1.5/1.5.2g.md`; two landings under full harnesses).** S-42, the
library workbench's first CI finding — the pinned commit's `build/npkc`
differed between this machine and GitHub's runner while `npkrt.o` did not —
ratified as **D-265**: D-204's toolchain pin is a VERSION and stays one (a
tool-binary digest would refuse every machine but one; z3's digest pin,
D-218.1, is the deliberate asymmetry — a solver's output is a committed
verdict, a toolchain's is checked bytes), the claim that holds across
machines is the EMISSION (`build/npkc.ll`; a difference there is a compiler
defect), every `npkg` ladder run prints one `sha256` line per intermediate
in ladder order with the harness's `parity` stage holding each to an
independent digest, and every pin notice quotes the lines with the
emission's named. **1.5.2h IS COMPLETE (2026-09-06;
`meta/roadmap/done/1.5/1.5.2h.md`; three landings under full harnesses).** S-41
ratified as **D-266**: a lending `pick (v)` binds VIEWS — each binding the
payload in place, read-only, typed as itself, no copy at the bind, no address
(TYPE-066: an assignment, `@`/`$$i`/`$$m`, a pointer-receiver call, a stateful
operation, and no view of a stateful kind at all), the selector's root FROZEN
inside an arm that binds (TYPE-067), a `move` or `pass` of a view TYPE-047; the
consuming form unchanged. One fact recorded once: the resolver links a pattern
symbol to its selector and `sym_is_view` reads the spelling. The binding's slot
holds the payload's ADDRESS in sync and coroutine bodies alike, and the
selector's root is frame-resident when an arm binds — a view across an
`await` is sound by residency, as `@x` is. The derive generator's `Eq`/`Ord`/
`PartialOrd`/`Clone` generate over `string` and `T` payloads again. **DEF-24**,
found by planning: TYPE-063 refused `@`/`$$i`/`$$m` on a limited binding but
not the implicit address a pointer-receiver method call takes — a limited
struct written through `Self->` with no trap — fixed at step 0, the site views
then mirror. Found on the way: `drop` over a refused operand reported TYPE-042
as a second sentence; it has the `raw` arm's D-240 short-circuit now.
**1.5.2i IS COMPLETE (2026-09-06; `meta/roadmap/done/1.5/1.5.2i.md`; two landings
under full harnesses).** DEF-25, the library workbench's report: `string_concat`
of two empties allocated a real 16-byte block (D-150's answer to a zero
request) and returned it with cap 0, so its drop never freed it — 16 bytes per
empty call since the primitive was written, the prelude's `string:Clone` of an
empty string and the compiler's own `string_concat(x, "")` copy idiom
included; the runtime's concat takes the branch `string_slice` has carried
since D-186, and `tests/cost/empty_concat.toml` holds the empty loop's peak to
the one-byte loop's (a factor of millions on the old runtime).
**1.5.3 (contracts live) IS COMPLETE (2026-09-06; `meta/roadmap/done/1.5/1.5.3.md`;
S-43 and S-44 ratified the same day as D-267 and D-268; four landings, each a
cumulative prefix under a full harness, D-228).** Step 0: the three trap
identities (`RequiresViolated` −4112, `EnsuresViolated` −4113,
`InvariantViolated` −4114), REACH arms each where its construct is declared,
a `failsafe` `exit` whose literal is not positive is REACH-004, `old(...)`
names the enclosing function's own parameters (D-243 read literally). Step 1:
CONTRACTS ARE CHECKED IN EVERY BUILD — a `requires` in the callee's `.req`
predicate (the function's parameter list verbatim, one trap per clause at the
clause's site), called from the checked entry of a sync function, which now
splits for a `requires` as it does for a limited parameter, or at state 0 of a
coroutine; an `ensures` at every return seam (`pass`, and the long form on a
success) with `result` the value in register and `old(...)` a snapshot at the
body's start (an alloca, or a frame slot at role 60+k); a loop `invariant` at
every head of every loop shape; `failsafe`'s exit code held positive at the
`exit` (the re-entry rule ending the process at 70); LIVE-1's four rungs
retired. Step 2: CONTRACTS ARE PROVEN WHERE THEY CAN BE — a `requires` row at
every call with a recorded callee (`bypass` at a direct sync call, `held` at an
`await` or through a `dyn`: recorded, never elided) and at every function's
entry, an `ensures` row per return point, a loop's entry, preservation and
`continue` rows with the invariant and the condition hypotheses inside the
body and the invariant a hypothesis after a loop nothing `break`s (the
prelude's `npk_gcd256` division discharged through its guard), `failsafe`'s
postcondition per `exit`, conformance rows in a space of their own, a `pure
never fails` callee an uninterpreted function and a callee's `ensures`
knowledge at every success-only unwrap; `rows.txt` carries each row's site,
role, group and traps, both runners' belts count sites and traps BY GROUP and
spell the manifest word by role; `nitpick.obligations` re-recorded at 178 rows
(the compiler's own 37 `failsafe-post` rows). Step 3: the docs. Found on the
way: the rung test `inline_mod.npk`'s hidden construct was a `requires` (a
`prove` now); every verify test names its `failsafe`'s rows, since every
`failsafe` has them (`expect-obligation: none` names no program any more).
**1.5.4 (path conditions, the counters, `prove`/`assert_static`) IS COMPLETE
(2026-09-06; `meta/roadmap/done/1.5/1.5.4.md`; six landings, each a cumulative
prefix under a full harness, D-228; S-45…S-48 landed under their
recommendations and were ratified 2026-09-07 as D-269…D-272).** Step 0 fixed five
findings: DEF-26 (a division inside a pick EXPRESSION's arm had no
obligation row since 1.5.0 — the arms were never walked; one enumeration,
`stmt_expr_ids`, now feeds every walker that must see into a statement's
expressions), DEF-27 (a callee parameter sharing a name with an address-taken
caller local bound unnamed in the contract substitution), DEF-28/DEF-30 (the
counted loop's head: a literal zero step compiled and trapped at run time
where D-022 promised a compile error, and the argument count was never
checked — `NITPICK-TYPE-068`), DEF-29 (the compile-time evaluator's counted
loops took their direction from the step's SIGN and read `till(limit, step)`
as `loop(lo, hi)`: a comptime descending sum was 0 where its run-time twin is
55). Step 1: PATH CONDITIONS — a branch condition is a hypothesis in its arm,
its negation after an arm that never falls through, a `pick` arm's pattern
(encoded under the SELECTOR's type: the checker types no pattern for its own
sake) and guard, a loop's negated condition after it, `when`'s two blocks; 13
of the compiler's 21 open rows discharged. Step 2: THE MERGE — an arm's facts
survive it guarded by its condition, versions merge as `ite` after `if`/
`when`/`pick`, a pick expression's value is its arms' `give` chain, `&&`/`||`
/ternary arms; `divz_after_branch.npk` keeps 1.5.0's promise; the walk 31.7 →
34.9 s on the compiler's own build. Step 3: THE COUNTERS — `$` and a range
`for`'s binding as terms (start, in-range body symbol with the residue class
of a numeral step, incremented at the preservation rows, the exit value after)
and the new kind `loop-step` for a computed step (S-46), a literal step
getting neither compare nor row. Step 4: `prove` lowers to nothing and is a
guard-less row that becomes a hypothesis after its site (S-48), the VERIFIED
build refusing an undischarged one with `NITPICK-VERIFY-001` (S-45);
`assert_static` folds at the frontend (`NITPICK-TYPE-069`); every `pick` and
`assert_static` is a `checker` row (`c` in `rows.txt`, tier `-`, word `none`);
a verify test's `expect-error` means the verified build refuses; the rung
suite retired (S-47) and both runner self-checks' negative construct became a
type mismatch the snapshot and the compiler under test refuse alike. Step 5:
the docs. `nitpick.obligations` at 184 rows (six `exhaustive checker`), 15 of
24 open rows discharged, none moved the other way. **Recorded for the user:**
DEF-31 — an inline module's members cannot be reached from the module that
declares it (`use nested.*;` is RESOLVE-002, `nested.fetch(...)` is
TYPE-007), found when the rung file became a positive program; which
spelling should reach them is a language question — ratified as D-273 and
landed as 1.5.4c. **1.5.4c (a module symbol means one thing, D-273) IS
COMPLETE (2026-09-09; `meta/roadmap/done/1.5/1.5.4c.md`; three landings, each a
cumulative prefix under a full harness, D-228).** The qualified call `m.f(x)`
is a DIRECT call of the member — an inline module's, an alias's, a nested
path's — through ONE walk (`namespace_path`, with the visibility rule at every
hop taken from outside), ONE direct-call typer (`type_direct_call`, factored
out of `type_call`) and ONE direct-call emitter (`emit_direct_call`, out of
`emit_call`); the node's `ns_decl` slot records what a path resolved to, and
every reader of a method call's receiver skips it when set (the 36 sites are
classified in the record). `nested.MAX` reads a binding, `?! nested.Boom` an
error constant (the reach analysis reads the record). `use nested.*;`, `use
nested.{f, Point};`, `use nested.f;`, `use core.math as cm;` bind a module
symbol's public names through the file forms' own binders — the only way an
inline module's TYPES are named from outside — a first segment naming no
module is RESOLVE-002 (R-1), a later one the module lacks RESOLVE-007, a
private one RESOLVE-003 from either spelling, `std` owned (RESOLVE-001); a
module symbol carries its scope wherever it is bound, so `helpers.f()` works
across files after `use "./h.npk".*;`, and order never matters by the
fixed-point import loop (R-2 without a second pass; the `tests/accept/` pair).
**Found on the way, fixed**: `Trait.method(recv, s)` passed no callee to the
argument check, so a `move` parameter never demanded `move(s)` through the
qualified spelling — the callee freed the string and the caller freed it
again; an async METHOD spawn (`drop j.run()`) armed no `DeadlineExceeded`
where the direct form did; a struct read through a module was typed invalid
with NO diagnostic and died as EMIT-002; the emitter wrote no type header for
a struct or enum declared inside an inline module; and the checker resolved a
body's type names inside an inline module in the FILE's scope, so a module's
own function could not name the module's own struct. **Recorded for the
user**: S-49 (a member-less `mod:name;` import binds a module symbol with an
EMPTY scope, so `name.f()` reports "no member" rather than reaching the file
— recommended: carry the loaded file's scope, as the alias does) and S-50 (an
error constant declared inside an inline module hashes under the FILE's name,
so its `failsafe` arm is `(file.Name)` and `(nested.Name)` matches nothing —
recommended: the file qualifies, and an arm's first segment is checked
against the module names the program knows). **Both ratified 2026-09-10 as
D-274 and D-275 and LANDED the same day as 1.5.4d** (`meta/roadmap/done/1.5/
1.5.4d.md`; four landings, each a cumulative prefix under a full harness,
D-228): a member-less `mod:name;` import carries the loaded file's scope —
`graph_load_file_module` records the entry it loaded and `graph_collect_all`
links the symbol to it LAST, before any import round can copy an empty scope
into a re-export — so `name.f()`, `use name.{g};` and a `pub mod:name;`
across files are one meaning with the alias; and every `pick` arm over an
`Error` is checked at the arm by the checker (`type_pick_rules`, both
spellings, `failsafe`'s and any other): an identity no declared constant
hashes to is RESOLVE-002 with the fix named (R-3, the PAIR check —
`(nested.Boom)`, `(file.Nosuch)`, `(nosuch.Boom)`, `(x.DivByZero)` all
matched nothing silently before), any other shape TYPE-007 (R-4, where each
died as EMIT-002 in the emitter); ONE `error_identity` (intern.npk) for the
resolver, the emitter and the checker, and the resolver's (declaration,
qualifier, code) rows on the `SymbolTable`. **Found by the first test, fixed:
DEF-32** — the reach analysis recorded a constant's qualifier from the module
of the first SITE that reached it (the root, walked first), so a root that
unwrapped an imported constant was told to name `(root.E)`, a pair no code
hashes under, and the correct `(file.E)` was refused — the exhaustive-
`failsafe` guarantee hollow for cross-file constants named qualified since
1.1.6 (the bare `(E)` matches by symbol origin and hid it). **Found by
planning and ratified the same day as D-276 (S-51)**: a `use` or a
`mod:name;` written INSIDE an inline module bound and loaded nothing,
silently, while an inline module is sealed from its own file's imports — so
the one construct that could reach an import from inside one was a no-op;
the loader, the import pass and the file-module link are one walk shape now
and descend into inline modules under the same rounds and refusals as at
file level (`inline_imports.npk`). `nitpick.obligations` never moved.
**1.5.4b (the remaining theories, D-218 items 4 and 5) IS COMPLETE
(2026-09-10; `meta/roadmap/done/1.5/1.5.4b.md`; S-52…S-57 ratified the day they
were raised as D-277…D-282; seven landings — steps 0–4, 4b, 5 — each a
cumulative prefix under a full harness, D-228).** Step 0 (D-277): a shift's
amount is DEFINED for `0 ≤ n < width` and nothing else — a known amount
outside it is TYPE-070 at the shift, a computed one is one unsigned compare
trapping `ShiftRange` (−4115) and the `shift-range` row (the planning probe
had shown `shl` poison reaching LLVM unguarded; the folder had bounded every
width by 64); the snapshot refreshed by the seed README's bridging variant
because forty roots' `failsafe`s had to name the new constant. Step 1
(D-279, D-280): every bitwise operation is encoded — the Int forms wherever
an operand is a numeral (a shift by `k` as `mod`/`div` by `2^k`, the
low-bits, one-bit and cleared-low-bits masks, `~`), at any width for a few
hundred rlimit, and the bit-vector crossing (`int2bv`/`bv2nat`, exact by
construction) at words of at most `BV_CROSS_MAX_BITS = 64` — measured free
at 64 bits, 4% of the budget per row at 128, 18% at 256, the budget at 2048
— with THE GATE that no discharged row regresses held at every step; a flag
family is an unsigned 32-bit word to the encoder. Step 2 (D-278): the
twisted kinds are unbounded `Int` in the carrier's range with ERR = `MIN` a
VALUE the terms carry, every operation the emitter's own saturate-to-ERR
`ite`, and `err-exit` rows live at every `TbbErr` guard (a compare's
operands, a cast out under both spellings, a checked crossing into or
within a family) — the compiler's own manifest 147 → 368 rows (+213
`err-exit`, the prelude's generated impls), a twisted division rowless;
three defects found and fixed: **DEF-33, a soundness hole with two faces —
a guard's fact pushed under a QUIET encoding (a call-site contract row
proved `n < 32` from itself) and inside a LOUD clause (`requires (1i32 <<
n) != 0i32` was discharged of itself and its check elided)** — closed by
one rule that is the meaning of a proposition from here on: A PROPOSITION
HOLDS ONLY WHERE ITS EVALUATION DOES NOT TRAP (every guard met inside a
clause is conjoined into the proposition's term and pushed as a hypothesis
nowhere); DEF-34 (`smt_int` trapped on a negative numeral — the compiler
died under `--obligations` at the first negative numeral in any row,
reachable since step 1's `fixed` fold); DEF-35 (a negated literal pattern
refused by the emitter). Step 3 (D-281): floats in two tiers —
`flt32`/`flt64` as IEEE-sort terms with every operation the emitter's
instruction (`fp.add`… under RNE, `fp.sqrt` for `#sqrt`, the ORDERED `fcmp`
predicates, a literal `to_fp` of its exact decimal), the manifest's tier
column fed by the encoder (`int`/`bv`/`fp`/`-`; `real` for a tier-2
discharge — the constant `int` until now), and the Real-interval twin
(`NNNN.t2.smt2`, every float a Real within `ε·|v| + η` of the exact
operation) written only under three soundness conditions (every float
symbol bounded both ways by comparison hypotheses; every intermediate's
magnitude within the normal range CONJOINED to the goal; the goal a
comparison), asked by both runners only after a tier-1 `budget`:
`flt_tier2.npk`'s `#sqrt(a*a + b*b) >= 0.0` is `unknown` in QF_FP at the
rlimit and `unsat` in the twin — D-218 (5)'s shape, end to end. Step 4
(D-282): a `simd<T, N>` value is N scalar terms — through expressions AND
bindings (S-58, the reading recorded for the user) — and a `simd`
division's any-lane guard is ONE `div-zero` row (one `div-min` for a signed
element) over the lane conjunction, a shift's one `shift-range` row; the
`unencoded` producers of 1.5.0 retire — and the step's first harness caught
the vector emitter's guards ignoring the manifest (rows discharged, traps
kept; fixed: each elides into one `llvm.assume`, the belts being the
instrument that saw it where a check of rows alone was green). Step 4b (DEF-37): the reach analysis
demanded `DivByZero`/`DivOverflow` arms of every program whose only
division was a float one — IEEE-total, a bare `fdiv`/`frem`, an arm nothing
can enter; both sites read the float kind now, a `simd` division its
ELEMENT's. **Recorded for the user: S-59/DEF-38 — a `simd` integer lane's
`+ - *` lowers to a bare vector `add` and WRAPS where its scalar traps
(D-210), measured (a lane at `INT_MAX` plus one read back negative)**, and
DEF-36 (a program's `?! DivByZero` and a guard's trap share one text, so
the runners' belts count it — an `npk_raise` floor entry is the recommended
fix, D-203's). The compiler's own set: 368 rows in 197 function files,
decided in 3.9 s under the profile. **1.5.4e (the 1.5.4b close's three) IS COMPLETE (2026-09-11;
`meta/roadmap/done/1.5/1.5.4e.md`; ratified 2026-09-10 as D-283…D-285; three
landings, each a cumulative prefix under a full harness, D-228).** Step 0
(D-284): a `simd` integer lane's `+ - *` computes through
`llvm.{s,u}{add,sub,mul}.with.overflow.<N x iW>` — legal at every lane
count and width, measured through `opt -O2` with no libcall — the overflow
lanes folded to one any-lane test trapping `IntOverflow`, the intrinsic
declared once per module after the bodies (`llctx_note_decl`); an integer
`.sum()` folds through the scalar core and traps per step; the reach
analysis reads a lane's element kind. DEF-39 fixed with it: `v op= w` on a
`simd` was admitted by the checker and EMIT-002 in the emitter since 1.3.3
— `emit_arith_value` dispatches a `simd` operand to the vector binop now,
and the encoder records the compound form's any-lane rows at the target's
site. DEF-41, found on the way: a compound shift through a field or element
had its `ShiftRange` guard and no row (the field-target branch recorded the
division's and not the shift's), so a verified build carried a trap the
belts could not account for. Step 1 (D-285): `npk_raise` in the floor — one
call of `npk_trap` under the program's name, class `syscall`, an export by
construction — is where `?!` and `!!!` enter, every guard keeping
`npk_trap`; the belts count `@npk_trap(` alone and each runner's self-check
holds the pair; DEF-36 closes (`?! DivByZero` in a verified build), and
DEF-40 with it: `!!!` had called `npk_failsafe` directly — no frozen flag,
no re-entry guard, no driver kill — and `trap_stmt_reentry.npk` exits 70
where it exited 33 through a second `failsafe`. `npkrt.o`'s digest moved for
the first time since DEF-25's fix; the notice to the library listener named
both. Step 2: the docs. `nitpick.obligations` never moved.
**1.5.5 (the aliasing half of D-004) IS COMPLETE (2026-09-11; `meta/roadmap/done/1.5/
1.5.5.md`; planned execution-grade on `cb8cbb0`, S-60…S-62 ratified the same day
as D-286 and D-287; four landings, each a cumulative prefix under a full
harness, D-228).** Measured first: nothing decided aliasing — two `$$m` of one
element ran (exit 32), the spec's qualifier example never parsed (the operator
form is the language's) — and planning found DEF-42 (a borrow assigned to an
OUTER holder accepted; rule 3 unchecked for local holders, benign only by the
emitter's construction) and DEF-43 (a write through a pointer to a `fixed`
module binding was a SIGSEGV with no `failsafe`). Step 0 (DEF-42):
`check_holder_scope` in `escape_assign`'s bare-local branch over every root the
value carries. Step 1 (D-286, D-287): `src/frontend/analysis/alias.npk` — `$$m`
EXCLUDES, `$$i` SHARES, `@` claims nothing but is a write-capable access;
lifetimes LEXICAL (a call argument for its call, a pointer local's value for
the holder's whole scope; a `defer` body sees its blocks' claims; NLL and
two-phase borrows OUT); a claim only as a whole call argument or a pointer
local's whole value, a holder used and not copied (BORROW-014 elsewhere); the
conflict table by the paths' common prefix (BORROW-013 for a static overlap);
one forward walk per function, no fixpoint; TYPE-071 (a `fixed` binding, or a
`fixed` field, has no address) beside TYPE-063; the tree's cost five sites
(`diaglist_sort` had moved an element out from under its own live `$$i`). Step
2 (D-286 §5): a computed-index overlap is a RUNTIME GUARD in every build — the
byte-range compare at the access in `addr_of`, `BorrowOverlap` (−4116) armed by
reach — that the verified build elides through the `disjoint` row (kind 20;
`-4116` in both runners' tables; the party's index terms memoised when its claim
was encoded; one row per site): the spec's own example discharges under the two
rules' Int `%` forms. Step 3: the docs. Found on the way: step 0's root
collector skipped the custom-shaped kinds (a call's arguments, a literal's
values), and the compiler's own `failsafe` cannot name a new prelude identity
until a snapshot refresh (D-205). `nitpick.obligations` did not move; no
snapshot refresh.
**1.5.6 (the floor's spec and the executor primitives) IS COMPLETE
(2026-09-12; `meta/roadmap/done/1.5/1.5.6.md`; S-63…S-70 ratified the day they
were asked as D-288…D-292; six landings, each a cumulative prefix under a
full harness, D-228).** The floor had been the one part of the artifact with
no evidence of its own: hand-written LLVM IR, permanent by D-203, trusted
because nothing could check it. It now has a SPECIFICATION
(`runtime/npkrt.spec`: 137 sections stating what each symbol does to memory
— `requires`/`ensures`/`frame`/`objects`, loop invariants and exact
unrollings, `(summary)` calls, `(ensures-trap …)`, and a `(boundary "…")`
promise where the kernel is the answer), a TRANSLATOR that reads the floor's
own IR against it (`npkg/floor_smt.npk`), six PROTOCOL MODELS with sixteen
controls (`runtime/models/`), and an enumerated SYSCALL BOUNDARY. 370 rows
live in `runtime/npkrt.obligations`: 363 discharged, 7 residue, none
refuted — and **an `open` floor row is a run failure by name**, because
nothing in the floor is a guard to retain: a counterexample there is a
defect or a false claim, both stop-the-line. **Four floor defects were fixed
on the way, none found by a test**: D-290 promoted four plain cross-thread
accesses that race in LLVM's model (a frame's `windup`, a channel's
generation, `@npk_frozen`, the join's tid word); D-291 made a trap a
whole-program event as D-063 always said (a thread registry, a SIGUSR1 stop
handler, `cmpxchg` arbitration of the failsafe holder, the stop walk under
the join deadline — two threads trapping at once used to run two
`failsafe`s); D-292 gave `failsafe` a 1 MiB preallocated region to allocate
from, so a handler can never block on the heap mutex a stopped thread holds;
and **DEF-51** — `npk_read_file`/`npk_read_stdin` leaked every buffer they
outgrew, invisible to D-151 and to every test, found because a frame claim
would not discharge. TCB.md is FINALIZED: three generated regions (the
membership table with each symbol's class and disposition, the syscall
boundary, and what the evidence does not cover) that cannot go stale without
a red run. **S-71, RATIFIED by the user 2026-09-17 as an amendment to D-218
(2)**: the determinism profile carries `lp.dio=false` — z3 4.16.0's Diophantine sub-solver undoes its terms at
every `(pop)`, so a row that answered `unsat` in 8 s returned 200 s later
and a larger one not within 22 minutes, a wedged solver under P-13 with only
the runners' hang net to catch it; off, no verdict moves anywhere.
**1.5.6b (the floor's evidence, re-examined by measurement) IS COMPLETE
(2026-09-17; `meta/roadmap/done/1.5/1.5.6b.md`; planned execution-grade the day of
the s6→s7 hand-off, S-72…S-76 ratified the day each was asked as D-293…D-296
and an amendment to D-288; nine landings — steps 0–4, 4b, 4c, 4d, 5 — each a
cumulative prefix under a full harness, D-228).** The outgoing seat named four
places it would try to prove 1.5.6's evidence wrong, and the first, taken up by
MEASUREMENT, paid within the hour. Step 0, the stack rule: **DEF-52** —
`npk_hardware_concurrency` handed the raw `sched_getaffinity` an unzeroed
128-byte mask, the kernel wrote 8 bytes and returned the count (the zero-fill is
glibc's wrapper's, and this runtime has none), and the popcount over all
sixteen words answered 1008 on a 48-thread machine; **DEF-53** — D-173's
entry-block rule had never been pointed at the hand-written floor: eight of
fifteen allocas outside their entry blocks, two in loops (a loop-body alloca
SIGSEGVs at the pinned `-O0` and `-O2`). All fifteen sit in their entry blocks
and are fully DEFINED there, held by D-173's own check over the floor and a
new `alloca-not-defined` belt. Step 1: the kernel-effect table is ONE generated
authority (VERIFICATION_REFERENCE §9.2's `kernel-effects` region; four
hand-maintained lists became zero) and `kernel_effects.npk` holds every write
row to the RUNNING kernel, demanding equality — four things were wrong in the
table, one in the unsound direction, and `npk_hardware_concurrency`, specified
at last, has six rows of which exactly one is refuted on a floor without step
0's memset: the method would have found DEF-52. Step 2: `park-unpark` read
against `npk_park_sleep` case by case — a deviation, a MISSING STEP (the epoll
wait's empty return: the model's reachable states went 263 → 358, no verdict
moved) and a gap, the seventh model `reactor-io`. Step 3 (D-293):
`hardware_concurrency()` is a builtin — its own program exits 20 on the old
floor at its FIRST call. Steps 4 and 4b (D-294, D-296): a builtin's name is
the compiler's — a module-level function, an `extern` METHOD (its stub is one)
and any CALLABLE binding of that name are `NITPICK-RESOLVE-001`. Step 4c: three
defects of the function-value corner found by writing 4b's test (DEF-54: a call
through a pattern-bound function value emitted a symbol that does not exist;
DEF-55: `raw o.f(x)` refused where `raw (o.f)(x)` was accepted; DEF-56: a trait
method with a function-typed parameter could never be implemented). Step 4d
(D-295): **the models are read a SECOND way** — exhaustive explicit-state
search, no bound and no solver, a belt in both runners beside the solver's
rows, the twins byte-identical on twelve planted texts; three of seven models'
depths were smaller than their diameters, a model's meaning had had ONE reader,
and the belt's first finding was the runner self-check's own toy model,
commented safe and unsafe in two steps. TCB.md §5's eighth and thirteenth
acceptances are narrowed. `runtime/npkrt.obligations`: 379 rows, 372
discharged, 7 `budget`, 0 open; `nitpick.obligations` never moved; the floor's
bytes moved once.
**1.5.6c (what the floor's spec ASSUMES of its callers) IS COMPLETE
(2026-09-17; `meta/roadmap/done/1.5/1.5.6c.md`; approved by the user to run before
1.5.7's plan; five landings, each a cumulative prefix under a full harness,
D-228).** A section's `requires` and `(objects …)` are HYPOTHESES of its rows,
and nothing checks the callers no row covers. **Two were FALSE for a legal
caller**, neither a behavioural defect, both evidence that said nothing where
it looked like it spoke, and no solver could have said so — they were found by
READING: `npk_string_concat` assumed its two inputs apart (`string_concat(s,
s)` is in the tree twice; the proof never needed it) and `npk_small_free`
assumed a chunk apart from the head of the list it was on, the ordinary LIFO
free. Step 0: `(lo len apart-when COND)` — a range in the address space always,
set apart only where the body reads it. Step 1: `(views (lo len) …)` — ranges
handed over to READ: apart from objects, never from each other, read-only
PROVEN by the frame row; and two silences refused in both runners (a clause
head the translator reads once, written twice; a loop sub-clause it does not
read). Step 2: every other apartness argued in the spec beside its clause, on
six named invariants. Step 3: TCB.md §4d GENERATED in both runners — per
section, the callers a row covers and the callers nothing does (31 sections: 2
covered, 18 with an unproved floor caller, 15 exported) — with §5's sixteenth
acceptance. No verdict moved (25 hashes over two symbols), no floor byte, no
ladder row. **S-77 — the solver's hang net leaving `npk_small_free` at 81% of
its bound — was settled as D-297 and LANDED 2026-09-17 as the s8 seat's first
landing, before 1.5.7 step 0:** the net is `120 + 10·checks + 60·B` seconds per
file in both runners, B the file's rows the committed manifest records `budget`
(B = checks with no manifest to trust); `npk_small_free` is decided under 610 s
where it was 250, and nothing else moved.
**CYCLE 1.5 IS COMPLETE (closed 2026-09-25 at 1.5.8d, `meta/roadmap/done/1.5/1.5.8d.md`)**: step 0 (`c93d80d`) landed D-317, D-318 and DEF-94; steps 1–3 landed as ONE commit — the one-hop refresh from the final `src/` (stage2 == stage3, 28,111,929 bytes, sha256 `4029fc70efbe9cd3da26b7fb379b477b5dc9417bba3cb0359d5dd25126a1f337`, 3,392 defines every one `"split-stack"`, zero absolute site paths), the doc sync (`done/1.5/README.md`'s "What cycle 1.5 taught", ROADMAP's compact form, the 1.6 README's start-here note, 87 living citations re-pointed at `done/1.5/`) and the archive; the harness green, 52/52, `parity` 1,711 verdicts agreeing, npkc byte-identical, `check_decisions_current` silent with 1.5 under `done/`. What remains of Phase C is cycle 1.6. **The paragraph below is the close's record of what the sentence said before it (kept for the history it carries):** ONE subcycle — 1.5.8d, the
cycle's close, PLANNED execution-grade 2026-09-24 (`meta/roadmap/done/1.5/1.5.8d.md`:
S-97/S-98 the two leads decided before the refresh, then the refresh, the doc
sync, the archive to `done/1.5/` and the 1.6.0 briefing). **Step 0 LANDED
2026-09-25** (D-317, D-318, DEF-94; the numbers in `1.5.8d.md`'s record and
D-317's landing note); steps 1–3 remain — the refresh from this `src/`, the doc
sync and the archive, ONE commit under one full harness. 1.5.8b, the
`overflow`, `bounds` and `cast-range` rows, is COMPLETE (2026-09-23,
`meta/roadmap/done/1.5/1.5.8b.md`), and 1.5.8c is COMPLETE (2026-09-24, seven
landings — step 0 landed 2026-09-24: the four codes declared; step 1 landed 2026-09-24: the mechanism, TYPE-072 dormant for the `neither` shape; step 2 landed 2026-09-24: the one-hop snapshot refresh — the committed builder parses `decreases`/`unbounded` and carries DEF-90's fix; step 3 landed 2026-09-24: THE SWEEP — every `while`/`when` of the tree states its clause, 977 loops: 392 written by the tool from the shape it proves, 563 by the reading committed as `meta/roadmap/done/1.5/tools/decreases_read.txt`, 72 of them `unbounded` with a reason (D-316), the evaluator checking a measure it runs, every `failsafe` naming `(DecreasesViolated)`; step 4 landed 2026-09-24: TYPE-072's `neither` shape LIVE, the recursive groups (`analysis/recursion.npk`) with TYPE-074/075, a FUNCTION's `decreases` checked at every call inside its group through the generated `<sym>.measure` predicate, the `terminate` call rows, the `stack-depth` rows derived by both runners, DEF-92 found by measuring the sweep's cost; step 4b landed 2026-09-24: DEF-92 fixed — `tt_instance`'s linear scan, 85% of the frontend, replaced by an instance index, the checker 5.4x faster and every program's emission byte-identical; step 5 landed 2026-09-24: the `terminate` residue measured by cause with `meta/roadmap/done/1.5/tools/residue.py` — 684 of 1,183 row sites in the compiler's own build discharged, 240 open through a pointer (E-4), 195 through a by-value aggregate's missing value term (lead E-6, OPEN_DECISIONS §4), 64 on their merits, the 116 `stack-depth` rows `open` as D-304 (5) accepted — the docs synced, `1.5.8d.md` planned). **1.5.8b's
planning measured first, and the user settled SEVEN questions the day each was
asked (D-308…D-314).** A struct field may carry `limit<Rules>` (D-308). The
overflow rows nothing proves stay guarded, measured and reported (D-309). A
certain constant overflow is refused (TYPE-076, D-310). `uint64`'s upper half
is built with bit operations, `~0u64` (D-311). The wrapping family `+% -% *%`
was the user's own question (D-312). And planning found THREE memory-safety
holes, each confirmed by a probe: a string's `.len` writable (DEF-72), the
prelude `List`'s `cap` writable (DEF-73), and `List` elements reached by
unchecked raw-pointer indexing everywhere (DEF-74). The user settled the field
qualifiers `sealed` (read anywhere, written only by the declaring module;
D-313) and `hidden` (neither; D-314), with `List` indexed `l[i]` and checked.
They land first, before any row. **Step 1 LANDED 2026-09-19**: `sealed` and
`hidden` are keywords and field qualifiers — TYPE-079 a write to a sealed field
from outside its module (every write form: an assignment through any path, a
compound, a struct literal, a `move`/`pass` out of an owning one, `@`, `$$m`, a
`Self->` receiver, a stateful operation; `$$i` reads), TYPE-080 any touch of a
hidden one, TYPE-081 either off a field — "outside" drawn where a private
member's line is; the compiler-known headers (`ptr`/`len`/`cap` of string,
cstring, slice and buffer, and `OwnedFd.value`) are sealed BY DEFINITION in
every module (DEF-72 fixed); and two holes of the same class found on the way
and closed: an `RGuard`'s `.value` was read-only only as an assignment's direct
target (`g.value.x = 5`, `@g.value.y`, `$$m g.value.x` were accepted — writes
through a SHARED hold; DEF-77), and `@f.value` on an `OwnedFd` was accepted
(DEF-78). Every program's IR is byte-identical. **Step 1b LANDED 2026-09-19**:
the prelude's `List` is `{ hidden wild T->:items; sealed int64:count; sealed
int64:cap; }` (DEF-73, DEF-74 fixed), `l[i]` is the element bounds-checked
against `count` and `l[lo...hi]` a checked view, a list behind a pointer is
`(<-p)[i]` (TYPE-082 refuses `p[i]` on a pointer to an array, a slice or a
`List`: pointer arithmetic that type-checked), and the prelude's checked
operations `list_pop`/`list_truncate`/`list_clear`/`list_insert`/
`list_remove`/`list_swap_remove` are how a list changes; 2,011 element accesses
swept, the compiler's own `OutOfBounds` guards 13 → 765, and the new check
found DEF-79 on its first run (the NONE declaration stored past `count` since
1.4.7). A bridging snapshot refresh carries it. **Step 2 LANDED 2026-09-19**:
a constant `+ - *` or negation is computed exactly at its width and refused
when it does not fit (TYPE-076, D-310; `0u64 - 1u64` among them — D-311's
`~0u64` and `(1u64 << 63u64) | k` build the upper half), emitted as its
constant when it fits (DEF-70: all 85 constant checked operations gone), and
the folder's every other operation is now the machine's (DEF-80: a wide shift
trapped the compiler, a narrow `<<` and `~` were not truncated, `uint64`
divided and compared signed). **Step 6 LANDED 2026-09-19** (D-308 §§1–5,
the field-limit MECHANISM; §§6–7 — the 2^47 ceiling, the length producers'
checks, the built-in length fact and the prelude `List`'s own rule — are step
6b's, split at the decision's seam because they move the floor's bytes): a
struct field may carry `limit<Rules>`, a rule about that ONE field, checked
after each of its three write points and a FACT at every read. The rule must
hold of the field's vacant value, decided by the constant folder at the
declaration (TYPE-077) — and a rule the folder cannot decide there is refused,
since nothing decides it later. A limited field has no address, through a
pointer to its struct as well (TYPE-063 extended: the case a rule about a
BINDING could never see). `field_limit.npk` is the payoff — a `div-zero`, a
`div-min` and three `overflow` rows discharge on facts that exist only because
a field carries a rule — and every existing program's emission is
byte-identical (the compiler's own moved, since `src/` grew), so no verdict
moved. **Step 6b LANDED 2026-09-23 (DEF-86)**: the reach analysis FOLLOWS
CALLS INTO THE PRELUDE — since 1.1.6 it had walked the program's modules and
not the prelude, on the premise that a program reaches the prelude's guards
only through machinery its own text contains, and `list_pop`'s `!!! OutOfBounds`
on an empty list falsified it: a program with no index of its own was told
`OutOfBounds` could not arrive and the raise landed in `(*)` (exit 44 where the
armed program answers 55). Every resolved callee is walked once now, a trait-
method callee reaches every impl of the trait, a function named as a value is
reached; 20 of the tree's 502 roots gained an arm (13 `IntOverflow`, 7
`OutOfBounds`, 4 `TbbErr`, 1 `ShiftRange`, 2 `BadPath`), no other diagnostic
moved. **Step 6c LANDED 2026-09-23 (D-308 §§6–7; DEF-76, DEF-87 fixed; DEF-88 found; D-315)**: the
CEILING — a request above 2^47 bytes is `HeapBadRequest` at every allocator
entry, ONE unsigned compare in each of the allocator's three entries (the
aligned entry bypasses the core), a PROMISE on `npk_alloc_internal`'s summary
that every translated caller's rows consume (the core is `boundary`: nothing
decides it; TCB.md §5 (10) accepts it; `alloc_ceiling.npk` tests it); the two
lengths no allocation makes (`string_from_bytes`, `#wild_slice` — measured:
exactly two) guarded by the EMITTER at the call, their `bounds` rows under the
same key, `OutOfBounds` armed there, a narrow count widened by its sign
(DEF-87: an `int32` variable count reached `llc` as a type error before); the
built-in length FACT `[0, 2^47]` at every `.len`/`.cap` read, through a pointer
too; the prelude's `pub Rules<int64>:ListLen` on `List`'s `count`/`cap` (so a
program that pushes to a `List` names `(LimitViolated)` — 64 of 506 roots
learned an arm); a one-hop refresh (stage2 == stage3, 26,612,308 bytes,
`557ec18f…`); the manifest 3,634 obligations — `overflow` 949 → 1,102
discharged, 1,113 → 964 open, the facts closing 149 rows of D-309's residue,
the gate over 2,368 shared rows moving nothing; the floor's bytes moved (its
first since `6340d5c`), its 388 rows unmoved, D-303's sweep 41 of 41 real
programs agreeing (the one real-child program disagrees by design, on both
floors, and is skipped by its marker now). **Step 6d LANDED 2026-09-23**: `intern.npk`'s `fnv1a_step` spells `*%`
(step 4's owed item, the first `src/` change after the refresh that carried
the operator into the builder), and **DEF-88 fixed** — `_` binds nothing in
the emitter as it always did in the checker: a lending arm `(Variant(_))`
over an owning payload lowers (EMIT-002 on every compiler before, unrun since
`tests/accept/` asks only the frontend), a consuming arm of wildcards owns and
drops the whole value, and a consuming `(Two(a, _))` drops the `_` payload in
place at the bind — measured at 21 bytes peak over 2,000 rounds. **Step 7 LANDED 2026-09-23 — 1.5.8b IS
COMPLETE**: `1.5.8c.md` planned execution-grade over a loop census (851 loops:
~550 a tool writes, ~300 read), `meta/NOTICES.md` (the notice log), the accept
suite EMITS each file it accepts (DEF-88's lesson), the two "byte-identical
emission" sentences corrected, DEF-85 fixed (a test's join deadline is a hang
net: `failsafe_alloc.npk` at sixty seconds) and D-316 (an event loop says
`unbounded` with its reason; S-96, the user). **Step 3 LANDED 2026-09-19**: every plain-integer
`+ - *` and negation is an `overflow` row at its guard's site (one over a
`simd` operation's lanes, one with N−1 traps for an integer `.sum()`), a
discharged row's branch becomes one `llvm.assume`, and the compiler's own
manifest is 2,439 rows — 1,273 discharged, 1,161 open, 0 budget, 0 unencoded.
D-309's residue is measured and reported by shape (a bounded counter
discharges; a length, an unbounded counter, a sum of two unknowns and the
prelude's numeric cores stay), no bound was written into the tree to close a
row, and the speed says the guards are free: removing 1,181 of the compiler's
2,307 `IntOverflow` traps does not move a 70-second compile. **The obligation
table now holds only what the emission holds** (D-262's rule carried to the
rows: 145 prelude functions' rows left it, the 213 `err-exit` of 1.5.4b among
them). Three defects were found on the way: **DEF-81**, a soundness hole — a
guard inside a loop's invariant was elided on the proof for the head's FIRST
visit, and the verified build divided by zero where the plain build trapped
(rows per context now, `rows.txt`'s twelfth field, and the belts count guards);
**DEF-84** — an index through a call's result or a `Result`'s `.value` was
admitted by the checker and refused by the emitter (EMIT-002), for arrays since
each arm was written; and **DEF-83** — an explorer control's finding seed,
which spins to the step budget by design, could pass its 60-second net under
load and read as a blind control (300 s now). **Step 4 LANDED 2026-09-19**: the
WRAPPING FAMILY `+% -% *%`, with `+%= -%= *%=` (D-312, the user's own
question) — arithmetic modulo 2^N at its trapping twin's precedence, with no
guard, no obligation row and no `failsafe` arm, refused by name on every kind
that owns its own arithmetic (NITPICK-TYPE-078, sixteen shapes pinned), folded
WITH the wrap (in `uint128`, since the compiler's own `+ - *` trap), and
modelled exactly by the encoder as `(mod t 2^N)` — two's complement for a
signed width — so a row after the site knows the value. FNV-1a 64 and
splitmix64 reproduce their published sequences, and the prelude's `fnv_mix`
adopted `*%`: a 128-bit multiply, an `IntOverflow` guard that can never fire
and a truncation became one `mul i64` in every program that hashes
(`intern.npk`'s copy of the same step waits for a snapshot that parses the
operator, D-205). **Step 5 LANDED 2026-09-19**: the `bounds` and `cast-range`
rows — an index inside its bound at every checked access (one row for a range
slice's pair) and a float's `=>!` cast to an integer inside its target's, each
goal the EMITTER's own guard read back, each elided into one `llvm.assume`
where discharged. A CONTAINER'S LENGTH IS ONE TERM (`(|npk.len| base)` per
binding, ended by a write to the binding), which is what lets a loop over
`xs.len` prove its own accesses; the compiler's own manifest is **3,040 rows —
1,136 discharged, 1,177 open, 722 `unencoded`**, and the 722 are one shape: a
`List` is address-taken by every `push`, so DEF-14 leaves it no length term
(E-4 re-homes that residue to 1.6's leg B, where a frame condition is the
tool). DEF-82 is fixed — a `defer` body's guard is ONE row and one trap PER
COPY the emitter writes. Four defects of the step's own making were found by
its instruments, not by reading: an element WRITE had a guard and no row (71
unaccounted traps), the range slice asked for its elision under the wrong site
key (the row read `discharged` while the guard stayed), the length symbol's
sentinel was 0 where `sym_new` hands out ids from 0 (DEF-69's rule again), and
a shared symbol's range axiom sat in the region of its first read, so two
functions lost discharged rows the gate then refused. The old 1.5.8 was PLANNED 2026-09-18 by
`nitpick-compiler_s11` as those four (`meta/roadmap/done/1.5/1.5.8.md` §0),
because planning MEASURED first and found three of its five kinds standing on
uncontrolled stops: DEF-58 (a float's `=>!` cast to an integer was LLVM poison
— one program exits 3 at `-O0` and 9 after `opt -O2`), DEF-59 (a stack
overflow killed with no `failsafe`; `npkc` under `ulimit -s 2048` exits 139),
DEF-60 (a spawned thread's one guard page could be jumped by a frame larger
than a page — 151 of the compiler's own). The user ratified S-84…S-87 in one
sentence ("go with all four") as D-304 (`decreases`/`unbounded`), D-305 (the
split-stack prologue on every emitted function, `StackExhausted`), D-306
(`CastRange`) and D-307 (`MachineFault`, the last net).** **1.5.8 IS COMPLETE
(2026-09-19; ten landings — steps 0, 1, 1b, 2, 2b, 2c, 3, 3b, 3c and 4 — each a
cumulative prefix under a full harness, D-228).** A float's `=>!` cast to an integer
traps `CastRange` (armed where one exists). A joined thread's stack is
unmapped. **Every function the compiler emits checks its frame against the
thread's limit word at `%fs:0x70` (`"split-stack"`, one text: `ll_fn_open`),
and every stack is the FLOOR's**: 8 MiB for main, 2 MiB for a thread and 1 MiB
for `failsafe`, whatever the shell's `ulimit -s` says. Each has a guard, a
signal stack and a 64 KiB reserve. `StackExhausted` is armed in every program.
The floor's own frames never check, and the `floor-stack-reserve` belt proves
they fit (1,712 of 16,384 bytes). Since step 4's one-hop snapshot refresh the
builder, and every tool it compiles, checks its own stack too. The syscall census reads `module asm` (2b, DEF-64), and the
explorer can HOLD a thread at a site until another passes it (`hold-at:`, 2c,
DEF-67): a kept seed goes stale when a step is added before its window, so
DEF-57's regression is now a held control, `frozen-traps.ctl`. **The last
net (step 3, D-307):** SIGSEGV, SIGBUS, SIGILL and SIGFPE enter the trap route
as `MachineFault` (armed in every program) on the thread's signal stack, with
SA_NODEFER so that a fault inside a fault's `failsafe` exits 70. SIGPIPE is
caught, so a write to a pipe with no reader answers EPIPE (DEF-68: it killed
the process, exit 141). A program faults the CPU only through JIT code
(`wildx`), which is how the tests do it. A spawned thread's trampoline block and
executor come from per-slot POOLS reborn for each thread in the slot (3b,
DEF-66): the join unmaps the stack and closes the epoll set before it retires
the slot. A standard descriptor closed at startup is opened onto `/dev/null`
before `main` (3c, DEF-69: a data file opened next became descriptor 2 and
took every stderr write), and the reactor's "none" is −1 at every site — 0 is
a descriptor once a program closes its stdin. The plan's execution record says
what each step found.
**1.5.7 (D-212's schedule-exploration harness) IS COMPLETE (2026-09-18;
`meta/roadmap/done/1.5/1.5.7.md`; eight landings, steps 0–7, each a cumulative
prefix under a full harness, D-228; seats s8 then s10)**: the REAL floor and
each concurrency test's own IR transformed, every synchronization step a
point of a seeded PCT schedule, the blocking syscalls virtual, quiescence
oracles, the spec's caller hypotheses executed, fourteen negative controls,
and the explorer's first floor find, DEF-57 (VERIFICATION_REFERENCE §10 is
the whole of it; TCB.md §5's seventeenth acceptance says what it does not
cover). The record, step by step: planned and measured by s7 with a throw-away prototype, approved
by the user 2026-09-17 in one sentence (S-77…S-83 → D-297…D-303), and its
**step 0 LANDED 2026-09-18** (`meta/roadmap/done/1.5/1.5.7.md`): the ONE
transformer (`npkg/explore.npk`; `tools/explored.npk` is the harness's
snapshot-built entry — a program cannot hold two modules named `explore`) puts
a scheduling point before each of the floor's 78 atomic step lines and routes
each of its 63 `@npk_sys6(` calls to the shim, and the TOTALITY BELT
(`explore-step-escapes`) counts in both runners — the harness has no
transformer of its own, by design (X-9: the property is an equality of two
counts); both self-checks hold the transformer's output to one literal over a
planted floor; the planning prototype's C shim is kept OUTSIDE every gate as
the IR shim's behavioural reference (D-303). **Step 1 LANDED 2026-09-18**:
`runtime/explore/npkx.ll`, the shim in hand-written IR (the baton, virtual
futex/epoll/clock, PCT with ordered bands and the fairness bound, exact
replay by seed), held to the C reference SCHEDULE HASH FOR SCHEDULE HASH on
all 30 signal-free programs × 20 seeds under a twelve-process load
(`meta/roadmap/done/1.5/tools/explore_prototype/hashcmp.sh`) — and the port found
X-13: the floor's `mmap` trims made a step count depend on an ADDRESS, so
both shims now place anonymous mappings at a 64 KiB-aligned bump pointer
with `MAP_FIXED_NOREPLACE`; the `explore` stage in both runners (a
`[[test]]` entry: per unit the measuring run, 1,000 seeds, the first seed
replayed to the same hash, PCT's bound printed); `// explore: 1000` on 30
`// stress:` programs, `// explore: no <reason>` on 14 (ten real-child, four
trap-route until step 2), the marker belt `explore-unmarked` in both
runners. **Step 2 LANDED 2026-09-18**: the signals are virtual
(`rt_sigaction` remembered, `tgkill` marks its target and makes a blocked one
runnable, the handler runs in the target's context at its next grant), so the
trap route is explored like everything else — the four trap-route programs
say `// explore: 1000` and agree with the C reference hash for hash on 20
seeds each; 34 of 34 explorable programs do. **Step 3 LANDED 2026-09-18**:
the quiescence oracles are live in every explored run (D-301: `LOST-WAKE`
and `LOST-FUTEX-WAKE` are red verdicts read off the REAL executor state at
quiescence — a lost wakeup here is lateness an exit code cannot see; the
shim's three struct offsets are held to the floor's type lines by
`explore-oracle-offsets` in both runners), and the CONTROL MECHANISM under
`runtime/explore/controls/` (a `.ctl` plants a defect by one exact-line
substitution and names the verdict the explorer must reach within N seeds —
`explore-control-blind` otherwise; `store-release` and `no-rouse` landed, both
found at seed 1; 680 clean runs, no false positive). **Step 4 LANDED
2026-09-18: the nineteen model controls walked** — eleven are `.ctl`s (ten
new), five measured NOT A FLOOR BUG (the stamp-as-deadline in
`npk_sl_earliest` is a second line of defence the `park-unpark` model has no
deadline to represent; a handle is minted by the open that publishes its slot
and a reclaim is the scope exit, once), one NOT OBSERVABLE (a write into freed
memory: the model's), two NOT EXPLORABLE (the driver pair is real-child); the
grammar grew several `old:`/`new:` pairs, the program's-answer verdicts
`wrong-exit`/`exit N`, the LATENESS verdict `late N` (the shim prints `vrun=`;
X-17), and DIRECTED controls (`preempt-at:` names a site of the floor at which
the shim demotes the arriving thread — a change point at a place, X-15 — for
the three windows one step wide that blind PCT cannot land on); and the walk
found and fixed THREE DEFECTS OF THE SHIM, each mirrored in the C reference
and re-swept (38 of 38 explorable programs agree hash for hash): a wait whose
deadline had already passed slept virtually until quiescence where the kernel
returns at once (X-14 — 3 false LOST-WAKEs in 100 seeds), the fixed fairness
bound resonated with a period-three thread so it never rested holding the lock
a control needed (X-16 — jittered by a seeded draw), and the measuring run's
steps were read off the wrong line when the defect fired at seed 0. Four
explored programs were written for shapes none had (`io_ready_declined`,
`shared_arena_race`, `trap_one_failsafe`, `reactor_arm_race`). **And the step's
own harness found the explorer's FIRST FLOOR DEFECT, DEF-57, fixed in the same
landing**: `npk_trap` publishes the frozen flag before it claims the failsafe
holder, and `npk_step`'s `frozen:` block, older than D-291, answered the flag
by trapping `Unreachable` ITSELF. So an executor that merely watched a trap
could win the holder in that two-instruction window, and `failsafe` ran with
the wrong error (seed 371 of `trap_one_failsafe`; 40 stress runs never reached
it). A watcher parks now and the holder keeps the re-entry exit 70. The
`trap-route` model, blind for want of an error code, gained one: `wrong-error`
and `holder-parks`, each with a control. The floor's bytes moved. **Step 5 LANDED
2026-09-18: the spec's caller hypotheses EXECUTED (D-302)**: `npkg/explore_req.npk` writes one entry checker per section of
`runtime/npkrt.spec` that has rows and a `requires`/`objects`/`views` clause —
31 checkers, 236 of 240 hypotheses evaluated over the entry state in i128 at
every call of every explored schedule, the 4 sorted-table `requires` that name
the free symbol `j` LISTED BY NAME (the stage's output, TCB.md §4d's new
column); a false one is the shim's verdict `ASSUMPTION <symbol>: <clause>`, and
the SPEC control `unconditional-apartness.ctl` (X-18: `spec-old:`/`spec-new:`
pairs) plants 1.5.6's clause that 1.5.6c found false by reading and sees it on
`drop_string`'s 27th step. The D-303 sweep after it found a REAL RACE in both
shims (X-19): the kernel clears a thread's `CHILD_CLEARTID` word after its last
virtual step, and the joiner's read of the word decided a step count — a
thread's end is now settled before anyone else steps (the clone's ctid word
handed to `npkx_spawned`, the next baton holder waiting for the clear). Found
on the way: the harness's `define_headers` read one line, so three multi-line
defines had no header and `check_spec`'s free-symbol check skipped them in
silence. **Step 6 LANDED 2026-09-18: the program's own steps** (X-20, X-21): every
explored unit's emitted IR goes through the same transformer's PROGRAM MODE
before `llc` — a point before each atomic step of its defines (`atomic<T>`,
`atomic_from_ptr` lower inline, in the program), each `sys` call's
`@npk_sys6(` routed, sites numbered from 1,000,000 — under the counting belt in
both runners. The census found no explored program holding a program-level
atomic (an `atomic<T>` borrow cannot cross a spawn, D-180), and 23 `sys` calls
in seven programs that had run as real syscalls with no point before them.
`atomic_threads.npk` counts to 400 from two threads through `atomic_from_ptr`
over `wild` storage, and the PROGRAM control `atomic-lost-update.ctl`
(`program-old:`/`program-new:` pairs over the named program's SOURCE) splits
its `fetch_add` into a `load` and a `store`. Found at seed 1 (55 of 100);
without the program's points, 0 of 100. **Step 7 LANDED 2026-09-18: the docs and the
close** — VERIFICATION_REFERENCE §10 (the explorer, whole), TCB.md (the explorer
among the standing instruments; §5's thirteenth and sixteenth acceptances
narrowed by dated notes; a seventeenth for what it does not cover: weak memory,
real-child programs, the trampoline, schedules beyond the seeds, liveness), the
landing notes of D-212 and D-298…D-303, E-3's instrument half closed. **1.5.8 is not a close-out footnote**: VERIFICATION_REFERENCE
§7b's catalogue assigns it the five obligation kinds that have NO rows in
`nitpick.obligations` — `overflow`, `bounds`, `cast-range` (guards to elide)
and `terminate`, `stack-depth` (none) — D-210 §4 commits cycle 1.5 to proving
the overflow traps away, and 1.6's leg B lists those rows as evidence arriving
from 1.5. Sized at `b7d60dc`: 2,248 of the 2,503 guard-trap call sites in the
compiler's own emission (90%) are `IntOverflow`, against the 273 guards the
verified build elides today (202 manifest rows). `terminate` has no surface syntax (`decreases`
appears nowhere in the lexer, the parser, the grammar or a spec), so 1.5.8's
planning opens with a language question — the user's — and the library
listener is owed the keyword-or-refusal answer, named with its code, before a
re-pin could surprise it. **Read the map, the catalogue and the manifest, not
a summary of them** — a hand-written summary of a held fact has no check on it.
**The decisions this cycle settled: D-224…D-233.** `exit` is process exit in
every body (D-224); declared-uninitialised managed storage holds its canonical
vacant value (D-225 — `OwnedFd`'s vacant is −1, not zero); the index type
follows the count (D-226); **a memoised layout fact is never read before it is
computed** (D-227 — the query ensures, the caller does not remember, and
`_recorded` is the explicit opt-out); the orchestration rules are normative
(D-228); the diagnostic walk is generic and borrowing and prints span-sorted
(D-229); D-044's flag types get implemented as one `TY_FLAGS` kind (D-230);
the sub-byte integer widths are struck and the wide ladder pinned (D-231);
and **D-233 replaced Astrée with LLVM-native analyzers over our own emitted
IR**, striking the C emitter (D-232, superseded).
**Four defects came out of D-227's neighbourhood, none found by a test of the
thing that broke**: `tt_grow` never zeroed two of its four side arrays (latent
since 1.2.5); the three memoised bits were read before computation, disabling
TYPE-046, D-215 and `gives` wherever the window was open; a payload-less enum's
bits were never written at all; and two live TYPE-046 violations in `src/`
itself, where a `PlaceVal` owning a string was copied into a consuming
parameter. All four share one root — **a fact that is ABSENT and a fact that is
FALSE were spelled the same way**, which is what the new `absent-fact` harness
stage now makes impossible to reintroduce.

**A concurrency test runs 40 times, not once.** `// stress: N` in a program makes
the harness require the same exit code every run. Two serious defects hid behind
single green runs — `npk_exit` calling `exit` rather than `exit_group`, so a
threaded program's status was whichever thread finished last, and a channel wake
landing between registering and sleeping, which the sleeper-push then erased.
Neither reproduced in fewer than about twenty runs.

`tools/check.npk` still runs the whole frontend over a program and exits 0 on a
clean one — Phase A's checker, now one thin `main` over the shared pipeline.

It refuses a program that returns a borrow, launders one through a call, reads an
unassigned binding, writes a `fixed` binding twice, uses a moved-from binding,
double-frees, takes the address of a temporary, leaves a `pick` arm uncovered, lets
`(*)` swallow ERR, reads a tainted `Result.value`, acquires a lock downward, expands
a macro without bound, names something in a macro body its defining scope does not
have, splices a body where it does not fit, evaluates a `comptime` that never
finishes, derives a trait that cannot be derived, writes a struct literal that
omits a field, discards a `Result` with a bare `f();`, lets a `never fails`
function `fail`/`relay`, drops a trait's `never fails` in an impl, or `fail`s
inside a `defer` (D-163, 1.1.0) — each with its own code, its own span, and a
case in one of the six rejection suites showing it refuse.

**Two dozen whole-tree checks run on every harness invocation** and each found
something on its first run: `check_kinds_typed` (every expression kind is typed),
`check_kinds_lowered_or_refused` (every Expr/Stmt/Decl kind lowers, refuses by
name, or is confessed — plus the LIVE-1 carrier accessors stay read; its first
run found five expression kinds dying as internal defects), `check_codes_tested`
(every code has a case, or a stated reason), `check_codes_centralised` (no code
literal outside a `*_codes.npk`), `check_ll_types_agree` (`// ll:` markers match
the lowering), `check_runtime_sigs_agree` (npkrt.ll vs seed vs
the ir_runtime table BUILTIN_REFERENCE now generates — the spec in the loop
since 1.4.2, and its derived-inner leg found dead on the day it was fixed),
`check_builtin_sig_texts` (every type text the generated signature table hands
the checker is one `builtin_text_type` can intern, and no arm of it is dead —
it caught a half-done `wildx any->` on its first run), and
`check_zero_dependency` (the undefined-symbol scan). A ninth,
`check_decisions_current`, REPORTS rather than fails: stale decision-log
candidates print on every full run for the doc-sync pass. They diff the compiler
against the thing that describes it, which is how cycle 0.6 found every one of
its holes — none was found by a test.

### Building and testing

```
python3 bootstrap/harness/harness.py                    # everything: about THREE HOURS since 1.5.8b step 3 (below)
python3 bootstrap/harness/harness.py --only type_stmt   # one test, ~1 minute
```

It assembles the committed snapshot (`bootstrap/seed/stage1.ll`) into the
BUILDER, has the builder compile `src/npkc.npk` into the compiler under test
(D-205 — the Python seed in `bootstrap/generator/` retired as a builder at
1.4.6), compiles each suite with that compiler, links against
`runtime/npkrt.ll` via `llc` and `ld.lld`, runs the result, and compares the
exit code. It also feeds every source through the **real** parser
(`tools/parse_check.npk`) and re-checks that every AST node kind is reachable.

**`npkg` (1.4.8, D-206) is the permanent runner, and it runs beside the
harness until `meta/SWITCH.md`.** Build it with the compiler under test and
run it from the tree root — it finds `nitpick.toml` by walking up, builds
into `build/` (gitignored), and `npkg build` leaves `build/npkc`:

```
python3 bootstrap/harness/quickemit.py --keep npkg/main.npk   # builds .internal/quickemit/p_main_npk
.internal/quickemit/p_main_npk build                          # the ladder: floor, builder, src/ -> build/npkc
.internal/quickemit/p_main_npk test                           # every suite, ~2.5 hours (the verify sweep); --only SUBSTR to iterate
.internal/quickemit/p_main_npk test --selfcheck               # the runner self-check alone (§7.1)
.internal/quickemit/p_main_npk test --verdicts out.txt        # plus one verdict line per unit (the parity diff's input)
```

**A full run takes about three hours, and most of it prints nothing** (corrected
2026-09-23; this file said "~20 minutes" and "~25 minutes" from 1.4.8 until then):
1.5.8b took the compiler's manifest from 368 rows to more than 3,000, so the
verify sweep decides an order of magnitude more per program — about 47 seconds
each over 115 programs, twice (the harness's own `verify` stage and the `parity`
stage's `npkg test`), and the `parity` stage prints nothing until it ends. A log
quiet for an hour is normal; check the process's live children, not the log.
The harness's `parity` stage does all of this on every full run and diffs the
verdicts, so a green harness already says the two runners agree; `npkg test`
by hand is for iterating on `npkg` itself, and its `--only` skips the sweeps
exactly as the harness's does.

For the middle of a subcycle, where the question is "does this one rule fire on
this one file", there is a faster loop that builds the checker once:

```
python3 bootstrap/harness/quickcheck.py tests/analysis/rejection/borrows.npk
```

And the same loop for the backend — build `npkc` once, then compile, link and
RUN programs with it, printing the exit code (or the refusal, or llc's first
error; `--ir` prints the IR too, `--keep` leaves the `.ll`/`.o`/binary behind):

```
python3 bootstrap/harness/quickemit.py tests/backend/programs/dyn_slots.npk
```

**The verification leg (1.5.0, D-218/D-219).** The compiler emits every
function's obligations and reads a manifest of verdicts; `npkg` owns z3:

```
.internal/quickemit/p_main_npk verify              # the VERIFIED build: obligations decided by the pinned z3, held to nitpick.obligations, guards elided, the verified compiler rebuilt from itself
.internal/quickemit/p_main_npk verify --record     # write nitpick.obligations from this run -- a deliberate re-baseline, committed with the change that moved it
.internal/quickemit/p_main_npk verify --explain    # plus build/verify/explain.txt: a model per open row, a reason per budget row, a core per discharged one
.internal/quickemit/npkc file.npk --obligations D  # the compiler's half by hand: D/NNNN.smt2, index.txt, rows.txt
z3 smt.random_seed=0 sat.random_seed=0 rlimit=20000000 lp.dio=false -smt2 D/0001.smt2   # one file, the profile spelled out
```

The harness's `verify` stage does the same over `tests/verify/` (each file
names its rows with `// expect-obligation: KIND VERDICT N`, exactly) and over
the compiler itself, and its `parity` stage byte-compares `npkg`'s verified
compiler with its own. A verdict that moves is a red run, never a rebaseline
(D-040): run `--record` only in the commit that changes what is proven.

Since 1.5.2 the leg carries `limit<Rules>`: every write point of a limited
binding is a `limit` row (its rule over the new value, the rule a hypothesis
on every later version — so a division by a limited divisor discharges), every
direct call of a sync callee with limited parameters a `limit-subsume` row,
and the elided build removes a discharged write point's check for one
`llvm.assume` over the rule's range clauses and lets a discharged call name
the callee's `.body` past its checked entry. A limited binding has no address
(TYPE-063) — pass it by value — and a `limit` refuses where no write point
exists (TYPE-064). Two ways to read a row by hand: `.internal/quickemit/npkc
FILE --obligations D` then z3 on `D/NNNN.smt2`, and a hand-written manifest
in `nitpick.obligations`'s shape fed to `--elide` (outside every gate, P-27).

**The cost leg (1.5.1b step 0).** `tests/cost/*.toml` are units judged by the
allocator's numbers under `NPK_HEAP_STATS` (BUILD_REFERENCE §7.1's `cost`
row): DEF-1's three recipes at N and 4N, the two temporaries probes, and the
compiler's own build. A unit marked `expect = "fail"` must FAIL its bound until
the commit its `until` names lands; the day it holds, the unit fails until
those two lines are removed in the same change. By hand:

```
NPK_HEAP_STATS=1 .internal/quickemit/npkc file.npk > /dev/null   # the line is the last on stderr
```

Neither is a substitute for the harness; both skip every whole-suite check.
`quickcheck` watches nothing — rebuild it after every edit to `src/`, since a
stale binary answering an old question is the failure mode to expect;
`quickemit` rebuilds itself when anything under `src/`, `lib/` or `bootstrap/`
is newer than its cached `npkc`.

Three things to know before you use it:

- **`--only` is for iterating, never for concluding.** It skips every whole-suite
  check — node-kind reachability, the real-parser sweep, module rejection — and
  its output says so twice. **Nothing is committed on the strength of a filtered
  run**; do a full one first.
- **A test's expectation lives inside the test**, as an `exit` code per case. A
  failure reports `exited N, expected 0`; find `exit Ni32` in the file to see
  which case broke.
- **Every test builds the whole frontend through the seed**, which is why even one
  test costs about a minute. That is the floor, not something to optimise around.

Five more, each of which cost a debugging cycle in 1.4 (the executor HANDOFF
that carried them retired at the cycle close):

- **Strings are move-only owners** (TYPE-046): no binding-to-binding copies;
  pass as plain arguments freely; consume with `move T:p`; in emitter code
  rebuild a name per use rather than holding one binding across lines.
- **The walkers-total instrument refuses a half-done type-kind change**
  (`check_type_walkers_total`, with excuse tables in `harness.py`). When it
  fires, complete the change or update the excuse WITH A TRUE REASON — never
  silence it.
- **A backend fix does not reach the tools until the snapshot carries it.**
  The harness compiles `tools/` with the SNAPSHOT, so a checker rule in
  `src/frontend/` is in the built tools at once and an emitter fix is not
  (`bootstrap/seed/README.md`, the mirror of D-205).
- **`src/`'s own code is checked like everyone else's since the switch** —
  overflow traps, the escape analysis, move-only owners. A trap inside the
  compiler is a `src/` bug, not a test bug; `gdb -ex "break npk_trap" -ex run
  -ex bt` on the built `npkc` names it in one shot.
- **Never rewrite `done/` archives or settled DECISIONS text** — annotate
  with dated notes (the D-085/D-202 pattern).
- **Both runners stand under `nitpick.toml`'s `[limits] nofile` (1024; 1.5.1b
  step 5).** A descriptor-exhaustion proof means something only under one
  known soft `RLIMIT_NOFILE`; this machine's session sets 1,048,576, and every
  such proof in the suite passed against a leaking build until the runners
  lowered their own limit. Running a program BY HAND runs it under the
  session's limit — `(ulimit -n 1024; ./prog)` is the spelling that matches
  the runners, and `fd_ceiling.npk` exits 5 without it.
- **`pass h.n` over a copyable field no longer clears `h`'s drop flag** (DEF-8,
  since 1.2.3): the clear is gated on the passed value's type dropping.
- **`wild_release_all()` is followed by `exit`, or TYPE-062** (1.5.1b step 5):
  nothing may run after the release, and a `main` that released and then
  RETURNED ran its drops over unmapped memory the day `List<T>` began to own.
  Measure after the release inside `exit`'s operand.
- **A `move` or `pass` out of a FIELD leaves the type's vacant value** (S-26):
  the aggregate stays live and drops its siblings; a vacant `List` grows from
  zero.
- **A derived body reaches every member through the trait it derives** (D-258,
  1.5.2b): a scalar through the PRELUDE's generated impl, a named type or a
  parameter through its own, so a generic subject's derive is
  `impl:<T: Eq>:Box<T>:Eq` and `Box<Point>.eq` needs `Point: Eq` — asked at the
  CALL (D-256), never at the instantiation. A family impl's bound is enforced
  where the impl is used, its parameters bound by UNIFYING the target pattern
  (`Pair<int32, T>` is a legal family target). A derived diagnostic reports at
  the derive's declaration (D-259); a `<derived-` path in any runner's output is
  a compiler defect and fails the unit. A program's own `impl:int32:Ord` is
  TYPE-013 against the prelude's; `impl:bool:Ord` is still admitted (S-37).
- **A builtin kind's fixed method set lives in the checker AND the emitter**
  (1.5.2b step 2): both intercepts are gated on the type's own names through
  one predicate in `types.npk`, and every other name goes to the impl table.
  A new builtin kind with methods owes the same gate on both sides.
- **A generic enum's payload-less variant needs the annotation** (D-261,
  1.5.2c): `Opt<int32>:o = Opt.None;` — a bare `Opt.None` where nothing is
  expected has nothing to infer from and is TYPE-022 naming `T`. A constructor
  infers from its payload (`pick (Opt.Some(big))` is `Opt<int64>`), an
  unsuffixed literal teaches nothing. A `pick` over an `Optional` spells `??`:
  `pick (o ?? Ordering.Equal) { … }` (D-260, TYPE-065).
- **A rule written for one spelling of a construct is owed to the other.** The
  `pick` expression form typed no arm binding from 1.0.9c until 1.5.2c while
  the statement form did (`type_pick_rules` is the one function now). When a
  construct has two spellings — statement and expression `pick`, `=>`/`=>!`,
  `?!`/`?|` — grep for the twin before calling a rule landed.
- **A prelude function is in an emitted module because something referenced
  it** (D-262, 1.5.2d): an assertion on a prelude symbol in emitted IR needs a
  use in the program, and a belt over emitted IR must know WHICH compiler
  emitted it — the committed snapshot carries an emitter change only after a
  refresh, so the tools, `npkg build`'s `npkc.ll` and the runner self-check's
  cases (compiled with the builder by design) are not asked. An instrument that
  counts sites (`llvm.assume` per discharged row) counts only the functions the
  emission holds.
- **A `T` is move-only in a generic body** (D-264, 1.5.2f): store a by-value
  `T:v` only as `move T:v` + `move(v)`; read an element out as
  `move(s.items[i])`; a lending `pick` binds a `T` payload as a VIEW (D-266).
  `move(x)`
  spends `x` at a scalar too (MOVE-001 on a later read of `x`).
- **The ladder prints its digests, and the emission's is the one that
  travels** (D-265, 1.5.2g): `npkg build` ends with one `sha256` line per
  intermediate; quote `build/npkc.ll`'s to another machine, since the object's
  and the binary's belong to the toolchain build. An emitted `.ll`'s BYTE
  COUNT is path-dependent (D-236's root-relative site paths), so a size
  compared across directories is the object's.
- **A lending `pick`'s binding is a VIEW, and a view has no address** (D-266,
  1.5.2h): read it, pass it by value, `give` it if copyable. A method that
  takes `Self->` on a payload needs the consuming form `pick (move(v))` or a
  by-value receiver; the selector cannot be written inside an arm that binds
  (TYPE-067) — assign after the `pick`, or bind `_`; a payload of a stateful
  kind (an arena, a lock, an atomic, a channel, a `dyn`) cannot be viewed at
  all. `drop` over a refused operand stays silent (D-240), as `raw` has since
  1.5.2b — a second sentence there is the checker's defect, not the program's.
- **A primitive's empty case is the runtime's to handle, and its neighbour is
  the model** (DEF-25, 1.5.2i): `string_slice` allocated nothing for an empty
  result since D-186 while `string_concat` beside it allocated a real block
  and returned cap 0. When one floor primitive handles an edge, read its
  siblings for the same edge; a leak of managed storage is invisible to D-151
  (which counts `wild` blocks) and shows only in `NPK_HEAP_STATS`.
- **`exit` runs joins and defers and no drops** (D-183's amendment, 1.4.4): an
  owning local of `main` is never dropped by a program that exits, so a
  program test with one in `main` measures the storage's REGIME under D-151,
  not the drop (D-263 moved the prelude's `List` buffer to managed storage for
  exactly that). Put a drop's behaviour under test in a function that RETURNS.
- **A contract is a check in every build and a row in the manifest** (1.5.3):
  `old(...)` names the enclosing function's PARAMETERS only (D-243 read
  literally — a local has no value at entry), `result` is the success value at
  the seam, a contract's call is `raw f(…)` of a `pure never fails` function
  (an uninterpreted function to the solver: `sq(3)` has no value, so a row
  over it is `open` unless the callee's `ensures` says enough). A function
  with a `requires` SPLITS like one with a limited parameter (`.body` plus the
  checked entry, which calls `<sym>.req`), so a belt over its symbols counts
  the predicate; a coroutine's check runs at state 0 and its call-site rows
  are `held`. `failsafe`'s `exit` must be positive: a literal that is not is
  REACH-004, a computed one traps to 70. Clauses repeat their keyword
  (`requires a requires b`), never a comma; `use` is a keyword, so no function
  is named `use`.
- **A `prove` is refused by the VERIFIED build only** (1.5.4, S-45/D-269): the plain
  build lowers it to nothing; `npkc --elide`, and so `npkg verify`, refuses an
  undischarged one with `NITPICK-VERIFY-001` naming the row's verdict. A
  discharged `prove` is a hypothesis for what follows (S-48). `assert_static`
  needs a proposition that folds (TYPE-069) and, in a `comptime` body, is
  evaluated per call — as is a `prove` there. A `bool` proposition always has
  at least an opaque term, so a `prove` row is `open`, never `unencoded`.
- **The counted loop's head is checked** (1.5.4, D-022): `loop(start, limit,
  step)`, `till(limit, step)`, a literal step positive — TYPE-068; a computed
  step is the `loop-step` row and keeps its compare. The compile-time
  evaluator agrees with the emitter on direction now (DEF-29), and `tests/`
  carried the evaluator's old two-argument vocabulary in three places.
- **The encoder is path-sensitive** (1.5.4): a branch condition is a fact in
  its arm, the versions after an `if` merge as `ite`, `$` and a range `for`'s
  binding are terms. A pattern's literal is encoded under the SELECTOR's type
  (the checker records no type on a pattern's own literal). Every `pick` is an
  `exhaustive checker` row a verify test must name — one per `pick`, both
  spellings, counted in CODE lines (a comment's `pick (` is not one).
- **A runner's negative self-check case runs under the SNAPSHOT** (1.4.6), so
  its construct must be refused identically by the snapshot and the compiler
  under test: a rule added this cycle cannot be it (1.5.4 step 4's first
  npkg self-check said `RUNG-001` where the new compiler said `TYPE-069`).
- **A module member is called `m.f(x)`, read `m.MAX`, imported `use m.*;`**
  (D-273, 1.5.4c): the call is a DIRECT call (no receiver — `raw m.f(x)` for
  a `never fails` member, `await m.run()` for an `async` one, `drop m.idle()`
  to spawn it), an alias (`use "./m.npk" as m;`) and a `pub mod` bound by a
  wildcard import carry their file's scope, and a private member refuses from
  either spelling (RESOLVE-003). An inline module's TYPES are named from
  outside only through `use m.{T};`/`use m.*;` — there is no qualified type
  path, for files either. A member-less `mod:name;` import carries the
  loaded file's scope (D-274, 1.5.4d) — one meaning with the alias. An
  error constant declared inside an inline module is the FILE's identity:
  its arm is `(file.Name)`, or `use m.{Name};` and `(Name)` (D-275); an
  arm over an `Error` whose identity no loaded file declares is RESOLVE-002
  naming the fix, and a literal, a global, a range, a bound destructure or
  `ERR:` over an `Error` is TYPE-007 — in `failsafe` and in any `pick (r.err)`.
  A `use`, a `pub use` or a `mod:name;` INSIDE an inline module binds and
  loads as at file level (D-276) — an inline module is sealed from its
  FILE's imports, so a name reaches one by being imported INTO it.
- **A `$$i`/`$$m` claim is a whole call argument or a pointer local's whole
  value, and `@` claims nothing** (D-286, 1.5.5): a claim's life is LEXICAL —
  end it with a block before touching its root again; a holder is used, not
  copied; a claim anywhere else is BORROW-014 and the fix is `@`. A read of
  the root under `$$m`, a write under `$$i`, a claim on storage a held `@`
  reaches, and a write through a shared holder are BORROW-013; two
  computed-index accesses are guarded at run time (`BorrowOverlap`) and
  proven away by the verified build (`disjoint`). Exclusivity is decided
  among accesses that spell the SAME ROOT — two pointer bindings that alias
  one storage are two roots.
- **A `fixed` binding has no address** (D-287, 1.5.5): `@`, `$$i`, `$$m` and
  a pointer-receiver call on one are TYPE-071, a `fixed` field included.
  Pass it by value or declare it plain.
- **The compiler's own `failsafe` cannot name a NEW prelude identity until a
  snapshot refresh** (D-205, found at 1.5.5 step 2): the builder is the
  snapshot, whose prelude is the old one, so `(BorrowOverlap)` in `npkc.npk`
  was RESOLVE-002 in the builder's eyes; an arm no identity reaches is
  accepted, so the arm waits for the refresh that carries the identity.
- **Measure before attributing a cost** (1.5.2d): the prelude's +0.75 s per
  program read as the price of D-257's 348 impls and was, to five sixths, the
  bindings analysis sizing its state by the whole program. `perf` cannot open
  events on this machine; `valgrind --tool=callgrind` on the checker over a
  floor-only probe answers in 30 s, `callgrind_annotate --inclusive=yes` reads
  it. **The same rule found DEF-92** (1.5.8c step 4b): the sweep's measures
  read as a 28% frontend cost and were field reads on generic instances
  reaching `tt_instance`, the third linear scan of the type table (85% of the
  checker's instructions); its index made the whole frontend 5.4x faster. When
  a cost appears with a change, profile over `npkg/main.npk` (callgrind, ~15
  min) and read the EXCLUSIVE top before reading the change.
- **A shift's amount is checked** (D-277, 1.5.4b): `x << n` is defined for
  `0 ≤ n < width(x)` only — a literal, a negated literal or a `fixed`
  constant outside it is TYPE-070 at the shift (both spellings), a computed
  amount traps `ShiftRange` (−4115), so a `failsafe` in a program with a
  computed shift must name `(ShiftRange)` — forty roots learned that at
  step 0, the compiler's own `types.npk` unit packing among them. The
  `shift-range` row's goal is a hypothesis after the site.
- **The bit-vector crossing stops at 64 bits** (D-280): `& | ^` and a
  non-numeral shift on a wider word are opaque to z3 — safe, unproven — and
  the Int forms (D-279) are what a wide-width row can use: a shift by a
  literal, a low-bits mask `& (2^j − 1)`, a one-bit mask, `~`. Write a wide
  mask as a numeral and the row decides at 2048 bits; write it as a
  computed word and the row is `open` by design. Two unknowns under `&`
  read by an inequality exhaust the budget even at 32 bits.
- **Every twisted compare and cast out is an `err-exit` row** (D-278,
  1.5.4b): the compiler's manifest grew 213 rows at step 2, most of them
  the prelude's generated impls, so new twisted-typed code in `src/` or the
  prelude moves the manifest (`--record` in the same commit, D-040). A row
  under an `is_err` test or a branch's bounds discharges; a bare compare of
  two opaque values is `open` and keeps its trap. A twisted division has no
  row — its zero divisor is ERR.
- **A proposition holds only where its evaluation does not trap** (DEF-33,
  1.5.4b step 2): a guard met inside a `requires`, an `ensures`, an
  `invariant`, a `Rules` clause or a `prove` — a shift's amount, a
  division's pair, a twisted operand — is part of the proposition, never a
  free hypothesis. So `requires (1i32 << n) != 0i32` proves `0 ≤ n < 32` at
  every call and in the body, and `$ != 0tbb32` as a rule holds only where
  `$` is not ERR. An encoder that pushes a guard's fact from inside a clause
  proves the clause from itself (measured, twice).
- **A float row needs bounds for tier 2** (D-281, 1.5.4b): floats never trap
  and carry no row of their own, but a `limit`, a contract or a `prove` over
  floats is decided — in QF_FP first, and where that exhausts the budget, by
  the Real-interval twin ONLY IF every float symbol in the cone is bounded
  below and above by comparisons against literals (a rule, a `requires`, a
  path condition) and the goal is a comparison. `prove(a + b > a)` is `open`
  (NaN, or `b ≤ 0`); `prove(a == a)` over a parameter is `open` (`fp.eq`'s
  NaN reading, tier 1 only); an unbounded `#sqrt` claim is `budget` and the
  verified build refuses the `prove`. A float `/` or `%` arms no
  `DivByZero` (DEF-37, step 4b) — a `failsafe` names the two only where an
  integer division exists.
- **`simd` lanes are terms, and a `simd` local's lanes are its versions**
  (D-282, 1.5.4b): a constructed or splat local, a lane read `v[2]`, `.len`,
  `.sum()` are known to the solver; a `simd` division is ONE `div-zero` row
  over all lanes, so one unconstrained lane keeps the whole guard. A
  `simd(...)` constructor needs an annotated context (`simd<int32, 4>:v =
  simd(k);`). **A `simd` integer lane's `+ - *` and an integer `.sum()` TRAP
  `IntOverflow`** as their scalars do (D-284, 1.5.4e), so a program with
  integer lanes names `(IntOverflow)`; `v op= w` on a `simd` lowers (DEF-39)
  and carries the guards.
- **The floor is specified now, and an `open` floor row is a red run**
  (D-288/D-289, 1.5.6): `runtime/npkrt.spec` says what each symbol does,
  `runtime/models/` says what its protocols do, and both are decided on
  every full run. Touching `runtime/npkrt.ll` means the spec may need to
  move with it — `npkg verify --floor-only` is the fast loop, and
  `.internal/quickemit/p_floorspec_npk <ABSOLUTE root> --emit DIR` writes
  the rows by hand (the root must be absolute: `Path` is). A spec clause the
  floor refutes fails by name; a `budget` row is residue and must carry a
  `(residue "…")` sentence saying why; a model's control that stops being
  `sat` is a model gone blind. After any floor, spec or model edit run
  `bootstrap/harness/tcb_floor.py --write`, which regenerates TCB.md's three
  marked regions, and re-record with `npkg verify --record` in the same
  commit (D-040).
- **A program's raise is `npk_raise`; a guard's trap is `npk_trap`** (D-285,
  1.5.4e): `?!` and `!!!` enter the trap route through the former, so a
  verified build's belts (which count `@npk_trap(i32 CODE)`) no longer
  mistake `?! DivByZero` for a guard — unwrapping with a system code is
  legal again — and `!!!` freezes, kills drivers and observes the re-entry
  rule (it called `failsafe` directly before, DEF-40). A compound shift
  through a field or element records its `shift-range` row (DEF-41).
- **Every alloca of the floor sits in its entry block and is fully DEFINED
  there** (the stack rule, 1.5.6b step 0; DEF-52, DEF-53): an alloca outside
  the entry block is dynamic (it moves the stack pointer each time it runs —
  D-173, which held every EMITTED module since 1.0.9a and had never been
  pointed at the hand-written floor), and a kernel-written buffer is zeroed
  first because how much the kernel writes is the kernel's promise (the raw
  `sched_getaffinity` writes 8 of the 128 bytes asked and RETURNS the count;
  the zero-fill is glibc's, and this runtime has none). Define by STORES
  where the function carries rows or runs hot — a `llvm.memset` cost
  `npk_hs_put_dec` 8 of its 23 rows — and by one whole-size memset for a cold
  buffer. `alloca-not-defined` is the belt, in both runners.
- **The kernel-effect table is ONE authority** (1.5.6b step 1; D-288 as
  amended): VERIFICATION_REFERENCE §9.2's `kernel-effects` region, generated
  into `npkg/floor_kernel.npk` and into the probe's claims. To make the floor
  issue a NEW syscall: the row first (effect, buffer, length, bound, the
  option set if the effect depends on an option), `gen_tables.py`, then a
  case in `tests/backend/programs/kernel_effects.npk` — the probe demands
  EQUALITY with what the running kernel writes, because an over-approximating
  row is sound and is what hid DEF-52. A `(boundary "…")` section emits NO
  rows whatever else it claims: moving a symbol from promised to proven means
  removing the sentence. A new section or model needs `tcb_floor.py --write`
  BEFORE `npkg verify --record` (the TCB belt runs before the solver) and
  again after.
- **A named block is not a modelled one** (1.5.6b step 2; E-3): the
  correspondence belt proves every atomic operation and syscall of a modelled
  symbol sits in a block some step NAMES; it cannot see a `next` that means
  something else, a named block's path with no transition (`park-unpark` had
  no step for the epoll wait's empty return), or a block named and not
  modelled at all. Read a model against the code CASE BY CASE. A row's budget
  margin is `(get-info :rlimit)` after each `(check-sat)` of the emitted file
  (the runners' push/pop mode; the rlimit is per check), and
  `meta/roadmap/done/1.5/tools/model_bfs.py` reads a model's whole reachable space
  with no bound — a measurement, outside the gates.
- **A builtin's name is the compiler's** (D-294, D-296; 1.5.6b steps 4, 4b):
  a module-level `func:` — or an `extern` METHOD, whose stub is one — named
  after a bare-name builtin is `NITPICK-RESOLVE-001`, and so is any CALLABLE
  binding of that name inside a function: a parameter, a local, a `for`
  binding or a `pick` pattern's binding whose TYPE is a function type
  (decided by the type at the sites that type a binding, never by a
  spelling — a pattern binding has no annotation). Methods are exempt, a
  function-typed FIELD is exempt (reached through its receiver), a binding
  of any other type is fine (`int64:read` cannot be called) — but a STRUCT
  PATTERN binds its field by name, so destructuring a function-typed field
  named after a builtin is refused. So ADDING a row to a `builtins` region
  reserves a name in every program: measure it across the tree, the
  libraries and the apps, and tell the library listener before it lands.
  `gen_tables.py` takes `--check` and nothing else — ANY other argument
  writes.
- **When a decision ENUMERATES the cases it knows and defaults the rest, the
  default must be the safe reading** (DEF-54, 1.5.6b step 4c): `emit_call`
  listed "a local" and "a parameter" as function VALUES and defaulted every
  other symbol to "a declared function" — so a function value bound by a
  `pick` pattern was called as a symbol that does not exist, and `llc`
  rejected the module. `emit_pipe`, beside it, had always defaulted to "a
  value". The same corner held two more twins that never got their rule
  (DEF-55: `raw o.f(x)` vs `raw (o.f)(x)`; DEF-56: the impl-versus-trait
  comparison had no function-type case, so a trait method with a callback
  could not be implemented) — `tt_func` interns a function type's parameter
  window by its START INDEX, so two spellings of one function type never
  share an id and every comparison of them must be structural.
- **The floor's models are read TWICE, and a probe with two cases in one
  file is read against the file** (D-295, 1.5.6b step 4d): the solver's
  bounded rows and an exhaustive explicit-state search
  (`floor.check_models_explicit`, `npkg/floor_explore.npk`) must both hold;
  a bad state reachable ANYWHERE, a control that cannot reach inside the
  bounds, or a step a variable's range blocks is a red run by name. Editing
  a model: `npkg verify --floor-only` runs both readings; TCB.md's generated
  paragraph prints each model's reachable states, so `tcb_floor.py --write`
  after a model edit. The twins must stay byte-identical — validate
  expressions whole and in one order, hold arithmetic operands under 2^31
  (plain integers trap here and Python's do not). And: a runner's fixture
  that CLAIMS a property is evidence of nothing until something decides it
  (the self-check's toy model was commented safe for a cycle and was unsafe
  in two steps).
- **The harness's `--only` reaches the UNIT tests alone** (found at 1.5.6b
  step 4): a new rejection file is iterated with its stage's tool by hand
  (`quickemit.py --keep tools/resolve_check.npk`, then the binary over the
  file) or with `quickcheck.py`, and concluded by the full run.
- **A floor spec clause is a HYPOTHESIS ABOUT THE CALLER, and TCB.md §4d says
  who keeps it** (1.5.6c; E-1, E-2): a section's `requires`, `(objects …)` and
  `(views …)` are assumed by its rows and PROVEN only at a translated call of
  a `(summary)` symbol; an untranslated floor caller and emitted code (the
  symbol is exported) are checked by nothing. So write a structural
  assumption's ARGUMENT beside the clause, and choose the form by what is
  TRUE for every legal caller: ranges that may coincide are `(views …)`
  (read-only, proven by the frame row) or a bare `requires`, never objects;
  an object the body touches on some paths alone is `(lo len apart-when
  COND)` — never an `ite` inside a range. A false hypothesis is invisible to
  a solver (the rows just hold vacuously for that caller): the two found were
  found by reading. `tcb_floor.py --write` writes FOUR regions now; a new
  CALL in the floor, or a `define` that gains or loses `internal`, moves §4d.
- **What the translator reads once is written once** (1.5.6c step 1): a
  second `objects`, `views`, `frame`, `ensures-trap`, `ensures-fresh`,
  `residue`, `boundary` or `summary` in a section, a loop sub-clause other
  than `invariant`/`unroll`/`inst`/`objects`, is refused by both runners —
  each was dropped in silence before, a claim written and never proven.
- **A solver cost is measured in the RUNNER'S OWN MODE** (1.5.6c step 0): one
  z3 process over the whole file under the profile, wall-clock BESIDE rlimit
  (`/usr/bin/time -f %es z3 smt.random_seed=0 sat.random_seed=0
  rlimit=20000000 lp.dio=false -smt2 FILE`). A row split out of its file, or a
  cost in rlimit alone, measured a spelling as free that took its file from
  202 s to 357 s — past the hang net (P-13; since D-297 `120 + 10·checks +
  60·B` seconds per file, B the file's rows the committed manifest records
  `budget`, and B = checks with no manifest to trust), which is a red run and
  no verdict. `npk_small_free` stood at 81% of the flat net (S-77) and stands
  at a third of its 610 s; a red on the net names the net and is READ, not
  re-run — the solver wedged (S-71's class) or the file is one the manifest
  does not justify.

- **A concurrency test is EXPLORED as well as stressed** (D-299, 1.5.7): every
  `// stress:` program says `// explore: N` or `// explore: no <reason>`
  (`explore-unmarked` otherwise), and a seed that once failed is kept as
  `// explore-seed: S`, run first forever. An explored failure prints its
  replay line; run it by hand under a BARE environment (`env -i NPKX_SEED=…
  NPKX_K=… NPKX_D=… NPKX_TRACE=1 ./prog < /dev/null` — the shim reads
  /proc/self/environ, first match wins), and `NPKX_TRACE=2` prints every step
  as `thread site` for diffing two schedules. After ANY change to the floor,
  the shim or the transformer, run D-303's alternating sweep
  (`meta/roadmap/done/1.5/tools/explore_prototype/hashcmp.sh`, the harness-built
  tool in OUT): it found X-13 and X-19, where the replay belt could not.
- **A control proves sight of its OWN defect only when the schedule that finds
  it goes through that defect** (DEF-57, 1.5.7 step 4): two directed controls
  had reached their verdicts through DEF-57's window, and went blind when it
  was fixed. Read a control's ROUTE, not only its verdict. A directed site
  cannot reorder two arrivals at ONE place: the later arrival is demoted below
  the earlier.
- **An executor that sees `@npk_frozen` is watching a trap, not making one**
  (DEF-57): it parks, as D-291's losers do, and only the failsafe holder
  re-enters (exit 70). Only a real fault may enter `npk_trap`; a watcher that
  did could win the holder, and `failsafe` then ran with the wrong error.
- **An `atomic<T>` cannot cross a spawn** (D-180 sanctions the locks and
  `shared_arena` only): threads share an atomic through `wild` storage and
  `atomic_from_ptr` (the Bridge's shape; `atomic_threads.npk`). A program's
  own atomic and `sys` steps are points of the explored schedule since 1.5.7
  step 6, their sites numbered from 1,000,000.
- **Every emitted function checks its stack; the floor's do not** (D-305,
  1.5.8 step 2). The prologue compares against the thread's limit word at
  `%fs:0x70`. A floor `define` never carries `"split-stack"`, and the
  `floor-stack-reserve` belt holds the floor's deepest chain within a quarter
  of the 64 KiB reserve, so a new call chain or a large alloca in the floor
  moves that measurement. `ulimit -s` sizes nothing: main has 8 MiB, a thread
  2 MiB and `failsafe` 1 MiB, each the floor's own mapping. A recursion test
  measures the floor's budget.
- **Every `failsafe` names `(StackExhausted)` and `(MachineFault)`** (D-305,
  D-307; the arms went in everywhere at 1.5.8 step 0). Every program can reach
  both, so REACH-002 refuses a new root's `failsafe` without them. **A float
  cast to an integer can trap** (D-306): `f =>! int32` is `CastRange` for NaN,
  an infinity or a truncation outside the target. A program with such a cast
  names `(CastRange)`, and REACH reads both cast spellings.
- **A sentinel is a value the domain never produces** (DEF-69, 1.5.8 step 3c).
  The reactor's "none" was 0, "safe because fd 0 is stdin", and nothing kept
  stdin open. It is −1 now, and the floor opens `/dev/null` onto any of 0, 1, 2
  closed at startup. Before choosing a "none", ask what makes the value
  impossible, and whether anything enforces that.
- **A floor global whose initial value is not zero gets a STATIC initializer,
  never a boot loop** (1.5.8 step 3c). An atomic store before `main` is a
  counted step in every explored run, and a boot loop of them moves every
  schedule (DEF-67's cause). An initializer also needs its named type's body
  ABOVE it in the file, or `llvm-as` reports "initializer with struct type has
  wrong # elements". A startup step the floor must add (a routed syscall)
  still moves schedules: re-run every explorer control after it.
- **A kept explorer seed goes stale with no signal** (DEF-67, 1.5.8 step 2c):
  one step added before a one-point window moves it out of every seed. The
  HOLD directive (`hold-at:`, `NPKX_HOLD1..4`) holds the first thread to
  arrive at a site until another passes it, which finds such a window without
  a seed. That is how `frozen-traps.ctl` regresses DEF-57 now.
- **A spawned thread's floor state is a pool entry, reborn per thread**
  (DEF-65, DEF-66; 1.5.8 steps 1b and 3b). The join unmaps the stack and closes
  the epoll set, and only then retires the registry slot. The next thread in
  the slot rewrites every word of the TLS block and the executor except the
  two reactor words: the epoll word stays as the join left it, and the eventfd
  is kept. So a new executor or TLS word must be written at the rebirth in
  `npk_thread_start`.
- **A declaration's flags live where its kind says, and a slot read as a flag
  word must BE one** (DEF-93, 1.5.8c step 3; the 1.4.8 global's lesson again):
  `decl_flags` answers `d.a` for most kinds, a global's and a rule's `c` high
  half. A `Rules` declaration's `pub` was never stored until this step, so its
  export was the parity of its subject type's node index -- true by coincidence
  for a cycle, false the day the prelude's AST moved. When a kind's `a` holds a
  type or a window, give it a flags home before its first `pub` is written, and
  test the visibility both ways (`rules_pub.npk`, `rules_private.npk`).
- **An expectation is a `//` comment of its OWN LINE** (1.5.8b step 4): both
  runners read `// expect-error: CODE` and `// expect-error-at: N` only from a
  comment that is the whole line, and a file in a rejection suite with no
  PARSED expectation is treated as a FIXTURE and skipped in silence. Writing the
  expectation after the code (`discard(x);  // expect-error-at:14 …`) therefore
  runs nothing: `wrap_kinds.npk` asserted sixteen refusals and ran zero, and only
  `check_codes_tested` noticed, because its code was new. Both runners now refuse
  a file that spells `expect-error` after code, by name.
- **A per-process temp path spells its pid FIXED-WIDTH** (`sys_pid_tag()`, DEF-137; 1.6.1d
  step 2): an explored program's schedule hash counts the allocator's steps, and a path one
  byte longer crosses a size class -- when the pid counter wrapped from seven digits to
  fewer between two replays of one seed, the replay belt called the explored build
  nondeterministic (landing 72's red). A red run is READ: this one's cause was a wrap
  between two replays, in a test program's own spelling, not in the landing's change.
- **A counted loop's operands are widened by their own signedness, `till` never infers a
  direction, and a bound is an integer that fits the `int64` counter** (DEF-129, DEF-130,
  DEF-135, DEF-136; 1.6.1d step 2): `loop(100u8, 200u8, 1u8)` counts up 100 times now
  (it counted DOWN 156), `till(-3i64, 1i64)` runs zero times at run time and in a
  `comptime` body alike (D-022's table), a `uint64` bound past 2^63 traps `IntOverflow`
  at the head (a program with one names the arm), a `tbb` bound holding ERR traps
  `TbbErr` (it ran the loop zero times in silence), and a 128-bit, float, bool or char
  bound is TYPE-068. The two guards are rows (`overflow`, `err-exit` at the operand).
- **A store through a pointer drops the old pointee when the pointer names MANAGED
  storage** (DEF-120, 1.6.1d step 3): `(<-p) = v` releases what `p` points at before
  the store, as `s.f = v` and `l[i] = v` do -- unless `p` is a `wild` binding,
  parameter or field, an `=>! wild` cast, `#ptr_add` or an allocator's result, where
  it drops nothing (manual storage holds no value until the author writes one). The
  reading is `wild_places.npk`'s, the escape analysis's own (D-223): a plain `T->` is a
  live managed value by contract, so a `wild` block cast to a PLAIN pointer by
  `=>! T->` and then stored through is dropped like any other -- spell the cast
  `=>! wild T->` for storage that holds no value yet.
- **A moved-out whole binding is VACANT** (DEF-120, 1.6.1d step 3): `move(x)` and `pass
  x` zero the slot and run the vacant fixup, as a field's or element's move has since
  D-254 -- a held `@x` then reads the vacant value, never the stale header. Every
  `move`/`pass` of an owning local moves the emission's text.
- **A consuming `pick`'s arm is a SCOPE, and its bindings drop at the arm's end**
  (DEF-121, 1.6.1d step 3): a read-only arm's payload is freed when the arm completes,
  an exit inside the arm (`pass`, `break`, `give`, `fall`) drops it there, a moved-out
  binding leaves with the move. In a coroutine the binding's flag is the frame byte at
  role `50 + ordinal` on the arm's statement (`scan_pick_binds` reserves it beside the
  slot at `7 + ordinal`; an alloca dies at a suspension inside the arm). The EXPRESSION
  form takes its consuming selector off the statement's temporaries as the statement
  form does (DEF-141): before it `give move(x)` handed out a body the statement freed.
- **A `where` guard decides and does not consume; a `fall` lands on an arm that binds
  nothing** (DEF-138, DEF-139, DEF-140; 1.6.1d step 3): `move(x)` of the arm's own
  binding inside its guard is `NITPICK-TYPE-089` (a failing guard hands the payload to
  the next arm, which bound and freed it a second time), `fall two;` into an arm whose
  pattern binds a name is `NITPICK-TYPE-090` (the target's pattern is never matched), and
  a `fall` to a label no arm of the `pick` carries is RESOLVE-002 (it was EMIT-002).
- **A consuming `pick`'s binding is a binding to the move rules** (DEF-118, 1.6.1d
  step 1): `pick (move(e)) { (Som(x)) { … } }` binds an owner; a second `move(x)` or a
  read after one is MOVE-001, and an arm inside a loop re-binds the name each
  iteration. The lending form's binding is a view and was never movable.
- **A write THROUGH a `$$i` holder is BORROW-013** (DEF-123, 1.6.1d step 1): `(<-p) = v`,
  `move(<-p)`, `@(<-p)` handed to a writer and a `Self->` call on `<-p` with `p = $$i x`, and
  a `$$i` claim (or a pointer holding one) handed to a callee whose summary writes through
  that position — a `dyn` method or a function value counts as writing. A reader spells
  `$$i` to a callee that writes nothing, or `@`; a writer claims `$$m`. And a `move(<-p)`
  inside a callee is a WRITE of the pointee in its summary (DEF-119): `@x` to such a callee
  while a view of `x` lives is BORROW-015.
- **A `for` binding ends with its loop, and a by-value receiver through a pointer is the
  pointee** (DEF-127, DEF-134; 1.6.1d step 1): the emitter releases the loop name at the
  loop's end (an outer name of the same spelling reads its own slot after it), and
  `q.peek()` with `Box->:q` and `peek = int64(Box:self)` loads the pointee before the call —
  the checker always admitted the shape; the emitter had passed the pointer's bits.
- **A rejection file's silent site is invisible to D-237's set rule** (1.6.1d step 1's
  sweep, S-113): `aliasing.npk` documented a shape its checker never reported for a cycle,
  because the code appeared elsewhere in the file. When a rule lands, sweep the tree under
  BOTH checkers and read every appeared site — a test file's own case may be among them.
- **ANY `src/` change moves `nitpick.obligations`, and the in-process checks cannot
  see it** (1.6.0 step 3b, found by a red harness): a row's hash is its problem
  text, and that text names types by their INTERNED IDS (`|npk.f.<TYPEID>.field|`,
  the sorts), allocated in interning order across the whole program — so a
  fourteen-line change in `reach.npk` that interned one new type renumbered every
  type interned after it and re-keyed 82 rows of `smt_encode`, `ir_expr` and
  `reach` with no verdict moving. Every `src/` change runs `npkg verify --record`
  in its commit and quotes `manifest_gate.py`'s two zeros (verdicts moved among
  shared rows, discharged counts fallen); "the manifest unchanged" is a
  measurement the harness's verify stage makes, never an assumption.
- **A change to emitted TEXT breaks the emitter's unit tests** (1.5.8 step 2):
  `tests/backend/ir_expr.npk`, `ir_func.npk` and `ir_stmt.npk` compare whole
  functions' text exactly, and step 2's `"split-stack"` broke eighteen of their
  expected `define` lines. Run `harness.py --only tests/backend/` before
  committing an emission change. And a harness run lists its failures ONLY in
  its summary: a run stopped early (the first step-2 run was) says nothing about
  the stages it passed.
- **A floor fix is measured against the floor before it** (every 1.5.8 step).
  Keep the test's object (`quickemit.py --keep`), assemble the previous
  step's `runtime/npkrt.ll` with the pinned flags, link the two with
  `ld.lld -static`, and run it (under `ulimit -n 1024`). A test that passes on
  both floors tests nothing about the fix. Where one program checks several
  things, check each case against its own defect: remove the earlier cases, or
  revert one site of the fix.

- **A `List` is indexed `l[i]`, and changed only through its operations**
  (D-313, D-314; 1.5.8b step 1b). `items` is `hidden` and `count`/`cap` are
  `sealed` outside the prelude: read `l.count`, index `l[i]` (bounds-checked
  against `count`), view `l[lo...hi]`, and change the list with `list_push`,
  `list_pop`, `list_truncate(@l, mark)`, `list_clear`, `list_insert`,
  `list_remove`, `list_swap_remove`. Behind a pointer the element is
  `(<-p)[i]` — `p[i]` on a pointer to an array, a slice or a `List` is
  TYPE-082, since it would index the POINTER. Assigning over an owning element
  drops the old value (a managed array's rule), and `list_clear`/
  `list_truncate` drop what they remove where a bare `count` write leaked it.
- **A field may carry its own `limit<Rules>`** (D-308, 1.5.8b step 6):
  `limit<r_level> int64:n;` in a struct body is a rule about THAT FIELD,
  checked after every write to it — a struct literal's value, an assignment
  through any path (a pointer's included), a compound — and a FACT at every
  read, which is what lets a row over `t.n + 1` decide where an opaque field
  read could not. The rule must hold of the field's VACANT value (TYPE-077,
  decided by the constant folder at the declaration: `0`, or `false`), and a
  rule the folder cannot decide there is refused, since nothing decides it
  later — so the subject is a plain integer, a `bool` or a `char`. A limited
  field has NO ADDRESS (TYPE-063), through a pointer to its struct as well.
  Arming is at the WRITES, so importing a struct and never writing it demands
  no `failsafe` arm.
- **A constant means what the run time means** (D-310, D-311; 1.5.8b step 2):
  a `+ - *` or negation of constants that does not fit its type is TYPE-076
  where it is written, `0u64 - 1u64` included — spell a `uint64` past 2^63−1
  with bit operations (`~0u64`, `(1u64 << 63u64) | k`). A fitting one is
  emitted as the constant, no guard. The ecosystem's FNV basis is
  `0xCBF5DAE484222325` ON PURPOSE (D-190), not the textbook `0xcbf29ce4…` —
  a comment claiming otherwise was stale, and changing the constant would move
  every error code and interface hash.
- **Every plain `+ - *` and negation has an `overflow` row** (D-309; 1.5.8b
  step 3): the guard's own site, one row over a `simd` operation's lanes, one
  row with N−1 traps for an integer `.sum()`, none for a node the folder
  writes as its constant. So new arithmetic in `src/`, `lib/` or the prelude
  moves `nitpick.obligations` (`--record` in the same commit, D-040), and a
  verify test names its own rows (`expect-obligation: overflow VERDICT N`):
  read each against its line, never copy a count. An `open` row keeps its
  guard, and D-309 writes no bound into the tree to close one. A proposition
  includes its arithmetic's range (DEF-33), so `prove(d * d + 1 > 0)` is
  `open` for an unbounded `d`.
- **A guard inside a clause check has a row per context the check runs in**
  (DEF-81, 1.5.8b step 3): a loop head's check runs at the entry, the back
  edge and each `continue`, and its guards are elided only when every such
  row is discharged; a guard inside a check that is not emitted is gone with
  it. `rows.txt` has TWELVE fields (the twelfth the clause context), both
  runners fail a malformed line by name (DEF-75), and both belts count
  GUARDS: an assume per elided guard's trap, a trap per retained one. The
  obligation table holds only functions the emission holds (D-262's rule,
  carried to the rows).
- **An explorer control's finding seed may spin for half a minute** (DEF-83,
  1.5.8b step 3): a `wrong-exit` control found through the shim's step budget
  runs to the budget by design, so a control seed's net is 300 s in both
  runners where a unit seed's is 60. A red control that says `hung` is READ:
  a finding seed killed by load reads exactly like a blind control.
- **Modular arithmetic is spelled `+% -% *%`** (D-312, 1.5.8b step 4): the
  wrapping family computes modulo 2^N and never traps, where `+ - *` trap
  (D-210) — a hash's mix step, a PRNG, a checksum. No guard, no obligation
  row, no `failsafe` arm, and the folder folds a constant wrap WITH the wrap
  where its trapping twin is TYPE-076. Plain integers and `simd` integer lanes
  only: every other kind is `NITPICK-TYPE-078` naming the kind. The compound
  forms are `+%= -%= *%=`. The prelude's `fnv_mix` uses `*%` since this step;
  `intern.npk`'s copy of the same step keeps the `uint128` spelling until a
  snapshot parses the operator (D-205), and the two agree to the bit.
- **`sealed`/`hidden` draw the private-member line** (D-313, D-314; 1.5.8b
  step 1): code outside the module that declares the struct may read a sealed
  field and may not write it (every write form — an assignment through any
  path, a compound, a struct literal, a move out of an owning one, `@`, `$$m`,
  a `Self->` receiver, a stateful operation; `$$i` reads), and may not touch a
  hidden one at all. The compiler-known headers (`ptr`/`len`/`cap` of string,
  cstring, slice, buffer; `OwnedFd.value`) are sealed in EVERY module.

- **An index's proof is a length TERM, and a length term belongs to a binding**
  (D-070's rows, 1.5.8b step 5): `xs.len` and `l.count` read off a BINDING are
  one symbol, so a loop written over the length proves the accesses inside it
  and the verified build drops their compares. A write to the binding ends the
  symbol; a name a pointer may write (DEF-14) never had one, which is why every
  `List` access — the container is address-taken by `push` — is an `unencoded`
  row that keeps its guard (722 of the compiler's own; E-4 sends that residue to
  1.6). So a `bounds` row discharges for a slice, a string, a buffer or a fixed
  array whose length is named and stable, and a `prove`-like bound on an index
  (a `limit<Rules>`) discharges too. A float's `=>!` cast to an integer carries
  a `cast-range` row over the OPERAND; the resulting integer stays opaque.
- **A guard inside a `defer` body is one row and several traps** (DEF-82,
  1.5.8b step 5): the walk sees the body once, the emitter writes it at every
  exit that runs it, and the row's `traps` field is one copy's times the copies.
  When adding a construct the emitter may write more than once, ask what the
  belts will count — they compare the IR's traps and assumes against the rows,
  and they fail closed.
- **Every length is at or below 2^47, and the allocator is why** (D-308 §7,
  1.5.8b step 6c): a request above 140,737,488,355,328 bytes is `HeapBadRequest`
  at every allocator entry — ONE unsigned compare in each of `npk_alloc_impl`,
  `npk_fs_alloc` and `npk_aalloc` (the aligned entry bypasses the core, so a
  single check would leave a hole), which is also the sign check. Exactly
  2^47 is legal and the kernel answers `HeapOom`. The four spellings of the
  number (the floor's three compares, the prelude's `ListLen`, `cast_bounds.npk`'s
  `len_ceiling`, the spec's promise on `npk_alloc_internal`) are held to one
  value by `check_len_ceiling_agree`. The allocator core is a `boundary`
  symbol: the ceiling is a PROMISE kept by one instruction and accepted at
  TCB.md §5 (10), never a row z3 decides — do not write "proven" of it.
- **`string_from_bytes` and `#wild_slice` are guarded, and arm `OutOfBounds`**
  (D-308 §7; K-18): the two producers of a length no allocation makes hold it
  to `[0, 2^47]` at the CALL — the emitter's guard keyed on the call node, the
  encoder's `bounds` row under the same key, REACH's arm through
  `builtin_caller_len` (the one predicate all three read). A narrow count is
  widened BY ITS SIGN first (DEF-87: an `int32` variable count reached `llc`
  as a type error on every compiler before 6c). A program calling either
  names `(OutOfBounds)`.
- **A `List`'s `count` and `cap` carry `limit<ListLen>`, so a program that
  pushes to one names `(LimitViolated)`** (D-308 §6, 1.5.8b step 6c): the
  prelude's own writes are checked (they can never fire — `count < cap <=
  2^47` by the ceiling — but REACH is syntactic, as it is for the program's
  own `+`), and 63 of the tree's roots learned the arm at 6c. `ListLen` is a
  prelude-owned name (D-239) and `pub`: a library container may carry it.
- **A prelude RULE's predicate is an item** (found at 6c): `emit_program.npk`
  brackets it as it brackets a function, or D-262's trim keeps
  `@"npk.prelude.ListLen"` in a program that never touches a `List` and
  `check_prelude_trimmed` fails on the first such program. Anything new the
  prelude emits per module — a predicate, a vtable, a helper — needs the same
  bracket.
- **The encoder's length fact must look THROUGH a pointer** (found at 6c):
  `b.cap` on a `buffer->` is auto-dereferenced (D-098), so the operand's type
  is the pointer's; a fact keyed on `has_len_term(operand type)` missed it and
  `b.cap + 1` stayed open. Any fact pushed at a member read should ask the
  pointee's kind when the base is a pointer.
- **A `while`/`when` loop states why it ends** (D-304, 1.5.8c step 1):
  `while (c) decreases n - i invariant P { … }` or `while (c) unbounded { … }`,
  the measure clause BEFORE `invariant`, exactly one of the two; `for`, `loop`
  and `till` take neither (TYPE-072). The measure is a plain integer
  (TYPE-073), checked at the top of the body each time the condition holds --
  below zero (signed) or not below the previous trip traps `DecreasesViolated`,
  so a program with a `decreases` names `(DecreasesViolated)`. A loop with NO
  clause is TYPE-072 since step 4 (the tree was swept at step 3). The `terminate`
  rows (entry, preservation, one per `continue`) discharge a counter loop's
  measure; a verify test names them (`expect-obligation: terminate discharged N`).
- **The frame roles 42, 43 and 44 are the measures'** (1.5.8c steps 1 and 4):
  a coroutine's `while`/`when` with a checked `decreases` keeps the previous
  measure and the first-visit flag in the frame (42, 43), and a coroutine of a
  recursive group that states a measure keeps its entry measure `m0` at role
  44 on the body's block. Roles in use on a statement: 0–7+, 20–25, 30+k
  (channel stashes), 41 (join mark), 42/43/44, 50+k (a consuming `pick` binding's
  flag byte, 1.6.1d step 3), 60+k (`old`).
- **A FUNCTION's `decreases` is checked at every call inside its recursive
  group** (D-304 (5), 1.5.8c step 4): `func:fact = int32(int32:n) decreases n
  never fails { … }` -- the clause among the contracts, before `never fails`.
  The groups are Tarjan over the checker's recorded calls (a `dyn` or a
  function-value call is no edge), so a measure on a function nobody in its
  group calls back is TYPE-075, a cyclic group with a measure on some member
  and none on another is TYPE-074, and a function measure wider than 64 bits
  is TYPE-073 (the members' measures compare in one `i128`; a loop's keeps
  any width). The check is `m0 >= 0 && m1 < m0` before the call -- the callee's
  measure over the arguments against the caller's at entry, through the
  generated `<sym>.measure` predicate -- so a call under `n > 0` with `n - 1`
  discharges its `terminate` row and a call under `n != 0` alone does not
  (`m0 >= 0` is open). A recursive call inside a `requires` clause is not
  checked (the predicate has no snapshot) and its row is `unencoded` with no
  trap. The `stack-depth` row (kind 13, `d` in `rows.txt`) is DERIVED by both
  runners after all files are decided -- one per cyclic group, `discharged`
  iff the group is measured and every call row discharged -- and `index.txt`
  has FIVE fields now (`NNNN symbol checks group measured`): a planted
  obligations directory (a self-check case, a tool) must write all five.
- **Every `while`/`when` in the tree states its clause, and a new one must**
  (1.5.8c step 3; TYPE-072's `neither` shape refuses from step 4). Write the
  measure with the loop: a counter's `bound - i`, a scan's bytes left
  `len - pos`, a walk along a chain built in order `count - c`, the parser's
  tokens left `raw p_left(p)`, a doubling under `c < cap` `cap - c`,
  `while (v > 0)` is `decreases v`, a hash probe carries a probe counter
  bounded by the table; a call as the bound is HOISTED into a local before
  the loop (`int32:n = raw f(x); while (i < n) decreases n - i`), never
  written as the measure unless the callee is `pure never fails` (TYPE-060:
  `p_left`, `item_member_count`, `d_at`, `ast_id_at`, the round constants are).
  A worklist that grows as it drains, a fixed point, a loop bounded by a
  deadline, a spin on the clock and an event loop are `unbounded` with the
  reason on the line above (D-316); a measure is never a trip budget. The
  reader's record is `meta/roadmap/done/1.5/tools/decreases_read.txt`; the tool
  (`decreases_sweep.py` over `loop_dump.npk`'s dump, built with `quickemit
  --keep`) writes the provable shape and applies the record.
- **A measure decides only over STABLE terms, so write the bound as a local**
  (1.5.8c step 5's measurement): `x.count - i` with `x` a POINTER parameter is
  a fresh opaque term at every read (DEF-14: a callee holding the pointer may
  write it), and so is a field of a BY-VALUE aggregate or a `raw f(s)` over
  one, because an aggregate has no value term yet (E-6). The sweep's `hoist`
  idiom — `int64:n = x.count;` before the loop where the body cannot change
  it, `decreases n - i` — is what the solver can read; a measure the check
  keeps is still a correct measure, only unproven. The residue by cause is
  `python3 meta/roadmap/done/1.5/tools/residue.py ROOT OUT.txt` over a tree's
  `build/verify/obl`.
- **A `decreases` in a `comptime` body is checked by the evaluator** (1.5.8c
  step 3): `fold_while` evaluates the measure each trip and a violation is a
  counterexample, TYPE-069 at the loop, as a `prove` there is (L-18).
- **Every `failsafe` names `(DecreasesViolated)`** (1.5.8c step 3): the
  prelude's loops carry measures, so nearly every program can reach it; the
  runners' generated failsafes (harness.py, selfcheck.py, npkg/suites.npk,
  npkg/selfcheck.npk) carry the arm too. REACH-002 names it where a root
  forgot.
- **Two hoisted counts in one scope collide** (1.5.8c step 3): the sweep's
  `hoist` declares `T:NAME` before each loop it decides, and two loops over
  the same window in one function are RESOLVE-001 -- keep the first local and
  drop the second declaration; the loop then reuses it.
- **The generated prelude is regenerated after a sweep of `src/prelude/`**
  (1.5.8c step 3): the compiler embeds `prelude_source.npk`, so a compiler
  built before `gen_tables.py` ran carried the UNSWEPT prelude and every
  smoke run over it measured nothing about the prelude's loops.
- **Emitting a branch to the label that follows does not mark the block
  terminated** (1.5.8c step 1): `x.fe.terminated = true` after a `br` makes the
  emitter skip everything up to the next `s_start`, and the elided measure
  check silently dropped the loop body that way -- the assume belt (fewer
  assumes than discharged guards) and the verified binary's exit code both
  caught it. Set the flag only where the block really ends (a trap's
  `unreachable`, a `ret`), and let `irw_label` open the next one.
- **A test's join deadline is a HANG NET, never a verdict** (DEF-85, 1.5.8b
  step 7): it bounds the trap route's stop walk (D-291), and under load a tight
  five-second bound made `failsafe_alloc.npk` answer the re-entry exit once in
  forty runs. Choose a bound only a real hang reaches (sixty seconds there), as
  the solver's net (D-297) and the explorer's control net (DEF-83) are chosen;
  a red that is a deadline under load is READ, not re-run.
- **A program test's temp path carries its pid** (DEF-91, 1.5.8b step 7): a
  literal `/tmp/npk_<name>` is shared by every concurrent run of that program,
  and four harnesses in four worktrees made `dyn_stream` and `fs_basic` answer
  their own `(E9)` exit through a truncated file. Spell it
  `string_concat("/tmp/npk_<name>.", int_to_string(sys(raw SYS_GETPID()) ?! E9))`
  and unlink it at the end (`scrub`); `trap_stops_runner.npk` is the model.
- **An accepted file is also EMITTED** (1.5.8b step 7): `tests/accept/`'s
  verdict is the frontend's silence AND the compiler under test lowering the
  file, in both runners. An `EMIT-002` there is a compiler defect by its own
  message; for a cycle the suite could not see one (DEF-88 sat in `moves.npk`
  since it was written). Put a construct the checker admits into an accept
  file and the emitter is now asked too.
- **`_` in a `pick` pattern binds nothing and keeps its position** (DEF-88,
  1.5.8b step 6d): the emitter's `enum_bind_is_wild` is the one notion, shared
  with `arms_bind_any` and the checker. A CONSUMING arm that discards a payload
  with `_` still owns it: all-wildcards drops the whole value through the
  enum's drop body, a mixed `(Two(a, _))` drops payload 1 in place at the bind.
  When adding a pattern shape to the emitter, ask who frees what the pattern
  does not name — and measure it with a churn twin under `NPK_HEAP_STATS`.
- **A whole-tree sweep with the CHECKER sees no emitter refusal** (found at
  6c): step 6b's sweep ran `tools/check` and reported nothing for
  `tests/accept/moves.npk`; the same sweep with `npkc` found the file dying as
  `EMIT-002` — pre-existing, since `tests/accept/` asks only the frontend
  (DEF-88). Sweep with the full compiler when the question is "does it
  compile".
- **A by-value struct's field is one term while nothing writes the binding**
  (D-317, 1.5.8d step 0): `w.count` in a loop's condition and in its
  `decreases` are the same `(|npk.f.T.count| w.1)`, so the measure decides
  and the rows over `w.count - i` follow; a field write bumps the version with
  the update frame (the other fields keep their values), a whole assignment,
  a `move` out of a field and a loop that writes any part of it bump it too.
  What ends the term for the whole function is an ESCAPE: `@s`, `$$i s`,
  `$$m s`, `list_push(@s.items, …)` — and, since DEF-94, a method call whose
  receiver is a pointer (`s.bump()` with `bump = NIL(Self->:self)`), because
  the emitter passes `@s`. A pointee's fields (`x.count` with `Reach->:x`)
  stay fresh per read (E-4, 1.6's). A `pure never fails` call takes a struct
  argument's identity only when the struct is PLAIN DATA (no pointer, no
  container, no enum inside): a pure function may read the heap through a
  reference its argument holds. A `prove` over a struct's fields is the way
  to see what the encoder knows (`tests/verify/agg_frame.npk`). **An encoder
  change moves EXISTING verify tests' rows** (three at this step, each read
  one by one), and `--only` cannot reach the verify suite: run `vprog.py`
  over every file of `tests/verify/` before the harness (twenty minutes
  against three hours).
- **An impl's parameter is `move` exactly where the trait's is** (DEF-116, 1.6.1
  step 0c): the caller spends its argument where the TRAIT's declaration says
  `move` — through a bound or a `dyn` it reads nothing else — so an impl that
  adds `move` to a lent parameter frees what the caller still owns and one that
  lends a parameter its trait consumes leaks what the caller spent; both are
  `NITPICK-TYPE-014` at the parameter. Write the impl's parameters as the trait
  declares them, and `.clone()` inside when the body needs to own.
- **The emitter resolves an identifier by the resolver's symbol, never by its
  name** (DEF-144, 1.6.1e step 1): `ident_slot` (ir_expr.npk) is the one reader —
  a local by its declaring statement, a parameter by the prologue's binding, a
  function, an error constant or a global to its own emission BEFORE any local of
  that name is consulted. A new emitter site that resolves an identifier NODE asks
  it; `fnem_lookup_idx` by name is for a name the emitter itself minted. A `for`
  binding is a `DeclParamDecl` node in a BLOCK scope (resolve.npk declares it
  so): a `DeclParamDecl` symbol is a parameter only when its scope is the
  FUNCTION scope — the first form read `for (int64:li …)` shadowing parameter
  `li` as the parameter, and npkg's `xfn_inline` trapped. A macro
  body's free name read the caller's local for a cycle because of the by-name
  lookup, and only the lone-identifier and comparison shapes showed it (the
  constant folder reads the symbol, so arithmetic was right by accident).
- **A character literal has a width** (DEF-145, 1.6.1e step 1): `'\u{…}'` is a
  `char32` literal whatever its value, a source character above U+00FF is one too,
  a plain character or a `\x` escape is `char8` — carried as the numeric literals
  carry theirs (`Token.width`/`ExprNode.width`, `WChar8`/`WChar32`; one token kind,
  one node kind; `char_lit_bits` reads it). A `char8` slot refuses the escape
  (TYPE-007); `128512char32` is the other spelling.
- **The join relays the EARLIEST-SPAWNED child's error** (DEF-146, 1.6.1e step 1;
  S-115, ratified as D-334): a child's error overwrites a
  previous child's in the LIFO walk and never the function's own; `main`'s join
  joins every child before `failsafe`. A test that expects "the first to fail in
  time" (the listener's `ctl_j3`) answers the other error by the rule.
- **An expired `timedwait` fails without re-acquiring, the guard spent** (DEF-147,
  1.6.1e step 1): the emitted wait reads the clock at its resume; a waiter loop
  that swallows the error (`?! E`) traps where it silently looped before, so bound
  the loop and handle the error.
- **A spawn LENDS its crossing until the block's join** (DEF-148, 1.6.1e step 1;
  `NITPICK-BORROW-016`): after `drop f(@s)` the owner may `alloc`/`get` a shared
  arena or acquire/signal a lock, lend it again, or hand `@s` to a helper that
  only allocates — and may not `destroy` it, assign over it, `move` it out or `$$m`
  it before the block ends. The concurrent-set table is `crossing_concurrent_method`
  (escape.npk), beside D-180's five kinds (`spawn_crossing_kind`); a new kind or
  operation goes there, and nowhere else.
- **A `cstring` is string-shaped and OWNS the copy `to_cstring` makes** (D-328,
  1.6.1d step 3b): `{ ptr, len, cap }`, `cap == 0` a body it does not own (a
  literal in `cstring` position, an `argv`/`environ()` element), `cap > 0` the
  owned copy, dropped at the scope exit; move-only under TYPE-046 — read an
  `argv` element in place (`argv[0i64].len`), `.clone()` for a copy, `move` to
  transfer. `cstring:cs = "Hello";` is the compile-time form (D-049), an interior
  NUL `NITPICK-TYPE-092` at the literal. The floor's six `cstring` symbols take
  and build the trio, so a floor edit near them keeps the third word; the
  generator's LLVM text for `cstring` is `{ ptr, i64, i64 }` (gen_tables.py).
  A cstring in `src/`/`tools/` is never `.clone()`d until a snapshot's prelude
  carries the impl (the bridging rule); the tools read `argv` in place.
- **A floor ABI change refreshes the snapshot in TWO hops** (1.6.1d step 3b,
  `1.6.1d.md` §2.9): the old builder on the OLD floor compiles the new `src/`
  (a body of the old ABI, an emitter of the new), that compiler on the OLD
  floor compiles `src/` into stage2, stage2 on the NEW floor compiles it into
  stage3, `cmp` silent, stage2 installed. `quickemit.py` links the builder with
  the tree's floor and is WRONG during such a step: build the dev compiler by
  hand (`.internal/twofloor_dev.sh`'s shape) and link what it compiles against
  the new floor.
- **`destroy` takes the OWNING place** (DEF-157, DEF-158, 1.6.1e step 1): a binding,
  or a field or element of one reached through no pointer — `NITPICK-TYPE-091` for
  a pointer parameter's pointee, a dereference, a pointee's field or a temporary
  (the owner freed the arena again: `MachineFault` in safe code). A destroyed field
  is vacated, not its aggregate's flag cleared (the siblings leaked). And DEF-159
  is OPEN: a value stored through a held `@x` after `move(x)` leaks, because the
  moved binding's flag is cleared — do not write that shape in `src/` until it
  lands.
- **`<=>` orders an integer, a `char`, a kernel identifier and the twisted kinds
  that order** (DEF-131, D-330; 1.6.1d step 4): `-1`/`0`/`1` as `int32`, the
  operands evaluated once, a `tfp`/`dim`/`tern` pair's ERR guard once (a program
  with one names `(TbbErr)`). A float pair and a frac pair are `NITPICK-TYPE-088`
  (no total order); a `string` and a `tbb` have no ordering at all (TYPE-008,
  D-093) -- `a.cmp(b)` for strings. The folder folds a constant pair; the
  encoder's term is `(ite (< a b) -1 (ite (> a b) 1 0))`, so a `prove` over the
  result decides.
- **A certain constant division is refused wherever the folder decides the
  pair** (D-331, 1.6.1d step 4): `5i32 / 0i32`, `7i32 % 0i32`, `(-127i8 - 1i8) /
  -1i8` are `NITPICK-TYPE-004` as a local's initialiser or an argument, not only
  in a `fixed` initialiser -- the typer folds the operands as `check_const_overflow`
  does and lets the folder's own division arm report ONCE (the diagnostic list
  keeps one copy of an identical finding, so a `fixed` initialiser's later fold
  adds nothing). `-128i8` is TYPE-031 (the literal `128i8` does not fit before the
  minus applies): spell a minimum `(-127i8 - 1i8)`.
- **An enum payload carries an address as a struct's field does** (DEF-160,
  1.6.1d step 4): the layout records an enum's pointer-bearing bit from its
  payloads (`tt_set_haspt` in the enum arm), `field_holds_ptr`, `type_holds_pointer`
  and `type_reachable_in` read it, and the constructor `E.Some(@local)` -- a
  METHOD-CALL-SHAPED node whose receiver names the enum (`call_is_enum_ctor`) --
  is an unknown callee to the ref collector that CARRIES every argument and views
  none, writes through none. Before this step `E.Some(@local)` left its frame in
  silence (exit 3 at -O0 through a dead frame). A new predicate over types must
  answer for `TY_ENUM` by its payloads, never "a tag and a word".
- **A moved-out owning value carries what its place holds** (DEF-125, 1.6.1d step
  4): `move(self.v)` of a `string` (or any type with no foreign pointer inside)
  carries the refs recorded AT `self.v`, not `self`'s PASS ref --
  `collect_moved_place_refs`; the return seam's implicit move reads the same. A
  swap through a lent `dyn`'s method compiles; a view stored in a field and moved
  out is still BORROW-001.
- **A `move` parameter's re-assignment is an assignment** (DEF-124, 1.6.1d step
  4): `x = raw nw();` after `move(x)` clears the moved bit (`assign_assign`'s
  `SYM_DECL` branch marks a `DeclParamDecl`); a parameter moved twice with no
  assignment between stays MOVE-001.
- **An arm over an enum names one of ITS variants** (DEF-142, 1.6.1d step 4):
  `(K.C)` over a `K` without `C`, or `(Other.A)`, is `NITPICK-RESOLVE-002` at the
  pattern (`check_enum_arms` in `type_pick_rules`, both spellings) where it was
  EMIT-002. A `mod:` name is an identifier (DEF-143): `mod:error;` is PARSE-001 at
  the keyword, and the loader adds no second sentence (a header node with name 0
  is the parser's report).
- **A mismatch between two same-named types names their modules** (DEF-126,
  1.6.1d step 4): "expected `same_name.Row`, found `rows.Row`" --
  `type_display_qualified` finds the module NODE holding the declaration (an
  inline module by its name, a file by its header); only when the plain texts are
  equal.
- **A range pattern's bound is read through the folder** (DEF-132, 1.6.1d step
  4): `(-5i32..-2i32)` lowers (`pat_bound_value`); a bound that does not fold is
  still the frontend's broken promise (`iv_broken`).
- **`pkill -f PATTERN` from a Bash tool call kills the call itself when the
  pattern's text appears anywhere in that call's command line** (found four times
  in one evening): assemble the pattern from a variable (`P="emit_cm"; pkill -f
  "${P}p.sh"`) and put NOTHING else naming the target in the same call.
- **`cd X && <command> &` runs its `cd` in the background job, not in the shell** (found at landing 95): the
  foreground shell stays where it was, so a `$(git rev-parse HEAD)` on the next line reported another worktree's sha
  while the background command ran in the right one. Put the `cd` on its own line, or wrap the pair in `( … )`. And
  when stopping a chained wrapper (`bash -c 'a; b; echo DONE'`), kill the WRAPPER before its running step, or the
  wrapper runs the next step the moment the first dies.
- **An `async` METHOD is called under `await` or spawned with `drop`** (DEF-149,
  1.6.1e step 2): TYPE-043 at a bare `r.read(buf, d)`, `obj.run()` or a `dyn`'s
  method as at a bare free call -- applied where `type_method_call` makes the
  callee final. Before it the bare method call compiled to a call of the
  coroutine's undefined symbol and `llc` refused the module.
- **A macro body is declarations or statements, not both; a broken body expands
  to nothing; a declaration's modifiers are read past** (DEF-150, 1.6.1e step
  2): a statement in a declaring body is `NITPICK-MACRO-010` (once), a body with
  a parse error expands to nothing (the errors are the report; `record_site` had
  read the window past its end and trapped), and `comptime func:`/`async func:`
  in a body are declarations (`p_at_decl_start` peeks past the modifiers). The
  reference's `emit_methods` example spelled `$$i Box:self`, never a parameter
  form: a receiver is `Box:self` or `Box->:self`.
- **A rejection file names ONE `expect-error` line PER SITE** (D-332, landed
  2026-09-27): both runners count each code's reported sites against the lines
  naming it, so a file that names several sites under one line fails (`X
  reported at N site(s), expected at M`), and so does a SILENT site -- the one
  the set rule of D-237 could not see (`not_constant.npk` expected a refusal
  D-222 had made legal, for a month, because its code was reported elsewhere in
  the file). Write each site's line with its `expect-error-at`, measure the
  header over the step's checker, and READ every site: the rule's first run
  found three compiler defects (DEF-161…DEF-163) among thirteen files.
- **A refusal the PARSER makes is a unit case, never a rejection file** (D-085;
  1.6.1e step 2 and 1.6.1d step 4, two red harnesses in one night): the `grammar`
  stage feeds EVERY file of the tree through the real parser and requires it to
  parse, so a file whose expected refusal is PARSE-0NN, or a MACRO code the
  parser raises (MACRO-010), fails that stage and `parity` with it. Test it in
  `tests/frontend/parse_*.npk` (lex a string, `p_parse_decl`, count the
  diagnostics and read the code), or on a temporary file through the loader
  (`tests/frontend/module_graph.npk`'s `mod:error;` case).
- **No tree nests deeper than 256 levels** (`AST_DEPTH_MAX` in ast.npk, DEF-150,
  1.6.1e step 2): a function's body block is level 1, so a top-level statement's
  expression may be 254 tall (`expr_depth_max.npk`). The parser measures each
  declaration's tree after it (`ast_depth.npk`: the AST's construction log,
  children first, one pass) and refuses the first subtree past it with
  `NITPICK-PARSE-012`, once per declaration; its own descent (`p_unary`, once
  per level) is held to the same number. A LEFT chain `1 + 1 + ... + 1` is one
  level of the parser's recursion and 600 levels of tree: counting the parser's
  nesting alone left it trapping the folder, found by probing the shape the
  listener had not. The expander refuses a splice landing past the bound
  (MACRO-003, site depth + clone height); every analysis depth reads the one
  constant. A NEW node constructor must push the height and the log beside the
  flag byte, and a new node kind needs its children in `ast_depth.npk`.
- **The build mode is the front half's** (DEF-151, 1.6.1e step 2):
  `front_run_mode(f, root, no_wildx)` -- npkc and `tools/check.npk` hand it the
  same `--extra-picky=no-wildx`; `reject_wildx` walks the PROGRAM's modules (a
  node is the prelude's by its span's file) and reads four spellings (a `wildx`
  local, a `wildx`-returning function, an `=>! wildx …->` cast, a `wildx_*`
  call). A rejection or accept file names its flags in a `// npkc-flags:` header
  line and both runners pass them to the tool; a field of an unassigned local
  cannot be written by the caller (ASSIGN-001), which is why the mode is an
  argument.
- **A builtin scalar takes no type arguments** (DEF-152, 1.6.1e step 2):
  `tfp64<Meters>` is TYPE-016 (it was accepted and the unit dropped); a unit
  rides `dim256<Unit>` alone (D-196). `string` keeps its three spelled forms
  `string<char8>`/`<char16>`/`<char32>` (TYPE_REFERENCE §3.2), and every other
  argument on a `string` is TYPE-016 -- but the two wide forms are BYTE strings
  today (the width is ignored), which is S-116, settled as D-335: wide strings are IN, designed and
  implemented as 1.6.1f before the freeze -- until it lands, do not write one.
- **A module constant of a float, fixed-point, `tbb` or ternary type is WRITTEN, not computed** (DEF-179, DEF-197; landing
  87): a literal, a negated literal, `ERR`, another module binding of that type, or `comptime(…)` of one — `fixed
  flt64:TAU = 6.283185307179586f64;`. An expression over one (`2.0f64 * PI`, `100tbb8 + 100tbb8`, `29524 + 1` in a
  tryte) is NITPICK-TYPE-035: the folder computes in the plain integers only (`fold_computes`), and it carried these
  families as plain integers into three silent wrong constants until this landing. A `flt32` module constant is
  TYPE-035 by name until DEF-203 lands. In the folder a new numeric family is CARRIED (`CV_REAL`, or a typed `CV_INT`
  no operator arm accepts) and rendered by the type the checker recorded (`const_scalar_text`) — never `cv.num` alone.
- **A folded value is held to the type it lands in** (DEF-198, DEF-201; landing 87): a module binding read by name has
  its DECLARED type (`fold_stamp`: `comptime(BIG)` over an `int64` binding is an `int64`), an untyped `comptime(…)`
  value must fit its slot (TYPE-031), and an array size, a lane count and a channel's constant argument are plain
  integers in `int32` range. Every `fold_const` starts with fresh fuel, so a fold that RESOLVES A TYPE needs its own
  bound (`stamp_depth`: `fixed int32[K]:K = 5;` trapped the first form of this change).
- **A literal's PAYLOAD is its value only for a plain integer, a `tbb` and a ternary** (DEF-197, DEF-204; landing 87):
  a `tfp`/`dim256` literal's number is its Q constant (`tfp_q_of_int`, `tfp_q_decimal`), a float's is its text, and a
  fraction's payload is an intern index. Three readers wrote the payload whatever the type — the global's renderer,
  the `pick` pattern lowering, the `flt32` digit count — and each was silent or a trap. A new reader of a literal node
  asks the TYPE first (the node's recorded type, or the selector's for a pattern).
- **`tid` is a type keyword** (again): a parameter named `tid` is PARSE-002 "expected an expression" at its first USE,
  lines from the declaration. `pid`, `tid`, `fd`, `uid`, `gid` are never names.
- **Two emissions that differ only in how a constant is SPELLED are compared as ARTIFACTS and through a reference**
  (landing 89): assemble both under ONE module name (an emission states no `source_filename`, so `llc` names the module by
  its input file and the name reaches the object -- the same text under two names is two objects) and require the `-O0`
  object, the `opt -O2` text and the `-O2` object byte-identical; beside it, read every constant of both texts as its bits
  through an exact reference and require the texts equal (`wt/27a/.internal/tools/objcmp.sh`, `fnorm2.py`). Either leg
  alone says less: the objects say LLVM read both alike, the reference says the new spelling is the number written.
- **A test must hold the text that tells the roads apart** (landing 89): a short `flt64` literal is read alike by LLVM's
  parser and by `float_round`, so a road test with short texts cannot see a writer that bypasses the conversion.
  `float_literal_kat.npk`'s `roads` puts a text past LLVM's cap on every call site of the one writer. And a GENERATED
  `uint64` past 2^63 (a negative double's bits) is built, not written: `(…u64 | (1u64 << 63u64))`, LEX-004 otherwise (D-311).
- **The compile-time evaluator runs statements only inside a `comptime func:` call** (D-338, landing 90): a `pick`
  EXPRESSION folds only there (`TypeResolver.fold_frames`, raised and lowered by `fold_call` alone; never `fold_depth`),
  because the emitter and the typer ask the folder about run-time nodes (`emit_const_fold`, `check_const_overflow`) and
  an arm's assignment evaluated into the resolver's environment would never run. Any new evaluator arm that runs
  STATEMENTS asks the same counter; a pure expression arm (`Enum.Variant`, `a.cmp(b)`, `?!`) folds anywhere. An arm of an
  expression `pick` ends in `give`: a `pass` out of one is not evaluated at compile time (the statement form returns).
- **An enum value in the evaluator is its variant's POSITION, and it does not leave** (D-338): `CV_ENUM` holds the
  enum's type and the member index `variant_tag_of` takes; `==` on two is identity (tags are unique, DEF-193). A new
  consumer of `fold_const` must say what it does with one: `type_comptime` refuses it (TYPE-004, its own sentence) and
  `const_init_verdict` answers 1 (TYPE-035) until S-127 is answered. `a.cmp(b)` is recognised by the receiver's VALUE
  (the folder holds no call record) and its `Ordering` is found in the prelude's own scope, never by spelling.
- **A `comptime func:` is emitted nowhere** (DEF-214, OPEN; S-128): calling one outside `comptime(…)`, a constant site
  or another `comptime func:` reaches `llc` as an undefined symbol. A test that needs one body at compile time AND at
  run time writes it twice (`c_f` / `r_f`: `comptime_pick.npk`), and its run-time arguments come from a function the
  compiler cannot fold -- or the "run-time" side is the evaluator again.
- **The evaluator short-circuits, and a fold entered from inside an evaluation is part of it** (DEF-211, DEF-213;
  landing 90): `false && X` and `true || X` do not evaluate `X`; `fold_const` resets the depth and the said-flag only
  when no frame is open, so a type resolved mid-body (`#size_of<int8[N]>()`) no longer zeroes the recursion bound.
  When adding a bound, ask what RESETS it: this one was reset by the front door of its own module.
- **Two value arms with one value reach `llc`** (DEF-212, OPEN): the checker reports an arm after `(*)` (PICK-004) and
  nothing for a repeated value; the `switch` lowering is then invalid IR. Do not write one in a test of something else.
- **A relayed answer is recorded with where it was said** (landing 90): the user ratified S-119…S-126 in another seat's
  session; the decisions carry his words, the date, the session, and the questions as they were put -- never a
  paraphrase of the recommendation. The seat that holds the queue records it in its NEXT commit and tells the others.
- **A float constant has ONE writer, and it writes BITS at both widths** (DEF-203, DEF-209; landings 88, 89):
  `float_const_text` (ir_expr.npk) behind `emit_float_const` and `const_scalar_text` -- `0x` and sixteen hex digits: the
  double `float_round` makes of the text, or for a `flt32` the double holding its float. A new site that spells a float
  constant from a program's text asks it: a decimal handed to LLVM is read by a parser that caps the exponent at 24,000,
  and `fptrunc double <text> to float` is two roundings. The only decimals an emission holds are the emitter's own exact
  bounds (`cast_bounds.npk`, one `0.0`). The emitter and the encoder's `fp_literal` are ONE number by one rounding each:
  change either and the other moves in the same commit, or a verified build proves things about a float the program does
  not hold.
- **`float_round(text, width)` is the conversion, and its answers come from exact rationals** (numeric.npk; DEF-203): both
  formats, the `inf`/`zero` flags reported and unread (DEF-205, S-126). `tests/frontend/float_round.npk` and
  `tests/backend/programs/float_literal_kat.npk` are GENERATED by `bootstrap/generator/float_vectors.py` (`--write`; the
  harness's `check_generated_current` runs `--check`): never edit a vector by hand, and after touching the generator
  regenerate in the same commit. Both halves of the KAT test the compiler's conversion through the emitter since DEF-209
  (a red there is the compiler's); `float_vectors.py --llvm` is the measurement of LLVM's parser, outside every gate.
- **A `comptime(...)` operand is typed by NOBODY: the compile-time evaluator alone reads it** (DEF-205, landing 91).
  `type_comptime` folds its operand and never hands it to the typer, so a rule the checker asks of a literal or a name is
  skipped under `comptime(` unless the evaluator asks it too. A float literal's fit rule is asked in
  `fold_suffixed_literal` -- at `fold_depth <= 0` only (inside a call the literal sits in a function's body, which the
  checker types, and a report with a call chain behind it would be a second sentence), in the checker's own sentence at
  the literal's own span, so a literal both read is one report. A new rule about a literal owes the same question: where
  does the evaluator read this without the typer?
- **A float literal FITS its type, and its length is free** (D-346, landing 91): one that rounds to infinity, or a nonzero
  one that rounds to zero, is NITPICK-TYPE-031 (`float_fit_refusal`, numeric.npk); a subnormal result stays; an infinity
  is computed (`1.0e308f64 * 10.0f64`), never written. The 15-digit rule on `flt32` is gone. The emitter writes no
  constant for a literal that does not fit (`float_const_text` answers ""): if EMIT-002 ever names a float literal, a
  road reached the emitter that neither reader asked -- find the road, do not loosen the belt.
- **A float literal's rules follow its WIDTH, whatever gave it one** (DEF-206, landing 88): `float_text_ok` is asked by the
  suffix's branch, the contextual one (an unsuffixed fraction in a float slot) and `type_comptime` (an untyped folded
  fraction). A new rule about a float literal goes there, once -- "`flt128` has no literal" sat in the suffix's branch
  alone for a cycle; and after closing a hole, probe EVERY position the construct can sit in (the `comptime` road was
  found by a twelve-line probe after the first fix looked complete, and its suffixed twin by another at landing 91).
- **A bound on a text is stated against the text** (DEF-207, landing 88): an exponent's digits are summed only while
  `ex <= text.len + 1000` -- past that no digit of the literal can bring the number back into range, so the answer is
  forced; a FIXED cap misreads a fraction whose leading zeros outnumber it, and NO cap traps (`ex * 10` overflowed in
  the encoder) or builds a million-digit numeral. When a function builds something whose size a literal chooses, ask
  what a nine-digit exponent does to it -- in the plain build and under `--obligations`.
- **A measured claim has two halves, and a measurement has a domain** (landing 88): D-143 said two things about float
  literals -- the `flt32` half was false and the `flt64` half had never been checked. A first rig of 10,015 texts held
  it, and the landing was scoped on that; the COMMITTED measurement, with two texts written for another purpose (an
  exponent that undoes a hundred thousand leading zeros), found LLVM's parser capping the exponent at 24,000 (DEF-209).
  "None different" is a statement about the texts tried: say what family was not in them. And a reference function that
  builds the number is not safe on every legal text: `1.0e999999999` hung the first vector generator.
- **A float literal is its production, and its tail is a suffix or nothing** (DEF-166, DEF-174, DEF-175; 1.6.1e step
  3): `NITPICK-LEX-009` for any other tail (`1.5f512`, `3.14flt32`, `2.5e3zz`), the token still a float literal so the
  parser adds nothing (D-240); a sign belongs to a literal only inside an exponent with a digit after it -- `3.5f64-x`
  computed 3.5 before, in silence -- and the kept text carries no `_`. A lexer refusal is a unit case
  (`tests/frontend/lexer_numeric.npk`), never a rejection file. `f128` lexes and TYPE-030 refuses it. Both scans still
  accept a trailing or doubled `_` (`10_`, `1__0`): DEF-190, OPEN -- do not write one.
- **A literal with a fraction takes a float or fixed-point suffix** (DEF-167, 1.6.1e step 3): `2.5i32`, `2.0i32`,
  `1.5u8`, `2.5tbb8` are TYPE-031 at the literal. And `flt256`/`flt512` are TYPE-001 in every type position: the
  generated scalar table still answers both words, so the refusal stands AHEAD of it in `resolve_named`.
- **The task builtins are `async`'s** (DEF-168, DEF-169; 1.6.1e step 3): `suspend_until`, `suspend_io` and `io_watch`
  are TYPE-043 outside an `async` function, the two parks also inside `defer`; `io_unwatch` is legal anywhere. A park's
  wind-up exit runs the function's defers -- a park inside a defer body made the emitter lower the body inside itself.
- **A channel's LEVEL is a lock level** (1.6.1e step 3): `send`/`recv` are waits at it -- LOCK-001 at or below a held
  level, through a callee or a spawned task too -- and a wait raises no hold (`LockTable.hold_min`: what a call may
  leave HELD is fed by `acquires N`, an `acquire`/`read`/`write` and a callee's own hold, never by a wait). A test that
  sends with a guard held puts the channel ABOVE the mutex (`mutex_basic.npk`: level 6 over 5).
- **An implementation of an undeclared trait method acquires nothing** (D-056, D-113; 1.6.1e step 3): a trait method
  with no `acquires` clause gives its default body and every impl a bound of nothing -- LOCK-002 where the body can
  reach any level, a channel wait included (the sentinel is read through `bound_is_nothing` alone) -- and an exact
  `acquires N` on a trait's method is a checked ceiling that does not stand in for the body. So NO impl of a prelude
  trait (`Writer`, `Reader`, `Iterator`, `ToString`, ...) may take a lock or wait on a channel: take the guard outside
  the call and write through the guarded value. The lock walk reads the checker's call record (`exprtypes_callee`); an
  analysis that resolves a method call asks it too, never `find_method` on the receiver's recorded type (which misses a
  pointer receiver, UFCS, `Trait.m(x)` and a default body's `self.m()`). DEF-181's six holes and S-119 are OPEN:
  "lock-order freedom is proven" is the design's claim and not yet the compiler's.
- **A macro cycle is refused at its declaration, and the edge is what the body writes** (DEF-172, DEF-173; 1.6.1e step
  3): MACRO-004 at the cycle's first macro in source order, uninvoked cycles included; `#ign(#a())` inside `a` is an
  edge even if `ign` discards its argument. The round bound (32) reports at the invocation left standing (a probe walk
  that instantiates nothing), and a chain of exactly 32 settles. A `macro:` inside a macro body is MACRO-005. A
  program `check_macro_decls` refuses is not expanded.
- **The expansion walk reaches every expression, and every instantiation is its own copy of the body** (DEF-170,
  DEF-171, 1.6.1e step 3; DEF-189, step 3a): an invocation in a contract, a measure, a `Rules` clause, an array size or
  a `comptime` argument inside any type, a pattern, an attribute is expanded (no program reaches MACRO-006 now). The
  clone is TOTAL: an expression, a statement, a declaration, a pattern, an attribute, a generic parameter and a
  verification node are cloned, each one; a TYPE is cloned exactly when it holds an expression and is otherwise the
  template's node (nothing writes onto a type; the checker DOES write a destructure's binding types onto its pattern
  and the resolver keys a symbol by a declaration node — ask "does a later pass write onto it or key a table by it"
  before sharing a node). An alias (`macro:a = () { #b(); };`) is passed through at every declaration site. A new
  position that holds an expression needs its arm in the walk (`expand_in_*`), in `blank_*` and in the clone
  (`clone_*`) — what the clone clones is BLANKED in a detached original — or the audit calls it a hole.
- **A macro parameter is substituted wherever an expression stands, and cannot be a type's name or a name the body
  declares and uses** (DEF-189, 1.6.1e step 3a): `int32[N]`, `(LO..HI)`, `#[derive(TR)]`, a cast's target, a signature's
  types all take the argument. `NITPICK-MACRO-011` at the macro's declaration (uninvoked too) for `T:x`, `T{ … }`,
  `x =>! T`, a destructure's type, and for a local, a `for` binding, a pattern binding, a function parameter, a generic
  parameter or an emitted declaration of the parameter's spelling THAT THE BODY ALSO WRITES AS AN EXPRESSION. A
  parameter that only names an emitted declaration (`func:N`, literally called `N`) is MACRO_REFERENCE §10's open
  question, S-123 — a sweep over the library listener's corpus is what caught the first form refusing it: READ what
  a new refusal reaches there against the reference's own "open" lists before calling it a defect fix. A BARE NAME IN A TYPE-ARGUMENT LIST IS A TYPE'S
  NAME (D-064 §2): a compile-time value there is written in parentheses, `simd<int32, (N)>`. (DEF-183's argument rule landed as D-340, landing 92; DEF-192 at
  landing 86.)
- **A generic function's instance is its TYPE ARGUMENTS, at every reader** (DEF-191, DEF-195; 1.6.1e step 3a): the
  checker records them on the call (`exprtypes_callee_targs`: the window's start plus one, 0 for none) and the emitter
  matches `fninst_for_call` by their contents; the encoder puts them — and a method call's receiver type — in a `pure
  never fails` callee's function symbol (`CallEnc.inst`). Two instances can share a substituted SIGNATURE (a parameter
  that appears only in the body), and two calls can share argument TERMS (an `Int` is an `Int` at every width): a new
  reader that tells instances apart by either is the same defect. After fixing a wrong answer in the emitter, ask
  what the verified build's MODEL said about the same program — DEF-195 was found by that question.
- **An enum variant's explicit value is a bare integer literal in `[0, 2^31 − 1]`, distinct from every other tag**
  (DEF-193, landed 2026-10-01; S-122 the user's): `X = 7i32 + 1i32`, `X = BASE`, `X = -3i32` and a tag two variants
  share are `NITPICK-TYPE-093` at the declaration (each was accepted and wrong: the expression ignored, the position
  used). An unvalued variant's tag is its POSITION among the members, never the previous value plus one —
  `{ A = 1i32; B; }` is refused, because `B` is 1 too. `check_decl` had no arm for an enum: when a declaration kind
  has no arm in a checker, nothing about it is checked, and the silence reads as acceptance (D-085's shape).
- **A compile-time binding is the resolver's, never its spelling** (DEF-196, landed 2026-10-01): the evaluator keys a
  `comptime func:` call's variables by the declaring node (`FoldEnv` = kinds/origins/vals; `fold_ident` asks only
  for an identifier that HAS a symbol). A nested block's local is a new binding, a macro body's free name is the
  module's, and a name inside a type (`int32[LEN]`) is the module's `fixed` binding and never a local (D-222). The
  lesson is DEF-144's again: wherever a walker looks a NAME up itself, ask whether the resolver already bound it —
  a by-name table beside a resolver is a second, weaker resolver, and its own comment ("no nested scope here") was
  true only on the day it was written.
- **A type's name in a macro body resolves where the macro was written** (DEF-192, landed 2026-10-01; D-057): it
  binds a generic parameter only where the body declares it; the invoking function's, struct's or `impl`'s
  parameters are invisible to it (`V:item;` spliced into `struct:Box<V>` is TYPE-001 — it compiled by capture; S-124
  asks whether a body should have a way). `ast_macro_at(ast, span)` says which macro declaration a node was
  written in (a clone keeps its template's span; `Ast.macro_spans` is filled by `p_parse_macro`) and
  `macro_may_bind` is the one rule: a NEW lookup that answers a type's name from anything but a scope (a generic
  window, an associated type) owes the same question. An argument's type names stand at the invocation.
- **When a test answers wrongly, hand-write the shape without the feature under test before reading the feature's
  code** (1.6.1e step 3a): the turbofish case of DEF-189's test failed, and the same two calls written with no macro
  failed identically — DEF-191, a defect of generics three cycles older than the macro the test was about.
- **A compile-time failure names its call chain** (`fold_chain`; 1.6.1e step 3): "; reached through the compile-time
  calls `a` -> `b`", outermost first; the folder asks "already said" of the sentence WITHOUT its chain (`fold_said`),
  so one expression is one report whichever path reaches it first.
- **Read every delegated diff whole, and ask its author how each mechanism FAILS** (1.6.1e step 3's hand-off): three
  implementation agents in three worktrees each found defects beyond the design, one package reached integration with
  no seat review, and the incoming seat's question about one mechanism (the shared template node) produced DEF-189.
  The measurements say a change does what was asked; only a reading says what else it does. No more than three agents
  at once (they took the five-hour usage window from 12% to 65% in two hours).
- **`failsafe` must name what the PRELUDE can raise on the program's behalf**
  (DEF-86, 1.5.8b step 6b): the reach analysis follows every resolved call into
  the prelude and every import, so a program that calls `list_pop`, formats a
  float, hashes through a bound or parses a path is asked for the arms those
  bodies can reach — `(OutOfBounds)`, `(IntOverflow)`, `(TbbErr)`, `(BadPath)`
  — exactly as it is asked for its own. A `dyn` or bound call reaches EVERY impl
  of the trait (an over-approximation; E-5 owns narrowing it), so a generic
  bound over a prelude trait can demand `(TbbErr)` a program never instantiates.
  REACH-002 names each missing identity; add the arm with the code `(*)` would
  have answered and nothing else changes. The prelude is still not walked whole.
- **Every module of ours states the pinned layout and triple** (E-8, D-322 (5);
  1.6.1 step 1): the emitter's first two lines are `target datalayout = "…"` and
  `target triple = "…"`, the floor and the explorer shim carry the same two by
  hand, and `nitpick.toml`'s `[toolchain] triple`/`datalayout` are the pins both
  runners refuse a manifest without and hold every emission, the floor and the
  shim to. The layout is held to what the pinned `opt` derives from the triple
  (`opt` keeps a wrong layout line as written and `llc` accepts one in silence,
  so the line proves nothing about itself; `llvm-as` completes a partial string,
  so the pin is the full canonical one). A change to either string is a change
  to the emitter's constants, the floor, the shim AND the pin in one commit, with
  a snapshot refresh; the object does not move for the line (measured). A
  library's manifest carries the same two rows — checked, never chosen. The
  gate's whole-program form drops every `target` line of the floor (the program's
  header governs): a second layout line is "invalid redefinition" to `llvm-as`.
- **A call's result holds a borrow by PROVENANCE, not by shape** (D-326, 1.6.1b): a
  callee that BUILDS its result from what it is handed (`string_concat("made:",
  b.s)`, a copy of a view, a method's rendering) hands back nothing of the caller's
  frame, and the caller may return it — `string:r = raw make(@b); pass r;` compiles.
  A callee that returns a view, an address, or a struct carrying either still
  refuses at the caller's `pass` (BORROW-001), as does a `dyn` method or a function
  value (an unknown callee keeps the shape rule) and a trait method through a bound
  where ANY impl views (the union). When adding a reader of the summaries
  (`collect_call_refs`), read it with `x.reporting` off unless the walk is one whose
  report belongs there (a binding's or a return's) — the collector reports a view
  of a temporary as it walks — and never ask it about a bare builtin (its verdict is
  the `Views` column's; `escape_expr` discards the arm's).
- **`Copy` is a prelude-owned MARKER, and the bound `T: Copy` licenses the copy**
  (D-327, 1.6.1c): a generic body may copy a `T` (`T:x = (<-l)[i];`) only under
  `T: Copy`; a bare `T` stays move-only (TYPE-046) and so does `Self`. Every
  scalar is `Copy` (the generated region), the prelude's `Ordering`, `Duration`,
  `Whence`, `Fcmd`, `Advice` and `LineEnding` are, a struct or enum of copyable
  members derives it (`#[derive(Copy)]`; a `string`, a `List`, a `dyn`, an
  `Optional` member refuses at the declaration, DERIVE-006), and an impl on a
  type that owns or whose member is not `Copy` is `NITPICK-TYPE-087` — the one
  judge for hand-written and derived impls. `list_get(@l, i)` reads a
  `List<T: Copy>` element out by value and traps `OutOfBounds` outside
  `[0, count)` (name the arm). A program cannot declare `Copy` or `list_get`
  (RESOLVE-001). A REFUSED impl still sits in the impl table: a rejection file
  that also tests the bound must use a type with no impl anywhere in the file.

- **A macro ARGUMENT is the caller's text and resolves at the invocation site** (D-340, landing 92): pass a local,
  a parameter, any expression; nothing the body declares captures it, and what stands under it (a `pick` arm's binding,
  an invocation) resolves with it. An argument a macro BODY writes for an inner invocation is the body's text. A
  DECLARATION macro's invocation stands at module level, so its argument resolves in the module's scope — naming the
  emitted function's own parameter is RESOLVE-002 (it compiled by capture before). A name inside a TYPE is the
  module's binding wherever written (D-222), an argument's included. `#caller` in an argument written outside a macro
  body is MACRO-008. Through an ALIAS (`macro:a = () { #b(); };`, D-125) the target's `#caller` reaches the alias's
  caller; through a body that merely contains the invocation it reaches that body's site, the module.
- **The hygiene marks are COUNTS in a node's flag word, and whatever replaces or copies a marked node owes them**
  (landing 92; `ast.npk`): `back` (steps out), `HYG_SITE`, `roots`. The clone carries them (`clone_expr`/`clone_stmt`
  are wrappers: a fresh node's word is empty, which is how `#caller` lost its mark, DEF-221); `instantiate_expr`
  COMPOSES the invocation's, its own root and the clone root's (a root then a step cancel); `instantiate_stmt` hands
  the invocation's to the block. A new expander path that writes over a node, or builds one from another, asks the
  same question: what did the old node carry? The resolver refuses a count that does not fit (RESOLVE-INTERNAL).
- **In the SMT encoder a spelling is not a binding** (DEF-223, landing 92): the bindings are kept by spelling,
  innermost first, so a site that turns an identifier NODE into a binding asks `ident_bind` (a module-scope symbol
  never reads a local), and a spelling that an identifier inside an expansion uses and the function declares twice is
  put in the escape set by `enc_prepare`. A NEW BINDING FORM in the encoder must be counted in the pre-walk
  (`collect_escaped_stmt`, `note_pattern_decls`), or the ambiguity rule misses it. The probe that found the hole asks
  the VERIFIED build, not the plain one: `python3 .internal/handoff_s11/vprog.py WT NPKC FILE` with the rows named —
  a wrong proof shows as "the verified binary exited N, expected M".
- **Read a finding against the reference's OTHER rules before calling it a defect** (DEF-220, withdrawn inside landing
  92): `#caller` reaching "two sites out" through `macro:w = () { #inner(); };` was D-125's alias rule, and the tree's
  own `macro_alias_sites.npk` said so the moment the chain ran. An inconsistency between two spellings is a defect
  only if no rule separates them; here one did.

- **A bare `#unreachable();` leaves, and a leaver is a row in BOTH lists** (DEF-225, landing 93): `stmt_exits` (which
  arm a merge drops: definite assignment, a `fixed` binding's one write, a move, the taint) and `stmt_completes`
  (FLOW-001) are twins with opposite conservatism, each a list of statement KINDS. A statement that never completes
  and is no kind of its own (the builtin is an expression statement) needs a helper read by both
  (`stmt_is_unreachable`). Under another expression (`int32:z = #unreachable();`) the statement completes, on
  purpose; the encoder's `stmt_falls_through` is a THIRD list and reads the builtin as falling through, on purpose
  (its claim becomes a hypothesis). One missing row showed as FIVE codes: when a finding names one refusal, probe
  every consumer of the predicate before sizing the fix. The early exit through `pick (r.is_error) { … }` is
  DEF-226, OPEN: write the `if` form.

- **A temporary is registered ONCE, by the one site that produces its value** (DEF-227, landing 94): `emit_fit`'s
  transfer registers the `dyn` cell (and the `Optional` wrap) it builds, so the expression kinds that lower THROUGH
  `emit_fit` -- the explicit `=> dyn` cast -- must not also be in `temp_producer`'s list: two entries under one name
  are taken once (`fnem_temp_take`) and dropped once, a use after free the binding then reads and a second free at
  the scope's exit. When a lowering hands a value to a path that registers it, ask whether the node's own kind
  registers it again; and when a floor call COPIES its operands (`npk_string_concat` frees neither), ask who frees
  them -- the template splice freed nothing (DEF-228) and handed a lone item's header through as its value (DEF-241).

- **A template's pieces are the statement's temporaries, and its value is its own body** (DEF-228, DEF-241; landing
  95): `emit_template` registers each `to_string` result and each replaced intermediate concatenation through
  `temp_register` once it has been copied past, never a string ITEM (a place's header is borrowed; a call's result is
  that call's temporary), and never the final value (the producer rule registers it once -- a second registration is
  F-037's double free). A lone string PLACE or CALL item is copied, so `\`&{ s }\`` is never `s`'s header; a lone
  LITERAL is not -- `\`plain\`` is the literal's own header, as `"plain"` is, at no cost (the landing's first form
  copied it too, and the sweep's one unexplained moved program, `op0391`, said so: read every moved program against
  the change). When a lowering hands an owned value to a floor call that COPIES (`npk_string_concat` frees neither
  operand), ask who frees the operand; and measure a lowering that allocates with a churn twin under
  `NPK_HEAP_STATS` (`tests/cost/template_splice.toml`, `template_literal.toml`).

- **A generic function may not instantiate itself at a type built from its own parameter** (DEF-229, landing
  96): `deep::<Box<T>>` inside `deep<T>` is TYPE-018 at the call; a cycle through another function that grows the
  argument is refused by the emitter's cap at 64 (the depth of the requested type), by the same code -- a backend
  refusal by name travels as `iv_refused`/`LL_REFUSED`, the third state beside the rung and the defect. A cycle
  that closes at a concrete type is fine. The two walks over a type's structure live in types.npk
  (`type_mentions_params_of`, `type_nest_depth`): a new type kind with an operand goes into `type_kind_has_elem`.

- **A toolchain pin is MOVED by measuring first, with the machine untouched** (D-349, landing 99): fetch the signed
  packages of the new release, verify them (`gpgv` on InRelease, the index's and each `.deb`'s sha256), `dpkg -x` them
  into a user prefix WITH `llvm-20-dev` -- the `usr/lib/llvm-20/lib/libLLVM.so.20.1` symlink ships in that package, and
  without it the tools' RUNPATH `$ORIGIN/../lib` falls through to the SYSTEM's library in silence (`ldd` on the prefix's
  `llc` is the proof; the version line is printed by the library that loaded) -- then run the ladder and the harness
  under `PATH=<prefix>/usr/lib/llvm-20/bin:$PATH` and compare object for object. Across 20.1.2 -> 20.1.8 every object and
  the emission were byte-identical and one `.comment` byte per linked binary moved. Every tool that links LLVM is
  rebuilt into a FRESH `ENGINES_ROOT` (never over the pinned install) and re-pinned -- Alive2 links `libLLVM.so.20.1`
  dynamically and would otherwise drift under its old digest. The machine's own move is the user's `sudo` at an hour
  when no runner is mid-run, told to the library seat in advance (their CI pins by the upstream tarball, their local
  workers by a private prefix), and every other worktree on the machine refuses by the pin until it is rebased. And a
  system-wide `apt` install of the release can be IMPOSSIBLE on a desktop: the 32-bit Mesa drivers hold `libllvm20:i386`,
  apt.llvm.org ships no i386, and a multi-arch library must stay at one version -- so the release goes in as a PREFIX (the
  same signed packages, `dpkg-deb -x`, the `~/.local/bin` links repointed, no sudo), which is how this machine moved; the
  engines then build against PATH's `llc` (engines.sh's default), never against `/usr/lib/llvm-20` by name.

- **A struct literal that writes several `sealed` fields from outside their module is ONE report** (D-337, landing 100):
  `type_struct_literal` collects the sealed fields the literal writes and reports once at the literal, naming them all, with
  the "declared `sealed` here" note under each (`note_sealed_decl`); a literal writing one keeps the per-field sentence, and
  every other write form is one report per write. When a rule is "one mistake, one report" (D-240), ask which NODE the mistake
  is: the literal, not its fields. And a rejection file's header is MEASURED after the file has its final line count -- a
  header written against a placeholder of another length, or shortened by a pair of lines, shifts every `expect-error-at`
  by its own change (found twice at this landing: a 4-line placeholder replaced by 14 lines moved five sites by ten, and two
  lines dropped from `list_fields.npk` moved eleven sites by two; D-332's count tool counts per CODE and cannot see it).

- **A refusal at the lexer's or the parser's seam is ONE report** (DEF-164, landing 101; D-240): a refused numeric literal
  stays a LITERAL token (`IntLit` with value 0, `FloatLit` with its text) so the parser has an operand and says nothing; a
  float's dangling exponent sign before `;` `)` `]` `}` `,` or the end is the refused literal's tail (`1.5e+`), while `3.5e-x`
  stays a literal, a minus and a name; a DECLARATION's recovery (`p_recover_decl`) skips to a sync point -- `;`, `}` or a
  declaration keyword -- never to an identifier the broken declaration left behind (`error:E(P);` was reported at the `(`
  and again at the `)`), where a statement's recovery (`p_recover`) still stops at anything that can start an item; and a
  trait-signature comparison says nothing about a signature holding a refused type (`sig_holds_invalid`: TYPE-001 was
  also TYPE-014). A parse refusal's count is tested in a frontend UNIT (`parse_one_report.npk`), never a rejection file
  (D-085: the grammar stage parses every tree file).
- **A `fixed` parameter is written by its caller and never by its body** (DEF-248, landing 102): `n = …` and `n += …` on a
  `fixed int32:n` parameter are ASSIGN-002, as a `fixed` local's second write is; a plain or `move` parameter is re-assignable
  (DEF-124). The lesson is D-085's again: the arm's comment said "a parameter carries no `fixed`" and the bit had been there
  since 0.7.3 -- when a comment claims a flag is absent, grep the flag's readers before believing it. And a `fixed` VIEW's
  bytes travel by no qualifier until D-348 step (ii) lands (DEF-246: a view ranged from, copied out of, or assigned from
  `fixed` storage, or handed through a `Writer` impl that dropped the trait's `fixed`, writes it -- two of the six faces fault):
  until then write through no view of anything `fixed`, and spell `write`'s parameter as the trait does.
- **`fixed T[]` is a TYPE, the read-only view, and a view's rights travel by its type** (D-348 (ii), D-350, D-351;
  landing 103): `fixed uint8[]:v` means what it meant -- written once, read-only through it -- and `v`'s TYPE is
  `fixed uint8[]` wherever `v` goes: a plain `T[]` converts to it at any slot (an argument, a field, a return, a
  `List<fixed uint8[]>` element) and it converts back to nothing (TYPE-007 with the rule; `=>`/`=>!` TYPE-032), a
  write through it is TYPE-086 and an address of its element TYPE-071 in whatever slot it sits, a range of `fixed`
  storage and a slice read off a `fixed` struct are produced AS the view, `string_bytes` returns one (D-351: a
  reader of the bridge declares its slot `fixed uint8[]`; seven of the tree's own did), and an impl's slice
  parameter is `fixed` exactly where its trait's is (TYPE-014). In a bare type position -- a type argument, a function
  type's parameter, a return type -- `fixed` precedes a slice type only: `fixed int32` there, `stack` on a return and
  `fixed` on a cast target are PARSE-013 (DEF-247: they parsed and meant nothing). The type is `TY_SLICE` with its `b`
  slot set (`tt_slice_fixed`, `type_slice_fixed`): every kind-keyed walker keeps its answer, and interning one more
  type in the prelude shifts every type id after it -- the emission comparison renumbers (`tidnorm.py`). A
  re-pointable binding of a read-only view is not expressible (R1): walk a view by an offset, or `for`.
- **A write or an address into something that is not a place is ONE report** (landing 103; D-240): `require_place`
  and `require_place_operand` answer false after TYPE-024 and their callers stop -- `(raw f(v))[0i64] = b` over a
  read-only view was TYPE-024 and TYPE-086 for an hour. When a new rule asks a question of a target, ask whether the
  target's own refusal comes first.

- **A `frac`'s parts are its readable parts, and the prelude is regenerated after any edit** (D-347, DEF-231;
  landing 97): whole and num share a sign, |num| < denom, value = whole + num/denom always -- so −1 3/8 is
  {−1, −3, 8}, prints "-1 3/8", `.whole` is −1 and `=>! int` is the whole field; one value has one form, so a
  derived `Hash` and `==` agree. `src/prelude/prelude.npk` is EMBEDDED through the generated
  `src/frontend/prelude_source.npk`: run `python3 bootstrap/generator/gen_tables.py` (no arguments; `--check` only
  checks) before building after any prelude edit, or the compiler carries the old prelude in silence -- the
  landing's first build measured the old behaviour that way.
- **A harness and a manifest re-record in one worktree RACE; the harness starts after the record, on the committed
  final tree** (landing 95's first harness, 2026-10-08): the harness's `verify` stage compares the compiler's rows
  with `nitpick.obligations` AS IT READS IT, and `npkg verify --record` rewrites that file -- so a record step that
  lands under a running harness reds the stage on every re-keyed row (188 rows there: landing 94's rows re-keyed
  each way, which the record's own gate had already read as zero moved) and leaves `parity` with no verified
  compiler. A red of that shape is READ, not re-run blind: its rows are the gate's re-keyed rows exactly, or
  something else moved. Sequence a landing: chain, record, in-process checks, COMMIT, then the harness on that
  commit -- a docs-only amend under a running harness is harmless, a `src/` or manifest change is not.
- **A prelude edit moves EVERY program's emission, through D-179's site table** (landing 97; DEF-242): each
  emission carries the prelude's WHOLE error-origin table (`@npk.sitep.N`, `@npk.site.paths`, `@npk.site.lines`;
  some 290 prelude sites, referenced or not), numbered before the program's own sites, so a guard added to the
  prelude shifts every program site's id and a line added shifts every prelude site's line below it -- 561 of 561
  programs "different" at landing 97, whose frac section gained six guards and 27 lines. Read such a comparison
  with the table canonicalised first (`python3 meta/roadmap/1.6/tools/sitenorm.py BASE_PRELUDE NEW_PRELUDE
  PAIRS_DIR .base.ll .new.ll`: ids to path and line, prelude lines mapped through the edit); the residue is what
  the landing changed (97's: the frac programs). The same growth moves every verify row's SITE KEY (node ids) while
  the problem hashes stand: compare `rows.txt` by its hash column, and an `.smt2` without its `;` comment lines.

- **A `fixed` slice is read-only through it** (D-348 (i), DEF-230; landing 98): `fixed uint8[]:v` cannot be
  written through (`v[i] = b`, `v[i] += 1u8`, `v[lo...hi][i] = b` are TYPE-086), its elements have no address
  (`@v[i]`, `$$m v[i]`, `v[i].bump()` are TYPE-071), and the same holds for a slice held in a `fixed` aggregate or
  declared a `fixed` field -- `place_fixed`'s walk goes through a slice base to the view's root (a pointer and a
  handle still stop it; `place_through_slice` picks the view's sentence). Read it, range it, pass it on; a callee
  that writes takes a plain `T[]`. Handing a `fixed` view to a plain parameter still compiles until D-348's step
  (ii) makes `fixed T[]` a type: do not rely on `fixed` at a call site to protect bytes a callee declares plain.

- **An `exit` operand is held to `int32` as any slot's value is** (DEF-249, landing 104): `exit may(x);` of a fallible
  `may`, `exit 5i64;` and `exit true;` are TYPE-007 at the operand, in `main` and `failsafe` alike -- unwrap first (`?!`, `?|`,
  a `pick`). Two lessons. SETTING `e.expected` types an operand under an expectation and holds it to nothing: every site that
  sets it owes a check after `type_of_expr`, or the emitter meets a type the statement never promised (here `llc` refused the
  `Result`'s pair where an `i32` stands -- loud, in the wrong phase); grep the `e.expected =` sites when adding a statement kind.
  And the check is `fits` ONLY where the emitter converts: `fits` admits the `Optional` wrap, the `dyn` coercion, the read-only
  view, and `check_slot_sites_agree` (harness.py's SLOT_SITE_PAIRS) refuses a `fits` site with no `emit_fit` partner -- a slot
  that admits no conversion compares exactly, as a condition's `!= t_bool` does. And D-240: an entry point whose signature is
  refused (TYPE-083/TYPE-044) keeps that one report; the operand that follows the wrong signature is not a second mistake.

- **An address-taken binding drops unconditionally at its scope exit** (DEF-159, landing 105): once `@x`, `$$i x`/`$$m x`
  or a pointer-receiver call has taken a flagged local's address, its scope-exit drop is the bare `call @"npk.drop.<tid>"`
  -- the flag a move, a pass or a destroy clears stays the fast path only for a binding nobody addressed. What makes the
  bare call sound is the slot's VACANCY (DEF-120, DEF-158): every site that clears a local's flag vacates the slot first
  (`clear_root_flag` has two callers, `emit_move_out` and `destroy_vacate`, and both do), so a NEW flag-clearing site owes
  the vacate too, and a new address-producing lowering owes a `mark_root_taken`. An emission comparison across a change to
  the drops is READ with `meta/roadmap/1.6/tools/dropnorm.py` (the flag tests stripped from both sides, temporaries and
  labels renumbered); a raw diff shows every later temporary renamed.

- **An enum values every variant or none, and a macro parameter cannot name what the body emits** (D-342 R1, D-343;
  landing 106): `{ A; B; Late = 9i32; }` is TYPE-093 at `Late` -- write `A = 0i32; B = 1i32; Late = 9i32;` or no values
  (an enum with a payload variant values none: a payload variant takes no value) -- and `macro:m = (N) { func:N = …; };`
  is MACRO-011 at the declaration (the argument would be dropped and the function literally called `N`). Two tooling
  lessons from the landing: a rejection file's header is REGENERATED from the checker's own report to a fixed point
  (`.internal/handoff_34/tools/header_from_check.py`: a header that grows moves every site's line, so one pass is never
  enough), and the tool must read the checker's STDERR -- its first run saw no sites and wrote an empty header.
  `tests/grammar/` is parse-only: a construct the CHECKER refuses may stay there as a grammar sample (`whole_grammar.npk`'s
  mixed `Net`), since the sweep's checker never reaches its enum.

- **A builtin is called, never named as a value** (DEF-243, landing 107): `func int64() never fails:f = mono_now;`,
  `call_it(mono_now)`, `Holder{ f: mono_now }`, `pass mono_now;` are TYPE-054 at the name -- write a function that calls the
  builtin and name that. In the checker, a type of 0 means "refused and already reported" (D-240), and `type_ident`
  answered 0 for a bare builtin's name with NOTHING reported: a slot's fit then asks nothing more, and the shape reaches the
  emitter. When a typer returns 0, ask who reported. And a call's callee must never reach `type_ident`: the first form of
  this rule refused every builtin CALL in the prelude, which the test's own `mono_now()` control and a prelude-heavy program
  showed before the chain ran -- a new rule about a NAME is measured on a program full of that name's ordinary uses first.

- **A module binding may be initialised from another `fixed` module binding of ANY type** (DEF-202, landing 108):
  `fixed Pt:Q = P;`, `fixed int32[3]:B = A;`, `fixed int32?:O2 = O;`, `Box{ at: P, dims: A }` and a reference to an
  inline module's `pub fixed` compile; the renderer copies the target's initialiser into the reader's constant (LLVM has
  no constant that is another global's VALUE). Two lessons. A walk that answers "what does this name stand for" belongs
  in ONE function every consumer reads (`fixed_global_sym`): the folder had it as a private step, and the gate and the
  renderer asked the folder for a VALUE instead -- which it holds for a scalar and a string only -- so the refusal's own
  sentence listed the form it refused, since D-165 landed (1.0.9d). And when a gate admits a reference, ask who reports the TARGET'S
  mistake: its own declaration does, so the reference answers "constant" and D-240's one report stands at the binding
  that holds the mistake -- following the reference into the gate would have reported one mistake twice.

- **`failsafe`'s pick may stand in a bare block, a statement macro's expansion among them** (DEF-224, D-352; landing 109):
  `func:failsafe = int32(Error:e) { #shared_failsafe(e); exit 9i32; };` compiles with the macro's pick, and so does
  `{ { pick (e) { … } } }`; a pick inside an `if`, a `when`, a loop or another pick's arm is REACH-001 as before, because
  the path that skips it reaches no arm. REACH-002 asks the arm contract of the pick where it stands, so a shared macro
  names what EVERY program using it reaches -- the union; MACRO-009's note points the report into the macro body. The
  lesson is D-085's cousin: "must contain" was implemented as "among the body's own statements", and the one shape the
  reference's own text invited (an expansion is a block) was refused from the day D-340 let a probe reach it (landing 92). When an analysis walks a body for ONE
  construct, ask what else the grammar lets stand between the body and the construct, and whether that thing changes
  the construct's meaning (a bare block does not; a conditional does).

### Reserved words that read like ordinary names

Each of these has cost an edit-build-fail cycle, because the error arrives as a
parse failure some lines away from the mistake:

| Looks like a name | Actually |
|---|---|
| `pid`, `tid`, `fd`, `uid`, `gid` | the five kernel identifier **types** (D-042) |
| `limit` | the verification keyword (`limit<Rules>`) |
| `any` | the type |
| `as` | a keyword |
| `comptime`, `derive` | keywords — so `mod:comptime;` and `mod:derive;` are not modules, and the loader reports `NITPICK-RESOLVE-005` at the `mod:` line as though the file were missing |
| `move`, `buffer`, `raw` | keywords that read like ordinary local names |
| `assoc` | the associated-type keyword (D-160) — so `bool:assoc;` is not a field |
| `on` | a keyword — so `Node?:on = nd;` is parsed as the expression `Node ? …` and fails at the `:` |
| `is_err`, `defaults`, `any` | keyword forms (`is_err(x)`, D-096), a struck-but-reserved operator word (D-167), and the type — each has cost a build as a local name |
| `channel`, `atomic`, `thread`, `joins` | the constructor, the type, the function modifier and the contract clause (D-181/D-182) — `thread` in particular reads like an ordinary noun |
| `error` | the declaration keyword (D-179) — so `error` is not a local name, and `Result`'s field is `.err` |
| `gives` | the factory contract clause (D-183 1.2.6) — a channel-returning creator hands its channels' reclaim to the caller |
| `Mutex`, `Guard`, `acquire`, `RwLock`, `RGuard`, `CondVar`, `Barrier` | the sync primitives' type keywords (D-056, 1.1.11) — `acquire` interns itself only after a `.` |
| `unit` | the unit-declaration keyword (D-196, 1.3.3) — `unit:Hertz = 1 / Seconds;`; it reads like the most ordinary local name in any measurement code |
| `trit`, `nit` | the single-digit ternary/nonary type keywords (D-197, 1.3.4) — like `acquire`/`any`, each interns itself as a NAME only after a `.` (the digit extraction `t.trit(i)`) |
| `oflags`, `prot`, `mflags`, `fmode` | the four flag-family TYPE keywords (D-044/D-230, 1.4.8) — `prot` and `fmode` in particular read like the most ordinary locals in any file code; their members (`O_RDONLY`, `PROT_READ`, `MAP_SHARED`, `S_IRUSR`, …) are prelude constants, so those names are taken too |
| `fails`, `end` | the `never fails` contract clause's second word (D-002/D-163) and the `when`/`then`/`end` control-flow family's terminator (LEXICAL_REFERENCE's keyword table) — each cost the 1.4.8 executor a build |
| `in`, `mod` | the `for … in` keyword and the module-declaration keyword (`mod:name;`) — each cost the 1.5.0 executor a build, as a local named `in` (a byte source) and one named `mod` (a module name) |
| `old`, `result`, `pure` | the verification keywords 1.5.1 added (D-243, D-245, D-242): `old(expr)` is a keyword operator (the value at entry), `result` a leaf keyword (the success value, `ensures` only — `Result` with a capital R is the type, and `result`'s token is `KwResultValue` for that reason), `pure` a contract clause. `old` was a local in the SMT encoder and a test, `result` a field in `FnSig` and four unit-test fixtures — every one renamed; each reads like the most ordinary local name there is |
| `decreases`, `unbounded` | the termination clauses of D-304 (1.5.8c): `while (c) decreases n - i { … }` says why a loop ends, `while (c) unbounded { … }` that it may not. Keywords from 1.5.8c step 1 (the codes TYPE-072…075 are declared at step 0); until then `unbounded` was a function name in one test. Both read like ordinary names — a budget, a queue, a flag |
| `sealed`, `hidden` | the field qualifiers 1.5.8b step 1 added (D-313, D-314): `sealed int64:bal;` is written only by the declaring module, `hidden` is not touched outside it. Both read like ordinary names — the compiler had a local and a field named `sealed`, seven tests an inline module named `hidden`, and five more a function or a local named `hidden` (all renamed: `after_wildcard`, `sealed_any`, `nested`, `secret`) |

The worst offenders are **gone**: before D-147 (0.9.9) the balanced and hex
literal forms could begin with a letter, so `an`, `bn`, `cn`, `dn`, `tt`,
`ban`, `FFhex` were numbers, and each cost an edit-build-fail cycle when used
as a name. Now **every numeric literal begins with a decimal digit** — those
are ordinary identifiers, and the values ride a value-neutral leading zero
(`0dn` is −4, `0FFhex` is 255). The legacy `0x`/`0b`/`0o` prefixes were
removed by the same decision.

Three more shapes that are not what a C or Rust habit expects:

- **Adjacent string literals do not concatenate.** `"a" "b"` is two literals, not
  one; use `string_concat`.
- **`discard(expr);` and `defer { … }`** take parentheses and no trailing
  semicolon respectively — `discard x;` and `defer { … };` are both parse errors.
- **A file's `mod:` name must match its basename**, or the loader reports
  `NITPICK-RESOLVE-012` at line 1 — "file `other.npk` declares `mod:hello;`
  first: a file's header names the file" (D-248). (This said RESOLVE-005, the
  missing-file code, until the install README's VM test measured it, 2026-10-08.)

**`npkc` now means `src/npkc.npk`** — the harness builds and runs it for every
backend stage (IR on stdout; `llc` and `ld.lld` after, per BUILD_REFERENCE §4).
The `npkc` on PATH (`/usr/local/bin/npkc`) is still the *installed* C/C++
prototype's compiler, not this project's output; nothing installs ours yet.
(The prototype's source is `../ARCHIVE/nitpick-prototype/`.)

```
src/          # THE COMPILER — Nitpick source only; nothing else belongs here
  frontend/   #   built once, in full (analysis/, macro/)
  backend/    #   grown rung by rung (ir/, layout/)
  driver/     #   manifest, module graph, subprocess invocation
bootstrap/    # seed/ — THE COMMITTED SNAPSHOT: stage1.ll + STAMP + README (D-203).
              #   This is what builds src/ since 1.4.6; read seed/README.md
              #   before touching it. generator/ made the FIRST one and builds
              #   nothing now; harness/ runs beside `npkg` until meta/SWITCH.md
              #   retires it (D-206) — parity is a stage on every full run.
runtime/      # npkrt.ll — the runtime FLOOR, hand-written LLVM IR, PERMANENT
              #   (D-203). In every artifact; re-homed out of bootstrap/ at 1.4.6
tools/        # check/resolve_check/parse_check — the real frontend, for the harness
tests/        # FIVE rejection suites, named by the stage that refuses:
              #   modules/rejection/ (loader), types/rejection/ (type checker),
              #   analysis/rejection/ (a static analysis), expansion/rejection/
              #   (macro expansion), derive/rejection/ (the derive reader);
              #   the backend-rung suite retired at 1.5.4 when the last rung
              #   fell (S-47, D-271); nitpick.toml's [[test]] table is
              #   the one list of suites both runners read (D-238)
              #   accept/ is ONE suite for all of them — silence has no stage
              #   conformance/ (subset 1 compiles and runs), frontend/, grammar/
meta/specs/   # language specs — see below
meta/roadmap/ # the plan; meta/roadmap/done/ archives completed cycles
meta/LAYOUT.md# the tree, and why it departs from ../ARCHIVE/npkc-native
.internal/    # gitignored scratch area — never commit anything from here
```

**`src/inc/` is gone.** It was listed as "shared headers / includes"; Nitpick has
modules, not headers. See `meta/LAYOUT.md` for that and the four other departures
from the `npkc-native` decomposition, each with the decision that forced it.

`meta/specs/` holds ten `.md` reference documents carried over from
`../ARCHIVE/nitpick-next/meta/specs/` (the Gemini experiment), plus two written here:

- `PROTOTYPE_DELTA.md` — what changed between the prototype's specs and these,
  and which questions the carried-over set leaves open.
- `PRE_PLANNING_REVIEW.md` — safety concerns, cross-document contradictions,
  missing specs, and a suggested decision order. **Read this before planning
  implementation work.**

⚠️ **The carried-over specs contradict each other in several places** and have
not yet been reconciled — they were written for a separate experiment and some
content came from a verbal retelling of prototype-vs-new differences. Do not
treat any single one as authoritative without checking `PRE_PLANNING_REVIEW.md`
Part 3 first. Notably, the memory model (GC vs RAII) is **an open decision**, not
settled fact, despite `SPEC_GAPS_AND_AMBIGUITIES.md` reading as resolved.

## Why this language exists: Nikola

Nitpick will be released publicly for general safety-critical use, but that is
not its primary purpose. Nitpick is the **host language for Nikola**, a
physics-based AGI, and essentially every unusual decision in the language traces
back to Nikola's requirements. Without this context most of the pedantry looks
like over-engineering; with it, it is load-bearing.

**Nikola's intended users are why the safety bar is where it is.** The primary
use case is a companion for neurodivergent children, extending later to children
in long-term hospital stays, and eventually a teacher's-assistant role where each
student gets a tutor that can also help with homework at home. Several of these
goals involve **robotics**.

Repeated safety reviews of the engineering documents surfaced the finding that
drives the design: **small drift in numbers can produce behavior resembling PTSD
or schizophrenia**. Around vulnerable children that is categorically
unacceptable, and preventing it outranks schedule and effort.

Two language features follow directly:

- **`Result<T>` everywhere, no exceptions.** Errors are values the caller is
  forced to handle.
- **`exit` only from `main` or `failsafe`.** Anything uncaught must be caught by
  the runtime and routed through `failsafe` so shutdown is *controlled*. An
  uncontrolled stop with actuators live is a physical safety event, not a
  debugging inconvenience.

This is also the second, stronger reason for the zero-dependency rule below:
**past the FFI barrier the runtime cannot intercept a fault** and route it
through `failsafe`, which breaks the controlled-shutdown guarantee outright.

**Performance is a first-class requirement** — Nikola is computationally enormous
and will not reach intended speed until purpose-built hardware exists;
demonstrating viable performance is what funds getting there. **But performance
is explicitly subordinate to safety.** Never trade a safety property for speed.
Raise the tradeoff instead.

When a safety mechanism looks excessive or redundant, preserve it. The standing
instruction is that these requirements remain as they are or become *more*
pedantic if required.

## The hard constraint

Nitpick is a safety-critical language, and this compiler is subject to formal
verification requirements. Consequently:

**No external dependencies. No C, no C++, no Rust, no Python, no third-party
packages of any kind.** Everything in the trusted computing base must be
verifiable, and an unverified third-party toolchain or runtime breaks that
guarantee.

This is not a stylistic preference and it is not negotiable. When a task appears
to need a dependency, the correct response is to surface the tradeoff and design
an in-house replacement — never to quietly add one. Expect a large share of the
low-level work to be hand-written LLVM IR, which is the level at which the
project can do systems work without inheriting a runtime.

The build-out is therefore much larger than a compiler of comparable scope would
normally be. The prototype (see `../ARCHIVE/nitpick-prototype`) exceeded 50k lines *with* heavy
C/C++ dependency use; this implementation is expected to be bigger precisely
because those dependencies are being replaced with verifiable in-house code.

## Bootstrap strategy: the capability ladder

The frontend is built **once, in full**. The backend is grown **incrementally**,
rung by rung. The entire point of this arrangement is to avoid rewriting the
parser at every bootstrap stage — a failure mode that the predecessor efforts hit
repeatedly.

**"Built once" means do not REBUILD — not do not extend.** The failure this was
written against is concrete: the previous attempt's parser supported only certain
keywords at each step, so every rung meant going back and teaching it more
syntax. The fix was to have the parser accept the whole language and forward what
it does not yet understand, letting the BACKEND carry the incompleteness as a
named refusal. That is where the ladder lives.

Practical consequence when proposing changes: treat the frontend as the stable
component and the backend as the part that advances. A change that would require
reworking the lexer/parser/AST **to unblock a backend rung** is almost always the
wrong shape — the rung should refuse by name instead. But **adding a production
because the LANGUAGE genuinely needs one is ordinary work**, not a violation of
this; judge it on its real costs (node-kind coverage across every walker,
verification surface, downstream obligations). Raise it either way, because a
grammar change is a language change and those are the user's call.

## Memory model

The language has four allocation regimes, spelled as modifiers in
source. **`DECISIONS.md` is the authority here**, not
`../ARCHIVE/nitpick-prototype-docs/specs/memory_specs.txt`, which still describes a collector this
language does not have:

| Modifier   | Regime                                                      |
|------------|-------------------------------------------------------------|
| *(default)* | Managed — static ownership, RAII at scope exit               |
| `stack`    | Stack-scoped                                                 |
| `wild`     | Unmanaged / manual (paired with `defer` blocks and `nodrop`) |
| `wildx`    | Executable memory — W^X, backs the JIT                       |

**There is no `gc` and no tracing collector.** D-003 dropped both: static
ownership covers unique and scoped data, and **arenas with `Handle<T>`** cover the
graph-shaped and cyclic data a collector would otherwise be needed for. `gc` is
not a keyword in the lexer.

This table listed `gc` as a fifth regime until cycle 0.5.3, and the default row
read "implicit GC / RAII" — both carried over from the prototype, both
contradicting a decision settled long before.

Anything touching allocation, lifetimes, drop semantics, or codegen for
references must be reasoned about against **all** of these, not just the one
being edited. `wildx` in particular carries the W^X invariant: a page is never
simultaneously writable and executable, and the JIT depends on that transition
being correct.

## What this replaces, and what must change

`../ARCHIVE/npkc-native` is the direct predecessor and the most useful structural
reference: a self-hosted Nitpick frontend (`.npk` sources) organized as
`src/frontend/`, `src/backend/`, `src/driver/`, `src/tools/`. Its module
breakdown — `lexer`, `parser`, `type_system`, `type_checker_*`, `borrow_checker`,
`symbol_table`, `module_{table,resolver,loader}`, `diagnostics`, `source_location`
— is a reasonable starting decomposition.

**But its backend is exactly what this project exists to eliminate.** `npkc-native`
reached the C++ nitpick backend (LLVM 20, Z3, IKOS) through an FFI bridge
(`src/backend/ffi_bridge.npk`). That bridge, and everything behind it, is
disallowed here. Read `../ARCHIVE/npkc-native/MAPPING.md` for the frontend
decomposition; ignore its backend arrangement.

One transferable frontend technique documented there: Nitpick has no OOP
inheritance, so the C++ AST class hierarchy is expressed as tagged enums over
composable structs rather than base/derived nodes.

## Reference material (read-only, archived)

**Moved to `../ARCHIVE/` on 2026-09-02** — the user's tidy-up executed the
repository half of `meta/SWITCH.md` early: the prototype is archived on GitHub as
`alternative-intelligence-cp/nitpick-prototype`, its documentation as
`nitpick-prototype-docs`, the rest archived or deleted. All of it stays
browsable and must never be modified:

- `../ARCHIVE/nitpick-prototype-docs/specs/` — the PROTOTYPE's language
  specification, split by topic (`memory_specs.txt`,
  `formal_verification_specs.txt`, `safety_systems_specs.txt`,
  `compiler_specs.txt`, `pointer_system_specs.txt`, `traits_oop_specs.txt`, …).
  `FULL_specs.txt` is the ~14k-line consolidated version. **It was the authority
  on language semantics when this repo opened; `meta/specs/` and `DECISIONS.md`
  are the authority now**, and where they disagree with it the difference is
  recorded in `PROTOTYPE_DELTA.md`.
- `../ARCHIVE/nitpick-prototype-docs/reference/COMPILER_ARCHITECTURE.md` —
  pipeline walkthrough for the C++ prototype (preprocessor → lexer → parser →
  type/borrow check → IR gen). Good for *what the stages do*; its
  implementation is dependency-laden and is not a model to copy. The same
  directory has `TYPE_SYSTEM_DESIGN.md`, `TRAITS_AND_BORROW_SEMANTICS_RFC.md`,
  `UNDEFINED_STATE_PREVENTION.md`, `GC_TUNING_GUIDE.md`, `abi.md`,
  `RESERVED_WORDS.md`.
- `../ARCHIVE/nitpick-prototype/` — the ~26k-file C/C++ prototype compiler.
  Useful as a behavioral oracle; its dependency choices are **not** precedent.
- `../ARCHIVE/nitpick-proofs/` — verification harnesses (`esbmc/`, `frama-c/`,
  `smt/`).
- `../ARCHIVE/nitpick-bootstrap/`, `../ARCHIVE/nitpick-next/` — earlier
  bootstrap attempts; `../ARCHIVE/npkc-native/` — the frontend decomposition
  above.

## Ecosystem conventions

- Source extension is `.npk`; package manifest is `nitpick.toml`.
- `npkc` is the compiler, `npkpkg` the package manager. Both resolve on PATH
  today (`/usr/local/bin/`) as the prototype's INSTALLED binaries, so treat
  them as the *old* compiler, not this one.
- **The repository is `alternative-intelligence-cp/nitpick`** (renamed from
  `nitpick-native` on 2026-09-02; the local checkout is renamed at the 1.4
  close). Everything lives under that organisation, never under a personal
  account. `alternative-intelligence-cp/nitpick-docs` exists again, empty: the
  home of the OFFICIAL documentation (HTML, man pages, Markdown), built after the
  compiler is finished — until then `meta/specs/` is the specification.
- **LLVM 20.1.8** is the toolchain (D-349, 2026-10-08: the LLVM 20.1 release the
  distributions serve; 20.1.2, the version the prototype targeted, from 1.4.5 until
  then). ON THIS MACHINE IT IS A PREFIX, `~/.local/llvm-20.1.8/usr/lib/llvm-20`:
  apt.llvm.org's signed noble-20 packages (the ten `.deb`s and their index are
  kept in its `debs/`), extracted with `dpkg-deb -x`, because `apt` could not
  install them system-wide -- the 32-bit Mesa drivers hold `libllvm20:i386` at
  the archive's 20.1.2 and the suite ships no i386 -- so `/usr/lib/llvm-20` stays
  the distribution's 20.1.2 for Mesa and everything else. Ubuntu/Mint ship only
  versioned binaries (`llc-20`, `opt-20`, …) because LLVM 14, 18, and 20 coexist
  on this machine, so unversioned names are provided by symlinks in `~/.local/bin`,
  which point into the prefix's `bin` for every tool it has (`llc`, `opt`,
  `ld.lld`, `llvm-config`, `llvm-as`, `llvm-readelf`, …) and into
  `/usr/lib/llvm-20/bin` for the rest (`FileCheck`, `not`, `split-file`, `lldb`);
  `~/.local/llvm-20.1.8/links_before.txt` records where each pointed before. Available
  unversioned: `llc`, `opt`, `lli`, `llvm-as`, `llvm-dis`, `llvm-link`,
  `llvm-config`, `llvm-extract`, `llvm-reduce`, `bugpoint`, `llvm-jitlink`,
  `llvm-mc`, `llvm-objdump`, `llvm-readelf`, plus `FileCheck` / `not` /
  `split-file` for test harnesses. `clang` is on update-alternatives and already
  resolves to 20.
  - Verify with `llvm-config --version` (expect 20.1.8). If unversioned names
    stop resolving, the symlinks are the thing to check, not the packages.
  - `lld-20` is installed and symlinked; `ld.lld --version` reports 20.1.8, so
    the linker is version-matched with the rest of the toolchain.
- Note the naming migration in flight across the docs: older material uses
  earlier project names. Prefer current naming in new code.
