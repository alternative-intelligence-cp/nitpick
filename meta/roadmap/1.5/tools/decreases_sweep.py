#!/usr/bin/env python3
"""1.5.8c step 3 -- THE SWEEP (D-304 (6), P-3): every `while` / `when` loop of the tree
states `decreases E` or `unbounded`. This tool writes ONLY the shape it can PROVE
monotone and lists every other loop for a reader.

It drives the parser-driven loop dump (`loop_dump.npk`, beside this file), never a
regex over the line: the dump gives each loop's clause, the exact position of its
condition and body, and the names the body WRITES as the AST shows them
(`=x` assigned, `@x` address taken / `$$i` / `$$m` / a method receiver, `~x` moved),
with a nested loop's writes joined into its enclosing loop's.

THE SHAPE IT WRITES -- a counter against a bound nothing in the body touches:

    while (v < bound)  ...  { ... v = v + 1<sfx>; ... }   ->  decreases bound - v
    while (v <= bound) ...                                ->  decreases bound - v
    while (v > bound)  ...  { ... v = v - 1<sfx>; ... }   ->  decreases v - bound
    while (v >= bound) ...                                ->  decreases v - bound

where `v` is a plain identifier, EVERY write to `v` in the body is that one
form (`v = v +/- 1<sfx>;` or `v +=/-= 1<sfx>;` -- the dump's `=v` count equals
the count of those textual forms, and the dump shows no `@v` and no `~v`), and
`bound` is STABLE: an expression over integer literals, `+ - *`, parentheses,
plain identifiers, `x.len` / `x.count` of a plain identifier, and a widening
`(… => intN)` of one -- each identifier a name the body writes nowhere (no `=`,
`@` or `~` entry: a `push` through `@l` and a method call on `l` both show as
`@l`), whose address is taken nowhere in the ENCLOSING FUNCTION (`@x`, `$$m x`,
`$$i x` absent from its text), declared there without a pointer type and at the
counter's width (a `.len`/`.count` is `int64`, so the counter must be). The
condition may be a top-level `&&`-conjunction with exactly one such comparison
among its conjuncts: the other conjuncts only add exits, and the measure still
shrinks on every trip -- a `||` would not (the body may run with `v >= bound`,
and the entry check would trap). Everything else -- a call as the bound, a flag,
a `while (true)`, a counter written by anything but its unit step -- is LISTED,
each with its class, for the reader to give a measure or `unbounded` with a
reason (D-316).

Usage:
    decreases_sweep.py DUMP_BINARY [--write] [--report FILE] [DIR ...]

Without `--write` it is a dry run: the counts and the list. With it, the clauses
are inserted after the condition's `)` (before `invariant` or `{`), from the end
of each file backwards so the dump's offsets stay valid, and the file is
rewritten. Files the dump cannot parse are listed and untouched. Every loop that
already carries a clause is left alone.
"""
import os, re, sys, subprocess, collections

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
EXCLUDE = (
    "src/frontend/prelude_source.npk",         # GENERATED from src/prelude/prelude.npk
    "tests/types/rejection/decreases_rules.npk",  # the shapes under test, clause-less on purpose
)
ID = r"[A-Za-z_][A-Za-z0-9_]*"

