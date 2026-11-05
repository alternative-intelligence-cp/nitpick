#!/usr/bin/env python3
"""Does every writer of the assignment state say what it does with all eight facts?

Run from the repository root:  python3 bootstrap/harness/assign_fields.py [--table] [ROOT]

`AssignState` (src/frontend/analysis/bindings.npk) holds eight facts per binding
and one flag per ROAD, `dead`: the road left, or every road into a join had.
Four PERMIT something and have to hold on every path, so they intersect where
paths meet. Four FORBID something and fire on any path, so they union. A function
that carries some of them and not others leaves the rest as they were before the
construct, which reads as true on a path that made them false. That was
`state_take_definite` without `sealed_firm` and `assign_loop_body` with no
intersect: a page or a `Result` an arm or a loop rewrote stayed good to use after
it (DEF-520, DEF-522). `state_absorb_may`'s own comment had also miscounted them.

It fails when:

  - the struct holds a fact this table has no direction for, or a road flag ROAD
    doesn't name;
  - a function writes a fact and has no row in WRITES;
  - a writer's row doesn't match the text: a fact missing, or written in the wrong
    direction, or written from another fact. A carrier's row comes from FACTS, so
    a new fact fails at every carrier that doesn't write it;
  - a call of a writer has no row in CALLS, or a row has no call;
  - a line writes the road's flag and has no row in DEAD, or a row has no line;
  - a writer isn't in exactly one of FLAG (it sets, copies or reads the flag),
    BLIND (it joins or carries facts between two roads and never touches the
    flag) and ONE_ROAD (it writes one binding on one road), or the text disagrees;
  - a call of a BLIND writer has no fourth field saying why the flag needn't be
    read there, or its destination is a road kept on the walker (a frame's
    `break`s or `continue`s, a pick's `give`s or `fall`s): those are joined by
    `state_merge` alone. A pair of blind carriers stood at a `give` and kept the
    first road's flag whatever arrived after it (H-A, landing 177d2b2);
  - a line names a road kept on the walker, or calls a function that returns a
    road, and has no row in CALLS or KEPT;
  - the state's type is named outside bindings.npk;
  - its controls stop failing: the reader runs over edited copies of the text
    that must each fail, so a reader gone blind fails too.

ITS REACH: it reads text. It sees which facts a writer writes and which call
sites exist, and it pins each site's arguments, so a site that changes has to be
read again. It doesn't see whether a direction is the right one for a site, which
is what the rows say and the cells in tests/ measure, or whether a road reaches
the join it should: where a `break`, a `continue`, a `give` or a `fall` takes its
road is read by the cells too. It doesn't follow a road through a local either:
a state copied into a local and joined from there is read by the row's reason.
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


# What the struct holds per ROAD rather than per binding: no direction, since a join
# decides it from both sides (`state_merge`), and every line that writes it is in DEAD.
ROAD = {
    "dead": "the road left: a leaver ran, or every road into a join had; a join takes the other side whole",
}

# The roads the walker keeps between statements, and joins as jumps arrive.
WALKER_ROADS = ("give_road", "fall_road", "lp_brk", "lp_cont")


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
    "state_merge": (merge, "Two roads meeting: an `if`'s arms, a loop's head and exit, a frame's `break`s and "
                           "`continue`s, a pick's arms, `give`s and `fall`s: all eight, each in its direction."),
    "state_absorb_may": (
        lambda f: "||" if FACTS[f][0] == FORBID else None,
        "The four that permit: a path that may not run can't make one true, and clearing "
        "one is `state_merge`'s wherever roads meet and `state_take_definite`'s at a total "
        "pick. This alone was DEF-520's guard and DEF-522's loop."),
    "state_carry_may": (
        lambda f: "||" if f == "may" else None,
        "The other seven wait for the block's end, since the code after a `defer` in its block "
        "runs before the body: `defer_bodies_land` walks the body on the state, last first, then."),
    "state_intersect_must": (
        lambda f: None if f == "may" else merge(f),
        "A cap, not a join: `dst` keeps a permit only if `other` has it too, and takes back what `other` forbids "
        "and a clause's assignment cleared. `may`: nothing clears it, so there is nothing to take back, and each "
        "clause's road carries its own."),
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

# How each writer treats the road's flag. FLAG: it sets, copies or reads it. BLIND: it
# joins or carries facts between two roads and never touches the flag, so each call says
# in a fourth field why the flag needn't be read there. ONE_ROAD: one binding, one road.
FLAG = {
    "state_new": "sets it false: a fresh state is a road that runs",
    "state_copy": "copies it",
    "state_overwrite": "copies it",
    "state_merge": "reads both sides' and writes the join's: a side that left gives the other whole",
}
BLIND = ("state_absorb_may", "state_carry_may", "state_take_definite", "state_intersect_must")
ONE_ROAD = ("state_mark", "state_mark_checked", "state_mark_errchecked", "state_mark_moved", "state_mark_freed",
            "state_mark_sealed", "state_clear_refinements")

# A row above whose reason doesn't settle it: what's left open, under which defect.
# A row here must still match the text, and it isn't read as right.
OPEN = {
    "state_clear_refinements": ("DEF-523", "the body is walked where it's written, so a page sealed or "
                                           "re-assigned before its run gets past"),
    "state_take_definite": ("DEF-524", "`mayacc` takes the arms that leave, so `pick` refuses what `if` accepts"),
}

# Every call of a writer: the function, the code on the line, and what it's for. A call of a
# BLIND writer has a fourth field: why the road's flag needn't be read there.
CALLS = [
    # --- a frame
    ("state_copy", "AssignState:d = raw state_new(src.count);", "a copy's own arrays"),
    ("assign_decl", "AssignState:st = raw state_new(raw symtab_fn_slots(x.tab, id.index));", "the frame's state"),
    ("assign_decl", "drop state_mark(st, ps);", "a parameter is assigned"),
    ("assign_decl", "AssignState:pre = raw state_new(raw symtab_fn_slots(x.tab, id.index));",
     "the pre-walk's one shared state: it collects addresses, and nothing reads its facts or its flag"),
    ("assign_init", "give_road: raw state_new(1i32), give_seen: false,", "a placeholder: `give_seen` says no arm's `give` has written it yet"),
    ("assign_init", "fall_road: raw state_new(1i32), fall_seen: false,", "a placeholder: `fall_seen` says no `fall` of the pick being walked has written it yet"),
    # --- an `if`
    ("state_merge", "drop state_overwrite(dst, b);", "the first side left: all of the second, its flag with it"),
    ("state_merge", "drop state_overwrite(dst, a);", "the second side left: all of the first, its flag with it"),
    ("assign_stmt", "AssignState:a = raw state_copy(st);", "the then arm's start"),
    ("assign_stmt", "if (et.then_checked) { drop state_mark_checked(a, et.slot); }", "the then arm's road"),
    ("assign_stmt", "else                 { drop state_mark_errchecked(a, et.slot); }", "the then arm's road"),
    ("assign_stmt", "AssignState:b = raw state_copy(st);", "the else arm's start"),
    ("assign_stmt", "if (!et.then_checked) { drop state_mark_checked(b, et.slot); }", "the else arm's road"),
    ("assign_stmt", "else                  { drop state_mark_errchecked(b, et.slot); }", "the else arm's road"),
    ("assign_stmt", "drop state_merge(st, a, false, b, false);",
     "all eight, in direction; an arm that left is dead, and the join takes the other whole"),
    # --- a `when`: `then` from the loop's exit, `end` from the condition's false road; exactly one of them runs (D-027),
    # and neither grants a permit (DECISIONS.md:9069)
    ("assign_stmt", "AssignState:ex = raw state_copy(st);", "the loop's exit before `then` and `end`, kept to cap them with"),
    ("assign_stmt", "drop state_merge(st, st, false, nf, false);", "`then`'s end meets `end`'s, all eight"),
    ("assign_stmt", "drop state_intersect_must(st, ex);",
     "`then` and `end` grant nothing: the permits keep only what held at the loop's exit, the forbids the union "
     "(DECISIONS.md:9069)",
     "`st`'s flag is the join's on the line above it, and a cap changes no road: where both clauses left, `st` is "
     "dead and stays so, and `ex` is the exit they started from, not a road arriving here"),
    # --- a declaration
    ("assign_stmt", "drop state_mark(st, raw symtab_stmt_ord(x.tab, id.index));", "a local with a value"),
    # --- a `defer`
    ("assign_stmt", "AssignState:dv = raw state_copy(st);",
     "the body's own road: nothing comes back, since the code after it runs first"),
    ("assign_stmt", "drop state_clear_refinements(dv);", "what was earned above it is stale at the exit"),
    ("assign_stmt", "drop state_carry_may(st, dv);", "a body's write of a `fixed` is a write, wherever the next stands",
     "`st` is the road the `defer` stands on and keeps its own flag: the body runs at the block's exit, not here, so "
     "whether the body's road ends says nothing about this one"),
    ("assign_lazy_operands", "AssignState:a = raw state_copy(st);", "an operand that may not run walks on its own state (DEF-527)"),
    ("assign_lazy_operands", "drop state_merge(st, a, false, st, false);", "an `if`'s merge; `st` is `is`'s other arm, or the state before an operand with no twin"),
    # --- a jump: its own road, through the `defer`s of the blocks it leaves, to where the emitter branches
    ("road_out", "AssignState:road = raw state_copy(st);",
     "the jump's own copy, which the `defer` bodies it runs are walked on; the walk's road is dead after the jump"),
    ("leave_to", "drop state_merge(x.lp_brk[f], x.lp_brk[f], false, road, false);",
     "a `break`'s road meets the loop's other `break`s, at its exit"),
    ("leave_to", "drop state_merge(x.lp_cont[f], x.lp_cont[f], false, road, false);",
     "a `continue`'s road meets the body's end and the other `continue`s, at its head"),
    ("leave_arm", "if (x.give_seen) { drop state_merge(x.give_road, x.give_road, false, road, false); }",
     "a later `give`'s road meets the arm's earlier ones (DEF-526, DEF-535); a first road a `defer` body ended is dead, "
     "and this is the writer that knows (H-A)"),
    ("leave_arm", "if (x.fall_seen) { drop state_merge(x.fall_road, x.fall_road, false, road, false); }",
     "a later `fall`'s road meets the pick's earlier ones (DEF-437)"),
    # --- a loop: S on entry, back(X) the body's end and every `continue` walked from X, the head H = S met with back(S)
    ("loop_trip", "AssignState:b = raw state_copy(h);", "the body starts on the condition's true road; `h` keeps the false road"),
    ("loop_trip", "drop state_merge(back, back, false, b, false);", "the body's end meets the `continue`s: what reaches the head again"),
    ("assign_loop", "AssignState:back = raw state_new(st.count);", "back(S), dead until a road arrives"),
    ("assign_loop", "AssignState:brk = raw state_new(st.count);", "the `break`s, dead until one arrives"),
    ("assign_loop", "AssignState:h = raw state_copy(st);", "the silent trip's head: S"),
    ("assign_loop", "drop state_merge(st, st, false, back, false);", "the head's fixed point H: S met with back(S)"),
    ("assign_loop", "if (!(h.dead[0i64]))   { drop state_merge(h, h, false, back, false); }",
     "a silent walk's one trip: the condition's false road from S, met with back(S), covers the one from H (DEF-281); "
     "a false road that isn't there stays so"),
    ("assign_loop", "if (!(brk.dead[0i64])) { drop state_merge(brk, brk, false, back, false); }",
     "and the `break`s from S, the same"),
    ("assign_loop", "brk = raw state_new(st.count);", "the reporting trip sends its `break`s again, from H"),
    ("assign_loop", "h = raw state_copy(st);", "the reporting trip's head: H"),
    ("assign_loop", "drop state_merge(st, h, false, brk, false);", "the exit: the condition's false road meets every `break`"),
    ("widen_roads", "if ((x.lp_bgen[f] > g0) && !(x.lp_brk[f].dead[0i64])) { drop state_merge(x.lp_brk[f], x.lp_brk[f], false, back, false); }",
     "an outer loop's `break` channel, if a jump stamped it after g0: the whole channel, a road sent before this silent trip too (P-1, DEF-570), met with back(S) as the false road is; a channel dead after the merge is skipped and stays dead"),
    ("widen_roads", "if ((x.lp_cgen[f] > g0) && !(x.lp_cont[f].dead[0i64])) { drop state_merge(x.lp_cont[f], x.lp_cont[f], false, back, false); }",
     "an outer loop's `continue` channel, the same"),
    ("widen_roads", "if ((x.give_gen > g0) && !(x.give_road.dead[0i64])) { drop state_merge(x.give_road, x.give_road, false, back, false); }",
     "the innermost pick's `give` channel, the same"),
    ("widen_roads", "if ((x.fall_gen > g0) && !(x.fall_road.dead[0i64])) { drop state_merge(x.fall_road, x.fall_road, false, back, false); }",
     "the innermost pick's `fall` channel, the same"),
    # --- a `pick`
    ("assign_pick", "AssignState:acc = raw state_new(st.count);", "the arms' ends, joined: dead until one arrives"),
    ("assign_pick", "AssignState:pristine = raw state_copy(st);", "each arm's start (F-6)"),
    ("assign_pick", "AssignState:mayacc = raw state_copy(st);", "what forbids, from every arm"),
    ("assign_pick", "AssignState:one = raw state_copy(pristine);", "one arm's road"),
    ("assign_pick", "drop state_mark(one, raw symbol_slot(x, ps));", "the arm's bindings"),
    ("assign_pick", "drop state_mark_checked(one, et.slot);", "the arm's road"),
    ("assign_pick", "drop state_mark_errchecked(one, et.slot);", "the arm's road"),
    ("assign_pick", "drop state_merge(pristine, pristine, false, one, false);",
     "a guard runs whether its arm is taken or not (DEF-520), so a later arm starts from the join of what stood before "
     "it and what it did; a guard that never yields is a dead road and gives nothing"),
    ("assign_pick", "if (x.fall_seen && ((raw arm_label(arm)) != 0i32)) { drop state_merge(one, one, false, x.fall_road, false); }",
     "a labelled arm's body: its own road and every `fall`'s before it, after the refinement and the guard (DEF-437, DEF-536)"),
    ("assign_pick", "drop state_absorb_may(mayacc, one);", "the arm's prohibitions, the arm that leaves among them (DEF-524's price)",
     "`mayacc`'s flag is never read: its four forbids land on `st` after the arms, and nothing else of it does"),
    ("assign_pick", "drop state_absorb_may(mayacc, x.give_road);", "the `give` roads' forbids land once with the other arms' (F-6)",
     "as the line above; `x.give_road` is the source here, and the destination is `mayacc`"),
    ("assign_pick", "drop state_merge(acc, acc, false, one, false);",
     "an arm's end, or its `give` roads, meets the earlier arms'; an arm that left is dead and gives nothing"),
    ("assign_pick", "drop state_take_definite(st, acc);", "the four that permit, for a total pick",
     "asked on the line before it: it runs only where `acc` is live. `st` keeps the flag it came with, so a pick on a "
     "dead road ends dead whatever its arms did (F2)"),
    ("assign_pick", "drop state_absorb_may(st, mayacc);", "the four that forbid, once, after every arm",
     "`st`'s flag was settled on the lines above it, by `acc`'s"),
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

# Every other line that names a road kept on the walker, or calls a function that returns a road. Each hands a
# state on by reference, flag and all: no copy is made and nothing is joined.
KEPT = [
    ("assign_stmt", "discard(raw assign_loop(x, st, s, raw stmt_body(x.ast, s), raw stmt_cond(s), d));",
     "a `while`: its exit is in `st`, and the false road it returns is `when`'s alone"),
    ("assign_stmt", "discard(raw assign_loop(x, st, s, raw stmt_for_body(x.ast, s), raw expr_none(), d));", "a `for`, the same"),
    ("assign_counted", "discard(raw assign_loop(x, st, s, raw stmt_counted_body(s), raw expr_none(), depth));", "a counted loop, the same"),
    ("assign_stmt", "AssignState:nf = raw assign_loop(x, st, s, raw stmt_when_body(x.ast, s), raw stmt_cond(s), d);",
     "a `when`: the condition's false road, where `end` starts"),
    ("leave_to", "AssignState:road = raw road_out(x, st, x.lp_dh[f], depth, sp);",
     "a `break`'s or a `continue`'s road, down to its loop's frame; the span is where a body cut at the depth bound on "
     "that road is refused (ASSIGN-003)"),
    ("leave_arm", "AssignState:road = raw road_out(x, st, x.pick_dh, depth, sp);", "a `give`'s or a `fall`'s road, down to its pick; the span, the same"),
    ("leave_arm", "else             { x.give_road = road; x.give_seen = true; }",
     "the arm's first `give`: the road itself, dead if a `defer` body ended it"),
    ("leave_arm", "else             { x.fall_road = road; x.fall_seen = true; }", "the pick's first `fall`, the same"),
    ("loop_trip", "drop list_push(@x.lp_brk, brk);", "the frame's `break` entry is the caller's `brk`"),
    ("loop_trip", "drop list_push(@x.lp_cont, back);", "the frame's `continue` entry is the caller's `back`"),
    ("loop_trip", "drop list_truncate(@x.lp_brk, n);", "the frame ends; the caller holds the state"),
    ("loop_trip", "drop list_truncate(@x.lp_cont, n);", "the same"),
    ("assign_pick", "AssignState:outer_road = x.give_road;", "a pick in an arm has its own `give`s: the outer arm's road is set aside"),
    ("assign_pick", "AssignState:outer_fall = x.fall_road;", "and its own `fall`s (DEF-437)"),
    ("assign_pick", "one = x.give_road;",
     "an arm that gave ends in its `give` roads: its own end is a road that left, or FLOW-001's. A `give` road a "
     "`defer` body ended is dead, and the join below takes none of it"),
    ("assign_pick", "x.give_road = outer_road;", "the outer arm's road, back"),
    ("assign_pick", "x.fall_road = outer_fall;", "the outer pick's road, back"),
]

# Every line that writes the road's flag, and why. A `true` drops the road at the next join, so it stands only where
# the road has really left; a `false` on a road that can't arrive refuses more, never less. Read with the code, as
# CALLS is: a line that changes has to be read again.
DEAD = [
    ("state_new", "s.dead[0i64] = false;", "a fresh state is a road that runs"),
    ("state_copy", "d.dead[0i64] = src.dead[0i64];", "a copy of a road that left has left too"),
    ("state_overwrite", "dst.dead[0i64] = src.dead[0i64];", "the other side whole, its flag with it"),
    ("state_merge", "dst.dead[0i64] = true;", "both sides left: nothing reaches the join"),
    ("state_merge", "dst.dead[0i64] = false;", "both sides arrive"),
    ("assign_stmt", "st.dead[0i64] = true;", "a `pass`, after its operand"),
    ("assign_stmt", "if (s.kind == StmtKind.StmtFailStmt)   { drop assign_expr(x, st, raw stmt_operand(s), d); st.dead[0i64] = true; pass NIL; }",
     "a `fail`, after its operand"),
    ("assign_stmt", "if (s.kind == StmtKind.StmtReturnStmt) { drop assign_expr(x, st, raw stmt_operand(s), d); st.dead[0i64] = true; pass NIL; }",
     "a `return`, after its operand"),
    ("assign_stmt", "if (s.kind == StmtKind.StmtExitStmt)   { drop assign_expr(x, st, raw stmt_expr(s), d); st.dead[0i64] = true; pass NIL; }",
     "an `exit`, after its operand"),
    ("assign_stmt", "if ((s.kind == StmtKind.StmtTrapStmt) || (raw stmt_is_unreachable(x, s))) { st.dead[0i64] = true; }",
     "a trap and a bare `#unreachable();` (DEF-225)"),
    ("leave_to", "st.dead[0i64] = true;", "a `break` or a `continue`, after its road went to its frame"),
    ("leave_arm", "st.dead[0i64] = true;", "a `give` or a `fall`, after its road went to its pick"),
    ("loop_trip", "if (raw cond_is_true(x, cond)) { h.dead[0i64] = true; }", "a literal `true` condition has no false road"),
    ("assign_loop", "back.dead[0i64] = true;", "back(S) before a road arrives"),
    ("assign_loop", "brk.dead[0i64] = true;", "the `break`s before one arrives"),
    ("assign_loop", "brk.dead[0i64] = true;", "the reporting trip's `break`s, the same"),
    ("assign_pick", "acc.dead[0i64] = true;", "the arms' ends before one arrives"),
    ("assign_pick", "if (valued && st.dead[0i64]) { pristine.dead[0i64] = false; x.lp_floor = x.lp_label.count; }",
     "a value pick's arms start live on a road that left, so FLOW-001 asks the arm's text (F2); the floor keeps what "
     "they send from every loop around the pick (P-B), and `st` keeps its own flag"),
    ("assign_pick", "st.dead[0i64] = true;", "a total pick whose every arm left: nothing reaches its end"),
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


def road_returners(text):
    """The functions that return a state and aren't writers: a call of one hands a road on."""
    return [f for f in re.findall(r"^(?:pub )?func:(\w+) = AssignState\(", text, re.M) if f not in WRITES]


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
        if f not in FACTS and f not in ROAD:
            fails.append("the struct holds `%s`, and FACTS gives it no direction (nor is it in ROAD)" % f)
    for f in ROAD:
        if f not in fields:
            fails.append("ROAD names `%s`, and the struct doesn't hold it" % f)
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
    table = collections.Counter((row[0], row[1]) for row in CALLS)
    for k in found - table:
        fails.append("%s: `%s` has no row in CALLS" % k)
    for k in table - found:
        fails.append("%s: CALLS has `%s`, and the text doesn't" % k)

    # the road's flag: every line that writes it
    road = re.compile(r"\b\w+\.(%s)\[[^\]]*\]\s*=(?!=)" % "|".join(ROAD))
    dfound = collections.Counter((fn, code) for fn, code in code_lines(text) if road.search(code))
    dtable = collections.Counter((fn, code) for fn, code, _ in DEAD)
    for k in dfound - dtable:
        fails.append("%s: `%s` writes the road's flag and has no row in DEAD" % k)
    for k in dtable - dfound:
        fails.append("%s: DEAD has `%s`, and the text doesn't" % k)

    # how each writer treats it
    touches = collections.Counter(fn for fn, code in code_lines(text) if re.search(r"\.(%s)\[" % "|".join(ROAD), code))
    for fn in WRITES:
        homes = [n for n, t in (("FLAG", FLAG), ("BLIND", BLIND), ("ONE_ROAD", ONE_ROAD)) if fn in t]
        if len(homes) != 1:
            fails.append("%s is in %s: a writer is in exactly one of FLAG, BLIND and ONE_ROAD" % (fn, homes or "none"))
        elif (homes[0] == "FLAG") != (touches[fn] > 0):
            fails.append("%s is in %s, and the text %s the road's flag" % (fn, homes[0], "touches" if touches[fn] else "doesn't touch"))
    blind = re.compile(r"\b(%s)\(\s*([^,)]*)" % "|".join(BLIND))
    for row in CALLS:
        m = blind.search(row[1])
        if bool(m) != (len(row) == 4):
            fails.append("%s: `%s` %s" % (row[0], row[1], "calls a BLIND writer and doesn't say why the flag needn't be read"
                                          if m else "has a fourth field and calls no BLIND writer"))
    for fn, code in code_lines(text):
        m = blind.search(code)
        if m and not code.startswith(("func:", "pub func:")) and re.match(r"x\.", m.group(2).strip()):
            fails.append("%s: `%s` carries into a road kept on the walker, and those are joined by `state_merge` alone" % (fn, code))

    # every other line that keeps a road or hands one on
    keeps = re.compile(r"\bx\.(%s)\b|\b(%s)\(" % ("|".join(WALKER_ROADS), "|".join(road_returners(text)) or "<none>"))
    kfound = collections.Counter((fn, code) for fn, code in code_lines(text)
                                 if keeps.search(code) and (fn, code) not in found and not code.startswith(("func:", "pub func:")))
    ktable = collections.Counter((fn, code) for fn, code, _ in KEPT)
    for k in kfound - ktable:
        fails.append("%s: `%s` keeps a road or hands one on, and has no row in KEPT" % k)
    for k in ktable - kfound:
        fails.append("%s: KEPT has `%s`, and the text doesn't" % k)
    return fails


