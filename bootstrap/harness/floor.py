"""floor.py -- the floor's shared-state belt (1.5.6 step 0; D-290).

Every word of `runtime/npkrt.ll` that two threads can reach is either an
ATOMIC access or a PLAIN access that a named happens-before edge orders, and
`runtime/npkrt.spec`'s `(shared ...)` section says which, per word. This
module reads the floor's text (defines, blocks, instructions), builds each
function's control-flow graph and dominators, lists every access of the four
shared struct types' fields and of every `global`, reads the spec's
classification, and fails when:

  - an access has no classification (`shared-unclassified`);
  - a word classified `atomic` has a plain access (`shared-plain-atomic`);
  - a word classified `mutex` is loaded or stored at all;
  - a word classified `lock L` is accessed where no `lock(L)` call dominates
    the access and no `unlock(L)` post-dominates it, in a function that is
    neither `(under-lock @fn L)` nor `(exempt @fn L "...")`;
  - an `(under-lock @fn L)` function is called where no `lock(L)` dominates
    the call, from a function that is neither under-lock nor exempt for L;
  - a word classified `publish L` has a plain access outside L, or an
    atomic access weaker than acquire/release.

The classes `owner-only`, `born-before-publish`, `once-before-threads` and
`stated "reason"` are the reviewer's statements (TCB.md SS5 lists them); the
belt records them and checks nothing further. `npkg/floor_shared.npk` is
this module's Nitpick twin, case for case; the runner self-check carries
`shared-unclassified` and `shared-plain-atomic` in both.

The parser reads exactly the floor's own spellings (labels, `br`, `ret`,
`unreachable`, typed `getelementptr` into `%npk.hdr`/`%npk.exec`/`%npk.chan`/
`%npk.tls`, `getelementptr` off a global, `load`/`store` plain and atomic,
`atomicrmw`, `cmpxchg`, `call`) and nothing else; a form it does not know is
not an access, and the classification's completeness is what the accesses'
enumeration is held to.
"""
import re

FIELD_NAMES = {
    "%npk.hdr": ["resume_fn", "state", "windup", "result", "join_head", "sibling", "awaitee",
                 "qnext", "wake_at", "thread_tls", "chan_next", "owner"],
    "%npk.exec": ["rq_head", "rq_tail", "sl_head", "park_at", "park_pending", "park_word",
                  "join_ns", "grace_ns", "chain", "chain_n", "windup_seen", "epfd", "evfd", "cur_task"],
    "%npk.chan": ["buf", "cap", "elem_size", "head", "tail", "count", "gen", "closed", "lock",
                  "recv_waiters", "send_waiters"],
    "%npk.tls": ["self", "exec", "root", "resume", "tid"],
}
ORDERINGS = ("seq_cst", "acq_rel", "acquire", "release", "monotonic", "unordered")
# The lock functions and the lock each call names: a channel's own word
# through `npk_ch_lock`, a global mutex word by its name, a primitive's cell
# word (a register) as `cell`.
LOCK_GLOBALS = {"@npk_ch_open_lock": "ch-open-lock", "@npk_heap_mx": "heap-mx"}


class Block:
    __slots__ = ("label", "lines", "succs", "index")

    def __init__(self, label, index):
        self.label = label
        self.lines = []
        self.succs = []
        self.index = index


class Fn:
    __slots__ = ("name", "blocks", "by_label", "gep", "internal", "mutable")

    def __init__(self, name, internal):
        self.name = name
        self.internal = internal
        self.blocks = []
        self.by_label = {}
        self.gep = {}
        self.mutable = set()


