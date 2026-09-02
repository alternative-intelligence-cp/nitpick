#!/usr/bin/env python3
"""Does the harness actually detect a wrong expectation?

Run from the repository root:  python3 bootstrap/harness/selfcheck.py

A test suite that only ever agrees with what it is handed is worse than no suite,
because it reports green while checking nothing. So the harness is itself tested,
against cases where it MUST report a failure:

  - a negative test expecting the wrong code
  - a negative test whose program compiles cleanly
  - a negative test with no expectation at all
  - a negative test reporting a second code its expectation does not name (D-237)
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

# EVERY CASE IS A COMPLETE PROGRAM NOW (1.4.6). Until the switch these ran
# through the Python seed, which accepted a bare `failsafe` and refused
# anything outside subset 1 -- so `trait:T = { };` was a convenient RUNG-001.
# The cases run through the compiler under test now, which supports traits,
# checks failsafe's coverage (D-179) and demands a `main`. The rung the
# negative cases lean on is `limit<Rules>`, one of the eight the real backend
# still refuses; it is a rung rather than a type error on purpose, because the
# property under test is D-085's -- the parser reads it, the backend refuses
# it.
FAILSAFE = """func:failsafe = int32(Error:e) {
    pick (e) {
        (HeapBadRequest) { exit 1i32; },
        (HeapOom) { exit 1i32; },
        (Unreachable) { exit 1i32; },
        (WildLeak) { exit 1i32; },
        (*) { exit 1i32; }
    }
    exit 1i32;
};
"""
MAIN_OK = "func:main = int32(cstring[]:_~argv) { exit 0i32; };\n"
# The refused construct, at line 2 of whatever it is spliced into.
RUNG = """func:build = int32(int32:seed) {
    limit<r_pos> int32:x = seed;
    pass x;
};
"""
# Two findings from two statements, neither a consequence of the other
# (D-237): a narrower integer where `buffer_new` takes `int64` (TYPE-007) and
# a bare may-fail call whose `Result` is discarded (TYPE-039). The case names
# the first only.
EXTRA = """func:noisy = int32() { pass 1i32; };
func:one = buffer() never fails {
    buffer:b = buffer_new(16i32);
    pass (move(b));
};
func:two = NIL() {
    noisy();
    pass NIL;
};
"""

CASES = [
    # (name, kind, source, must_fail, why)
    ("correct-expectation", "negative",
     "// expect-error: NITPICK-RUNG-001\n" + RUNG + MAIN_OK + FAILSAFE,
     False, "a correct expectation must pass"),

    ("wrong-code", "negative",
     "// expect-error: NITPICK-CHECK-001\n" + RUNG + MAIN_OK + FAILSAFE,
     True, "expecting the wrong code must fail"),

    ("compiles-anyway", "negative",
     "// expect-error: NITPICK-RUNG-001\n" + MAIN_OK + FAILSAFE,
     True, "a negative test that compiles must fail"),

    ("no-expectation", "negative", RUNG + MAIN_OK + FAILSAFE,
     True, "a negative test with no expect-error must fail"),

    ("wrong-line", "negative",
     "// expect-error: NITPICK-RUNG-001\n// expect-error-at: 99:1\n"
     + RUNG + MAIN_OK + FAILSAFE,
     True, "expecting the wrong line must fail"),

    # A SECOND CODE NOBODY ASSERTED (D-237, 1.4.8b): the subset rule accepted
    # this shape from 0.8 to 1.4.8; under set equality it fails, which is what
    # makes an unasserted extra a finding rather than a noise floor.
    ("unasserted-extra", "negative",
     "// expect-error: NITPICK-TYPE-007\n" + EXTRA + MAIN_OK + FAILSAFE,
     True, "a diagnostic no expectation names must fail"),

    ("wrong-exit", "positive",
     "// expect-exit: 3\n" + MAIN_OK + FAILSAFE,
     True, "a positive test exiting with the wrong code must fail"),

    ("right-exit", "positive", MAIN_OK + FAILSAFE,
     False, "a positive test exiting as expected must pass"),

    # The one that guards D-085. A file meant to reach the backend but tripping
    # the PARSER instead must be reported, not quietly accepted as "it failed,
    # close enough".
    ("parse-error-not-backend", "negative",
     "// expect-error: NITPICK-RUNG-001\n// expect-no-parse-error\n"
     "func:main = int32() { this is not nitpick };\n" + FAILSAFE,
     True, "a parse error where a backend rejection was expected must fail"),
]


def main():
    tmp = tempfile.mkdtemp(prefix="npk-selfcheck-")
    tools = shutil.which("llc") and shutil.which("ld.lld")
    if tools:
        import subprocess
        subprocess.run(["llc"] + harness.LLC_FLAGS
                       + [harness.RUNTIME_LL, "-o", os.path.join(tmp, "npkrt.o")],
                       capture_output=True)
        # THE COMMITTED SNAPSHOT IS THE COMPILER HERE (1.4.6), not a fresh
        # build of `src/`. These cases ask whether the HARNESS reports
        # failures, not whether today's compiler is right about anything, so
        # the snapshot is both sufficient and ~3 minutes cheaper than
        # rebuilding the compiler to ask a question about the runner.
        harness.BUILDER = harness.build_builder(tmp)
        if not os.path.exists(str(harness.BUILDER)):
            print("selfcheck: no builder (%s)" % harness.BUILDER)
            return 1
        harness.COMPILER = harness.BUILDER

    print("harness self-check")
    bad = 0
    for name, kind, src, must_fail, why in CASES:
        path = os.path.join(tmp, name + ".npk")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(src)
        exp = harness.read_expectations(path)
        fails = harness.KINDS[kind](name, path, exp, tmp, tools)
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
