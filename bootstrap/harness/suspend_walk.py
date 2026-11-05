#!/usr/bin/env python3
"""Does the suspend walk reach every child of every node kind?

Run from the repository root:  python3 bootstrap/harness/suspend_walk.py [--table] [ROOT]

`src/frontend/analysis/suspend.npk` keeps a local in a coroutine's frame only if
it sees the read and the back edge. A child it does not walk is a read it does
not see, and the local's slot is then a dead resume alloca: a wrong value with
no sign. This came back three times in one day (DEF-431, DEF-494, DEF-496), so
it is a check now (DEF-494's face C, ruled 2026-11-03; compiler_69's census494.py).

The children come from `ast_depth.npk`, the tree's own total reader (a kind it
misses fails tests/frontend/ast.npk). The walk's arms come from the source text
of `suspend_stmt`, `suspend_expr` and their helpers. It fails when:

  - a statement kind has a child the walk's arm doesn't name;
  - a statement kind has no arm (at run time that is SUSPEND-001, closed, and it
    still means no async function can hold one);
  - `stmt_children_h` reads a child through an accessor this table doesn't know,
    because a slot it can't name is a slot it can't count;
  - a kind with a clause, which is what makes it a loop, has no `loop_push`;
  - an expression kind has no shape, or a custom shape with no arm that walks it,
    or an arm of its own this table hasn't read.

ITS REACH: it reads text, so it sees which children an arm names, not that the
arm's code walks them in the right order or at all paths; it knows nothing of a
read the source doesn't spell (an implicit call, a hidden slot). The cells in
tests/backend/programs/ measure those.
"""

import os
import re
import sys

KIND = r"(?:StmtKind|ExprKind)\.([A-Za-z]+)"

# Each accessor `stmt_children_h` may read, and the slot it names. Capitals mark
# a verify node. A new accessor needs a line here, which is the point.
SLOT = [
    (r"stmt_list_start", "list"), (r"stmt_list_count", "list"),
    (r"stmt_decl_init", "init"),
    (r"stmt_target", "target"), (r"stmt_assigned", "assigned"),
    (r"stmt_cond", "cond"), (r"stmt_then", "then"), (r"stmt_else", "else"),
    (r"stmt_body", "body"),
    (r"stmt_while_invariant", "INVARIANT"), (r"stmt_while_measure", "MEASURE"),
    (r"stmt_for_iter", "iter"), (r"stmt_for_body", "body"),
    (r"stmt_for_invariant", "INVARIANT"), (r"stmt_for_measure", "MEASURE"),
    (r"stmt_when_body", "body"), (r"stmt_when_then", "then"), (r"stmt_when_end", "end"),
    (r"stmt_when_invariant", "INVARIANT"), (r"stmt_when_measure", "MEASURE"),
    (r"stmt_counted_body", "body"), (r"stmt_counted_arg", "args"), (r"stmt_counted_argc", "args"),
    (r"stmt_counted_invariant", "INVARIANT"), (r"stmt_counted_measure", "MEASURE"),
    (r"pick_selector", "selector"), (r"pick_arm_start", "arms"), (r"pick_arm_count", "arms"),
    (r"arm_guard", "guard"), (r"arm_body", "body"),
]
# In `stmt_children_h`, the arithmetic of a height, not a child.
HEIGHT = {"eh", "sh", "hmax", "verify_h", "stmt_window_h", "stmt_carries_expr"}

# An expression kind's own arm in `suspend_expr`, read and accounted for by name:
# a shape with no walk of its own needs a line, and what the line must still say.
CUSTOM = {
    "ExprStructLiteralExpr": r"i = i \+ 2i32",  # the odd indexes are the values
    "ExprCallExpr": r"expr_callee\(n\).*?\+ gc",  # the callee, then the list past the generics
    "ExprBuiltinExpr": r"\+ bg",  # the list past the generics
    "ExprPickExpr": r"pick_selector.*?pick_arm_start",
}
OWN_ARM = {
    "ExprIdentifierExpr": r"reg_use",
    "ExprAwaitExpr": r"suspend_expr\(x, awop2",
    "ExprBorrowExpr": r"suspend_expr\(x, raw expr_operand\(n\)",
    "ExprAddressOfExpr": r"suspend_expr\(x, raw expr_operand\(n\)",
    "ExprIndexExpr": r"suspend_expr\(x, raw expr_lhs\(n\).*?suspend_expr\(x, raw expr_rhs\(n\)",
    "ExprMethodCallExpr": r"recv_takes_address",  # falls to the shape walk
    "ExprRangeExpr": None,  # read inside the index arm, and walked by shape
}


def func(src, name):
    i = src.index("func:%s =" % name)
    return src[i:src.index("\n};", i)]


def enum(src, name):
    b = src[src.index("pub enum:%s = {" % name):]
    return re.findall(r"^\s+((?:Stmt|Expr)[A-Za-z]+)\s*=", b[:b.index("};")], re.M)


def arms(body, var):
    """One (kinds, text) per `if` at the body's own indent that names a kind."""
    out, lines, i = [], body.split("\n"), 0
    while i < len(lines):
        line = lines[i]
        if re.match(r"^    if \(", line) and var in line:
            kinds = re.findall(KIND, line)
            text = [line]
            last = line
            while not re.search(r"\{\s*(?:pass [^;]*; \}|)\s*$", last) and i + 1 < len(lines):
                i += 1
                last = lines[i]
                text.append(last)
                kinds += re.findall(KIND, last)
            if not re.search(r"\}\s*$", last):
                while i + 1 < len(lines):
                    i += 1
                    text.append(lines[i])
                    if re.match(r"^    \}", lines[i]):
                        break
            if kinds:
                out.append((kinds, "\n".join(text)))
        i += 1
    return out


