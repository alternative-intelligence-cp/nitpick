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


def check_totality(floor_text, explored_text, name="explore", what="floor"):
    """THE TOTALITY BELT (the twin of `explore_totality`): the floor's step
    count equals the explored floor's points plus routed calls, no atomic step
    of the output lacks the point immediately before it, and no `@npk_sys6(`
    call is left unrouted. Findings by name; none when it holds. `what` names
    the module in the count's finding -- "program" for a unit's own IR (step 6,
    the twin of `explore_totality_of`)."""
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
        fails.append("%s-step-escapes: the %s holds %d step line(s) and the explored %s %d point(s) plus %d routed call(s) -- "
                     "a step escaped the transform, or a point precedes no step" % (name, what, n, what, points, routed))
    return fails


def check_sites(explored_text, sites_text, name="explore", base=0):
    """The site map agrees with the text: one `N<TAB>function<TAB>kind<TAB>text`
    line per point or routed call, the points numbered by their sites in
    order, every `atomic` site a point and every `sys6` site a routed call.
    A unit's own sites are numbered from `base` (step 6: 1,000,000, the
    transformer's `PROGRAM_SITE_BASE`)."""
    fails = []
    rows = [l.split("\t") for l in sites_text.split("\n") if l.strip()]
    for i, r in enumerate(rows):
        if len(r) != 4 or r[0] != str(base + i) or r[2] not in ("atomic", "sys6"):
            return ["%s: sites.txt line %d is not `%d<TAB>function<TAB>atomic|sys6<TAB>text`" % (name, i + 1, base + i)]
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
    `verdict:`, `within:` and one or more `old:`/`new:` pairs of line blocks
    (step 4: a control may mutate several places; each pair is applied in
    order and each `old` must occur exactly once). The verdict is a word of
    the shim (`DEADLOCK`, `LOST-WAKE`, ...), the program's own answer --
    `wrong-exit` (any exit other than its `expect-exit:`, a signal included,
    or any shim verdict) or `exit N` (that exit exactly) -- or its LATENESS:
    `late N`, the virtual run time at or past N nanoseconds (the shim's
    `vrun=`; the class a lost wakeup degrades to under D-301, which no exit
    code and no stamp oracle can see). A DIRECTED control (X-15) adds up to
    four `preempt-at: @function <instruction text prefix>` lines, each naming
    one atomic site of the explored floor at which every arriving thread is
    demoted below every other -- a change point at a place, for a window one
    step wide that blind PCT cannot land on; the runners resolve each against
    the patched floor's sites.txt (exactly one `atomic` row, or the control is
    refused by name). (control, reason)."""
    ctl = {"program": "", "verdict": "", "within": 0, "subs": [], "spec_subs": [], "prog_subs": [], "preempt": []}
    block = None
    target = "subs"
    # the three pair kinds: the floor's `old:`/`new:`, a SPEC control's (step 5, X-18) and a PROGRAM
    # control's (step 6, X-21: a defect planted in the named program's source)
    kinds = {"old:": "subs", "spec-old:": "spec_subs", "program-old:": "prog_subs"}
    news = {"new:": "subs", "spec-new:": "spec_subs", "program-new:": "prog_subs"}
    for raw in text.split("\n"):
        if raw.startswith(";"):
            continue
        if raw in kinds:
            target = kinds[raw]
            ctl[target].append(([], []))
            block = 0
            continue
        if raw in news:
            want = news[raw]
            if want != target or not ctl[target] or ctl[target][-1][1]:
                return None, "%s: a `%s` with no `%s` before it" % (name, raw, raw[:-len("new:")] + "old:")
            block = 1
            continue
        if block is not None and (raw.startswith("  ") or raw == ""):
            if raw:
                ctl[target][-1][block].append(raw)
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
        elif key == "preempt-at":
            fn, _, prefix = val.partition(" ")
            if not fn.startswith("@") or not prefix.strip():
                return None, "%s: `preempt-at:` is `@function <instruction text prefix>`: %r" % (name, val)
            ctl["preempt"].append((fn, prefix.strip()))
        else:
            return None, "%s: a line the control grammar does not know: %r" % (name, raw[:80])
    for key in ("program", "verdict"):
        if not ctl[key]:
            return None, "%s: no `%s:`" % (name, key)
    if ctl["within"] < 1:
        return None, "%s: `within:` must be at least 1" % name
    if not ctl["subs"] and not ctl["spec_subs"] and not ctl["prog_subs"]:
        return None, "%s: no `old:`/`new:` pair, no `spec-old:`/`spec-new:` pair and no `program-old:`/`program-new:` pair" % name
    for i, (old, new) in enumerate(ctl["subs"]):
        if not old or not new:
            return None, "%s: `old:` and `new:` must each hold at least one line (pair %d)" % (name, i + 1)
    for i, (old, new) in enumerate(ctl["spec_subs"]):
        if not old or not new:
            return None, "%s: `spec-old:` and `spec-new:` must each hold at least one line (pair %d)" % (name, i + 1)
    for i, (old, new) in enumerate(ctl["prog_subs"]):
        if not old or not new:
            return None, "%s: `program-old:` and `program-new:` must each hold at least one line (pair %d)" % (name, i + 1)
    if len(ctl["preempt"]) > 4:
        return None, "%s: at most four `preempt-at:` sites (the shim holds four)" % name
    v = ctl["verdict"]
    for form in ("exit ", "late "):
        if v.startswith(form):
            try:
                int(v[len(form):].strip())
            except ValueError:
                return None, "%s: `verdict: %sN` needs a number: %r" % (name, form, v)
    return ctl, ""


def apply_control(floor_text, ctl, name="explore", which="subs"):
    """The floor (`subs`), the spec (`spec_subs`) or the program's source
    (`prog_subs`) with each pair's `old` lines replaced by its `new` lines, in
    order -- each exactly one occurrence in the text as it stands, or the
    control is refused by name. A SPEC control (step 5, D-302) plants a FALSE
    caller hypothesis, its verdict the shim's `ASSUMPTION`; a PROGRAM control
    (step 6, X-21) plants a defect in the program itself."""
    text = floor_text
    what = {"subs": "runtime/npkrt.ll", "spec_subs": "runtime/npkrt.spec", "prog_subs": ctl["program"]}[which]
    key = {"subs": "old", "spec_subs": "spec-old", "prog_subs": "program-old"}[which]
    for i, (old_lines, new_lines) in enumerate(ctl[which]):
        old = "\n".join(old_lines) + "\n"
        n = text.count(old)
        if n != 1:
            return None, ("%s: explore-control-unmatched: the control's `%s` lines (pair %d) occur %d time(s) in %s, not once"
                          % (name, key, i + 1, n, what))
        text = text.replace(old, "\n".join(new_lines) + "\n")
    return text, ""


def control_verdict_met(ctl, exp, returncode, shim_verdict, vrun=0):
    """Whether one run met the control's verdict: a shim word by equality;
    `wrong-exit` by any exit other than the program's own or any shim verdict;
    `exit N` by that exit exactly; `late N` by a virtual run time at or past N."""
    v = ctl["verdict"]
    if v == "wrong-exit":
        return bool(shim_verdict) or returncode != exp.exit
    if v.startswith("exit "):
        return returncode == int(v[5:].strip())
    if v.startswith("late "):
        return vrun >= int(v[5:].strip())
    return shim_verdict == v


def resolve_sites(ctl, sites_text, name="explore"):
    """The directed sites' numbers (X-15): each `preempt-at:` against the
    patched floor's sites.txt -- exactly one row of that function whose text
    starts with the prefix, and an `atomic` one (a `sys6` site is a routed
    call, not a point the shim can hold). (numbers, fails)."""
    rows = [l.split("\t") for l in sites_text.split("\n") if l.strip()]
    nums, fails = [], []
    for fn, prefix in ctl["preempt"]:
        hits = [r for r in rows if len(r) == 4 and r[1] == fn and r[3].startswith(prefix)]
        if len(hits) != 1:
            fails.append("%s: explore-control-site-unmatched: `preempt-at: %s %s` names %d site(s) of the explored floor, not one"
                         % (name, fn, prefix, len(hits)))
            continue
        if hits[0][2] != "atomic":
            fails.append("%s: explore-control-site-kind: `preempt-at: %s %s` is a `%s` site; only an atomic step can be a directed site"
                         % (name, fn, prefix, hits[0][2]))
            continue
        nums.append(int(hits[0][0]))
    return nums, fails


# --- step 5: the spec's caller hypotheses, executed (D-302) -----------------------------
#
# `npkg/explore_req.npk` is the ONE generator of the entry checkers; this is its
# counting second reader (X-9): it enumerates the same clauses and classifies each
# by the same rules -- checked when every name is the entry state (a parameter or
# an aggregate parameter's leaf, `exec`/`tls`, a global's address) and every form
# is one of the entry vocabulary; listed otherwise -- and holds the tool's
# `assumptions.txt` to that, line for line, and the explored text to one call of
# each checker at its symbol's entry.

_ENTRY_FORMS_VAL = ("+", "-", "*", "mod", "ite", "load8", "load16", "load32", "load64", "and64", "or64", "xor64", "s8", "s32", "s64")
_ENTRY_FORMS_BOOL = ("and", "or", "not", "=>", "<", "<=", ">", ">=", "=")


def _sx_text(x):
    """The S-expression's canonical text: one space, no newlines."""
    if isinstance(x, tuple):
        return x[1] if x[0] == "atom" else '"%s"' % x[1]
    return "(" + " ".join(_sx_text(i) for i in x) + ")"


