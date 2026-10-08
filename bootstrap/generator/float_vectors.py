#!/usr/bin/env python3
"""1.6.1e, DEF-203 and DEF-209: the known-answer vectors of the compiler's decimal -> float conversion.

THE SPECIFICATION is exact rational arithmetic: the decimal a literal writes is a
rational number, the float it denotes is the nearest value of the format to that
number, ties to even (IEEE 754 roundTiesToEven), a value at or past half-way to
2^(emax+1) is infinity. `ref` below computes it with `fractions.Fraction` and
shares no step with the compiler's algorithm (`float_round`, numeric.npk, which
halves and doubles decimal digit runs).

Two generated files hold the compiler to it, both written here and both held
current by the harness (`check_generated_current` runs `--check`, beside
`gen_tables.py --check`):

  tests/frontend/float_round.npk
      `float_round` and `flt32_bits_as_flt64` called directly: both formats, and
      the two answers the checker refuses (a literal that rounds to infinity, a
      nonzero one that rounds to zero: NITPICK-TYPE-031, D-346) by their flags.
  tests/backend/programs/float_literal_kat.npk
      literals COMPILED, each read back as its bits, at both legs. A literal of
      either width is the compiler's own constant -- the bits `float_round`
      makes of its text. A `flt32` was `fptrunc double <text> to float` until
      DEF-203, and a `flt64` was `double <text>`, converted by LLVM's decimal
      parser, until DEF-209: that parser caps the exponent it reads (see
      `--llvm`), and the `flt64` half holds four texts past the cap.

usage:
  float_vectors.py --write    write both files
  float_vectors.py --check    both files are what this script writes (exit 1 otherwise)
  float_vectors.py --llvm     the vector texts, and five more draws of every family, as
                              `double <text>` through llc: the object's bytes against
                              the specification. A measurement of LLVM's decimal parser,
                              outside the gates -- no emission of ours asks it to convert
                              a decimal since DEF-209. On LLVM 20.1.2 and on 20.1.8 alike
                              (D-349, measured 2026-10-08): none differs among
                              the texts whose decimal exponent is within 24,000, and LLVM
                              is WRONG past that -- its parser caps the exponent, so
                              `0.<24000 zeros>1e24001` (the number 1.0) reads as 0.1 and a
                              longer one as zero. Exit 1 if a text within the cap differs.
"""
import os, random, struct, subprocess, sys, tempfile
from fractions import Fraction
from decimal import Decimal

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
UNIT = os.path.join(ROOT, "tests", "frontend", "float_round.npk")
KAT = os.path.join(ROOT, "tests", "backend", "programs", "float_literal_kat.npk")
FMT32 = (24, 127)
FMT64 = (53, 1023)

# --- the specification ---------------------------------------------------------------

def body_of(text):
    """the numeric body of a literal's text: digits, one point, an exponent; a suffix ends it"""
    out = []
    st = 0
    for c in text:
        if c.isdigit():
            out.append(c)
        elif c == "." and st == 0:
            st = 1; out.append(c)
        elif c in "eE" and st < 2:
            st = 2; out.append(c)
        elif c in "+-" and st == 2 and out[-1] in "eE":
            out.append(c)
        else:
            break
    return "".join(out)

def ref(text, p, emax):
    """(bits, flag): the IEEE encoding of the nearest value (sign clear), flag 0 / 1 infinity / 2 a nonzero number rounding to zero"""
    emin = 1 - emax
    body = body_of(text)
    # An exponent no format reaches is answered without building the number (10^400 is past 2^1024 and
    # 10^-400 below 2^-1075, half the smallest subnormal): `1.0e999999999` is a legal text.
    mant, _, ex = body.lower().partition("e")
    whole, _, frac = mant.partition(".")
    digs = (whole + frac).lstrip("0")
    if digs == "":
        return 0, 0
    mag = len(whole) - (len(whole + frac) - len(digs)) + (int(ex) if ex else 0)   # 10^(mag-1) <= x < 10^mag
    if mag > 400:
        return (2 * emax + 1) << (p - 1), 1
    if mag < -400:
        return 0, 2
    x = Fraction(Decimal(body))
    e = x.numerator.bit_length() - x.denominator.bit_length()
    if Fraction(2) ** e > x: e -= 1
    if Fraction(2) ** (e + 1) <= x: e += 1
    assert Fraction(2) ** e <= x < Fraction(2) ** (e + 1)
    q = max(e, emin) - (p - 1)                  # the exponent of the last kept bit
    y = x / Fraction(2) ** q
    m = y.numerator // y.denominator
    r = y - m
    if r > Fraction(1, 2) or (r == Fraction(1, 2) and (m & 1)): m += 1
    if m == 0:
        return 0, 2
    bits = ((max(e, emin) - emin) << (p - 1)) + m
    if (bits >> (p - 1)) >= 2 * emax + 1:
        return (2 * emax + 1) << (p - 1), 1
    return bits, 0

