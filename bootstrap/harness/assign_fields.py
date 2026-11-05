#!/usr/bin/env python3
"""Does every writer of the assignment state say what it does with all eight facts?

Run from the repository root:  python3 bootstrap/harness/assign_fields.py [--table] [ROOT]

`AssignState` (src/frontend/analysis/bindings.npk) holds eight facts per binding.
Four PERMIT something and have to hold on every path, so they intersect where
paths meet. Four FORBID something and fire on any path, so they union. A function
that carries some of them and not others leaves the rest as they were before the
construct, which reads as true on a path that made them false. That was
`state_take_definite` without `sealed_firm` and `assign_loop_body` with no
intersect: a page or a `Result` an arm or a loop rewrote stayed good to use after
it (DEF-520, DEF-522). `state_absorb_may`'s own comment had also miscounted them.

It fails when:

  - the struct holds a fact this table has no direction for;
  - a function writes a fact and has no row in WRITES;
  - a writer's row doesn't match the text: a fact missing, or written in the wrong
    direction, or written from another fact. A carrier's row comes from FACTS, so
    a new fact fails at every carrier that doesn't write it;
  - a call of a writer has no row in CALLS, or a row has no call;
  - the state's type is named outside bindings.npk;
  - its controls stop failing: the reader runs over edited copies of the text
    that must each fail, so a reader gone blind fails too.

ITS REACH: it reads text. It sees which facts a writer writes and which call
sites exist, and it pins each site's arguments, so a site that changes has to be
read again. It doesn't see whether a direction is the right one for a site, which
is what the rows say and the cells in tests/ measure, or a path that leaves a
construct by `break` or `continue` (DEF-429: `state_merge` drops an arm that leaves,
and the loop's end doesn't take what it carried).
"""

import collections
import os
import re
import sys

PERMIT, FORBID = "permit", "forbid"

# What each fact does for a read, and which way it goes where paths meet.
FACTS = {
    "must": (PERMIT, "a read: assigned on every path"),
    "may": (FORBID, "a second write of a `fixed`: assigned on some path"),
    "moved": (FORBID, "a read: moved on some path"),
    "freed": (FORBID, "a read: freed on some path"),
    "checked": (PERMIT, "`.value`: `is_error` false on every path"),
    "errchecked": (PERMIT, "`.err`: `is_error` true on every path"),
    "sealed_any": (FORBID, "a write of the page: sealed on some path"),
    "sealed_firm": (PERMIT, "a call of the page: sealed on every path"),
}


def merge(fact):
    return "&&" if FACTS[fact][0] == PERMIT else "||"


def only(*names):
    return lambda fact: "true" if fact in names else None


# The function, what it writes for a fact (None: it doesn't), and why a fact it
# doesn't write is right to leave.
WRITES = {
    "state_new": (lambda f: "false", "A fresh frame knows nothing."),
    "state_copy": (lambda f: "=", "A snapshot: all eight."),
    "state_overwrite": (lambda f: "=", "The other path's whole state, when one arm leaves: all eight."),
    "state_merge": (merge, "Two arms of an `if`: all eight, each in its direction."),
    "state_absorb_may": (
        lambda f: "||" if FACTS[f][0] == FORBID else None,
        "The four that permit: a path that may not run can't make one true, and clearing "
        "one is `state_intersect_must`'s at a loop or a guard and `state_take_definite`'s "
        "at a total pick. This alone was DEF-520's guard and DEF-522's loop."),
    "state_carry_may": (
        lambda f: "||" if f == "may" else None,
        "The other seven wait for the block's end, since the code after a `defer` in its block "
        "runs before the body: `defer_bodies_land` walks the body on the state, last first, then."),
    "state_intersect_must": (
        lambda f: None if f == "may" else merge(f),
        "`may`: every caller carries it another way. A loop and a guard call "
        "`state_absorb_may` beside it, and a pick's `acc` is only read by "
        "`state_take_definite`, which takes no `may`, while `mayacc` carries it."),
    "state_take_definite": (
        lambda f: "=" if FACTS[f][0] == PERMIT else None,
        "The four that forbid: `state_absorb_may(st, mayacc)` lands them on the next "
        "statement from every arm. That takes the arms that leave too, which `state_merge` "
        "drops, so it refuses more, never less."),
    "state_mark": (
        lambda f: "true" if f in ("must", "may") else "false",
        "An assignment states all eight: a new value is assigned, valid, unchecked and unsealed."),
    "state_mark_checked": (only("checked"), "One refinement, earned by one test."),
    "state_mark_errchecked": (only("errchecked"), "One refinement, earned by one test."),
    "state_mark_moved": (only("moved"),
                         "A read asks `moved` before any permission, and an assignment clears it, "
                         "so a permission left standing can't be used."),
    "state_mark_freed": (only("freed"), "As `state_mark_moved`."),
    "state_mark_sealed": (only("sealed_any", "sealed_firm"),
                          "Sealing changes the page's rights and nothing else about the binding."),
    "state_clear_refinements": (
        lambda f: "false" if f in ("checked", "errchecked") else None,
        "A `defer` body runs at the block's exit, so what was earned above it is stale "
        "(DEF-405 (i)). `must` and `moved`/`freed` are right to leave: nothing un-assigns a "
        "binding, and a move or free of what a body read is refused where it stands (DEF-405 "
        "(ii)). `may` is `state_carry_may`'s. `sealed_any` and `sealed_firm` are NOT right to leave."),
}

