"""D-309's residue report over the compiler's own manifest, by function and by
shape, with each row's source line -- the `overflow` rows since 1.5.8b step 3,
and since 1.5.8c step 5 the `terminate` rows (a loop's measure, or a recursive
call's) and the `stack-depth` rows (one per cyclic group, derived). Reads
build/verify/obl (rows.txt, index.txt, the smt2 comments' line:col) under ROOT
and ROOT's committed manifest for the verdicts. A measurement outside every
gate (P-8): it classifies by the TEXT of the site, never by a type.

    python3 residue.py ROOT OUT.txt      # OUT.txt: every open row, one per line
"""
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
# the root's own symbols carry no module qualifier (`@main`): they are src/npkc.npk's
for rootsym in ("main", "failsafe"):
    files.setdefault(rootsym, os.path.join(root, "src", "npkc.npk"))
src_cache = {}
def lines_of(mod):
    p = files.get(mod)
    if not p: return None
    if p not in src_cache: src_cache[p] = open(p, encoding="utf-8").read().split("\n")
    return src_cache[p]
def line_of(mod, ln):
    s = lines_of(mod)
    if s is None: return None
    return s[ln - 1] if 0 < ln <= len(s) else None
def module_of(sym):
    m = re.match(r'@"npk\.([^.]+)\.', sym)
    return m.group(1) if m else sym.lstrip("@").split(".")[0]

# ---- overflow: the shape of the operation at the row's column (1.5.8b step 3) ----
def shape(sym, text, col):
    if sym.startswith('@"npk.prelude.'): return "prelude numeric core"
    if text is None or col - 1 >= len(text): return "?"
    i = col - 1
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

# ---- terminate: the shape of the loop's MEASURE, read off the loop head (1.5.8c step 5) ----
MEASURE_RE = re.compile(r"\bdecreases\s+(.*?)\s*(?:\binvariant\b|\{|$)")
def measure_text(mod, ln):
    """The `decreases E` text of the loop head whose statement is at line `ln` (the
    ENTRY row's site; a preservation row and a `continue` row find their head through
    the row's group). A head may span lines, the clause on a later one than the
    `while`, so the lines just below are read as well as the line itself."""
    s = lines_of(mod)
    if s is None: return None
    for k in list(range(ln, min(ln + 4, len(s)) + 1)) + list(range(ln - 1, max(ln - 3, 0), -1)):
        if 0 < k <= len(s):
            m = MEASURE_RE.search(s[k - 1])
            if m: return m.group(1).strip()
            if re.search(r"\bunbounded\b", s[k - 1]): return None
    return None
FUNC_RE = re.compile(r"^\s*(?:pub\s+)?func:")
def root_kind(mod, ln, root):
    """How the measure's root name `root` is declared in the enclosing function, read
    off the text between the function header and the loop: `T->:root` (or a `->` inside
    the type) is THROUGH A POINTER, `T:root` BY VALUE; unknown otherwise."""
    s = lines_of(mod)
    if s is None: return "?"
    for k in range(ln, 0, -1):
        t = s[k - 1]
        if re.search(r"[A-Za-z_0-9>\]]\s*->\s*:\s*%s\b" % re.escape(root), t): return "pointer"
        if re.search(r"(?<![-])[A-Za-z_0-9>\]]\s*:\s*%s\b(?!\s*\()" % re.escape(root), t): return "value"
        if FUNC_RE.match(t): break
    return "?"
CAST_RE = re.compile(r"\(\s*([^()]*?)\s*=>\s*[A-Za-z_0-9]+\s*\)")
def mshape(E, mod=None, ln=0):
    if E is None: return "? (no `decreases` found at the site)"
    # a widening `=>` is the operand's own term to the encoder (D-095's lossless rule),
    # so the cast is stripped and the shape is the operand's, marked [widened]
    widened = "=>" in E
    while True:
        E2 = CAST_RE.sub(r"\1", E)
        if E2 == E: break
        E = E2
    E = re.sub(r"\(\(([^()]*)\)\)", r"(\1)", E)
    tag = " [widened]" if widened else ""
    if re.search(r"\braw\s+[A-Za-z_][A-Za-z_0-9]*\s*\(", E): return "a pure call in the measure (`raw f(...) - v`)" + tag
    if E.count("/") >= 1: return "a quotient (the numeric cores)" + tag
    if re.fullmatch(r"\(?[A-Za-z_][A-Za-z_0-9]*\)?", E): return "the operand itself (`while (v > 0) decreases v`)" + tag
    fields = len(re.findall(r"[A-Za-z_][A-Za-z_0-9]*(?:\.[A-Za-z_][A-Za-z_0-9]*)+", E))
    names = len(re.findall(r"(?<![.\w])[A-Za-z_][A-Za-z_0-9]*(?![.\w(])", E))
    if fields >= 1:
        roots = sorted(set(re.findall(r"(?<![.\w])([A-Za-z_][A-Za-z_0-9]*)\.[A-Za-z_]", E)))
        kinds = sorted(set(root_kind(mod, ln, r) for r in roots)) if mod else ["?"]
        via = "through a POINTER" if kinds == ["pointer"] else ("of a BY-VALUE aggregate" if kinds == ["value"] else "of a root declared how? (%s)" % "/".join(kinds))
        if fields >= 2: return "two field reads %s (a scan's bytes left: `x.len - x.pos`)" % via + tag
        return "a field read %s against a local (`x.count - c`)" % via + tag
    if names >= 2: return "two locals (`bound - v`)" + tag
    return "other" + tag

