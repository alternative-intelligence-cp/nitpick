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
  - an `undef` seed in emitted IR (D-218.10)
  - a z3 whose sha256 is not the pinned one, no z3 pin at all, and a
    profile carrying a wall-clock knob (D-218.1, D-218.2)
  - a verify test expecting `discharged` where the divisor is opaque (P-22)

and five where it must NOT: a correct expectation, the real toolchain, the
word `undef` in a comment or a string constant rather than as a token, the
pinned z3 that is installed, and a verify test naming its rows exactly.

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
# negative cases lean on is `prove` (1.5.4's; `limit<Rules>` until 1.5.2
# lowered it), one the real backend still refuses; it is a rung rather than a type error on purpose, because the
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
# The refused construct, at line 2 of whatever it is spliced into: `prove`,
# 1.5.4's rung -- the LAST verification rung to retire (1.5.2 step 1 moved the
# cases off `limit<Rules>`, which it lowers, so the example moves exactly once
# more, when 1.5.4 chooses the permanent one).
RUNG = """func:build = int32(int32:seed) {
    prove(seed > 0i32);
    pass seed;
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

# EVERY CASE CARRIES ITS HEADER after its directives (D-248, 1.5.1b step 1: a
# file's first declaration is `mod:<basename>;`), which is also why the names
# are identifiers now -- the file is written as `<name>.npk`. `npkg/selfcheck.npk`
# writes the same nine texts, byte for byte.
def _case(name, kind, directives, body, must_fail, why):
    return (name, kind, directives + "mod:" + name + ";\n" + body, must_fail, why)


CASES = [
    # (name, kind, source, must_fail, why)
    _case("correct_expectation", "negative",
          "// expect-error: NITPICK-RUNG-001\n", RUNG + MAIN_OK + FAILSAFE,
          False, "a correct expectation must pass"),

    _case("wrong_code", "negative",
          "// expect-error: NITPICK-CHECK-001\n", RUNG + MAIN_OK + FAILSAFE,
          True, "expecting the wrong code must fail"),

    _case("compiles_anyway", "negative",
          "// expect-error: NITPICK-RUNG-001\n", MAIN_OK + FAILSAFE,
          True, "a negative test that compiles must fail"),

    _case("no_expectation", "negative", "", RUNG + MAIN_OK + FAILSAFE,
          True, "a negative test with no expect-error must fail"),

    _case("wrong_line", "negative",
          "// expect-error: NITPICK-RUNG-001\n// expect-error-at: 99:1\n",
          RUNG + MAIN_OK + FAILSAFE,
          True, "expecting the wrong line must fail"),

    # A SECOND CODE NOBODY ASSERTED (D-237, 1.4.8b): the subset rule accepted
    # this shape from 0.8 to 1.4.8; under set equality it fails, which is what
    # makes an unasserted extra a finding rather than a noise floor.
    _case("unasserted_extra", "negative",
          "// expect-error: NITPICK-TYPE-007\n", EXTRA + MAIN_OK + FAILSAFE,
          True, "a diagnostic no expectation names must fail"),

    _case("wrong_exit", "positive",
          "// expect-exit: 3\n", MAIN_OK + FAILSAFE,
          True, "a positive test exiting with the wrong code must fail"),

    _case("right_exit", "positive", "", MAIN_OK + FAILSAFE,
          False, "a positive test exiting as expected must pass"),

    # The one that guards D-085. A file meant to reach the backend but tripping
    # the PARSER instead must be reported, not quietly accepted as "it failed,
    # close enough".
    _case("parse_error_not_backend", "negative",
          "// expect-error: NITPICK-RUNG-001\n// expect-no-parse-error\n",
          "func:main = int32() { this is not nitpick };\n" + FAILSAFE,
          True, "a parse error where a backend rejection was expected must fail"),

    # AN EXIT NO RUN CAN PRODUCE (O-N15, 1.5.1b step 5): a status is one byte,
    # so `expect-exit: 321` fails forever -- refused at read time, by name.
    _case("exit_out_of_range", "positive",
          "// expect-exit: 321\n", MAIN_OK + FAILSAFE,
          True, "an expect-exit above a byte can never be satisfied and must fail by name"),
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
        # THE VERIFY CASES NEED A COMPILER THAT KNOWS `--obligations` (1.5.0):
        # the snapshot does only after the refresh that follows the pipeline's
        # landing, so the harness hands its compiler under test through
        # NPK_SELFCHECK_COMPILER, and a standalone run without one builds it.
        given = os.environ.get("NPK_SELFCHECK_COMPILER")
        if given and os.path.exists(given):
            harness.COMPILER = given
        else:
            built = harness.build_tool(tmp, tools, harness.EMIT_CHECK, "npkc")
            if built and os.path.exists(str(built)):
                harness.COMPILER = built

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

    # THE SOLVER PIN REPORTS A MISMATCH (D-218.1/D-218.2, 1.5.0): the sha moved,
    # the pin absent, a wall-clock knob in the profile -- and the real pin
    # passes. Driven by moving the pin and the options, the halves this can
    # move.
    real_sha, real_opts = harness.Z3_SHA, harness.Z3_OPTIONS
    try:
        harness.Z3_SHA = "0" * 64
        z3_mismatch = harness.check_verify_pin()
        harness.Z3_SHA = ""
        z3_unpinned = harness.check_verify_pin()
        harness.Z3_SHA = real_sha
        harness.Z3_OPTIONS = real_opts + ["timeout=1000"]
        z3_clock = harness.check_verify_pin()
    finally:
        harness.Z3_SHA, harness.Z3_OPTIONS = real_sha, real_opts
    for name, fails, why in (
            ("z3-mismatch", z3_mismatch, "a z3 whose sha256 is not the pinned one must fail"),
            ("z3-unpinned", z3_unpinned, "no [verify] z3-sha256 pin at all must fail"),
            ("z3-wall-clock", z3_clock, "a wall-clock knob in the profile must fail")):
        ok = bool(fails)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      check_verify_pin accepted it; it should not have")
    z3_ok = harness.check_verify_pin()
    if z3_ok:
        bad += 1
        print("  %-26s %-4s  %s" % ("z3-restored", "BAD",
                                    "the pinned z3 that is installed must pass: %s" % z3_ok[0]))
    else:
        print("  %-26s %-4s  %s" % ("z3-restored", "ok",
                                    "the pinned z3 that is installed must pass"))

    # THE VERIFY STAGE REPORTS A WRONG VERDICT (P-22, 1.5.0): an opaque
    # divisor's obligation is `open`; a test that expects `discharged` must
    # fail, and the same program naming `open` must pass. Both need the pinned
    # z3, which the pin cases above have just checked.
    VDIV = ("func:main = int32(cstring[]:argv) {\n"
            "    int32:zero = (argv.len =>! int32) - 1i32;\n"
            "    int32:q = 10i32 / zero;\n    exit q;\n};\n")
    VFS = ("func:failsafe = int32(Error:e) {\n    pick (e) {\n        (DivByZero) { exit 21i32; },\n"
           "        (DivOverflow) { exit 22i32; },\n        (HeapBadRequest) { exit 9i32; },\n"
           "        (HeapOom) { exit 9i32; },\n        (IntOverflow) { exit 9i32; },\n"
           "        (OutOfBounds) { exit 9i32; },\n        (Unreachable) { exit 9i32; },\n"
           "        (WildLeak) { exit 9i32; },\n        (*) { exit 9i32; }\n    }\n    exit 9i32;\n};\n")
    # AND A LIMIT'S ROWS (1.5.2 step 3): a limited parameter's entry row is
    # `open` (nothing is known of the argument at the callee) and the call
    # site's `limit-subsume` row over an opaque argument is `open` too; a test
    # expecting the entry `discharged` must fail. The texts are npkg's, byte
    # for byte.
    VLIM = ("Rules<int32>:r_pos = { $ > 0i32 };\n"
            "func:limited = int32(limit<r_pos> int32:x) { pass x; };\n"
            "func:main = int32(cstring[]:argv) {\n"
            "    int32:v = limited(argv.len =>! int32) ?! E9;\n    exit (v - 1i32);\n};\n"
            "error:E9;\n")
    VLFS = ("func:failsafe = int32(Error:e) {\n    pick (e) {\n        (E9) { exit 8i32; },\n"
            "        (LimitViolated) { exit 31i32; },\n        (HeapBadRequest) { exit 9i32; },\n"
            "        (HeapOom) { exit 9i32; },\n        (IntOverflow) { exit 9i32; },\n"
            "        (Unreachable) { exit 9i32; },\n        (WildLeak) { exit 9i32; },\n"
            "        (*) { exit 9i32; }\n    }\n    exit 9i32;\n};\n")
    for name, head, body, must_fail, why in (
            ("wrong-verdict", "// expect-exit: 21\n// expect-obligation: div-zero discharged 1\n// expect-obligation: div-min discharged 1\n",
             VDIV + VFS, True, "a verify test expecting `discharged` for an opaque divisor must fail"),
            ("right-verdict", "// expect-exit: 21\n// expect-obligation: div-zero open 1\n// expect-obligation: div-min discharged 1\n",
             VDIV + VFS, False, "a verify test naming its rows exactly must pass"),
            ("wrong-limit", "// expect-exit: 0\n// expect-obligation: limit discharged 1\n// expect-obligation: limit-subsume open 1\n",
             VLIM + VLFS, True, "a verify test expecting `discharged` for a limited parameter's entry must fail"),
            ("right-limit", "// expect-exit: 0\n// expect-obligation: limit open 1\n// expect-obligation: limit-subsume open 1\n",
             VLIM + VLFS, False, "a verify test naming a limit's rows exactly must pass")):
        # THE FILE'S BASENAME MUST MATCH ITS `mod:` NAME (RESOLVE-005), so the
        # hyphen in the case name becomes an underscore in both.
        modname = name.replace("-", "_")
        path = os.path.join(tmp, modname + ".npk")
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(head + "mod:" + modname + ";\n" + body)
        fails = harness.check_verify_program(path, name, harness.read_expectations(path), tmp) if tools else []
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      the verify stage accepted this; it should not have")
            else:
                print("      the verify stage rejected this: %s" % fails[0])

    # THE UNDEF BAN REPORTS AN `undef` (D-218.10, 1.5.0) -- and only a real
    # one: the word in a comment or inside a string constant is prose, not
    # IR, and a ban that fired on prose would be silenced by the first
    # comment that explained it.
    bad_ir = ("define { i32 } @f() {\nentry:\n"
              "  %t0 = insertvalue { i32 } undef, i32 1, 0\n"
              "  ret { i32 } %t0\n}\n")
    ok_ir = ("; an undef in a comment is not an undef\n"
             "@s = constant [7 x i8] c\"undef;\\00\"\n"
             "define i32 @g() {\nentry:\n  ret i32 0\n}\n")
    for name, text, must_fail, why in (
            ("undef-in-emission", bad_ir, True,
             "an `undef` seed in emitted IR must fail"),
            ("undef-in-comment", ok_ir, False,
             "the word in a comment or a string constant must pass")):
        fails = harness.check_no_undef(text, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      check_no_undef accepted it; it should not have")
            else:
                print("      check_no_undef rejected prose: %s" % fails[0])

    # THE BYPASS BELT COUNTS THROUGH THE QUOTES (D-252, 1.5.2 step 4): a
    # `.body` symbol is a D-156 name, quoted, and the code-only text the other
    # counts run over blanks every quoted span -- the belt's first full run
    # counted no `.body` use in any emission and failed five programs for it.
    # One IR text, three shapes over one discharged `limit-subsume` row: the
    # direct call names the body (must pass; a comment spelling the suffix is
    # prose); the call names the entry instead (the defect's own report, "0
    # bypass calls for 1 discharged" row); the body taken as a function VALUE
    # beside a proper call (a `.body` where it may not appear). The texts are
    # npkg's, byte for byte.
    B_HEAD = ('@s = constant [7 x i8] c".body\\22\\00"\n'
              'define i32 @"npk.m.f.body"(i32 %a0) {\nentry:\n  ret i32 %a0\n}\n'
              'define i32 @"npk.m.f"(i32 %a0) {\nentry:\n'
              '  %ok = call i8 @"npk.m.r_pos"(i32 %a0)\n  %p = icmp ne i8 %ok, 0\n'
              '  br i1 %p, label %go, label %trap\ntrap:\n  call void @npk_chain_reset()\n'
              '  call void @npk_trap(i32 -4111)\n  unreachable\ngo:\n'
              '  %r = tail call i32 @"npk.m.f.body"(i32 %a0)\n  ret i32 %r\n}\n'
              'define i32 @main() {\nentry:\n')
    B_TAIL = "  ret i32 %v\n}\n"
    B_ROWS = [("0001", "1", "limit", "h1", "open", '@"npk.m.f"'),
              ("0002", "1", "limit-subsume", "h2", "discharged", "@main")]
    for name, mid, must_fail, why in (
            ("bypass-counted", '  %v = call i32 @"npk.m.f.body"(i32 3) ; prose may spell @"x.body"( too\n', False,
             "a direct call naming the body, counted through the quotes, must pass"),
            ("bypass-missing", '  %v = call i32 @"npk.m.f"(i32 3)\n', True,
             "a discharged limit-subsume row whose call names the entry must fail"),
            ("bypass-as-value", '  %v = call i32 @"npk.m.f.body"(i32 3)\n  %fp = ptrtoint ptr @"npk.m.f.body" to i64\n', True,
             "a `.body` taken as a function value must fail")):
        fails = harness.elided_ir_checks(B_ROWS, B_HEAD + mid + B_TAIL, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      elided_ir_checks accepted it; it should not have")
            else:
                print("      elided_ir_checks rejected it: %s" % fails[0])

    shutil.rmtree(tmp, ignore_errors=True)
    if bad:
        print("\n%d case(s) wrong -- the harness cannot be trusted to report "
              "failures." % bad)
        return 1
    print("\nok  the harness reports failures as well as passes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
