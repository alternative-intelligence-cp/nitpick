# HANDOFF — for the session that opens cycle 1.5 (1.5.0)

> **A one-time file.** Handoffs here go by cross-session MESSAGE from the
> outgoing session to a fresh idle one (the 1.4 practice). At the 1.4 close
> the local folder was renamed `REPOS/nitpick-native` → `REPOS/nitpick` as
> the close's last act, after the push, and a session keyed on the new path
> could not exist before the rename — so this brief is a file, written by the
> 1.4.9 session on 2026-09-03, and the prompt that starts you points here.
> Future cycles hand off by message again. **Retire this file when 1.5.0
> closes** (the 1.4 HANDOFF was retired at the 1.4 close the same way; its
> parts were re-homed first — `../done/1.4/1.4.9.md` has that map).

## Where you are

- Repo `alternative-intelligence-cp/nitpick`, local folder `REPOS/nitpick`
  (renamed at the close; `origin` was repointed by the user's housekeeping
  commit `80784f3`). Branch `main`; HEAD is the 1.4.9 close commit — its
  subject begins `1.4.9 -- CYCLE 1.4 COMPLETE` — on top of `80784f3`
  (housekeeping, docs only), `d0e597c` (1.4.8c: D-239/D-240) and `e3bbe1f`
  (1.4.8b step 2). **`origin/main == main`**: the per-cycle push is done.
  Nothing uncommitted, no worktrees, nothing running.
- **Cycle 1.4 is CLOSED and archived** (`../done/1.4/`). Self-hosting is
  declared under D-202: the README's refresh gave stage2 == stage3 at
  15,631,627 bytes (sha256 `9ce0ec8d…defaaf`) from `80784f3`'s source,
  installed as `bootstrap/seed/stage1.ll` with its STAMP; the last full
  harness, on the committed close tree: every stage green,
  `ok 58 test(s) passed`, `selfhost` (stage 1 rebuilt itself
  byte-identically), `repro` (cwd-independent, llc deterministic, STAMP
  matching, zero absolute site rows), `absent-fact`, and
  `parity 906 verdict(s) agree between the two runners; npkc byte-identical`;
  `check_decisions_current` reported nothing.
