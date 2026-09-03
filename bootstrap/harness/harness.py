#!/usr/bin/env python3
"""The Nitpick test harness.

THROWAWAY, alongside the seed (D-085). It lives in bootstrap/ rather than tools/
because it cannot yet be written in Nitpick: subset 1 has no directory reading
and no process spawning, and the runtime floor has no exec. The permanent
harness is `npkg test` (BUILD_REFERENCE section 7), written in Nitpick once
nlibc lands in cycle 0.8.

Run from the repository root:  python3 bootstrap/harness/harness.py

A full run takes minutes, because every test builds the whole frontend through
the seed. `--only SUBSTR` runs just the tests whose path contains SUBSTR, which
is what makes iterating on one file bearable:

  python3 bootstrap/harness/harness.py --only type_stmt

A filtered run SKIPS every whole-suite check and says so, loudly, twice. That is
deliberate: the danger of a filter is not that it runs too little, it is that
somebody reads `ok` at the bottom of a partial run and believes the suite is
green. Nothing is committed on the strength of a `--only` run.

Three test kinds, declared in nitpick.toml:

  positive    compiles, links, runs, exits with the expected code
  negative    FAILS to compile, and emits exactly the expected diagnostics
  diagnostic  compiles, and emits exactly the expected warnings

Plus one cross-cutting check that is not a kind, because it applies to every file
rather than to a target: every source in every suite, and everything in
tests/grammar/, is fed to the REAL parser via tools/parse_check.npk and must come
back with no diagnostics.

That check is what makes tests/rejection/ mean what D-085 says. Its files must
PARSE and be refused later -- and until 0.2.7 that was asserted against the seed's
parser, the throwaway one, while the rule was written about the real one. A suite
that tests the wrong parser tests nothing.

Expectations live in the file, next to the code, so a test and its expectation
cannot drift apart:

  // expect-error: NITPICK-RUNG-001
  // expect-error-at: 14:9
  // expect-exit: 7
  // expect-no-parse-error

Assertions are on CODES and SPANS, never on message text -- messages should be
free to improve without breaking the suite.
"""

import os
import re
import sys
import glob
import shutil
import tempfile
import subprocess
import tomllib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
# THE GENERATOR IS NOT IMPORTED ANY MORE (D-205, 1.4.6). Five modules were
# pulled in here -- diag, lex, parse, check, emit -- because the seed WAS the
# harness's compiler. It is not, and importing a retired builder just to keep
# `sys.path` interesting is how a dependency survives its own retirement.
# `bootstrap/generator/` still runs, on purpose, as the tool that made the
# first snapshot and as the audit path for pre-switch history; nothing the
# harness does reaches it.

RUNTIME_LL = os.path.join(ROOT, "runtime", "npkrt.ll")

# --- THE SWITCH (D-203/D-205, 1.4.6) -----------------------------------------
#
# `bootstrap/seed/stage1.ll` is the compiler, in IR, committed. It is what
# builds `src/` now; the Python generator built the FIRST one and never builds
# again. Two module-level handles, and the difference between them matters:
#
#   BUILDER   llc + ld.lld over the committed snapshot. A compiler as old as
#             the last snapshot refresh. Its ONE job is compiling `src/` and
#             `tools/` into the compiler under test.
#   COMPILER  what the BUILDER just built out of the CURRENT tree. Everything
#             a test asserts is asserted against this one, because a test that
#             passed against the snapshot would be testing last month's
#             compiler and reporting on today's.
#
# Both are set once per run, before any suite, and stay None when llc/ld.lld
# are missing (the same tools-absent path the suites already take).
SNAPSHOT_LL = os.path.join(ROOT, "bootstrap", "seed", "stage1.ll")
BUILDER = None
COMPILER = None


