#!/usr/bin/env python3
"""dropnorm.py -- read an emission comparison whose only expected difference is a scope-exit
drop's flag test REMOVED (DEF-159, 1.6.1e landing 105): for an address-taken owning binding the
emitter writes `call void @"npk.drop.<tid>"(ptr %slot)` where it wrote

    %a = load i8, ptr %flag
    %b = icmp ne i8 %a, 0
    br i1 %b, label %drop.goN, label %drop.doneM
  drop.goN:
    call void @"npk.drop.<tid>"(ptr %slot)
    br label %drop.doneM
  drop.doneM:

and every later temporary and label of the function renumbers. This tool renumbers `%tN`
temporaries and the emitter's `name<N>` labels by first appearance within each `define`, then
strips every drop's flag test from BOTH texts (the seven lines above become the kept `call`
line), renumbers `%tN` temporaries and the emitter's `name<N>` labels by first appearance
within each `define`, and classifies each pair: `same` (the raw texts agree),
`flag-tests-only` (the stripped, renumbered texts agree -- the two counts say how many flag
tests each text held, and base minus new is the number of drops made unconditional), or
`DIFFERENT` (anything else -- READ it). Usage: dropnorm.py A.base.ll A.new.ll [B.base.ll B.new.ll ...]
Prints one line per pair and a DROPNORM summary line. Outside every gate: a measurement."""
import re, sys, difflib

TEMP = re.compile(r"%t(\d+)\b")
LABEL_DEF = re.compile(r"^([A-Za-z_.][A-Za-z0-9_.]*?)(\d+):$")
LABEL_REF = re.compile(r"%([A-Za-z_.][A-Za-z0-9_.]*?)(\d+)\b")

def canon(text):
    out = []; tmap = {}; lmap = {}
    def tsub(m):
        k = m.group(1)
        if k not in tmap: tmap[k] = str(len(tmap))
        return "%t" + tmap[k]
    def lkey(name, num):
        k = name + num
        if k not in lmap: lmap[k] = str(len(lmap))
        return name + "L" + lmap[k]
    for ln in text.split("\n"):
        if ln.startswith("define "):
            tmap = {}; lmap = {}
        m = LABEL_DEF.match(ln)
        if m:
            out.append(lkey(m.group(1), m.group(2)) + ":"); continue
        ln = TEMP.sub(tsub, ln)
        ln = LABEL_REF.sub(lambda m: "%" + lkey(m.group(1), m.group(2)) if m.group(1) != "t" else m.group(0), ln)
        out.append(ln)
    return out

LOAD = re.compile(r"^\s+(%t\d+) = load i8, ptr (%t\d+)$")
ICMP = re.compile(r"^\s+(%t\d+) = icmp ne i8 (%t\d+), 0$")
BR = re.compile(r"^\s+br i1 (%t\d+), label %(drop\.go\d+), label %(drop\.done\d+)$")
CALL = re.compile(r'^\s+call void @"npk\.drop\.\d+"\(ptr %t\d+\)$')
BRL = re.compile(r"^\s+br label %(drop\.done\d+)$")

def strip(text):
    """Remove every scope-exit drop's FLAG TEST, keeping its drop call: the seven-line shape
    above becomes the one `call` line. Returns (lines, how many tests were stripped)."""
    lines = text.split("\n"); out = []; i = 0; n = 0
    while i < len(lines):
        if i + 6 < len(lines):
            m1 = LOAD.match(lines[i]); m2 = ICMP.match(lines[i+1]); m3 = BR.match(lines[i+2])
            if m1 and m2 and m3 and m2.group(2) == m1.group(1) and m3.group(1) == m2.group(1) \
               and lines[i+3] == m3.group(2) + ":" and CALL.match(lines[i+4]) \
               and BRL.match(lines[i+5]) and BRL.match(lines[i+5]).group(1) == m3.group(3) \
               and lines[i+6] == m3.group(3) + ":":
                out.append(lines[i+4]); i += 7; n += 1; continue
        out.append(lines[i]); i += 1
    return out, n

def classify(base, new):
    """'same' when the raw texts agree; 'flag-tests-only' when the texts agree once every
    drop's flag test is stripped from BOTH and renumbered (the counts say how many tests each
    held: base minus new is the number of drops made unconditional); 'DIFFERENT' otherwise."""
    if base == new: return "same", 0, 0
    sb, nb = strip(base); sn, nn = strip(new)
    if canon("\n".join(sb)) == canon("\n".join(sn)): return "flag-tests-only", nb, nn
    return "DIFFERENT", nb, nn

def main(argv):
    pairs = list(zip(argv[0::2], argv[1::2]))
    counts = {"same": 0, "flag-tests-only": 0, "DIFFERENT": 0}
    for bp, np_ in pairs:
        base = open(bp).read(); new = open(np_).read()
        kind, nb, nn = classify(base, new)
        counts[kind] += 1
        extra = "" if kind == "same" else " (flag tests: base %d, new %d)" % (nb, nn)
        print("%s %s%s" % (kind, bp, extra))
    print("DROPNORM: %d same, %d flag-tests-only, %d DIFFERENT" % (counts["same"], counts["flag-tests-only"], counts["DIFFERENT"]))
    return 0 if counts["DIFFERENT"] == 0 else 1

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