_GLOBAL_DECL_RE = re.compile(r'^(@[\w.$-]+)\s*=\s*(?:internal\s+|private\s+)?(global|constant)\b')
_DEFINE_RE = re.compile(r'^define\s+(internal\s+)?[^@]*?(@(?:"[^"]*"|[\w.$-]+))\s*\(')
_LABEL_RE = re.compile(r'^([\w.$-]+):')
_GEP_STRUCT_RE = re.compile(r'^(%[\w.$-]+) = getelementptr (?:inbounds )?(%npk\.(?:hdr|exec|chan|tls)), ptr (%[\w.$-]+|@[\w.$-]+), i32 0, i32 (\d+)')
_GEP_GLOBAL_RE = re.compile(r'^(%[\w.$-]+) = getelementptr (?:inbounds )?[^,]+, ptr (@[\w.$-]+)')
_LOAD_RE = re.compile(r'^(%[\w.$-]+) = load (atomic )?(?:volatile )?(.+), ptr (%[\w.$-]+|@[\w.$-]+)((?:,| ).*)?$')
_STORE_RE = re.compile(r'^store (atomic )?(?:volatile )?(.+), ptr (%[\w.$-]+|@[\w.$-]+)((?:,| ).*)?$')
_RMW_RE = re.compile(r'^(?:%[\w.$-]+ = )?(atomicrmw \w+|cmpxchg) ptr (%[\w.$-]+|@[\w.$-]+),')
_CALL_RE = re.compile(r'call [^@]*?(@[\w.$-]+)\(([^)]*)\)')


def code_part(line):
    """The line up to its comment, with string constants blanked (a `;`
    inside `c"..."` is data)."""
    out = []
    in_str = False
    for ch in line:
        if ch == '"':
            in_str = not in_str
            out.append(ch)
            continue
        if in_str:
            out.append(" ")
            continue
        if ch == ";":
            break
        out.append(ch)
    return "".join(out).strip()


def parse_floor(text):
    """Every define of the floor: its blocks in order, each block's
    instruction lines (code parts, non-empty) and successors, the function's
    GEP map (register -> (struct, field) or ("global", name))."""
    fns = {}
    order = []
    cur = None
    blk = None
    mutable = set()
    raws = text.split("\n")
    ri = 0
    while ri < len(raws):
        line = code_part(raws[ri])
        ri += 1
        # an instruction may continue on the next line (the clone's call ends a
        # line with a comma): join until the code part ends in neither `,` nor `(`
        while cur is not None and line and line[-1] in ",(" and ri < len(raws):
            line = line + " " + code_part(raws[ri])
            ri += 1
        if cur is None:
            gd = _GLOBAL_DECL_RE.match(line)
            if gd:
                if gd.group(2) == "global":
                    mutable.add(gd.group(1))
                continue
            m = _DEFINE_RE.match(line)
            if m:
                name = m.group(2).strip('"')
                cur = Fn(name, m.group(1) is not None)
                blk = Block("entry", 0)
                cur.blocks.append(blk)
                cur.by_label["entry"] = blk
            continue
        if line == "}":
            _finish(cur)
            fns[cur.name] = cur
            order.append(cur.name)
            cur = None
            blk = None
            continue
        if not line:
            continue
        lm = _LABEL_RE.match(line)
        if lm:
            label = lm.group(1)
            if blk.label == "entry" and not blk.lines and label != "entry" and len(cur.blocks) == 1:
                # the floor names its first block `entry:` almost everywhere;
                # a first label that is not `entry` renames the implicit block
                cur.by_label.pop("entry")
                blk.label = label
                cur.by_label[label] = blk
                continue
            if label in cur.by_label and not cur.by_label[label].lines and cur.by_label[label] is blk:
                continue
            blk = Block(label, len(cur.blocks))
            cur.blocks.append(blk)
            cur.by_label[label] = blk
            continue
        blk.lines.append(line)
        gm = _GEP_STRUCT_RE.match(line)
        if gm:
            cur.gep[gm.group(1)] = (gm.group(2), int(gm.group(4)))
            continue
        gg = _GEP_GLOBAL_RE.match(line)
        if gg:
            cur.gep[gg.group(1)] = ("global", gg.group(2))
    if cur is not None:
        raise ValueError("floor: the last define never closed")
    for fname in order:
        fns[fname].mutable = mutable
    return fns, order