def dump(binary, path):
    r = subprocess.run([binary, path], capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        return None
    loops = []
    for ln in r.stdout.split("\n"):
        f = ln.split("\t")
        if len(f) < 13 or f[0] != "L":
            continue
        loops.append({
            "line": int(f[1]), "col": int(f[2]), "kind": f[3], "clause": f[4],
            "cline": int(f[5]), "ccol": int(f[6]), "coff": int(f[7]),
            "bline": int(f[8]), "bcol": int(f[9]), "boff": int(f[10]),
            "writes": [w for w in f[11].split(",") if w],
            "koff": int(f[12]) if len(f) > 12 and f[12].strip() else -1,
        })
    loops.sort(key=lambda l: (l["line"], l["col"]))
    return loops

def match_paren(text, open_at):
    """The index of the `)` matching the `(` at `open_at`, skipping strings and comments naively."""
    depth = 0; i = open_at; n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\": i += 1
                i += 1
        elif c == "(": depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0: return i
        i += 1
    return -1

def match_brace(text, open_at):
    depth = 0; i = open_at; n = len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                if text[i] == "\\": i += 1
                i += 1
        elif c == "/" and i + 1 < n and text[i + 1] == "/":
            while i < n and text[i] != "\n": i += 1
        elif c == "{": depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0: return i
        i += 1
    return -1

FUNC_HEAD = re.compile(r"^[ \t]*(?:pub\s+)?(?:async\s+)?(?:thread\s+)?(?:comptime\s+)?func:", re.M)

def enclosing_function(text, at):
    """The text of the function whose body holds offset `at` -- the last `func:` head
    before it, its body from the first `{` after the head's `=` to the matching `}`.
    The whole file when none is found (a macro body)."""
    heads = [m.start() for m in FUNC_HEAD.finditer(text, 0, at)]
    if not heads:
        return text
    start = heads[-1]
    eq = text.find("=", start)
    brace = text.find("{", eq if eq >= 0 else start)
    if brace < 0 or brace > at:
        return text
    end = match_brace(text, brace)
    if end < at:
        return text
    return text[start:end + 1]

def classify(text, loop):
    """Returns (verdict, detail): verdict `write` with the clause text, or `list` with the class."""
    # the condition: the `(` before the condition's first token, then the matching `)`
    # a binary condition's span is its OPERATOR's, so the `(` is found from the
    # loop's KEYWORD: the first `(` after `while`/`when`
    koff = loop["koff"]
    open_at = text.find("(", koff) if koff >= 0 else text.rfind("(", 0, loop["coff"])
    if open_at < 0: return ("list", "no `(` after the keyword", None)
    close_at = match_paren(text, open_at)
    if close_at < 0: return ("list", "no matching `)`", None)
    cond = text[open_at + 1:close_at].strip()
    loop["cond"] = cond
    body_end = match_brace(text, loop["boff"])
    body = text[loop["boff"]:body_end + 1] if body_end > 0 else ""
    writes = loop["writes"]
    if cond == "true":
        cls = "while(true) with break" if re.search(r"\bbreak\b", body) else "while(true), no break (an event loop?)"
        return ("list", cls, close_at)
    # A TOP-LEVEL `&&` CONJUNCTION: exactly one conjunct is the counter's comparison.
    conjuncts = split_top(cond, "&&")
    if len(conjuncts) == 1 and len(split_top(cond, "||")) > 1:
        return ("list", "compound condition (a `||`: the body may run past the bound)", close_at)
    ms = []
    for cj in conjuncts:
        cjs = cj.strip()
        while cjs.startswith("(") and cjs.endswith(")") and match_paren(cjs, 0) == len(cjs) - 1:
            cjs = cjs[1:-1].strip()
        mm = re.match(r"^(%s)\s*(<=|<|>=|>)\s*(.+)$" % ID, cjs)
        if mm: ms.append(mm)
    if len(ms) != 1:
        if re.match(r"^!?\s*%s$" % ID, cond) or re.match(r"^!?\s*(raw\s+)?%s\(.*\)$" % ID, cond):
            return ("list", "flag or call as the condition", close_at)
        if len(ms) > 1:
            return ("list", "compound condition (two counters)", close_at)
        return ("list", "compound condition", close_at)
    m = ms[0]
    v, op, bound = m.group(1), m.group(2), m.group(3).strip()
    asc = op in ("<", "<=")
    # THE ENCLOSING FUNCTION is the region the declaration and address checks read:
    # another function's `int32:i` or `@n` says nothing about this loop's names.
    region = enclosing_function(text, loop["koff"] if loop["koff"] >= 0 else open_at)
    # v: written only by the one form
    v_writes = [w for w in writes if w[1:] == v]
    if any(w[0] != "=" for w in v_writes):
        return ("list", "counter `%s` address-taken or moved in the body" % v, close_at)
    if not v_writes:
        return ("list", "condition variable `%s` written by nothing in the body (a call, a pointer)" % v, close_at)
    sfx = r"[iu]\d+"
    if asc:
        forms = re.findall(r"\b%s\s*=\s*%s\s*\+\s*1%s\s*;|\b%s\s*\+=\s*1%s\s*;" % (v, v, sfx, v, sfx), body)
    else:
        forms = re.findall(r"\b%s\s*=\s*%s\s*-\s*1%s\s*;|\b%s\s*-=\s*1%s\s*;" % (v, v, sfx, v, sfx), body)
    if len(forms) != len(v_writes):
        return ("list", "counter `%s` written but not only by +-1 (%d writes, %d unit steps)" % (v, len(v_writes), len(forms)), close_at)
    # the counter's width
    vd = re.findall(r"\b((?:u?int)\d+)\s*:\s*%s\b" % re.escape(v), region)
    if not vd:
        return ("list", "counter `%s` has no plain integer declaration in the function" % v, close_at)
    if len(set(vd)) > 1:
        return ("list", "counter `%s` is declared at two widths in the function" % v, close_at)
    vtype = vd[0]
    vsfx = vtype[0] + vtype[3 if vtype.startswith("int") else 4:]
    # THE BOUND IS STABLE: every token of it is a literal, an operator, a paren,
    # a stable name, a stable name's `.len`/`.count`, or a widening cast of one.
    why = stable_bound(bound, writes, region, vtype, vsfx)
    if why:
        return ("list", why, close_at)
    if not lit_or_ident(bound):
        bound = "(" + bound + ")"
    clause = "decreases %s - %s" % (bound, v) if asc else "decreases %s - %s" % (v, bound)
    return ("write", clause, close_at)

def split_top(text, op):
    """Split at top-level occurrences of `op` (outside parentheses)."""
    parts = []; depth = 0; i = 0; last = 0; n = len(text)
    while i < n:
        c = text[i]
        if c == "(": depth += 1
        elif c == ")": depth -= 1
        elif depth == 0 and text.startswith(op, i):
            parts.append(text[last:i]); i += len(op); last = i; continue
        i += 1
    parts.append(text[last:])
    return parts

def lit_or_ident(bound):
    """A bound that needs no parentheses in `bound - v`: a literal, a name, a name's length."""
    return re.match(r"^(\d[A-Za-z0-9_]*|%s(\.(?:len|count))?)$" % ID, bound) is not None

TOKEN = re.compile(r"\s*(?:(\d[A-Za-z0-9_]*)|(%s)(\.(?:len|count))?|(=>)|([()+*-]))" % ID)

def stable_bound(bound, writes, region, vtype, vsfx):
    """None when the bound is stable at the counter's width; else why it is not."""
    i = 0; n = len(bound); saw_name = False
    while i < n:
        m = TOKEN.match(bound, i)
        if not m or m.end() == i:
            return "bound `%s` is not a plain name, literal or arithmetic over them" % bound
        lit, name, member, cast, op = m.groups()
        i = m.end()
        if lit:
            ml = re.match(r"^\d+([iu]\d+)?$", lit)
            if not ml: return "bound literal `%s` is not a plain integer literal" % lit
            if ml.group(1) and ml.group(1) != vsfx:
                return "bound literal `%s` and counter `%s` differ in width" % (lit, vtype)
            continue
        if cast:
            mt = re.match(r"\s*(u?int\d+)", bound[i:])
            if not mt: return "bound `%s`: a cast to something but an integer" % bound
            if mt.group(1) != vtype: return "bound `%s`: cast to `%s`, the counter is `%s`" % (bound, mt.group(1), vtype)
            i += mt.end(); continue
        if op: continue
        if name in ("raw", "relay"):
            return "bound `%s` is a call's result" % bound
        saw_name = True
        if any(w[1:] == name for w in writes):
            return "bound name `%s` written in the body" % name
        if re.search(r"(@|\$\$m\s*|\$\$i\s*)%s\b" % re.escape(name), region):
            return "bound name `%s` has its address taken in the function" % name
        if re.search(r"->\s*:\s*%s\b" % re.escape(name), region):
            return "bound name `%s` is declared as a pointer" % name
        if member:
            if re.search(r"\b%s\s*\(" % re.escape(name), region) and not re.search(r"\b(?:[A-Za-z_][A-Za-z0-9_<>, \[\]]*):%s\b" % re.escape(name), region):
                return "bound `%s`: `%s` is not declared in the function" % (bound, name)
            # `.len` / `.count` is int64 unless the cast says otherwise: the cast is
            # checked above at its own token; a bare member needs an int64 counter
            if "=>" not in bound and vtype != "int64":
                return "bound `%s` is int64 and the counter `%s` is not" % (bound, vtype)
            continue
        bd = re.findall(r"\b((?:u?int)\d+)\s*:\s*%s\b" % re.escape(name), region)
        if not bd:
            return "bound name `%s` has no plain integer declaration in the function" % name
        if len(set(bd)) > 1:
            return "bound name `%s` is declared at two widths in the function" % name
        if bd[0] != vtype:
            return "bound name `%s: %s` and counter `%s` differ in width" % (name, bd[0], vtype)
    return None

def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__); sys.exit(2)
    binary = args.pop(0)
    write = "--write" in args
    if write: args.remove("--write")
    report = None
    if "--report" in args:
        i = args.index("--report"); report = args[i + 1]; del args[i:i + 2]
    dirs = args or ["src", "lib", "npkg", "tools", "tests"]
    files = []
    for d in dirs:
        for dp, _, fns in os.walk(os.path.join(ROOT, d)):
            for fn in fns:
                if fn.endswith(".npk"):
                    files.append(os.path.join(dp, fn))
    files.sort()
    written = []; listed = collections.defaultdict(list); unparsed = []; clause_already = 0
    for path in files:
        rel = os.path.relpath(path, ROOT)
        if rel in EXCLUDE: continue
        loops = dump(binary, path)
        if loops is None:
            unparsed.append(rel); continue
        if not loops: continue
        # BYTES, NOT CHARACTERS: the dump's offsets are byte offsets, and a comment's
        # em dash is three of them -- latin-1 maps one byte to one character both ways.
        text = open(path, encoding="latin-1").read()
        edits = []
        for lp in loops:
            if lp["clause"] != "none":
                clause_already += 1; continue
            verdict, detail, close_at = classify(text, lp)
            where = "%s:%d" % (rel, lp["line"])
            if verdict == "write":
                written.append((where, lp["kind"], detail)); edits.append((close_at, detail))
            else:
                listed[detail if ":" not in detail else detail.split(":")[0]].append((where, lp["kind"], lp.get("cond", "?")[:70]))
        if write and edits:
            for close_at, clause in sorted(edits, reverse=True):
                text = text[:close_at + 1] + " " + clause + text[close_at + 1:]
            open(path, "w", encoding="latin-1").write(text)
    out = []
    out.append("loops written: %d   listed for a reader: %d   already clausal: %d   unparsed files: %d"
               % (len(written), sum(len(v) for v in listed.values()), clause_already, len(unparsed)))
    out.append("")
    out.append("== written ==")
    for where, kind, clause in written:
        out.append("  %s  %s  %s" % (where, kind, clause))
    out.append("")
    out.append("== listed, by class ==")
    for cls, items in sorted(listed.items(), key=lambda kv: -len(kv[1])):
        out.append("  [%d] %s" % (len(items), cls))
        for where, kind, snippet in items:
            out.append("      %s  %s  (%s" % (where, kind, snippet))
    if unparsed:
        out.append("")
        out.append("== unparsed (the dump refused; untouched) ==")
        for u in unparsed: out.append("  " + u)
    text = "\n".join(out) + "\n"
    if report:
        open(report, "w", encoding="utf-8").write(text)
    print(out[0])
    if not report:
        print(text)

if __name__ == "__main__":
    main()
