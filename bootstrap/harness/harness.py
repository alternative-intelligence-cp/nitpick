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
sys.path.insert(0, os.path.join(ROOT, "bootstrap", "generator"))

import diag        # noqa: E402
import lex         # noqa: E402
import parse       # noqa: E402
import check       # noqa: E402
import emit        # noqa: E402

RUNTIME_LL = os.path.join(ROOT, "bootstrap", "runtime", "npkrt.ll")

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
            elif body.startswith("expect-no-parse-error"):
                e.no_parse_error = True
    return e


# --- compilation -------------------------------------------------------------

class Outcome:
    def __init__(self):
        self.diags = []
        self.ir = None


def compile_files(paths):
    """Run the seed over a file group, collecting diagnostics rather than
    raising. The seed stops at the first error -- it has no recovery, which is
    a deliberate scope limit (0.0.2), so at most one diagnostic appears."""
    out = Outcome()
    mods = []
    for p in paths:
        try:
            with open(p, "r", encoding="utf-8") as fh:
                mods.append(parse.parse_source(fh.read(), os.path.relpath(p, ROOT)))
        except diag.NpkError as e:
            out.diags.append(e.diag)
            return out
    try:
        ck = check.Checker()
        prog = ck.check(mods)
        out.ir = emit.emit_module(prog, ck, module_id="test")
    except diag.NpkError as e:
        out.diags.append(e.diag)
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