def _finish(fn):
    for b in fn.blocks:
        succs = []
        if b.lines:
            last = b.lines[-1]
            if last.startswith("br "):
                succs = re.findall(r'label %([\w.$-]+)', last)
            elif last.startswith("switch "):
                succs = re.findall(r'label %([\w.$-]+)', last)
        b.succs = [fn.by_label[s].index for s in succs if s in fn.by_label]


def dominators(fn):
    """dom[b] = the set of block indexes dominating b (b included); the
    entry is block 0. The iterative dataflow, on a graph small enough that
    nothing cleverer is worth reading."""
    n = len(fn.blocks)
    preds = [[] for _ in range(n)]
    for b in fn.blocks:
        for s in b.succs:
            preds[s].append(b.index)
    full = set(range(n))
    dom = [full.copy() for _ in range(n)]
    dom[0] = {0}
    changed = True
    while changed:
        changed = False
        for i in range(1, n):
            if not preds[i]:
                new = {i}          # unreachable from the entry: dominates only itself
            else:
                inter = None
                for p in preds[i]:
                    inter = dom[p].copy() if inter is None else (inter & dom[p])
                new = inter | {i}
            if new != dom[i]:
                dom[i] = new
                changed = True
    return dom


def word_key(fn, ptr):
    """The word an address names: ("%npk.hdr", 2) for a struct field GEP,
    ("global", "@npk_frozen") for a global or a GEP off one, or None."""
    if ptr.startswith("@"):
        return ("global", ptr) if ptr in fn.mutable else None
    key = fn.gep.get(ptr)
    if key and key[0] == "global" and key[1] not in fn.mutable:
        return None
    return key


def accesses(fn):
    """Every access of a classified word in `fn`: (block index, line index,
    key, kind, ordering) with kind `load`/`store`/`atomicrmw op`/`cmpxchg`
    and ordering `plain` or the atomic word."""
    out = []
    for b in fn.blocks:
        for i, line in enumerate(b.lines):
            m = _LOAD_RE.match(line)
            if m:
                key = word_key(fn, m.group(4))
                if key:
                    ordr = _ordering(m.group(5)) if m.group(2) else "plain"
                    out.append((b.index, i, key, "load", ordr))
                continue
            m = _STORE_RE.match(line)
            if m:
                key = word_key(fn, m.group(3))
                if key:
                    ordr = _ordering(m.group(4)) if m.group(1) else "plain"
                    out.append((b.index, i, key, "store", ordr))
                continue
            m = _RMW_RE.match(line)
            if m:
                key = word_key(fn, m.group(2))
                if key:
                    out.append((b.index, i, key, m.group(1), "atomic"))
                continue
            m = _CALL_RE.search(line)
            if m:
                callee = m.group(1)
                for arg in re.findall(r'ptr (%[\w.$-]+)', m.group(2)):
                    key = fn.gep.get(arg)
                    if key and not (key[0] == "global" and key[1] not in fn.mutable):
                        kind = "lock-arg" if callee in ("@npk_mx_lock", "@npk_mx_unlock", "@npk_ch_lock", "@npk_ch_unlock") else "call-arg"
                        out.append((b.index, i, key, kind, "plain"))
    return out


def _ordering(tail):
    for o in ORDERINGS:
        if re.search(r'\b' + o + r'\b', tail):
            return o
    return "plain"


def lock_calls(fn):
    """Every lock/unlock call in `fn`: (block, line, lock name, is_lock)."""
    out = []
    for b in fn.blocks:
        for i, line in enumerate(b.lines):
            m = _CALL_RE.search(line)
            if not m:
                continue
            callee, args = m.group(1), m.group(2)
            if callee in ("@npk_ch_lock", "@npk_ch_unlock"):
                out.append((b.index, i, "chan.lock", callee == "@npk_ch_lock"))
            elif callee in ("@npk_mx_lock", "@npk_mx_unlock"):
                am = re.search(r'ptr (@[\w.$-]+|%[\w.$-]+)', args)
                arg = am.group(1) if am else ""
                name = LOCK_GLOBALS.get(arg, "cell" if arg.startswith("%") else arg)
                out.append((b.index, i, name, callee == "@npk_mx_lock"))
    return out