def build_builder(tmp):
    """The committed snapshot, made runnable. Returns a path or an error."""
    if not os.path.exists(SNAPSHOT_LL):
        return ("bootstrap/seed/stage1.ll is missing -- it IS the bootstrap "
                "(D-203): without it nothing here can build `src/`. "
                "bootstrap/seed/README.md has the refresh ritual")
    b = os.path.join(tmp, "builder")
    r = subprocess.run(["llc"] + LLC_FLAGS + [SNAPSHOT_LL, "-o", b + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "llc rejected the committed snapshot: %s" % r.stderr.strip()[:200]
    r = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", b, b + ".o",
                                                 os.path.join(tmp, "npkrt.o")],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "the snapshot did not link: %s" % r.stderr.strip()[:200]
    return b

# --- the pinned toolchain (D-204, 1.4.5) -------------------------------------
#
# THE MANIFEST IS THE AUTHORITY, not this file. `nitpick.toml`'s `[toolchain]`
# records the LLVM version and the exact flag sets, and every `llc`/`opt`/
# `ld.lld` invocation below is BUILT from these lists — the same shape
# BUILTIN_REFERENCE's marked regions have had since 1.4.2, for the same reason:
# a table nothing consumes is a document that goes stale silently, and this
# project has been bitten by that class often enough to stop writing them.
#
# The version is checked once per run, where the tools are first needed. R-4's
# directive is that a patch release is a breaking change for byte-identity, so
# the comparison is exact and the failure is loud.
def _load_manifest():
    with open(os.path.join(ROOT, "nitpick.toml"), "rb") as fh:
        return tomllib.load(fh)


MANIFEST = _load_manifest()
_TC = MANIFEST.get("toolchain", {})
LLVM_PIN = _TC.get("llvm", "")
LLC_FLAGS = list(_TC.get("llc-flags", []))
LLC_OPT_FLAGS = list(_TC.get("llc-opt-flags", []))
OPT_FLAGS = list(_TC.get("opt-flags", []))
LLD_FLAGS = list(_TC.get("lld-flags", []))

# Files that are imported by another test rather than run on their own.
# `use "x.npk".*` names the dependency, so this is derived, not configured.
USE_RE = re.compile(r'use\s+"([^"]+)"')

# `pub func:TYPE_MISMATCH = string() { pass "NITPICK-TYPE-007"; };`
CODE_DECL_RE = re.compile(r'pub func:(\w+) = string\(\)(?: never fails)?\s*\{\s*pass "([A-Z0-9\-]+)"')


# --- expectations ------------------------------------------------------------

class Expect:
    def __init__(self):
        self.errors = []        # [(code, line|None, col|None)]
        # NOTES ARE ASSERTED SEPARATELY FROM FINDINGS. A note says WHERE to look,
        # not what is wrong -- `NITPICK-MACRO-009` points at the invocation that
        # expanded the body a diagnostic landed in -- so a test that listed one
        # among its `expect-error`s would be asserting the wrong kind of thing.
        self.notes = []         # [(code, line|None, col|None)]
        self.exit = 0
        self.no_parse_error = False
        # `// argv: TOK ...` -- extra argv for the RUN. A token that names a
        # fixture (its basename, uppercased -- MOCK_DRIVER for
        # tests/backend/fixtures/mock_driver.npk) is substituted with the
        # built fixture's absolute path; anything else passes verbatim. This
        # is how a test reaches a helper BINARY without hardcoding a path:
        # `Path` refuses relative paths by design, and a fixed absolute path
        # would bake one machine's layout into the suite.
        self.argv = []
        # HOW MANY TIMES TO RUN IT (1.1.10-C). One run is not a test of a
        # concurrent program: a schedule-dependent bug passes most of the time,
        # and "most" is what makes it survive. Two real defects hid behind
        # single runs of a green suite -- `npk_exit` calling `exit` rather than
        # `exit_group`, so a threaded program's status was whichever thread
        # finished last, and a channel wake landing between registering and
        # sleeping, which the sleeper-push then erased. Neither reproduced in
        # fewer than about twenty runs, and both are gone from 200.
        self.stress = 1

    @property
    def expects_failure(self):
        return bool(self.errors)


def read_expectations(path):
    e = Expect()
    pending_at = None
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            s = line.strip()
            if not s.startswith("//"):
                if e.errors or e.exit or e.no_parse_error:
                    continue
                continue
            body = s[2:].strip()
            if body.startswith("expect-error-at:"):
                loc = body.split(":", 1)[1].strip()
                ln, _, cl = loc.partition(":")
                pending_at = (int(ln), int(cl)) if cl else (int(ln), None)
                if e.errors:
                    code, _, _ = e.errors[-1]
                    e.errors[-1] = (code, pending_at[0], pending_at[1])
            elif body.startswith("expect-error:"):
                e.errors.append((body.split(":", 1)[1].strip(), None, None))
            elif body.startswith("expect-note-at:"):
                loc = body.split(":", 1)[1].strip()
                ln, _, cl = loc.partition(":")
                if e.notes:
                    code, _, _ = e.notes[-1]
                    e.notes[-1] = (code, int(ln), int(cl) if cl else None)
            elif body.startswith("expect-note:"):
                e.notes.append((body.split(":", 1)[1].strip(), None, None))
            elif body.startswith("expect-exit:"):
                e.exit = int(body.split(":", 1)[1].strip())
            elif body.startswith("stress:"):
                e.stress = int(body.split(":", 1)[1].strip())
            elif body.startswith("argv:"):
                e.argv = body.split(":", 1)[1].split()
            elif body.startswith("expect-no-parse-error"):
                e.no_parse_error = True
    return e


# --- compilation -------------------------------------------------------------

# `compile_files` -- THE SEED'S COMPILE PATH -- RETIRED AT 1.4.6 (D-205).
#
# It ran the Python seed over a file group and collected diagnostics. Every
# caller now goes through the compiler under test instead: the seed is not a
# builder any more, and a suite gated on it would be asserting about a tool
# that builds nothing. `bootstrap/generator/` survives as what produced the
# FIRST snapshot and as the audit path for pre-switch history; it is simply
# not in any path the harness takes.


# EVERY UNIT'S VERDICT, recorded beside the failure list (1.4.8, D-206 §5).
# The failure list says what went wrong; this says what was RUN, pass or
# fail, as `(suite, name, ok, message)` -- the thing the parity stage diffs
# against `npkg test --verdicts`, since a runner that silently ran fewer
# units would otherwise agree with this one on every failure and still be
# a different suite. Written out by `--verdicts PATH`, one
# `STATUS<TAB>suite<TAB>name<TAB>message` per line, the same shape npkg writes.
VERDICTS = []


def record_verdict(suite, name, fails):
    VERDICTS.append((suite, name, not fails, " | ".join(fails)))
    return fails


def verdicts_text():
    out = []
    for suite, name, ok, msg in VERDICTS:
        out.append("%s\t%s\t%s\t%s\n" % ("PASS" if ok else "FAIL", suite, name,
                                          msg.replace("\n", " ")))
    return "".join(out)


def read_verdicts(text):
    """The verdict lines as a dict {(suite, name): (ok, message)} -- a repeated
    unit keeps its FIRST verdict. (The hardcoded grammar sweep parsed six files
    twice; since 1.4.8b the table sweeps each file once, and a repeat here
    would be a runner judging one unit twice.)"""
    out = {}
    for line in text.splitlines():
        parts = line.split("\t", 3)
        if len(parts) < 3:
            continue
        key = (parts[1], parts[2])
        if key not in out:
            out[key] = (parts[0] == "PASS", parts[3] if len(parts) > 3 else "")
    return out


def group_for(path, all_paths=None):
    """A test file plus everything it imports, TRANSITIVELY, in dependency order.

    Following `use` only one level deep worked until a frontend module imported
    another one, and then failed as an "unknown name" in a file that looked
    fine. Dependencies are transitive; resolution has to be too.

    Depth-first post-order, so a module is always emitted after everything it
    depends on. Cycles are broken rather than diagnosed -- the seed has no module
    system worth the name, and cycle detection belongs in the real one
    (MODULE_REFERENCE.md).
    """
    seen, order = set(), []

    def visit(p):
        real = os.path.abspath(p)
        if real in seen:
            return
        seen.add(real)
        try:
            with open(p, "r", encoding="utf-8") as fh:
                text = fh.read()
        except OSError:
            return
        for m in USE_RE.finditer(text):
            cand = os.path.normpath(os.path.join(os.path.dirname(p), m.group(1)))
            if os.path.exists(cand):
                visit(cand)
        order.append(p)

    visit(path)
    return order


def imported_by_others(paths):
    """Files that some other test imports, so they are not run standalone."""
    used = set()
    for p in paths:
        for d in group_for(p)[:-1]:
            used.add(os.path.abspath(d))
    return used


# --- checking a single test --------------------------------------------------

def check_positive(name, entry, exp, tmp, tools):
    """Compiles, links, runs, exits as expected -- WITH THE COMPILER UNDER TEST.

    Until 1.4.6 this ran the Python seed. Two reasons it could not stay there,
    and the second is the one that forced the change now:

      - The seed retires as a builder at the switch (D-205), and a suite that
        gates on a retired tool is asserting about something that no longer
        builds anything.
      - `tests/frontend/` and `tests/backend/` IMPORT `src/` modules. The
        moment `src/` adopts a construct outside subset 1 -- which D-205
        permits from 1.4.6 and D-209 schedules for 1.4.7 -- the seed cannot
        compile those imports and 37 tests break for a reason that has nothing
        to do with what they test.

    So the entry file goes to the real compiler, which resolves its own
    imports. `group_for` survives for `imported_by_others` alone.
    """
    if not tools or not COMPILER:
        return []
    base = os.path.join(tmp, name.replace("/", "_"))
    fails = emit_and_link(COMPILER, entry, name, base, tmp)
    if fails:
        return fails
    # A `// stress: N` marker runs it N times and requires the SAME answer
    # every time. These binaries take milliseconds, so a concurrency test
    # earning its keep costs about a second.
    seen = {}
    for _ in range(max(1, exp.stress)):
        try:
            got = subprocess.run([base], capture_output=True, timeout=10).returncode
        except subprocess.TimeoutExpired:
            return ["%s: timed out" % name]
        if got != exp.exit:
            seen[got] = seen.get(got, 0) + 1
    if seen:
        if exp.stress > 1:
            detail = ", ".join("%d x%d" % (rc, n) for rc, n in sorted(seen.items()))
            return ["%s: expected %d every run, got %s in %d runs -- a "
                    "schedule-dependent answer is a bug that passes most of "
                    "the time" % (name, exp.exit, detail, exp.stress)]
        return ["%s: exited %d, expected %d" % (name, list(seen)[0], exp.exit)]
    return []


def check_negative(name, entry, exp, tmp, tools):
    """A correct program the BACKEND must refuse with NITPICK-RUNG-001 (D-085).

    THE PARSER NEVER RESTRICTS; THE BACKEND DOES. The file must pass the whole
    frontend -- parse, resolve, type-check, analyse -- and be refused by
    EMISSION, naming the construct and the rung. A file that fails earlier is a
    test of the wrong stage and reports as one.

    This asserted against the SEED's rungs until 1.4.6, and separately against
    the real compiler's in a `rungs` stage further down. Post-switch those were
    the same question asked twice with one of them aimed at a retired tool, so
    the stage is gone and the suite is here, where nitpick.toml declares it.
    """
    if not exp.errors:
        return ["%s: negative test declares no `// expect-error:` -- a test that "
                "only asserts 'it failed somehow' stops noticing when it starts "
                "failing for a different reason" % name]
    if not tools or not COMPILER:
        return []
    return check_backend_rejection(COMPILER, entry, name, exp)


def check_diagnostic(name, entry, exp, tmp, tools):
    """Compiles, but must emit exactly the expected warnings.

    Nothing emits warnings yet, so no target uses this kind. It is defined
    rather than deferred so the harness shape is fixed before the first one
    arrives.
    """
    if not tools or not COMPILER:
        return []
    base = os.path.join(tmp, name.replace("/", "_"))
    fails = emit_and_link(COMPILER, entry, name, base, tmp)
    if fails:
        return fails
    if exp.errors:
        return ["%s: expected warnings %s, none emitted"
                % (name, [c for c, _, _ in exp.errors])]
    return []


KINDS = {"positive": check_positive,
         "negative": check_negative,
         "diagnostic": check_diagnostic}


# --- every node kind must be reachable ---------------------------------------

KIND_DECL_RE = re.compile(r'^\s+((?:Decl|Stmt|Expr|Type|Verify|Pat)\w+)\s+=\s+\d+i32;',
                          re.M)
KIND_USE_RE = re.compile(r'\b(?:Decl|Stmt|Expr|Type|Verify|Pat)Kind\.(\w+)')


def check_kinds_reachable():
    """Every generated node kind must be one the parser can actually build.

    This is the check that found every defect in cycle 0.2, and it takes
    milliseconds: `IdentifierExpr` missing entirely, `FallStmt` and `GiveStmt`
    declared in prose the generator cannot see, `Attribute` in a section it does
    not read, `FuncType` with no spelling in any document, `cstring` and `any`
    unreachable behind the named-type path.

    None of those announced itself. A kind nothing constructs is not an error at
    any point -- it is simply dead, and stays dead until somebody needs it and
    finds the gap the expensive way. Reading either file alone never reveals it;
    the two lists have to be diffed.
    """
    kinds_path = os.path.join(ROOT, "src", "frontend", "ast_kind.npk")
    with open(kinds_path, encoding="utf-8") as fh:
        declared = set(KIND_DECL_RE.findall(fh.read()))

    # RECURSIVE, because the frontend has subdirectories now. `analysis/` (cycle
    # 0.5) and `macro/` (0.6) hold real compiler modules, and a non-recursive glob
    # would quietly stop counting the kinds they construct -- turning this check
    # from "nothing is unreachable" into "nothing in the top directory is", which
    # is the same sentence with a hole in it.
    used = set()
    for p in glob.glob(os.path.join(ROOT, "src", "frontend", "**", "*.npk"),
                       recursive=True):
        with open(p, encoding="utf-8") as fh:
            used.update(KIND_USE_RE.findall(fh.read()))

    # The reserved zeroes are absences, not constructs, and nothing builds one.
    nones = {"DeclNone", "StmtNone", "ExprNone", "TypeNone", "VerifyNone", "PatNone"}
    unreachable = sorted(declared - used - nones)
    if unreachable:
        return ["node kinds no parser rule can build: %s -- a kind nothing "
                "constructs is dead, and stays dead until somebody needs it"
                % ", ".join(unreachable)]
    return []


# EVERY EXPRESSION KIND THE TYPE CHECKER NEVER NAMES.
#
# `type_of_expr_inner` reaches every kind by name and falls through to type `0` --
# the INVALID type, which exists so that one bad annotation produces one diagnostic
# instead of a cascade, and which is therefore SILENT. A kind with no case is not a
# crash and not a diagnostic: it is a construct the checker accepts without looking
# at it.
#
# Two were found the expensive way. `#name(...)` in cycle 0.6.2 --
# `#totally_not_a_macro(3i32)` compiled clean -- and `Point{ x: 1i32 }` in 0.6.3,
# which accepts a field the struct does not have, a value of the wrong type, and a
# literal that names no field at all. Neither announced itself, and reading the type
# checker alone never reveals it: the kind list and the checker have to be diffed,
# exactly as `check_kinds_reachable` diffs the kind list against the parser.
#
# THE ALLOW-LIST SHRINKS AND NEVER GROWS. Each entry is a construct the frontend
# currently accepts unchecked, with where it is scheduled. Adding to it means
# admitting a new hole, and this exists so that admission is deliberate.
UNTYPED_EXPR_KINDS = {
}


def check_kinds_typed():
    """Every expression kind is typed, or is on the list of ones known not to be."""
    kinds_path = os.path.join(ROOT, "src", "frontend", "ast_kind.npk")
    with open(kinds_path, encoding="utf-8") as fh:
        declared = set(k for k in KIND_DECL_RE.findall(fh.read())
                       if k.startswith("Expr"))
    named = set()
    for p in glob.glob(os.path.join(ROOT, "src", "frontend", "type_*.npk")):
        with open(p, encoding="utf-8") as fh:
            named.update(KIND_USE_RE.findall(fh.read()))

    untyped = sorted(declared - named - {"ExprNone"})
    fails = []
    for k in untyped:
        if k not in UNTYPED_EXPR_KINDS:
            fails.append("expression kind %s is never named by the type checker, so "
                         "it types as INVALID and is accepted unchecked -- write the "
                         "rule, or add it to UNTYPED_EXPR_KINDS with where it is "
                         "scheduled" % k)
    for k in sorted(UNTYPED_EXPR_KINDS):
        if k not in untyped:
            fails.append("%s is typed now, so it comes off UNTYPED_EXPR_KINDS -- an "
                         "allow-list that outlives its entries stops being read" % k)
    return fails


# EVERY DIAGNOSTIC CODE HAS A TEST THAT SHOWS IT FIRE.
#
# A code with no case is a rule nobody has watched refuse anything. It may be
# unreachable, it may report the wrong thing, it may point at the wrong span -- and
# none of that shows up in a suite that never triggers it. Cycles 0.1.6, 0.2.1,
# 0.3.6, 0.4.8, 0.5.7 and 0.6.6 each centralised codes because a mistyped literal
# invents a new one; this is the other half, and it is what makes "every rule has a
# test" a fact rather than an intention.
#
# THE ALLOW-LIST SHRINKS AND NEVER GROWS, like `UNTYPED_EXPR_KINDS`. Two kinds of
# entry, and they are different admissions:
#
#   INTERNAL   a defect in the compiler, not in any program. There is no source
#              that triggers one, and a test asserting it would be asserting that
#              the compiler is broken.
#   SCHEDULED  reachable, and no case yet, with where it lands.
UNTESTED_CODES = {
    # INTERNAL -- a defect in this compiler. No source triggers one.
    "NITPICK-ASSIGN-004":  "internal -- an unclassified node kind in the binding walk",
    "NITPICK-BORROW-008":  "internal -- an unclassified node kind in the escape walk",
    "NITPICK-SUSPEND-001": "internal -- an unclassified node kind in the suspend walk",
    "NITPICK-MACRO-006":   "internal -- a node kind the macro clone has no case for",
    "NITPICK-RESOLVE-009": "internal -- a node kind the resolver has no case for",
    "NITPICK-TYPE-011":    "internal -- a node kind the type checker has no case for",
    "NITPICK-EMIT-002":    "internal -- a node kind the emitter has no case for",

    # AHEAD OF THE LANGUAGE -- the rule is written, correct, and unreachable
    # because the construct it governs does not exist yet. Deliberate, and the
    # reason is the one-shot Astree run: a rule added AFTER the analysis is
    # verified is a re-verification, and this project gets one attempt. Cycle 1.1
    # lowers concurrency and closures, and these become testable then.
    "NITPICK-BORROW-005":  "RETIRED by D-180 -- the blanket borrow-across-await rule's residue is empty: every borrow an async function can spell is frame-resident (the suspend walk marks address-taken locals as crossing, stage D frames them) or crosses a spawn, which is BORROW-004. Kept declared so the reasoning outlives the rule",
    "NITPICK-BORROW-006":  "ahead of the language -- there are no closures yet (cycle 1.1)",

    # OPEN -- a question, not an omission.
    "NITPICK-TYPE-023":    "open -- the C variadic tail, recorded in PROTOTYPE_DELTA",

    # BUILD-MODE -- emitted only under `--extra-picky=no-wildx`, which the
    # flagless rejection suites do not pass. Exercised directly against npkc
    # with the flag (0.10.5); the state-machine codes WILDX-001/002 that the
    # DEFAULT build enforces are both in rejection/wildx.npk.
    "NITPICK-WILDX-003":   "build-mode -- only under --extra-picky=no-wildx",
}


def check_codes_centralised():
    """No diagnostic code is written as a literal at the site that emits it.

    Cycles 0.1.6, 0.2.1, 0.3.6, 0.4.8, 0.5.7 and 0.6.6 each centralised codes for
    the same reason: A MISTYPED LITERAL AT A CALL SITE SILENTLY INVENTS A NEW CODE,
    and the test asserting the old one then fails for a reason unrelated to the bug
    it guards. Six cycles said so and nothing checked it.
    """
    fails = []
    for p in (glob.glob(os.path.join(ROOT, "src", "**", "*.npk"), recursive=True)
              + glob.glob(os.path.join(ROOT, "tools", "*.npk"))):
        if p.endswith("_codes.npk"):
            continue
        with open(p, encoding="utf-8") as fh:
            for i, line in enumerate(fh, 1):
                if '"NITPICK-' in line:
                    fails.append("%s:%d writes a diagnostic code as a literal -- "
                                 "codes live in a `*_codes.npk` module, because a "
                                 "mistyped one at a call site invents a new code "
                                 "and says nothing"
                                 % (os.path.relpath(p, ROOT), i))
    return fails


def check_no_kind_literals():
    """No token-kind VALUE is written as an integer literal outside its table.

    Found at 1.3.3: `bridge_stubs.npk` compared type-name payloads against
    hardcoded numbers annotated `// KwInt32` -- a hand copy of the GENERATED
    TokenKind enum. The `unit` keyword's insertion renumbered every kind
    after it, and the extern wire vocabulary silently began refusing legal
    `int64` returns. The tell is exactly the annotation a conscientious
    author writes when hardcoding: an integer literal explained by a `Kw`
    comment. Compare `TokenKind.KwName` symbolically instead -- the enum is
    regenerated, copies of it are not.
    """
    fails = []
    pat = re.compile(r'\d+i(?:32|64)\b.*//.*\bKw[A-Z]')
    for p in glob.glob(os.path.join(ROOT, "src", "**", "*.npk"), recursive=True):
        if p.endswith("token_kind.npk"):
            continue
        with open(p, encoding="utf-8") as fh:
            for i, line in enumerate(fh, 1):
                if pat.search(line):
                    fails.append("%s:%d compares a token kind by NUMBER -- the "
                                 "TokenKind enum is generated and renumbers; a "
                                 "copied value drifts silently. Compare "
                                 "`TokenKind.KwName` symbolically"
                                 % (os.path.relpath(p, ROOT), i))
    return fails


# Sites where `type_name` legitimately appears with a comparison operator that
# is NOT a type-identity test (the name compared against a sentinel, or two
# names rendered side by side). Empty today: after D-162 the frontend holds no
# `type_name`-as-identity comparison at all.
IDENTITY_BY_DECL_OK = set()


def check_identity_by_decl():
    """A type's identity is its DECLARATION (D-090), never its name.

    The struct-literal typer compared `type_name(want) == name` to decide which
    declaration a literal meant (0.6.7), and it was wrong the day it was written:
    a same-named struct from another module captured the literal, changing its
    meaning with context and silencing the omitted-field check (D-162, 1.0.1).
    The bug was exactly one `type_name` used where a declaration comparison
    belonged. This lists every `type_name(...)` on a line that also carries `==`
    or `!=`, so a new identity-by-name comparison is caught the way the first one
    was not. `type_name` for RENDERING (type_names.npk, the ir_types symbol
    builders) carries no comparison and does not appear.
    """
    fails = []
    for p in glob.glob(os.path.join(ROOT, "src", "**", "*.npk"), recursive=True):
        with open(p, encoding="utf-8") as fh:
            for i, line in enumerate(fh, 1):
                if line.lstrip().startswith("//"):
                    continue
                if "type_name(" in line and ("==" in line or "!=" in line):
                    key = "%s:%d" % (os.path.relpath(p, ROOT), i)
                    if key in IDENTITY_BY_DECL_OK:
                        continue
                    fails.append("%s compares `type_name` -- a type's identity is "
                                 "its declaration (D-090/D-162), so compare "
                                 "`type_decl_of`; allowlist the site if it is "
                                 "rendering, not identity" % key)
    return fails


# THE SLOT SITES, PAIRED (1.0.6e). Every frontend function that asks `fits` --
# "may a value of this type be written where that type is expected" -- against
# the backend function that BUILDS the value into that slot with `emit_fit`.
# `fits` admits two things the argument's own type does not say (a concrete
# value into a `dyn`, a `T` into a `T?`), so a `fits` site with no `emit_fit`
# partner is a slot the emitter stores raw -- which is how an assignment into a
# `dyn` wrote 4 bytes over a 16-byte fat pointer and segfaulted.
#
# A value is a set of backend functions, or a string `refused: <construct>`
# naming the rung string the backend refuses that construct with: the check
# then requires that rung string to still exist, so the day the construct is
# lowered the pairing must be written rather than forgotten.
SLOT_SITE_PAIRS = {
    "check_var_decl":      {"emit_vardecl"},
    "check_assign":        {"emit_assign"},
    "check_pass":          {"emit_pass"},
    "check_args":          {"emit_call", "emit_indirect_call", "emit_method_call",
                            "emit_qualified_call",    # `Trait.method(recv, …)` (D-172, 1.0.9b)
                            "emit_field_call",        # `s.f(x)` through a fn-valued field (1.0.9c)
                            "emit_child_frame",       # await/spawn args, stored into the frame (1.1.4)
                            "emit_dyn_await",         # the same args, into the vtable-sized frame (D-185, 1.1.12d)
                            "va_fill"},               # a variadic tail's elements, fit to the collector's element (D-191, 1.1-close)
    "type_method_call":    {"emit_method_call"},      # the receiver, parameter 0 (1.0.8)
    "type_pipe":           {"emit_pipe"},             # `x |> g` fits x to g's parameter (1.0.9c)
    "check_ctor_args":     {"emit_ctor"},
    "type_struct_literal": {"emit_struct_lit"},
    "type_array_literal":  {"emit_array_lit"},
    "type_result_literal": {"emit_result_lit"},
    "arm_give_type":       {"emit_give"},
    "type_safe_unwrap":    {"emit_expr_kind"},      # `r ? d` lowers inline
    "type_arena_method":   {"emit_arena_method"},
    "type_call":           {"call_builtin"},        # `mutex(initial)` fits the element (D-056, 1.1.11)
    # The channel `send` value, built into the element's slot on the frame
    # before the retry loop (D-182, 1.1.10-B). `recv` builds nothing.
    "type_channel_method": {"emit_channel_await"},
    "type_sarena_method":  {"emit_sarena_method"},
    # The Handle argument is checked by equality; `fits` is used there so the
    # diagnostic is the same one every slot gives. Nothing is built for it,
    # and the arena emitter is where it would be.
    "want_handle_of":      {"emit_arena_method"},
    "type_null_coalesce":  {"emit_coalesce"},          # lowered at 1.0.7
    # The simd constructor's components, fit to the element before their
    # insertelement (D-194, 1.3.1) -- an inline arm, so the enclosing
    # emitter function is the partner, the type_safe_unwrap precedent.
    "type_vector_ctor":    {"emit_expr_kind"},
    # A BUILTIN'S ARGUMENT SLOTS (D-201, 1.4.2). `call_builtin` is where a
    # builtin's arguments are built, and it is already the partner for the
    # annotation-directed constructors' elements. Nothing is BUILT for a regular
    # builtin's argument, and nothing can be: the parameter types come from
    # `builtin_text_type`'s closed table, which has no `dyn` and no `Optional`
    # arm -- the two things `fits` admits that a value's own type does not say --
    # and `check_builtin_sig_texts` fails the build if a signature row uses a
    # text that table does not have. The pairing is written anyway, so the day
    # such a type does appear in a signature the slot has an owner.
    "builtin_arg_fits":    {"call_builtin"},
}


def _sites_by_function(pattern, paths, definition):
    """Every non-comment line matching `pattern`, keyed by its enclosing
    `func:` -- the definition line itself excluded."""
    found = {}
    for p in paths:
        cur = None
        with open(p, encoding="utf-8") as fh:
            for i, line in enumerate(fh, 1):
                m = re.match(r"\s*(?:pub )?func:(\w+)\s*=", line)
                if m:
                    cur = m.group(1)
                if line.lstrip().startswith("//"):
                    continue
                if re.search(definition, line):
                    continue
                if re.search(pattern, line):
                    found.setdefault(cur, []).append(
                        "%s:%d" % (os.path.relpath(p, ROOT), i))
    return found


# EVERY TYPE KIND HAS AN ANSWER FROM EVERY TYPE WALKER (B-7, 1.4.1). A new
# type kind places an obligation on every function that branches per kind, and
# a walker written before the kind falls to its default silently — five of
# 1.1.10-D's seven defects were exactly that (`type_subst` made a generic
# taking a channel uninstantiable; `type_mentions_param` let a channel open
# with a zero-byte element; `field_holds_ptr` called every endpoint-holding
# struct pointer-bearing), and none was found by a test of the feature that
# broke. `check_drops_total` (1.2.0) was the shape applied to ONE walker;
# this is the companion B-7 asked for: every named walker below must mention
# every TY_* kind, or its excuse table must state why the default is correct
# for that kind. An excuse is a claim reviewed once and diffed forever.
#
# `type_drops` (1.2.0): decides whether a value owns anything released at
# scope exit (D-183). A kind it does not name defaults to "owns nothing" —
# a silent leak if wrong, since nothing fails when a drop is not emitted.
DROPS_DEFAULT_OK = {
    "TY_INVALID":   "resolution already failed; nothing was built to own anything",
    "TY_NIL":       "no value, no storage",
    "TY_BOOL":      "scalar", "TY_INT": "scalar", "TY_CHAR": "scalar",
    "TY_FLOAT":     "scalar", "TY_TBB": "scalar",
    "TY_KERNEL":    "a kernel identifier is a number (D-042)",
    "TY_FLAGS":     "a flag set is a word (D-044/D-230)",
    "TY_ERROR":     "a code word, not an address (D-179)",
    "TY_CSTRING":   "the kernel's storage (argv), never ours to free (D-049)",
    "TY_POINTER":   "a pointer is not an owner; `wild` memory is manual, via "
                    "`defer`/`dalloc`, which is what makes the regime explicit",
    "TY_SLICE":     "a borrow (D-070)",
    "TY_HANDLE":    "an index, not an owner (D-152)",
    "TY_CHANNEL":   "an endpoint is an index (D-182); the CHANNEL ITSELF is "
                    "reclaimed by the creating function's exit (D-183 1.2.5) "
                    "-- a creation-site finalizer after the join, not a value "
                    "drop, because endpoints are copyable non-owners",
    "TY_ANY":       "unsized; never a value at this rung",
    "TY_TRAIT":     "a bound, not a value",
    "TY_FUNC":      "a code address", "TY_FUNC_VARIADIC": "a code address",
    "TY_SELF":      "resolves to the concrete type before it is asked",
    "TY_PARAM":     "substituted before it is asked; the specialization answers",
    "TY_ASSOC":     "a projection; resolves to its bound type",
    "TY_RANGE":     "two scalars",
    "TY_COMPTIME":  "a compile-time argument, never a runtime value",
}


# Shared reasons, so a rule stated once is cited rather than restated.
_SCALAR = "plain bits, no address and no handle inside"
_TIER = "a 1.3-tier value: carrier bits or plain components (D-194..D-199)"
_META_CLOSED = ("reaches the walker's STATED conservative tail -- generics "
                "and unresolved kinds answer the closed side")
_LEAF_SUBST = "no type operand: its own substitution (the stated tail)"
_NOT_REGISTERED = ("never registered: `type_drops` answers false, and the "
                   "default now fails LOUD if that ever changes (1.4.1)")

# THE THREE `_recorded` NAMES ARE THE SAME WALKERS (D-227, 1.4.7). They kept
# their bodies and their excuse tables and gained a suffix: the UNQUALIFIED
# names now live in type_layout.npk and ensure the memoised bit is computed
# before delegating here. Only the readers below switch on TY_ kinds, so only
# they carry a B-7 row -- an ensuring wrapper has no kind switch to be total
# over. This instrument is what caught the rename, by refusing to find a walker
# it was told exists, which is the behaviour to preserve rather than relax.
#
# The walkers under the B-7 obligation, each with its excuse table. A walker
# belongs here when a kind it fails to consider produces a SILENTLY WRONG
# default (a leak, a wrong verdict, a wrong layout) rather than a loud one;
# a loud default (ll_broken, an internal-defect trap) still earns a row,
# because the excuse table is where "this kind is deliberately unanswered"
# becomes a reviewed claim instead of an accident. The tables were filled at
# 1.4.1 by READING each walker against each kind -- the first run's 195
# findings classified one by one; the fixes that pass produced are in
# types.npk, escape.npk, type_layout.npk, type_names.npk, type_generic.npk,
# ir_types.npk and emit_program.npk (the TY_ENUM drop arm, a silent leak
# live since 1.2).
WALKER_DEFAULT_OK = {
    # what a value of the kind owes at scope exit (see DROPS_DEFAULT_OK above)
    ("type_drops_recorded", "src/frontend/types.npk"): DROPS_DEFAULT_OK,
    # whether a type can carry a borrow across a boundary (D-004/D-070)
    ("type_contains_borrow_recorded", "src/frontend/types.npk"): {
        "TY_INVALID": "resolution already failed and already said so",
        "TY_NIL": _SCALAR, "TY_BOOL": _SCALAR, "TY_INT": _SCALAR,
        "TY_CHAR": _SCALAR, "TY_FLOAT": _SCALAR, "TY_TBB": _SCALAR,
        "TY_KERNEL": _SCALAR, "TY_FLAGS": _SCALAR,
        "TY_ERROR": "a code word (D-179)",
        "TY_SIMD": _TIER, "TY_TFP": _TIER, "TY_DIM": _TIER,
        "TY_TERN": _TIER, "TY_FRAC": _TIER, "TY_COMPLEX": _TIER,
        "TY_RANGE": "two values of an integer element (D-093)",
        "TY_TRAIT": "a bound, not a value",
        "TY_FUNC": "a code address never dangles",
        "TY_FUNC_VARIADIC": "a code address never dangles",
        "TY_COMPTIME": "a compile-time argument, never a runtime value",
        "TY_SELF": ("resolves at impl binding (D-157) before crossing rules "
                    "ask; the channel-element rule (TYPE-057) runs at the "
                    "spelling and again at substitution"),
        "TY_PARAM": ("the borrow admissions run POST-SUBSTITUTION: the "
                     "channel-element rule (TYPE-057) at type_subst's channel "
                     "arm, the layout bits per concrete instance"),
        "TY_ASSOC": "a projection; resolves to its bound type first",
    },
    # what a channel may carry (TYPE-057, 1.4.7): every kind named in the body,
    # so a kind added later must be classified rather than admitted by default
    ("chan_elem_verdict_recorded", "src/frontend/types.npk"): {},
    # shared-cell containment (D-235): every kind named, like the verdict table
    ("type_contains_shared_recorded", "src/frontend/types.npk"): {},
    # whether a type carries a channel endpoint (D-183's gives/factory rules)
    ("type_contains_channel_recorded", "src/frontend/types.npk"): {
        "TY_INVALID": "resolution already failed and already said so",
        "TY_NIL": _SCALAR, "TY_BOOL": _SCALAR, "TY_INT": _SCALAR,
        "TY_CHAR": _SCALAR, "TY_FLOAT": _SCALAR, "TY_TBB": _SCALAR,
        "TY_KERNEL": _SCALAR, "TY_FLAGS": _SCALAR,
        "TY_ERROR": "a code word (D-179)",
        "TY_STRING": "a byte cell; no handle inside",
        "TY_CSTRING": "a byte cell; no handle inside",
        "TY_BUFFER": "a byte cell; no handle inside (D-200)",
        "TY_ANY": "unsized; never a value at this rung",
        "TY_OWNEDFD": "a descriptor number (D-185)",
        "TY_HANDLE": ("an index (D-152); reaching the element takes the "
                      "arena, and the ARENA answers from its element"),
        "TY_SIMD": _TIER, "TY_TFP": _TIER, "TY_DIM": _TIER,
        "TY_TERN": _TIER, "TY_FRAC": _TIER, "TY_COMPLEX": _TIER,
        "TY_RANGE": "two values of an integer element (D-093)",
        "TY_TRAIT": "a bound, not a value",
        "TY_FUNC": "a code address carries nothing",
        "TY_FUNC_VARIADIC": "a code address carries nothing",
        "TY_COMPTIME": "a compile-time argument, never a runtime value",
        "TY_SELF": "resolves at impl binding (D-157) before the gives rule asks",
        "TY_PARAM": ("the gives rule's own text defers generic templates; "
                     "an instantiation is checked with the parameter bound"),
        "TY_ASSOC": "a projection; resolves to its bound type first",
        "TY_CONDVAR": "carries only constants (LEVEL) -- no element",
        "TY_BARRIER": "carries only constants (N, LEVEL) -- no element",
    },
    # the escape analysis's pointer question (what can point into a frame)
    ("type_holds_pointer", "src/frontend/analysis/escape.npk"): {
        "TY_INVALID": _META_CLOSED, "TY_TRAIT": _META_CLOSED,
        "TY_SELF": _META_CLOSED, "TY_PARAM": _META_CLOSED,
        "TY_ASSOC": _META_CLOSED, "TY_COMPTIME": _META_CLOSED,
    },
    # struct-layout's pointer-bearing question (0.5's escape rules feed on it)
    ("field_holds_ptr", "src/frontend/type_layout.npk"): {
        "TY_STRING": "the stated tail's `true` IS the answer: a body address",
        "TY_CSTRING": "the stated tail's `true` IS the answer: an address",
        "TY_BUFFER": "the stated tail's `true` IS the answer (D-200)",
        "TY_ANY": "the stated tail's `true` IS the answer",
        "TY_POINTER": "the stated tail's `true` IS the answer",
        "TY_SLICE": "the stated tail's `true` IS the answer (D-070)",
        "TY_ARENA": "the stated tail's `true` IS the answer: slab pointers",
        "TY_SHARED_ARENA": "the stated tail's `true` IS the answer (D-154)",
        "TY_DYN": "the stated tail's `true` IS the answer: the data pointer",
        "TY_INVALID": _META_CLOSED, "TY_TRAIT": _META_CLOSED,
        "TY_SELF": _META_CLOSED, "TY_PARAM": _META_CLOSED,
        "TY_ASSOC": _META_CLOSED, "TY_COMPTIME": _META_CLOSED,
        "TY_FUNC": ("a code address; rides the conservative tail -- a "
                    "fn-typed field has no consumer to justify the precise "
                    "answer yet"),
        "TY_FUNC_VARIADIC": "as TY_FUNC",
    },
    # generic substitution -- an unnamed kind passes through unsubstituted
    ("type_subst", "src/frontend/type_generic.npk"): {
        "TY_INVALID": _LEAF_SUBST, "TY_NIL": _LEAF_SUBST,
        "TY_BOOL": _LEAF_SUBST, "TY_INT": _LEAF_SUBST,
        "TY_CHAR": _LEAF_SUBST, "TY_FLOAT": _LEAF_SUBST,
        "TY_TBB": _LEAF_SUBST, "TY_KERNEL": _LEAF_SUBST,
        "TY_FLAGS": _LEAF_SUBST,
        "TY_STRING": _LEAF_SUBST, "TY_CSTRING": _LEAF_SUBST,
        "TY_ANY": _LEAF_SUBST, "TY_ERROR": _LEAF_SUBST,
        "TY_OWNEDFD": _LEAF_SUBST, "TY_BUFFER": _LEAF_SUBST,
        "TY_TFP": _LEAF_SUBST, "TY_DIM": _LEAF_SUBST,
        "TY_TERN": _LEAF_SUBST, "TY_FRAC": _LEAF_SUBST,
        "TY_COMPLEX": _LEAF_SUBST, "TY_COMPTIME": _LEAF_SUBST,
        "TY_SELF": "resolves at impl binding (D-157), not here",
        "TY_CONDVAR": "carries only constants (LEVEL) -- the stated comment",
        "TY_BARRIER": "carries only constants (N, LEVEL) -- the stated comment",
        "TY_FUNC": ("handled via `type_is_func` -> `subst_func`; the "
                    "predicate hides the constant from the mention scan"),
        "TY_FUNC_VARIADIC": "as TY_FUNC, via `type_is_func`",
    },
    # does a type mention a generic parameter -- an unnamed kind says "no",
    # which is how a channel once opened with a zero-byte element (1.1.10-D)
    ("type_mentions_param", "src/backend/ir/ir_types.npk"): {
        "TY_INVALID": "no operand", "TY_NIL": "no operand",
        "TY_BOOL": "no operand", "TY_INT": "no operand",
        "TY_CHAR": "no operand", "TY_FLOAT": "no operand",
        "TY_TBB": "no operand", "TY_KERNEL": "no operand",
        "TY_FLAGS": "no type operand (the family index is not a type)",
        "TY_STRING": "no operand", "TY_CSTRING": "no operand",
        "TY_ANY": "no operand", "TY_ERROR": "no operand",
        "TY_OWNEDFD": "no operand", "TY_BUFFER": "no operand",
        "TY_TFP": "no operand", "TY_DIM": "no operand",
        "TY_TERN": "no operand", "TY_FRAC": "no operand",
        "TY_COMPLEX": "no operand",
        "TY_SELF": "resolves at impl binding (D-157), never a parameter",
        "TY_COMPTIME": ("a comptime argument's value-type is never a "
                        "parameter (the language has no comptime generic "
                        "parameters)"),
        "TY_CONDVAR": "carries only constants (LEVEL)",
        "TY_BARRIER": "carries only constants (N, LEVEL)",
        "TY_FUNC": ("handled via `type_is_func` (1.4.1); the predicate "
                    "hides the constant from the mention scan"),
        "TY_FUNC_VARIADIC": "as TY_FUNC, via `type_is_func` (1.4.1)",
    },
    # the emitter's type lowering -- the LLVM text every store trusts.
    # Fully total: every kind is named in the body. An empty table is the
    # assertion that this stays so.
    ("ll_type", "src/backend/ir/ir_types.npk"): {},
    # the diagnostic renderer -- a missing kind prints something unhelpful
    # at the exact moment a human needs the type named
    ("type_display", "src/frontend/type_names.npk"): {
        "TY_STRUCT": "the named-type tail renders the declaration's name",
        "TY_ENUM": "the named-type tail renders the declaration's name",
        "TY_TRAIT": "the named-type tail renders the declaration's name",
        "TY_DYN": "the named-type tail renders `dyn` over the trait window",
        "TY_PARAM": "the named-type tail renders the parameter's name",
        "TY_COMPTIME": ("carries its value-rendering AS its name; the "
                        "named-type tail prints it (the stated comment)"),
    },
    # the generated `@"npk.drop.<tid>"` bodies -- the backend half of
    # `type_drops`' claim: a kind the frontend says owns something must
    # have a body the backend can emit
    ("emit_one_drop", "src/backend/emit_program.npk"): {
        "TY_INVALID": _NOT_REGISTERED, "TY_NIL": _NOT_REGISTERED,
        "TY_BOOL": _NOT_REGISTERED, "TY_INT": _NOT_REGISTERED,
        "TY_CHAR": _NOT_REGISTERED, "TY_FLOAT": _NOT_REGISTERED,
        "TY_TBB": _NOT_REGISTERED, "TY_KERNEL": _NOT_REGISTERED,
        "TY_FLAGS": _NOT_REGISTERED,
        "TY_CSTRING": _NOT_REGISTERED, "TY_ANY": _NOT_REGISTERED,
        "TY_POINTER": _NOT_REGISTERED, "TY_SLICE": _NOT_REGISTERED,
        "TY_TRAIT": _NOT_REGISTERED, "TY_FUNC": _NOT_REGISTERED,
        "TY_SELF": _NOT_REGISTERED, "TY_PARAM": _NOT_REGISTERED,
        "TY_RANGE": _NOT_REGISTERED, "TY_FUNC_VARIADIC": _NOT_REGISTERED,
        "TY_COMPTIME": _NOT_REGISTERED, "TY_HANDLE": _NOT_REGISTERED,
        "TY_ASSOC": _NOT_REGISTERED, "TY_ERROR": _NOT_REGISTERED,
        "TY_SIMD": _NOT_REGISTERED, "TY_TFP": _NOT_REGISTERED,
        "TY_DIM": _NOT_REGISTERED, "TY_TERN": _NOT_REGISTERED,
        "TY_FRAC": _NOT_REGISTERED, "TY_COMPLEX": _NOT_REGISTERED,
        "TY_CHANNEL": ("reclaimed by the creating scope's finalizer "
                       "(D-183 1.2.5), never by value drop"),
    },
    # the generated `@"npk.vacant.<tid>"` bodies (D-225) -- the FIXUPS a zero
    # fill gets wrong. This table is the decision's rider 1 made executable:
    # every kind that carries a drop must have a STATED canonical vacant value
    # its drop body is a no-op on, and "all-zeroes" is a claim reviewed here
    # rather than assumed. The `OwnedFd` comment said "Negative means a zeroed
    # slot" for two cycles while a zero fill produced descriptor 0 -- prose was
    # not enough, which is why this row exists.
    # the generated `@"npk.vacant.<tid>"` bodies (D-225) -- the fixups a zero
    # fill gets WRONG. This table is the decision's rider made executable: every
    # kind carrying a drop has a STATED canonical vacant value its drop body is
    # a no-op on, and "all-zeroes" is a claim reviewed here rather than assumed.
    # The `OwnedFd` drop comment claimed "Negative means a zeroed slot" for two
    # cycles while a zero fill produced descriptor 0 -- prose was not enough,
    # which is the whole reason this row exists.
    ("emit_one_vacant", "src/backend/emit_program.npk"): {
        "TY_INVALID": _NOT_REGISTERED,
        "TY_NIL": _NOT_REGISTERED,
        "TY_BOOL": _NOT_REGISTERED,
        "TY_INT": _NOT_REGISTERED,
        "TY_CHAR": _NOT_REGISTERED,
        "TY_FLOAT": _NOT_REGISTERED,
        "TY_TBB": _NOT_REGISTERED,
        "TY_KERNEL": _NOT_REGISTERED,
        "TY_FLAGS": _NOT_REGISTERED,
        "TY_STRING": ("`cap == 0` IS the not-mine bit, so a zero fill is literally "
                     "the unowned value (D-183/D-200)"),
        "TY_CSTRING": _NOT_REGISTERED,
        "TY_ANY": _NOT_REGISTERED,
        "TY_POINTER": _NOT_REGISTERED,
        "TY_SLICE": _NOT_REGISTERED,
        "TY_OPTIONAL": _NOT_REGISTERED,
        "TY_RESULT": _NOT_REGISTERED,
        "TY_TRAIT": _NOT_REGISTERED,
        "TY_DYN": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_FUNC": _NOT_REGISTERED,
        "TY_SELF": _NOT_REGISTERED,
        "TY_PARAM": _NOT_REGISTERED,
        "TY_RANGE": _NOT_REGISTERED,
        "TY_FUNC_VARIADIC": _NOT_REGISTERED,
        "TY_COMPTIME": _NOT_REGISTERED,
        "TY_ARENA": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_HANDLE": _NOT_REGISTERED,
        "TY_SHARED_ARENA": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_ASSOC": _NOT_REGISTERED,
        "TY_ERROR": _NOT_REGISTERED,
        "TY_ATOMIC": _NOT_REGISTERED,
        "TY_MUTEX": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_GUARD": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_RWLOCK": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_RGUARD": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_CONDVAR": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_BARRIER": ("gates on a NULL handle; its own drop comment already "
                        "names a zeroed slot as the inert case"),
        "TY_CHANNEL": ("reclaimed by its creating scope's finalizer, never "
                       "by value drop -- no overwrite-drop to be vacant for"),
        "TY_SIMD": _NOT_REGISTERED,
        "TY_TFP": _NOT_REGISTERED,
        "TY_DIM": _NOT_REGISTERED,
        "TY_TERN": _NOT_REGISTERED,
        "TY_FRAC": _NOT_REGISTERED,
        "TY_COMPLEX": _NOT_REGISTERED,
        "TY_BUFFER": ("`cap == 0` IS the not-mine bit, so a zero fill is literally "
                     "the unowned value (D-183/D-200)"),
    },
}


