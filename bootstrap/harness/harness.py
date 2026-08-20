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
CODE_DECL_RE = re.compile(r'pub func:(\w+) = string\(\)\s*\{\s*pass "([A-Z0-9\-]+)"')


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
        if ret.startswith("{ {") or (ret.startswith("{") and ret.endswith("i32 }")):
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
        fails.append("`%s` is in ir_runtime.npk and not in the seed's floor -- "
                     "the two compilers disagree about what a program may call"
                     % name)
    return fails


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

    # `CODE line:col`, and `note CODE line:col` for a note.
    #
    # NOTES ARE NOT FINDINGS AND ARE NOT ASSERTED. `NITPICK-MACRO-009` says where a
    # macro body was expanded; making every expansion-related rejection test list a
    # code about a LOCATION would be asserting the wrong thing, and a suite that
    # could not tell the two apart would have to.
    got = []
    notes = []
    for line in r.stdout.decode("utf-8", "replace").splitlines():
        parts = line.split()
        into = got
        if parts and parts[0] == "note":
            parts = parts[1:]
            into = notes
        if len(parts) == 2 and ":" in parts[1]:
            # `CODE path:line:col` since 0.8.0 (`CODE line:col` before a module
            # graph made bare line numbers ambiguous across sixty files). The
            # span is the LAST two fields; everything before them is the path,
            # which expectations deliberately do not assert.
            pieces = parts[1].rsplit(":", 2)
            if len(pieces) == 3:
                _, ln, cl = pieces
            else:
                ln, _, cl = parts[1].partition(":")
            into.append((parts[0], int(ln), int(cl)))

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
                     for line in r.stdout.decode("utf-8", "replace").splitlines()
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
        got = r.stdout.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: the REAL parser rejected it (%s) -- every file here must "
                "parse and be refused later, which is what D-085 says" % (name, got)]
    return []


# The COMPILER ITSELF: src/main.npk is npkc's entry (nitpick.toml [build]), and
# the harness builds and runs it the way a build system will -- IR on stdout,
# llc and ld.lld after.
EMIT_CHECK = os.path.join(ROOT, "src", "main.npk")


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
        got = r.stdout.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: expected IR, got a refusal (%s)" % (name, got)]
    base = os.path.join(tmp, "prog_" + os.path.basename(path).replace(".", "_"))
    with open(base + ".ll", "wb") as fh:
        fh.write(r.stdout)
    r = subprocess.run(["llc", "-O0", "-filetype=obj", "-relocation-model=static",
                        base + ".ll", "-o", base + ".o"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
        return ["%s: llc rejected the REAL BACKEND's IR: %s"
                % (name, first.strip()[:160])]
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
        failures += check_codes_tested()
        failures += check_codes_centralised()
        failures += check_ll_types_agree()
        failures += check_runtime_sigs_agree()

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