def calls_of(fn):
    """Every call in `fn`: (block, line, callee)."""
    out = []
    for b in fn.blocks:
        for i, line in enumerate(b.lines):
            m = _CALL_RE.search(line)
            if m:
                out.append((b.index, i, m.group(1)))
    return out


def held_at(fn, dom, locks, bi, li, lock):
    """Is `lock` held at (bi, li): a lock(L) call dominates the point (an
    earlier line of the same block, or a strictly dominating block) AND an
    unlock(L) post-dominates it (every path from the point to a `ret` passes
    one; a path ending in `unreachable` ends the process holding it, which
    the trap route does on purpose)."""
    names = {lock} if lock != "waiter-list" else {"chan.lock", "cell"}
    lock_sites = [(b, i) for (b, i, name, is_lock) in locks if is_lock and name in names]
    unlock_sites = [(b, i) for (b, i, name, is_lock) in locks if (not is_lock) and name in names]
    dominated = any((b == bi and i < li) or (b != bi and b in dom[bi]) for (b, i) in lock_sites)
    if not dominated:
        return False
    # post-domination by an unlock: a block is `covered` when it holds an
    # unlock, ends in `unreachable`, or every successor is covered (a
    # greatest fixpoint, so a loop with no exit-without-unlock is covered).
    n = len(fn.blocks)
    has_unlock = [False] * n
    for (b, i) in unlock_sites:
        has_unlock[b] = True
    # the access's own block: an unlock AFTER the access counts; one before
    # it does not
    own_after = any(b == bi and i > li for (b, i) in unlock_sites)
    covered = [True] * n
    changed = True
    while changed:
        changed = False
        for b in fn.blocks:
            last = b.lines[-1] if b.lines else ""
            if has_unlock[b.index] or last == "unreachable" or last.startswith("unreachable"):
                v = True
            elif not b.succs:
                v = False
            else:
                v = all(covered[s] for s in b.succs)
            if v != covered[b.index]:
                covered[b.index] = v
                changed = True
    if own_after:
        return True
    ob = fn.blocks[bi]
    last = ob.lines[-1] if ob.lines else ""
    if last == "unreachable":
        return True
    if not ob.succs:
        return False
    return all(covered[s] for s in ob.succs)


# --- the spec's (shared ...) section ---------------------------------------------------------

def sexpr(text):
    """A minimal S-expression reader: lists, atoms, "strings"; `;` comments."""
    toks = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            i += 1
        elif c == ";":
            while i < n and text[i] != "\n":
                i += 1
        elif c in "()":
            toks.append(c); i += 1
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 1
            toks.append(("str", text[i + 1:j])); i = j + 1
        else:
            j = i
            while j < n and text[j] not in " \t\r\n();\"":
                j += 1
            toks.append(("atom", text[i:j])); i = j
    pos = 0

    def read():
        nonlocal pos
        t = toks[pos]; pos += 1
        if t == "(":
            items = []
            while toks[pos] != ")":
                items.append(read())
            pos += 1
            return items
        if t == ")":
            raise ValueError("spec: unbalanced `)`")
        return t
    out = []
    while pos < len(toks):
        out.append(read())
    return out


def _atom(x):
    return x[1] if isinstance(x, tuple) and x[0] == "atom" else None


def _string(x):
    return x[1] if isinstance(x, tuple) and x[0] == "str" else None


