"""D-309's residue report: every `overflow` row of the compiler's own manifest,
by function and by shape, with its source line. Reads build/verify/obl (rows.txt,
the smt2 comments' line:col) and the committed-to-be manifest for verdicts."""
import os, re, sys, collections
root = sys.argv[1]
obl = os.path.join(root, "build", "verify", "obl")
man = {}
for l in open(os.path.join(root, "nitpick.obligations"), encoding="utf-8"):
    if l.startswith("#") or not l.strip(): continue
    h, kind, tier, v, word, sym = l.rstrip("\n").split(" ", 5)
    man[(h, kind, sym)] = v
loc = {}
for fn in os.listdir(obl):
    if not fn.endswith(".smt2") or ".t2." in fn: continue
    for l in open(os.path.join(obl, fn), encoding="utf-8"):
        m = re.match(r"^; o(\d+) (\S+) ([0-9a-f]{64}) (\d+):(\d+)", l)
        if m: loc[(fn[:4], m.group(1))] = (int(m.group(4)), int(m.group(5)))
        elif not l.startswith(";"): break
files = {}
for base in ("src", "lib"):
    for dp, dn, fns in os.walk(os.path.join(root, base)):
        for f in fns:
            if f.endswith(".npk"): files.setdefault(f[:-4], os.path.join(dp, f))
src_cache = {}
def line_of(mod, ln):
    p = files.get(mod)
    if not p: return None
    if p not in src_cache: src_cache[p] = open(p, encoding="utf-8").read().split("\n")
    s = src_cache[p]
    return s[ln - 1] if 0 < ln <= len(s) else None
def shape(sym, text, col):
    if sym.startswith('@"npk.prelude.'): return "prelude numeric core"
    if text is None: return "?"
    i = col - 1
    op = text[i:i+2]
    if text[i:i+2] in ("+=", "-=", "*="): return "compound"
    right = text[i+1:].lstrip()
    left = text[:i].rstrip()
    rtok = re.match(r"[A-Za-z_][A-Za-z_0-9.]*|\d[0-9a-z]*|\(", right)
    rtok = rtok.group(0) if rtok else ""
    ltok = re.search(r"([A-Za-z_][A-Za-z_0-9.\[\]]*|\)|\d[0-9a-z]*)$", left)
    ltok = ltok.group(1) if ltok else ""
    o = text[i]
    rlit = bool(re.match(r"\d", rtok))
    if o == "-" and (ltok == "" or left.endswith(("(", "=", ",", "return", "pass"))): return "negation"
    if o == "*":
        return "doubling or scaling by a constant" if rlit else "product of two unknowns"
    if rlit:
        if "." in ltok or ltok.endswith("]") or ltok == ")": return "field, element or call +/- a constant"
        return "name +/- a constant (counter, index, offset)"
    return "sum or difference of two unknowns"
rows = []
for l in open(os.path.join(obl, "rows.txt"), encoding="utf-8"):
    f = l.rstrip("\n").split("\t")
    if f[2] != "overflow": continue
    v = man.get((f[3], f[2], f[5]), "?")
    ln, col = loc.get((f[0], f[1]), (0, 0))
    m = re.match(r'@"npk\.([^.]+)\.', f[5])
    mod = m.group(1) if m else f[5].lstrip("@").split(".")[0]
    text = line_of(mod, ln)
    rows.append((f[5], v, ln, col, mod, text, shape(f[5], text, col)))
tot = collections.Counter(r[1] for r in rows)
print("overflow rows: %d  %s" % (len(rows), dict(tot)))
opn = [r for r in rows if r[1] in ("open", "budget")]
print("\nresidue by shape (open and budget):")
for k, n in collections.Counter(r[6] for r in opn).most_common(): print("  %5d  %s" % (n, k))
print("\nresidue by module:")
for k, n in collections.Counter(r[4] for r in opn).most_common(25): print("  %5d  %s" % (n, k))
print("\nresidue by function (top 30):")
for k, n in collections.Counter(r[0] for r in opn).most_common(30): print("  %5d  %s" % (n, k))
print("\ndischarged by shape:")
for k, n in collections.Counter(r[6] for r in rows if r[1] == "discharged").most_common(): print("  %5d  %s" % (n, k))
with open(sys.argv[2], "w", encoding="utf-8") as fh:
    for r in sorted(opn, key=lambda r: (r[4], r[2], r[3])):
        fh.write("%s\t%s\t%d:%d\t%s\t%s\n" % (r[1], r[0], r[2], r[3], r[6], (r[5] or "?").strip()[:140]))
