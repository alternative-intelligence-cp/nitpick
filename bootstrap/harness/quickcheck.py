"""Build `tools/check.npk` once, then run it on files -- the fast iteration loop.

NOT A SUBSTITUTE FOR THE HARNESS, and it does not try to be. `harness.py` is what
CONCLUDES: it runs every suite, sweeps every source through the real parser, and
re-checks that every AST node kind is reachable. Nothing is committed on the
strength of this script.

What it is for is the middle of a subcycle, where the question is "does this one
rule fire on this one file" and a full run costs sixteen minutes to answer it. This
builds the checker once and then answers in milliseconds.

    python3 bootstrap/harness/quickcheck.py                     # just build it
    python3 bootstrap/harness/quickcheck.py a.npk b.npk         # build and run

Rebuild after every edit to `src/` -- it does not watch anything, and a stale
binary answering an old question is the failure mode to expect from it.

Output is `<file>: <exit code>` followed by whatever the checker printed --
`CODE path:line:col: message` per diagnostic (BUILD_REFERENCE 7.1).
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "bootstrap", "generator"))

import harness  # noqa: E402

OUT = os.path.join(ROOT, ".internal", "quickcheck")


def build():
    os.makedirs(OUT, exist_ok=True)
    rt = os.path.join(OUT, "npkrt.o")
    if not os.path.exists(rt):
        r = subprocess.run(["llc"] + harness.LLC_FLAGS + [harness.RUNTIME_LL, "-o", rt],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return "runtime: %s" % r.stderr.strip()[:400]
    # THE BUILDER IS THE COMMITTED SNAPSHOT (D-203/D-205, 1.4.6). Cached in
    # the same .internal/ home as everything else here, and rebuilt only when
    # the snapshot itself changes -- llc over 15 MB of IR is not free, and a
    # snapshot only moves at a refresh.
    b = os.path.join(OUT, "builder")
    if (not os.path.exists(b)
            or os.path.getmtime(b) < os.path.getmtime(harness.SNAPSHOT_LL)):
        made = harness.build_builder(OUT)
        if not os.path.exists(str(made)):
            return made
    harness.BUILDER = b
    res = harness.build_tool(OUT, True, harness.TYPE_CHECK, "check")
    if isinstance(res, str) and not os.path.exists(res):
        return res
    return res


def main():
    binary = build()
    if not os.path.exists(binary):
        print(binary)
        return 1
    print(binary)
    for path in sys.argv[1:]:
        r = subprocess.run([binary, path], capture_output=True, timeout=60)
        print("\n%s: exit %d" % (path, r.returncode))
        out = r.stderr.decode("utf-8", "replace").strip()
        if out:
            print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
