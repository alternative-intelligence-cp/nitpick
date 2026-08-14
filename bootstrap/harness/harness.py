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


def group_for(path, all_paths):
    """A test file plus whatever it imports, in dependency order."""
    deps = []
    with open(path, "r", encoding="utf-8") as fh:
        for m in USE_RE.finditer(fh.read()):
            cand = os.path.join(os.path.dirname(path), m.group(1))
            if os.path.exists(cand):
                deps.append(cand)
    return deps + [path]


def imported_by_others(paths):
    used = set()
    for p in paths:
        for d in group_for(p, paths)[:-1]:
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
    r = subprocess.run(["llc", "-filetype=obj", "-relocation-model=static",
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
        r = subprocess.run(["llc", "-filetype=obj", "-relocation-model=static",
                            RUNTIME_LL, "-o", os.path.join(tmp, "npkrt.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("runtime floor did not compile: %s" % r.stderr.strip()[:200])
            return 1

    total = 0
    failures = []
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
        print("  %-11s %2d %s test(s)" % (t["name"], len(run_paths), kind))

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