def f32_as_f64(b32):
    """a flt32's bits as the bits of the flt64 holding the same value"""
    return struct.unpack("<Q", struct.pack("<d", struct.unpack("<f", struct.pack("<I", b32))[0]))[0]

# --- the vectors ---------------------------------------------------------------------

def dyadic(x):
    """the exact decimal expansion of a dyadic rational x > 0, as `III.FFF`"""
    k = x.denominator.bit_length() - 1
    assert x.denominator == 1 << k
    digits = str(x.numerator * 5 ** k)
    if k == 0:
        return digits + ".0"
    if len(digits) <= k:
        digits = "0" * (k - len(digits) + 1) + digits
    return digits[:-k] + "." + digits[-k:]

def sci(digs, e10):
    if len(digs) == 1:
        return "%s.0e%d" % (digs, e10)
    return "%s.%se%d" % (digs[0], digs[1:], e10)

def nudge(text, delta):
    """`III.FFF` plus delta units in its last place"""
    i = text.index(".")
    k = len(text) - i - 1
    v = int(text[:i] + text[i + 1:]) + delta
    assert v > 0
    s = str(v)
    if len(s) <= k:
        s = "0" * (k - len(s) + 1) + s
    return s[:-k] + "." + s[-k:]

def near(mid, nd, dv):
    """the midpoint cut to nd significant digits (dv: 0 below it, 1 above), in exponent form"""
    t = dyadic(mid)
    i = t.index(".")
    whole = t[:i].lstrip("0")
    digs = (t[:i] + t[i + 1:]).lstrip("0")
    e10 = (len(whole) - 1) if whole else -(len(t[i + 1:]) - len(t[i + 1:].lstrip("0")) + 1)
    h = str(int(digs[:nd].ljust(nd, "0")) + dv)
    return sci(h, e10 + (len(h) - nd))

def two_roundings(text):
    """the flt32 a decimal became through a double: python's float is the nearest double, and packing it narrows it"""
    return struct.unpack("<I", struct.pack("<f", float(text)))[0]

def counterexamples(n, seed):
    """n fifteen-digit texts whose nearest flt32 is NOT the float of their nearest double (DEF-203), found by search"""
    rnd = random.Random(seed)
    out = []
    while len(out) < n:
        m = (1 << 23) + rnd.randrange(1 << 23)
        mid = Fraction(2 * m + 1) * Fraction(2) ** (rnd.randint(-100, 100) - 24)
        for dv in (0, 1):
            t = near(mid, 15, dv)
            if two_roundings(t) != ref(t, *FMT32)[0] and t not in out:
                out.append(t)
    return out