def check_type_walkers_total():
    """Every TY_* kind is named by each walker, or excused with a reason (B-7)."""
    fails = []
    with open(os.path.join(ROOT, "src", "frontend", "types.npk"),
              encoding="utf-8") as fh:
        kinds = re.findall(r"pub func:(TY_[A-Z_]+) = int32", fh.read())
    for (fn, rel), excused in WALKER_DEFAULT_OK.items():
        with open(os.path.join(ROOT, rel), encoding="utf-8") as fh:
            src = fh.read()
        m = re.search(r"^(?:pub )?func:%s = .*?^\};" % re.escape(fn),
                      src, re.M | re.S)
        if not m:
            fails.append("walkers-total: `%s` not found in %s" % (fn, rel))
            continue
        body = "\n".join(l for l in m.group(0).split("\n")
                         if not l.lstrip().startswith("//"))
        named = set(re.findall(r"TY_[A-Z_]+", body))
        for k in kinds:
            if k in named or k in excused:
                continue
            fails.append("walkers-total: `%s` (%s) does not name %s and its "
                         "excuse table gives no reason -- an unconsidered kind "
                         "takes the walker's default silently, which is the "
                         "1.1.10-D defect class" % (fn, rel, k))
        for k in sorted(excused):
            if k not in kinds:
                fails.append("walkers-total: `%s`'s excuse table names %s, "
                             "which is no longer a type kind" % (fn, k))
            elif k in named:
                fails.append("walkers-total: %s is both named by `%s` and "
                             "excused -- one of the two is stale" % (k, fn))
    return fails


def check_slot_sites_agree():
    """Every `fits` site has an `emit_fit` partner, by the table above.

    Three ways to fail: a frontend function asks `fits` and is not in the
    table; a backend function calls `emit_fit` and no row names it; a row names
    a backend function that no longer calls `emit_fit`, or a `refused:` row
    whose rung string is gone. The last is the one that matters over time --
    it is how lowering `??` at 1.0.7 is forced to write its pairing.
    """
    fails = []
    fe = _sites_by_function(r"\bfits\(",
                            glob.glob(os.path.join(ROOT, "src", "frontend", "*.npk")),
                            r"func:fits\b")
    be = _sites_by_function(r"\bemit_fit\(",
                            glob.glob(os.path.join(ROOT, "src", "backend", "**", "*.npk"),
                                      recursive=True),
                            r"func:emit_fit\b")
    backend_text = ""
    for p in glob.glob(os.path.join(ROOT, "src", "backend", "**", "*.npk"),
                       recursive=True):
        with open(p, encoding="utf-8") as fh:
            backend_text += fh.read()

    for fn, where in sorted(fe.items()):
        if fn not in SLOT_SITE_PAIRS:
            fails.append("slot-sites: `%s` asks `fits` (%s) and has no `emit_fit` "
                         "partner in SLOT_SITE_PAIRS -- a slot the emitter would "
                         "store raw" % (fn, ", ".join(where)))
    claimed = set()
    for fn, partner in SLOT_SITE_PAIRS.items():
        if fn not in fe:
            fails.append("slot-sites: SLOT_SITE_PAIRS names `%s`, which no longer "
                         "asks `fits`" % fn)
        if isinstance(partner, str):
            what = partner[len("refused:"):].strip()
            if ('iv_rung("%s"' % what) not in backend_text:
                fails.append("slot-sites: `%s` is paired with the refusal \"%s\", "
                             "and no backend rung refuses it any more -- write its "
                             "`emit_fit` pairing" % (fn, what))
            continue
        for b in partner:
            claimed.add(b)
            if b not in be:
                fails.append("slot-sites: `%s` is paired with `%s`, which does not "
                             "call `emit_fit`" % (fn, b))
    for b, where in sorted(be.items()):
        if b not in claimed:
            fails.append("slot-sites: `%s` calls `emit_fit` (%s) and no `fits` "
                         "site is paired with it" % (b, ", ".join(where)))
    return fails


def check_one_renderer():
    """Every driver prints diagnostics through `diag_line`, and no diagnostic is
    constructed with an empty message (1.0.8).

    Five renderers existed and no two agreed; the two that ran dropped the
    message, so 79 hand-written messages never reached a reader and were never
    reviewed -- 1.0.4c wrote one whose two halves were bound to declaration
    order, backwards, and nothing could have shown it. The drift was possible
    only because a second renderer was allowed to exist, so this pins the
    thing that broke: no `func:emit_line` in a driver, and every driver that
    reports at all names `diag_line`. The empty-message rule is cheap and
    catches the construction site that writes the code and forgets the rest.
    """
    fails = []
    drivers = [os.path.join(ROOT, "src", "main.npk")] + sorted(
        glob.glob(os.path.join(ROOT, "tools", "*.npk")))
    for p in drivers:
        with open(p, encoding="utf-8") as fh:
            text = fh.read()
        rel = os.path.relpath(p, ROOT)
        if re.search(r"^\s*(?:pub )?func:emit_line\b", text, re.M):
            fails.append("%s defines its own `emit_line` -- diagnostics render "
                         "through `diag_line` (diagnostics.npk) and nowhere else"
                         % rel)
        if "diaglist_at(" in text and "diag_line(" not in text:
            fails.append("%s walks a diagnostic list without `diag_line` -- a "
                         "second renderer, or none" % rel)
    ctors = r"\b(te_error|ty_error|p_error|diag_error|diag_warning|diag_note|diag_make)\("
    for p in glob.glob(os.path.join(ROOT, "src", "**", "*.npk"), recursive=True):
        with open(p, encoding="utf-8") as fh:
            for i, line in enumerate(fh, 1):
                if line.lstrip().startswith("//"):
                    continue
                if re.search(ctors, line) and re.search(r',\s*""\s*[,)]', line):
                    fails.append("%s:%d constructs a diagnostic with an EMPTY "
                                 "message -- a reader cannot act on a code alone"
                                 % (os.path.relpath(p, ROOT), i))
    return fails


