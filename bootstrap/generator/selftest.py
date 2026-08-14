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


def run(label, pattern, why):
    paths = sorted(glob.glob(os.path.join(ROOT, pattern)))
    if not paths:
        print("  !! no files matched %s" % pattern)
        return 1
    bad = 0
    for p in paths:
        rel = os.path.relpath(p, ROOT)
        try:
            with open(p, "r", encoding="utf-8") as fh:
                parse.parse_source(fh.read(), rel)
        except (lex.LexError, parse.ParseError) as e:
            print("  FAIL %s" % e)
            bad += 1
    print("  %-9s %2d/%2d parsed   (%s)" % (label, len(paths) - bad, len(paths), why))
    return bad


def main():
    print("seed self-test")
    bad = 0
    bad += run("conform", "tests/conformance/*.npk",
               "the subset the seed must lower")
    bad += run("reject", "tests/rejection/*.npk",
               "outside subset 1 -- must still PARSE")
    if bad:
        print("\n%d file(s) failed to parse." % bad)
        return 1
    print("\nok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
