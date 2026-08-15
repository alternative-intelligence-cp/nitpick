#!/usr/bin/env python3
"""The Nitpick test harness.

THROWAWAY, alongside the seed (D-085). It lives in bootstrap/ rather than tools/
because it cannot yet be written in Nitpick: subset 1 has no directory reading
and no process spawning, and the runtime floor has no exec. The permanent
harness is `npkg test` (BUILD_REFERENCE section 7), written in Nitpick once
nlibc lands in cycle 0.8.

Run from the repository root:  python3 bootstrap/harness/harness.py

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


# --- expectations ------------------------------------------------------------

class Expect:
    def __init__(self):
        self.errors = []        # [(code, line|None, col|None)]
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

    used = set()
    for p in glob.glob(os.path.join(ROOT, "src", "frontend", "*.npk")):
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


# --- the real parser, on real files ------------------------------------------

PARSE_CHECK = os.path.join(ROOT, "tools", "parse_check.npk")


def build_parse_check(tmp, tools):
    """Compile tools/parse_check.npk and return its path, or None.

    Everything above tests the SEED's parser. This builds the REAL one, so the
    rule D-085 states -- every file in tests/rejection/ must PARSE and be refused
    later -- can be checked against the parser it was written about instead of
    the throwaway one that happens to be running.
    """
    if not tools:
        return None
    out = compile_files(group_for(PARSE_CHECK))
    if out.diags:
        return "DIAG %s" % out.diags[0]
    base = os.path.join(tmp, "parse_check")
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


def check_parses(binary, path, name):
    """The real parser must accept this file with no diagnostics at all."""
    with open(path, "rb") as fh:
        src = fh.read()
    try:
        r = subprocess.run([binary], input=src, capture_output=True, timeout=20)
    except subprocess.TimeoutExpired:
        return ["%s: the real parser did not terminate" % name]
    if r.returncode == 2:
        return ["%s: parse_check could not read stdin" % name]
    if r.returncode == 3:
        return ["%s: the real parser TRAPPED -- a defect in the compiler, not in "
                "this file" % name]
    if r.returncode != 0:
        got = r.stdout.decode("utf-8", "replace").strip().replace("\n", ", ")
        return ["%s: the REAL parser rejected it (%s) -- every file here must "
                "parse and be refused later, which is what D-085 says" % (name, got)]
    return []


# --- driver ------------------------------------------------------------------

def load_targets():
    with open(os.path.join(ROOT, "nitpick.toml"), "rb") as fh:
        manifest = tomllib.load(fh)
    return manifest.get("test", [])


def main(argv):
    targets = load_targets()
    if not targets:
        print("no [[test]] targets in nitpick.toml")
        return 2

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
    failures = []
    all_sources = []
    for t in targets:
        kind = t["kind"]
        paths = sorted(glob.glob(os.path.join(ROOT, t["path"], "*.npk")))
        skip = imported_by_others(paths)
        run_paths = [p for p in paths if os.path.abspath(p) not in skip]
        for p in run_paths:
            total += 1
            name = os.path.relpath(p, ROOT)
            exp = read_expectations(p)
            failures += KINDS[kind](name, group_for(p, paths), exp, tmp, tools)
        all_sources += paths
        print("  %-11s %2d %s test(s)" % (t["name"], len(run_paths), kind))

    # Every source in every suite, plus tests/grammar/, through the REAL parser.
    # A rejection test the real parser cannot read is not testing D-085's rule;
    # it is testing that the seed and the real parser happen to disagree.
    #
    # tests/grammar/ is NEVER compiled and never run. It exists only to be parsed,
    # which is what lets it use the whole language rather than subset 1.
    failures += check_kinds_reachable()

    pc = build_parse_check(tmp, tools)
    if isinstance(pc, str) and not os.path.exists(pc):
        failures.append("tools/parse_check.npk did not build: %s" % pc)
    elif pc:
        grammar = sorted(glob.glob(os.path.join(ROOT, "tests", "grammar", "*.npk")))
        n = 0
        for p in sorted(set(all_sources)) + grammar:
            name = os.path.relpath(p, ROOT)
            failures += check_parses(pc, p, name)
            n += 1
        print("  %-11s %2d real-parser check(s)" % ("grammar", n))

    shutil.rmtree(tmp, ignore_errors=True)

    if not tools:
        print("\n  (llc / ld.lld not on PATH -- positive tests were not run)")

    if failures:
        print("\n%d failure(s):" % len(failures))
        for f in failures:
            print("  - %s" % f)
        return 1
    print("\nok  %d test(s) passed" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