def read_shared(spec_text):
    """The classification: word key -> (class, arg, reason); the under-lock
    and exempt tables; the lock names declared."""
    forms = sexpr(spec_text)
    shared = None
    for f in forms:
        if isinstance(f, list) and f and _atom(f[0]) == "shared":
            shared = f
    if shared is None:
        raise ValueError("runtime/npkrt.spec: no `(shared ...)` section")
    words, under, exempt, locks = {}, {}, {}, {}
    for entry in shared[1:]:
        if not isinstance(entry, list) or not entry:
            raise ValueError("spec: a (shared ...) entry is not a list")
        head = _atom(entry[0])
        if head == "lock":
            locks[_atom(entry[1])] = entry[2:]
        elif head == "word":
            target = _atom(entry[1])
            if target.startswith("%"):
                key = (target, int(_atom(entry[2])))
                rest = entry[3:]
            else:
                key = ("global", target)
                rest = entry[2:]
            cls = _atom(rest[0])
            arg = _atom(rest[1]) if len(rest) > 1 and _atom(rest[1]) is not None else None
            reason = None
            for r in rest[1:]:
                if _string(r) is not None:
                    reason = _string(r)
            if cls == "stated" and reason is None:
                raise ValueError("spec: a `stated` word needs its reason string: %s" % (key,))
            if cls in ("lock", "publish") and arg is None:
                raise ValueError("spec: a `%s` word names its lock: %s" % (cls, key))
            words[key] = (cls, arg, reason)
        elif head == "under-lock":
            under.setdefault(_atom(entry[1]), set()).add(_atom(entry[2]))
        elif head == "exempt":
            reason = _string(entry[3]) if len(entry) > 3 else None
            if reason is None:
                raise ValueError("spec: an `exempt` entry needs its reason string")
            exempt.setdefault(_atom(entry[1]), {})[_atom(entry[2])] = reason
        else:
            raise ValueError("spec: unknown (shared ...) entry `%s`" % head)
    return words, under, exempt, locks


def key_text(key):
    if key[0] == "global":
        return key[1]
    names = FIELD_NAMES.get(key[0], [])
    name = names[key[1]] if key[1] < len(names) else str(key[1])
    return "%s slot %d (%s)" % (key[0], key[1], name)


def check_shared(floor_text, spec_text, name="floor"):
    """The belt over one floor text and one spec text; the failures by name."""
    fails = []
    try:
        fns, order = parse_floor(floor_text)
        words, under, exempt, locks = read_shared(spec_text)
    except ValueError as e:
        return ["%s: %s" % (name, e)]
    # every function's accesses, dominators and lock sites, once
    info = {}
    for fname in order:
        fn = fns[fname]
        info[fname] = (accesses(fn), dominators(fn), lock_calls(fn))
    reported = set()
    for fname in order:
        fn = fns[fname]
        acc, dom, lk = info[fname]
        for (bi, li, key, kind, ordr) in acc:
            if key not in words:
                if key not in reported:
                    reported.add(key)
                    fails.append("%s: shared-unclassified: `%s` is accessed (%s in %s) and runtime/npkrt.spec's "
                                 "(shared ...) section does not classify it (D-290)" % (name, key_text(key), kind, fname))
                continue
            cls, arg, reason = words[key]
            if cls == "atomic":
                if kind == "call-arg":
                    fails.append("%s: shared-atomic-by-pointer: `%s` is classified atomic and `%s` hands its address to a call -- "
                                 "the callee's accesses are not this belt's to see" % (name, key_text(key), fname))
                    continue
                if ordr == "plain":
                    fails.append("%s: shared-plain-atomic: `%s` is classified atomic and `%s` accesses it with a plain %s (D-290)"
                                 % (name, key_text(key), fname, kind))
            elif cls == "mutex":
                if kind == "lock-arg":
                    continue
                fails.append("%s: shared-mutex-accessed: `%s` is a futex mutex word and `%s` %ss it directly -- only "
                             "npk_mx_lock/npk_mx_unlock may touch it" % (name, key_text(key), fname, kind))
            elif cls in ("lock", "publish"):
                if cls == "publish" and ordr != "plain":
                    if ordr not in ("acquire", "release", "seq_cst", "acq_rel", "atomic"):
                        fails.append("%s: shared-publish-weak: `%s` is published under %s and `%s` reads it `%s` -- acquire/release or stronger"
                                     % (name, key_text(key), arg, fname, ordr))
                    continue
                if fname in under and arg in under[fname]:
                    continue
                if fname in exempt and arg in exempt[fname]:
                    continue
                if not held_at(fn, dom, lk, bi, li, arg):
                    fails.append("%s: shared-outside-lock: `%s` is classified under %s and `%s` %ss it at block `%s` line %d "
                                 "where no lock(%s) dominates the access and an unlock(%s) post-dominates it -- name the function "
                                 "(under-lock ...) if its callers hold the lock, or (exempt ...) with the reason"
                                 % (name, key_text(key), arg, fname, kind, fn.blocks[bi].label, li + 1, arg, arg))
            elif cls in ("owner-only", "born-before-publish", "once-before-threads", "stated"):
                pass
            else:
                fails.append("%s: shared-unknown-class: `%s` is classified `%s`, which the belt does not know" % (name, key_text(key), cls))
    # every call of an under-lock function is made with the lock held, or
    # from a function that is itself under-lock or exempt for that lock
    for fname in order:
        fn = fns[fname]
        acc, dom, lk = info[fname]
        for (bi, li, callee) in calls_of(fn):
            if callee not in under:
                continue
            for lock in under[callee]:
                if fname in under and lock in under[fname]:
                    continue
                if fname in exempt and lock in exempt[fname]:
                    continue
                if not held_at(fn, dom, lk, bi, li, lock):
                    fails.append("%s: shared-call-outside-lock: `%s` is (under-lock %s) and `%s` calls it at block `%s` line %d "
                                 "without holding %s" % (name, callee, lock, fname, fn.blocks[bi].label, li + 1, lock))
    # every classified word exists (a stale classification is the next stale document)
    seen = set()
    for fname in order:
        for (bi, li, key, kind, ordr) in info[fname][0]:
            seen.add(key)
    for key, (cls, arg, reason) in words.items():
        if key not in seen and cls not in ("mutex",):
            fails.append("%s: shared-stale: `%s` is classified `%s` and no function accesses it -- remove the entry or "
                         "the belt is describing a word the floor no longer has" % (name, key_text(key), cls))
    for fname in list(under) + list(exempt):
        if fname not in fns:
            fails.append("%s: shared-stale: `%s` is named (under-lock/exempt) and the floor does not define it" % (name, fname))
    return fails


