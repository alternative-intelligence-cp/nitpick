#!/usr/bin/env python3
"""Does the token set still match the grammar?

Run from the repository root:  python3 bootstrap/harness/spec_coverage.py

src/frontend/token_kind.npk was generated from LEXICAL_REFERENCE.md sections 4
and 5. This re-extracts the grammar and checks that every keyword, operator and
punctuation mark still has a token kind, and that no kind exists for something
the grammar no longer has.

The failure it guards against is quiet: a keyword with no token kind lexes as an
ORDINARY IDENTIFIER, so the program still parses and goes wrong somewhere else
entirely. A removed keyword that keeps its kind is the same problem in reverse.

The specs move -- six keywords were removed by D-041 and D-074 in one day -- so
"we generated it once from the spec" is not a durable claim without this.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPEC = os.path.join(ROOT, "meta", "specs", "LEXICAL_REFERENCE.md")
TOKENS = os.path.join(ROOT, "src", "frontend", "token_kind.npk")

PRODUCTIONS = ["MemoryQualifier", "MemoryOrdering", "ControlFlow",
               "VerificationKeyword", "AsyncKeyword", "ModuleKeyword",
               "TypeKeyword", "BuiltinType", "BuiltinHelper",
               "Operator", "Punctuation"]


def terminals(spec, header):
    m = re.search(r'^%s\s*::=(.*?)(?=\n\n|\n[A-Z][A-Za-z]*\s*::=|\n```)' % header,
                  spec, re.S | re.M)
    if not m:
        return None
    return re.findall(r'"([^"]+)"', m.group(1))


def main():
    spec = open(SPEC, encoding="utf-8").read()
    src = open(TOKENS, encoding="utf-8").read()

    # Every variant carries its spelling in a trailing comment.
    covered = set(re.findall(r'=\s*\d+i32;\s*//\s*(\S.*?)\s*$', src, re.M))

    missing, unknown_prod = [], []
    total = 0
    for prod in PRODUCTIONS:
        got = terminals(spec, prod)
        if got is None:
            unknown_prod.append(prod)
            continue
        for t in got:
            total += 1
            if t not in covered:
                missing.append((prod, t))

    print("spec coverage")
    print("  grammar terminals: %d" % total)
    print("  token kinds:       %d" % len(re.findall(r'=\s*\d+i32;', src)))

    bad = 0
    if unknown_prod:
        print("  FAIL production(s) not found in the spec: %s"
              % ", ".join(unknown_prod))
        bad += len(unknown_prod)
    if missing:
        print("  FAIL %d grammar terminal(s) with no token kind:" % len(missing))
        for prod, t in missing[:20]:
            print("      %-22s %r" % (prod, t))
        bad += len(missing)

    if bad:
        print("\nA keyword with no token kind lexes as an identifier, and the "
              "program\nthen goes wrong somewhere else entirely.")
        return 1
    print("\nok  every grammar terminal has a token kind")
    return 0


if __name__ == "__main__":
    sys.exit(main())