def unit_vectors(p, emax, seed):
    """texts for the direct test: every family, the flagged answers included"""
    emin = 1 - emax
    rnd = random.Random(seed)
    lim, hi = (46, 39) if p == 24 else (325, 309)
    out = []
    for _ in range(260):                                   # random decimals, the whole range and beyond
        nd = rnd.randint(1, 30)
        digs = str(rnd.randint(1, 9)) + "".join(str(rnd.randint(0, 9)) for _ in range(nd - 1))
        out.append(sci(digs, rnd.randint(-lim - 3, hi + 1)))
    for _ in range(60):                                    # plain forms
        out.append("%d.%s" % (rnd.randint(0, 10 ** rnd.randint(1, 12)),
                               "".join(str(rnd.randint(0, 9)) for _ in range(rnd.randint(1, 12)))))
    for _ in range(14 if p == 24 else 8):                  # exact midpoints and the texts beside them
        m = (1 << (p - 1)) + rnd.randrange(1 << (p - 1))
        t = dyadic(Fraction(2 * m + 1) * Fraction(2) ** (rnd.randint(emin, emax) - p))
        out += [t, nudge(t, 1), nudge(t, -1)]
    for _ in range(6 if p == 24 else 3):                   # subnormal midpoints
        t = dyadic(Fraction(2 * rnd.randrange(1, 1 << (p - 1)) + 1) * Fraction(2) ** (emin - p))
        out += [t, nudge(t, 1), nudge(t, -1)]
    for _ in range(140):                                   # SHORT decimals beside a midpoint: DEF-203's shape
        m = (1 << (p - 1)) + rnd.randrange(1 << (p - 1))
        mid = Fraction(2 * m + 1) * Fraction(2) ** (rnd.randint(max(emin, -70), min(emax, 70)) - p)
        nd = rnd.randint(9, 15) if p == 24 else rnd.randint(16, 22)
        out += [near(mid, nd, 0), near(mid, nd, 1)]
    step = 1 if p == 24 else 9
    for e10 in list(range(-lim - 4, hi + 3, step)) + [-324, -323, -308, -307, 308, 309]:
        if p == 24 and abs(e10) > 60: continue
        out.append("1.0e%d" % e10)
        out.append("9.99999999999999999999e%d" % e10)
    for e in list(range(emin - p - 1, emin + 2)) + [-1, 0, 1, p - 1, p, p + 1, emax]:
        t = dyadic(Fraction(2) ** e)                       # powers of two and the texts beside them
        out += [t, nudge(t, 1), nudge(t, -1)]
    top = (Fraction(2) ** p - 1) * Fraction(2) ** (emax - p + 1)
    tiny = Fraction(2) ** (emin - p + 1)
    for x in (top, top + Fraction(2) ** (emax - p), tiny, tiny / 2, tiny * 3 / 2):
        t = dyadic(x)                                      # the largest value, half-way to infinity, the smallest, half of it
        out += [t, nudge(t, 1), nudge(t, -1)]
    return out

# PAST THE EXPONENT LLVM'S PARSER READS (24,000: DEF-209). The number 1.0 written both ways round the cap --
# `double <text>` read the first as 0.1 and the second as 10.0 -- and an exponent that undoes a hundred
# thousand leading zeros: 1.0 and 5.0, exactly (LLVM read both as zero; a fixed cap on the exponent's own
# digits would read them as 1e-6).
PAST_CAP = ["0." + "0" * 24000 + "1e24001", "1" + "0" * 24001 + ".0e-24001",
            "0." + "0" * 100005 + "1e100006", "0." + "0" * 100005 + "5e100006"]

FORMS = ["0.0", "0.0e10", "0.0e-10", "000.000", "0", "7", "007", "16777217", "9007199254740993",
         "0.1", "1.0", "1.5", "2.5", "0.5", "0.25", "10.0", "100.0", "3.14", "2.718281828459045",
         "1.5f32", "1.5f64", "2.5e-3f32", "2.5e-3f64", "1.0E5", "1.0E+5f64", "1.0e+5f32", "1.0E-5",
         "0." + "0" * 40 + "1e41", "1" + "0" * 30 + ".0e-30", "0.000001e6", "123456789.0e-9",
         "6.02214076e23", "1.602176634e-19", "9.51125303839185e-19", "1.0000000596046448309",
         "16777217.0", "9007199254740993.0", "0.1234567890123456789",
         "4.9e-324", "2.4703282292062327e-324", "2.4703282292062328e-324",
         "1.7976931348623157e308", "1.7976931348623158e308", "1.7976931348623159e308",
         "2.2250738585072011e-308", "2.2250738585072012e-308", "2.2250738585072014e-308",
         "3.4028234e38", "3.4028235e38", "3.4028236e38", "3.40282356e38", "3.40282357e38",
         "1.17549435e-38", "1.4e-45", "7.0e-46", "7.1e-46",
         "1.0e39", "1.0e999", "1.0e-60", "1.0e-999", "1.0e99999", "1.0e-99999", "1.0e999999999",
         "0.0e999999999", "1" + "0" * 320 + ".0", "0." + "0" * 340 + "1"] + PAST_CAP

