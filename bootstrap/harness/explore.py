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
    explore-oracle-offsets the shim's three struct offsets (step 3) are not
                           what the floor's `%npk.exec`/`%npk.hdr` types say
    explore-control-*      a control file the grammar cannot read, whose `old`
                           lines do not occur exactly once, or whose verdict the
                           explorer never reaches (`-blind`)
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


# --- step 3: the oracle's offsets and the controls ------------------------------------


_TYPE_RE = re.compile(r"^(%[\w.]+) = type (\{.*\})\s*$", re.M)
_SHIM_OFF_RE = re.compile(r"^@npkx_off_(sl_head|qnext|wake_at) = internal constant i64 (\d+)\s*$", re.M)


def _split_fields(body):
    """The top-level fields of `{ a, b, ... }`, brackets kept whole."""
    inner = body.strip()[1:-1]
    out, depth, cur = [], 0, []
    for ch in inner:
        if ch in "{[":
            depth += 1
        elif ch in "}]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if "".join(cur).strip():
        out.append("".join(cur).strip())
    return out


def type_size_align(ty):
    """(size, align) of an LLVM type on x86_64: `iN`, `ptr`, `[N x T]`, `{ ... }` -- the shapes the floor's structs use."""
    ty = ty.strip()
    if ty == "ptr":
        return 8, 8
    m = re.match(r"^i(\d+)$", ty)
    if m:
        bits = int(m.group(1))
        n = max(1, (bits + 7) // 8)
        return n, min(n, 8) if n & (n - 1) == 0 else 8
    m = re.match(r"^\[(\d+) x (.+)\]$", ty)
    if m:
        s, a = type_size_align(m.group(2))
        return int(m.group(1)) * s, a
    if ty.startswith("{"):
        off, align = 0, 1
        for f in _split_fields(ty):
            s, a = type_size_align(f)
            off = (off + a - 1) // a * a + s
            align = max(align, a)
        return (off + align - 1) // align * align, align
    raise ValueError("explore: a type the layout reader does not know: %r" % ty)


def struct_offsets(body):
    """The byte offset of every field of a struct body `{ ... }`."""
    off, out = 0, []
    for f in _split_fields(body):
        s, a = type_size_align(f)
        off = (off + a - 1) // a * a
        out.append(off)
        off += s
    return out


def check_oracle_offsets(floor_text, shim_text, name="explore"):
    """THE ORACLE'S OFFSETS (step 3; D-301): the shim reads three words of the
    floor's structs -- `%npk.exec` field 2 (the sleeper list's head), `%npk.hdr`
    fields 7 (`qnext`) and 8 (`wake_at`) -- by byte offsets it carries as named
    constants; this holds them to the floor's own type lines
    (`explore-oracle-offsets`)."""
    types = {m.group(1): m.group(2) for m in _TYPE_RE.finditer(floor_text)}
    have = {m.group(1): int(m.group(2)) for m in _SHIM_OFF_RE.finditer(shim_text)}
    fails = []
    for key in ("sl_head", "qnext", "wake_at"):
        if key not in have:
            fails.append("%s-oracle-offsets: the shim carries no `@npkx_off_%s` constant" % (name, key))
    if fails:
        return fails
    for tname in ("%npk.exec", "%npk.hdr"):
        if tname not in types:
            return ["%s-oracle-offsets: the floor defines no `%s` type" % (name, tname)]
    want = {"sl_head": struct_offsets(types["%npk.exec"])[2],
            "qnext": struct_offsets(types["%npk.hdr"])[7],
            "wake_at": struct_offsets(types["%npk.hdr"])[8]}
    for key in ("sl_head", "qnext", "wake_at"):
        if have[key] != want[key]:
            fails.append("%s-oracle-offsets: the shim reads `%s` at byte %d and the floor's type puts it at %d"
                         % (name, key, have[key], want[key]))
    return fails


def read_control(text, name="explore"):
    """A control file (`runtime/explore/controls/<name>.ctl`): `program:`,
    `verdict:`, `within:` and the `old:`/`new:` line blocks; (control, reason)."""
    ctl = {"program": "", "verdict": "", "within": 0, "old": [], "new": []}
    block = None
    for raw in text.split("\n"):
        if raw.startswith(";"):
            continue
        if raw in ("old:", "new:"):
            block = raw[:-1]
            continue
        if block is not None and (raw.startswith("  ") or raw == ""):
            if raw:
                ctl[block].append(raw)
            continue
        block = None
        if not raw.strip():
            continue
        key, _, val = raw.partition(":")
        key, val = key.strip(), val.strip()
        if key == "within":
            try:
                ctl["within"] = int(val)
            except ValueError:
                return None, "%s: `within:` is not a number: %r" % (name, val)
        elif key in ("program", "verdict"):
            ctl[key] = val
        else:
            return None, "%s: a line the control grammar does not know: %r" % (name, raw[:80])
    for key in ("program", "verdict"):
        if not ctl[key]:
            return None, "%s: no `%s:`" % (name, key)
    if ctl["within"] < 1:
        return None, "%s: `within:` must be at least 1" % name
    if not ctl["old"] or not ctl["new"]:
        return None, "%s: `old:` and `new:` must each hold at least one line" % name
    return ctl, ""


def apply_control(floor_text, ctl, name="explore"):
    """The floor with the control's `old` lines replaced by its `new` lines --
    exactly one occurrence, or the control is refused by name."""
    old = "\n".join(ctl["old"]) + "\n"
    n = floor_text.count(old)
    if n != 1:
        return None, ("%s: explore-control-unmatched: the control's `old` lines occur %d time(s) in runtime/npkrt.ll, not once" % (name, n))
    return floor_text.replace(old, "\n".join(ctl["new"]) + "\n"), ""
