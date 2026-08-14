#!/usr/bin/env python3
"""The Nitpick bootstrap seed.

THROWAWAY (D-085). Reads subset-1 Nitpick and will emit LLVM IR text. It exists
only to compile the compiler's own sources until stage 1 can compile them, and
is deleted at self-hosting.

    Rebuilding from nothing never needs this script: the IR it emits for the
    compiler is committed as bootstrap/seed/stage1.ll. The script is needed to
    REGENERATE that seed, never to build.

Cycle 0.0.2 implements lexing and parsing; 0.0.3 checking and emission; 0.0.4
the runtime floor and linking.
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import lex          # noqa: E402
import parse        # noqa: E402
import check        # noqa: E402


def parse_file(path):
    with open(path, "r", encoding="utf-8") as fh:
        return parse.parse_source(fh.read(), path)


def main(argv):
    if len(argv) < 2:
        sys.stderr.write("usage: npkseed.py <file.npk> [...]\n")
        return 2

    mods = []
    for path in argv[1:]:
        try:
            mods.append(parse_file(path))
        except (lex.LexError, parse.ParseError) as e:
            print("PARSE-FAIL  %s" % e)
            return 1

    try:
        check.Checker().check(mods)
    except check.RungError as e:
        print("RUNG  %s" % e)
        return 1
    except check.CheckError as e:
        print("CHECK %s" % e)
        return 1

    print("ok    %d module(s) parsed and checked" % len(mods))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