def check_rung_names_open_cycle():
    """Every backend refusal names a cycle that is still open, or a row that
    exists (1.0.9).

    0.9 closed on the rule that a cycle does not close while a refusal string
    names it, checked by a grep at 0.9.7 -- and the same sweep re-pointed every
    stale "0.8" string at "1.0" as the NEXT cycle rather than an owner, which
    is how 1.0 opened its tail with 27 strings to honour. This makes the grep a
    check: the moment a cycle's folder moves under `meta/roadmap/done/`, any
    `iv_rung`/`ll_rung` still naming it fails the harness. A rung may name a
    cycle ("1.1") or a cycle with its row ("1.1 (G-3)") or a row alone
    ("a later cycle (G-2)"); a row must exist in OPEN_DECISIONS.md.
    """
    fails = []
    done = set(os.listdir(os.path.join(ROOT, "meta", "roadmap", "done")))
    with open(os.path.join(ROOT, "meta", "roadmap", "OPEN_DECISIONS.md"),
              encoding="utf-8") as fh:
        rows = set(re.findall(r"\*\*([A-Z]+-\d+)\*\*", fh.read()))
    # A call may break its line between the construct and the rung, so the
    # FILE is scanned, not its lines: `iv_rung("what", "rung", …)` and
    # `pv_rung("what", "rung", …)` carry the rung second, `ll_rung("rung")`
    # first. Comment lines are blanked first so a quoted example in prose
    # does not count.
    two = re.compile(r'\b(?:iv_rung|pv_rung)\(\s*"[^"]*"\s*,\s*"([^"]*)"', re.S)
    one = re.compile(r'\bll_rung\(\s*"([^"]*)"')
    for p in glob.glob(os.path.join(ROOT, "src", "backend", "**", "*.npk"),
                       recursive=True):
        with open(p, encoding="utf-8") as fh:
            text = "".join(("\n" if ln.lstrip().startswith("//") else ln)
                           for ln in fh.read().splitlines(True))
        hits = [(m.start(), m.group(1)) for m in two.finditer(text)]
        hits += [(m.start(), m.group(1)) for m in one.finditer(text)]
        for at, rung in hits:
            key = "%s:%d" % (os.path.relpath(p, ROOT), text.count("\n", 0, at) + 1)
            m = re.match(r"(\d+\.\d+)", rung)
            row = re.search(r"\(([A-Z]+-\d+)\)", rung)
            if m:
                if m.group(1) in done:
                    fails.append("%s refuses with rung %r, and cycle %s is CLOSED "
                                 "(meta/roadmap/done/%s) -- a refusal may not name a "
                                 "cycle that will never enable it; lower it, convert "
                                 "it, or re-point it BY NAME"
                                 % (key, rung, m.group(1), m.group(1)))
            elif row is None:
                fails.append("%s refuses with rung %r, which names neither a cycle "
                             "nor an OPEN_DECISIONS row" % (key, rung))
            if row is not None and row.group(1) not in rows:
                fails.append("%s refuses with rung %r, and row %s is not in "
                             "OPEN_DECISIONS.md" % (key, rung, row.group(1)))
    return fails


# `check_ll_types_agree` RETIRED AT 1.4.6 (D-205, which names it).
#
# It diffed two emitters -- `bootstrap/generator/ntypes.py`, which lowered
# types for the compiler that built stage 1, against `src/backend/ir/
# ir_types.npk`, which lowers them for stage 1 itself -- because their IR met
# at every runtime symbol and a one-field disagreement is a call through a
# wrong aggregate rather than a loud error. The switch removed the first
# emitter from every build path, so the question has one answer and nothing to
# compare it against.
#
# What it was really protecting is NOT lost: `tests/backend/ir_types.npk` still
# pins the real compiler's lowering as a string on every `ll_is` line (fifteen
# of them added for the 1.3 tier alone), and `check_runtime_sigs_agree` still
# holds every aggregate that crosses the runtime boundary. The retired half is
# the half that asked a retired tool.


def _npkrt_defines():
    """The functions npkrt.ll DEFINES, parsed from the IR text itself.

    Multi-line parameter lists are real (string_concat's spans two), so the
    define is matched through balanced parentheses rather than to end-of-line --
    the first version of this scan was line-anchored and reported two runtime
    functions as missing.
    """
    src = open(RUNTIME_LL, encoding="utf-8").read()
    out = {}
    for m in re.finditer(r"define\s+(?:internal\s+)?(.+?)\s+@(\w+)\(", src):
        ret, name = m.group(1).strip(), m.group(2)
        depth, i, start = 1, m.end(), m.end()
        while depth:
            c = src[i]
            if c == "(":
                depth += 1
            elif c == ")":
                depth -= 1
            i += 1
        args, cur, d2 = [], "", 0
        for c in src[start:i - 1]:
            if c == "," and d2 == 0:
                args.append(cur.strip())
                cur = ""
                continue
            if c in "{[(":
                d2 += 1
            if c in "}])":
                d2 -= 1
            cur += c
        if cur.strip():
            args.append(cur.strip())
        # strip the parameter NAME, keeping the type text
        tys = []
        for a in args:
            a = " ".join(a.split())
            tys.append(a.rsplit(" %", 1)[0] if " %" in a else a)
        out[name] = (ret.replace("noreturn", "").strip(), tys)
    return out


def check_builtin_sig_texts():
    """Every type text the generated signature table hands the checker is one the
    checker's resolver actually has, and no arm of the resolver is dead.

    D-201 (1.4.2) made BUILTIN_REFERENCE.md's Signature column load-bearing:
    `builtins.npk` carries the parameter and return types as TEXT and
    `builtin_text_type` in `type_access.npk` interns them. The generator cannot
    check the second half -- it has no idea what the compiler can resolve -- so
    without this a builtin added with a type nobody taught the resolver would
    generate cleanly, build cleanly, and refuse at its first CALL SITE with
    "the generated table and `builtin_text_type` disagree". This turns that into
    a build failure, which is where it belongs, and it fails the other way too:
    an arm for a text no builtin uses is a row nothing checks.
    """
    fails = []
    bl = open(os.path.join(ROOT, "src", "frontend", "builtins.npk"),
              encoding="utf-8").read()
    used = set()
    for fn in ("builtin_sig_param", "builtin_sig_ret"):
        m = re.search(r'pub func:%s = [^{]*\{(.*?)\n\};' % fn, bl, re.S)
        if m is None:
            return ["check_builtin_sig_texts found no `%s` in builtins.npk -- "
                    "the check has stopped checking" % fn]
        used |= {t for t in re.findall(r'pass "([^"]*)";', m.group(1)) if t}
    acc = open(os.path.join(ROOT, "src", "frontend", "type_access.npk"),
               encoding="utf-8").read()
    m = re.search(r'func:builtin_text_type = [^{]*\{(.*?)\n\};', acc, re.S)
    if m is None:
        return ["check_builtin_sig_texts found no `builtin_text_type` in "
                "type_access.npk -- the check has stopped checking"]
    known = set(re.findall(r'string_eq\(t, "([^"]*)"\)', m.group(1)))
    for t in sorted(used - known):
        fails.append("the generated signature table uses the type text `%s` and "
                     "`builtin_text_type` cannot intern it -- every builtin "
                     "written with it would refuse at its first call site" % t)
    for t in sorted(known - used):
        fails.append("`builtin_text_type` interns the type text `%s` and no "
                     "builtin row uses it -- a dead arm in a table that has to "
                     "stay honest" % t)
    return fails


def check_runtime_sigs_agree():
    """npkrt.ll and src/backend/ir/ir_runtime.npk must state the SAME signature
    for every runtime symbol.

    Two copies of one fact, and neither removable: the runtime DEFINES, the
    compiler declares. npkrt.ll's own header records what a disagreement does
    -- llc rejects the IR -- and a size disagreement is worse, because a call
    through a wrong aggregate type corrupts the callee's view of memory
    instead of failing.

    IT WAS A THREE-WAY DIFF until 1.4.6 (D-205 says it drops to two). The third
    copy was the Python seed's `RUNTIME` table, which declared the floor for
    the stage-1 it built; the switch means it builds nothing, so its table
    describes no compiler. The remaining pair is the sharper one anyway, since
    1.4.2 made BUILTIN_REFERENCE generate `ir_runtime.npk` -- the spec is in
    this loop now, and the seed never was.
    """
    fails = []
    defs = _npkrt_defines()

    # --- the compiler against the runtime --------------------------------
    rtsrc = open(os.path.join(ROOT, "src", "backend", "ir", "ir_runtime.npk"),
                 encoding="utf-8").read()
    entries = {}
    for m in re.finditer(
            r'string_eq\(name, "(\w+)"\)\)\s*\{\s*pass \(raw rt\((.*?)\)\);',
            rtsrc, re.S):
        name, body = m.group(1), m.group(2)
        strs = re.findall(r'"((?:[^"\\]|\\.)*)"', body)
        argc = re.search(r"(\d+)i32", body)
        # THE WRAPPED FLAG IS PARSED PER ENTRY. It used to be read below, out of
        # `body` -- which by then held whichever entry this loop happened to
        # parse LAST, so every row was told the last row's answer and the
        # derived-inner cross-check underneath has never once run (1.4.2). The
        # flag is the third argument, and reading it by splitting on commas found
        # the middle of `{ i32, i32 }` instead, which is the other half of why.
        wm = re.search(r'",\s*(true|false),\s*"', body)
        entries[name] = (strs, int(argc.group(1)),
                         wm is not None and wm.group(1) == "true")
    if not entries:
        return ["check_runtime_sigs_agree parsed no entries out of "
                "src/backend/ir/ir_runtime.npk -- the check has stopped checking"]
    for name, (strs, argc, wrapped) in sorted(entries.items()):
        sym, ret, inner = strs[0], strs[1], strs[2]
        args = [a for a in strs[3:3 + argc]]
        d = defs.get(sym.lstrip("@"))
        if d is None:
            fails.append("ir_runtime.npk declares `%s` as %s and npkrt.ll does "
                         "not define it" % (name, sym))
            continue
        if d[0] != ret:
            fails.append("`%s` returns `%s` in npkrt.ll and `%s` in "
                         "ir_runtime.npk" % (name, d[0], ret))
        if d[1] != args:
            fails.append("`%s` takes %s in npkrt.ll and %s in ir_runtime.npk"
                         % (name, d[1], args))
        # Result-SHAPED returns are no longer exclusively Results -- a Handle
        # is { i64, i32 } and is not one (0.10.2) -- so the derived-inner
        # cross-check applies to entries that CLAIM the wrapper, where a wrong
        # inner silently mis-extracts at every call site.
        if wrapped and (ret.startswith("{ {") or (ret.startswith("{") and ret.endswith("i32 }"))):
            want_inner = ret[1:].rsplit(",", 1)[0].strip() if ret != "{ i32 }" else ""
            if inner != want_inner:
                fails.append("`%s`'s wrapped inner is `%s` in ir_runtime.npk and "
                             "`%s` derived from its return" % (name, inner, want_inner))

    # --- every compiler entry points at something --------------------------
    #
    # The seed-vs-compiler reconciliation that stood here went with the seed's
    # table (1.4.6). What survives is the half that was ever load-bearing: a
    # floor entry naming a symbol the runtime does not define is a call into
    # nothing, and llc will not catch it -- the declaration makes it look fine
    # until the link.
    for name in sorted(entries):
        sym = entries[name][0][0].lstrip("@")
        if sym not in defs:
            fails.append("`%s` is in ir_runtime.npk and npkrt.ll does not define "
                         "%s -- a floor entry pointing at nothing" % (name, sym))
    return fails




# EVERY AST KIND EITHER LOWERS OR REFUSES BY NAME. D-085's contract has no third
# outcome -- yet the 0.8-close audit found constructs in one (dropped, or dying
# as iv_broken "internal defect" when they are really unlowered features). This
# table is the DESCRIPTION the backend is diffed against, in the exact tradition
# of `check_kinds_typed` (0.6.3): the table says what each kind is, the diff
# says whether the emitter agrees.
#
# Statuses:
#   "lowered"        the emitter has a real case (sub-cases may still rung)
#   "rung"           wholesale-refused with NITPICK-RUNG-001 naming a cycle
#   "inert: why"     deliberately no code of its own -- the reason is stated
#   "hole: ticket"   a known third-outcome kind, confessed, with its owner
#
# HONEST COVERAGE NOTE: this diff sees KIND-LEVEL absence. An attribute dropped
# from a lowered kind (LIVE-1's `limit` on a vardecl) is beyond it -- that class
# is held by BACKEND_CARRIER_READS below; guard behavior (LIVE-2) is held by
# executed-exit tests, the 0.7.7 instrument.
KIND_STATUS = {
    # --- ExprKind ---------------------------------------------------------
    "ExprIntLiteral": "lowered", "ExprCharLiteral": "lowered",
    "ExprStringLiteral": "lowered", "ExprRawStringLiteral": "lowered",
    "ExprBlockStringLiteral": "lowered", "ExprBoolLiteral": "lowered",
    "ExprBinaryExpr": "lowered", "ExprUnaryExpr": "lowered",
    "ExprAddressOfExpr": "lowered", "ExprDerefExpr": "lowered",
    "ExprBorrowExpr": "lowered", "ExprResultLiteralExpr": "lowered",
    "ExprIdentifierExpr": "lowered", "ExprMemberAccessExpr": "lowered",
    "ExprIndexExpr": "lowered", "ExprCallExpr": "lowered",
    "ExprMethodCallExpr": "lowered",   # variant ctors lower; UFCS rungs inside
    "ExprBuiltinExpr": "lowered", "ExprRawUnwrapExpr": "lowered",
    "ExprDropExpr": "lowered", "ExprRelayExpr": "lowered",
    "ExprCastExpr": "lowered", "ExprUncheckedCastExpr": "lowered",
    "ExprStructLiteralExpr": "lowered", "ExprArrayLiteralExpr": "lowered",
    "ExprMoveExpr": "lowered",         # yields its place; the transfer is static (D-065)
    "ExprFloatLiteral": "lowered",   # 0.9.4
    "ExprSentinelLiteral": "lowered", # 0.9.5 — NIL/NULL always were; ERR joined
    "ExprTemplateLiteral": "lowered",   # 1.0.9d — &{ } via ToString (D-168)
    "ExprPostfixExpr": "inert: `++`/`--` struck at parse (D-174, PARSE-010); the backend guard is a defensive confession",
    "ExprPipeExpr": "lowered",   # 1.0.9c
    "ExprRangeExpr": "lowered",   # 0.9.6
    "ExprSpreadExpr": "rung",
    "ExprTernaryExpr": "lowered",  # 0.9.6 — lazy, branch-based
    "ExprIsErrExpr": "lowered",  # 0.9.5
    "ExprSafeNavExpr": "rung", "ExprComptimeExpr": "lowered",  # 1.0.9c — folded at check time
    "ExprSafeUnwrapExpr": "lowered",   # the `?|` Result fallback (D-175 restored the spelling)
    "ExprEmphaticUnwrapExpr": "lowered",  # 0.9.7
    "ExprNullCoalesceExpr": "rung",
    "ExprDefaultsExpr": "inert: a bare `?` and the word `defaults` struck at parse (D-175, PARSE-011); the backend guard is a defensive confession",
    "ExprVectorCtorExpr": "lowered",  # 1.3.1 — simd<T, N> construction (D-194)
    "ExprAwaitExpr": "lowered",  # 1.1.4 — the machines compose; positional rungs remain until D/E
    "ExprIterationVarExpr": "lowered",  # 0.9.7
    "ExprDynCastExpr": "lowered",  # 1.0.9b — the fit to the dyn target
    "ExprPickExpr": "lowered",  # 0.9.7
    # --- StmtKind ---------------------------------------------------------
    "StmtBlockStmt": "lowered", "StmtVarDeclStmt": "lowered",
    "StmtAssignStmt": "lowered", "StmtExprStmt": "lowered",
    "StmtIfStmt": "lowered", "StmtPickStmt": "lowered",
    "StmtWhileStmt": "lowered", "StmtBreakStmt": "lowered",
    "StmtContinueStmt": "lowered", "StmtPassStmt": "lowered",
    "StmtFailStmt": "lowered", "StmtReturnStmt": "lowered",
    "StmtExitStmt": "lowered", "StmtTrapStmt": "lowered",
    "StmtDeferStmt": "lowered", "StmtDiscardStmt": "lowered",
    "StmtForStmt": "lowered", "StmtLoopStmt": "lowered", "StmtTillStmt": "lowered",  # 0.9.7
    "StmtWhenStmt": "lowered",  # 0.9.7
    "StmtProveStmt": "rung",
    "StmtAssertStaticStmt": "rung",
    "StmtFallStmt": "lowered", "StmtGiveStmt": "lowered",  # 0.9.7
    "StmtPickArm": "inert: walked inside its pick, never dispatched alone",
    # --- DeclKind ---------------------------------------------------------
    "DeclFunctionDecl": "lowered", "DeclStructDecl": "lowered",
    "DeclEnumDecl": "lowered", "DeclFieldDecl": "lowered",
    "DeclEnumVariant": "lowered",
    "DeclImplDecl": "rung", "DeclGlobalDecl": "rung",
    # D-190 (1.1.13c): the block is the INTERFACE RECORD -- its stubs are
    # generated as source in the expansion phase and lower as the ordinary
    # async functions they are; the block itself emits nothing.
    "DeclExternBlock": "inert: interface record; its generated stubs carry the code (D-190)",
    "DeclModuleDecl": "lowered",   # 0.9.6 — emit_all descends; the hole closed
    "DeclImportDecl": "inert: resolved by the loader; nothing to emit",
    "DeclTraitDecl": "inert: a signature set; code arrives via impls (1.0)",
    "DeclRuleDecl": "inert: consumed by the verifier (1.3), never emitted",
    "DeclMacroDecl": "inert: expansion consumed it before the backend",
    "DeclMacroSplice": "inert: expansion consumed it before the backend",
    "DeclOpaqueDecl": "inert: a type assertion; layout answers, no code",
    "DeclAssocTypeDecl": "inert: trait member, no code until 1.0",
    "DeclGenericParam": "inert: substituted at instantiation (1.0)",
    "DeclParamDecl": "inert: emitted inside its function's signature walk",
    "DeclExternFn": "inert: interface-record member; the generated stub carries the code (D-190)",
    "DeclVariadicSpec": "inert: part of a signature, not a construct",
    "DeclFailsOn": "inert: D-002's dead surface; refused by name in expansion (EXTERN-002)",
    "DeclNeverFails": "inert: the D-163 contract, checked in the frontend; on an extern method it refuses (EXTERN-002)",
    "DeclAttribute": "inert: read by the passes it decorates, never emitted",
    "DeclErrorDecl": "lowered",  # D-179: ident references emit the assigned code inline
    "DeclUnitDecl": "inert: a unit names a compile-time exponent vector (D-196); erased before lowering",
}

# THE ATTRIBUTE CARRIERS THE BACKEND MUST KEEP READING. LIVE-1 happened because
# nothing in src/backend/ read these accessors -- the constructs lowered with
# their verification attributes silently dropped. 0.9.0 added the reads (as
# refusals); this list keeps them read: each name must appear in src/backend/**
# or the regression is named on the next full run.
BACKEND_CARRIER_READS = {
    "stmt_decl_limit":      "a vardecl's limit<Rules> (refused until 1.3)",
    "param_limit":          "a parameter's limit<Rules> (refused until 1.3)",
    "fn_contract_count":    "a function's requires/ensures (refused until 1.3)",
    "stmt_while_invariant": "a while loop's invariant (refused until 1.3)",
}