def _sx_is_numeral(x):
    a = floor._atom(x)
    return a is not None and a.isdigit()


def _is_pow2(numeral):
    v = int(numeral)
    return 0 < v <= (1 << 64) and (v & (v - 1)) == 0


def _unevaluable(x, names, want_bool):
    """The reason `x` is not an entry-state expression, or None. `names` are the
    names the checker can materialise; `want_bool` whether a proposition is expected."""
    a = floor._atom(x)
    if a is not None:
        if want_bool:
            if a in ("true", "false"):
                return None
            return "a bare name where a proposition was expected: %s" % a
        if a.isdigit():
            return None
        if a in names:
            return None
        return "names `%s`, which is not the entry state" % a
    if isinstance(x, tuple):
        return "a string where %s was expected" % ("a proposition" if want_bool else "an integer")
    if not x:
        return "an empty %s" % ("proposition" if want_bool else "expression")
    h = floor._atom(x[0])
    n = len(x)
    if want_bool:
        if h in ("and", "or"):
            if n < 2:
                return "a form the checker cannot evaluate: %s" % _sx_text(x)
            for i in x[1:]:
                r = _unevaluable(i, names, True)
                if r:
                    return r
            return None
        if h == "not":
            if n != 2:
                return "a form the checker cannot evaluate: %s" % _sx_text(x)
            return _unevaluable(x[1], names, True)
        if h == "=>":
            if n != 3:
                return "a form the checker cannot evaluate: %s" % _sx_text(x)
            return _unevaluable(x[1], names, True) or _unevaluable(x[2], names, True)
        if h in ("<", "<=", ">", ">=", "="):
            if n != 3:
                return "a comparison of other than two operands: %s" % _sx_text(x)
            return _unevaluable(x[1], names, False) or _unevaluable(x[2], names, False)
        return "a form the checker cannot evaluate: %s" % _sx_text(x)
    if h in ("+", "*"):
        if n < 3:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        for i in x[1:]:
            r = _unevaluable(i, names, False)
            if r:
                return r
        return None
    if h == "-":
        if n == 2:
            return _unevaluable(x[1], names, False)
        if n != 3:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        return _unevaluable(x[1], names, False) or _unevaluable(x[2], names, False)
    if h == "mod":
        if n != 3:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        if not (_sx_is_numeral(x[2]) and _is_pow2(floor._atom(x[2]))):
            return "a `mod` by other than a power of two: %s" % _sx_text(x)
        return _unevaluable(x[1], names, False)
    if h == "ite":
        if n != 4:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        return _unevaluable(x[1], names, True) or _unevaluable(x[2], names, False) or _unevaluable(x[3], names, False)
    if h in ("and64", "or64", "xor64"):
        if n != 3:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        return _unevaluable(x[1], names, False) or _unevaluable(x[2], names, False)
    if h in ("s8", "s32", "s64"):
        if n != 2:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        return _unevaluable(x[1], names, False)
    if h in ("load8", "load16", "load32", "load64"):
        if n != 3:
            return "a form the checker cannot evaluate: %s" % _sx_text(x)
        if floor._atom(x[1]) != "mem":
            return "reads a memory that is not the entry state: %s" % _sx_text(x)
        return _unevaluable(x[2], names, False)
    return "a form the checker cannot evaluate: %s" % _sx_text(x)