# A row above whose reason doesn't settle it: what's left open, under which defect.
# A row here must still match the text, and it isn't read as right.
OPEN = {
    "state_clear_refinements": ("DEF-523", "the body is walked where it's written, so a page sealed or "
                                           "re-assigned before its run gets past"),
    "state_take_definite": ("DEF-524", "`mayacc` takes the arms that leave, so `pick` refuses what `if` accepts"),
}

# Every call of a writer: the function, the code on the line, and what it's for.
CALLS = [
    # --- a frame
    ("state_copy", "AssignState:d = raw state_new(src.count);", "a copy's own arrays"),
    ("assign_decl", "AssignState:st = raw state_new(raw symtab_fn_slots(x.tab, id.index));", "the frame's state"),
    ("assign_decl", "drop state_mark(st, ps);", "a parameter is assigned"),
    ("assign_decl", "AssignState:pre = raw state_new(raw symtab_fn_slots(x.tab, id.index));",
     "the pre-walk's one shared state: it collects addresses, and nothing reads its facts"),
    # --- an `if`
    ("state_merge", "drop state_overwrite(dst, b);", "the then arm left: all of the else arm"),
    ("state_merge", "drop state_overwrite(dst, a);", "the else arm left: all of the then arm"),
    ("assign_stmt", "AssignState:a = raw state_copy(st);", "the then arm's start"),
    ("assign_stmt", "if (et.then_checked) { drop state_mark_checked(a, et.slot); }", "the then arm's road"),
    ("assign_stmt", "else                 { drop state_mark_errchecked(a, et.slot); }", "the then arm's road"),
    ("assign_stmt", "AssignState:b = raw state_copy(st);", "the else arm's start"),
    ("assign_stmt", "if (!et.then_checked) { drop state_mark_checked(b, et.slot); }", "the else arm's road"),
    ("assign_stmt", "else                  { drop state_mark_errchecked(b, et.slot); }", "the else arm's road"),
    ("assign_stmt", "drop state_merge(st, a, raw stmt_exits(x, then_id, raw ASSIGN_DEPTH()),", "all eight, in direction"),
    # --- a declaration
    ("assign_stmt", "drop state_mark(st, raw symtab_stmt_ord(x.tab, id.index));", "a local with a value"),
    # --- a `defer`
    ("assign_stmt", "AssignState:dv = raw state_copy(st);",
     "the body's own road: nothing comes back, since the code after it runs first"),
    ("assign_stmt", "drop state_clear_refinements(dv);", "what was earned above it is stale at the exit"),
    ("assign_lazy_operands", "AssignState:a = raw state_copy(st);", "an operand that may not run walks on its own state (DEF-527)"),
    ("assign_lazy_operands", "drop state_merge(st, a, false, st, false);", "an `if`'s merge; `st` is `is`'s other arm, or the state before an operand with no twin"),
    ("assign_init", "give_road: raw state_new(1i32), give_seen: false,", "a placeholder: `give_seen` says no arm's `give` has written it yet"),
    ("assign_init", "fall_road: raw state_new(1i32), fall_seen: false };", "a placeholder: `fall_seen` says no `fall` of the pick being walked has written it yet"),
    ("defer_bodies_land", "if (x.give_seen) { drop state_merge(x.give_road, x.give_road, false, st, false); }",
     "a `give` runs the `defer`s of the blocks it leaves: what a body forbids (DEF-526) and what it un-permits (DEF-535), all eight "
     "in direction, past a dead `pass`'s merge. Asked of `give_seen`: until this arm gives, the road object is the enclosing arm's"),
    ("give_road_note", "drop state_absorb_may(x.give_road, st);", "a later `give`: the roads' forbids, the union"),
    ("give_road_note", "drop state_intersect_must(x.give_road, st);", "a later `give`: the roads' permits, the intersection"),
    ("give_road_note", "x.give_road = raw state_copy(st);", "the arm's first `give`: its own storage, as the walk goes on in `st`"),
    ("assign_pick", "drop state_absorb_may(mayacc, x.give_road);", "the roads' forbids land once with the other arms' (F-6)"),
    # --- a `fall`: one road for the pick, read at the start of every labelled arm after it (DEF-437, DEF-536)
    ("assign_stmt", "else             { x.fall_road = raw state_copy(st); x.fall_seen = true; }",
     "the pick's first `fall`: its own storage, as the walk reads on in `st` through the code after it"),
    ("assign_stmt", "if (x.fall_seen) { drop state_merge(x.fall_road, x.fall_road, false, st, false); }",
     "a later `fall`: all eight, in direction"),
    ("defer_bodies_land", "if (x.fall_seen) { drop state_merge(x.fall_road, x.fall_road, false, st, false); }",
     "a `fall` runs the `defer`s of the blocks it leaves: what a body forbids and what it un-permits, alike"),
    ("assign_pick", "if (x.fall_seen && ((raw arm_label(arm)) != 0i32)) { drop state_merge(one, one, false, x.fall_road, false); }",
     "a labelled arm's body: its own road and every `fall`'s before it, after the refinement and the guard"),
    ("assign_stmt", "drop state_carry_may(st, dv);", "a body's write of a `fixed` is a write, wherever the next stands"),
    # --- a loop: the body's first trip may not run, a later one starts from what it left
    ("assign_loop_body", "AssignState:once = raw state_copy(st);", "the quiet road's one walk"),
    ("assign_loop_body", "drop state_absorb_may(st, once);", "what forbids carries past the loop"),
    ("assign_loop_body", "drop state_intersect_must(st, once);", "what permits holds only if the body kept it"),
    ("assign_loop_body", "AssignState:pre = raw state_copy(st);", "the silent first trip"),
    ("assign_loop_body", "drop state_absorb_may(st, pre);", "the reporting walk starts from what a trip left"),
    ("assign_loop_body", "drop state_intersect_must(st, pre);",
     "and a read in the body sees what an earlier trip cleared"),
    ("assign_loop_body", "AssignState:inner = raw state_copy(st);", "the reporting walk"),
    # EQUIVALENT TODAY, both: every fact is written whatever state a walk starts from, and a
    # fact is read only to report, so a second trip clears or sets nothing the first didn't.
    # Removing either moves nothing (`522_no_inner`, `522_no_inner_absorb`). A walk that reads
    # a fact to decide a write needs a cell here.
    ("assign_loop_body", "drop state_absorb_may(st, inner);", "as `pre`; no cell can fail it yet"),
    ("assign_loop_body", "drop state_intersect_must(st, inner);", "as `pre`; no cell can fail it yet"),
    # --- a `pick`
    ("assign_pick", "AssignState:acc = raw state_new(st.count);", "what every arm that falls through keeps"),
    ("assign_pick", "AssignState:pristine = raw state_copy(st);", "each arm's start (F-6)"),
    ("assign_pick", "AssignState:mayacc = raw state_copy(st);", "what forbids, from every arm"),
    ("assign_pick", "AssignState:one = raw state_copy(pristine);", "one arm's road"),
    ("assign_pick", "drop state_mark(one, raw symbol_slot(x, ps));", "the arm's bindings"),
    ("assign_pick", "drop state_mark_checked(one, et.slot);", "the arm's road"),
    ("assign_pick", "drop state_mark_errchecked(one, et.slot);", "the arm's road"),
    ("assign_pick", "drop state_absorb_may(pristine, one);", "a guard runs whether its arm is taken or not (DEF-520)"),
    ("assign_pick", "drop state_intersect_must(pristine, one);", "so a later arm may or may not have seen its write"),
    ("assign_pick", "drop state_absorb_may(mayacc, one);", "the arm's prohibitions"),
    ("assign_pick", "drop state_intersect_must(acc, one);", "the later arms that fall through"),
    ("assign_pick", "drop state_overwrite(acc, one);", "the first arm that falls through"),
    ("assign_pick", "drop state_take_definite(st, acc);", "the four that permit, for a total pick"),
    ("assign_pick", "drop state_absorb_may(st, mayacc);", "the four that forbid, once, after every arm"),
    # --- a write
    ("assign_assign", "drop state_mark(st, raw symbol_slot(x, tn.a));", "a plain assignment"),
    ("assign_assign", "drop state_mark(st, raw symbol_slot(x, tn.a));", "the same, on its second road"),
    ("assign_assign", "drop state_mark(st, dslot);", "an assignment through a field or a deref's root"),
    ("assign_mark_place", "drop state_mark(st, cslot);", "`$$m` of a binding"),
    # --- a page, a move, a free
    ("wildx_transition", "drop state_mark_sealed(st, slot);", "`wildx_seal`"),
    ("invalidate_place", "drop state_mark_freed(st, ms);", "a free"),
    ("invalidate_place", "drop state_mark_moved(st, ms);", "a move"),
]