def check_positive(name, group, exp, tmp, tools):
    out = compile_files(group)
    if out.diags:
        return ["%s: expected to compile, got %s" % (name, out.diags[0])]
    if not tools:
        return []
    base = os.path.join(tmp, name.replace("/", "_"))
    with open(base + ".ll", "w", encoding="utf-8") as fh:
        fh.write(out.ir)
    r = subprocess.run(["llc", "-O0", "-filetype=obj", "-relocation-model=static",
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
        return ["%s: llc rejected the emitted IR: %s" % (name, first.strip()[:140])]
    fails = check_zero_dependency(base + ".o", runtime_allowlist(), name)
    if fails:
        return fails
    r = subprocess.run(["ld.lld", "-static", "-o", base, base + ".o",
                        os.path.join(tmp, "npkrt.o")], capture_output=True, text=True)
    if r.returncode != 0:
        return ["%s: link failed: %s" % (name, r.stderr.strip()[:140])]
    try:
        got = subprocess.run([base], capture_output=True, timeout=10).returncode
    except subprocess.TimeoutExpired:
        return ["%s: timed out" % name]
    if got != exp.exit:
        return ["%s: exited %d, expected %d" % (name, got, exp.exit)]
    return []


def check_negative(name, group, exp, tmp, tools):
    out = compile_files(group)
    fails = []

    if not exp.errors:
        return ["%s: negative test declares no `// expect-error:` -- a test that "
                "only asserts 'it failed somehow' stops noticing when it starts "
                "failing for a different reason" % name]

    if not out.diags:
        return ["%s: expected %s, but it compiled cleanly"
                % (name, exp.errors[0][0])]

    if exp.no_parse_error:
        bad = [d for d in out.diags if d.phase in ("lex", "parse")]
        if bad:
            fails.append("%s: PARSE error %s -- the parser must never restrict; "
                         "capability rejection belongs in the backend (D-085)"
                         % (name, bad[0]))

    got_codes = sorted(d.code for d in out.diags)
    want_codes = sorted(c for c, _, _ in exp.errors)
    if got_codes != want_codes:
        fails.append("%s: expected %s, got %s" % (name, want_codes, got_codes))
        return fails

    for code, line, col in exp.errors:
        if line is None:
            continue
        hit = [d for d in out.diags if d.code == code]
        for d in hit:
            if d.line != line or (col is not None and d.col != col):
                fails.append("%s: %s at %d:%d, expected %d:%s"
                             % (name, code, d.line, d.col, line, col or "*"))
    return fails


def check_diagnostic(name, group, exp, tmp, tools):
    """Compiles, but must emit exactly the expected warnings.

    The seed emits no warnings, so nothing exercises this kind yet. It is
    defined rather than deferred so the harness shape is fixed before cycle 0.1
    starts producing warnings.
    """
    out = compile_files(group)
    if out.diags:
        return ["%s: expected to compile, got %s" % (name, out.diags[0])]
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
    "NITPICK-MACRO-006":   "internal -- a node kind the macro clone has no case for",
    "NITPICK-RESOLVE-009": "internal -- a node kind the resolver has no case for",
    "NITPICK-TYPE-011":    "internal -- a node kind the type checker has no case for",
    "NITPICK-EMIT-002":    "internal -- a node kind the emitter has no case for",

    # AHEAD OF THE LANGUAGE -- the rule is written, correct, and unreachable
    # because the construct it governs does not exist yet. Deliberate, and the
    # reason is the one-shot Astree run: a rule added AFTER the analysis is
    # verified is a re-verification, and this project gets one attempt. Cycle 1.1
    # lowers concurrency and closures, and these become testable then.
    "NITPICK-BORROW-004":  "ahead of the language -- there is no spawn yet (cycle 1.1)",
    "NITPICK-BORROW-005":  "ahead of the language -- there is no reachable `await` yet (cycle 1.1)",
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
                            "emit_field_call"},       # `s.f(x)` through a fn-valued field (1.0.9c)
    "type_method_call":    {"emit_method_call"},      # the receiver, parameter 0 (1.0.8)
    "type_pipe":           {"emit_pipe"},             # `x |> g` fits x to g's parameter (1.0.9c)
    "check_ctor_args":     {"emit_ctor"},
    "type_struct_literal": {"emit_struct_lit"},
    "type_array_literal":  {"emit_array_lit"},
    "type_result_literal": {"emit_result_lit"},
    "arm_give_type":       {"emit_give"},
    "type_safe_unwrap":    {"emit_expr_kind"},      # `r ? d` lowers inline
    "type_arena_method":   {"emit_arena_method"},
    "type_sarena_method":  {"emit_sarena_method"},
    # The Handle argument is checked by equality; `fits` is used there so the
    # diagnostic is the same one every slot gives. Nothing is built for it,
    # and the arena emitter is where it would be.
    "want_handle_of":      {"emit_arena_method"},
    "type_null_coalesce":  {"emit_coalesce"},          # lowered at 1.0.7
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


def check_ll_types_agree():
    """The two emitters must lower a type to the SAME LLVM text.

    `bootstrap/generator/ntypes.py` answers this for the compiler that builds
    stage 1, and `src/backend/ir/ir_types.npk` answers it for stage 1 itself. Their
    IR meets at every runtime symbol and every aggregate that crosses between them,
    and `llc` rejects a caller whose struct disagrees with the callee's by one
    field -- which is how the runtime's header records the mismatch being caught
    the first time.

    So the two are DIFFED rather than trusted. `tests/backend/ir_types.npk` pins
    the real compiler's answer as a string on each `ll_is` line, with a `// ll:`
    marker naming the shape; this reads the pair off that ONE line and asks the
    seed the same question. Both halves on one line is deliberate: a marker on the
    line above could stop describing the assertion beneath it and nothing would
    say so.
    """
    sys.path.insert(0, os.path.join(ROOT, "bootstrap", "generator"))
    import ntypes as T

    # The seed decides per compilation which enums carry a payload, because
    # `llvm()` has no other way to know. The fixture's two are stated here.
    T.reset_enums()
    T.ENUM_HAS_PAYLOAD["Flat"] = False
    T.ENUM_HAS_PAYLOAD["Tagged"] = True

    shapes = {
        "int8": T.Prim("int8"), "int16": T.Prim("int16"),
        "int32": T.Prim("int32"), "int64": T.Prim("int64"),
        "uint8": T.Prim("uint8"), "uint32": T.Prim("uint32"),
        "bool": T.BOOL, "char8": T.CHAR8, "tbb32": T.TBB32,
        "string": T.STRING, "cstring": T.CSTRING, "NIL": T.NIL,
        "int32->": T.Ptr(T.I32), "int32[]": T.Slice(T.I32),
        "int32[4]": T.Array(T.I32, 4),
        "Result<int32>": T.ResultT(T.I32),
        "Result<NIL>": T.ResultT(T.NIL),
        "Result<string>": T.ResultT(T.STRING),
        "Pair": T.Named("Pair"), "Flat": T.Named("Flat"),
        "Tagged": T.Named("Tagged"),
    }

    path = os.path.join(ROOT, "tests", "backend", "ir_types.npk")
    fails, seen = [], set()
    with open(path, encoding="utf-8") as fh:
        for i, line in enumerate(fh, 1):
            m = re.search(r"//\s*ll:\s*(\S+)\s*$", line)
            if not m:
                continue
            shape = m.group(1)
            texts = re.findall(r'"((?:[^"\\]|\\.)*)"', line)
            if len(texts) != 1:
                fails.append("tests/backend/ir_types.npk:%d carries a `// ll:` "
                             "marker and %d string literals -- the marker names "
                             "the shape whose text is pinned ON THAT LINE, so "
                             "exactly one is what makes the pair readable"
                             % (i, len(texts)))
                continue
            if shape not in shapes:
                fails.append("tests/backend/ir_types.npk:%d pins the shape `%s`, "
                             "which `check_ll_types_agree` cannot build for the "
                             "seed -- add it to `shapes` so the two answers are "
                             "actually compared" % (i, shape))
                continue
            seen.add(shape)
            want = T.llvm(shapes[shape])
            if texts[0] != want:
                fails.append("`%s` lowers to `%s` in src/backend/ir/ir_types.npk "
                             "and to `%s` in bootstrap/generator/ntypes.py -- the "
                             "seed builds stage 1 and stage 1 builds stage 2, so "
                             "their IR has to agree or the link does not"
                             % (shape, texts[0], want))
    for shape in sorted(set(shapes) - seen):
        fails.append("`%s` is in `check_ll_types_agree`'s table and no `// ll:` "
                     "line pins it -- an entry nothing compares is an entry that "
                     "stopped being a check" % shape)
    return fails


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


def check_runtime_sigs_agree():
    """npkrt.ll, the seed's RUNTIME table, and src/backend/ir/ir_runtime.npk must
    state the SAME signature for every runtime symbol.

    Three copies of one fact, and none removable: the runtime DEFINES, the seed
    declares for stage 1, the compiler declares for stage 2. npkrt.ll's own
    header records what a disagreement does -- IR llc rejects -- and a size
    disagreement is worse, because a call through a wrong aggregate type
    corrupts the callee's view of memory instead of failing.
    """
    fails = []
    defs = _npkrt_defines()

    # --- the seed against the runtime ------------------------------------
    for name, (sym, ret, args) in sorted(emit.RUNTIME.items()):
        d = defs.get("npk_" + name)
        if d is None:
            fails.append("seed RUNTIME declares `%s` and npkrt.ll does not define "
                         "@npk_%s" % (name, name))
            continue
        if d[0] != ret:
            fails.append("`%s` returns `%s` in npkrt.ll and `%s` in the seed's "
                         "RUNTIME table" % (name, d[0], ret))
        if d[1] != args:
            fails.append("`%s` takes %s in npkrt.ll and %s in the seed's RUNTIME "
                         "table" % (name, d[1], args))

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
        entries[name] = (strs, int(argc.group(1)))
    if not entries:
        return ["check_runtime_sigs_agree parsed no entries out of "
                "src/backend/ir/ir_runtime.npk -- the check has stopped checking"]
    for name, (strs, argc) in sorted(entries.items()):
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
        wrapped = "true" in body.split(",")[2]
        if wrapped and (ret.startswith("{ {") or (ret.startswith("{") and ret.endswith("i32 }"))):
            want_inner = ret[1:].rsplit(",", 1)[0].strip() if ret != "{ i32 }" else ""
            if inner != want_inner:
                fails.append("`%s`'s wrapped inner is `%s` in ir_runtime.npk and "
                             "`%s` derived from its return" % (name, inner, want_inner))

    # --- every seed entry has a compiler entry, and back -------------------
    seed_names = set(emit.RUNTIME) | {"exit"}
    comp_names = set(entries)
    for name in sorted(seed_names - comp_names):
        fails.append("`%s` is in the seed's RUNTIME floor and not in "
                     "ir_runtime.npk -- stage 2 could not compile a program the "
                     "seed compiles" % name)
    for name in sorted(comp_names - seed_names):
        # The compiler's floor may EXCEED the seed's (0.8.4): the seed only ever
        # compiles sources that call the shared subset, while npkc serves whole
        # programs. A compiler-only entry is fine exactly when the runtime
        # defines its symbol -- anything else is a call into nothing.
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
    "ExprVectorCtorExpr": "rung", "ExprAwaitExpr": "rung",
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
    "DeclImplDecl": "rung", "DeclExternBlock": "rung", "DeclGlobalDecl": "rung",
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
    "DeclExternFn": "inert: child of DeclExternBlock, which refuses wholesale",
    "DeclVariadicSpec": "inert: part of a signature, not a construct",
    "DeclFailsOn": "inert: an FFI contract; consumed with extern (FFI rung)",
    "DeclNeverFails": "inert: an FFI contract; consumed with extern (FFI rung)",
    "DeclAttribute": "inert: read by the passes it decorates, never emitted",
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


def check_raw_licensed(tc):
    """D-163's instrument -- REPORTS, never fails (1.1.0).

    For each tree, how far it stands from the licence: the count of `raw` and
    `drop` sites whose resolved callee does not declare `never fails`, split
    may-fail / builtin / other. RESOLUTION, not regex -- `check --raw-report`
    runs the real frontend, so UFCS and imports are answered correctly. The
    refusal is 1.1.2's closing act, once the sweeps drive `mayfail` to zero;
    until then this prints the debt on every full run, the way
    `check_decisions_current` prints doc-sync candidates.
    """
    per_file = {}
    skipped = []

    def run(root):
        try:
            r = subprocess.run([tc, root, "--raw-report"], capture_output=True,
                               text=True, timeout=180)
        except Exception:
            skipped.append(root)
            return
        if r.returncode != 0:
            skipped.append(root)
            return
        for m in re.finditer(r"^(\S+)  mayfail=(\d+) builtin=(\d+) other=(\d+)$",
                             r.stdout, re.M):
            per_file[os.path.relpath(m.group(1), ROOT)
                     if os.path.isabs(m.group(1)) else m.group(1)] = (
                int(m.group(2)), int(m.group(3)), int(m.group(4)))

    # ONE run over src/main.npk (~45s -- it fronts the whole compiler, and
    # lib/ rides in with it), plus the fast test trees (each loads only the
    # prelude and itself). The tools roots are deliberately absent: each one
    # imports all of src/ (another ~45s for three files' own sites); their
    # counts are in the committed worklist (raw_sweep_worklist.md), measured
    # once at 1.1.0.
    roots = [os.path.join(ROOT, "src", "main.npk")]
    for sub in (("tests", "accept"), ("tests", "conformance"),
                ("tests", "backend", "programs"), ("tests", "frontend")):
        roots += sorted(glob.glob(os.path.join(ROOT, *sub, "*.npk")))
    for root in roots:
        run(root)

    trees = {}
    for path, (may, bi, ot) in per_file.items():
        tree = path.split(os.sep)[0]
        t = trees.setdefault(tree, [0, 0, 0])
        t[0] += may; t[1] += bi; t[2] += ot
    lines = []
    for tree in sorted(trees):
        may, bi, ot = trees[tree]
        lines.append("%s: mayfail=%d builtin=%d other=%d"
                     % (tree, may, bi, ot))
    worst = sorted(per_file.items(), key=lambda kv: -kv[1][0])[:5]
    return lines, worst, len(skipped)


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
    """Compile a tool under tools/ and return its path, or an error string."""
    if not tools:
        return None
    out = compile_files(group_for(source))
    if out.diags:
        return "DIAG %s" % out.diags[0]
    base = os.path.join(tmp, name)
    with open(base + ".ll", "w", encoding="utf-8") as fh:
        fh.write(out.ir)
    r = subprocess.run(["llc", "-O0", "-filetype=obj", "-relocation-model=static",
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return "LLC %s" % r.stderr.strip()[:160]
    r = subprocess.run(["ld.lld", "-static", "-o", base, base + ".o",
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


def build_parse_check(tmp, tools):
    """Compile tools/parse_check.npk.

    Everything above tests the SEED's parser. This builds the REAL one, so the
    rule D-085 states -- every file in tests/rejection/ must PARSE and be refused
    later -- can be checked against the parser it was written about instead of
    the throwaway one that happens to be running.
    """
    return build_tool(tmp, tools, PARSE_CHECK, "parse_check")


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


def check_emitted_program(binary, path, name, exp, tmp):
    """A program COMPILED BY THIS COMPILER'S BACKEND, run, and its exit compared.

    The seed is nowhere in this path: `emit_check` loads, checks and EMITS with
    the real compiler, and what runs is what `emit_program` wrote. This is the
    strongest instrument the backend has -- a byte-pin proves the text is stable,
    but only execution proves the text means what the source said.
    """
    try:
        r = subprocess.run([binary, path], capture_output=True, timeout=30)
    except subprocess.TimeoutExpired:
        return ["%s: emit_check did not terminate" % name]
    if r.returncode == 3:
        return ["%s: the compiler TRAPPED -- a defect in it, not in this file"
                % name]
    if r.returncode != 0:
        got = r.stderr.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: expected IR, got a refusal (%s)" % (name, got)]
    base = os.path.join(tmp, "prog_" + os.path.basename(path).replace(".", "_"))
    with open(base + ".ll", "wb") as fh:
        fh.write(r.stdout)
    ir_text = r.stdout.decode("utf-8", "replace")
    dupes = check_symbols_unique(ir_text, name)
    if dupes:
        return dupes
    hoist = check_allocas_hoisted(ir_text, name)
    if hoist:
        return hoist
    r = subprocess.run(["llc", "-O0", "-filetype=obj", "-relocation-model=static",
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
        return ["%s: llc rejected the REAL BACKEND's IR: %s"
                % (name, first.strip()[:160])]
    fails = check_zero_dependency(base + ".o", runtime_allowlist(), name)
    if fails:
        return fails
    r = subprocess.run(["ld.lld", "-static", "-o", base, base + ".o",
                        os.path.join(tmp, "npkrt.o")], capture_output=True, text=True)
    if r.returncode != 0:
        return ["%s: link failed: %s" % (name, r.stderr.strip()[:140])]
    try:
        got = subprocess.run([base], capture_output=True, timeout=10).returncode
    except subprocess.TimeoutExpired:
        return ["%s: timed out" % name]
    if got != exp.exit:
        return ["%s: exited %d, expected %d (compiled by the REAL backend)"
                % (name, got, exp.exit)]
    return []


# --- driver ------------------------------------------------------------------

def load_targets():
    with open(os.path.join(ROOT, "nitpick.toml"), "rb") as fh:
        manifest = tomllib.load(fh)
    return manifest.get("test", [])


USAGE = """usage: python3 bootstrap/harness/harness.py [--only SUBSTR]...

  (no arguments)    run everything -- the only run whose result means the suite
                    is green, and the only kind to commit on
  --only SUBSTR     run only tests whose repo-relative path contains SUBSTR.
                    Repeatable. Skips every whole-suite check, and says so.
  -h, --help        this"""


class Options:
    def __init__(self):
        self.only = []
        self.help = False
        self.error = None


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

    targets = load_targets()
    if not targets:
        print("no [[test]] targets in nitpick.toml")
        return 2

    if filtering:
        print("PARTIAL RUN -- matching %s" % ", ".join(repr(s) for s in opts.only))

    tools = shutil.which("llc") and shutil.which("ld.lld")
    tmp = tempfile.mkdtemp(prefix="npk-harness-")
    if tools:
        r = subprocess.run(["llc", "-O0", "-filetype=obj", "-relocation-model=static",
                            RUNTIME_LL, "-o", os.path.join(tmp, "npkrt.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("runtime floor did not compile: %s" % r.stderr.strip()[:200])
            return 1

    total = 0
    available = 0
    failures = []
    all_sources = []
    for t in targets:
        kind = t["kind"]
        paths = sorted(glob.glob(os.path.join(ROOT, t["path"], "*.npk")))
        skip = imported_by_others(paths)
        run_paths = [p for p in paths if os.path.abspath(p) not in skip]
        available += len(run_paths)
        if filtering:
            run_paths = [p for p in run_paths
                         if any(s in os.path.relpath(p, ROOT) for s in opts.only)]
            if not run_paths:
                continue
        for p in run_paths:
            total += 1
            name = os.path.relpath(p, ROOT)
            exp = read_expectations(p)
            failures += KINDS[kind](name, group_for(p, paths), exp, tmp, tools)
        all_sources += paths
        print("  %-11s %2d %s test(s)" % (t["name"], len(run_paths), kind))

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
    # They are also where most of the minutes go: each builds a whole tool through
    # the seed, which is the cost `--only` exists to avoid.
    if not filtering:
        # Every source in every suite, plus tests/grammar/, through the REAL
        # parser. A rejection test the real parser cannot read is not testing
        # D-085's rule; it is testing that the seed and the real parser happen to
        # disagree.
        #
        # tests/grammar/ is NEVER compiled and never run. It exists only to be
        # parsed, which is what lets it use the whole language rather than
        # subset 1.
        failures += check_kinds_reachable()
        failures += check_kinds_typed()
        failures += check_kinds_lowered_or_refused()
        failures += check_codes_tested()
        failures += check_codes_centralised()
        failures += check_identity_by_decl()
        failures += check_slot_sites_agree()
        failures += check_one_renderer()
        failures += check_rung_names_open_cycle()
        failures += check_ll_types_agree()
        failures += check_runtime_sigs_agree()

        # REPORTED, never failing (0.9.1): stale-decision candidates need a
        # human eye -- but silence would be the lint not existing, so the count
        # prints on every full run and the list prints when non-empty.
        stale = check_decisions_current()
        if stale:
            print("  decisions-current: %d candidate(s) for the doc-sync pass:"
                  % len(stale))
            for c in stale:
                print("    ~ %s" % c)

        pc = build_parse_check(tmp, tools)
        if isinstance(pc, str) and not os.path.exists(pc):
            failures.append("tools/parse_check.npk did not build: %s" % pc)
        elif pc:
            # tests/grammar/ is parse-only by design; tests/modules/ holds
            # fixtures that a test loads through the real loader. Neither is
            # compiled, and both must still PARSE -- a broken fixture would
            # otherwise surface as a confusing failure inside whichever test
            # loads it.
            grammar = sorted(glob.glob(os.path.join(ROOT, "tests", "grammar", "*.npk")))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "modules", "**", "*.npk"),
                                        recursive=True))
            # tests/types/ likewise: every file there must PARSE, because a type
            # rejection test that tripped the parser would be testing the wrong
            # stage and passing for the wrong reason.
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "types", "**", "*.npk"),
                                        recursive=True))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "analysis", "**", "*.npk"),
                                        recursive=True))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "expansion", "**", "*.npk"),
                                        recursive=True))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "derive", "**", "*.npk"),
                                        recursive=True))
            # THE PRELUDE IS REAL SOURCE AND IS CHECKED AS SUCH. It is put through
            # the real parser at the start of every `check` run anyway; sweeping it
            # here says so, and catches a syntax error in it as a parse failure of
            # its own file rather than as a diagnostic in every test at once.
            grammar += sorted(glob.glob(os.path.join(ROOT, "src", "prelude", "*.npk")))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "accept", "**", "*.npk"),
                                        recursive=True))
            grammar += sorted(glob.glob(os.path.join(ROOT, "tests", "backend", "**", "*.npk"),
                                        recursive=True))

            # THE COMPILER'S OWN SOURCE, which is the file set that matters most
            # and the one this sweep did not cover until 0.7.3.
            #
            # `src/prelude/` was here from the start and the other fifty-eight
            # modules were not, so the parser was asked about every test in the
            # tree and never about the thing it is part of. WHEN IT WAS FINALLY
            # ASKED, 22 OF 62 FILES WERE REJECTED AND FIVE CRASHED IT -- a
            # qualifier the parser never learned to read on a field or a
            # parameter, `dn`/`bn`/`cn`/`tt` used as names when the grammar makes
            # them NUMERIC LITERALS, and an out-of-bounds read in `ralloc` that
            # took the process down on any file past about 511 declarations.
            #
            # This is not a nicety. STAGE 1 IS BUILT BY THE SEED AND MUST THEN
            # PARSE THESE FILES to build stage 2, so a source the real parser
            # cannot read is a source that never self-hosts -- and every one of
            # the three causes was invisible for as long as nobody asked.
            compiler = sorted(glob.glob(os.path.join(ROOT, "src", "**", "*.npk"),
                                        recursive=True))
            compiler += sorted(glob.glob(os.path.join(ROOT, "tools", "*.npk")))
            compiler += sorted(glob.glob(os.path.join(ROOT, "lib", "**", "*.npk"),
                                         recursive=True))

            n = 0
            for p in sorted(set(all_sources)) + grammar + sorted(set(compiler)):
                name = os.path.relpath(p, ROOT)
                failures += check_parses(pc, p, name)
                n += 1
            print("  %-11s %2d real-parser check(s)" % ("grammar", n))

        # Whole programs that must be refused BY THE LOADER. A file here with no
        # `expect-error:` is a fixture another one imports, not a test.
        rc = build_tool(tmp, tools, RESOLVE_CHECK, "resolve_check")
        if isinstance(rc, str) and not os.path.exists(rc):
            failures.append("tools/resolve_check.npk did not build: %s" % rc)
        elif rc:
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "modules",
                                                   "rejection", "**", "*.npk"),
                                      recursive=True)):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_module_rejection(rc, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d module-rejection test(s)" % ("modules", n))

        # Whole programs that LOAD and RESOLVE and must be refused BY THE TYPE
        # CHECKER. A file here with no `expect-error:` is a fixture, not a test.
        tc = build_tool(tmp, tools, TYPE_CHECK, "check")
        if isinstance(tc, str) and not os.path.exists(tc):
            failures.append("tools/check.npk did not build: %s" % tc)
        elif tc:
            # D-163's instrument -- REPORTED, never failing (1.1.0): the
            # distance of each tree from the raw/drop licence, printed on
            # every full run until 1.1.1/1.1.2 drive it to zero and flip
            # the refusal on.
            rl_lines, rl_worst, rl_skipped = check_raw_licensed(tc)
            print("  raw-licence: unlicensed `raw`/`drop` sites by tree "
                  "(D-163; %d root(s) skipped un-runnable):" % rl_skipped)
            for ln in rl_lines:
                print("    ~ %s" % ln)
            for path, (may, bi, ot) in rl_worst:
                print("      worst: %-46s mayfail=%d" % (path, may))
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "types",
                                                   "rejection", "**", "*.npk"),
                                      recursive=True)):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_type_rejection(tc, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d type-rejection test(s)" % ("types", n))

            # Whole programs that TYPE-CHECK and are refused by a STATIC ANALYSIS.
            #
            # A FOURTH SUITE FOR A FOURTH STAGE, and the split is the same argument
            # the other three rest on: a file that stops earlier would satisfy a
            # test written about a later stage. The analyses run only over a program
            # the type checker accepted, so a case here that failed to type would
            # never reach the rule it was written for.
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "analysis",
                                                   "rejection", "**", "*.npk"),
                                      recursive=True)):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_type_rejection(tc, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d analysis-rejection test(s)" % ("analysis", n))

            # Whole programs refused during MACRO EXPANSION, before a name is
            # bound. A fifth stage and a fifth suite, for the reason the other four
            # are separate: expansion runs before resolution, so a case here that
            # failed to parse would never reach the rule it was written for.
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "expansion",
                                                   "rejection", "**", "*.npk"),
                                      recursive=True)):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_type_rejection(tc, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d expansion-rejection test(s)" % ("expansion", n))

            # Whole programs refused while reading `#[derive]`, which runs in the
            # expansion phase but is a different stage for the reason its codes have
            # a different prefix: a reader filtering for derive failures is asking a
            # different question from one filtering for expansion failures.
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "derive",
                                                   "rejection", "**", "*.npk"),
                                      recursive=True)):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_type_rejection(tc, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d derive-rejection test(s)" % ("derive", n))

        # --- the REAL BACKEND (0.7.7) ------------------------------------------
        #
        # `tests/rejection/` asserts against THIS COMPILER's rungs now, which is
        # what D-085 promised the day the suite was written: its files must pass
        # the whole frontend and be refused by EMISSION with NITPICK-RUNG-001.
        # Until this tool existed the suite could only test the seed's rungs.
        ec = build_tool(tmp, tools, EMIT_CHECK, "npkc")
        if isinstance(ec, str) and not os.path.exists(ec):
            failures.append("src/main.npk (npkc) did not build: %s" % ec)
        elif ec:
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "rejection",
                                                   "*.npk"))):
                exp = read_expectations(p)
                if not exp.errors:
                    continue
                failures += check_backend_rejection(ec, p, os.path.relpath(p, ROOT), exp)
                n += 1
            print("  %-11s %2d backend-rejection test(s)" % ("rungs", n))

            # Whole programs COMPILED BY THE REAL BACKEND, linked against the
            # runtime, and RUN. A byte-pin proves the text is stable; only
            # execution proves the text means what the source said.
            #
            # THE CONFORMANCE SUITE IS IN THIS SWEEP, which is the cycle's goal
            # sentence made a test: subset 1 compiles and runs under THIS
            # compiler, with the same expectations the seed meets. A file another
            # one imports is a fixture here exactly as it is for the seed.
            progs = sorted(glob.glob(os.path.join(ROOT, "tests", "backend",
                                                  "programs", "*.npk")))
            # A program's imported halves are fixtures, not standalone programs
            # (they have no `main`) -- filter them out, the same rule the
            # conformance sweep already applies. same_name_a/b (D-162, 1.0.1)
            # are the first multi-file programs and the first to need it.
            progs = [p for p in progs if p not in imported_by_others(progs)]
            conf = sorted(glob.glob(os.path.join(ROOT, "tests", "conformance",
                                                 "*.npk")))
            conf = [p for p in conf if p not in imported_by_others(conf)]
            n = 0
            for p in progs + conf:
                exp = read_expectations(p)
                failures += check_emitted_program(ec, p, os.path.relpath(p, ROOT),
                                                  exp, tmp)
                n += 1
            print("  %-11s %2d real-backend program(s)" % ("programs", n))

            # D-163 IS INERT IN THE BACKEND (1.1.0): the twin programs under
            # conformance/nf_twin/ differ only in their `never fails` clauses
            # and must emit byte-identical IR -- the contract is a checked
            # claim, never a codegen hint.
            twa = os.path.join(ROOT, "tests", "conformance", "nf_twin",
                               "with", "nf_inert.npk")
            twb = os.path.join(ROOT, "tests", "conformance", "nf_twin",
                               "without", "nf_inert.npk")
            ra = subprocess.run([ec, twa], capture_output=True, text=True)
            rb = subprocess.run([ec, twb], capture_output=True, text=True)
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

            # --- RUNTIME-FLOOR UNIT TESTS (0.10.3) --------------------------
            #
            # Hand-written .ll drivers for runtime families with NO surface
            # syntax (the frame allocator is 1.1's coroutine machinery's, not a
            # program's). Each defines main + npk_failsafe, links against the
            # same npkrt.o everything else does, runs, and asserts an exit
            # code -- the same execution standard the program suite holds.
            rtests = sorted(glob.glob(os.path.join(ROOT, "bootstrap", "runtime",
                                                   "tests", "*.ll")))
            rn = 0
            for rp in rtests:
                rexp = 0
                with open(rp, encoding="utf-8") as fh:
                    rhead = fh.read(400)
                rm = re.search(r"expect-exit:\s*(\d+)", rhead)
                if rm:
                    rexp = int(rm.group(1))
                rbase = os.path.join(tmp, "rt_" + os.path.basename(rp)[:-3])
                r = subprocess.run(["llc", "-O0", "-filetype=obj",
                                    "-relocation-model=static", rp,
                                    "-o", rbase + ".o"],
                                   capture_output=True, text=True)
                if r.returncode != 0:
                    failures.append("%s: llc failed: %s"
                                    % (os.path.relpath(rp, ROOT),
                                       r.stderr.strip()[:120]))
                    continue
                r = subprocess.run(["ld.lld", "-static", "-o", rbase,
                                    rbase + ".o", os.path.join(tmp, "npkrt.o")],
                                   capture_output=True, text=True)
                if r.returncode != 0:
                    failures.append("%s: link failed: %s"
                                    % (os.path.relpath(rp, ROOT),
                                       r.stderr.strip()[:120]))
                    continue
                r = subprocess.run([rbase], capture_output=True)
                if r.returncode != rexp:
                    failures.append("%s: exited %d, expected %d"
                                    % (os.path.relpath(rp, ROOT),
                                       r.returncode, rexp))
                rn += 1
            if rn:
                print("  %-11s %2d runtime-floor test(s)" % ("runtime", rn))

            # --- THE SELF-CHECK AND THE FIXPOINT (0.8.1) --------------------
            #
            # npkc compiles ITSELF: the whole 60-module compiler through its own
            # frontend, its own analyses, and its own emitter. The emitted IR is
            # assembled and linked into STAGE 1 -- a compiler with no seed in it
            # -- and stage 1 emits the compiler again. The two emissions must be
            # BYTE-IDENTICAL (D-078/D-085): this is the 1.2 fixpoint, run as a
            # standing instrument from the day it first closed. A drift here is
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
                # THE COMPILER'S OWN OUTPUT IS THE BIGGEST MODULE IT EMITS, so
                # it is the one where a duplicate symbol is hardest to read off
                # an llc error. Checked here first, in the compiler's terms.
                dupes = check_symbols_unique(
                    stage1_ir.decode("utf-8", "replace"), "stage1.ll")
                if dupes:
                    failures += dupes
                    ok = False
                rr = subprocess.run(["llc", "-O0", "-filetype=obj",
                                     "-relocation-model=static",
                                     s1 + ".ll", "-o", s1 + ".o"],
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
                    rr = subprocess.run(["ld.lld", "-static", "-o", s1,
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
                        failures.append("STAGE 1 cannot compile the compiler: %s"
                                        % rr.stderr.decode("utf-8", "replace")[:300])
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

            # And whole programs that must be ACCEPTED, in full silence.
            #
            # A REJECTION SUITE CANNOT TELL A CORRECT CHECKER FROM ONE THAT
            # REFUSES EVERYTHING. Every negative test above passes trivially for an
            # analysis that answers "violation" to every question, and cycle 0.5's
            # analyses are deliberately conservative -- they fail closed on fuel
            # exhaustion, on an unclassified node, on anything undecidable -- so
            # over-refusal is the failure mode they are most likely to have.
            #
            # ONE SUITE, NOT ONE PER STAGE. Silence has no stage: a program the
            # whole frontend accepts is accepted by every part of it, so splitting
            # this the way the rejections are split would be four directories
            # asserting the same thing.
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "accept",
                                                   "**", "*.npk"),
                                      recursive=True)):
                failures += check_type_accept(tc, p, os.path.relpath(p, ROOT))
                n += 1
            print("  %-11s %2d acceptance test(s)" % ("accept", n))

    shutil.rmtree(tmp, ignore_errors=True)

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