# Each must make `check` fail, whatever the tree says: a check that can't fail
# proves nothing, and each of these is a road a change has really taken or could.
CONTROLS = [
    ("a new fact in the struct", "    wild bool->:must;\n", "    wild bool->:must;\n    wild bool->:newfact;\n"),
    ("`state_take_definite` loses `sealed_firm`", "        // sealed after the pick, and the call went to writable bytes unsealed.\n        dst.sealed_firm[i] = src.sealed_firm[i];\n", ""),
    ("`state_merge` unions a permission", "dst.checked[i] = a.checked[i] && b.checked[i];", "dst.checked[i] = a.checked[i] || b.checked[i];"),
    ("a merge writes one fact from another", "dst.freed[i] = a.freed[i] || b.freed[i];", "dst.freed[i] = a.moved[i] || b.moved[i];"),
    ("a loop's head merge goes", "    drop state_merge(st, st, false, back, false);\n", ""),
    ("a new call of a carrier", "    drop state_absorb_may(st, mayacc);\n", "    drop state_absorb_may(st, mayacc);\n    drop state_absorb_may(st, mayacc);\n"),
    ("a new writer", "func:state_copy =", "func:state_sneak = NIL(AssignState:s) never fails { s.moved[0i64] = false; pass NIL; };\nfunc:state_copy ="),
    ("a new write of the road's flag", "    s.dead[0i64] = false;\n", "    s.dead[0i64] = false;\n    s.dead[0i64] = true;\n",
     "writes the road's flag and has no row in DEAD"),
    ("a carrier drops the road's flag", "    dst.dead[0i64] = src.dead[0i64];\n", "", "the text doesn't touch the road's flag"),
    ("a carrier joins a road kept on the walker (H-A)",
     "            if (x.give_seen) { drop state_merge(x.give_road, x.give_road, false, road, false); }\n",
     "            if (x.give_seen) { drop state_absorb_may(x.give_road, road); }\n",
     "carries into a road kept on the walker"),
    ("a road is handed on with no row", "            one = x.give_road;\n", "            one = x.give_road;\n            one = x.fall_road;\n",
     "keeps a road or hands one on, and has no row in KEPT"),
    ("a new function returns a road", "func:cond_is_true =",
     "func:road_sneak = AssignState(AssignState:s) never fails { pass s; };\nfunc:use_sneak = NIL(AssignState:s) never fails {\n    AssignState:t = raw road_sneak(s);\n    pass NIL;\n};\nfunc:cond_is_true =",
     "keeps a road or hands one on, and has no row in KEPT"),
]


