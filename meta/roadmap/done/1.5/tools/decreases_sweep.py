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

TWO MORE SHAPES THE TOOL PROVES (added at step 3, after the dry run of step 1):
a counter WIDENED in the comparison, `(v => T) < bound` with `bound` stable at
the wider width `T` -- the measure is `bound - (v => T)`, at `T`; and a unit
step of ANY positive literal (`v = v + 2i64;`, `v += 4i32;`): the measure still
shrinks on every trip, by at least one.

THE READING IS A FILE, NOT A HAND EDIT (P-3, D-316): `decreases_read.txt`
beside this tool holds one line per loop a reader decided, and `--write`
applies it -- so the decision, its reason and the text it produced are one
greppable record, and the harness proves the record. The directives:

    stable    FILE:LINE            the tool's own shape applies; the reader saw that
                                   the bound is stable although a rule of thumb
                                   refused it (a pointer's field nothing grows, a
                                   `.count` that is not a List's int64)
    hoist     FILE:LINE NAME       the bound (a call's result) is captured once
                                   before the loop into `T:NAME` (T the counter's
                                   type), the condition names NAME, and the clause is
                                   `decreases NAME - v`
    measure   FILE:LINE EXPR       the clause is `decreases EXPR`, as written
    unbounded FILE:LINE REASON     the clause is `unbounded`, with `// REASON` on the
                                   line above (D-316)
    manual    FILE:LINE NOTE       the loop was restructured by hand and carries its
                                   clause in the source (the tool checks it does)

A line beginning with `#` is a comment. A directive for a loop the tool would
write itself, or for a loop that already carries a clause (other than `manual`),
is STALE and reported; a directive the tool cannot apply is an ERROR and the
file it names is left untouched. Line numbers are the CURRENT tree's (the dump's):
after a `--write` that inserted lines, regenerate the report before adding more.

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

def classify(text, loop, trust=False):
    """Returns (verdict, detail, close_at): verdict `write` with the clause text, or `list` with the class.
    With `trust` (the reader's `stable` directive) the rules of thumb about the bound's names -- a
    pointer, an address taken, a width, a missing declaration -- are not asked."""
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
        if mm: ms.append((mm.group(1), None, mm.group(2), mm.group(3))); continue
        # THE WIDENED COUNTER: `(v => T) OP bound` -- the measure is taken at T.
        mw = re.match(r"^\(\s*(%s)\s*=>\s*(u?int\d+)\s*\)\s*(<=|<|>=|>)\s*(.+)$" % ID, cjs)
        if mw: ms.append((mw.group(1), mw.group(2), mw.group(3), mw.group(4)))
    if len(ms) != 1:
        if re.match(r"^!?\s*%s$" % ID, cond) or re.match(r"^!?\s*(raw\s+)?%s\(.*\)$" % ID, cond):
            return ("list", "flag or call as the condition", close_at)
        if len(ms) > 1:
            return ("list", "compound condition (two counters)", close_at)
        return ("list", "compound condition", close_at)
    v, wide, op, bound = ms[0]
    bound = bound.strip()
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
    # A UNIT STEP OF ANY POSITIVE LITERAL: `v = v + k<sfx>;` / `v += k<sfx>;` (k >= 1)
    # in the counting direction -- the measure shrinks by at least one per trip.
    step = r"[1-9]\d*[iu]\d+"
    if asc:
        forms = re.findall(r"\b%s\s*=\s*%s\s*\+\s*%s\s*;|\b%s\s*\+=\s*%s\s*;" % (v, v, step, v, step), body)
    else:
        forms = re.findall(r"\b%s\s*=\s*%s\s*-\s*%s\s*;|\b%s\s*-=\s*%s\s*;" % (v, v, step, v, step), body)
    if len(forms) != len(v_writes):
        return ("list", "counter `%s` written but not only by a positive literal step (%d writes, %d unit steps)" % (v, len(v_writes), len(forms)), close_at)
    # the counter's width
    vd = re.findall(r"\b((?:u?int)\d+)\s*:\s*%s\b" % re.escape(v), region)
    if not vd and not trust:
        return ("list", "counter `%s` has no plain integer declaration in the function" % v, close_at)
    if len(set(vd)) > 1 and not trust:
        return ("list", "counter `%s` is declared at two widths in the function" % v, close_at)
    vtype = vd[0] if vd else "int64"
    if wide is not None:
        vtype = wide          # the measure is taken at the widened type
    vsfx = vtype[0] + vtype[3 if vtype.startswith("int") else 4:]
    # THE BOUND IS STABLE: every token of it is a literal, an operator, a paren,
    # a stable name, a stable name's `.len`/`.count`, or a widening cast of one.
    why = stable_bound(bound, writes, region, vtype, vsfx, trust)
    if why:
        return ("list", why, close_at)
    if not lit_or_ident(bound):
        bound = "(" + bound + ")"
    vtext = v if wide is None else "(%s => %s)" % (v, wide)
    if (not asc) and re.match(r"^0[iu]\d+$", bound):
        clause = "decreases %s" % vtext          # `v > 0`: the counter is its own measure
    else:
        clause = "decreases %s - %s" % (bound, vtext) if asc else "decreases %s - %s" % (vtext, bound)
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
    return re.match(r"^(\d[A-Za-z0-9_]*|%s(?:\.%s)*(\.(?:len|count))?)$" % (ID, ID), bound) is not None

# a name is a DOTTED PATH rooted at an identifier (`t.v`, `x.sites.v`); the rules of thumb ask the ROOT
TOKEN = re.compile(r"\s*(?:(\d[A-Za-z0-9_]*)|(%s(?:\.%s)*?)(\.(?:len|count))?(?![A-Za-z0-9_.])|(=>)|([()+*-]))" % (ID, ID))

def stable_bound(bound, writes, region, vtype, vsfx, trust=False):
    """None when the bound is stable at the counter's width; else why it is not.
    `trust` (the reader's `stable`) drops the rules of thumb about a name -- its address,
    a pointer type, a width, a missing declaration -- and keeps the one fact the dump
    shows: a name the BODY writes is never stable."""
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
            if ml.group(1) and ml.group(1) != vsfx and not trust:
                return "bound literal `%s` and counter `%s` differ in width" % (lit, vtype)
            continue
        if cast:
            mt = re.match(r"\s*(u?int\d+)", bound[i:])
            if not mt: return "bound `%s`: a cast to something but an integer" % bound
            if mt.group(1) != vtype and not trust: return "bound `%s`: cast to `%s`, the counter is `%s`" % (bound, mt.group(1), vtype)
            i += mt.end(); continue
        if op: continue
        root = name.split(".")[0]
        if root in ("raw", "relay"):
            return "bound `%s` is a call's result" % bound
        saw_name = True
        if any(w[1:] == root for w in writes):
            return "bound name `%s` written in the body" % root
        if trust:
            continue
        if "." in name and not member:
            return "bound `%s` is a member read, not a plain name" % name
        name = root
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

DECISIONS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "decreases_read.txt")