def code_lines(text):
    """(enclosing function, code with its comment and indent gone) per line."""
    cur = None
    for line in text.split("\n"):
        m = re.match(r"(?:pub )?func:(\w+) =", line)
        if m:
            cur = m.group(1)
        code = line.split("//")[0].strip()
        if code:
            yield cur, code


def fields_of(text):
    i = text.index("pub struct:AssignState = {")
    return re.findall(r"^\s*wild \w+->:(\w+);", text[i:text.index("\n};", i)], re.M)


def classify(fact, rhs):
    """How a write of `fact` reads: its operator, or None for a form this can't read."""
    rhs = rhs.strip()
    if rhs in ("true", "false"):
        return rhs
    one = r"\w+\.%s\[[^\]]*\]" % fact
    if re.fullmatch(one, rhs):
        return "="
    m = re.fullmatch(r"(%s)\s*(&&|\|\|)\s*(%s)" % (one, one), rhs)
    return m.group(2) if m else None


def check(text):
    fails = []
    fields = fields_of(text)
    for f in fields:
        if f not in FACTS:
            fails.append("the struct holds `%s`, and FACTS gives it no direction" % f)
    for f in FACTS:
        if f not in fields:
            fails.append("FACTS names `%s`, and the struct doesn't hold it" % f)
    if fails:
        return fails

    wrote = collections.defaultdict(dict)
    pat = re.compile(r"\b\w+\.(%s)\[[^\]]*\]\s*=(?!=)\s*([^;]*);" % "|".join(FACTS))
    for fn, code in code_lines(text):
        for m in pat.finditer(code):
            how = classify(m.group(1), m.group(2))
            if how is None:
                fails.append("%s writes `%s` by `%s`, which isn't a form this reads" % (fn, m.group(1), m.group(2)))
            wrote[fn][m.group(1)] = how
    for fn in wrote:
        if fn not in WRITES:
            fails.append("%s writes %s and has no row in WRITES" % (fn, ", ".join(sorted(wrote[fn]))))
    for fn in OPEN:
        if fn not in WRITES:
            fails.append("OPEN names %s, which has no row in WRITES" % fn)
    for fn, (rule, _) in WRITES.items():
        if fn not in wrote:
            fails.append("%s has a row and writes nothing" % fn)
            continue
        for f in FACTS:
            want, got = rule(f), wrote[fn].get(f)
            if want != got:
                fails.append("%s: `%s` should be `%s`, and the text has `%s`" % (fn, f, want, got))

    names = "|".join(WRITES)
    found = collections.Counter()
    for fn, code in code_lines(text):
        if re.search(r"\b(%s)\(" % names, code) and not code.startswith(("func:", "pub func:")):
            found[(fn, code)] += 1
    table = collections.Counter((fn, code) for fn, code, _ in CALLS)
    for k in found - table:
        fails.append("%s: `%s` has no row in CALLS" % k)
    for k in table - found:
        fails.append("%s: CALLS has `%s`, and the text doesn't" % k)
    return fails