def stated_words(spec_text):
    """The reviewer's statements, for TCB.md SS5: [(word text, class, reason)]."""
    words, under, exempt, locks = read_shared(spec_text)
    out = []
    for key in sorted(words, key=key_text):
        cls, arg, reason = words[key]
        if cls in ("owner-only", "born-before-publish", "once-before-threads", "stated"):
            out.append((key_text(key), cls, reason or ""))
    for fname in sorted(exempt):
        for lock, reason in sorted(exempt[fname].items()):
            out.append(("%s (exempt %s)" % (fname, lock), "exempt", reason))
    return out


if __name__ == "__main__":
    import sys, os
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    ft = open(os.path.join(root, "runtime", "npkrt.ll"), encoding="utf-8").read()
    st = open(os.path.join(root, "runtime", "npkrt.spec"), encoding="utf-8").read()
    fl = check_shared(ft, st)
    for f in fl:
        print(f)
    print("%d finding(s)" % len(fl))
    sys.exit(1 if fl else 0)


# --- the spec belt and TCB.md's disposition column (1.5.6 step 3; D-288) ----------------
#
# The Python twins of `npkg/floor.npk`'s `floor_spec_current`, `floor_disposition`
# and `tcb_floor_current`: the harness runs them on every full run before the
# floor's writer (`tools/floorspec.npk`) and z3 are spawned.

_DEFINE_HEAD_RE = re.compile(r'^define\s+(?:internal\s+|private\s+)?(.*?)\s*(@[\w.$-]+)\s*\((.*?)\)', re.S)
_STRUCT_TYPE_RE = re.compile(r'^(%[\w.$-]+)\s*=\s*type\s*(\{.*\})\s*$')

_CLAUSE_HEADS = ("free", "requires", "ensures", "ensures-trap", "frame", "loop", "summary", "residue", "boundary")