def check_kinds_lowered_or_refused():
    """Every Expr/Stmt/Decl kind lowers, refuses by name, or is confessed."""
    kinds_path = os.path.join(ROOT, "src", "frontend", "ast_kind.npk")
    src = open(kinds_path, encoding="utf-8").read()
    declared = set()
    for enum in ("ExprKind", "StmtKind", "DeclKind"):
        m = re.search(r'pub enum:%s = \{(.*?)\};\n' % enum, src, re.S)
        declared.update(re.findall(r'(\w+)\s+=\s+\d+i32;', m.group(1)))
    declared -= {"ExprNone", "StmtNone", "DeclNone"}

    back = ""
    for p in glob.glob(os.path.join(ROOT, "src", "backend", "**", "*.npk"),
                       recursive=True):
        back += open(p, encoding="utf-8").read()

    fails = []
    for k in sorted(declared - set(KIND_STATUS)):
        fails.append("kind %s is not classified in KIND_STATUS -- say whether it "
                     "lowers, rungs, is inert, or is a confessed hole" % k)
    for k in sorted(set(KIND_STATUS) - declared):
        fails.append("KIND_STATUS row %s matches no declared kind -- a dead row "
                     "stops the table being read" % k)
    for k, status in sorted(KIND_STATUS.items()):
        if k not in declared:
            continue
        mentioned = ("Kind.%s" % k) in back
        if status in ("lowered", "rung") and not mentioned:
            fails.append("%s is claimed %s but src/backend/ never names it -- the "
                         "kind falls to the fail-closed default and dies as an "
                         "internal defect instead of its honest answer"
                         % (k, status))
        if status.startswith("hole:") and mentioned:
            fails.append("%s is confessed as a hole but the backend now names it "
                         "-- the hole closed; reclassify it" % k)
    for name, what in sorted(BACKEND_CARRIER_READS.items()):
        if name not in back:
            fails.append("src/backend/ no longer reads `%s` (%s) -- LIVE-1's "
                         "class reopens the moment a carrier goes unread"
                         % (name, what))
    return fails


def check_decisions_current():
    """Candidates for stale decision-log entries -- REPORTED, never failing.

    The audit's Theme F: a SETTLED heading whose blocks/settle-by clause names a
    finished cycle, an OPEN heading whose scheduled cycle is long done, a
    supersession with no back-reference. This lint cannot judge prose, so it
    reports candidates for a human pass (the 0.9.8 doc-sync drains the first
    run's list) -- the posture of a spell-checker, not a type checker.
    """
    path = os.path.join(ROOT, "meta", "specs", "DECISIONS.md")
    text = open(path, encoding="utf-8").read()
    done = set()
    for d in glob.glob(os.path.join(ROOT, "meta", "roadmap", "done", "*")):
        base = os.path.basename(d)
        if re.match(r"^\d+\.\d+$", base):
            done.add(base)

    sections = re.split(r"^## (?=D-\d+)", text, flags=re.M)
    heads = re.findall(r"^## (D-\d+)[^\n]*?\*\*([A-Z]+)", text, flags=re.M)
    report = []
    bynum = {}
    for sec in sections[1:]:
        # A heading may carry no status marker at all (D-114 does not) -- scan
        # it anyway; the cycle/phase rules do not depend on the marker.
        m = re.match(r"(D-\d+)", sec)
        if not m:
            continue
        num = m.group(1)
        sm = re.match(r"D-\d+[^\n]*?\*\*([A-Z]+)", sec)
        status = sm.group(1) if sm else "UNMARKED"
        bynum[num] = sec
        cycles = set(re.findall(r"\b(\d+\.\d+)(?:\.\d+)?\b", sec))
        stale_cycles = sorted(c for c in cycles if c in done)
        if status == "OPEN" and stale_cycles:
            report.append("%s is OPEN but cites finished cycle(s) %s -- it may "
                          "have landed" % (num, ", ".join(stale_cycles)))
        if status == "SETTLED":
            for c in sorted(cycles & done):
                if re.search(r"(settle before|blocks?)\s+(cycle\s+)?%s\b"
                             % re.escape(c), sec, re.I):
                    report.append("%s says 'settle before/blocks %s', a finished "
                                  "cycle -- the clause is stale" % (num, c))
        if re.search(r"settled? before Phase B", sec, re.I):
            report.append("%s says 'settle before Phase B', which is underway -- "
                          "annotate or resolve" % num)
    for num, sec in bynum.items():
        for target in re.findall(r"[Ss]upersedes (?:the [\w-]+ (?:half|part) of )?(D-\d+)", sec):
            if target in bynum and num not in bynum[target]:
                report.append("%s supersedes %s, but %s carries no back-reference "
                              "-- a targeted reader of %s builds the dead design"
                              % (num, target, target, target))
    return report


def check_codes_tested():
    """Every code a rule can emit is asserted by some test, or is on the list."""
    codes = {}
    # THE WHOLE COMPILER, not just the frontend. The glob said `src/frontend` when
    # the frontend was all there was; a backend code added under `src/backend`
    # would have been invisible to this check on the day it was written, which is
    # the day it needs to be visible.
    for p in glob.glob(os.path.join(ROOT, "src", "**", "*.npk"), recursive=True):
        with open(p, encoding="utf-8") as fh:
            for m in CODE_DECL_RE.finditer(fh.read()):
                codes[m.group(2)] = m.group(1)

    # A test asserts a code either as `expect-error: CODE`, or -- in the frontend
    # unit tests, which check in-process -- by naming the constant.
    names = set(codes.values())
    asserted = set()
    for p in glob.glob(os.path.join(ROOT, "tests", "**", "*.npk"), recursive=True):
        with open(p, encoding="utf-8") as fh:
            text = fh.read()
        asserted.update(re.findall(r"expect-error:\s*([A-Z0-9\-]+)", text))
        asserted.update(re.findall(r"expect-note:\s*([A-Z0-9\-]+)", text))
        for name in re.findall(r"raw ([A-Z][A-Z0-9_]+)\(\)", text):
            if name in names:
                asserted.update(c for c, n in codes.items() if n == name)

    fails = []
    for code in sorted(codes):
        if code not in asserted and code not in UNTESTED_CODES:
            fails.append("%s (%s) is emitted by the compiler and asserted by no test"
                         " -- a code with no case is a rule nobody has watched refuse"
                         " anything" % (code, codes[code]))
    for code in sorted(UNTESTED_CODES):
        if code in asserted:
            fails.append("%s has a test now, so it comes off UNTESTED_CODES -- an "
                         "allow-list that outlives its entries stops being read"
                         % code)
        elif code not in codes:
            fails.append("%s is on UNTESTED_CODES and no longer exists" % code)
    return fails


# --- the real FRONTEND, on real programs --------------------------------------

def build_tool(tmp, tools, source, name):
    """Compile a tool under `tools/` or `src/` and return its path.

    THE BUILDER IS THE COMMITTED SNAPSHOT (D-203/D-205, 1.4.6). This used to
    run the Python seed over the source and its transitive imports, which is
    why `group_for` existed: the seed has no module loader. The snapshot is a
    real compiler and resolves its own imports, so it gets the ENTRY file and
    nothing else.

    That is also what makes D-205's rule enforceable rather than aspirational.
    While the seed was the builder, `src/` had to stay inside subset 1 and the
    only thing holding it there was discipline. Now the constraint is
    mechanical: a construct the snapshot cannot compile fails here, in the
    first thing the harness does, naming the file.
    """
    if not tools:
        return None
    if not BUILDER or not os.path.exists(str(BUILDER)):
        return "no builder: %s" % BUILDER
    base = os.path.join(tmp, name)
    try:
        r = subprocess.run([BUILDER, source, "-o", base + ".ll"],
                           capture_output=True, timeout=900)
    except subprocess.TimeoutExpired:
        return "the snapshot did not terminate compiling %s" % source
    if r.returncode != 0:
        return "DIAG %s" % r.stderr.decode("utf-8", "replace").strip()[:400]
    r = subprocess.run(["llc"] + LLC_FLAGS + [
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "LLC %s" % r.stderr.strip()[:160]
    r = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", base, base + ".o",
                        os.path.join(tmp, "npkrt.o")], capture_output=True, text=True)
    if r.returncode != 0:
        return "LINK %s" % r.stderr.strip()[:160]
    return base