# Each must make `check` fail, whatever the tree says: a check that can't fail
# proves nothing, and each of these is a road a change has really taken or could.
CONTROLS = [
    ("a new fact in the struct", "    wild bool->:must;\n", "    wild bool->:must;\n    wild bool->:newfact;\n"),
    ("`state_take_definite` loses `sealed_firm`", "        // sealed after the pick, and the call went to writable bytes unsealed.\n        dst.sealed_firm[i] = src.sealed_firm[i];\n", ""),
    ("`state_merge` unions a permission", "dst.checked[i] = a.checked[i] && b.checked[i];", "dst.checked[i] = a.checked[i] || b.checked[i];"),
    ("a merge writes one fact from another", "dst.freed[i] = a.freed[i] || b.freed[i];", "dst.freed[i] = a.moved[i] || b.moved[i];"),
    ("a loop's intersect goes", "    drop state_intersect_must(st, pre);\n", ""),
    ("a new call of a carrier", "    drop state_absorb_may(st, inner);\n", "    drop state_absorb_may(st, inner);\n    drop state_absorb_may(st, inner);\n"),
    ("a new writer", "func:state_copy =", "func:state_sneak = NIL(AssignState:s) never fails { s.moved[0i64] = false; pass NIL; };\nfunc:state_copy ="),
]