def kat_vectors(p, emax, seed):
    """literal texts a PROGRAM may write: finite nonzero answers only (one that rounds to infinity or to zero is
    refused, D-346), at any length at either width (the 15-digit rule on a flt32 went with the same decision)"""
    emin = 1 - emax
    rnd = random.Random(seed)
    lim, hi = (44, 38) if p == 24 else (322, 308)
    maxd = 25
    out = []
    for _ in range(150):
        nd = rnd.randint(1, maxd)
        digs = str(rnd.randint(1, 9)) + "".join(str(rnd.randint(0, 9)) for _ in range(nd - 1))
        out.append(sci(digs, rnd.randint(-lim, hi - 1)))
    for _ in range(40):
        out.append("%d.%s" % (rnd.randint(0, 10 ** rnd.randint(1, 6)),
                               "".join(str(rnd.randint(0, 9)) for _ in range(rnd.randint(1, 7)))))
    for _ in range(90):                                    # DEF-203's shape, at lengths a program may write
        m = (1 << (p - 1)) + rnd.randrange(1 << (p - 1))
        mid = Fraction(2 * m + 1) * Fraction(2) ** (rnd.randint(max(emin, -70), min(emax, 70)) - p)
        # fifteen digits is where a `flt32` text sat inside half a double's unit of the midpoint (DEF-203)
        nd = (15 if rnd.random() < 0.6 else rnd.randint(10, 22)) if p == 24 else rnd.randint(16, 22)
        out += [near(mid, nd, 0), near(mid, nd, 1)]
    for _ in range(8):                                     # exact midpoints, written out (up to several hundred digits)
        m = (1 << (p - 1)) + rnd.randrange(1 << (p - 1))
        t = dyadic(Fraction(2 * m + 1) * Fraction(2) ** (rnd.randint(-60, 60) - p))
        out += [t, nudge(t, 1), nudge(t, -1)]
    for e10 in range(-lim - 1, hi + 1, 1 if p == 24 else 7):
        out.append("1.0e%d" % e10)
    if p == 24:
        out += counterexamples(40, seed + 1)
        out += ["3.4028235e38", "3.4028234e38", "1.17549435e-38", "1.4e-45", "7.1e-46", "16777217.0",
                "0.1", "0.2", "0.3", "1.0", "0.5", "9.51125303839185e-19", "7.99248255789280e-2",
                "8.89761617066066e16", "2.47979713208224e-7",
                # past fifteen digits: legal since D-346, and the nearest float (the first is one unit above 1.0)
                "1.0000000596046448309", "0.1234567890123456789", "1.0000000000000001",
                "3.40282356e38", "7.1e-46"]
    else:
        out += ["1.7976931348623157e308", "1.7976931348623158e308", "4.9e-324", "2.4703282292062328e-324",
                "2.2250738585072011e-308", "2.2250738585072012e-308", "2.2250738585072014e-308",
                "9007199254740993.0", "0.1", "0.2", "0.3", "1.0", "0.5", "3.141592653589793",
                "0.1234567890123456789012345678901234567890"] + PAST_CAP
    keep = []
    for t in out:
        if ref(t, p, emax)[1] == 0 and ref(t, p, emax)[0] != 0 and t not in keep:
            keep.append(t)
    return keep

# --- the two files -------------------------------------------------------------------

UNIT_ARMS = ["DecreasesViolated", "DivByZero", "DivOverflow", "HeapBadRequest", "HeapOom", "IntOverflow",
             "LimitViolated", "MachineFault", "OutOfBounds", "ShiftRange", "StackExhausted", "Unreachable",
             "WildLeak", "ast.BoundsId", "ast.BoundsScratch", "ast.BoundsWindow", "intern.BoundsId",
             "intern.ProvenSlice", "types.BoundsId", "types.BoundsIndex"]
KAT_ARMS = ["DecreasesViolated", "HeapBadRequest", "HeapOom", "IntOverflow", "LimitViolated", "MachineFault",
            "OutOfBounds", "StackExhausted", "Unreachable", "WildLeak"]

def failsafe(arms, code):
    out = ["func:failsafe = int32(Error:e) {", "    pick (e) {"]
    for a in arms:
        out.append("        (%s) { exit %di32; }," % (a, code))
    out += ["        (*) { exit %di32; }" % code, "    }", "    exit %di32;" % code, "};"]
    return out

