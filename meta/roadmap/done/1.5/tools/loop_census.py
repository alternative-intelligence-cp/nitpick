"""1.5.8c planning: every `while` / `when` loop in the tree, classified by the SHAPE of
its condition and of the writes to the condition's variables inside its body --
what an obvious `decreases` measure would be, and which loops need reading."""
import os, re, sys, glob, collections
ROOT = "/home/randy/Workspace/REPOS/nitpick"
files = []
for d in ["src", "lib", "npkg", "tools", "tests"]:
    files += glob.glob(os.path.join(ROOT, d, "**", "*.npk"), recursive=True)
files = [f for f in files if "prelude_source" not in f]
shape = collections.Counter(); per_dir = collections.Counter(); examples = collections.defaultdict(list)
total = 0
def body_of(lines, i):
    # from the line holding the `{` that opens the loop, collect until the matching `}`
    depth = 0; out = []; started = False
    for j in range(i, min(i + 400, len(lines))):
        l = lines[j]
        for ch in l:
            if ch == "{": depth += 1; started = True
            elif ch == "}": depth -= 1
        out.append(l)
        if started and depth <= 0: break
    return out
for f in files:
    try: lines = open(f, encoding="utf-8").read().split("\n")
    except Exception: continue
    for i, l in enumerate(lines):
        s = l.strip()
        if s.startswith("//"): continue
        m = re.match(r"^(?:.*\b)?(while|when)\s*\((.*)\)\s*(?:invariant\b.*)?\{?\s*$", s)
        if not m: continue
        kind, cond = m.group(1), m.group(2).strip()
        total += 1
        d = os.path.relpath(f, ROOT).split("/")[0]; per_dir[d] += 1
        body = "\n".join(body_of(lines, i))
        # classify
        c = None
        mm = re.match(r"^\(?\s*([A-Za-z_][A-Za-z0-9_.]*)\s*(<|<=|>|>=|!=)\s*(.+?)\s*\)?$", cond)
        if cond in ("true",):
            c = "while(true) with break" if "break" in body else "while(true), no break"
        elif mm:
            v, op, rhs = mm.group(1), mm.group(2), mm.group(3)
            inc = re.search(r"\b%s\s*=\s*%s\s*\+\s*1[iu]\d*;" % (re.escape(v), re.escape(v)), body) or re.search(r"\b%s\s*\+=\s*1" % re.escape(v), body)
            dec = re.search(r"\b%s\s*=\s*%s\s*-\s*1[iu]\d*;" % (re.escape(v), re.escape(v)), body) or re.search(r"\b%s\s*-=\s*1" % re.escape(v), body)
            anyw = re.search(r"\b%s\s*(=|\+=|-=)" % re.escape(v), body)
            if op in ("<", "<=") and inc and not dec: c = "counter up: %s %s bound, +1 in body -> decreases bound - %s" % ("v", op, "v")
            elif op in (">", ">=") and dec and not inc: c = "counter down: v %s bound, -1 in body -> decreases v - bound" % op
            elif op == "!=" and (inc or dec): c = "counter != bound, +-1 in body"
            elif anyw: c = "condition var written in body, not a +-1 counter (READ)"
            else: c = "condition var NOT written in body (a call moves it, or a pointer) (READ)"
        elif re.match(r"^\(?\s*!?\s*[A-Za-z_][A-Za-z0-9_.]*\s*\)?$", cond) or re.match(r"^\(?[A-Za-z_][A-Za-z0-9_.]*\(.*\)\)?$", cond):
            c = "boolean flag or call as condition (READ)"
        else:
            c = "compound condition (READ)"
        shape[(kind, c)] += 1
        if len(examples[(kind, c)]) < 2: examples[(kind, c)].append("%s:%d: %s" % (os.path.relpath(f, ROOT), i + 1, s[:90]))
print("loops:", total, "| by dir:", dict(per_dir))
for k, n in sorted(shape.items(), key=lambda kv: -kv[1]):
    print("%5d  %s %s" % (n, k[0], k[1]))
    for e in examples[k]: print("         e.g. " + e)