_CLASS_DEFAULT = {
    "asm": "the volatile bottom (inline asm): TRUSTED, documented; no proof",
    "atomic": "a modelled primitive (1.5.6, the r6 verdict: model the primitive, never the whole executor)",
    "syscall": "specified at the syscall boundary (1.5.6); the kernel is trusted",
    "pure": "pure IR: Z3-specified at 1.5.6 where feasible",
}


def _split_top(s, sep=","):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "{[":
            depth += 1
        elif ch in "}]":
            depth -= 1
        if ch == sep and depth == 0:
            out.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def define_headers(floor_text):
    """@name -> (return type, [(type, %name)]) for every define of the floor."""
    out = {}
    types = {}
    for raw in floor_text.split("\n"):
        line = code_part(raw)
        tm = _STRUCT_TYPE_RE.match(line)
        if tm:
            types[tm.group(1)] = tm.group(2)
        if not line.startswith("define "):
            continue
        m = _DEFINE_HEAD_RE.match(line)
        if not m:
            continue
        params = []
        for piece in _split_top(m.group(3)):
            pm = re.match(r'^(.*?)\s*(%[\w.$-]+)$', piece)
            if pm:
                params.append((pm.group(1).strip(), pm.group(2)))
            elif piece:
                params.append((piece, ""))
        out[m.group(2)] = (m.group(1).strip(), params)
    return out, types


def spec_param_names(header, types):
    """The names a spec may spell for a define's parameters: `dst`, and an
    aggregate's fields as `a.0`, `a.1`..."""
    names = []
    for ty, pn in header[1]:
        if not pn:
            continue
        bare = pn[1:]
        body = types.get(ty, ty) if ty.startswith("%") else ty
        if body.startswith("{"):
            fields = _split_top(body[1:-1])
            names.extend("%s.%d" % (bare, i) for i in range(len(fields)))
        else:
            names.append(bare)
    return names


def spec_sections(spec_text):
    """[(name, [clauses])] for every (symbol @name ...) form, in order."""
    out = []
    for form in sexpr(spec_text):
        if isinstance(form, list) and form and _atom(form[0]) == "symbol":
            out.append((_atom(form[1]) if len(form) > 1 else None, form[2:]))
    return out


def check_spec(floor_text, spec_text, name="floor"):
    """THE SPEC BELT: every (symbol ...) section names a define; a (loop LABEL
    ...) names a block of it with exactly one treatment; every section claims
    something (a clause that yields rows) or says why not (residue, boundary);
    a (summary) section has an ensures; no symbol twice; no clause head outside
    the grammar; no free symbol shadowing a parameter."""
    fails = []
    try:
        forms = sexpr(spec_text)
    except ValueError as e:
        return ["%s: runtime/npkrt.spec: %s" % (name, e)]
    fns, _ = parse_floor(floor_text)
    heads, types = define_headers(floor_text)
    seen = set()
    for form in forms:
        head = _atom(form[0]) if isinstance(form, list) and form else None
        if head == "shared":
            continue
        if head != "symbol":
            fails.append("%s: runtime/npkrt.spec: a top-level form that is neither (shared ...) nor (symbol ...): %s" % (name, head))
            continue
        sym = _atom(form[1]) if len(form) > 1 else None
        if not sym:
            fails.append("%s: runtime/npkrt.spec: a (symbol ...) without a name" % name)
            continue
        if sym in seen:
            fails.append("%s: runtime/npkrt.spec: a symbol specified twice: %s" % (name, sym))
            continue
        seen.add(sym)
        if sym not in fns:
            fails.append("%s: runtime/npkrt.spec names a symbol the floor does not define: %s" % (name, sym))
            continue
        claims = excused = summary = has_ensures = False
        free = []
        for cl in form[2:]:
            if not isinstance(cl, list) or not cl:
                fails.append("%s: runtime/npkrt.spec: a bare word inside the section of %s" % (name, sym))
                break
            ch = _atom(cl[0])
            if ch not in _CLAUSE_HEADS:
                fails.append("%s: runtime/npkrt.spec: a clause the grammar does not know in %s: %s" % (name, sym, ch))
                continue
            if ch == "free" and len(cl) > 1:
                free.append(_atom(cl[1]))
            if ch in ("ensures", "ensures-trap", "frame", "loop"):
                claims = True
            if ch == "ensures":
                has_ensures = True
            if ch == "summary":
                summary = True
            if ch in ("residue", "boundary"):
                excused = True
            if ch == "loop":
                label = _atom(cl[1]) if len(cl) > 1 else None
                if label not in fns[sym].by_label:
                    fails.append("%s: runtime/npkrt.spec: %s has no block labelled %s" % (name, sym, label))
                treatments = sum(1 for x in cl[2:] if isinstance(x, list) and x and _atom(x[0]) in ("invariant", "unroll"))
                if treatments != 1:
                    fails.append("%s: runtime/npkrt.spec: the loop %s of %s needs exactly one of (invariant I) and (unroll N)" % (name, label, sym))
        if not (claims or excused):
            fails.append("%s: runtime/npkrt.spec: a section with no claim and no residue or boundary sentence: %s" % (name, sym))
        if summary and not has_ensures:
            fails.append("%s: runtime/npkrt.spec: a (summary) symbol without an ensures -- a caller would assume nothing: %s" % (name, sym))
        if sym in heads:
            for f in free:
                if f in spec_param_names(heads[sym], types):
                    fails.append("%s: runtime/npkrt.spec: a free symbol of %s shadows its parameter %s" % (name, sym, f))
    return fails


