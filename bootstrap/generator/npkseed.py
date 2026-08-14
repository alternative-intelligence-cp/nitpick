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
import emit         # noqa: E402


def parse_file(path):
    with open(path, "r", encoding="utf-8") as fh:
        return parse.parse_source(fh.read(), path)


def main(argv):
    args, out_path = [], None
    i = 1
    while i < len(argv):
        if argv[i] == "-o" and i + 1 < len(argv):
            out_path = argv[i + 1]
            i += 2
            continue
        args.append(argv[i])
        i += 1
    if not args:
        sys.stderr.write("usage: npkseed.py [-o out.ll] <file.npk> [...]\n")
        return 2

    mods = []
    for path in args:
        try:
            mods.append(parse_file(path))
        except (lex.LexError, parse.ParseError) as e:
            print("PARSE-FAIL  %s" % e)
            return 1

    ck = check.Checker()
    try:
        prog = ck.check(mods)
    except check.RungError as e:
        print("RUNG  %s" % e)
        return 1
    except check.CheckError as e:
        print("CHECK %s" % e)
        return 1

    try:
        ir = emit.emit_module(prog, ck, module_id=mods[0]._path)
    except emit.EmitError as e:
        print("EMIT  %s" % e)
        return 1

    if out_path:
        with open(out_path, "w", encoding="utf-8") as fh:
            fh.write(ir)
    else:
        sys.stdout.write(ir)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