def _range_ok(en):
    if not isinstance(en, list):
        return False
    if len(en) == 2:
        return True
    return len(en) == 4 and floor._atom(en[2]) == "apart-when"


def _range_facts(entries, first_view):
    """The fact texts of `entries` (objects then views), `objects_facts`' order."""
    out = []
    texts = [_sx_text(e) for e in entries]
    for i in range(len(entries)):
        out.append("(in-space %s)" % texts[i])
        kmax = i if i < first_view else first_view
        for k in range(kmax):
            out.append("(apart %s %s)" % (texts[i], texts[k]))
    return out


def assumption_facts(floor_text, spec_text):
    """[(symbol, status, text, reason)] for every caller clause of every section
    that has rows -- what `assumptions.txt` must say, line for line."""
    headers, types = floor.define_headers(floor_text)
    globals_ = set(m.group(1)[1:] for m in floor._GLOBAL_DECL_RE.finditer(floor_text))
    for raw in floor_text.split("\n"):
        gm = floor._GLOBAL_DECL_RE.match(floor.code_part(raw))
        if gm:
            globals_.add(gm.group(1)[1:])
    out = []
    for sym, clauses in floor.spec_sections(spec_text):
        heads = [floor._atom(c[0]) for c in clauses if isinstance(c, list) and c]
        if not floor._translated(heads):
            continue
        if not any(h in heads for h in ("requires", "objects", "views")):
            continue
        if sym not in headers:
            continue
        names = set(floor.spec_param_names(headers[sym], types)) | {"exec", "tls"} | globals_
        objects = None
        for c in clauses:
            if isinstance(c, list) and c and floor._atom(c[0]) == "objects":
                objects = c[1:]
                break
        for c in clauses:
            if not isinstance(c, list) or not c:
                continue
            h = floor._atom(c[0])
            if h == "requires":
                text = _sx_text(c)
                why = _unevaluable(c[1], names, True) if len(c) == 2 else "a `requires` of other than one proposition"
                out.append((sym, "listed" if why else "checked", text, why or ""))
            elif h in ("objects", "views"):
                entries = list(objects or []) + c[1:] if h == "views" else c[1:]
                first_view = len(objects or []) if h == "views" else len(entries)
                why = None
                for en in entries:
                    if not _range_ok(en):
                        why = "a range that is not `(lo len)` or `(lo len apart-when COND)`: %s" % _sx_text(en)
                        break
                    why = _unevaluable(en[0], names, False) or _unevaluable(en[1], names, False)
                    if not why and len(en) == 4:
                        why = _unevaluable(en[3], names, True)
                    if why:
                        break
                facts = _range_facts(entries, first_view)
                if h == "views":
                    facts = facts[sum(1 + min(i, first_view) for i in range(first_view)):]
                for text in facts:
                    out.append((sym, "listed" if why else "checked", text, why or ""))
    return out


