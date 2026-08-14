#!/usr/bin/env python3
"""The Nitpick bootstrap seed.

THROWAWAY (D-085). Reads subset-1 Nitpick and will emit LLVM IR text. It exists
only to compile the compiler's own sources until stage 1 can compile them, and
is deleted at self-hosting.

    Rebuilding from nothing never needs this script: the IR it emits for the
    compiler is committed as bootstrap/seed/stage1.ll. The script is needed to
    REGENERATE that seed, never to build.

Cycle 0.0.2 implements lexing and parsing. Checking and emission are 0.0.3, the
runtime floor and linking are 0.0.4.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lex          # noqa: E402
import parse        # noqa: E402


def parse_file(path):
    with open(path, "r", encoding="utf-8") as fh:
        return parse.parse_source(fh.read(), path)


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: npkseed.py <file.npk> [...]\n")
        return 2

    failures = 0
    for path in argv[1:]:
        try:
            mod = parse_file(path)
            print("ok    %s  (%d items)" % (path, len(mod.items)))
        except (lex.LexError, parse.ParseError) as e:
            print("FAIL  %s" % e)
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