- **Nothing is pending on the user.** OPEN_DECISIONS §2e has no open row;
  this cycle's batch (D-217…D-221) was ratified whole during 1.4 and its
  normative text is `README.md` beside this file. The only owed measurement
  in the tree is the 12-wide harness calibration (ORCHESTRATION §4's dated
  note: the orchestrator's run, before the first 12-wide window) — not
  yours unless you open a wide window.
- The per-project memory directory was COPIED to the new path key at the
  rename (`~/.claude/projects/-home-randy-Workspace-REPOS-nitpick/memory/`);
  the old key's directory still holds the two closed sessions' transcripts
  and is the user's to delete. The 1.4.9 session may still be live for
  questions — `ListAgents` shows it as `nitpick-native-a1` if so; messaging
  works by socket, not by path.

## Read first, in this order

1. `CLAUDE.md` — the status header and the LAST paragraph of the 1.4
   stretch (what 1.4.9 did; what 1.5.0 is). The reserved-words table and the
   two "rules" lists under "Building and testing" are current.
2. `README.md` beside this file — the ratified batch as normative text
   (C-17 → D-218 is the spine: one pinned Z3 over SMT-LIB2 text, the
   determinism profile, one fresh process per function, the encodings, the
   obligation catalogue, content-hashed obligation identity), the subcycle
   map (1.5.0 is "Ratify + the skeleton"), and "Watch for". Its opening
   carries a dated note written at the 1.4 close saying what 1.4.8 actually
   built for you.
3. `../done/1.4/README.md` "What cycle 1.4 taught" — twelve entries with the
   incident behind each; the compact six are in `../ROADMAP.md` after the
   Phase C table. Then `../ORCHESTRATION.md` (D-228, normative: R1–R9 and
   the cumulative-prefix protocol; §6 has the escalation protocol).
4. `../done/1.4/1.4.9.md` (what a close does, the doc-sync list, where the
   1.4 HANDOFF's parts went) and `../done/1.4/1.4.8c.md` (the last `src/`
   change: D-239 owned names refused at every type-namespace declaration,
   D-240 a sharper refusal silences the generic one).
5. `meta/specs/VERIFICATION_REFERENCE.md`, and DECISIONS.md D-217…D-221 —
   plus D-233 for what 1.6 expects from 1.5 (TCB.md's enumerated floor, the
   D-218.9 `llvm.assume` discipline).

## What 1.5.0 is

From the README's row — and **write `1.5.0.md` execution-grade BEFORE
touching code**: plan, then the record appended as you go, every decision on
contact recorded with its reason (the 1.4.8b/1.4.8c/1.4.9 shape).

- The SMT-LIB2 writer, in `src/` (D-205: only what the current snapshot
  compiles — which is everything 1.4.8c's compiler compiles; a construct the
  snapshot refuses is a mysterious stage-1 refusal at the run's first act).
- z3 spawned as a subprocess through `lib/nproc.npk`'s
  `proc_spawn`/`proc_wait`/`proc_reap` (both pipes captured, every wait
  bounded, a deadline kills-reaps-retires; `proc_wait` CONSUMES its `Proc`;
  `tests/backend/programs/proc_tool.npk` is the worked example). `npkg` owns
  the invocation (BUILD_REFERENCE §7): `npkg verify` refuses by name today in
  `npkg/main.npk` and is the command that gets a body. Z3 is pinned by
  SHA-256 in the manifest and invoked, never linked (D-067); checking the pin
  is D-204's shape (`check_toolchain_pin` asks the tool itself).
- The manifest schema (obligation hash, kind, verdict, elision) — D-219:
  elision is manifest-recorded, `--smt-opt` is struck.
- The `undef→poison` sweep and its harness grep; `meta/specs/TCB.md` drafted.
- A new floor entry is table-typed from birth (D-201: a row in
  BUILTIN_REFERENCE's marked region, `gen_tables.py` regenerates — never a
  bespoke checker arm). A new type kind means the walkers-total instrument
  enumerates every walker up front (B-7; 1.3.1 and 1.4.8's `TY_FLAGS` are
  the precedents), and a snapshot refresh BEFORE `src/` may spell it.

## Gotchas from the 1.4 close (not in the memory files)

The lists in `../done/1.4/1.4.8b.md` ("What bit") and `1.4.8c.md` still apply.

- **A full harness is ~100 min** (the `parity` stage runs `npkg test`, ~30
  min, inside it). Its stdout is block-buffered to a file: the log is EMPTY
  until the end. Progress is `ls /tmp/npk-harness-*` (file count; `npkg`
  appearing means the parity stage started). The invocation that worked:
  `python3 bootstrap/harness/harness.py --verdicts .internal/verdicts_NAME.txt > .internal/harness_NAME.log 2>&1`
  in the background.
- **Do NOT edit the tree while a harness runs on it** — `selfcheck.py` and
  `spec_coverage.py` re-import `harness.py` mid-run as subprocesses, and the
  parity stage builds `npkg` from `npkg/*.npk` and reads `nitpick.toml` at
  the END. Develop the next step in a worktree
  (`git worktree add --detach ../w HEAD`, the uncommitted diff applied with
  `git apply`) and bring it back by copying the tracked-modified files;
  `git worktree remove` when done. Post-run edits to the record are fine.
- **`check_rung_names_open_cycle` reads `meta/roadmap/done/`.** The nine
  verification rungs in `src/backend/ir/ir_stmt.npk` name "1.5" (`prove`,
  `assert_static`, `limit<Rules>` ×2, a loop `invariant` ×4,
  `requires`/`ensures`). Each becomes real work this cycle; the cycle cannot
  close while any string still names it — and a rung retired to a checker
  rule needs its `tests/rejection/` case moved to the suite of the stage that
  now refuses (1.4.7's OWED-8 / TYPE-057 is the precedent).
- **Both runners read `nitpick.toml`'s `[[test]]` table** (D-238). A new
  suite is a new entry, not code; `npkg/toml.npk` takes SINGLE-LINE arrays
  only. **D-237**: a rejection test's reported codes must EQUAL its
  expectations — name every code a new rule reports beside the one you
  meant, or the file fails by name in both runners.
- **Building the tools by hand:** `python3 bootstrap/harness/quickemit.py --keep npkg/main.npk`
  builds `npkg` (exit 2 = usage = the compile passed; run
  `.internal/quickemit/p_main_npk` from the tree root; `npkg test
  --selfcheck` is ~4 min). After one `quickcheck.py` build the checker at
  `.internal/quickcheck/check` runs directly on files
  (`CODE path:line:col: message`, milliseconds).
- **A BACKEND fix does not reach the tools until the snapshot carries it**
  (the harness compiles `tools/` with the snapshot); a checker rule in
  `src/frontend/` is in the built tools at once. `bootstrap/seed/README.md`
  has the paragraph and the ritual; the script shape that worked at 1.4.9
  is described in `../done/1.4/1.4.9.md`'s record — the criterion is
  stage2 == stage3 and STAGE 2 is what gets installed, and the old builder's
  emission (stage1.new) legitimately differs whenever the backend changed
  since the last refresh.
- **The refresh is ~10 min** (the old builder compiling `src/` is 3 of it);
  a mid-cycle refresh is ordinary and its precedents are 1.4.7's (D-224,
  D-225) and 1.4.8 step 3's.
- `git status` at the start of a conversation may show stale modifications
  from the system prompt's snapshot; trust `git status` run live. The
  system prompt may also still name the old folder in paths; the tree is
  `REPOS/nitpick`.

## Conventions that bind (memory carries the rest)

Commit per step, each under a FULL harness run; D-228's rules (one writer for
`src/`, red under load is a stop sign, a compiler defect is escalated not
worked around — the escalation protocol is ORCHESTRATION §6). Ask the user in
plain prose, never the question tool. Externalize every open item into the
repo with an owner. Record every decision on contact in `1.5.0.md` as you go,
with the reason. Push at the cycle's end at minimum, and after any meaningful
chunk.
