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
  - a layout pin that is not what opt derives from the triple, no layout pin,
    no triple pin, and a module whose header is not the pinned one (E-8,
    D-322 (5))

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
import subprocess
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
        (StackExhausted) { exit 1i32; },
        (MachineFault) { exit 1i32; },
        (DecreasesViolated) { exit 1i32; },
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
# THE NEGATIVE CONSTRUCT: a type mismatch, refused by the frontend with
# TYPE-007 (1.5.4 step 4). It was a `prove` until 1.5.4 lowered that, a
# `requires` before 1.5.3, an `async` function before 1.1.4: the last
# construct that rung fell with this subcycle, so the runner's rules about a
# negative test are exercised on a frontend refusal now -- one the SNAPSHOT
# (which compiles these cases, 1.4.6) and the compiler under test report
# identically, which a rule added this cycle would not be.
RUNG = """func:build = int32(int32:seed) {
    bool:flag = seed;
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
          "// expect-error: NITPICK-TYPE-007\n", RUNG + MAIN_OK + FAILSAFE,
          False, "a correct expectation must pass"),

    _case("wrong_code", "negative",
          "// expect-error: NITPICK-CHECK-001\n", RUNG + MAIN_OK + FAILSAFE,
          True, "expecting the wrong code must fail"),

    _case("compiles_anyway", "negative",
          "// expect-error: NITPICK-TYPE-007\n", MAIN_OK + FAILSAFE,
          True, "a negative test that compiles must fail"),

    _case("no_expectation", "negative", "", RUNG + MAIN_OK + FAILSAFE,
          True, "a negative test with no expect-error must fail"),

    _case("wrong_line", "negative",
          "// expect-error: NITPICK-TYPE-007\n// expect-error-at: 99:1\n",
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


# The toy model both `floor-control-blind` cases run (npkg's copy is
# `selfcheck.npk`'s, byte for byte): `set` fires once AND ONLY BEFORE `tick`
# (its guard asks `y = 0`), and `tick` sets y and clears x, so the two are
# never 1 together -- unless a mutation drops the clear, which is what the
# seeing control does. THE `y = 0` CONJUNCT ARRIVED AT 1.5.6b STEP 4d: until
# then this comment claimed the safety and the model did not have it -- `tick`
# then `set` reaches `both` in two steps -- and nothing had ever decided the
# toy's own row (the two cases decide its CONTROLS). The explicit reading
# (D-295) found it on its first run, in the self-check's own fixture.
TOY = ('(model toy (state (x 0 1) (y 0 1) (done 0 1)) (init (and (= x 0) (= y 0) (= done 0))) (thread a (step set (ir @f entry) (guard (and (= x 0) (= done 0) (= y 0))) (next (x 1) (done 1)))) (thread b (step tick (ir @g entry) (guard (= y 0)) (next (y 1) (x 0)))) (bad both (and (= x 1) (= y 1))) (depth 6) (preempt 3) %s)')
BLIND = '(control blind both (remove b tick))'
SEEING = '(control seeing both (replace b tick (guard (= y 0)) (next (y 1))))'

# The explicit reading's texts (1.5.6b step 4d; D-295; npkg's copies are `selfcheck.npk`'s, byte for
# byte). MX_UNSAFE is the toy with the clear ALREADY dropped from the model itself: `both` is reachable
# in two steps. MX_RANGE counts past its variable's range unless the guard is spliced in. MX_UNREADABLE
# uses an operator the models' grammar does not have.
MX_UNSAFE = ('(model toy (state (x 0 1) (y 0 1) (done 0 1)) (init (and (= x 0) (= y 0) (= done 0))) (thread a (step set (ir @f entry) (guard (and (= x 0) (= done 0))) (next (x 1) (done 1)))) (thread b (step tick (ir @g entry) (guard (= y 0)) (next (y 1)))) (bad both (and (= x 1) (= y 1))) (depth 6) (preempt 3))')
MX_RANGE = ('(model toy (state (n 0 2)) (init (and (= n 0))) (thread a (step inc (ir @f entry) %s(next (n (+ n 1))))) (bad never (= n 9)) (depth 6) (preempt 3))')
MX_UNREADABLE = ('(model toy (state (n 0 2)) (init (and (= n 0))) (thread a (step inc (ir @f entry) (guard (< n 2)) (next (n (mod (+ n 1) 3))))) (bad never (= n 9)) (depth 6) (preempt 3))')


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

    # A `<derived-` PATH FAILS A UNIT (D-259, 1.5.2b step 4), in the direction
    # of the defect: the parser handed a canned finding at `<derived-1>:6:12`
    # must flag it, and the same finding at a real path must not. Unit calls
    # on `parse_findings`, no compiler involved; `npkg/selfcheck.npk` carries
    # the same two cases by name.
    _, _, flagged = harness.parse_findings(
        "NITPICK-TYPE-019 <derived-1>:6:12: `bool` has no method `cmp`\n")
    okd = len(flagged) == 1
    if not okd:
        bad += 1
    print("  %-26s %-4s  %s" % ("derived-path", "ok" if okd else "BAD",
                                "a finding at a `<derived-N>` line nobody wrote must be flagged"))
    _, _, unflagged = harness.parse_findings(
        "NITPICK-TYPE-019 p.npk:6:12: `bool` has no method `cmp`\n")
    okc = len(unflagged) == 0
    if not okc:
        bad += 1
    print("  %-26s %-4s  %s" % ("derived-path-control", "ok" if okc else "BAD",
                                "the same finding at a real path must not be flagged"))

    # AN EXPECTATION WRITTEN AFTER CODE IS FLAGGED, AND A FIXTURE'S PROSE IS
    # NOT (1.5.8b step 4): the expectation reader takes a `//` comment that is
    # the whole line, so `stmt;  // expect-error-at:3 CODE` parses as nothing
    # and the suite walk would skip the file as a fixture -- `wrap_kinds.npk`
    # asserted sixteen refusals and ran zero. Unit calls, no compiler involved;
    # `npkg/selfcheck.npk` carries the same two cases by name.
    stray = harness.stray_expectation_lines(
        "mod:m;\nfunc:f = int32() never fails {\n"
        "    discard(1tbb32 +% 1tbb32);   // expect-error-at:3 NITPICK-TYPE-078\n"
        "    pass 0i32;\n};\n")
    oks = stray == [3]
    if not oks:
        bad += 1
    print("  %-26s %-4s  %s" % ("stray-expectation", "ok" if oks else "BAD",
                                "an expectation written after code must be flagged, by line"))
    calm = harness.stray_expectation_lines(
        "// A FIXTURE for header_mismatch.npk (no expect-error: the resolve stage skips\n"
        "// it).\nmod:m;\n")
    okf = calm == []
    if not okf:
        bad += 1
    print("  %-26s %-4s  %s" % ("stray-expectation-control", "ok" if okf else "BAD",
                                "a fixture whose leading comment mentions the word must not be"))

    # THE SHARED-STATE BELT REPORTS AN UNCLASSIFIED WORD AND A PLAIN ACCESS OF
    # AN ATOMIC ONE (1.5.6 step 0, D-290), and passes a classified word: one
    # synthetic floor text, three spec texts. `npkg/selfcheck.npk` carries the
    # same three cases by name, over the same texts.
    import floor
    sh_floor = ("@npk_flag = internal global i32 0\n"
                "define void @f(ptr %fr) {\nentry:\n  %w = getelementptr %npk.hdr, ptr %fr, i32 0, i32 2\n"
                "  %v = load i32, ptr %w\n  %g = load i32, ptr @npk_flag\n  ret void\n}\n")
    sh_none = "(shared (word @npk_flag owner-only))\n"
    sh_atomic = "(shared (word %npk.hdr 2 atomic) (word @npk_flag owner-only))\n"
    sh_ok = "(shared (word %npk.hdr 2 owner-only) (word @npk_flag owner-only))\n"
    for name, spec_text, must_fail, why in (
            ("shared-unclassified", sh_none, True,
             "an access of a word the (shared ...) section does not classify must fail"),
            ("shared-plain-atomic", sh_atomic, True,
             "a plain load of a word classified atomic must fail"),
            ("shared-classified-control", sh_ok, False,
             "the same access under a stated classification must pass")):
        fails = floor.check_shared(sh_floor, spec_text, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      the belt accepted it; it should not have")
            else:
                print("      the belt rejected it: %s" % fails[0])

    # THE STACK RULE (1.5.6b; DEF-52, DEF-53). SECOND HALF, `floor.check_stack`:
    # an alloca handed to a call before its entry block has defined ALL of it
    # must fail -- undefined, and half defined -- and the same alloca under one
    # whole-size memset, or under covering stores, must pass. FIRST HALF: D-173's
    # own `check_allocas_hoisted` REACHES A FLOOR TEXT -- an alloca in a loop body
    # of a hand-written define must fail and the same alloca hoisted must pass;
    # that the check was never pointed at the floor WAS the defect.
    # `npkg/selfcheck.npk` carries the same six cases by name, over the same texts.
    st_undef = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f() {\nentry:\n  %m = alloca [2 x i64], align 16\n  %r = call i64 @k(ptr %m)\n  ret i64 %r\n}\n"
    st_half = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f() {\nentry:\n  %m = alloca [2 x i64], align 16\n  %m0 = getelementptr [2 x i64], ptr %m, i64 0, i64 0\n  store i64 0, ptr %m0\n  %r = call i64 @k(ptr %m)\n  ret i64 %r\n}\n"
    st_memset = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f() {\nentry:\n  %m = alloca [2 x i64], align 16\n  call void @llvm.memset.p0.i64(ptr %m, i8 0, i64 16, i1 false)\n  %r = call i64 @k(ptr %m)\n  ret i64 %r\n}\n"
    st_stores = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f() {\nentry:\n  %m = alloca [2 x i64], align 16\n  %m0 = getelementptr [2 x i64], ptr %m, i64 0, i64 0\n  store i64 0, ptr %m0\n  %m1 = getelementptr [2 x i64], ptr %m, i64 0, i64 1\n  store i64 0, ptr %m1\n  %r = call i64 @k(ptr %m)\n  ret i64 %r\n}\n"
    st_loop = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f(i64 %n) {\nentry:\n  br label %loop\nloop:\n  %i = phi i64 [ 0, %entry ], [ %j, %body ]\n  %d = icmp uge i64 %i, %n\n  br i1 %d, label %fin, label %body\nbody:\n  %m = alloca i64, align 8\n  store i64 1, ptr %m\n  %r = call i64 @k(ptr %m)\n  %j = add i64 %i, 1\n  br label %loop\nfin:\n  ret i64 0\n}\n"
    st_hoisted = "declare i64 @k(ptr)\ndeclare void @llvm.memset.p0.i64(ptr, i8, i64, i1)\ndefine i64 @f(i64 %n) {\nentry:\n  %m = alloca i64, align 8\n  store i64 1, ptr %m\n  br label %loop\nloop:\n  %i = phi i64 [ 0, %entry ], [ %j, %body ]\n  %d = icmp uge i64 %i, %n\n  br i1 %d, label %fin, label %body\nbody:\n  %r = call i64 @k(ptr %m)\n  %j = add i64 %i, 1\n  br label %loop\nfin:\n  ret i64 0\n}\n"
    for name, text, must_fail, first_half, why in (
            ("alloca-not-defined", st_undef, True, False,
             "an alloca handed to a call with none of it defined must fail"),
            ("alloca-half-defined", st_half, True, False,
             "an alloca with 8 of its 16 bytes stored must fail"),
            ("alloca-memset-control", st_memset, False, False,
             "the same alloca under one whole-size memset must pass"),
            ("alloca-stores-control", st_stores, False, False,
             "the same alloca under two covering stores must pass"),
            ("floor-alloca-not-entry", st_loop, True, True,
             "D-173's check must reach a floor text: an alloca in a loop body must fail"),
            ("floor-alloca-entry-control", st_hoisted, False, True,
             "the same alloca hoisted to the entry block must pass")):
        fails = (harness.check_allocas_hoisted(text, name) if first_half
                 else floor.check_stack(text, name))
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      the belt accepted it; it should not have")
            else:
                print("      the belt rejected it: %s" % fails[0])

    # THE FLOOR'S STACK RESERVE (D-305, 1.5.8 step 2; `floor.reserve_measure`): a chain of
    # frames over a quarter of the reserve must fail, and so must a floor that recurses other
    # than through the trap route; the same chain under a 64 KiB reserve must pass. The sizes
    # are planted (the real ones come from `llc -stack-size-section`). `npkg/selfcheck.npk`
    # carries the same three cases by name, over the same texts and sizes.
    rs_chain = ("define void @f() {\nentry:\n  call void @g()\n  ret void\n}\n"
                "define void @g() {\nentry:\n  call void @h()\n  ret void\n}\n"
                "define void @h() {\nentry:\n  ret void\n}\n")
    rs_cycle = ("define void @f() {\nentry:\n  call void @g()\n  ret void\n}\n"
                "define void @g() {\nentry:\n  call void @f()\n  ret void\n}\n")
    rs_sizes = {"f": 100, "g": 2000, "h": 100}
    for name, text, must_fail, why in (
            ("reserve-over", "@npk_stack_reserve = internal constant i64 4096\n" + rs_chain, True,
             "a chain of 2,224 bytes with the slack is over a quarter of a 4,096-byte reserve and must fail"),
            ("reserve-recurses", "@npk_stack_reserve = internal constant i64 65536\n" + rs_cycle, True,
             "a floor that recurses other than through the trap route must fail"),
            ("reserve-control", "@npk_stack_reserve = internal constant i64 65536\n" + rs_chain, False,
             "the same chain under a 65,536-byte reserve must pass")):
        fails = floor.reserve_measure(text, rs_sizes, name)[0]
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      the belt accepted it; it should not have")
            else:
                print("      the belt rejected it: %s" % fails[0])

    # THE KERNEL-EFFECT TABLE'S BELT (1.5.6b step 1; D-288 as amended): a call site whose
    # option its row does not speak for fails by name -- `arch_prctl(ARCH_GET_FS)` WRITES user
    # memory where `ARCH_SET_FS` does not, and a row keyed by number alone would model either as
    # writing nothing -- the row's own option passes, and a number with no row fails.
    # `npkg/selfcheck.npk` carries the same three cases by name, over the same texts.
    for name, text, must_fail, finding, why in (
            ("floor-syscall-option", "declare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)\ndefine i64 @f(i64 %a) {\nentry:\n  %r = call i64 @npk_sys6(i64 158, i64 4099, i64 %a, i64 0, i64 0, i64 0, i64 0)\n  ret i64 %r\n}\n", True, "floor-syscall-option",
             "arch_prctl with ARCH_GET_FS (4099), an option its row does not speak for, must fail"),
            ("floor-syscall-option-control", "declare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)\ndefine i64 @f(i64 %a) {\nentry:\n  %r = call i64 @npk_sys6(i64 158, i64 4098, i64 %a, i64 0, i64 0, i64 0, i64 0)\n  ret i64 %r\n}\n", False, "floor-syscall-option",
             "the same call with ARCH_SET_FS (4098), the row's option, must pass"),
            ("floor-syscall-row", "declare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)\ndefine i64 @f(i64 %a) {\nentry:\n  %r = call i64 @npk_sys6(i64 12, i64 0, i64 %a, i64 0, i64 0, i64 0, i64 0)\n  ret i64 %r\n}\n", True, "floor-syscall-row",
             "a syscall number the kernel-effect table has no row for (12) must fail")):
        fails = [f for f in floor.check_syscall_names(text) if finding in f]
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the belt accepted it; it should not have" if must_fail else "      the belt rejected it: %s" % fails[0])

    # THE `module asm` CENSUS (1.5.8 step 2b; DEF-64): a syscall written in assembly is a
    # syscall site like any other -- a number without a kernel-effect row fails by name, a
    # `syscall` whose number is not loaded by the instruction right before it fails by name, a
    # `module asm` line the census cannot read fails by name, and a readable one with a row
    # passes. Then the explorer's half: a census syscall the unrouted list does not state fails,
    # a stated one the census does not find fails, and the list that states exactly the census
    # passes. `npkg/selfcheck.npk` carries the same seven by name, over the same texts.
    import explore
    asm_row = ('module asm ".globl f_raw"\nmodule asm "f_raw:"\nmodule asm "  movl $12, %eax"\n'
               'module asm "  syscall"\nmodule asm "  retq"\n')
    asm_unread = ('module asm ".globl f_raw"\nmodule asm "f_raw:"\nmodule asm "  movl $15, %eax"\n'
                  'module asm "  xorl %edi, %edi"\nmodule asm "  syscall"\nmodule asm "  retq"\n')
    asm_line = ('module asm ".globl f_raw" junk\nmodule asm "f_raw:"\nmodule asm "  movl $15, %eax"\n'
                'module asm "  syscall"\nmodule asm "  retq"\n')
    asm_ok = ('module asm ".globl f_raw"\nmodule asm "f_raw:"\nmodule asm "  movl $15, %eax"   ; the number\n'
              'module asm "  syscall"\nmodule asm "  retq"\n')
    for name, text, must_fail, finding, why in (
            ("floor-asm-syscall-row", asm_row, True, "floor-syscall-row",
             "a `module asm` syscall with no kernel-effect row (12) must fail: the census reads assembly"),
            ("floor-asm-syscall-unread", asm_unread, True, "floor-asm-syscall-unread",
             "a `module asm` syscall whose number is not loaded right before it must fail by name"),
            ("floor-asm-line-unread", asm_line, True, "floor-asm-line-unread",
             "a `module asm` line the census cannot read must fail by name, never end its run in silence"),
            ("floor-asm-control", asm_ok, False, "floor-",
             "a readable `module asm` syscall with a row (15) must pass")):
        fails = [f for f in floor.check_syscall_names(text) if finding in f]
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the belt accepted it; it should not have" if must_fail else "      the belt rejected it: %s" % fails[0])
    for name, listed, must_fail, finding, why in (
            ("explore-asm-unlisted", "; nothing stated\n", True, "explore-asm-unlisted",
             "a `module asm` syscall the unrouted list does not state must fail by name"),
            ("explore-asm-stale", "@f_raw 15 the reason\n@g_raw 12 a line the census does not find\n", True,
             "explore-asm-stale", "a stated syscall the census does not find must fail by name"),
            ("explore-asm-control", "@f_raw 15 the reason\n", False, "explore-",
             "the list that states exactly the census must pass")):
        fails = [f for f in explore.check_unrouted(asm_ok, listed)[0] if finding in f]
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the belt accepted it; it should not have" if must_fail else "      the belt rejected it: %s" % fails[0])

    # THE MODELS' SECOND READING (1.5.6b step 4d; D-295): the explicit-state search over a
    # model's whole reachable space. A bad state reachable fails by name and the safe model
    # passes; a control whose bad state is reachable nowhere fails and the seeing one passes;
    # a step a variable's range blocks fails and the guarded one passes. No solver and no
    # tool: the belt reads text. `npkg/selfcheck.npk` carries the same six by name, over the
    # same texts, byte for byte.
    for name, text, finding, must_fail, why in (
            ("model-bad-reachable", MX_UNSAFE, "floor-model-bad-reachable", True,
             "a model whose bad state is reachable must fail the explicit reading by name"),
            ("model-bad-reachable-control", TOY % SEEING, "floor-model-", False,
             "the safe toy model under its seeing control must pass the explicit reading"),
            ("model-control-unreachable", TOY % BLIND, "floor-model-control-unreachable", True,
             "a control whose bad state is reachable nowhere must fail the explicit reading by name"),
            ("model-range-blocks", MX_RANGE % "", "floor-model-range-blocks", True,
             "a step that takes a variable out of its range must fail by name: the unrolling drops it silently"),
            ("model-range-blocks-control", MX_RANGE % "(guard (< n 2)) ", "floor-model-", False,
             "the same step under a guard that keeps the variable in range must pass"),
            ("model-unreadable", MX_UNREADABLE, "floor-model-unreadable", True,
             "an operator outside the models' grammar must fail by name, never be guessed at")):
        fails = [f for f in floor.check_model_explicit("toy", text) if finding in f]
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the explicit reading accepted it; it should not have" if must_fail else "      the explicit reading rejected it: %s" % fails[0][:300])

    # THE FLOOR'S LEG (1.5.6 step 3, D-288): a refuted clause fails by name
    # and the same symbol under a true clause passes; a section with no claim
    # and no residue fails; an instruction form outside the subset fails by
    # line; a manifest row of a floor kind in nitpick.obligations is refused,
    # and a program kind in runtime/npkrt.obligations likewise; the committed
    # floor parses to its counts. One synthetic floor text -- npkg's, byte for
    # byte -- through the tool the harness's verify stage builds
    # (`tools/floorspec.npk`, with the snapshot) over a synthetic root.
    fl_floor = ("define i64 @f(i64 %a) {\nentry:\n  %r = add i64 %a, 1\n  ret i64 %r\n}\n"
                "define i64 @g(i64 %a) {\nentry:\n  switch i64 %a, label %d [ i64 0, label %z ]\n"
                "z:\n  ret i64 0\nd:\n  ret i64 1\n}\n"
                "define void @v(ptr %p, ptr %q) {\nentry:\n  ret void\n}\n"
                "define void @w(ptr %p, ptr %q) {\nentry:\n  store i64 1, ptr %p\n  ret void\n}\n")
    ftool = harness.build_tool(tmp, tools, os.path.join(ROOT, "tools", "floorspec.npk"), "floorspec")
    if not ftool or not os.path.exists(str(ftool)):
        bad += 1
        print("  %-26s %-4s  %s" % ("floor-refuted", "BAD", "tools/floorspec.npk did not build: %s" % ftool))
    else:
        for name, spec_text, must_fail, why in (
                ("floor-refuted", "(symbol @f (ensures (= result a)))\n", True,
                 "a spec clause the floor refutes must fail by name"),
                ("floor-refuted-control", "(symbol @f (ensures (= result (mod (+ a 1) 18446744073709551616))))\n", False,
                 "the same symbol under a clause its IR meets must pass"),
                ("floor-form", "(symbol @g (ensures (< result 2)))\n", True,
                 "an instruction form outside the floor's subset must fail by line"),
                # `(lo len apart-when COND)` (1.5.6c step 0): the range is set apart only where COND holds, so
                # a claim that needs the apartness everywhere is refuted and the same claim under COND discharges
                ("floor-apart-when", "(symbol @v (objects (p 8) (q 8 apart-when (= p 16))) (ensures (=> (and (not (= p 0)) (not (= q 0))) (not (= p q)))))\n", True,
                 "an `apart-when` range is apart only under its condition: a claim that needs it everywhere must fail"),
                ("floor-apart-when-control", "(symbol @v (objects (p 8) (q 8 apart-when (= p 16))) (ensures (=> (and (= p 16) (not (= q 0))) (not (= p q)))))\n", False,
                 "the same claim under the range's condition must pass"),
                # `(views ...)` (1.5.6c step 1): read-only is PROVEN -- the frame row exempts no byte of a
                # view, so a body that stores through one is refuted; and views MAY alias, so a claim that
                # two of them differ is refuted where the same claim over two objects discharges
                ("floor-views-written", "(symbol @w (free x Addr) (requires (not (= p 0))) (views (p 8) (q 8)) (frame (p 8)))\n", True,
                 "a body that writes a byte of a view must fail: the frame row exempts none"),
                ("floor-views-control", "(symbol @w (free x Addr) (requires (not (= p 0))) (objects (p 8)) (views (q 8)) (frame (p 8)))\n", False,
                 "the same body writing its OBJECT, the view apart from it, must pass"),
                ("floor-views-alias", "(symbol @v (views (p 8) (q 8)) (ensures (=> (and (not (= p 0)) (not (= q 0))) (not (= p q)))))\n", True,
                 "two views may be one range: a claim that they differ must fail"),
                ("floor-objects-apart-control", "(symbol @v (objects (p 8) (q 8)) (ensures (=> (and (not (= p 0)) (not (= q 0))) (not (= p q)))))\n", False,
                 "two objects are apart: the same claim over them must pass")):
            froot = os.path.join(tmp, name.replace("-", "_"))
            os.makedirs(os.path.join(froot, "runtime"), exist_ok=True)
            with open(os.path.join(froot, "runtime", "npkrt.ll"), "w", encoding="utf-8") as fh:
                fh.write(fl_floor)
            with open(os.path.join(froot, "runtime", "npkrt.spec"), "w", encoding="utf-8") as fh:
                fh.write(spec_text)
            fdir = os.path.join(froot, "obl")
            r = subprocess.run([ftool, froot, "--emit", fdir], capture_output=True, text=True, timeout=300)
            fails = []
            if r.returncode != 0:
                fails.append((r.stdout + r.stderr).strip())
            else:
                full, f2 = harness.z3_verdicts(fdir, name)
                fails = f2 or harness.floor_verdict_failures(full)
            ok = (bool(fails) == must_fail)
            if name == "floor-form" and ok:
                ok = "instruction form not read" in fails[0]
            if not ok:
                bad += 1
            print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
            if not ok:
                if must_fail:
                    print("      the floor leg accepted it; it should not have%s" % ((": " + fails[0][:200]) if fails else ""))
                else:
                    print("      the floor leg rejected it: %s" % fails[0][:300])
    # floor-control-blind (1.5.6 step 5): a model whose control reaches no bad
    # state fails by name through the same tool; one that does passes. The
    # tool reads runtime/models/ under the synthetic root.
    if ftool and os.path.exists(str(ftool)):
        toy = ("%s\n" % TOY)
        for name, control, must_fail, why in (
                ("floor-control-blind", BLIND, True,
                 "a control that reaches no bad state must fail by name"),
                ("floor-control-blind-control", SEEING, False,
                 "a control that reaches its bad state passes")):
            froot = os.path.join(tmp, name.replace("-", "_"))
            os.makedirs(os.path.join(froot, "runtime", "models"), exist_ok=True)
            with open(os.path.join(froot, "runtime", "npkrt.ll"), "w", encoding="utf-8") as fh:
                fh.write(fl_floor)
            with open(os.path.join(froot, "runtime", "npkrt.spec"), "w", encoding="utf-8") as fh:
                fh.write("(symbol @f (ensures (= result (mod (+ a 1) 18446744073709551616))))\n")
            with open(os.path.join(froot, "runtime", "models", "toy.model"), "w", encoding="utf-8") as fh:
                fh.write(toy % control)
            fdir = os.path.join(froot, "obl")
            r = subprocess.run([ftool, froot, "--emit", fdir], capture_output=True, text=True, timeout=300)
            fails = []
            if r.returncode != 0:
                fails.append((r.stdout + r.stderr).strip())
            else:
                fails = harness.floor_controls(fdir)
            ok = (bool(fails) == must_fail)
            if ok and must_fail:
                ok = "floor-control-blind" in fails[0]
            if not ok:
                bad += 1
            print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
            if not ok:
                if must_fail:
                    print("      the floor leg accepted a blind control; it should not have%s" % ((": " + fails[0][:200]) if fails else ""))
                else:
                    print("      the floor leg rejected a seeing control: %s" % fails[0][:300])
    fun = floor.check_spec(fl_floor, "(symbol @f)\n", "floor-unspecified")
    okfu = bool(fun)
    if not okfu:
        bad += 1
    print("  %-26s %-4s  %s" % ("floor-unspecified", "ok" if okfu else "BAD",
                                "a section with no claim and no residue sentence must fail"))

    # THE EXPLORED FLOOR (1.5.7 step 0; X-1, X-9): the ONE transformer's output
    # over a planted floor is a LITERAL -- every atomic step line gets its
    # point, the `@npk_sys6(` call is routed, the trampoline's own body and the
    # `module asm` line are left alone, the lifecycle hooks land around the
    # clone call and inside `@npk_thread_entry`, the declares are appended --
    # and the site map beside it; then the counting belt: the literal with one
    # point deleted must fail by name, the literal itself must pass. npkg's
    # self-check holds its transformer and its belt to the SAME three texts.
    import explore
    xp_floor = ("module asm \".globl npk_clone_raw\"\n"
                "define i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,\n"
                "                     i64 %a4, i64 %a5, i64 %a6) {\n"
                "  ret i64 0\n}\n"
                "define void @npk_lock(ptr %w) {\nentry:\n"
                "  %c = cmpxchg ptr %w, i32 0, i32 1 seq_cst seq_cst   ; a step\n"
                "  %v = load atomic i32, ptr %w seq_cst, align 4\n"
                "  fence seq_cst\n"
                "  %s = call i64 @npk_sys6(i64 202, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n"
                "  ret void\n}\n"
                "define void @npk_thread_entry(i64 %tlsi) {\nentry:\n"
                "  store atomic i32 1, ptr null seq_cst, align 4\n"
                "  ret void\n}\n"
                "define i64 @npk_thread_start(i64 %sp) {\nentry:\n"
                "  %r = call i64 @npk_clone_raw(i64 4001536, i64 %sp, i64 0, i64 0,\n"
                "                               ptr @npk_thread_entry, i64 0)\n"
                "  ret i64 %r\n}\n"
                "declare i64 @npk_clone_raw(i64, i64, i64, i64, ptr, i64)\n")
    xp_expected = ("module asm \".globl npk_clone_raw\"\n"
                   "define i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,\n"
                   "                     i64 %a4, i64 %a5, i64 %a6) {\n"
                   "  ret i64 0\n}\n"
                   "define void @npk_lock(ptr %w) {\nentry:\n"
                   "  call void @npkx_point(i32 0)\n"
                   "  %c = cmpxchg ptr %w, i32 0, i32 1 seq_cst seq_cst   ; a step\n"
                   "  call void @npkx_point(i32 1)\n"
                   "  %v = load atomic i32, ptr %w seq_cst, align 4\n"
                   "  call void @npkx_point(i32 2)\n"
                   "  fence seq_cst\n"
                   "  %s = call i64 @npkx_sys6(i64 202, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n"
                   "  ret void\n}\n"
                   "define void @npk_thread_entry(i64 %tlsi) {\nentry:\n"
                   "  call void @npkx_begin()\n"
                   "  call void @npkx_point(i32 4)\n"
                   "  store atomic i32 1, ptr null seq_cst, align 4\n"
                   "  call void @npkx_end()\n"
                   "  ret void\n}\n"
                   "define i64 @npk_thread_start(i64 %sp) {\nentry:\n"
                   "  call void @npkx_prespawn()\n"
                   "  %r = call i64 @npk_clone_raw(i64 4001536, i64 %sp, i64 0, i64 0,\n"
                   "                               ptr @npk_thread_entry, i64 0)\n"
                   "  call void @npkx_spawned(i64 %r, i64 0)\n"
                   "  ret i64 %r\n}\n"
                   "declare i64 @npk_clone_raw(i64, i64, i64, i64, ptr, i64)\n"
                   "\n"
                   "; --- the explorer's shim (runtime/explore/npkx.ll; 1.5.7) ---\n"
                   "declare void @npkx_point(i32)\n"
                   "declare i64 @npkx_sys6(i64, i64, i64, i64, i64, i64, i64)\n"
                   "declare void @npkx_prespawn()\n"
                   "declare void @npkx_spawned(i64, i64)\n"
                   "declare void @npkx_begin()\n"
                   "declare void @npkx_end()\n")
    xp_sites = ("0\t@npk_lock\tatomic\t%c = cmpxchg ptr %w, i32 0, i32 1 seq_cst seq_cst\n"
                "1\t@npk_lock\tatomic\t%v = load atomic i32, ptr %w seq_cst, align 4\n"
                "2\t@npk_lock\tatomic\tfence seq_cst\n"
                "3\t@npk_lock\tsys6\t%s = call i64 @npk_sys6(i64 202, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n"
                "4\t@npk_thread_entry\tatomic\tstore atomic i32 1, ptr null seq_cst, align 4\n")
    xp_escaped = xp_expected.replace("  call void @npkx_point(i32 2)\n", "", 1)
    xtool = harness.build_tool(tmp, tools, os.path.join(ROOT, "tools", "explored.npk"), "explored")
    if not xtool or not os.path.exists(str(xtool)):
        bad += 1
        print("  %-26s %-4s  %s" % ("explore-transform", "BAD", "tools/explored.npk did not build: %s" % xtool))
    else:
        xroot = os.path.join(tmp, "explore_transform")
        os.makedirs(os.path.join(xroot, "runtime"), exist_ok=True)
        with open(os.path.join(xroot, "runtime", "npkrt.ll"), "w", encoding="utf-8") as fh:
            fh.write(xp_floor)
        xdir = os.path.join(xroot, "out")
        r = subprocess.run([xtool, xroot, "--emit", xdir], capture_output=True, text=True, timeout=300)
        got_text = got_sites = None
        if r.returncode == 0:
            with open(os.path.join(xdir, "npkrt.explore.ll"), encoding="utf-8") as fh:
                got_text = fh.read()
            with open(os.path.join(xdir, "sites.txt"), encoding="utf-8") as fh:
                got_sites = fh.read()
        okx = got_text == xp_expected and got_sites == xp_sites
        if not okx:
            bad += 1
        print("  %-26s %-4s  %s" % ("explore-transform", "ok" if okx else "BAD",
                                    "the transformer's output over a planted floor is the literal, text and sites"))
        if not okx:
            if r.returncode != 0:
                print("      the transformer refused: %s" % (r.stdout + r.stderr).strip()[:300])
            else:
                print("      the text %s the literal, the sites %s" % ("matches" if got_text == xp_expected else "differs from",
                                                                      "match" if got_sites == xp_sites else "differ"))
    for name, text, must_fail, why in (
            ("explore-step-escapes", xp_escaped, True,
             "an atomic step with no point before it must fail the counting belt by name"),
            ("explore-step-escapes-control", xp_expected, False,
             "the transformer's own output must pass the counting belt")):
        fails = explore.check_totality(xp_floor, text)
        if not must_fail:
            fails += explore.check_sites(text, xp_sites)
        ok = (bool(fails) == must_fail)
        if ok and must_fail:
            ok = "explore-step-escapes" in fails[0]
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the belt accepted it; it should not have" if must_fail else "      the belt rejected it: %s" % fails[0][:300])
    # STEP 6 (X-20): the transformer's PROGRAM mode over a planted unit's IR is a literal too -- through
    # the tool's `--program` mode, the same three texts npkg's self-check holds its module to -- and the
    # pair passes this runner's counting belt and site map (sites from PROGRAM_SITE_BASE).
    xq_prog = 'define internal void @"p.bump"(ptr %c) {\nentry:\n  %t1 = atomicrmw add ptr %c, i64 1 seq_cst\n  %t2 = call i64 @npk_sys6(i64 39, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n  ret void\n}\ndefine i32 @"p.main"() {\nentry:\n  store atomic i64 0, ptr null seq_cst, align 8\n  ret i32 0\n}\ndeclare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)\n'
    xq_expected = 'define internal void @"p.bump"(ptr %c) {\nentry:\n  call void @npkx_point(i32 1000000)\n  %t1 = atomicrmw add ptr %c, i64 1 seq_cst\n  %t2 = call i64 @npkx_sys6(i64 39, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n  ret void\n}\ndefine i32 @"p.main"() {\nentry:\n  call void @npkx_point(i32 1000002)\n  store atomic i64 0, ptr null seq_cst, align 8\n  ret i32 0\n}\ndeclare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)\n\n; --- the explorer\'s shim, for the program\'s own steps (runtime/explore/npkx.ll; 1.5.7 step 6) ---\ndeclare void @npkx_point(i32)\ndeclare i64 @npkx_sys6(i64, i64, i64, i64, i64, i64, i64)\n'
    xq_sites = '1000000\t@"p.bump"\tatomic\t%t1 = atomicrmw add ptr %c, i64 1 seq_cst\n1000001\t@"p.bump"\tsys6\t%t2 = call i64 @npk_sys6(i64 39, i64 0, i64 0, i64 0, i64 0, i64 0, i64 0)\n1000002\t@"p.main"\tatomic\tstore atomic i64 0, ptr null seq_cst, align 8\n'
    okxq = False
    xq_why = "tools/explored.npk did not build"
    if xtool and os.path.exists(str(xtool)):
        xq_dir = os.path.join(tmp, "explore_transform_program")
        os.makedirs(xq_dir, exist_ok=True)
        with open(os.path.join(xq_dir, "p.ll"), "w", encoding="utf-8") as fh:
            fh.write(xq_prog)
        r = subprocess.run([xtool, "--program", os.path.join(xq_dir, "p.ll"), os.path.join(xq_dir, "p.x.ll"),
                            os.path.join(xq_dir, "p.x.sites.txt")], capture_output=True, text=True, timeout=300)
        if r.returncode != 0:
            xq_why = "the tool refused: " + (r.stdout + r.stderr).strip()[:300]
        else:
            with open(os.path.join(xq_dir, "p.x.ll"), encoding="utf-8") as fh:
                xq_text = fh.read()
            with open(os.path.join(xq_dir, "p.x.sites.txt"), encoding="utf-8") as fh:
                xq_got_sites = fh.read()
            xq_belt = (explore.check_totality(xq_prog, xq_text, "explore", "program")
                       + explore.check_sites(xq_text, xq_got_sites, "explore", harness.PROGRAM_SITE_BASE))
            okxq = xq_text == xq_expected and xq_got_sites == xq_sites and not xq_belt
            xq_why = ("the text " + ("matches" if xq_text == xq_expected else "differs from") + " the literal, the sites "
                      + ("match" if xq_got_sites == xq_sites else "differ") + (("; the belt: " + xq_belt[0][:200]) if xq_belt else ""))
    if not okxq:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-transform-program", "ok" if okxq else "BAD",
                                "the transformer's program mode over a planted unit's IR is the literal, text and sites, and passes the counting belt"))
    if not okxq:
        print("      " + xq_why)
    # THE MARKER BELT (1.5.7 step 1; D-299): a `// stress:` program says whether it
    # is explored. Three planted headers through the real reader: unmarked fails
    # by name, `explore: no <reason>` and `explore: N d=4` pass and read as
    # written. npkg's self-check holds its reader to the same three texts.
    xm_dir = os.path.join(tmp, "explore_markers")
    os.makedirs(xm_dir, exist_ok=True)
    for name, head, must_fail, want, why in (
            ("explore-unmarked", "// expect-exit: 0\n// stress: 40\n", True, None,
             "a `// stress:` program with no explore marker must fail by name"),
            ("explore-marked-no", "// expect-exit: 0\n// stress: 40\n// explore: no a reason\n", False, ("no", "a reason"),
             "`// explore: no <reason>` reads as excluded, with its reason"),
            ("explore-marked-n", "// expect-exit: 0\n// stress: 40\n// explore: 7 d=4\n// explore-seed: 3\n", False, (7, 4, [3]),
             "`// explore: N d=D` and `// explore-seed: S` read as written")):
        xm_path = os.path.join(xm_dir, name.replace("-", "_") + ".npk")
        with open(xm_path, "w", encoding="utf-8") as fh:
            fh.write(head + "func:main = int32(cstring[]:_~argv) { exit 0i32; };\n")
        xe = harness.read_expectations(xm_path)
        fails = harness.explore_marker_finding(xe, name)
        ok = (bool(fails) == must_fail)
        if ok and must_fail:
            ok = "explore-unmarked" in fails[0]
        if ok and want == ("no", "a reason"):
            ok = xe.explore is None and xe.explore_no == "a reason"
        if ok and want == (7, 4, [3]):
            ok = xe.explore == 7 and xe.explore_depth == 4 and xe.explore_first == [3] and xe.explore_no == ""
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the marker belt said %r; explore=%r no=%r depth=%r first=%r"
                  % (fails, xe.explore, xe.explore_no, xe.explore_depth, xe.explore_first))
    # THE ORACLE'S OFFSETS (1.5.7 step 3; D-301): the shim's three struct offsets held to
    # the floor's own type lines -- a tampered constant fails by name, the real pair passes.
    with open(harness.RUNTIME_LL, encoding="utf-8") as fh:
        xo_floor = fh.read()
    with open(os.path.join(ROOT, "runtime", "explore", "npkx.ll"), encoding="utf-8") as fh:
        xo_shim = fh.read()
    xo_tampered = xo_shim.replace("@npkx_off_qnext = internal constant i64 48", "@npkx_off_qnext = internal constant i64 40")
    for name, shim_text, must_fail, why in (
            ("explore-oracle-offsets", xo_tampered, True, "a shim reading `qnext` at a byte the floor's type does not put it must fail by name"),
            ("explore-oracle-offsets-control", xo_shim, False, "the shim as committed reads the floor's own offsets")):
        fails = explore.check_oracle_offsets(xo_floor, shim_text)
        ok = (bool(fails) == must_fail)
        if ok and must_fail:
            ok = "explore-oracle-offsets" in fails[0]
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the belt accepted it; it should not have" if must_fail else "      the belt rejected it: %s" % fails[0][:300])
    # THE CONTROLS (1.5.7 step 3; X-10): the grammar refuses a control without its verdict, the
    # substitution refuses `old` lines that do not occur exactly once, and -- end to end through the
    # real mechanism -- a control that plants NOTHING (a comment line replaced by itself) is a
    # control the explorer is blind to, by name, where the committed `store-release` is found.
    xc_bad, xc_why = explore.read_control("program: tests/backend/programs/mutex_basic.npk\nwithin: 2\nold:\n  a\nnew:\n  b\n", "planted")
    okc1 = xc_bad is None and "verdict" in xc_why
    if not okc1:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-malformed", "ok" if okc1 else "BAD", "a control with no `verdict:` must be refused by name"))
    xc_two, _ = explore.read_control("program: p\nverdict: DEADLOCK\nwithin: 2\nold:\n  ret void\nnew:\n  ret void\n", "planted")
    xc_patched, xc_why2 = explore.apply_control(xo_floor, xc_two, "planted")
    okc2 = xc_patched is None and "explore-control-unmatched" in xc_why2
    if not okc2:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-unmatched", "ok" if okc2 else "BAD", "`old` lines that occur more than once in the floor must be refused by name"))
    # STEP 4's two widenings, on planted text in both runners: SEVERAL pairs apply in order -- the second
    # pair's `old` is a line the FIRST pair wrote, so it matches only after the first is applied -- and
    # the verdict judgement's three forms answer the same in both runners.
    xc_multi, xc_why3 = explore.read_control("program: p\nverdict: wrong-exit\nwithin: 1\nold:\n  b\n  c\nnew:\n  B\n  C2\nold:\n  C2\n  d\nnew:\n  D\n", "planted")
    xc_out, xc_why4 = (None, xc_why3) if xc_multi is None else explore.apply_control("a\n  b\n  c\n  d\ne\n", xc_multi, "planted")
    okc3 = xc_out == "a\n  B\n  D\ne\n"
    if not okc3:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-two-pairs", "ok" if okc3 else "BAD", "a control's pairs apply in order, each `old` against the text as it stands"))
    if not okc3:
        print("      got %r (%s)" % (xc_out, xc_why4))
    # STEP 6's PROGRAM control (X-21), on planted text in both runners: `program-old:`/`program-new:` pairs
    # are their own kind -- applied to the program's SOURCE and never to the floor -- and a `program-new:`
    # closing a floor `old:` is refused by name.
    xc_prog, xc_why9 = explore.read_control("program: p.npk\nverdict: wrong-exit\nwithin: 1\nprogram-old:\n    x = 1;\nprogram-new:\n    x = 2;\n", "planted")
    okc9 = False
    xc_pgot = xc_why9
    if xc_prog is not None:
        xc_pf, _ = explore.apply_control("a\n    x = 1;\n", xc_prog, "planted")
        xc_pp, xc_why10 = explore.apply_control("a\n    x = 1;\n", xc_prog, "planted", "prog_subs")
        okc9 = (not xc_prog["subs"] and len(xc_prog["prog_subs"]) == 1 and xc_pf == "a\n    x = 1;\n" and xc_pp == "a\n    x = 2;\n")
        xc_pgot = "%r %s" % (xc_pp, xc_why10)
    xc_mix, xc_why11 = explore.read_control("program: p.npk\nverdict: wrong-exit\nwithin: 1\nold:\n    x = 1;\nprogram-new:\n    x = 2;\n", "planted")
    if okc9:
        okc9 = xc_mix is None and "`program-new:` with no `program-old:`" in xc_why11
    if not okc9:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-program", "ok" if okc9 else "BAD",
                                "`program-old:`/`program-new:` pairs are their own kind, applied to the program's source and never to the floor"))
    if not okc9:
        print("      got %s" % xc_pgot[:200])

    class _XcExp:
        exit = 3
    xc_forms = [(("wrong-exit", 3, "", 0), False), (("wrong-exit", 4, "", 0), True), (("wrong-exit", -11, "", 0), True), (("wrong-exit", 3, "DEADLOCK", 0), True),
                (("exit 7", 7, "", 0), True), (("exit 7", 3, "", 0), False), (("exit 7", 7, "DEADLOCK", 0), True),
                (("LOST-WAKE", 3, "LOST-WAKE", 0), True), (("LOST-WAKE", 4, "LOST-FUTEX-WAKE", 0), False), (("LOST-WAKE", 4, "", 0), False),
                (("late 1000", 3, "", 1000), True), (("late 1000", 3, "", 999), False)]
    xc_got = [explore.control_verdict_met({"verdict": v}, _XcExp, code, word, vrun) for (v, code, word, vrun), _ in xc_forms]
    okc4 = xc_got == [want for _, want in xc_forms]
    if not okc4:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-verdicts", "ok" if okc4 else "BAD", "`wrong-exit`, `exit N`, `late N` and a shim word are judged as the grammar says"))
    if not okc4:
        print("      got %r" % (xc_got,))
    # THE DIRECTED SITES (X-15), against the planted floor's sites: a `preempt-at:` that names one
    # atomic row resolves to its number; one naming two rows, none, or a `sys6` row is refused by name.
    xc_dir = ("program: p\nverdict: DEADLOCK\nwithin: 1\npreempt-at: @npk_lock %v = load atomic i32\npreempt-at: @npk_lock %c = cmpxchg\n"
              "old:\n  a\nnew:\n  b\n")
    xc_dctl, xc_dwhy = explore.read_control(xc_dir, "planted")
    xc_dnums, xc_dfails = explore.resolve_sites(xc_dctl, xp_sites, "planted") if xc_dctl else ([], [xc_dwhy])
    xc_bad2 = [explore.resolve_sites(explore.read_control("program: p\nverdict: D\nwithin: 1\npreempt-at: @npk_lock %\nold:\n  a\nnew:\n  b\n", "planted")[0], xp_sites, "planted")[1],
               explore.resolve_sites(explore.read_control("program: p\nverdict: D\nwithin: 1\npreempt-at: @npk_lock %z = nothing\nold:\n  a\nnew:\n  b\n", "planted")[0], xp_sites, "planted")[1],
               explore.resolve_sites(explore.read_control("program: p\nverdict: D\nwithin: 1\npreempt-at: @npk_lock %s = call i64 @npk_sys6\nold:\n  a\nnew:\n  b\n", "planted")[0], xp_sites, "planted")[1]]
    okc7 = (xc_dnums == [1, 0] and not xc_dfails
            and len(xc_bad2[0]) == 1 and "explore-control-site-unmatched" in xc_bad2[0][0] and "3 site(s)" in xc_bad2[0][0]
            and len(xc_bad2[1]) == 1 and "explore-control-site-unmatched" in xc_bad2[1][0] and "0 site(s)" in xc_bad2[1][0]
            and len(xc_bad2[2]) == 1 and "explore-control-site-kind" in xc_bad2[2][0])
    if not okc7:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-sites", "ok" if okc7 else "BAD", "a directed site resolves to one atomic row of sites.txt, or is refused by name"))
    if not okc7:
        print("      got %r %r %r" % (xc_dnums, xc_dfails, xc_bad2))
    # THE HELD SITES (1.5.8 step 2c; DEF-67), the same way: a `hold-at:` naming one atomic row
    # resolves to its number beside a `preempt-at:` naming another; one naming a `sys6` row is
    # refused by name, naming the directive; a fifth `hold-at:` is refused by the grammar.
    # `npkg/selfcheck.npk` carries the same case over the same texts.
    xc_hctl, xc_hwhy = explore.read_control("program: p\nverdict: D\nwithin: 1\npreempt-at: @npk_lock %v = load atomic i32\n"
                                           "hold-at: @npk_lock %c = cmpxchg\nold:\n  a\nnew:\n  b\n", "planted")
    xc_hnums, xc_hfails = explore.resolve_sites(xc_hctl, xp_sites, "planted", "hold-at") if xc_hctl else ([], [xc_hwhy])
    xc_hpre, _ = explore.resolve_sites(xc_hctl, xp_sites, "planted") if xc_hctl else ([], [])
    xc_hkind = explore.resolve_sites(explore.read_control("program: p\nverdict: D\nwithin: 1\nhold-at: @npk_lock %s = call i64 @npk_sys6\n"
                                                          "old:\n  a\nnew:\n  b\n", "planted")[0], xp_sites, "planted", "hold-at")[1]
    xc_hfive = explore.read_control("program: p\nverdict: D\nwithin: 1\n" + "hold-at: @npk_lock %c = cmpxchg\n" * 5
                                    + "old:\n  a\nnew:\n  b\n", "planted")
    okc8 = (xc_hnums == [0] and not xc_hfails and xc_hpre == [1]
            and len(xc_hkind) == 1 and "explore-control-site-kind" in xc_hkind[0] and "`hold-at:" in xc_hkind[0]
            and xc_hfive[0] is None and "at most four `hold-at:`" in xc_hfive[1])
    if not okc8:
        bad += 1
    print("  %-26s %-4s  %s" % ("explore-control-holds", "ok" if okc8 else "BAD", "a held site resolves to one atomic row, or is refused by name; five are refused"))
    if not okc8:
        print("      got %r %r %r %r %r" % (xc_hnums, xc_hfails, xc_hpre, xc_hkind, xc_hfive))
    if xtool and os.path.exists(str(xtool)) and harness.COMPILER and os.path.exists(str(harness.COMPILER)):
        xc_shim_o = os.path.join(tmp, "npkx_selfcheck.o")
        r = subprocess.run(["llc"] + harness.LLC_FLAGS + [os.path.join(ROOT, "runtime", "explore", "npkx.ll"), "-o", xc_shim_o], capture_output=True, text=True)
        xc_blind_path = os.path.join(tmp, "blind.ctl")
        with open(xc_blind_path, "w", encoding="utf-8") as fh:
            fh.write("; a control that plants nothing: a comment line replaced by itself\nprogram: tests/backend/programs/mutex_basic.npk\n"
                     "verdict: LOST-FUTEX-WAKE\nwithin: 2\nold:\n  ; ...and the eventfd, when the owner's idle wait is the reactor's\n"
                     "new:\n  ; ...and the eventfd, when the owner's idle wait is the reactor's\n")
        xc_found_path = os.path.join(tmp, "found.ctl")
        with open(os.path.join(ROOT, "runtime", "explore", "controls", "store-release.ctl"), encoding="utf-8") as fh:
            with open(xc_found_path, "w", encoding="utf-8") as out:
                out.write(fh.read().replace("within: 20", "within: 3"))
        xc_spec_path = os.path.join(ROOT, "runtime", "explore", "controls", "unconditional-apartness.ctl")
        for name, cpath, must_fail, why in (
                ("explore-control-blind", xc_blind_path, True, "a control the explorer never reaches the verdict of must fail by name"),
                ("explore-control-found", xc_found_path, False, "the committed `store-release` control is found within three seeds"),
                ("explore-control-spec", xc_spec_path, False, "the committed SPEC control plants a false hypothesis and the entry checker reports it (D-302)")):
            if r.returncode != 0:
                fails, seed = ["llc rejected the shim: %s" % r.stderr[:100]], 0
            else:
                fails, seed = harness.run_explore_control(tmp, cpath, name, xc_shim_o)
            ok = (bool(fails) == must_fail)
            if ok and must_fail:
                ok = "explore-control-blind" in fails[0]
            if not ok:
                bad += 1
            print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
            if not ok:
                print("      the mechanism accepted a blind control; it should not have" if must_fail else "      the mechanism said: %s" % fails[0][:300])
    # THE SPEC'S CALLER HYPOTHESES, EXECUTED (1.5.7 step 5, D-302): the real floor and spec through the
    # built tool; the second reader agrees with the tool's assumptions.txt and finds every checker at its
    # symbol's first instruction (clean), and a row whose status is flipped or a call that is gone fails by
    # name (the two tampers).
    if xtool and os.path.exists(str(xtool)):
        xa_dir = os.path.join(tmp, "xa")
        r = subprocess.run([str(xtool), ROOT, "--emit", xa_dir], capture_output=True, text=True, timeout=300)
        xa_ok = r.returncode == 0
        xa_why = ""
        if xa_ok:
            with open(os.path.join(ROOT, "runtime", "npkrt.spec"), encoding="utf-8") as fh:
                xa_spec = fh.read()
            with open(os.path.join(xa_dir, "npkrt.explore.ll"), encoding="utf-8") as fh:
                xa_xt = fh.read()
            with open(os.path.join(xa_dir, "assumptions.txt"), encoding="utf-8") as fh:
                xa_at = fh.read()
            xa_clean = explore.check_assumptions(xo_floor, xa_spec, xa_xt, xa_at)
            xa_flip = explore.check_assumptions(xo_floor, xa_spec, xa_xt, xa_at.replace("\tchecked\t", "\tlisted\t", 1))
            xa_gone = explore.check_assumptions(xo_floor, xa_spec, xa_xt.replace('  call void @"npkx.req.memcpy"', "  ; gone", 1), xa_at)
            xa_ok = (not xa_clean and len(xa_flip) >= 1 and "explore-assumptions" in xa_flip[0]
                     and any("`@memcpy` has 0 checker call(s)" in f for f in xa_gone))
            xa_why = "clean %r flip %r gone %r" % (xa_clean[:1], xa_flip[:1], xa_gone[:1])
        else:
            xa_why = (r.stdout + r.stderr)[:200]
        if not xa_ok:
            bad += 1
        print("  %-26s %-4s  %s" % ("explore-assumptions", "ok" if xa_ok else "BAD", "the entry checkers' rows agree with the second reader, and a flipped row or a missing call fails by name"))
        if not xa_ok:
            print("      %s" % xa_why)
    # WHO KEEPS A SECTION'S ASSUMPTIONS (1.5.6c step 3): TCB.md SS4d's generator on a planted floor whose
    # answer is known by reading it -- `@callee` assumes something of its caller; `@covered` is translated
    # and INLINES it; `@bare` has no section, so nothing covers its call; `@callee` is `internal`, so
    # emitted code cannot reach it. npkg's twin computes the same text from the same two strings, and both
    # are held to ONE literal, so a twin that drifts fails here before it fails on the real floor. Then
    # the currency belt's own failure path: a stale region must fail by name, the current one must pass.
    cl_floor = ("define internal i64 @callee(i64 %a) {\nentry:\n  ret i64 %a\n}\n"
                "define i64 @covered(i64 %a) {\nentry:\n  %r = call i64 @callee(i64 %a)\n  ret i64 %r\n}\n"
                "define i64 @bare(i64 %a) {\nentry:\n  %r = call i64 @callee(i64 %a)\n  ret i64 %r\n}\n")
    cl_spec = "(symbol @callee (requires (< a 10)) (ensures (= result a)))\n(symbol @covered (ensures (= result a)))\n"
    cl_want = ("1 sections have rows AND assume something of their caller. For 0 of them every caller is covered -- no\n"
               "untranslated floor caller, and the symbol is not exported; 1 have a floor caller no row covers; 0 are\n"
               "EXPORTED, so emitted code can call them and nothing proves the assumption there. Under the explorer (1.5.7\n"
               "step 5, D-302) 1 of the 1 hypotheses these sections state are EXECUTED at every call of every explored\n"
               "schedule by a generated entry checker, untranslated floor callers and emitted code alike; the 0 it cannot\n"
               "evaluate are listed by name below the table.\n\n"
               "| symbol | assumes | a row at the call | inlined into | NOT PROVED: floor callers | NOT PROVED: emitted code | executed at every explored call |\n"
               "|---|---|---|---|---|---|---|\n"
               "| `@callee` | requires | -- | `@covered` | `@bare` | no | 1 of 1 |")
    cl_got = floor.callers_region(cl_floor, cl_spec)
    cl_doc = "x\n<!-- BEGIN floor-callers -->\n%s\n<!-- END floor-callers -->\ny\n"
    for name, okc, why in (
            ("tcb-callers-region", cl_got == cl_want,
             "the callers table of a planted floor must be the text its reading gives"),
            ("tcb-callers-stale", any("tcb-callers:" in f for f in harness.check_tcb_callers_current(cl_doc % "stale", cl_floor, cl_spec)),
             "a callers region that is not what the floor and the spec say must fail by name"),
            ("tcb-callers-control", harness.check_tcb_callers_current(cl_doc % cl_want, cl_floor, cl_spec) == [],
             "the generated region must pass")):
        if not okc:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if okc else "BAD", why))
        if not okc and name == "tcb-callers-region":
            print("      the generator said:\n%s" % cl_got)
    # A HEAD READ ONCE IS WRITTEN ONCE, AND A LOOP HOLDS WHAT THE TRANSLATOR READS (1.5.6c step 1): the
    # translator takes the FIRST `frame`/`objects`/`views`/`ensures-trap`/... and drops the rest in silence, and
    # a loop sub-clause outside its four was never read at all -- a claim written and never proven. By name.
    f_ens = "(ensures (= result (mod (+ a 1) 18446744073709551616)))"
    for name, spec_text, phrase, why in (
            ("floor-spec-duplicate", "(symbol @f %s (frame) (frame))\n" % f_ens, "a clause read once and written twice in @f: frame",
             "a second `frame` clause -- read by nobody -- must fail by name"),
            ("floor-spec-duplicate-control", "(symbol @f %s (frame))\n" % f_ens, None,
             "the same section with one `frame` clause must pass"),
            ("floor-spec-loop-clause", "(symbol @g (loop z (unroll 1) (decreases a)) (ensures (< result 2)))\n",
             "a loop sub-clause the grammar does not know in the loop z of @g: decreases",
             "a loop sub-clause the translator does not read must fail by name"),
            ("floor-spec-loop-control", "(symbol @g (loop z (unroll 1)) (ensures (< result 2)))\n", None,
             "the same loop holding only what is read must pass")):
        got = floor.check_spec(fl_floor, spec_text, name)
        okg = (any(phrase in g for g in got) if phrase else not got)
        if not okg:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if okg else "BAD", why))
        if not okg:
            print("      the spec belt said: %s" % (got[0][:300] if got else "nothing"))
    fhash = "0" * 64
    fman = "# nitpick.obligations v1\n# z3 4.16.0 sha256 %s\n# options rlimit=1\n" % fhash
    m1, _ = harness.manifest_rows(fman + "%s floor-spec int discharged none @f\n" % fhash)
    m2, _ = harness.manifest_rows(fman + "%s div-zero int discharged elided @f\n" % fhash, floor=True)
    m3, _ = harness.manifest_rows(fman + "%s floor-spec int discharged none @f\n" % fhash, floor=True)
    okfm = (m1 is None) and (m2 is None) and (m3 is not None)
    if not okfm:
        bad += 1
    print("  %-26s %-4s  %s" % ("floor-kind-misplaced", "ok" if okfm else "BAD",
                                "a floor kind in nitpick.obligations and a program kind in runtime/npkrt.obligations must be refused; the floor kind in its own file must pass"))
    real_fns, _ = floor.parse_floor(open(harness.RUNTIME_LL, encoding="utf-8").read())
    real_cls = harness._floor_classes()
    counts = (len(real_fns), sum(1 for v in real_cls.values() if v == "asm"), sum(1 for v in real_cls.values() if v == "atomic"),
              sum(1 for v in real_cls.values() if v == "syscall"), sum(1 for v in real_cls.values() if v == "pure"))
    okfp = counts == (187, 6, 42, 95, 44)
    if not okfp:
        bad += 1
    print("  %-26s %-4s  %s" % ("floor-parse", "ok" if okfp else "BAD",
                                "the committed floor parses to 187 defines: 6 asm, 42 atomic, 95 syscall, 44 pure"))
    if not okfp:
        print("      the floor parsed to %d defines: %d asm, %d atomic, %d syscall, %d pure" % counts)

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

    # THE LAYOUT PIN REPORTS A MISMATCH (E-8, D-322 (5); 1.6.1 step 1): the
    # layout pin moved, no layout pin, no triple pin -- and the real pins
    # pass. Driven by moving the pins, the only half of the comparison this
    # can move; the pinned `opt` is the other half.
    real_dl, real_tr = harness.DATALAYOUT_PIN, harness.TRIPLE_PIN
    try:
        harness.DATALAYOUT_PIN = (real_dl + "-S64") if real_dl else "e-m:e"
        dl_mismatch = harness.check_datalayout_pin()
        harness.DATALAYOUT_PIN = ""
        dl_unpinned = harness.check_datalayout_pin()
        harness.DATALAYOUT_PIN = real_dl
        harness.TRIPLE_PIN = ""
        tr_unpinned = harness.check_datalayout_pin()
    finally:
        harness.DATALAYOUT_PIN, harness.TRIPLE_PIN = real_dl, real_tr
    for name, fails, why in (
            ("datalayout-mismatch", dl_mismatch,
             "a layout pin that is not what opt derives from the triple must fail"),
            ("datalayout-unpinned", dl_unpinned,
             "no [toolchain] datalayout pin at all must fail"),
            ("triple-unpinned", tr_unpinned,
             "no [toolchain] triple pin at all must fail")):
        ok = bool(fails)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      check_datalayout_pin accepted it; it should not have")
    dl_still = harness.check_datalayout_pin()
    if dl_still:
        bad += 1
        print("  %-26s %-4s  %s" % ("datalayout-restored", "BAD",
                                    "the real pins must pass: %s" % dl_still[0]))
    else:
        print("  %-26s %-4s  %s" % ("datalayout-restored", "ok",
                                    "the pinned layout is what the pinned opt "
                                    "derives from the pinned triple"))
    # EVERY MODULE STATES THE PINNED HEADER (the belt's failure path): no
    # layout line, a wrong layout, the two lines swapped -- and the pinned
    # header passes.
    tail = "\n\ndefine i32 @f() {\n  ret i32 0\n}\n"
    hdr_ok = 'target datalayout = "%s"\ntarget triple = "%s"' % (real_dl, real_tr) + tail
    hdr_missing = 'target triple = "%s"' % real_tr + tail
    hdr_wrong = 'target datalayout = "e-m:e-i64:64-n8:16:32:64-S128"\ntarget triple = "%s"' % real_tr + tail
    hdr_order = 'target triple = "%s"\ntarget datalayout = "%s"' % (real_tr, real_dl) + tail
    for name, text, must_fail, why in (
            ("header-missing", hdr_missing, True, "a module stating no layout must fail"),
            ("header-wrong", hdr_wrong, True, "a module stating a layout that is not the pinned one must fail"),
            ("header-order", hdr_order, True, "the two lines in the other order must fail"),
            ("header-pinned", hdr_ok, False, "the pinned header must pass")):
        fails = harness.check_module_header(text, name)
        ok = bool(fails) if must_fail else not fails
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      check_module_header %s it; it should not have"
                  % ("accepted" if must_fail else "rejected"))

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

    # THE HANG NET KNOWS THE MANIFEST'S BUDGET ROWS (P-13; D-297, S-77): a file
    # of three rows, one of them recorded `budget` in the manifest the run is
    # held to, runs under 120 + 10*3 + 60*1 = 210 s; with no manifest to trust
    # every row takes the larger bound, 120 + 10*3 + 60*3 = 330 s. One planted
    # text and the same two literals in npkg's self-check (`hang-net`,
    # `hang-net-untrusted`): a net that drifts between the runners is caught
    # here, since a solver killed at one runner's net and not the other's is
    # a red run only one of them reports.
    HN_MANIFEST = ("# nitpick.obligations v1\n# z3 4.16.0 sha256 " + "0" * 64 + "\n# options rlimit=1\n"
                   + "a" * 64 + " div-zero int discharged elided @npk_m_f\n"
                   + "b" * 64 + " div-min int budget retained @npk_m_f\n"
                   + "c" * 64 + " div-zero int open retained @npk_m_f\n")
    hn_keys = [("a" * 64, "div-zero", "@npk_m_f"), ("b" * 64, "div-min", "@npk_m_f"),
               ("c" * 64, "div-zero", "@npk_m_f")]
    hn_rows, hn_why = harness.manifest_rows(HN_MANIFEST)
    hn_trusted = (None if hn_rows is None else
                  harness.hang_net(3, harness.file_budget(hn_keys, harness.budget_rows_of(hn_rows))))
    hn_untrusted = harness.hang_net(3, harness.file_budget(hn_keys, harness.budget_rows_of(None)))
    for name, got, want, why in (
            ("hang-net", hn_trusted, 210, "a file of three rows, one recorded `budget`, runs under 120 + 10*3 + 60*1 s"),
            ("hang-net-untrusted", hn_untrusted, 330, "with no manifest to trust every row takes the larger bound: 120 + 10*3 + 60*3 s")):
        ok = got == want
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      the net is %r, not %d%s" % (got, want, "" if hn_rows is not None else " (the planted manifest did not read: %s)" % hn_why))

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
           "        (WildLeak) { exit 9i32; },\n        (StackExhausted) { exit 9i32; },\n        (MachineFault) { exit 9i32; },\n        (DecreasesViolated) { exit 9i32; },\n        (*) { exit 9i32; }\n    }\n    exit 9i32;\n};\n")
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
    VPROVE = ("func:main = int32(cstring[]:argv) {\n    int32:d = (argv.len =>! int32) - 1i32;\n"
              "    prove(d != 0i32);\n    exit 0i32;\n};\n")
    VLFS = ("func:failsafe = int32(Error:e) {\n    pick (e) {\n        (E9) { exit 8i32; },\n"
            "        (LimitViolated) { exit 31i32; },\n        (HeapBadRequest) { exit 9i32; },\n"
            "        (HeapOom) { exit 9i32; },\n        (IntOverflow) { exit 9i32; },\n"
            "        (Unreachable) { exit 9i32; },\n        (WildLeak) { exit 9i32; },\n"
            "        (StackExhausted) { exit 9i32; },\n        (MachineFault) { exit 9i32; },\n        (DecreasesViolated) { exit 9i32; },\n        (*) { exit 9i32; }\n    }\n    exit 9i32;\n};\n")
    for name, head, body, must_fail, why in (
            ("wrong-verdict", "// expect-exit: 21\n// expect-obligation: div-zero discharged 1\n// expect-obligation: div-min discharged 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 13\n// expect-obligation: exhaustive checker 1\n",
             VDIV + VFS, True, "a verify test expecting `discharged` for an opaque divisor must fail"),
            ("right-verdict", "// expect-exit: 21\n// expect-obligation: div-zero open 1\n// expect-obligation: div-min discharged 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 13\n// expect-obligation: exhaustive checker 1\n",
             VDIV + VFS, False, "a verify test naming its rows exactly must pass"),
            ("wrong-limit", "// expect-exit: 0\n// expect-obligation: limit discharged 1\n// expect-obligation: limit-subsume open 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 12\n// expect-obligation: exhaustive checker 1\n",
             VLIM + VLFS, True, "a verify test expecting `discharged` for a limited parameter's entry must fail"),
            ("right-limit", "// expect-exit: 0\n// expect-obligation: limit open 1\n// expect-obligation: limit-subsume open 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 12\n// expect-obligation: exhaustive checker 1\n",
             VLIM + VLFS, False, "a verify test naming a limit's rows exactly must pass"),
            # AN UNPROVEN `prove` REFUSES THE VERIFIED BUILD (1.5.4 step 4, L-21;
            # S-45): a unit whose `prove` is `open` and names no error must
            # FAIL (the `--elide` run refused); the same unit naming
            # NITPICK-VERIFY-001 passes -- the refusal is the expectation. And
            # a `checker` row is read as one: an `exhaustive` row named
            # `discharged` must fail. The texts are npkg's, byte for byte.
            ("prove-open", "// expect-obligation: prove open 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 13\n// expect-obligation: exhaustive checker 1\n",
             VPROVE + VFS, True, "a verify unit whose `prove` is open and names no refusal must fail"),
            ("prove-open-named", "// expect-error: NITPICK-VERIFY-001\n// expect-obligation: prove open 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 13\n// expect-obligation: exhaustive checker 1\n",
             VPROVE + VFS, False, "a verify unit naming the verified build's refusal of its open `prove` must pass"),
            ("checker-row", "// expect-exit: 21\n// expect-obligation: div-zero open 1\n// expect-obligation: div-min discharged 1\n// expect-obligation: overflow open 1\n// expect-obligation: failsafe-post discharged 13\n// expect-obligation: exhaustive discharged 1\n",
             VDIV + VFS, True, "a verify test naming a `checker` row `discharged` must fail")):
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

    # THE ALLOWLIST IS THE RUNTIME'S EXPORTS (1.5.2d step 2b, DEF-21): a
    # non-internal define and a `.globl` name are in; an `internal` define is
    # not. One synthetic floor text, three answers.
    floor_text = ('module asm ".globl asm_entry"\n'
                  'module asm "asm_entry:"\n'
                  'define void @exported() {\nentry:\n  ret void\n}\n'
                  'define internal void @helper() {\nentry:\n  ret void\n}\n')
    exports = harness.runtime_exports(floor_text)
    for name, sym, must_have, why in (
            ("allowlist-exported", "exported", True, "a non-internal define is an export"),
            ("allowlist-globl", "asm_entry", True, "a `.globl` name in the module asm is an export"),
            ("allowlist-internal", "helper", False, "an `internal` define is not an export")):
        ok = ((sym in exports) == must_have)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      runtime_exports answered %s for %s" % (sym in exports, sym))

    # THE TRIM BELT (1.5.2d, D-262 section 1): a prelude `define` referenced
    # nowhere else in the module is an unreferenced item the trim should have
    # dropped -- reported; one the module calls passes.
    untrimmed_ir = ('define i32 @"npk.prelude.unused"() {\nentry:\n  ret i32 0\n}\n'
                    'define i32 @main() {\nentry:\n  ret i32 0\n}\n')
    trimmed_ir = ('define i32 @"npk.prelude.used"() {\nentry:\n  ret i32 0\n}\n'
                  'define i32 @main() {\nentry:\n'
                  '  %t = call i32 @"npk.prelude.used"()\n  ret i32 %t\n}\n')
    for name, text, must_fail, why in (
            ("prelude-untrimmed", untrimmed_ir, True,
             "a prelude define nothing references must fail"),
            ("prelude-trimmed-control", trimmed_ir, False,
             "a prelude define the module calls must pass")):
        fails = harness.check_prelude_trimmed(text, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      check_prelude_trimmed accepted it; it should not have")
            else:
                print("      check_prelude_trimmed rejected a referenced define: %s" % fails[0])

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
    B_ROWS = [("0001", "1", "limit", "h1", "open", '@"npk.m.f"', "2:5", "guard", "5", 1, "int", 0),
              ("0002", "1", "limit-subsume", "h2", "discharged", "@main", "0:9", "bypass", "9", 0, "int", 0)]
    for name, mid, must_fail, why in (
            ("bypass-counted", '  %v = call i32 @"npk.m.f.body"(i32 3) ; prose may spell @"x.body"( too\n', False,
             "a direct call naming the body, counted through the quotes, must pass"),
            ("bypass-missing", '  %v = call i32 @"npk.m.f"(i32 3)\n', True,
             "a discharged limit-subsume row whose call names the entry must fail"),
            ("bypass-as-value", '  %v = call i32 @"npk.m.f.body"(i32 3)\n  %fp = ptrtoint ptr @"npk.m.f.body" to i64\n', True,
             "a `.body` taken as a function value must fail"),
            # A RAISE IS NOT A GUARD (D-285, 1.5.4e step 1; DEF-36): a program's
            # `?! DivByZero` lowers to `@npk_raise(i32 -4097)`, which the trap
            # count must not read as a `div-zero` guard; the same text spelled
            # `@npk_trap` is the defect's own shape and must fail.
            ("raise-not-a-guard", '  %v = call i32 @"npk.m.f.body"(i32 3)\n  call void @npk_raise(i32 -4111)\n', False,
             "a program's raise beside the retained group's one trap must pass"),
            ("raise-as-trap", '  %v = call i32 @"npk.m.f.body"(i32 3)\n  call void @npk_trap(i32 -4111)\n', True,
             "the same raise spelled as a guard's trap must fail: two traps for one retained")):
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

    # THE CONTRACT BELTS (1.5.3 step 2, L-13): traps are counted BY GROUP -- a
    # retained entry `requires` row keeps one trap per clause in the `.req`
    # predicate (its `traps` column), a retained `ensures` row one at its
    # seam -- a `held` row's discharge bypasses nothing, and a call whose
    # bypass rows are not ALL discharged names the entry. The texts are
    # npkg's, byte for byte.
    C_IR = ('define i32 @"npk.m.g.body"(i32 %a0) {\nentry:\n  ret i32 %a0\n}\n'
            'define i32 @"npk.m.g"(i32 %a0) {\nentry:\n  %q = call i8 @"npk.m.g.req"(i32 %a0)\n'
            '  %r = tail call i32 @"npk.m.g.body"(i32 %a0)\n  ret i32 %r\n}\n'
            'define i8 @"npk.m.g.req"(i32 %a0) {\nentry:\n  %c1 = icmp sgt i32 %a0, 0\n'
            '  br i1 %c1, label %ok1, label %bad1\nbad1:\n  call void @npk_chain_reset(i32 1)\n'
            '  call void @npk_trap(i32 -4112)\n  unreachable\nok1:\n  %c2 = icmp slt i32 %a0, 9\n'
            '  br i1 %c2, label %ok2, label %bad2\nbad2:\n  call void @npk_chain_reset(i32 2)\n'
            '  call void @npk_trap(i32 -4112)\n  unreachable\nok2:\n  ret i8 1\n}\n'
            'define i32 @main() {\nentry:\n  %v = call i32 @"npk.m.g"(i32 3)\n'
            '  %e = icmp sgt i32 %v, 0\n  br i1 %e, label %eok, label %ebad\nebad:\n'
            '  call void @npk_chain_reset(i32 3)\n  call void @npk_trap(i32 -4113)\n  unreachable\neok:\n'
            '  ret i32 %v\n}\n')
    C_ENTRY = ("0001", "1", "requires", "h1", "open", '@"npk.m.g"', "2:7", "guard", "7", 2, "int", 0)
    C_SEAM = ("0002", "1", "ensures", "h2", "open", "@main", "1:11", "guard", "11", 1, "int", 0)
    C_HELD = ("0002", "2", "requires", "h3", "discharged", "@main", "0:13", "held", "13", 0, "int", 0)
    for name, rows, must_fail, why in (
            ("contract-traps", [C_ENTRY, C_SEAM, C_HELD], False,
             "two `-4112` traps for a retained two-clause entry row and one `-4113` for a retained seam must pass"),
            ("contract-traps-missing", [("0001", "1", "requires", "h1", "open", '@"npk.m.g"', "2:7", "guard", "7", 1, "int", 0), C_SEAM, C_HELD], True,
             "a retained entry row counting one clause against a two-trap predicate must fail"),
            ("held-not-bypassed", [C_ENTRY, C_SEAM, C_HELD], False,
             "a discharged `held` row whose call names the entry must pass (nothing bypasses a held row)"),
            ("bypass-needs-all", [C_ENTRY, C_SEAM,
                                  ("0002", "2", "limit-subsume", "h3", "discharged", "@main", "0:13", "bypass", "13", 0, "int", 0),
                                  ("0002", "3", "requires", "h4", "open", "@main", "0:13", "bypass", "13", 0, "int", 0)], True,
             "a call whose bypass rows are not all discharged, named by a `.body` call, must fail")):
        text = C_IR
        if name == "bypass-needs-all":
            text = C_IR.replace('%v = call i32 @"npk.m.g"(i32 3)', '%v = call i32 @"npk.m.g.body"(i32 3)')
        fails = harness.elided_ir_checks(rows, text, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      elided_ir_checks accepted it; it should not have")
            else:
                print("      elided_ir_checks rejected it: %s" % fails[0])

    # A GUARD INSIDE A CLAUSE CHECK (DEF-81, 1.5.8b step 3): it exists only
    # where its check is emitted, and a loop head's check runs at every visit,
    # so its guard has a row per context and is elided only when every one is
    # discharged. A discharged seam takes its inner guard with it; an open
    # seam keeps its check, and the inner guard's discharged row is its
    # assume; a head whose back edge is open keeps its check, and an inner
    # guard whose back-edge row is open keeps its trap -- the verified build
    # that elided it on its first visit's proof (the defect's own shape) must
    # fail. The texts are npkg's, byte for byte.
    D_SEAM_D = ("0001", "1", "ensures", "h1", "discharged", "@main", "1:11", "guard", "11", 1, "int", 0)
    D_SEAM_O = ("0001", "1", "ensures", "h1", "open", "@main", "1:11", "guard", "11", 1, "int", 0)
    D_INNER = ("0001", "2", "overflow", "h2", "discharged", "@main", "0:20", "guard", "20", 1, "int", 11)
    D_HEAD = [("0001", "1", "invariant", "h3", "discharged", "@main", "1:30", "guard", "30", 1, "int", 0),
              ("0001", "2", "invariant", "h4", "open", "@main", "1:31", "guard", "30", 1, "int", 0),
              ("0001", "3", "overflow", "h5", "discharged", "@main", "0:40", "guard", "40", 1, "int", 30),
              ("0001", "4", "overflow", "h6", "open", "@main", "0:40", "guard", "40", 1, "int", 30)]
    D_NONE = "define i32 @main() {\nentry:\n  ret i32 0\n}\n"
    D_SEAM_IR = ("define i32 @main() {\nentry:\n  %o = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 1, i32 2)\n"
                 "  %b = extractvalue { i32, i1 } %o, 1\n  %n = xor i1 %b, true\n  call void @llvm.assume(i1 %n)\n"
                 "  %v = extractvalue { i32, i1 } %o, 0\n  %e = icmp sgt i32 %v, 0\n  br i1 %e, label %eok, label %ebad\n"
                 "ebad:\n  call void @npk_chain_reset(i32 3)\n  call void @npk_trap(i32 -4113)\n  unreachable\neok:\n"
                 "  ret i32 %v\n}\n")
    D_HEAD_IR = ("define i32 @main() {\nentry:\n  %o = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 1, i32 2)\n"
                 "  %b = extractvalue { i32, i1 } %o, 1\n  br i1 %b, label %ovf, label %ook\novf:\n"
                 "  call void @npk_trap(i32 -4110)\n  unreachable\nook:\n  %v = extractvalue { i32, i1 } %o, 0\n"
                 "  %e = icmp sgt i32 %v, 0\n  br i1 %e, label %iok, label %ibad\nibad:\n"
                 "  call void @npk_chain_reset(i32 3)\n  call void @npk_trap(i32 -4114)\n  unreachable\niok:\n"
                 "  ret i32 %v\n}\n")
    D_HEAD_WRONG = D_HEAD_IR.replace("  br i1 %b, label %ovf, label %ook\novf:\n  call void @npk_trap(i32 -4110)\n  unreachable\nook:\n",
                                     "  %n = xor i1 %b, true\n  call void @llvm.assume(i1 %n)\n")
    for name, rows, text, must_fail, why in (
            ("clause-guard-gone", [D_SEAM_D, D_INNER], D_NONE, False,
             "a discharged seam's check is not emitted, and its inner guard with it: no assume, no trap must pass"),
            ("clause-guard-kept", [D_SEAM_O, D_INNER], D_SEAM_IR, False,
             "an open seam keeps its check, and its inner guard's discharged row is one assume: must pass"),
            ("clause-guard-kept-missing", [D_SEAM_O, D_INNER], D_SEAM_IR.replace("  %n = xor i1 %b, true\n  call void @llvm.assume(i1 %n)\n", ""), True,
             "an open seam's check with its inner guard's assume missing must fail"),
            ("head-guard-every-visit", D_HEAD, D_HEAD_IR, False,
             "a head whose back edge is open keeps its check, and a guard with an open back-edge row its trap: must pass"),
            ("head-guard-first-visit", D_HEAD, D_HEAD_WRONG, True,
             "the head's guard elided on its first visit's row alone (DEF-81's shape) must fail")):
        fails = harness.elided_ir_checks(rows, text, name)
        ok = (bool(fails) == must_fail)
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            if must_fail:
                print("      elided_ir_checks accepted it; it should not have")
            else:
                print("      elided_ir_checks rejected it: %s" % fails[0])

    # A `rows.txt` LINE THE RUNNERS CANNOT READ FAILS THE RUN (DEF-75, 1.5.8b
    # step 3): npkg's reader skipped one in silence until then, where this one
    # failed by name, so a row could vanish from one runner's belts and not the
    # other's. One case each way, and the shape before the twelfth field (the
    # clause context, DEF-81) among the refused. The lines are npkg's, byte for
    # byte; this runner also decides the well-formed file's one row.
    for name, line, must_fail, why in (
            ("rows-wellformed", "0001\t1\toverflow\th1\t1\t@main\t0:5\tguard\t5\t1\tint\t0", False,
             "a twelve-field row naming a known role must be read"),
            ("rows-eleven-fields", "0001\t1\toverflow\th1\t1\t@main\t0:5\tguard\t5\t1\tint", True,
             "an eleven-field row (the shape before the clause context) must fail by name"),
            ("rows-role-unknown", "0001\t1\toverflow\th1\t1\t@main\t0:5\tguardx\t5\t1\tint\t0", True,
             "a row naming a role the runners do not know must fail by name")):
        rdir = os.path.join(tmp, "rows_" + name.replace("-", "_"))
        os.makedirs(rdir, exist_ok=True)
        with open(os.path.join(rdir, "index.txt"), "w", encoding="utf-8") as fh:
            fh.write("0001\t@main\t1\t0\t0\n")
        with open(os.path.join(rdir, "rows.txt"), "w", encoding="utf-8") as fh:
            fh.write(line + "\n")
        with open(os.path.join(rdir, "0001.smt2"), "w", encoding="utf-8") as fh:
            fh.write("; @main\n; o1 overflow h1 1:1\n; checks 1\n(set-logic ALL)\n(push)\n(assert (not true))\n(check-sat)\n(pop)\n")
        full, fails = harness.z3_verdicts(rdir, name)
        ok = (bool(fails) == must_fail)
        if ok and not must_fail:
            ok = len(full) == 1 and full[0][4] == "discharged" and full[0][11] == 0
        if ok and must_fail:
            ok = "rows.txt" in fails[0]
        if not ok:
            bad += 1
        print("  %-26s %-4s  %s" % (name, "ok" if ok else "BAD", why))
        if not ok:
            print("      z3_verdicts said: %s" % (fails[0] if fails else full))

    shutil.rmtree(tmp, ignore_errors=True)
    if bad:
        print("\n%d case(s) wrong -- the harness cannot be trusted to report "
              "failures." % bad)
        return 1
    print("\nok  the harness reports failures as well as passes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