def read_decisions():
    """`decreases_read.txt`: {"file:line": (directive, argument)}; a line beginning with `#` is a comment."""
    d = {}
    if not os.path.exists(DECISIONS):
        return d
    for raw in open(DECISIONS, encoding="utf-8"):
        ln = raw.rstrip("\n")
        if not ln.strip() or ln.lstrip().startswith("#"):
            continue
        parts = ln.split(None, 2)
        if len(parts) < 2 or parts[0] not in ("stable", "hoist", "measure", "unbounded", "manual"):
            raise SystemExit("decreases_read.txt: cannot read the line %r" % ln)
        d[parts[1]] = (parts[0], parts[2].strip() if len(parts) > 2 else "")
    return d

def line_start_of(text, off):
    return text.rfind("\n", 0, off) + 1

def indent_of(text, off):
    ls = line_start_of(text, off)
    j = ls
    while j < len(text) and text[j] in " \t": j += 1
    return text[ls:j]

def hoist_plan(text, loop, name):
    """The three edits of a `hoist`: (error, edits) -- edits are (offset, kind, payload)."""
    koff = loop["koff"]
    open_at = text.find("(", koff) if koff >= 0 else -1
    if open_at < 0: return ("no `(` after the keyword", [])
    close_at = match_paren(text, open_at)
    if close_at < 0: return ("no matching `)`", [])
    cond = text[open_at + 1:close_at]
    m = re.match(r"^(\s*)(%s)(\s*)(<=|<|>=|>)(\s*)(.+?)(\s*)$" % ID, cond, re.S)
    if not m: return ("the condition is not `v OP bound`: %r" % cond.strip(), [])
    v, op, bound = m.group(2), m.group(4), m.group(6)
    region = enclosing_function(text, koff)
    vd = re.findall(r"\b((?:u?int)\d+)\s*:\s*%s\b" % re.escape(v), region)
    if len(set(vd)) != 1: return ("counter `%s` has no single plain integer declaration in the function" % v, [])
    vtype = vd[0]
    if re.search(r"\b%s\b" % re.escape(name), region):
        return ("the name `%s` already occurs in the function" % name, [])
    init = bound
    if init.startswith("(") and init.endswith(")") and match_paren(init, 0) == len(init) - 1:
        init = init[1:-1].strip()
    ls = line_start_of(text, koff)
    indent = indent_of(text, koff)
    decl = "%s%s:%s = %s;\n" % (indent, vtype, name, init)
    new_cond = m.group(1) + v + m.group(3) + op + m.group(5) + name + m.group(7)
    asc = op in ("<", "<=")
    clause = "decreases %s - %s" % (name, v) if asc else "decreases %s - %s" % (v, name)
    return (None, [(close_at, "clause", clause),
                   (open_at + 1, "replace", (close_at, new_cond)),
                   (ls, "insert", decl)])

