#!/usr/bin/env python3
# tidnorm.py BASE.ll NEW.ll [more pairs...] -- compare two emissions with their TYPE-ID-BEARING names renumbered by order of
# first appearance (`@"npk.drop.<tid>"`, `@"npk.vacant.<tid>"`: the per-type generated bodies, named by the interned type id,
# which moves for every program when a compiler interns one more type ahead of the program's own -- D-348 (ii) interned the
# prelude's `fixed uint8[]`). The twin of sitenorm.py (the site table). Prints `same`/`DIFFERENT` per pair and a summary; a
# DIFFERENT pair is read (diff the renumbered texts). A measurement, outside every gate.
import re, sys
FAM = re.compile(r'npk\.(drop|vacant)\.(\d+)')
def norm(text):
    seen = {}
    def rep(m):
        key = (m.group(1), m.group(2))
        if key not in seen: seen[key] = len(seen) + 1
        return 'npk.%s.#%d' % (m.group(1), seen[key])
    return FAM.sub(rep, text)
args = sys.argv[1:]
if len(args) % 2 or not args: sys.exit("usage: tidnorm.py BASE.ll NEW.ll [BASE2.ll NEW2.ll ...]")
same = diff = skipped = 0
for i in range(0, len(args), 2):
    a, b = open(args[i]).read(), open(args[i+1]).read()
    # a pair with an EMPTY side is a program one compiler refused (the comparison scripts keep the file the other
    # wrote): not a pair of emissions, and said so rather than counted as a difference
    if not a.strip() or not b.strip(): skipped += 1; print("one side empty (refused there): %s" % args[i+1]); continue
    if norm(a) == norm(b): same += 1; print("same      %s" % args[i+1])
    else:
        diff += 1; print("DIFFERENT %s" % args[i+1])
        open(args[i] + ".norm", "w").write(norm(a)); open(args[i+1] + ".norm", "w").write(norm(b))
print("TIDNORM: %d same, %d DIFFERENT, %d skipped (one side empty) (renumbered %s)" % (same, diff, skipped, ", ".join("npk.%s.N" % f for f in ("drop", "vacant"))))
