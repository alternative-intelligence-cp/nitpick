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

    got = []
    for line in r.stdout.decode("utf-8", "replace").splitlines():
        parts = line.split()
        if len(parts) == 2 and ":" in parts[1]:
            ln, _, cl = parts[1].partition(":")
            got.append((parts[0], int(ln), int(cl)))

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
            n = 0
            for p in sorted(set(all_sources)) + grammar:
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

            # And whole programs that must be ACCEPTED, in full silence.
            #
            # A REJECTION SUITE CANNOT TELL A CORRECT CHECKER FROM ONE THAT
            # REFUSES EVERYTHING. Every negative test above passes trivially for an
            # analysis that answers "violation" to every question, and cycle 0.5's
            # analyses are deliberately conservative -- they fail closed on fuel
            # exhaustion, on an unclassified node, on anything undecidable -- so
            # over-refusal is the failure mode they are most likely to have.
            n = 0
            for p in sorted(glob.glob(os.path.join(ROOT, "tests", "types",
                                                   "accept", "**", "*.npk"),
                                      recursive=True)):
                failures += check_type_accept(tc, p, os.path.relpath(p, ROOT))
                n += 1
            print("  %-11s %2d type-acceptance test(s)" % ("accept", n))

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
