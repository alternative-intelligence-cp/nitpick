#!/usr/bin/env python3
"""Seed self-test. Run from the repository root:  python3 bootstrap/generator/selftest.py

Two properties, and the second is the one that matters:

  1. Every file in tests/conformance/ parses -- the subset the seed must lower.
  2. Every file in tests/rejection/ ALSO parses -- they are valid Nitpick and
     must reach the backend to be rejected there with NITPICK-RUNG-001.

     The parser never restricts. The backend does.  (D-085)

A parse error on a rejection file is a failure of the PARSER, not of the file.
It means the grammar has been made partial, which is what ended
nitpick-bootstrap.
"""

import os
import sys
import glob
import shutil
import subprocess
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import lex        # noqa: E402
import parse      # noqa: E402
import check      # noqa: E402
import emit       # noqa: E402


def _parse(paths):
    mods = []
    for p in paths:
        rel = os.path.relpath(p, ROOT)
        with open(p, "r", encoding="utf-8") as fh:
            mods.append(parse.parse_source(fh.read(), rel))
    return mods


# 10_module_use.npk imports 09_module_lib.npk, so they check as one unit.
GROUPS = {"10_module_use.npk": ["09_module_lib.npk"]}
SKIP_ALONE = {"09_module_lib.npk"}


def _compile(group):
    ck = check.Checker()
    prog = ck.check(_parse(group))
    return emit.emit_module(prog, ck, module_id="selftest")


def conformance():
    """Must parse, check clean, and emit IR that llc accepts."""
    paths = sorted(glob.glob(os.path.join(ROOT, "tests/conformance/*.npk")))
    bad = total = 0
    llc_bad = 0
    have_llc = shutil.which("llc") is not None
    tmp = tempfile.mkdtemp(prefix="npkseed-")
    for p in paths:
        base = os.path.basename(p)
        if base in SKIP_ALONE:
            continue
        total += 1
        group = [os.path.join(os.path.dirname(p), d) for d in GROUPS.get(base, [])] + [p]
        try:
            ir = _compile(group)
        except Exception as e:
            print("  FAIL %s" % e)
            bad += 1
            continue
        if have_llc:
            ll = os.path.join(tmp, base + ".ll")
            with open(ll, "w", encoding="utf-8") as fh:
                fh.write(ir)
            r = subprocess.run(["llc", "-filetype=obj", ll, "-o", os.devnull],
                               capture_output=True, text=True)
            if r.returncode != 0:
                first = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr)
                print("  FAIL %s  llc rejected: %s" % (base, first.strip()[:120]))
                llc_bad += 1
    shutil.rmtree(tmp, ignore_errors=True)
    print("  conform   %2d/%2d parse + check clean" % (total - bad, total))
    if have_llc:
        print("  llc       %2d/%2d emitted IR accepted"
              % (total - bad - llc_bad, total - bad))
    else:
        print("  llc       skipped (not on PATH)")
    return bad + llc_bad


def determinism():
    """Identical input must produce byte-identical IR.

    D-078 requires it, and D-085's stage-1/stage-2 fixpoint check is impossible
    without it -- a compiler whose output varies run to run can never be shown to
    have converged.
    """
    paths = sorted(glob.glob(os.path.join(ROOT, "tests/conformance/*.npk")))
    bad = total = 0
    for p in paths:
        base = os.path.basename(p)
        if base in SKIP_ALONE:
            continue
        total += 1
        group = [os.path.join(os.path.dirname(p), d) for d in GROUPS.get(base, [])] + [p]
        try:
            if _compile(group) != _compile(group):
                print("  FAIL %s emits different IR on a second run" % base)
                bad += 1
        except Exception:
            pass          # already reported by conformance()
    print("  determ    %2d/%2d byte-identical on re-emit" % (total - bad, total))
    return bad


def rejection():
    """Must PARSE, then be rejected by the CHECKER with NITPICK-RUNG-001.

    A parse error here is a failure of the parser, not of the file: it means the
    grammar has been made partial, which is what ended nitpick-bootstrap.
    """
    paths = sorted(glob.glob(os.path.join(ROOT, "tests/rejection/*.npk")))
    bad = 0
    for p in paths:
        rel = os.path.relpath(p, ROOT)
        try:
            mods = _parse([p])
        except (lex.LexError, parse.ParseError) as e:
            print("  FAIL %s  <-- PARSE error; the parser must never restrict" % e)
            bad += 1
            continue
        try:
            check.Checker().check(mods)
        except check.RungError:
            continue                      # exactly what this file exists to prove
        except Exception as e:
            print("  FAIL %s  <-- wrong diagnostic; expected NITPICK-RUNG-001" % e)
            bad += 1
            continue
        print("  FAIL %s accepted; expected NITPICK-RUNG-001" % rel)
        bad += 1
    print("  reject    %2d/%2d parse, then RUNG-rejected" % (len(paths) - bad, len(paths)))
    return bad


def main():
    print("seed self-test")
    bad = conformance() + rejection() + determinism()
    if bad:
        print("\n%d failure(s)." % bad)
        return 1
    print("\nok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