def controls(text):
    """The controls this tree can run: each edit applies once, and the check then fails."""
    out = []
    for name, a, b in CONTROLS:
        if text.count(a) != 1:
            out.append("control `%s` no longer applies: its text is there %d times" % (name, text.count(a)))
        elif not check(text.replace(a, b)):
            out.append("control `%s` didn't fail, so this reader has gone blind" % name)
    return out


def outside(root):
    out = []
    for base in ("src", "tools", "npkg"):
        for d, _, names in os.walk(os.path.join(root, base)):
            for n in names:
                p = os.path.join(d, n)
                if n.endswith(".npk") and not p.endswith(os.path.join("analysis", "bindings.npk")):
                    with open(p, encoding="utf-8") as fh:
                        if "AssignState" in fh.read():
                            out.append(os.path.relpath(p, root))
    return ["%s names `AssignState`, and this reads bindings.npk alone" % p for p in out]


def table():
    print("the facts")
    for f, (way, what) in FACTS.items():
        print("  %-12s %-7s %-3s %s" % (f, way, merge(f), what))
    print("\nthe writers, each fact as the text writes it ('-' for not at all)")
    print("  %-24s %s" % ("", " ".join("%-11s" % f for f in FACTS)))
    for fn, (rule, why) in WRITES.items():
        print("  %-24s %s" % (fn, " ".join("%-11s" % (rule(f) or "-") for f in FACTS)))
        print("      %s" % why)
        if fn in OPEN:
            print("      OPEN on %s: %s." % OPEN[fn])
    print("\nthe calls: %d" % len(CALLS))
    for fn, code, why in CALLS:
        print("  %-18s %-72s %s" % (fn, code, why))


def main():
    args = [a for a in sys.argv[1:] if a != "--table"]
    root = args[0] if args else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    with open(os.path.join(root, "src", "frontend", "analysis", "bindings.npk"), encoding="utf-8") as fh:
        text = fh.read()
    fails = check(text) + outside(root)
    if not fails:
        fails = controls(text)
    if "--table" in sys.argv:
        table()
    for f in fails:
        print("assign-fields: " + f)
    if "--table" in sys.argv:
        print("\nassign-fields: %d failure(s), %d facts, %d writers, %d calls, %d controls"
              % (len(fails), len(FACTS), len(WRITES), len(CALLS), len(CONTROLS)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