def apply_edits(text, edits):
    """Edits are (offset, kind, payload), applied from the highest offset down so none moves another."""
    for off, kind, payload in sorted(edits, key=lambda e: -e[0]):
        if kind == "clause":
            text = text[:off + 1] + " " + payload + text[off + 1:]
        elif kind == "insert":
            text = text[:off] + payload + text[off:]
        elif kind == "replace":
            end, new = payload
            text = text[:off] + new + text[end:]
    return text

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
    dirs = args or ["src", "lib", "npkg", "tools", "tests", "meta/roadmap/done/1.5/tools"]   # the dump tool sweeps itself
    files = []
    for d in dirs:
        for dp, _, fns in os.walk(os.path.join(ROOT, d)):
            for fn in fns:
                if fn.endswith(".npk"):
                    files.append(os.path.join(dp, fn))
    files.sort()
    decisions = read_decisions()
    written = []; decided = collections.defaultdict(list); listed = collections.defaultdict(list)
    unparsed = []; clause_already = 0; manual_present = []; stale = []; errors = []; applied = 0
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
        edits = []; file_errors = []
        for lp in loops:
            where = "%s:%d" % (rel, lp["line"])
            dec = decisions.pop(where, None)
            if lp["clause"] != "none":
                clause_already += 1
                if dec and dec[0] == "manual":
                    manual_present.append((where, lp["kind"], dec[1]))
                elif dec and (dec[0] == "unbounded") == (lp["clause"] == "unbounded"):
                    applied += 1          # the directive was applied by an earlier `--write`: the record stands
                elif dec:
                    stale.append((where, "already carries `%s`; the directive `%s` is stale" % (lp["clause"], dec[0])))
                continue
            verdict, detail, close_at = classify(text, lp)
            if verdict == "write":
                written.append((where, lp["kind"], detail)); edits.append((close_at, "clause", detail))
                if dec: stale.append((where, "the tool writes `%s` itself; the directive `%s` is stale" % (detail, dec[0])))
                continue
            if dec is None:
                listed[detail if ":" not in detail else detail.split(":")[0]].append((where, lp["kind"], lp.get("cond", "?")[:70]))
                continue
            kind, arg = dec
            if kind == "stable":
                v2, d2, c2 = classify(text, lp, trust=True)
                if v2 != "write":
                    file_errors.append((where, "stable: still no measure -- " + d2))
                else:
                    decided["stable"].append((where, lp["kind"], d2)); edits.append((c2, "clause", d2))
            elif kind == "hoist":
                if not re.match(r"^%s$" % ID, arg):
                    file_errors.append((where, "hoist: `%s` is not a name" % arg)); continue
                err, ed = hoist_plan(text, lp, arg)
                if err: file_errors.append((where, "hoist: " + err))
                else:
                    decided["hoist"].append((where, lp["kind"], "%s := %s; %s" % (arg, lp.get("cond", "?")[:60], ed[0][2])))
                    edits.extend(ed)
            elif kind == "measure":
                if not arg: file_errors.append((where, "measure: no expression")); continue
                decided["measure"].append((where, lp["kind"], "decreases " + arg))
                edits.append((close_at, "clause", "decreases " + arg))
            elif kind == "unbounded":
                if not arg: file_errors.append((where, "unbounded: no reason (D-316: the reason goes on the line above)")); continue
                ls = line_start_of(text, lp["koff"]); indent = indent_of(text, lp["koff"])
                decided["unbounded"].append((where, lp["kind"], arg))
                edits.append((close_at, "clause", "unbounded"))
                edits.append((ls, "insert", "%s// %s\n" % (indent, arg)))
            elif kind == "manual":
                file_errors.append((where, "manual: the source carries no clause yet"))
        if file_errors:
            errors.extend(file_errors)
        elif write and edits:
            open(path, "w", encoding="latin-1").write(apply_edits(text, edits))
    scanned = set(os.path.relpath(f, ROOT) for f in files)
    for where, dec in sorted(decisions.items()):
        if where.rsplit(":", 1)[0] not in scanned:
            continue          # a directive for a file outside the directories this run swept
        stale.append((where, "no loop at this line (directive `%s`): the line numbers are the current tree's" % dec[0]))
    n_listed = sum(len(v) for v in listed.values()); n_decided = sum(len(v) for v in decided.values())
    out = []
    out.append("loops written: %d   decided by the reading: %d   manual (clause present): %d   listed, undecided: %d   already clausal: %d (%d by an applied directive)   unparsed files: %d   stale directives: %d   errors: %d"
               % (len(written), n_decided, len(manual_present), n_listed, clause_already, applied, len(unparsed), len(stale), len(errors)))
    out.append("")
    if errors:
        out.append("== ERRORS (the file is left untouched) ==")
        for where, msg in errors: out.append("  %s  %s" % (where, msg))
        out.append("")
    if stale:
        out.append("== stale directives ==")
        for where, msg in stale: out.append("  %s  %s" % (where, msg))
        out.append("")
    out.append("== written (the tool's own shape) ==")
    for where, kind, clause in written:
        out.append("  %s  %s  %s" % (where, kind, clause))
    out.append("")
    out.append("== decided by the reading (decreases_read.txt) ==")
    for k in ("hoist", "measure", "stable", "unbounded"):
        if decided[k]:
            out.append("  [%d] %s" % (len(decided[k]), k))
            for where, kind, what in decided[k]:
                out.append("      %s  %s  %s" % (where, kind, what))
    if manual_present:
        out.append("  [%d] manual (clause present in the source)" % len(manual_present))
        for where, kind, note in manual_present:
            out.append("      %s  %s  %s" % (where, kind, note))
    out.append("")
    out.append("== listed, undecided, by class ==")
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
    if errors:
        sys.exit(1)

if __name__ == "__main__":
    main()