def chunks(lines, per, name, ret="int32"):
    """lines of `if (...) { pass K; }` checks into functions of `per`; the function names"""
    out, names = [], []
    for c in range(0, len(lines), per):
        nm = "%s%d" % (name, c // per)
        names.append(nm)
        out.append("func:%s = %s() never fails {" % (nm, ret))
        for k, ln in enumerate(lines[c:c + per]):
            out.append("    if (%s) { pass %di32; }" % (ln, k + 1))
        out += ["    pass 0i32;", "};", ""]
    return out, names

def unit_text():
    checks = []
    for (p, emax), w, seed in ((FMT32, 32, 20320), (FMT64, 64, 20640)):
        seen = []
        for t in unit_vectors(p, emax, seed) + FORMS:
            if t in seen: continue
            seen.append(t)
            bits, flag = ref(t, p, emax)
            checks.append('!(raw ck("%s", %di32, %du64, %di32))' % (t, w, bits, flag))
    rnd = random.Random(20329)
    pats = [0, 1, 2, 0x7FFFFF, 0x800000, 0x800001, 0x7F7FFFFF, 0x7F800000, 0x3F800000, 0x00400000, 0x00000400]
    pats += [rnd.randrange(1, 1 << 23) for _ in range(24)] + [rnd.randrange(1 << 23, 0x7F800000) for _ in range(40)]
    dchecks = ["(raw flt32_bits_as_flt64(%du64)) != %du64" % (b, f32_as_f64(b)) for b in pats]
    body, names = chunks(checks, 40, "c")
    dbody, dnames = chunks(dchecks, 40, "d")
    assert len(names) + len(dnames) < 250
    out = [
        "// GENERATED by bootstrap/generator/float_vectors.py -- do not edit; `--write` regenerates it and",
        "// `--check` holds it current.",
        "//",
        "// `float_round` AND `flt32_bits_as_flt64` AGAINST THE EXACT RATIONALS (DEF-203, 1.6.1e). Each vector is a",
        "// literal's text, the format's width, the IEEE bits of the nearest value to the number written (round to",
        "// nearest, ties to even; the sign clear, a literal carries none) and the flag: 0, 1 the number rounds to",
        "// infinity, 2 a NONZERO number rounds to zero. The answers come from `fractions.Fraction`, which shares no",
        "// step with the digit-run algorithm under test. The families: random decimals over the whole range and",
        "// past it; exact midpoints between adjacent values and the texts one unit in the last place beside them",
        "// (a `flt64` midpoint runs to several hundred digits); subnormal midpoints; SHORT decimals beside a",
        "// midpoint, the shape that made `fptrunc double <text> to float` a different float (9.51125303839185e-19",
        "// was 0x218C5C80, the nearest is 0x218C5C7F); powers of ten and of two; the largest value, half-way to",
        "// infinity, the smallest subnormal and half of it; and the forms a text takes (a suffix, an integer body,",
        "// leading and trailing zeros, an exponent that undoes a long fraction, exponents past any range).",
        "//",
        "// exit N: a vector of the N-th function below failed (c0 is 1), and its `pass K` names the line.",
        "",
        "mod:float_round;",
        'use "../../src/frontend/num_width.npk".*;',
        'use "../../src/frontend/intern.npk".*;',
        'use "../../src/frontend/numeric.npk".*;',
        "",
        "func:ck = bool(string:t, int32:w, uint64:want, int32:flag) never fails {",
        "    FloatBits:b = raw float_round(t, w);",
        "    if (b.bits != want) { pass false; }",
        "    int32:got = 0i32;",
        "    if (b.inf) { got = 1i32; }",
        "    if (b.zero) { got = got + 2i32; }",
        "    pass (got == flag);",
        "};",
        "",
    ] + body + dbody + ["func:main = int32(cstring[]:_~argv) {"]
    for k, nm in enumerate(names + dnames):
        out.append("    if ((raw %s()) != 0i32) { exit %di32; }" % (nm, k + 1))
    out += ["    exit 0i32;", "};", ""] + failsafe(UNIT_ARMS, 250)
    return "\n".join(out) + "\n", len(checks), len(dchecks)

def kat_text():
    c32 = ["(raw b32(%sf32)) != %du32" % (t, ref(t, *FMT32)[0]) for t in kat_vectors(24, 127, 30320)]
    c64 = ["(raw b64(%sf64)) != %du64" % (t, ref(t, *FMT64)[0]) for t in kat_vectors(53, 1023, 30640)]
    s32, n32 = chunks(c32, 40, "s")
    s64, n64 = chunks(c64, 40, "w")
    assert len(n32) + len(n64) < 249
    one, ten = PAST_CAP[0], PAST_CAP[1]                    # the number 1.0, written past the cap both ways round
    b1 = ref(one, *FMT64)[0]
    assert b1 == ref(ten, *FMT64)[0] == 0x3FF0000000000000
    neg = "(%du64 | (1u64 << 63u64))" % b1                 # a `uint64` past 2^63 is built, not written (D-311)
    roads = [
        "// THE ROADS a constant's text takes to the emitter's one writer besides a literal in a body, each with a",
        "// text past the cap: a module binding, one under a sign, an aggregate's element, a literal with no suffix",
        "// in a `flt64` slot, `comptime(...)` and `comptime(-...)`.",
        "fixed flt64:M_ONE = %sf64;" % one,
        "fixed flt64:M_NEG = -%sf64;" % ten,
        "fixed flt64[2]:M_ARR = [0.1f64, %sf64];" % one,
        "",
        "func:roads = int32() never fails {",
        "    if ((raw b64(M_ONE)) != %du64) { pass 1i32; }" % b1,
        "    if ((raw b64(M_NEG)) != %s) { pass 2i32; }" % neg,
        "    if ((raw b64(M_ARR[1])) != %du64) { pass 3i32; }" % b1,
        "    flt64:u = %s;" % ten,
        "    if ((raw b64(u)) != %du64) { pass 4i32; }" % b1,
        "    flt64:c = comptime(%sf64);" % one,
        "    if ((raw b64(c)) != %du64) { pass 5i32; }" % b1,
        "    flt64:cn = comptime(-%sf64);" % ten,
        "    if ((raw b64(cn)) != %s) { pass 6i32; }" % neg,
        "    pass 0i32;",
        "};",
        "",
    ]
    out = [
        "// expect-exit: 0",
        "//",
        "// GENERATED by bootstrap/generator/float_vectors.py -- do not edit; `--write` regenerates it and",
        "// `--check` holds it current.",
        "//",
        "// A FLOAT LITERAL IS THE NEAREST VALUE OF ITS TYPE TO THE NUMBER WRITTEN (DEF-203, DEF-209; 1.6.1e): each",
        "// literal below is compiled, handed through a call, read back as its bits and compared with the answer the",
        "// exact rationals give (`fractions.Fraction`: round to nearest, ties to even). A literal of either width is",
        "// the compiler's own constant -- the bits `float_round` makes of its text. A `flt32` was `fptrunc double",
        "// <text> to float` until DEF-203, LLVM's rounding to 53 bits and then the instruction's to 24: a different",
        "// float for the texts of the `s` functions' near-midpoint family. A `flt64` was `double <text>`, converted",
        "// by LLVM's decimal parser, until DEF-209: right for the `w` functions' exact midpoints written out to",
        "// several hundred digits and the texts beside them, the decimals of 16 to 22 digits beside a midpoint, the",
        "// subnormal boundary and the largest value -- and WRONG for the four texts that end the `w` family, whose",
        "// exponent is past the 24,000 that parser reads: the number 1.0 was the double 0.1, or 10.0, or zero (two",
        "// of the four are a hundred thousand digits long, and are one line each). Only literals whose answer is",
        "// finite and nonzero: one that rounds to infinity, or a nonzero one that rounds to zero, is refused",
        "// (NITPICK-TYPE-031, D-346: `tests/types/rejection/float_fit.npk`). A literal's length is free at both",
        "// widths (the 15-digit rule on a `flt32` went with the same decision): the `s` family runs to 25 digits",
        "// and to exact midpoints written out.",
        "//",
        "// exit N: a literal of the N-th function below is not the nearest value (s0 is 1); `pass K` names the line.",
        "// `roads`, the last, holds the past-the-cap text on every other road to the constant's writer.",
        "mod:float_literal_kat;",
        "",
        "func:b32 = uint32(flt32:v) never fails {",
        "    wild int8->:b = alloc(4i64);",
        "    wild flt32->:fp = b =>! wild flt32->;",
        "    <-fp = v;",
        "    wild uint32->:ip = b =>! wild uint32->;",
        "    uint32:w = <-ip;",
        "    dalloc(b);",
        "    pass w;",
        "};",
        "func:b64 = uint64(flt64:v) never fails {",
        "    wild int8->:b = alloc(8i64);",
        "    wild flt64->:fp = b =>! wild flt64->;",
        "    <-fp = v;",
        "    wild uint64->:ip = b =>! wild uint64->;",
        "    uint64:w = <-ip;",
        "    dalloc(b);",
        "    pass w;",
        "};",
        "",
    ] + s32 + s64 + roads + ["func:main = int32(cstring[]:_~argv) {"]
    for k, nm in enumerate(n32 + n64 + ["roads"]):
        out.append("    if ((raw %s()) != 0i32) { exit %di32; }" % (nm, k + 1))
    out += ["    exit 0i32;", "};", ""] + failsafe(KAT_ARMS, 250)
    return "\n".join(out) + "\n", len(c32), len(c64)

# --- the measurement of LLVM's parser ------------------------------------------------

def llvm_form(t):
    b = body_of(t)
    if "." not in b:
        i = len(b)
        for k, c in enumerate(b):
            if c in "eE":
                i = k
                break
        b = b[:i] + ".0" + b[i:]
    return b

def measure_llvm():
    seen = set()
    texts = []
    pool = FORMS + kat_vectors(53, 1023, 30640)
    for k in range(6):                         # the committed vectors (k = 0) and five more draws of each family
        pool += unit_vectors(53, 1023, 20640 + k) + unit_vectors(24, 127, 20320 + k)
    for t in pool:
        if t not in seen:
            seen.add(t); texts.append(t)
    d = tempfile.mkdtemp(prefix="float_vectors_")
    ll, ob, bn = (os.path.join(d, "v" + x) for x in (".ll", ".o", ".bin"))
    with open(ll, "w") as f:
        f.write("@a = constant [%d x double] [\n%s\n]\n" % (len(texts), ",\n".join("  double " + llvm_form(t) for t in texts)))
    r = subprocess.run(["llc", "-O0", "-filetype=obj", ll, "-o", ob], capture_output=True, text=True)
    if r.returncode != 0:
        print("llc refused the module: " + r.stderr[:400])
        return 1
    subprocess.run(["llvm-objcopy", "-O", "binary", "--only-section=.rodata", ob, bn], check=True)
    raw = open(bn, "rb").read()
    assert len(raw) == 8 * len(texts)
    bad = 0
    capped = [0, 0]
    for k, t in enumerate(texts):
        got = struct.unpack_from("<Q", raw, 8 * k)[0]
        want = ref(t, *FMT64)[0]
        ex = body_of(t).lower().partition("e")[2]
        past = abs(int(ex)) > 24000 if ex else False        # LLVM's parser caps the exponent it reads (DEF-209)
        if past:
            capped[0] += 1
            capped[1] += got != want
        elif got != want:
            bad += 1
            if bad <= 8: print("LLVM %016X, exact %016X: %s" % (got, want, t[:80]))
    print("%d texts as `double <text>` with an exponent within 24,000: %d differ from the nearest double"
          % (len(texts) - capped[0], bad))
    print("%d texts with an exponent past 24,000: %d differ (LLVM caps the exponent: DEF-209)" % tuple(capped))
    return 1 if bad else 0

def main():
    mode = sys.argv[1] if len(sys.argv) == 2 else ""
    if mode == "--llvm":
        return measure_llvm()
    if mode not in ("--write", "--check"):
        print(__doc__)
        return 2
    ut, nu, nd = unit_text()
    kt, k32, k64 = kat_text()
    if mode == "--write":
        open(UNIT, "w").write(ut)
        open(KAT, "w").write(kt)
        print("float_round.npk: %d vectors and %d bit patterns; float_literal_kat.npk: %d flt32 and %d flt64 literals"
              % (nu, nd, k32, k64))
        return 0
    stale = [p for p, t in ((UNIT, ut), (KAT, kt)) if not os.path.exists(p) or open(p).read() != t]
    for p in stale:
        print("stale: %s (run float_vectors.py --write)" % os.path.relpath(p, ROOT))
    return 1 if stale else 0

if __name__ == "__main__":
    sys.exit(main())