def disposition(sections, sym, cls, rows):
    """One symbol's disposition in D-288 §2.1's words (the twin of
    `floor_disposition`): `trusted (inline asm)`; `specified (N discharged, M
    residue)` from the committed floor manifest's `floor-spec` rows; the
    `residue (...)` and `boundary (...)` sentences; else the class default."""
    if cls == "asm":
        return "trusted (inline asm)"
    sec = dict(sections).get(sym)
    if sec is None:
        return _CLASS_DEFAULT[cls]
    d = sum(1 for r in rows if r[3] == sym and r[1] == "floor-spec" and r[2] == "discharged")
    b = sum(1 for r in rows if r[3] == sym and r[1] == "floor-spec" and r[2] == "budget")
    o = sum(1 for r in rows if r[3] == sym and r[1] == "floor-spec" and r[2] == "open")
    parts = []
    if d + b + o:
        parts.append("specified (%d discharged, %d residue)%s" % (d, b, " -- REFUTED: a red run" if o else ""))
    for cl in sec:
        if isinstance(cl, list) and cl and _atom(cl[0]) == "residue" and len(cl) > 1:
            parts.append("residue (%s)" % _string(cl[1]))
    for cl in sec:
        if isinstance(cl, list) and cl and _atom(cl[0]) == "boundary" and len(cl) > 1:
            parts.append("boundary (%s)" % _string(cl[1]))
    return "; ".join(parts) if parts else _CLASS_DEFAULT[cls]


def manifest_rows_of(text):
    """(hash, kind, verdict, symbol) per row of a floor manifest text (no
    validation: the runners' readers do that)."""
    out = []
    for l in text.splitlines():
        if not l.strip() or l.startswith("#"):
            continue
        parts = l.split(" ", 5)
        if len(parts) == 6:
            out.append((parts[0], parts[1], parts[3], parts[5]))
    return out


def tcb_rows(spec_text, classes, manifest_text):
    """TCB.md's floor-table rows, sorted: the table's `| symbol | class | disposition |` lines."""
    sections = spec_sections(spec_text)
    rows = manifest_rows_of(manifest_text) if manifest_text else []
    return sorted("| `%s` | %s | %s |" % (sym, cls, disposition(sections, sym, cls, rows))
                  for sym, cls in classes.items())


def tcb_region(spec_text, classes, manifest_text):
    """The whole marked region's body, header row included."""
    return "\n".join(["| symbol | class | disposition |", "|---|---|---|"] + tcb_rows(spec_text, classes, manifest_text))