_LABEL_LINE = re.compile(r"^[A-Za-z_.$][\w.$-]*:$")


def _code_keep_strings(raw):
    """The line's code part with its comment cut and its quoted names KEPT."""
    out, in_str = [], False
    for ch in raw:
        if ch == '"':
            in_str = not in_str
        elif ch == ";" and not in_str:
            break
        out.append(ch)
    return "".join(out).strip()


def check_assumptions(floor_text, spec_text, explored_text, assumptions_text, name="explore"):
    """THE HYPOTHESES BELT (step 5): the tool's assumptions.txt says what this
    reader says, line for line (symbol, status, text), and the explored text
    calls each checked symbol's checker once, first thing at its entry."""
    fails = []
    want = [(s, st, tx) for (s, st, tx, _r) in assumption_facts(floor_text, spec_text)]
    got = []
    for l in assumptions_text.split("\n"):
        if not l.strip():
            continue
        parts = l.split("\t")
        if len(parts) != 4 or parts[1] not in ("checked", "listed"):
            return ["%s-assumptions: assumptions.txt line is not `SYMBOL<TAB>checked|listed<TAB>TEXT<TAB>REASON`: %s" % (name, l[:120])]
        got.append((parts[0], parts[1], parts[2]))
    if got != want:
        for i in range(max(len(got), len(want))):
            g = got[i] if i < len(got) else None
            w = want[i] if i < len(want) else None
            if g != w:
                fails.append("%s-assumptions: assumptions.txt line %d is %s where the second reader says %s" % (name, i + 1, g, w))
                break
    checked = []
    for (s, st, _tx) in want:
        if st == "checked" and s not in checked:
            checked.append(s)
    fn = None
    entry_call = {}
    calls_elsewhere = 0
    defines = set()
    first = False                         # the next instruction is the function's first
    in_header = False                     # a define header continuing past its first line
    for raw in explored_text.split("\n"):
        code = _code_keep_strings(raw)    # the comment cut, the quoted names kept (`code_part` blanks them)
        if in_header:
            in_header = not code.endswith("{")
            continue
        if fn is None:
            m = floor._DEFINE_RE.match(code)
            if m:
                fn = m.group(2).replace('"', "")
                if fn.startswith("@npkx.req."):
                    defines.add("@" + fn[len("@npkx.req."):])
                first = True
                in_header = not code.endswith("{")
            continue
        if code == "}":
            fn = None
            continue
        if not code:
            continue
        if _LABEL_LINE.match(code):
            continue
        if code.startswith('call void @"npkx.req.'):
            if first:
                entry_call[fn] = entry_call.get(fn, 0) + 1
            else:
                calls_elsewhere += 1
        first = False
    for s in checked:
        if entry_call.get(s, 0) != 1:
            fails.append("%s-assumptions: `%s` has %d checker call(s) as its first instruction, not one" % (name, s, entry_call.get(s, 0)))
        if s not in defines:
            fails.append("%s-assumptions: no `@\"npkx.req.%s\"` is defined in the explored floor" % (name, s[1:]))
    for s in entry_call:
        if s not in checked:
            fails.append("%s-assumptions: `%s` calls a checker and the second reader gives it no checked clause" % (name, s))
    if calls_elsewhere:
        fails.append("%s-assumptions: %d checker call(s) somewhere other than a function's first instruction" % (name, calls_elsewhere))
    return fails


def assumption_summary(facts):
    """The stage's sentence: counts, and every listed clause by name."""
    checked = sum(1 for f in facts if f[1] == "checked")
    syms = sorted(set(f[0] for f in facts if f[1] == "checked"))
    listed = [f for f in facts if f[1] == "listed"]
    line = ("D-302: %d caller hypotheses checked at every call of %d symbol(s), %d listed by name"
            % (checked, len(syms), len(listed)))
    return line, ["%s %s -- %s" % (s, tx, r) for (s, _st, tx, r) in listed]