def check_module_rejection(binary, path, name, exp):
    """A whole program that must be refused BY THE LOADER, with these codes.

    Distinct from tests/rejection/, whose files parse cleanly and are refused
    LATER by the checker -- that is D-085's rule and the point of that suite. A
    file naming a module that does not exist never reaches a checker at all, and
    running both kinds through one tool would make the two sorts of "correctly
    refused" indistinguishable.
    """
    try:
        r = subprocess.run([binary, path], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return ["%s: the frontend did not terminate" % name]
    if r.returncode == 3:
        return ["%s: the frontend TRAPPED -- a defect in the compiler, not in "
                "this file" % name]
    if r.returncode == 0:
        return ["%s: expected %s, but it resolved cleanly"
                % (name, [c for c, _, _ in exp.errors])]

    # `CODE path:line:col: message` (1.0.8), and `note CODE …` / `warning CODE …`
    # in front of it for those severities; an error is the unmarked case.
    #
    # NOTES ARE NOT FINDINGS AND ARE NOT ASSERTED. `NITPICK-MACRO-009` says where a
    # macro body was expanded; making every expansion-related rejection test list a
    # code about a LOCATION would be asserting the wrong thing, and a suite that
    # could not tell the two apart would have to.
    #
    # KEYED ON POSITION, NOT ON TOKEN COUNT. This accepted a line only when it
    # split into exactly two tokens -- `CODE path:line:col` -- so the day the
    # message was appended (1.0.8) every line grew past two tokens, matched
    # nothing, and every rejection test failed at once with "expected [CODE],
    # got []", which reads like the checker broke rather than like the printer
    # changed. The code is token 0, the span is token 1 (its trailing colon is
    # the separator before the message), and everything after is the message,
    # which expectations deliberately never assert -- codes are stable
    # identifiers, wording stays free to improve.
    got = []
    notes = []
    # Diagnostics arrive on STDERR since 0.8.5 (D-141): stdout is the product
    # channel, and the report must survive a redirect of it.
    for line in r.stderr.decode("utf-8", "replace").splitlines():
        parts = line.split()
        into = got
        if parts and parts[0] == "note":
            parts = parts[1:]
            into = notes
        elif parts and parts[0] == "warning":
            parts = parts[1:]
        if len(parts) >= 2 and ":" in parts[1]:
            # `CODE path:line:col` since 0.8.0 (`CODE line:col` before a module
            # graph made bare line numbers ambiguous across sixty files). The
            # span is the LAST two fields; everything before them is the path,
            # which expectations deliberately do not assert.
            span = parts[1].rstrip(":")
            pieces = span.rsplit(":", 2)
            if len(pieces) == 3:
                _, ln, cl = pieces
            else:
                ln, _, cl = span.partition(":")
            try:
                into.append((parts[0], int(ln), int(cl)))
            except ValueError:
                # A line whose second token is not `path:line:col` -- the
                # `<unknown file>` fallback -- is not a positioned finding, as
                # `CODE <no span>: …` never was (its second token has no colon
                # and never reached here).
                continue

    fails = []
    for code, line, col in exp.errors:
        hit = [g for g in got if g[0] == code]
        if not hit:
            fails.append("%s: expected %s, got %s"
                         % (name, code, sorted(set(g[0] for g in got))))
            continue
        if line is not None:
            if not any(g[1] == line and (col is None or g[2] == col) for g in hit):
                fails.append("%s: %s at %s, expected %d:%s"
                             % (name, code, [(g[1], g[2]) for g in hit], line,
                                col if col is not None else "*"))

    # A SPAN RULE THAT IS ONLY DESCRIBED IS A SPAN RULE NOTHING CHECKS. The note
    # that says where a macro body was expanded is asserted the same way a finding
    # is, and with a location -- because the location IS the content of the note.
    for code, line, col in exp.notes:
        hit = [g for g in notes if g[0] == code]
        if not hit:
            fails.append("%s: expected note %s, got %s"
                         % (name, code, sorted(set(g[0] for g in notes))))
            continue
        if line is not None:
            if not any(g[1] == line and (col is None or g[2] == col) for g in hit):
                fails.append("%s: note %s at %s, expected %d:%s"
                             % (name, code, [(g[1], g[2]) for g in hit], line,
                                col if col is not None else "*"))

    # EVERY REPORTED FINDING IS EXPECTED (D-237, 1.4.8b). The two loops above
    # ask whether each expectation was met; this asks the converse -- whether
    # anything was reported that no expectation names -- and the SET of codes
    # on the error channel (findings, with `warning` counted as one) must
    # EQUAL the set the expectations name. BUILD_REFERENCE §7.1 said so from
    # the day it was written and neither runner enforced it: the subset rule
    # carried from 0.8 to 1.4.8 let seventeen rejection files report a code
    # nobody asserted -- nine from one resolve_check defect, two expectations
    # still spelling a code 1.4.2 retired, two failsafes written before D-210,
    # and cascades the tests never named. Notes keep their own rule (an
    # unexpected note is not a finding: MACRO-009 says where a body was
    # expanded, and every expansion test would otherwise name a location).
    expected = set(c for c, _, _ in exp.errors)
    for code in sorted(set(g[0] for g in got)):
        if code not in expected:
            fails.append("%s: reported %s, which no expectation names -- an "
                         "unexpected diagnostic fails a test as surely as a "
                         "missing one (BUILD_REFERENCE §7.1, D-237)"
                         % (name, code))
    return fails


def check_type_rejection(binary, path, name, exp):
    """A whole program that must be refused BY THE TYPE CHECKER, with these codes.

    Three rejection suites, three stages, and the split is the whole point:

      tests/modules/rejection/  refused by the LOADER -- never reaches a checker
      tests/types/rejection/    refused HERE -- it loads and resolves, and a type
                                rule says no
      tests/rejection/          refused by the BACKEND -- it is a correct program
                                at a rung that cannot lower it yet (D-085)

    Collapsing any two of them would make "correctly refused" mean less, because
    a file that stops early would satisfy a test written about a later stage.
    A file here that RESOLVES but does not TYPE-CHECK is the only thing this
    suite accepts as a pass.
    """
    return check_module_rejection(binary, path, name, exp)


def check_type_accept(binary, path, name):
    """A whole program the frontend must accept, with NO diagnostics at all.

    The counterweight to the three rejection suites. Those establish that a rule
    fires; this establishes that it fires only when it should, which no number of
    negative cases can show -- a checker that refused every program would pass
    every one of them.

    Any diagnostic is a failure, whatever its code: a program here is correct, so
    there is nothing for the frontend to say about it.
    """
    try:
        r = subprocess.run([binary, path], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return ["%s: the frontend did not terminate" % name]
    if r.returncode == 3:
        return ["%s: the frontend TRAPPED -- a defect in the compiler, not in "
                "this file" % name]
    if r.returncode == 0:
        return []
    got = sorted(set(line.split()[0]
                     for line in r.stderr.decode("utf-8", "replace").splitlines()
                     if line.split()))
    return ["%s: expected no diagnostics, got %s" % (name, got or r.returncode)]


# --- the real parser, on real files ------------------------------------------

PARSE_CHECK = os.path.join(ROOT, "tools", "parse_check.npk")
RESOLVE_CHECK = os.path.join(ROOT, "tools", "resolve_check.npk")
TYPE_CHECK = os.path.join(ROOT, "tools", "check.npk")


def check_parses(binary, path, name):
    """The real parser must accept this file with no diagnostics at all.

    The file is passed BY PATH rather than piped (0.3.0). That is not cosmetic:
    it means the compiler opens it, so this exercises `read_file`, `to_cstring`
    and the source manager on every run, and a span's `file` field is a real
    entry in that manager rather than a hardcoded zero.
    """
    try:
        r = subprocess.run([binary, path], capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return ["%s: the real parser did not terminate" % name]
    if r.returncode == 2:
        return ["%s: parse_check could not read the file: %s"
                % (name, r.stderr.decode("utf-8", "replace").strip())]
    if r.returncode == 3:
        return ["%s: the real parser TRAPPED -- a defect in the compiler, not in "
                "this file" % name]
    if r.returncode != 0:
        got = r.stderr.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: the REAL parser rejected it (%s) -- every file here must "
                "parse and be refused later, which is what D-085 says" % (name, got)]
    return []


# The COMPILER ITSELF: src/main.npk is npkc's entry (nitpick.toml [build]), and
# the harness builds and runs it the way a build system will -- IR on stdout,
# llc and ld.lld after.
EMIT_CHECK = os.path.join(ROOT, "src", "main.npk")


def undefined_symbols(obj):
    """Every symbol this object needs someone else to provide."""
    r = subprocess.run(["llvm-readelf", "-s", obj], capture_output=True, text=True)
    out = set()
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 8 and parts[6] == "UND" and parts[7]:
            out.add(parts[7])
    return out


def check_zero_dependency(obj, allow, name):
    """D-011, enforced mechanically: no object may need a symbol the project does
    not itself provide.

    LLVM emits calls behind the program's back -- `__divti3` for i128 division,
    `memcpy`/`memset` for large struct copies -- and each is a zero-dependency
    violation that would otherwise surface as a link error on whatever target
    first lacks the symbol. This check turns the rule into a named build failure
    at the object, with the symbol in the message. The allowlist is not a config
    file: it is parsed out of npkrt.ll's own defines, so a symbol is allowed
    exactly when the project's runtime provides it.
    """
    extra = undefined_symbols(obj) - allow
    if not extra:
        return []
    return ["%s: needs symbol(s) the project does not provide: %s -- a "
            "zero-dependency violation (D-011); either the emitter must stop "
            "producing the reference or npkrt.ll must define it as hand-written, "
            "verified IR" % (name, ", ".join(sorted(extra)))]


DEFINE_RE = re.compile(r"^\s*define[^@]*(@(?:\"[^\"]*\"|[\w.$-]+))\s*\(",
                       re.MULTILINE)
GLOBAL_RE = re.compile(r"^\s*(@(?:\"[^\"]*\"|[\w.$-]+))\s*=\s*(?!.*\bexternal\b)",
                       re.MULTILINE)


def check_symbols_unique(ll_text, name):
    """No symbol may be DEFINED twice in one emitted module.

    D-156 says a symbol collision "is caught at emit by the generalized collision
    check"; that forward reference pointed at a check retired at 1.0.1, so from
    1.0.1 until 1.0.5b nothing was looking. What it would have caught: a method's
    symbol did not name its impl, so two impls of one trait on different types
    both rendered `@"npk.<module>.<name>"` -- invalid LLVM for a program with
    nothing wrong with it.

    `llc` reports the redefinition too, and that is not a reason to skip this.
    llc says a name is defined twice; the invariant this states is the
    compiler's, in the compiler's terms, and it runs over the self-compile as
    well -- where a duplicate would otherwise surface as one line of llc output
    about a sixty-module artifact.
    """
    seen = {}
    dupes = []
    for pat in (DEFINE_RE, GLOBAL_RE):
        for m in pat.finditer(ll_text):
            sym = m.group(1)
            if sym in seen:
                dupes.append(sym)
            seen[sym] = True
    if not dupes:
        return []
    return ["%s: %s defined more than once in one module -- the emitter "
            "produced two definitions of one symbol, which is invalid LLVM and "
            "means two declarations rendered the same name (D-156)"
            % (name, ", ".join(sorted(set(dupes))))]


def check_allocas_hoisted(ll_text, name):
    """Every `alloca` is in its function's entry block (D-173, 1.0.9a).

    An `alloca` in a loop body allocates stack each iteration and reclaims none
    until return, so a walk that scales with the program overflows the stack --
    which is how the seed-built npkc came to segfault compiling `src/`. The fix
    hoists every alloca to the entry block; this pins it, so a later lowering
    that writes an alloca inline instead of through `irw_alloca`/`self.alloca`
    fails here rather than as a crash on a large input. The entry block is
    everything from `entry:` to the first terminator; an alloca after any
    branch, return, or later label is in a body block.
    """
    bad = []
    fn = None
    in_entry = False
    for raw_line in ll_text.splitlines():
        line = raw_line.strip()
        m = DEFINE_RE.search(raw_line)
        if m:
            fn = m.group(1)
            in_entry = False
            continue
        if fn is None:
            continue
        if line == "entry:":
            in_entry = True
            continue
        if line.endswith(":") and " " not in line:   # a basic-block label
            in_entry = False
            continue
        if line.startswith("br ") or line.startswith("ret ") or line == "unreachable":
            in_entry = False
            continue
        if re.match(r"%\S+ = alloca ", line) and not in_entry:
            bad.append("%s: `%s` in `%s` is not in the entry block -- an alloca "
                       "in a loop body overflows the stack over a large input; "
                       "route it through irw_alloca (D-173)"
                       % (name, line, fn))
    return bad


def runtime_allowlist():
    """Every symbol npkrt.ll defines, plus `main` -- which is the one symbol the
    RUNTIME is allowed to need, because the program provides it."""
    return set(_npkrt_defines().keys()) | {"main"}


def check_backend_rejection(binary, path, name, exp):
    """A correct program the BACKEND must refuse with NITPICK-RUNG-001 (D-085).

    Until 0.7.7 this suite could only be run against the SEED's rungs, because
    the seed was the only backend. `tools/emit_check.npk` is the real one: the
    file must pass the whole frontend -- parse, resolve, type-check, analyse --
    and be refused by EMISSION, naming the construct and the rung. A file that
    fails earlier is a test of the wrong stage and reports as one.
    """
    return check_module_rejection(binary, path, name, exp)


def emit_and_link(binary, path, name, base, tmp):
    """Compile `path` with the REAL backend and link it to the binary at `base`.

    The seed is nowhere in this path: `emit_check` loads, checks and EMITS with
    the real compiler, and what runs is what `emit_program` wrote. Shared by the
    program sweep and the FIXTURES build (a helper binary a test spawns), so a
    fixture is held to every check a program is -- symbol uniqueness, hoisted
    allocas, the zero-dependency scan.
    """
    # THE TIMEOUT IS CALIBRATED FOR WHAT THIS NOW COMPILES (1.4.6). It was 30
    # seconds, which was generous for `tests/backend/programs/` -- standalone
    # programs that compile in about 40 ms. The switch pointed the conformance,
    # frontend and backend suites here too, and those `use` the compiler's own
    # modules: `tests/frontend/resolve.npk` takes 11.6 s on an idle machine,
    # which is comfortably inside 30 until the machine is not idle. 300 still
    # catches a hang and stops being a measurement of load.
    try:
        r = subprocess.run([binary, path], capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        return ["%s: emit_check did not terminate" % name]
    if r.returncode == 3:
        return ["%s: the compiler TRAPPED -- a defect in it, not in this file"
                % name]
    if r.returncode != 0:
        got = r.stderr.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: expected IR, got a refusal (%s)" % (name, got)]
    with open(base + ".ll", "wb") as fh:
        fh.write(r.stdout)
    ir_text = r.stdout.decode("utf-8", "replace")
    dupes = check_symbols_unique(ir_text, name)
    if dupes:
        return dupes
    hoist = check_allocas_hoisted(ir_text, name)
    if hoist:
        return hoist
    r = subprocess.run(["llc"] + LLC_FLAGS + [
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
        return ["%s: llc rejected the REAL BACKEND's IR: %s"
                % (name, first.strip()[:160])]
    fails = check_zero_dependency(base + ".o", runtime_allowlist(), name)
    if fails:
        return fails
    r = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", base, base + ".o",
                        os.path.join(tmp, "npkrt.o")], capture_output=True, text=True)
    if r.returncode != 0:
        return ["%s: link failed: %s" % (name, r.stderr.strip()[:140])]
    return []


def check_emitted_program(binary, path, name, exp, tmp, fixture_map=None):
    """A program COMPILED BY THIS COMPILER'S BACKEND, run, and its exit compared.

    A byte-pin proves the text is stable; only execution proves the text means
    what the source said. `// stress: N` runs it N times and requires the SAME
    answer every time -- the loop lives HERE, in the stage that actually runs
    these programs (its first home, `check_positive`, is the seed's path, which
    none of the concurrency programs ever took: the marker was silently dead
    for the whole real-backend suite until 1.1.13a). `// argv:` tokens resolve
    through the fixture map (built helper binaries) or pass verbatim.
    """
    base = os.path.join(tmp, "prog_" + os.path.basename(path).replace(".", "_"))
    fails = emit_and_link(binary, path, name, base, tmp)
    if fails:
        return fails
    run = [base]
    for tok in exp.argv:
        run.append((fixture_map or {}).get(tok, tok))
    seen = {}
    for _ in range(max(1, exp.stress)):
        try:
            got = subprocess.run(run, capture_output=True, timeout=10).returncode
        except subprocess.TimeoutExpired:
            return ["%s: timed out" % name]
        if got != exp.exit:
            seen[got] = seen.get(got, 0) + 1
    if seen:
        if exp.stress > 1:
            detail = ", ".join("%d x%d" % (rc, n) for rc, n in sorted(seen.items()))
            return ["%s: expected %d every run, got %s in %d runs -- a "
                    "schedule-dependent answer is a bug that passes most of "
                    "the time (compiled by the REAL backend)"
                    % (name, exp.exit, detail, exp.stress)]
        return ["%s: exited %d, expected %d (compiled by the REAL backend)"
                % (name, list(seen)[0], exp.exit)]
    return check_optimised_program(name, exp, base, tmp, run)


def check_optimised_program(name, exp, base, tmp, run):
    """The SAME program through `opt -O2` and `llc -O2`, same answer required.

    LLVM's optimiser once removed a load-bearing guarantee from the prototype
    compiler, and the workaround there -- disabling optimisation for the
    affected integer types -- is exactly the kind of silent semantic fork this
    project refuses. So the optimised pipeline is not trusted, it is TESTED:
    every real-backend program runs twice, and a program whose answer changes
    under -O2 is a program whose emitted IR leans on behaviour the optimiser
    is licensed to remove (1.3.8; poison-carrying flags, dead-store
    elimination around escaped locals, and vector widening are the known
    suspects). The zero-dependency scan runs on the optimised object too,
    because `opt` is licensed to MINT libcalls -- a vectorised loop becoming
    `memcpy` is a new undefined symbol the -O0 scan never saw.
    """
    if not shutil.which("opt"):
        return ["%s: `opt` is not on PATH -- the optimised-output check "
                "cannot run, and skipping it silently is how an optimiser "
                "defect ships (install LLVM's opt, CLAUDE.md lists the "
                "symlink set)" % name]
    r = subprocess.run(["opt"] + OPT_FLAGS + [base + ".ll", "-o", base + ".opt.ll"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if l.strip()), r.stderr)
        return ["%s: opt -O2 rejected the emitted IR: %s"
                % (name, first.strip()[:160])]
    r = subprocess.run(["llc"] + LLC_OPT_FLAGS + [base + ".opt.ll", "-o", base + ".opt.o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
        return ["%s: llc -O2 rejected the OPTIMISED IR: %s"
                % (name, first.strip()[:160])]
    fails = check_zero_dependency(base + ".opt.o", runtime_allowlist(),
                                  name + " (opt -O2)")
    if fails:
        return fails
    r = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", base + ".opt", base + ".opt.o",
                        os.path.join(tmp, "npkrt.o")], capture_output=True, text=True)
    if r.returncode != 0:
        return ["%s: optimised link failed: %s" % (name, r.stderr.strip()[:140])]
    orun = [base + ".opt"] + run[1:]
    seen = {}
    for _ in range(max(1, exp.stress)):
        try:
            got = subprocess.run(orun, capture_output=True, timeout=10).returncode
        except subprocess.TimeoutExpired:
            return ["%s: timed out under -O2 -- an optimised hang where -O0 "
                    "terminated is an optimiser interaction, not a flake" % name]
        if got != exp.exit:
            seen[got] = seen.get(got, 0) + 1
    if seen:
        if exp.stress > 1:
            detail = ", ".join("%d x%d" % (rc, n) for rc, n in sorted(seen.items()))
            return ["%s: expected %d every run under -O2, got %s in %d runs "
                    "(the -O0 build answered correctly -- the optimiser "
                    "changed the program)"
                    % (name, exp.exit, detail, exp.stress)]
        return ["%s: exited %d under -O2, expected %d (the -O0 build answered "
                "correctly -- the optimiser changed the program)"
                % (name, list(seen)[0], exp.exit)]
    return []


def first_difference(a, b):
    """Where two byte strings diverge, in the terms a reproducibility failure
    needs (D-204, 1.4.5): the offset, and enough context to recognise it.

    A diff of two 40 MB IR files is not evidence anybody reads. The offset plus
    the two bytes is what identifies the class -- a path fragment, a pointer
    printed as a value, a reordered symbol -- and the length difference is
    usually the giveaway when the divergence is structural rather than local.
    """
    n = min(len(a), len(b))
    i = 0
    while i < n and a[i] == b[i]:
        i += 1
    if i == n and len(a) == len(b):
        return "identical"
    lo = max(0, i - 40)
    ctx = a[lo:i].decode("utf-8", "replace").replace("\n", "\\n")
    ga = a[i:i + 30].decode("utf-8", "replace").replace("\n", "\\n")
    gb = b[i:i + 30].decode("utf-8", "replace").replace("\n", "\\n")
    return ("first differs at byte %d of %d/%d, after %r: %r vs %r"
            % (i, len(a), len(b), ctx, ga, gb))


# --- the toolchain pin, checked (D-204, 1.4.5) -------------------------------

_VER_RE = {
    # `llc --version` / `opt --version`: "Ubuntu LLVM version 20.1.2"
    "llc": re.compile(r"LLVM version\s+(\d+\.\d+\.\d+)"),
    "opt": re.compile(r"LLVM version\s+(\d+\.\d+\.\d+)"),
    # `ld.lld --version`: "Ubuntu LLD 20.1.2 (compatible with GNU linkers)"
    "ld.lld": re.compile(r"LLD\s+(\d+\.\d+\.\d+)"),
}


def check_toolchain_pin():
    """Every tool that touches the artifact is the pinned version (D-204).

    `llvm-config` is NOT used: it ships in a -dev package the build does not
    otherwise need, and asking a tool we already require for its own version is
    both fewer dependencies and a more honest question -- the version that
    matters is the one that will run, not the one a sibling binary reports.

    A patch release counts. R-4's finding is that pinning to a minor version is
    insufficient for strict byte-identity, so 20.1.2 and 20.1.3 are different
    toolchains and the run says so rather than producing output that cannot be
    reproduced. `opt` is optional-by-absence elsewhere (the -O2 leg says so
    loudly), so it is only checked when present.
    """
    if not LLVM_PIN:
        return ["nitpick.toml has no [toolchain] llvm pin -- D-204 makes the "
                "toolchain a build INPUT, and an unpinned input is not one"]
    fails = []
    for tool in ("llc", "opt", "ld.lld"):
        if not shutil.which(tool):
            if tool == "opt":
                continue      # the -O2 leg reports its own absence
            fails.append("%s is not on PATH" % tool)
            continue
        try:
            r = subprocess.run([tool, "--version"], capture_output=True,
                               text=True, timeout=30)
        except (OSError, subprocess.SubprocessError) as e:
            fails.append("%s --version failed: %s" % (tool, e))
            continue
        m = _VER_RE[tool].search(r.stdout)
        if not m:
            fails.append("%s --version did not report a version this can read: "
                         "%r" % (tool, r.stdout.strip()[:120]))
            continue
        if m.group(1) != LLVM_PIN:
            fails.append("%s is %s but nitpick.toml pins %s -- the toolchain is "
                         "a BUILD INPUT (D-204), and a patch release is enough "
                         "to change instruction selection or section ordering. "
                         "Install the pinned version, or update the pin AND "
                         "regenerate every expected hash in the same change"
                         % (tool, m.group(1), LLVM_PIN))
    return fails


# --- driver ------------------------------------------------------------------

# THE STAGES A `[[test]]` ENTRY MAY NAME (D-238, 1.4.8b): the tool that judges
# the suite and what it must say. `compile` is the default and the only stage
# with a `kind`. Both runners carry this list and refuse anything outside it.
STAGES = ("compile", "parse", "resolve", "check", "accept", "fixture",
          "program", "runtime")
TEST_KEYS = ("name", "stage", "kind", "path", "paths", "recursive")


def load_targets():
    """Every `[[test]]` entry, validated and normalised (D-238, 1.4.8b).

    ONE read of the manifest, at import (D-204, 1.4.5): the toolchain pin
    needs it before any test runs, and two readers of one file is two chances
    to disagree about it. EVERY suite either runner runs is declared here -- a
    manifest that declared four of fourteen suites was a document a reader
    could not trust to say what ran (the stale-document shape D-204 refused
    for flags) -- so an entry the runner cannot honour is refused BY NAME
    before anything runs, never skipped: a stage this runner does not know, a
    `kind` on a stage that has none, a compile entry with no kind, no
    `paths`/`path` (or both), a `recursive` that is not a boolean, a key the
    schema lacks. Returns (targets, problem); `problem` is the one sentence to
    print before exiting 2. Paths come back absolute."""
    out = []
    for i, t in enumerate(MANIFEST.get("test", [])):
        where = "nitpick.toml [[test]] #%d" % (i + 1)
        if not isinstance(t, dict):
            return None, "%s is not a table" % where
        name = t.get("name")
        if not isinstance(name, str) or not name:
            return None, "%s needs a `name` (a string)" % where
        where += " (`%s`)" % name
        for k in t:
            if k not in TEST_KEYS:
                return None, ("%s: `%s` is not a key a [[test]] entry has; the "
                              "keys are name, stage, kind, paths (or path), "
                              "recursive (BUILD_REFERENCE §7.1, D-238)"
                              % (where, k))
        stage = t.get("stage", "compile")
        if stage not in STAGES:
            return None, ("%s: stage `%s` is not one this runner knows -- the "
                          "stages are %s (D-238); a stage the runner cannot "
                          "judge is refused, not skipped"
                          % (where, stage, ", ".join(STAGES)))
        kind = t.get("kind")
        if stage == "compile":
            if kind not in KINDS:
                return None, ("%s: a compile-stage target needs `kind` "
                              "positive, negative or diagnostic" % where)
        elif kind is not None:
            return None, ("%s: `kind` belongs to the compile stage only; stage "
                          "`%s` judges every file one way" % (where, stage))
        if "paths" in t and "path" in t:
            return None, "%s: `paths` or `path`, not both" % where
        if "paths" in t:
            paths = t["paths"]
            if (not isinstance(paths, list) or not paths
                    or not all(isinstance(q, str) and q for q in paths)):
                return None, ("%s: `paths` must be a non-empty array of "
                              "strings" % where)
        elif "path" in t:
            if not isinstance(t["path"], str) or not t["path"]:
                return None, "%s: `path` must be a string" % where
            paths = [t["path"]]
        else:
            return None, ("%s needs `paths` (or its one-element shorthand "
                          "`path`)" % where)
        recursive = t.get("recursive", False)
        if not isinstance(recursive, bool):
            return None, "%s: `recursive` must be true or false" % where
        out.append({"name": name, "stage": stage, "kind": kind,
                    "paths": [os.path.join(ROOT, q) for q in paths],
                    "recursive": recursive})
    return out, None


def files_of(t, suffix=".npk"):
    """Every file with `suffix` under the entry's paths, EACH ONCE, in manifest
    path order and sorted within a path -- `glob` per path, `**` when the
    entry says `recursive`. The hardcoded sweeps concatenated their lists, so
    the grammar sweep parsed six files twice; a table sweeps a file once."""
    seen = set()
    out = []
    for d in t["paths"]:
        if t["recursive"]:
            found = glob.glob(os.path.join(d, "**", "*" + suffix), recursive=True)
        else:
            found = glob.glob(os.path.join(d, "*" + suffix))
        for p in sorted(found):
            ap = os.path.abspath(p)
            if ap not in seen:
                seen.add(ap)
                out.append(p)
    return out


class Session:
    """What one run carries between stages: the temp dir and tool availability,
    the tools built so far (once each, kept), the fixture map the `fixture`
    stage fills and the `program` stage reads, the counts and the failures."""
    def __init__(self, tmp, tools):
        self.tmp = tmp
        self.tools = tools
        self.built = {}
        self.fixture_map = {}
        self.total = 0
        self.available = 0
        self.failures = []


TOOL_SOURCES = {"parse_check": os.path.join(ROOT, "tools", "parse_check.npk"),
                "resolve_check": os.path.join(ROOT, "tools", "resolve_check.npk"),
                "check": os.path.join(ROOT, "tools", "check.npk")}


def tool_for(s, which):
    """The tool a stage judges with, built on first use and kept. A tool that
    did not build is one recorded failure and every stage that needs it
    skips; None also when llc/ld.lld are absent (nothing built, as today)."""
    if which not in s.built:
        made = build_tool(s.tmp, s.tools, TOOL_SOURCES[which], which)
        if made is None:
            s.built[which] = None
        elif isinstance(made, str) and not os.path.exists(made):
            s.failures.append("tools/%s.npk did not build: %s" % (which, made))
            s.built[which] = None
        else:
            s.built[which] = made
    return s.built[which]


# --- the stages (D-238): one function per stage, the entry decides the files ----

def stage_compile(t, s, only):
    """The compiler under test over every standalone file, held to the entry's
    kind. A file some other file in the suite imports is a fixture and is not
    run. `--only` narrows here and nowhere else; the counts are these."""
    kind = t["kind"]
    paths = files_of(t)
    skip = imported_by_others(paths)
    run_paths = [p for p in paths if os.path.abspath(p) not in skip]
    s.available += len(run_paths)
    if only:
        run_paths = [p for p in run_paths
                     if any(sub in os.path.relpath(p, ROOT) for sub in only)]
        if not run_paths:
            return
    for p in run_paths:
        s.total += 1
        name = os.path.relpath(p, ROOT)
        exp = read_expectations(p)
        s.failures += record_verdict(t["name"], name,
                                     KINDS[kind](name, p, exp, s.tmp, s.tools))
    print("  %-11s %2d %s test(s)" % (t["name"], len(run_paths), kind))


def stage_parse(t, s):
    """Every source under the entry's paths through the REAL parser, accepted
    with no diagnostic.

    A rejection test the real parser cannot read is not testing D-085's rule.
    tests/grammar/ is never compiled and never run -- it exists only to be
    parsed, which is what lets it use the whole language. The prelude and the
    compiler's own source are here too, and the latter is the file set that
    matters most: when the sweep first reached it (0.7.3), 22 OF 62 FILES WERE
    REJECTED AND FIVE CRASHED THE PARSER -- a qualifier never learned on a
    field, `dn`/`bn`/`cn`/`tt` used as names when the grammar made them
    numeric literals, an out-of-bounds read in `ralloc` past about 511
    declarations -- and every one was invisible for as long as nobody asked.
    Stage 1 must parse these files to build stage 2, so a source the real
    parser cannot read is a source that never self-hosts. `npkg/` is source
    the parser must read for the same reason (1.4.8)."""
    pc = tool_for(s, "parse_check")
    if not pc:
        return
    n = 0
    for p in files_of(t):
        name = os.path.relpath(p, ROOT)
        s.failures += record_verdict(t["name"], name, check_parses(pc, p, name))
        n += 1
    print("  %-11s %2d real-parser check(s)" % (t["name"], n))


def stage_rejection(t, s, which):
    """Whole programs that must be refused with EXACTLY the expected codes by
    the tool the stage names: `resolve` is the LOADER (tools/resolve_check),
    `check` is the whole frontend (tools/check -- the type checker, an
    analysis, expansion, derive, whichever the suite's directory says). The
    split is the point (see `check_type_rejection`): a file that stops earlier
    would satisfy a test written about a later stage. A file with no
    `expect-error:` is a fixture another one imports, not a test."""
    tool = tool_for(s, which)
    if not tool:
        return
    judge = check_module_rejection if which == "resolve_check" else check_type_rejection
    n = 0
    for p in files_of(t):
        exp = read_expectations(p)
        if not exp.errors:
            continue
        name = os.path.relpath(p, ROOT)
        s.failures += record_verdict(t["name"], name, judge(tool, p, name, exp))
        n += 1
    print("  %-11s %2d rejection test(s), refused by %s"
          % (t["name"], n, "the loader" if which == "resolve_check" else "the frontend"))


def stage_accept(t, s):
    """Whole programs the frontend must ACCEPT, in full silence. A rejection
    suite cannot tell a correct checker from one that refuses everything, and
    cycle 0.5's analyses fail closed by design, so over-refusal is the failure
    mode they are likeliest to have. One suite, not one per stage: silence has
    no stage."""
    tc = tool_for(s, "check")
    if not tc:
        return
    n = 0
    for p in files_of(t):
        name = os.path.relpath(p, ROOT)
        s.failures += record_verdict(t["name"], name, check_type_accept(tc, p, name))
        n += 1
    print("  %-11s %2d acceptance test(s)" % (t["name"], n))


def stage_fixture(t, s):
    """Helper binaries a test spawns (1.1.13a): every `.npk` here is built by
    the same real-backend pipeline as a program -- and held to the same checks
    -- but never run by the harness itself; a test names one in `// argv:` by
    its uppercased stem (MOCK_DRIVER) and receives the built binary's absolute
    path in its own argv (`Path` refuses relative paths by design). Built into
    .internal/ so the path is stable across runs. A `.c` here is a reference
    DRIVER (1.1.13c), built with the system C compiler: TEST TOOLING, never in
    the artifact -- the zero-dependency rule governs what ships, and a driver
    is outside the TCB by definition (D-149)."""
    fixdir = os.path.join(ROOT, ".internal", "fixtures")
    npks = files_of(t, ".npk")
    cs = files_of(t, ".c")
    if npks or cs:
        os.makedirs(fixdir, exist_ok=True)
    for p in npks:
        stem = os.path.basename(p)[:-4]
        fbase = os.path.join(fixdir, stem)
        fails = record_verdict(t["name"], os.path.relpath(p, ROOT),
                               emit_and_link(COMPILER, p, os.path.relpath(p, ROOT),
                                             fbase, s.tmp))
        if fails:
            s.failures += fails
            continue
        s.fixture_map[stem.upper()] = fbase
    for p in cs:
        stem = os.path.basename(p)[:-2]
        fbase = os.path.join(fixdir, stem)
        r = subprocess.run(["cc", "-O1", "-Wall", "-o", fbase, p],
                           capture_output=True, text=True)
        cfails = []
        if r.returncode != 0:
            cfails = ["%s: cc failed: %s"
                      % (os.path.relpath(p, ROOT), r.stderr.strip()[:160])]
        record_verdict(t["name"], os.path.relpath(p, ROOT), cfails)
        if cfails:
            s.failures += cfails
            continue
        s.fixture_map[stem.upper()] = fbase
    print("  %-11s %2d fixture(s) built, never run" % (t["name"], len(npks) + len(cs)))


def stage_program(t, s):
    """Whole programs COMPILED BY THE REAL BACKEND, linked against the runtime,
    RUN at -O0 and again through `opt -O2`. A byte-pin proves the text is
    stable; only execution proves the text means what the source said. The
    conformance suite is in this stage, which is the self-hosting goal
    sentence made a test. A file another one imports is a fixture (same_name_a/b,
    D-162, were the first multi-file programs). Fixtures named in `// argv:`
    resolve through the map the `fixture` stage filled -- so that entry runs
    first in the manifest."""
    paths = files_of(t)
    skip = imported_by_others(paths)
    n = 0
    for p in paths:
        if os.path.abspath(p) in skip:
            continue
        exp = read_expectations(p)
        name = os.path.relpath(p, ROOT)
        s.failures += record_verdict(t["name"], name,
                                     check_emitted_program(COMPILER, p, name, exp,
                                                           s.tmp, s.fixture_map))
        n += 1
    print("  %-11s %2d real-backend program(s)" % (t["name"], n))
    print("  %-11s every program re-run through opt -O2 + llc -O2, "
          "same exit required" % "opt-O2")


def stage_runtime(t, s):
    """Hand-written .ll drivers for runtime families with NO surface syntax
    (0.10.3; the frame allocator is the coroutine machinery's, not a
    program's). Each defines main + npk_failsafe, links against the same
    npkrt.o everything else does, runs, and asserts an exit code read from
    `expect-exit:` in its first 400 bytes -- the same execution standard the
    program suite holds."""
    n = 0
    for rp in files_of(t, ".ll"):
        rexp = 0
        with open(rp, encoding="utf-8") as fh:
            rhead = fh.read(400)
        rm = re.search(r"expect-exit:\s*(\d+)", rhead)
        if rm:
            rexp = int(rm.group(1))
        rbase = os.path.join(s.tmp, "rt_" + os.path.basename(rp)[:-3])
        rname = os.path.relpath(rp, ROOT)
        r = subprocess.run(["llc"] + LLC_FLAGS + [rp, "-o", rbase + ".o"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            s.failures += record_verdict(t["name"], rname,
                                         ["%s: llc failed: %s"
                                          % (rname, r.stderr.strip()[:120])])
            continue
        r = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", rbase,
                            rbase + ".o", os.path.join(s.tmp, "npkrt.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            s.failures += record_verdict(t["name"], rname,
                                         ["%s: link failed: %s"
                                          % (rname, r.stderr.strip()[:120])])
            continue
        r = subprocess.run([rbase], capture_output=True)
        rfails = []
        if r.returncode != rexp:
            rfails = ["%s: exited %d, expected %d" % (rname, r.returncode, rexp)]
        s.failures += record_verdict(t["name"], rname, rfails)
        n += 1
    print("  %-11s %2d runtime-floor test(s)" % (t["name"], n))


def run_stage(t, s, only):
    """Dispatch on the entry's stage. `compile` is the only stage `--only` can
    select; a filtered run skips every other stage, saying so -- today's
    behaviour, by stage rather than by position in this file."""
    stage = t["stage"]
    if stage == "compile":
        stage_compile(t, s, only)
    elif stage == "parse":
        stage_parse(t, s)
    elif stage == "resolve":
        stage_rejection(t, s, "resolve_check")
    elif stage == "check":
        stage_rejection(t, s, "check")
    elif stage == "accept":
        stage_accept(t, s)
    elif stage == "fixture":
        stage_fixture(t, s)
    elif stage == "program":
        stage_program(t, s)
    elif stage == "runtime":
        stage_runtime(t, s)
    else:
        # load_targets refused everything else before any test ran.
        raise AssertionError("unknown stage reached run_stage: %s" % stage)


USAGE = """usage: python3 bootstrap/harness/harness.py [--only SUBSTR]...

  (no arguments)    run everything -- the only run whose result means the suite
                    is green, and the only kind to commit on
  --only SUBSTR     run only the compile-stage tests whose repo-relative path
                    contains SUBSTR. Repeatable. Skips every other stage and
                    every whole-suite check, and says so.
  --verdicts PATH   also write every unit's verdict, one
                    `STATUS<TAB>suite<TAB>name<TAB>message` per line -- the
                    list the parity stage diffs against `npkg test --verdicts`
  -h, --help        this"""


class Options:
    def __init__(self):
        self.only = []
        self.help = False
        self.error = None
        self.verdicts = None


def parse_args(args):
    """Hand-rolled rather than argparse, for the same reason everything else here
    is: this file is throwaway and its replacement (`npkg test`) will not inherit
    a line of it. Two flags do not justify a dependency on argparse's behaviour
    around abbreviation and prefix matching, which is exactly the kind of thing
    that makes `--onl` silently mean something."""
    o = Options()
    i = 0
    while i < len(args):
        a = args[i]
        if a in ("-h", "--help"):
            o.help = True
        elif a == "--only":
            i += 1
            if i >= len(args):
                o.error = "--only needs a substring to match test paths against"
                return o
            o.only.append(args[i])
        elif a.startswith("--only="):
            value = a[len("--only="):]
            if not value:
                o.error = "--only= needs a substring after the `=`"
                return o
            o.only.append(value)
        elif a == "--verdicts":
            i += 1
            if i >= len(args):
                o.error = "--verdicts needs a path to write the verdict list to"
                return o
            o.verdicts = args[i]
        else:
            o.error = "unknown argument: %s" % a
            return o
        i += 1
    return o


def main(argv):
    opts = parse_args(argv[1:])
    if opts.error:
        print("%s\n\n%s" % (opts.error, USAGE))
        return 2
    if opts.help:
        print(USAGE)
        return 0
    filtering = bool(opts.only)

    targets, problem = load_targets()
    if problem:
        print(problem)
        return 2
    if not targets:
        print("no [[test]] targets in nitpick.toml")
        return 2

    if filtering:
        print("PARTIAL RUN -- matching %s" % ", ".join(repr(s) for s in opts.only))

    tools = shutil.which("llc") and shutil.which("ld.lld")
    tmp = tempfile.mkdtemp(prefix="npk-harness-")
    if tools:
        r = subprocess.run(["llc"] + LLC_FLAGS + [
                            RUNTIME_LL, "-o", os.path.join(tmp, "npkrt.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("runtime floor did not compile: %s" % r.stderr.strip()[:200])
            return 1
        # THE BUILDER, THEN THE COMPILER UNDER TEST (D-203/D-205, 1.4.6).
        # Before any suite, because everything below asserts against the
        # compiler this pair produces -- and because a snapshot that cannot
        # build the current `src/` is D-205's rule being broken, which should
        # be the first thing the run says rather than the last.
        global BUILDER, COMPILER
        BUILDER = build_builder(tmp)
        if not os.path.exists(str(BUILDER)):
            print("the committed builder is unusable: %s" % BUILDER)
            return 1
        COMPILER = build_tool(tmp, tools, EMIT_CHECK, "npkc")
        if not COMPILER or not os.path.exists(str(COMPILER)):
            print("the snapshot could not build src/main.npk -- D-205: `src/` "
                  "may not use a construct its builder cannot compile. "
                  "Refresh the snapshot first (bootstrap/seed/README.md): %s"
                  % COMPILER)
            return 1
        print("  %-11s snapshot -> builder -> npkc (D-205)" % "builder")

    s = Session(tmp, tools)
    failures = s.failures

    # THE TOOLCHAIN FIRST (D-204, 1.4.5). It gates everything below that
    # assembles or links, and a version mismatch explains a byte-difference
    # that would otherwise look like a compiler defect -- so it is worth
    # knowing before an hour of evidence gets collected under the wrong tools.
    # (It stood after the compile targets until 1.4.8b; the table put every
    # suite in one loop, and the pin belongs before the loop.)
    if not filtering:
        failures += check_toolchain_pin()

    # EVERY SUITE, IN MANIFEST ORDER (D-238, 1.4.8b). The table in nitpick.toml
    # is the one list of what runs; `npkg test` reads the same table, and the
    # parity stage below diffs the two runners' verdicts unit for unit. A
    # filtered run touches the compile-stage entries only: the other stages
    # ask questions about the sources as a set -- does every file parse, is
    # every rejection exact -- which a subset cannot answer, so `--only` skips
    # them and says so.
    for t in targets:
        if filtering and t["stage"] != "compile":
            continue
        run_stage(t, s, opts.only)
    total = s.total
    available = s.available

    # A FILTER THAT MATCHES NOTHING IS AN ERROR, not a pass. `--only tpye_stmt`
    # otherwise reports `ok 0 test(s) passed` and reads exactly like success --
    # the one outcome a filter must never be able to produce.
    if filtering and total == 0:
        shutil.rmtree(tmp, ignore_errors=True)
        print("\nno test matched %s -- nothing ran"
              % ", ".join(repr(s) for s in opts.only))
        return 2

    # THE WHOLE-SUITE CHECKS, and they are all-or-nothing by nature: each asks a
    # question about the sources as a set -- is every node kind reachable from
    # SOME rule, does every file parse -- which a subset cannot answer. Running
    # them over whatever `--only` happened to match would give an answer to a
    # question nobody asked, so a filtered run does not run them at all.
    #
    # They are also where most of the minutes go, which is the cost `--only`
    # exists to avoid.
    if not filtering:
        failures += check_kinds_reachable()
        failures += check_kinds_typed()
        failures += check_kinds_lowered_or_refused()
        failures += check_codes_tested()
        failures += check_codes_centralised()
        failures += check_no_kind_literals()
        failures += check_identity_by_decl()
        failures += check_slot_sites_agree()
        failures += check_type_walkers_total()
        failures += check_one_renderer()
        failures += check_rung_names_open_cycle()
        failures += check_runtime_sigs_agree()
        failures += check_builtin_sig_texts()

        # The two standalone instruments that were wired to NOTHING until
        # 1.4.1 (found by the 1.4.0 survey): the harness's own self-check
        # (BUILD_REFERENCE §7.1 -- a suite that only agrees with what it is
        # handed is worse than no suite) and the token-table-vs-grammar diff.
        # Run as subprocesses so each stays independently runnable; output
        # surfaces only on failure.
        for script, what in (("spec_coverage.py", "spec-coverage"),
                             ("selfcheck.py", "harness-selfcheck")):
            r = subprocess.run(
                [sys.executable, os.path.join(ROOT, "bootstrap", "harness",
                                              script)],
                capture_output=True, text=True, cwd=ROOT)
            if r.returncode != 0:
                failures.append("%s: %s exited %d:\n%s"
                                % (what, script, r.returncode,
                                   (r.stdout + r.stderr).strip()))

        # REPORTED, never failing (0.9.1): stale-decision candidates need a
        # human eye -- but silence would be the lint not existing, so the count
        # prints on every full run and the list prints when non-empty.
        stale = check_decisions_current()
        if stale:
            print("  decisions-current: %d candidate(s) for the doc-sync pass:"
                  % len(stale))
            for c in stale:
                print("    ~ %s" % c)

        # THE SUITES RAN ABOVE, from the manifest (D-238). What follows are the
        # whole-tree instruments that are not suites and stay the harness's
        # until the SWITCH: the `never fails` twins, the fixpoint, the
        # reproducibility legs, the absent-fact flip, and parity with npkg.
        ec = COMPILER



        # D-163 IS INERT IN THE BACKEND (1.1.0): the twin programs under
        # conformance/nf_twin/ differ only in their `never fails` clauses
        # and must emit byte-identical IR -- the contract is a checked
        # claim, never a codegen hint.
        #
        # COMPILED AT THE SAME PATH, one after the other. Their own file
        # names are part of the emission -- D-179's site table records the
        # path a trap was stamped at -- so from 1.4.2b, when D-210's
        # overflow guard gave these twins their first stamped site, the
        # `with/` and `without/` directory names were the whole diff and
        # the check reported that `never fails` had changed the IR. It had
        # not; the fixture's own layout had. Copying each twin to one path
        # keeps the comparison about the clause and nothing else, without
        # eliding anything from the text.
        twa = os.path.join(ROOT, "tests", "conformance", "nf_twin",
                           "with", "nf_inert.npk")
        twb = os.path.join(ROOT, "tests", "conformance", "nf_twin",
                           "without", "nf_inert.npk")
        twdir = os.path.join(tmp, "nf_twin")
        os.makedirs(twdir, exist_ok=True)
        twone = os.path.join(twdir, "nf_inert.npk")
        shutil.copyfile(twa, twone)
        ra = subprocess.run([ec, twone], capture_output=True, text=True,
                            cwd=ROOT)
        shutil.copyfile(twb, twone)
        rb = subprocess.run([ec, twone], capture_output=True, text=True,
                            cwd=ROOT)
        if ra.returncode != 0 or rb.returncode != 0:
            failures.append("nf_twin: a twin failed to emit "
                            "(with=%d, without=%d)"
                            % (ra.returncode, rb.returncode))
        elif ra.stdout != rb.stdout:
            failures.append("nf_twin: `never fails` changed the emitted "
                            "IR -- D-163 is a checked claim, not a "
                            "codegen hint")
        else:
            print("  %-11s the `never fails` twins emit byte-identical IR"
                  % "nf-inert")


        # --- THE SELF-CHECK AND THE FIXPOINT (0.8.1) --------------------
        #
        # npkc compiles ITSELF: the whole compiler graph through its own
        # frontend, its own analyses, and its own emitter. The emitted IR is
        # assembled and linked into STAGE 1 -- a compiler with no seed in it
        # -- and stage 1 emits the compiler again. The two emissions must be
        # BYTE-IDENTICAL (D-078/D-202): the self-hosting fixpoint (cycle
        # 1.4; "1.2" under the numbering when this first closed at 0.8.1),
        # run as a standing instrument from that day. A drift here is
        # either nondeterminism or a semantic divergence between what the
        # seed built and what the compiler builds -- both are stop-the-line.
        # Stage 1 is emitted THROUGH `-o` — the flag's standing coverage is
        # its most important consumer — and stage 2 through stdout, so both
        # delivery paths are exercised and then compared byte-for-byte.
        s1 = os.path.join(tmp, "stage1")
        r = subprocess.run([ec, os.path.join(ROOT, "src", "main.npk"),
                            "-o", s1 + ".ll"],
                           capture_output=True, timeout=600)
        if r.returncode != 0 or not os.path.exists(s1 + ".ll"):
            got = r.stderr.decode("utf-8", "replace").strip()[:400]
            failures.append("npkc cannot compile ITSELF (via -o): %s" % got)
            print("  %-11s self-check FAILED" % ("selfhost",))
        elif r.stdout:
            failures.append("npkc -o wrote the IR to a file AND to stdout -- "
                            "one delivery, not two")
            print("  %-11s self-check FAILED" % ("selfhost",))
        else:
            with open(s1 + ".ll", "rb") as fh:
                stage1_ir = fh.read()
            ok = True
            # THE EMISSION IS PATH-FREE (D-236, 1.4.8). This compiler was
            # invoked with an ABSOLUTE `src/main.npk`, and every source path
            # in its site table must still render relative to the manifest
            # root. Before D-236 this leaked 1,479 of 1,637 rows, and the
            # `repro` stage below could not see it -- its two cwds agree by
            # construction when the argument is absolute. H9's leg, measuring
            # at last; the committed-snapshot guard further down stays as the
            # belt over the artifact.
            absfresh = len(re.findall(
                r'^@npk\.sitep\.\d+ = internal constant \[\d+ x i8\] c"/',
                stage1_ir.decode("utf-8", "replace"), re.M))
            if absfresh:
                failures.append(
                    "selfhost: the compiler's own emission carries %d ABSOLUTE "
                    "source paths in its site table -- D-236 says every path "
                    "renders relative to the manifest root whatever the "
                    "argument's spelling, so the emission depended on how the "
                    "compiler was invoked" % absfresh)
                ok = False
            # THE COMPILER'S OWN OUTPUT IS THE BIGGEST MODULE IT EMITS, so
            # it is the one where a duplicate symbol is hardest to read off
            # an llc error. Checked here first, in the compiler's terms.
            dupes = check_symbols_unique(
                stage1_ir.decode("utf-8", "replace"), "stage1.ll")
            if dupes:
                failures += dupes
                ok = False
            # D-173's defect was the SEED-BUILT npkc segfaulting on a
            # large input -- yet until 1.4.1 the alloca-hoisting pin ran
            # only on the little per-program emissions, never on the one
            # module big enough to overflow: the compiler's own.
            hoist = check_allocas_hoisted(
                stage1_ir.decode("utf-8", "replace"), "stage1.ll")
            if hoist:
                failures += hoist
                ok = False
            rr = subprocess.run(["llc"] + LLC_FLAGS + [s1 + ".ll", "-o", s1 + ".o"],
                                capture_output=True, text=True)
            if rr.returncode != 0:
                first = next((l for l in rr.stderr.splitlines()
                              if "error" in l), rr.stderr)
                failures.append("llc rejected the SELF-EMITTED compiler: %s"
                                % first.strip()[:160])
                ok = False
            if ok:
                failures += check_zero_dependency(
                    s1 + ".o", runtime_allowlist(), "stage1.o")
                # The runtime itself may need exactly TWO symbols: `main`
                # and `npk_failsafe` -- the program's entry and its
                # controlled-shutdown handler (D-013 makes failsafe
                # mandatory, D-142 routes runtime traps through it).
                # Anything else in npkrt's undefined set means the runtime
                # is not the floor -- it is standing on something.
                failures += check_zero_dependency(
                    os.path.join(tmp, "npkrt.o"),
                    {"main", "npk_failsafe"}, "npkrt.o")
                rr = subprocess.run(["ld.lld"] + LLD_FLAGS + ["-o", s1,
                                     s1 + ".o", os.path.join(tmp, "npkrt.o")],
                                    capture_output=True, text=True)
                if rr.returncode != 0:
                    failures.append("stage 1 failed to link: %s"
                                    % rr.stderr.strip()[:160])
                    ok = False
            if ok:
                rr = subprocess.run([s1, os.path.join(ROOT, "src", "main.npk")],
                                    capture_output=True, timeout=600)
                if rr.returncode != 0:
                    # THE RETURNCODE IS PART OF THE EVIDENCE: a negative one
                    # is a signal (-11 SIGSEGV), and an empty stderr with a
                    # positive code is a trap that reached failsafe. The
                    # binary is kept for the autopsy.
                    import shutil as _sh
                    _sh.copy(s1, os.path.join(ROOT, ".internal", "s1.failed"))
                    failures.append("STAGE 1 cannot compile the compiler: rc=%d %s"
                                    % (rr.returncode,
                                       rr.stderr.decode("utf-8", "replace")[:300]))
                    ok = False
                elif rr.stdout != stage1_ir:
                    failures.append("THE FIXPOINT DRIFTED: stage 1's emission "
                                    "of the compiler differs from the "
                                    "seed-built compiler's -- nondeterminism "
                                    "or a semantic divergence (D-078)")
                    ok = False
            if ok:
                print("  %-11s stage 1 rebuilt itself byte-identically"
                      % ("selfhost",))

            # --- REPRODUCIBILITY, TESTED (D-204, 1.4.5) --------------
            #
            # The fixpoint above already proves a great deal, but not
            # this: it compares TWO DIFFERENT BINARIES (the seed-built
            # compiler and stage 1), so "byte-identical" there means
            # they agree, not that either one is deterministic. Two
            # hazard classes hide in that gap, and R-4 names both:
            #
            #   H1  run-to-run variation from ASLR and hash iteration
            #       order -- the class with NO controlling flag, where
            #       testing is the only guard.
            #   H9  the build PATH leaking into the artifact. The
            #       compiler embeds source paths for real (D-179's
            #       origin-chain site tables), so this is not a
            #       hypothetical for us.
            #
            # The check: run the SAME binary again on the SAME absolute
            # inputs from a DIFFERENT working directory, and require the
            # same bytes. A difference is one of the two, and the
            # message says how to tell them apart.
            if ok:
                rdir = os.path.join(tmp, "repro-cwd")
                os.makedirs(rdir, exist_ok=True)
                rr = subprocess.run(
                    [ec, os.path.join(ROOT, "src", "main.npk")],
                    capture_output=True, timeout=600, cwd=rdir)
                rok = True
                if rr.returncode != 0:
                    failures.append(
                        "repro: the compiler failed when run from a "
                        "different working directory (rc=%d) -- its "
                        "output cannot depend on where it was invoked "
                        "from: %s"
                        % (rr.returncode,
                           rr.stderr.decode("utf-8", "replace")[:300]))
                    rok = False
                elif rr.stdout != stage1_ir:
                    failures.append(
                        "repro: THE EMISSION IS NOT REPRODUCIBLE -- the "
                        "same compiler on the same absolute inputs "
                        "emitted different bytes from a different "
                        "working directory (%s). Either the cwd leaked "
                        "into the artifact (D-204's H9; the site tables "
                        "are the place to look) or the run itself is "
                        "nondeterministic (H1: hash iteration order, an "
                        "address used as a value). Re-run twice from the "
                        "SAME directory to tell which"
                        % first_difference(stage1_ir, rr.stdout))
                    rok = False
                # AND `llc` ITSELF IS DETERMINISTIC on our input: the
                # same .ll assembled twice must give the same object.
                # This is H1 on the backend side, where we have no
                # source to inspect and only the output can answer.
                s1r = s1 + ".repro.o"
                rr = subprocess.run(["llc"] + LLC_FLAGS
                                    + [s1 + ".ll", "-o", s1r],
                                    capture_output=True, text=True)
                if rr.returncode != 0:
                    failures.append("repro: llc failed on the second "
                                    "assembly: %s"
                                    % rr.stderr.strip()[:160])
                    rok = False
                else:
                    with open(s1 + ".o", "rb") as fh:
                        o1 = fh.read()
                    with open(s1r, "rb") as fh:
                        o2 = fh.read()
                    if o1 != o2:
                        failures.append(
                            "repro: llc is not deterministic on this "
                            "input -- the same .ll assembled twice gave "
                            "different objects (%s). The pinned "
                            "toolchain is the first thing to check"
                            % first_difference(o1, o2))
                        rok = False
                # THE COMMITTED SNAPSHOT CANNOT SILENTLY ROT (D-203) -- and
                # what "rot" means here needed correcting at 1.4.6.
                #
                # This was written at 1.4.5 to assert that stage1.ll is
                # byte-identical to the CURRENT emission. That check
                # contradicts D-205: the snapshot refreshes at CYCLE CLOSES,
                # so between refreshes it is legitimately older than `src/`,
                # and demanding byte-equality every run would make a refresh
                # mandatory on every commit that changes any IR. D-202 says
                # the same thing from the other side -- the fixpoint compares
                # two emissions from CURRENT-source compilers precisely so a
                # stale builder is tolerated.
                #
                # The anti-rot property that IS checked, on every run and
                # before anything else, is that the snapshot still BUILDS
                # `src/`: that is run()'s first act now, and a snapshot too
                # old to compile the tree fails there, by name. What remains
                # for here is integrity of the PAIR -- a snapshot edited
                # without restamping, or a STAMP describing a different file.
                snap = os.path.join(ROOT, "bootstrap", "seed", "stage1.ll")
                stamp = os.path.join(ROOT, "bootstrap", "seed", "STAMP")
                if os.path.exists(snap) and os.path.exists(stamp):
                    import hashlib
                    with open(snap, "rb") as fh:
                        snap_ir = fh.read()
                    got = hashlib.sha256(snap_ir).hexdigest()
                    txt = open(stamp, encoding="utf-8").read()
                    m = re.search(r"sha256:\s*([0-9a-f]{64})", txt)
                    mb = re.search(r"bytes:\s*(\d+)", txt)
                    if not m:
                        failures.append(
                            "repro: bootstrap/seed/STAMP carries no sha256 -- "
                            "the stamp IS the snapshot's integrity claim "
                            "(D-203)")
                        rok = False
                    elif m.group(1) != got:
                        failures.append(
                            "repro: bootstrap/seed/stage1.ll does not match its "
                            "STAMP (%s on disk, %s stamped) -- it was changed "
                            "without being restamped, and the stamp is what "
                            "anyone auditing this build has to go on"
                            % (got[:16], m.group(1)[:16]))
                        rok = False
                    if mb and int(mb.group(1)) != len(snap_ir):
                        failures.append(
                            "repro: STAMP says %s bytes and stage1.ll is %d"
                            % (mb.group(1), len(snap_ir)))
                        rok = False
                    # THE SITE TABLE IS PATH-FREE (D-078; D-204's H9). D-179's
                    # site table records each source path AS GIVEN, so a
                    # refresh whose builder was handed an absolute
                    # `src/main.npk` embeds the machine's path into every one
                    # of its ~1,500 site constants -- and the fixpoint and the
                    # STAMP both pass, because each compares the emission with
                    # itself. Found at 1.4.7's close by doing exactly that in a
                    # dry run (1,489 of 1,647 rows). Relative paths only; the
                    # pattern is the emitter's own row shape (emit_program.npk).
                    absn = len(re.findall(
                        r'^@npk\.sitep\.\d+ = internal constant \[\d+ x i8\] c"/',
                        snap_ir.decode("utf-8", "replace"), re.M))
                    if absn:
                        failures.append(
                            "repro: bootstrap/seed/stage1.ll embeds %d ABSOLUTE "
                            "source paths in its site table -- it was refreshed "
                            "with an absolute path argument; bootstrap/seed/"
                            "README.md: run from the tree root with "
                            "`src/main.npk` spelled relatively" % absn)
                        rok = False
                if rok:
                    print("  %-11s emission cwd-independent, llc "
                          "deterministic" % ("repro",))

            # --- AN UNCOMPUTED LAYOUT FACT IS NEVER READ (D-227) --------
            #
            # Layout memoises three facts per struct and enum -- `tt_drops`,
            # `tt_haschan`, `tt_hasborrow` -- as 0 "not computed", 1 "no",
            # 2 "yes", and every reader spells the question `== 2i32`. That
            # makes an ABSENT fact and a FALSE fact the same answer, and the
            # absent one then answers the permissive way: owns nothing, holds
            # no channel, holds no borrow. Those decide TYPE-046's move-only
            # rule, D-215's dyn refusal and D-183's `gives`, so the permissive
            # answer is a rule not enforced.
            #
            # Three defects of that exact shape landed in 1.4.7 -- a tail
            # `tt_grow` never zeroed, a read that beat the computation, and a
            # payload-less enum whose bits were never written at all -- and
            # each was found by FLIPPING the reading and watching what moved.
            # This is that experiment, kept.
            #
            # Build a compiler whose four readers treat 0 as the NON-default
            # answer, and require its emission of the compiler to be
            # byte-identical to the real one. Byte-identity is the strong form:
            # "nothing refused" would miss a 0-bit read whose flipped answer
            # happens to trip no rule, while a changed byte means some query's
            # answer moved, which means some query saw a 0.
            #
            # It costs a build and an emission. The alternative was a permanent
            # trap on a 0 read, which needs a new error identity -- whose cost
            # under D-226 lands on every `failsafe` in the tree -- and turns a
            # regression into a broken compile rather than a failed stage.
            if ok:
                # THE COPY KEEPS THE LAYOUT, because `src/main.npk` imports
                # `../lib/nio.npk` -- the one import in `src/` that leaves it.
                # Copying `src/` alone produces a tree whose entry cannot
                # resolve, and this check's own negative control is what found
                # that, by failing to build for a reason that had nothing to do
                # with what it tests.
                strict = os.path.join(tmp, "strict")
                shutil.copytree(os.path.join(ROOT, "src"),
                                os.path.join(strict, "src"))
                shutil.copytree(os.path.join(ROOT, "lib"),
                                os.path.join(strict, "lib"))
                tp = os.path.join(strict, "src", "frontend", "types.npk")
                txt = open(tp, encoding="utf-8").read()
                flipped = 0
                for fn in ("tt_drops", "tt_haschan", "tt_hasborrow",
                           "tt_hasshared"):
                    old = "(raw %s(t, id)) == 2i32" % fn
                    flipped += txt.count(old)
                    txt = txt.replace(old, "(raw %s(t, id)) != 1i32" % fn)
                if flipped != 8:
                    failures.append(
                        "absent-fact: expected 8 memoised-bit readings to "
                        "flip and found %d -- this check has lost its grip on "
                        "the source it is about, which makes it a check that "
                        "passes without testing anything (D-227)" % flipped)
                else:
                    with open(tp, "w", encoding="utf-8") as fh:
                        fh.write(txt)
                    sb = build_tool(tmp, True,
                                    os.path.join(strict, "src", "main.npk"),
                                    "npkc-strict")
                    if not sb or not os.path.exists(str(sb)):
                        failures.append("absent-fact: could not build the "
                                        "flipped compiler: %s" % sb)
                    else:
                        rr = subprocess.run(
                            [sb, os.path.join(ROOT, "src", "main.npk")],
                            capture_output=True, timeout=600)
                        if rr.returncode != 0:
                            failures.append(
                                "absent-fact: AN UNCOMPUTED LAYOUT FACT IS "
                                "BEING READ (D-227). The compiler built with "
                                "\"not computed\" reading as the non-default "
                                "answer REFUSED its own source (rc=%d), so a "
                                "query somewhere saw a 0 bit: %s"
                                % (rr.returncode,
                                   rr.stderr.decode("utf-8", "replace")[:400]))
                        elif rr.stdout != stage1_ir:
                            failures.append(
                                "absent-fact: AN UNCOMPUTED LAYOUT FACT IS "
                                "BEING READ (D-227). The flipped compiler "
                                "emitted different bytes (%s) -- some query's "
                                "ANSWER changed, which it can only do by "
                                "reading a 0"
                                % first_difference(stage1_ir, rr.stdout))
                        else:
                            print("  %-11s an uncomputed layout fact is never "
                                  "read" % ("absent-fact",))


        # --- PARITY WITH `npkg test` (1.4.8, D-206 §5) --------------------
        #
        # SUCCESSION, NOT REPLACEMENT: both runners run over the full tree
        # and their verdicts are diffed unit for unit -- the same suites, the
        # same files, the same pass/fail -- and `npkg build`'s compiler must
        # be the bytes this run built. `npkg` is built by the compiler under
        # test (it is full Nitpick against the compiler's own modules), runs
        # from the manifest root exactly as a user would run it, and writes
        # its verdicts to a file this stage reads back. This stage is what
        # `meta/SWITCH.md` waits on: the harness retires only once parity has
        # held through 1.5, never earlier and never with a gap.
        failures += check_parity(tmp, tools)

    shutil.rmtree(tmp, ignore_errors=True)

    if opts.verdicts:
        with open(opts.verdicts, "w", encoding="utf-8") as fh:
            fh.write(verdicts_text())

    if not tools:
        print("\n  (llc / ld.lld not on PATH -- positive tests were not run)")

    if failures:
        print("\n%d failure(s):" % len(failures))
        for f in failures:
            print("  - %s" % f)
        if filtering:
            print("\n%s" % partial_warning(total, available))
        return 1

    # THE PASSING LINE FOR A FILTERED RUN DOES NOT SAY `ok`. Somebody scrolling
    # back to the bottom of a run reads one line, and "ok N test(s) passed" at the
    # end of a partial run is a false statement about the suite.
    if filtering:
        print("\n%d test(s) matched and passed." % total)
        print(partial_warning(total, available))
        return 0
    print("\nok  %d test(s) passed" % total)
    return 0


def check_parity(tmp, tools):
    """Both runners over the full tree, verdicts diffed run-for-run, and the
    artifact byte-compared (1.4.8 acceptance). Every difference is a named
    failure: a unit one runner has and the other lacks, a unit they judge
    differently, or a compiler whose bytes differ."""
    if not tools or not COMPILER:
        return []
    npkg_src = os.path.join(ROOT, "npkg", "main.npk")
    if not os.path.exists(npkg_src):
        return ["parity: npkg/main.npk is missing -- the build tool is part of the tree (D-206)"]
    # BUILT BY THE COMPILER UNDER TEST, not the snapshot: npkg is not src/ and
    # is bound by nothing but what today's compiler can compile.
    saved = BUILDER
    fails = []
    try:
        globals()["BUILDER"] = COMPILER
        npkg = build_tool(tmp, tools, npkg_src, "npkg")
    finally:
        globals()["BUILDER"] = saved
    if not npkg or not os.path.exists(str(npkg)):
        return ["parity: npkg did not build with the compiler under test: %s" % npkg]
    verdicts = os.path.join(tmp, "npkg.verdicts")
    try:
        r = subprocess.run([npkg, "test", "--verdicts", verdicts],
                           capture_output=True, text=True, timeout=5400, cwd=ROOT)
    except subprocess.TimeoutExpired:
        return ["parity: `npkg test` did not terminate in 90 minutes"]
    if r.returncode == 3:
        return ["parity: `npkg test` TRAPPED (exit 3) -- a defect in npkg:\n%s"
                % (r.stdout + r.stderr).strip()[-2000:]]
    if r.returncode == 2:
        return ["parity: `npkg test` could not run (exit 2):\n%s"
                % (r.stdout + r.stderr).strip()[-2000:]]
    if not os.path.exists(verdicts):
        return ["parity: `npkg test` wrote no verdict file (exit %d):\n%s"
                % (r.returncode, (r.stdout + r.stderr).strip()[-2000:])]
    with open(verdicts, encoding="utf-8") as fh:
        theirs = read_verdicts(fh.read())
    ours = read_verdicts(verdicts_text())
    only_ours = sorted(k for k in ours if k not in theirs)
    only_theirs = sorted(k for k in theirs if k not in ours)
    for k in only_ours[:20]:
        fails.append("parity: the harness ran %s/%s and npkg did not" % k)
    for k in only_theirs[:20]:
        fails.append("parity: npkg ran %s/%s and the harness did not" % k)
    if len(only_ours) > 20 or len(only_theirs) > 20:
        fails.append("parity: %d unit(s) run by one runner only (first 20 of each listed)"
                     % (len(only_ours) + len(only_theirs)))
    differ = 0
    for k in sorted(ours):
        if k in theirs and ours[k][0] != theirs[k][0]:
            differ += 1
            if differ <= 20:
                fails.append("parity: %s/%s -- harness %s, npkg %s%s%s"
                             % (k[0], k[1],
                                "PASS" if ours[k][0] else "FAIL",
                                "PASS" if theirs[k][0] else "FAIL",
                                (": " + ours[k][1][:200]) if ours[k][1] else "",
                                (" / " + theirs[k][1][:200]) if theirs[k][1] else ""))
    if differ > 20:
        fails.append("parity: %d verdict(s) differ (first 20 listed)" % differ)
    # THE ARTIFACT: `npkg build`'s compiler is this run's compiler, byte for byte.
    built = os.path.join(ROOT, "build", "npkc")
    if not os.path.exists(built):
        fails.append("parity: `npkg test` left no build/npkc behind")
    else:
        with open(built, "rb") as fa, open(COMPILER, "rb") as fb:
            if fa.read() != fb.read():
                fails.append("parity: build/npkc (npkg's) differs from the harness's npkc -- same inputs, same flags, different bytes (D-204/D-206)")
    if not fails:
        agreed = sum(1 for k in ours if k in theirs)
        print("  %-11s %d verdict(s) agree between the two runners; npkc byte-identical"
              % ("parity", agreed))
    return fails


def partial_warning(ran, available):
    """Says what did NOT run, and says nothing about whether what did run passed
    -- the failure list above already answers that, and a warning that claims
    "ran and passed" while sitting under a list of failures is worse than no
    warning at all."""
    return ("PARTIAL RUN -- %d of %d test(s) ran. THE SUITE IS NOT KNOWN GREEN.\n"
            "  Not run: %d other test(s), the node-kind reachability check, the\n"
            "  real-parser sweep over every source, and the module-rejection\n"
            "  suite. Run with no arguments before committing."
            % (ran, available, available - ran))


if __name__ == "__main__":
    sys.exit(main(sys.argv))
