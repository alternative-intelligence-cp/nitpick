#!/usr/bin/env python3
"""1.6.0 step 2: apply a control's PLANT to its emission (1.6.0.md §2.4, P-9). A .plant file names the program,
the exits the plain and the planted binaries must answer, and old:/new: blocks -- each old block must occur
EXACTLY ONCE in the emission (the explorer's .ctl rule, VERIFICATION_REFERENCE §10); the tool refuses otherwise.
usage: plant.py FILE.plant EMISSION.ll OUT.ll   -> prints the expected exits as `plain N planted M`"""
import sys

def parse(path):
    meta, pairs, mode, cur = {}, [], None, None
    for line in open(path).read().split("\n"):
        if line.startswith(";"): continue
        if line.startswith("old:"): mode = "old"; cur = [[], []]; pairs.append(cur); continue
        if line.startswith("new:"): mode = "new"; continue
        if line and not line.startswith(" ") and ":" in line and mode is None:
            k, v = line.split(":", 1); meta[k.strip()] = v.strip(); continue
        if mode == "old" and line != "": cur[0].append(line)
        elif mode == "new" and line != "": cur[1].append(line)
        elif line == "" and mode == "new": mode = None
    return meta, pairs

def main():
    plant, emission, out = sys.argv[1:4]
    meta, pairs = parse(plant)
    text = open(emission).read()
    for old, new in pairs:
        o = "\n".join(old) + "\n"; n = ("\n".join(new) + "\n") if new else ""
        c = text.count(o)
        if c != 1:
            sys.exit("plant.py: %s: an old block occurs %d times, not once:\n%s" % (plant, c, o))
        text = text.replace(o, n)
    open(out, "w").write(text)
    print("plain %s planted %s" % (meta.get("expect-plain"), meta.get("expect-planted")))

if __name__ == "__main__":
    main()