allrows = [l.rstrip("\n").split("\t") for l in open(os.path.join(obl, "rows.txt"), encoding="utf-8")]
# a loop's head line: the site of the row recorded AT the loop statement (`1:<group>`), so a
# preservation row (at the body block) and a `continue` row (at the `continue`) find the head
head_line = {}
for f in allrows:
    if f[2] == "terminate" and f[6] == "1:" + f[8]:
        head_line[(f[0], f[8])] = loc.get((f[0], f[1]), (0, 0))[0]
orows = []; trows = []; drows = []
for f in allrows:
    kind = f[2]
    if kind not in ("overflow", "terminate", "stack-depth"): continue
    v = man.get((f[3], kind, f[5]), "?")
    ln, col = loc.get((f[0], f[1]), (0, 0))
    mod = module_of(f[5])
    if kind == "overflow":
        text = line_of(mod, ln)
        orows.append((f[5], v, ln, col, mod, text, shape(f[5], text, col)))
    elif kind == "terminate":
        if f[6].startswith("0:"):
            E = "(a recursive call)"; sh = "a recursive call's row (the callee's measure below the caller's)"
        else:
            hl = head_line.get((f[0], f[8]), ln)
            E = measure_text(mod, hl); sh = mshape(E, mod, hl)
        trows.append((f[5], v, ln, col, mod, E, sh, f[11]))
    else:
        drows.append((f[5], v, ln, mod))

def report(title, rows, shape_ix, verdict_ix=1, mod_ix=4, fn_ix=0):
    tot = collections.Counter(r[verdict_ix] for r in rows)
    print("%s rows: %d  %s" % (title, len(rows), dict(sorted(tot.items()))))
    opn = [r for r in rows if r[verdict_ix] in ("open", "budget", "unencoded")]
    print("\n%s residue by shape (open, budget and unencoded):" % title)
    for k, n in collections.Counter(r[shape_ix] for r in opn).most_common(): print("  %5d  %s" % (n, k))
    print("\n%s residue by module:" % title)
    for k, n in collections.Counter(r[mod_ix] for r in opn).most_common(25): print("  %5d  %s" % (n, k))
    print("\n%s residue by function (top 30):" % title)
    for k, n in collections.Counter(r[fn_ix] for r in opn).most_common(30): print("  %5d  %s" % (n, k))
    print("\n%s discharged by shape:" % title)
    for k, n in collections.Counter(r[shape_ix] for r in rows if r[verdict_ix] == "discharged").most_common(): print("  %5d  %s" % (n, k))
    print()
    return opn

o_open = report("overflow", orows, 6)
t_open = report("terminate", trows, 6)
# the terminate rows by context (DEF-81: one row per visit context of the head's evaluation is
# the OVERFLOW rows' rule; a terminate row's own contexts are the entry, the preservation and each
# `continue`) -- printed by shape AND verdict, since the same measure is open at one and discharged
# at another
print("terminate: shape x verdict")
for (sh, v), n in sorted(collections.Counter((r[6], r[1]) for r in trows).items()): print("  %5d  %-70s %s" % (n, sh, v))

# ---- stack-depth: one row per cyclic group; the groups and their sizes from index.txt ----
groups = collections.defaultdict(list); measured = set()
for l in open(os.path.join(obl, "index.txt"), encoding="utf-8"):
    f = l.rstrip("\n").split("\t")
    if len(f) != 5: raise SystemExit("index.txt: five fields expected, got %d: %r" % (len(f), l))
    if f[3] != "0": groups[f[3]].append(f[1])
    if f[4] != "0": measured.add(f[3])
print("\nstack-depth rows: %d  %s" % (len(drows), dict(sorted(collections.Counter(r[1] for r in drows).items()))))
print("cyclic groups in index.txt: %d over %d function files; measured groups: %d"
      % (len(groups), sum(len(v) for v in groups.values()), len(measured)))
print("group sizes (members with a file in this emission -> groups):")
for sz, n in sorted(collections.Counter(len(v) for v in groups.values()).items()): print("  %3d members: %d group(s)" % (sz, n))
print("the largest groups:")
for g, mem in sorted(groups.items(), key=lambda kv: -len(kv[1]))[:8]:
    print("  %3d  %s ..." % (len(mem), ", ".join(m.strip('@"') for m in mem[:4])))
print("stack-depth rows by module:")
for k, n in collections.Counter(r[3] for r in drows).most_common(12): print("  %5d  %s" % (n, k))

with open(sys.argv[2], "w", encoding="utf-8") as fh:
    for r in sorted(o_open, key=lambda r: (r[4], r[2], r[3])):
        fh.write("overflow\t%s\t%s\t%d:%d\t%s\t%s\n" % (r[1], r[0], r[2], r[3], r[6], (r[5] or "?").strip()[:140]))
    for r in sorted(t_open, key=lambda r: (r[4], r[2], r[3])):
        fh.write("terminate\t%s\t%s\t%d:%d\t%s\t%s\n" % (r[1], r[0], r[2], r[3], r[6], (r[5] or "?").strip()[:140]))
    for r in sorted(drows, key=lambda r: (r[3], r[2])):
        fh.write("stack-depth\t%s\t%s\t%d\n" % (r[1], r[0], r[2]))