def controls(text):
    """The controls this tree can run: each edit applies once, and the check then fails."""
    out = []
    for row in CONTROLS:
        name, a, b = row[:3]
        if text.count(a) != 1:
            out.append("control `%s` no longer applies: its text is there %d times" % (name, text.count(a)))
            continue
        got = check(text.replace(a, b))
        if not got:
            out.append("control `%s` didn't fail, so this reader has gone blind" % name)
        elif len(row) == 4 and not any(row[3] in g for g in got):
            out.append("control `%s` failed, and not by the rule it's there for (`%s`)" % (name, row[3]))
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
    for row in CALLS:
        print("  %-18s %-72s %s" % row[:3])
    print("\nthe road's flag: %d writes" % len(DEAD))
    for fn, code, why in DEAD:
        print("  %-18s %-72s %s" % (fn, code, why))
    print("\nthe roads: every line that copies, joins, carries or hands on a state, and what it does with the road's flag")
    joiner = re.compile(r"\b(%s)\(" % "|".join(list(FLAG) + list(BLIND)))
    for row in CALLS:
        m = joiner.search(row[1])
        if m:
            how = FLAG[m.group(1)] if m.group(1) in FLAG else "NOT READ: " + row[3]
            print("  %-18s %s\n      %s. %s." % (row[0], row[1], row[2], how[0].upper() + how[1:] if m.group(1) in FLAG else how))
    for fn, code, why in KEPT:
        print("  %-18s %s\n      %s. The same state, by reference: its flag with it." % (fn, code, why))


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
        print("\nassign-fields: %d failure(s), %d facts, %d writers, %d calls, %d kept, %d flag writes, %d controls"
              % (len(fails), len(FACTS), len(WRITES), len(CALLS), len(KEPT), len(DEAD), len(CONTROLS)))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