def slots(text):
    return sorted({lab for pat, lab in SLOT if re.search(r"\b%s\b" % pat, text)})


def carried(parse_stmt):
    return re.findall(KIND, func(parse_stmt, "stmt_carries_expr"))


def check(root):
    def rd(p):
        with open(os.path.join(root, p), encoding="utf-8") as fh:
            return fh.read()
    kinds = rd("src/frontend/ast_kind.npk")
    depth = rd("src/frontend/ast_depth.npk")
    walk = rd("src/frontend/analysis/suspend.npk")
    carry = carried(rd("src/frontend/parse_stmt.npk"))
    fails, rows = [], []

    # --- statements
    sc = func(depth, "stmt_children_h")
    known = {pat for pat, _ in SLOT}
    for name in set(re.findall(r"\braw ([a-z_]+)\(", sc)) - HEIGHT:
        if name not in known:
            fails.append("`stmt_children_h` reads `%s`, which this table has no slot for" % name)
    want = {}
    for ks, text in arms(sc, "k =="):
        for k in ks:
            want[k] = slots(text)
    want["StmtDeferStmt"] = ["a"]
    for k in carry + ["StmtPassStmt", "StmtFailStmt", "StmtReturnStmt", "StmtExitStmt"]:
        want[k] = ["a"]
    sw = func(walk, "suspend_stmt")
    helpers = {h: func(walk, h) for h in ("suspend_operand", "suspend_counted", "suspend_window")}
    clauses = func(walk, "suspend_clauses")
    for name, what in (("invariant_start", "an `invariant`'s conjuncts"), ("measure_expr", "a `decreases` expression")):
        if name not in clauses:
            fails.append("`suspend_clauses` no longer walks %s" % what)
    got, pushed = {}, {}
    for ks, text in arms(sw, "s.kind =="):
        full = text
        for h, body in helpers.items():
            if h in text:
                full += body
        for k in ks:
            got[k] = slots(full)
            if re.search(r"suspend_(?:expr|operand)\(|stmt_expr|suspend_stmt\(x, StmtId\{ index: s\.a", full):
                got[k] = got[k] + ["a"]
            pushed[k] = "loop_push" in full
    if "stmt_carries_expr(s.kind)" not in sw or "stmt_expr(s)" not in sw:
        fails.append("`suspend_stmt` no longer walks `stmt_carries_expr`'s kinds the way this reads them")
    for k in carry:
        got[k] = ["a"]
        pushed[k] = False
    for k in enum(kinds, "StmtKind"):
        if k == "StmtNone":
            continue
        w = want.get(k, [])
        if k not in got:
            fails.append("%s: `suspend_stmt` has no arm for it" % k)
            rows.append((k, w, None, False))
            continue
        missing = [s for s in w if s not in got[k]]
        if missing:
            fails.append("%s: `suspend_stmt` doesn't walk %s" % (k, ", ".join(missing)))
        if ("INVARIANT" in w or "MEASURE" in w) and not pushed[k]:
            fails.append("%s: a loop with no `loop_push`, so a park in it can't reach back" % k)
        rows.append((k, w, got[k], pushed[k]))

    # --- expressions
    shapes = {}
    for m in re.finditer(r"ExprKind\.(Expr[A-Za-z]+)\)\s*\{ pass \(raw EXPR_SHAPE_([A-Z]+)\(\)\); \}",
                         func(rd("src/frontend/ast.npk"), "expr_shape")):
        shapes[m.group(1)] = m.group(2)
    se = func(walk, "suspend_expr")
    own = {}
    for ks, text in arms(se, "n.kind =="):
        for k in ks:
            own[k] = text
    for k in enum(kinds, "ExprKind"):
        if k == "ExprNone":
            continue
        s = shapes.get(k)
        if s is None:
            fails.append("%s: `expr_shape` gives it no shape, so the walk can't follow its slots" % k)
            continue
        if s == "CUSTOM":
            pat = CUSTOM.get(k)
            if pat is None or k not in own or not re.search(pat, own[k], re.S):
                fails.append("%s: a custom shape with no arm in `suspend_expr` that walks all of it" % k)
        elif k in own and k not in OWN_ARM:
            fails.append("%s: an arm of its own in `suspend_expr` that this table hasn't read" % k)
        elif k in own and OWN_ARM[k] and not re.search(OWN_ARM[k], own[k], re.S):
            fails.append("%s: its arm in `suspend_expr` no longer walks what this table read" % k)
    return fails, rows, len(shapes)


def main():
    args = [a for a in sys.argv[1:] if a != "--table"]
    root = args[0] if args else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    fails, rows, nexpr = check(root)
    if "--table" in sys.argv:
        print("%-22s %-34s %-34s %s" % ("kind", "children", "walked", "loop_push"))
        for k, w, g, p in rows:
            print("%-22s %-34s %-34s %s" % (k, ",".join(w) or "-", "NO ARM" if g is None else (",".join(g) or "-"), "yes" if p else ""))
        print("expression kinds with a shape:", nexpr)
    for f in fails:
        print("suspend-walk: " + f)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
