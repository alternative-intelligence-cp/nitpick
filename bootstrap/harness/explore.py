"""The schedule explorer's belts -- the harness's side (1.5.7; D-212, X-1, X-9
in meta/roadmap/1.5/1.5.7.md).

THE EXPLORED FLOOR IS THE REAL FLOOR, TRANSFORMED: `npkg/explore.npk` is the ONE
transformer (a scheduling point before every atomic step line, every
`@npk_sys6(` call routed to the shim, the thread lifecycle hooked), built into
`tools/explored.npk` with the snapshot and run by the harness. This module has
no transformer of its own, on purpose -- 1.5.6b's lesson was a model with one
reader, and the answer there was a second implementation because the semantics
were rich; here the property that matters is an EQUALITY OF COUNTS, so the
second reader counts: the step lines of the floor by the model belt's own
definition (`floor._STEP_LINE`), the points and routed calls of the transformed
text, and every atomic step of the output with its point immediately before it.
`npkg/explore.npk`'s `explore_totality` is the twin, finding for finding.

    explore-step-escapes   a step line of the floor with no point or route in
                           the explored floor, a point that precedes no step,
                           or a `@npk_sys6(` call left unrouted
"""

import re

import floor

_POINT_RE = re.compile(r"^call void @npkx_point\(i32 (\d+)\)$")
_ATOMIC_RE = re.compile(r"\b(atomicrmw|cmpxchg|fence)\b|\b(load|store) atomic\b")


def _bodies(text):
    """(function, code line) for every line inside a define's body -- the
    code part, comment stripped, strings blanked; a define's own header line
    and everything outside a body are not yielded."""
    fn = None
    out = []
    for raw in text.split("\n"):
        code = floor.code_part(raw)
        if fn is None:
            m = floor._DEFINE_RE.match(code)
            if m:
                fn = m.group(2).strip('"')
            continue
        if code == "}":
            fn = None
            continue
        out.append((fn, code))
    return out


def census(floor_text):
    """The step lines of the floor: `line_is_step`'s definition (the model
    belt's), over the bodies of its defines."""
    return sum(1 for _, code in _bodies(floor_text) if floor._STEP_LINE.search(code))


def _is_sys6_call(code):
    return "@npk_sys6(" in code and "call " in code


def check_totality(floor_text, explored_text, name="explore"):
    """THE TOTALITY BELT (the twin of `explore_totality`): the floor's step
    count equals the explored floor's points plus routed calls, no atomic step
    of the output lacks the point immediately before it, and no `@npk_sys6(`
    call is left unrouted. Findings by name; none when it holds."""
    fails = []
    n = census(floor_text)
    points = routed = 0
    prev = ""
    last_fn = None
    for fn, code in _bodies(explored_text):
        if fn != last_fn:
            prev = ""
            last_fn = fn
        if _POINT_RE.match(code):
            points += 1
            prev = code
            continue
        if "@npkx_sys6(" in code and "call " in code:
            routed += 1
            prev = code
            continue
        if _is_sys6_call(code):
            fails.append("%s-step-escapes: a `@npk_sys6(` call the transform did not route, in %s: %s" % (name, fn, code[:100]))
        if _ATOMIC_RE.search(code) and not _POINT_RE.match(prev):
            fails.append("%s-step-escapes: an atomic step with no scheduling point before it, in %s: %s" % (name, fn, code[:100]))
        prev = code
    if points + routed != n:
        fails.append("%s-step-escapes: the floor holds %d step line(s) and the explored floor %d point(s) plus %d routed call(s) -- "
                     "a step escaped the transform, or a point precedes no step" % (name, n, points, routed))
    return fails


def check_sites(explored_text, sites_text, name="explore"):
    """The site map agrees with the text: one `N<TAB>function<TAB>kind<TAB>text`
    line per point or routed call, the points numbered by their sites in
    order, every `atomic` site a point and every `sys6` site a routed call."""
    fails = []
    rows = [l.split("\t") for l in sites_text.split("\n") if l.strip()]
    for i, r in enumerate(rows):
        if len(r) != 4 or r[0] != str(i) or r[2] not in ("atomic", "sys6"):
            return ["%s: sites.txt line %d is not `%d<TAB>function<TAB>atomic|sys6<TAB>text`" % (name, i + 1, i)]
    atomic_sites = [int(r[0]) for r in rows if r[2] == "atomic"]
    points = [int(m.group(1)) for _, code in _bodies(explored_text) for m in [_POINT_RE.match(code)] if m]
    if points != atomic_sites:
        fails.append("%s: the points of the explored floor (%d) are not the `atomic` sites of sites.txt (%d), in order"
                     % (name, len(points), len(atomic_sites)))
    routed = sum(1 for _, code in _bodies(explored_text) if "@npkx_sys6(" in code and "call " in code)
    sys6_sites = sum(1 for r in rows if r[2] == "sys6")
    if routed != sys6_sites:
        fails.append("%s: the explored floor routes %d call(s) and sites.txt names %d `sys6` site(s)" % (name, routed, sys6_sites))
    return fails
