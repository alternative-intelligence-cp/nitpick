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

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import lex        # noqa: E402
import parse      # noqa: E402
import check      # noqa: E402


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


def conformance():
    """Must parse AND check clean -- this is the subset the seed must lower."""
    paths = sorted(glob.glob(os.path.join(ROOT, "tests/conformance/*.npk")))
    bad = total = 0
    for p in paths:
        base = os.path.basename(p)
        if base in SKIP_ALONE:
            continue
        total += 1
        group = [os.path.join(os.path.dirname(p), d) for d in GROUPS.get(base, [])] + [p]
        try:
            check.Checker().check(_parse(group))
        except Exception as e:
            print("  FAIL %s" % e)
            bad += 1
    print("  conform   %2d/%2d parse + check clean" % (total - bad, total))
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
    bad = conformance() + rejection()
    if bad:
        print("\n%d failure(s)." % bad)
        return 1
    print("\nok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
