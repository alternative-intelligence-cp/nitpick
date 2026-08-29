#!/usr/bin/env python3
"""Does the harness actually detect a wrong expectation?

Run from the repository root:  python3 bootstrap/harness/selfcheck.py

A test suite that only ever agrees with what it is handed is worse than no suite,
because it reports green while checking nothing. So the harness is itself tested,
against cases where it MUST report a failure:

  - a negative test expecting the wrong code
  - a negative test whose program compiles cleanly
  - a negative test with no expectation at all
  - a positive test that exits with the wrong code
  - a rejection file that fails at PARSE time rather than in the backend
  - a toolchain that is not the pinned version, and no pin at all (D-204)

and two where it must NOT: a correct expectation, and the real toolchain.

The last of the failing cases is the one this project cares about most. It is the
executable form of D-085's rule -- the parser never restricts, the backend does --
and it is what stops the grammar being quietly made partial.
"""

import os
import sys
import shutil
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import harness      # noqa: E402

FAILSAFE = "func:failsafe = int32(tbb32:err) { exit 1i32; };\n"

CASES = [
    # (name, kind, source, must_fail, why)
    ("correct-expectation", "negative", """
// expect-error: NITPICK-RUNG-001
trait:T = { };
""" + FAILSAFE, False, "a correct expectation must pass"),

    ("wrong-code", "negative", """
// expect-error: NITPICK-CHECK-001
trait:T = { };
""" + FAILSAFE, True, "expecting the wrong code must fail"),

    ("compiles-anyway", "negative", """
// expect-error: NITPICK-RUNG-001
func:main = int32() { exit 0i32; };
""" + FAILSAFE, True, "a negative test that compiles must fail"),

    ("no-expectation", "negative", """
struct:Bad = { nosuchtype:x; };
""" + FAILSAFE, True, "a negative test with no expect-error must fail"),

    ("wrong-line", "negative", """
// expect-error: NITPICK-RUNG-001
// expect-error-at: 99:1
trait:T = { };
""" + FAILSAFE, True, "expecting the wrong line must fail"),

    ("wrong-exit", "positive", """
// expect-exit: 3
func:main = int32() { exit 0i32; };
""" + FAILSAFE, True, "a positive test exiting with the wrong code must fail"),

    ("right-exit", "positive", """
func:main = int32() { exit 0i32; };
""" + FAILSAFE, False, "a positive test exiting as expected must pass"),

    # The one that guards D-085. A file that is meant to reach the backend but
    # trips the PARSER instead must be reported, not quietly accepted as "it
    # failed, close enough".
    ("parse-error-not-backend", "negative", """
// expect-error: NITPICK-RUNG-001
// expect-no-parse-error
func:main = int32() { this is not nitpick };
""" + FAILSAFE, True, "a parse error where a backend rejection was expected must fail"),
]


def main():
    tmp = tempfile.mkdtemp(prefix="npk-selfcheck-")
    tools = shutil.which("llc") and shutil.which("ld.lld")
    if tools:
        import subprocess
        subprocess.run(["llc"] + harness.LLC_FLAGS
                       + [harness.RUNTIME_LL, "-o", os.path.join(tmp, "npkrt.o")],
                       capture_output=True)

    print("harness self-check")
    bad = 0
    for name, kind, src, must_fail, why in CASES:
        path = os.path.join(tmp, name + ".npk")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(src)
        exp = harness.read_expectations(path)
        fails = harness.KINDS[kind](name, [path], exp, tmp, tools)
        did_fail = bool(fails)
        ok = (did_fail == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      the harness accepted this; it should have rejected it")
            else:
                print("      the harness rejected this: %s" % fails[0])

    # THE TOOLCHAIN PIN REPORTS A MISMATCH (D-204, 1.4.5). The pin's whole
    # value is its failure path, and a check that has only ever been seen to
    # PASS is a check nobody has tested -- the same reasoning that put every
    # case above in this file. Driven by moving the pin rather than the
    # toolchain, which is the only half of the comparison this can move.
    real = harness.LLVM_PIN
    try:
        harness.LLVM_PIN = real + ".999" if real else "0.0.0"
        mismatch = harness.check_toolchain_pin()
        harness.LLVM_PIN = ""
        unpinned = harness.check_toolchain_pin()
    finally:
        harness.LLVM_PIN = real
    for name, fails, why in (
            ("toolchain-mismatch", mismatch,
             "a toolchain that is not the pinned version must fail"),
            ("toolchain-unpinned", unpinned,
             "no [toolchain] pin at all must fail")):
        ok = bool(fails)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      check_toolchain_pin accepted it; it should not have")
    still_ok = harness.check_toolchain_pin()
    if still_ok:
        bad += 1
        print("  %-26s %-4s  %s" % ("toolchain-restored", "BAD",
                                    "the real toolchain must pass: %s"
                                    % still_ok[0]))
    else:
        print("  %-26s %-4s  %s" % ("toolchain-restored", "ok",
                                    "the pinned toolchain that is installed "
                                    "must pass"))

    shutil.rmtree(tmp, ignore_errors=True)
    if bad:
        print("\n%d case(s) wrong -- the harness cannot be trusted to report "
              "failures." % bad)
        return 1
    print("\nok  the harness reports failures as well as passes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
