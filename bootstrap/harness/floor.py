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


# ---------------------------------------------------------------------------
# THE STACK RULE's SECOND HALF (1.5.6b; DEF-52). Every `alloca` of the floor is
# FULLY DEFINED in its entry block before anything else touches it -- by stores
# that cover every byte of it, or by one `llvm.memset` of its whole size.
#
# The FIRST half -- every alloca sits in the entry block -- is D-173's, settled
# at 1.0.9a and held by `check_allocas_hoisted` over every EMITTED module since.
# It had never been pointed at the hand-written floor (DEF-53: eight of the
# floor's fifteen allocas sat outside their entry blocks, two of them inside
# loops); the harness and `npkg build` now run that same check over
# `runtime/npkrt.ll`, and this function adds nothing to it. One rule, one belt.
#
# DEF-52 is what this half is for: `npk_hardware_concurrency` handed the kernel
# a buffer it never zeroed and read all of it back, where the raw syscall
# writes only `result` bytes -- a value never written (D-227's family). The
# rule is syntactic on purpose: no exceptions to remember, and a belt can hold
# it. It looks only at the entry block, because D-173 says that is where every
# alloca is. `npkg/floor_stack.npk` is this function's twin, finding for
# finding.
_ALLOCA_RE = re.compile(r'^(%[\w.$-]+) = alloca (.+?)(?:, align \d+)?$')
_GEP_ARR_RE = re.compile(r'^(%[\w.$-]+) = getelementptr (?:inbounds )?\[(\d+) x ([^\]]+)\], ptr (%[\w.$-]+), i64 0, i64 (\d+)$')
_GEP_I8_RE = re.compile(r'^(%[\w.$-]+) = getelementptr (?:inbounds )?i8, ptr (%[\w.$-]+), i64 (\d+)$')
_STORE_TO_RE = re.compile(r'^store (?:atomic )?(?:volatile )?(i\d+|ptr) .+, ptr (%[\w.$-]+)(?:,| |$)')
_MEMSET_RE = re.compile(r'^call void @llvm\.memset\.p0\.i64\(ptr (%[\w.$-]+), i8 \d+, i64 (\d+), i1 false\)$')
_REG_RE = re.compile(r'%[\w.$-]+')


def ir_type_bytes(ty):
    """The size in bytes of the IR types the floor's allocas use: iN, ptr, and
    arrays of them. Anything else is a ValueError -- a form the rule was not
    written for is a finding, never a guess."""
    ty = ty.strip()
    m = re.match(r'^\[(\d+) x (.+)\]$', ty)
    if m:
        return int(m.group(1)) * ir_type_bytes(m.group(2))
    if ty == "ptr":
        return 8
    m = re.match(r'^i(\d+)$', ty)
    if m:
        return (int(m.group(1)) + 7) // 8
    raise ValueError("an alloca of a type the stack rule does not size: %s" % ty)


def check_stack(floor_text, name="floor"):
    """The rule over one floor text; the failures by name (`alloca-not-defined`)."""
    try:
        fns, order = parse_floor(floor_text)
    except ValueError as e:
        return ["%s: %s" % (name, e)]
    fails = []
    for fname in order:
        fn = fns[fname]
        size = {}      # alloca register -> bytes
        at = {}        # register -> (alloca register, byte offset)
        have = {}      # alloca register -> set of defined byte offsets
        told = set()
        for line in fn.blocks[0].lines:
            m = _ALLOCA_RE.match(line)
            if m:
                try:
                    size[m.group(1)] = ir_type_bytes(m.group(2))
                except ValueError as e:
                    fails.append("%s: alloca-not-defined: `%s`: %s" % (name, fname, e))
                    continue
                at[m.group(1)] = (m.group(1), 0)
                have[m.group(1)] = set()
                continue
            g = _GEP_ARR_RE.match(line)
            if g and g.group(4) in size:
                at[g.group(1)] = (g.group(4), int(g.group(5)) * ir_type_bytes(g.group(3)))
                continue
            g = _GEP_I8_RE.match(line)
            if g and g.group(2) in size:
                at[g.group(1)] = (g.group(2), int(g.group(3)))
                continue
            s = _STORE_TO_RE.match(line)
            if s and s.group(2) in at:
                base, off = at[s.group(2)]
                have[base].update(range(off, off + ir_type_bytes(s.group(1))))
                continue
            ms = _MEMSET_RE.match(line)
            if ms and ms.group(1) in size:
                have[ms.group(1)].update(range(0, int(ms.group(2))))
                continue
            # any other line naming the alloca, or a constant offset of it, is a USE
            for reg in _REG_RE.findall(line):
                if reg in at:
                    base = at[reg][0]
                    if base not in told and not set(range(size[base])) <= have[base]:
                        told.add(base)
                        fails.append("%s: alloca-not-defined: `%s` uses `%s` (%d bytes) before its entry block has "
                                     "defined all of it -- %d byte(s) defined; cover it with stores or one "
                                     "llvm.memset of its whole size before anything else touches it (DEF-52)"
                                     % (name, fname, base, size[base], len(have[base])))
        for base in size:
            if base not in told and not set(range(size[base])) <= have[base]:
                fails.append("%s: alloca-not-defined: `%s` leaves `%s` (%d bytes) with %d byte(s) defined at the end of "
                             "its entry block -- cover it with stores or one llvm.memset of its whole size (DEF-52)"
                             % (name, fname, base, size[base], len(have[base])))
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

_CLAUSE_HEADS = ("free", "requires", "ensures", "ensures-trap", "frame", "loop", "summary", "residue", "boundary",
                 "objects", "ensures-fresh", "views")

# A HEAD THE TRANSLATOR READS ONCE MAY BE WRITTEN ONCE (1.5.6c step 1). `spec_clause(s, sec, HEAD, 0)` is how
# `npkg/floor_smt.npk` reads each of these, so a second one was dropped in silence: for `frame`, `ensures-trap` and
# `ensures-fresh` a CLAIM written and never proven, for `objects` and `views` a hypothesis written and never
# assumed. The order is the order both runners report in.
_ONCE_HEADS = ("objects", "views", "frame", "ensures-trap", "ensures-fresh", "residue", "boundary", "summary")

# what a `(loop LABEL ...)` clause may hold: the translator reads these four and nothing else
_LOOP_CLAUSE_HEADS = ("invariant", "unroll", "objects", "inst")

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
    the grammar; no free symbol shadowing a parameter; a head the translator
    reads once written once, and a loop holding only the four sub-clauses it
    reads (1.5.6c step 1: either was dropped in silence before)."""
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
            if ch in ("ensures", "ensures-trap", "frame", "loop", "ensures-fresh"):
                claims = True
            if ch in ("ensures", "ensures-trap", "frame", "ensures-fresh"):
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
                # a sub-clause the translator does not read would be dropped in silence (1.5.6c step 1)
                for x in cl[2:]:
                    if not isinstance(x, list):
                        fails.append("%s: runtime/npkrt.spec: a bare word inside the loop %s of %s" % (name, label, sym))
                        continue
                    xh = (_atom(x[0]) if x else None) or ""   # the twin's `sx_head`: "" for a head that is no atom
                    if xh not in _LOOP_CLAUSE_HEADS:
                        fails.append("%s: runtime/npkrt.spec: a loop sub-clause the grammar does not know in the loop %s of %s: %s" % (name, label, sym, xh))
                if sum(1 for x in cl[2:] if isinstance(x, list) and x and _atom(x[0]) == "objects") > 1:
                    fails.append("%s: runtime/npkrt.spec: a clause read once and written twice in the loop %s of %s: objects" % (name, label, sym))
        for once in _ONCE_HEADS:
            if sum(1 for cl in form[2:] if isinstance(cl, list) and cl and _atom(cl[0]) == once) > 1:
                fails.append("%s: runtime/npkrt.spec: a clause read once and written twice in %s: %s" % (name, sym, once))
        if not (claims or excused):
            fails.append("%s: runtime/npkrt.spec: a section with no claim and no residue or boundary sentence: %s" % (name, sym))
        if summary and not has_ensures:
            fails.append("%s: runtime/npkrt.spec: a (summary) symbol without an ensures -- a caller would assume nothing: %s" % (name, sym))
        if sym in heads:
            for f in free:
                if f in spec_param_names(heads[sym], types):
                    fails.append("%s: runtime/npkrt.spec: a free symbol of %s shadows its parameter %s" % (name, sym, f))
    return fails


# --- the protocol models (1.5.6 step 5; D-289 §2.7) ---------------------------------

import glob
import os

_STEP_LINE = re.compile(r"\b(atomicrmw|cmpxchg|fence)\b|\b(load|store) atomic\b|@npk_sys6\(")


def read_models(root):
    """Every `runtime/models/*.model`, sorted by name: (name, [(symbol, block), ...])
    from its steps' `(ir @sym BLOCK ...)` forms -- the belt's reading; the
    model's meaning is the writer's (`npkg/floor_model.npk`)."""
    import glob
    out = []
    for path in sorted(glob.glob(os.path.join(root, "runtime", "models", "*.model"))):
        text = "\n".join(code_part(l) for l in open(path, encoding="utf-8").read().split("\n"))
        pairs = []
        for m in re.finditer(r"\(ir (@[\w.$-]+)((?:\s+[\w.$-]+)+)\)", text):
            for b in m.group(2).split():
                pairs.append((m.group(1), b))
        out.append((os.path.basename(path)[:-len(".model")], pairs))
    return out


def check_models(floor_text, models, name="floor"):
    """THE CORRESPONDENCE BELT (the twin of `floor_models_current`): every
    step names a define of the floor and a block of it, and every atomic
    operation or syscall of a symbol any model names sits in a block some
    step names -- a model can be wrong about a step's meaning, but it cannot
    be silent about a step."""
    fns, _ = parse_floor(floor_text)
    fails = []
    named_syms = set()
    named_pairs = set()
    for mname, pairs in models:
        for sym, blk in pairs:
            f = fns.get(sym)
            if f is None:
                fails.append("%s: runtime/models/%s names a symbol the floor does not define: %s" % (name, mname, sym))
                continue
            if blk not in f.by_label:
                fails.append("%s: runtime/models/%s names a block %s does not have: %s" % (name, mname, sym, blk))
                continue
            named_syms.add(sym)
            named_pairs.add((sym, blk))
    for sym in sorted(named_syms):
        for b in fns[sym].blocks:
            if (sym, b.label) in named_pairs:
                continue
            for line in b.lines:
                if _STEP_LINE.search(line):
                    fails.append("%s: runtime/models/: %s's block %s holds a step no model names: %s" % (name, sym, b.label, line.strip()[:120]))
                    break
    return fails


# --- the models read a SECOND way: explicit-state search (1.5.6b step 4d; D-295) ------
#
# D-289 decides a model's bad predicates by unrolling it to a depth K under a
# preemption bound D and asking the solver. The models are SMALL (68 to 1,086
# reachable states when this was written), and plain breadth-first search reads
# a model's WHOLE reachable space in well under a second: no depth bound, no
# preemption bound, no solver. It is a second, independent reader of what a
# model MEANS -- the unroller (`npkg/floor_model.npk`) writes the SMT text for
# BOTH runners, so until this belt the two-runner rule never reached a model's
# meaning -- and it says what a bounded row cannot: that a bad state is
# reachable NOWHERE, not merely nowhere within K steps.
#
# The semantics are the unroller's, mirrored: one step per tick or a stutter; a
# step's `next` expressions read the PRE-state, the first binding of a variable
# wins, an unbound variable keeps its value; the ranges hold at every tick (a
# successor out of range is no transition: the unrolling drops it SILENTLY,
# which is why it is a finding here); the kernel library's rules expand exactly
# as `read_kernel` expands them; a stutter tick's thread is free, so thread
# changes are counted over real steps; a bad predicate is read at ticks 1..K,
# and a stutter keeps the initial state at tick 1.
#
# THE RUN IS GREEN ONLY WHEN BOTH READINGS HOLD -- the solver's rows and this
# search -- which is the disagreement check: no verdict is passed between them.
# `npkg/floor_explore.npk` is the twin, finding for finding, byte for byte.

MODEL_STATE_CAP = 200000
_MX_OPERAND_MAX = 2147483648
_MX_PRODUCT_MAX = 4611686018427387904


class _ModelProblem(Exception):
    pass


def _mx_num(a):
    return a is not None and (a.isdigit() or (a.startswith("-") and a[1:].isdigit()))


_MX_ARITY = {"and": (1, None), "or": (1, None), "+": (1, None), "*": (1, None), "-": (1, None),
             "not": (1, 1), "=>": (2, 2), "ite": (3, 3), "=": (2, None), "distinct": (2, None),
             "<": (2, None), "<=": (2, None), ">": (2, None), ">=": (2, None)}


def _mx_check(e, names):
    """THE EXPRESSION IS VALIDATED WHOLE, UP FRONT, depth-first and left to right -- the order the Nitpick
    twin compiles in, so the first problem is the same problem (evaluation short-circuits; validation
    does not)."""
    a = _atom(e)
    if a is not None:
        if _mx_num(a):
            if len(a) > 18:
                raise _ModelProblem("a numeral too large for the explicit reading: %s" % a)
            return
        if a in ("true", "false"):
            return
        if a not in names:
            raise _ModelProblem("an expression names no state variable: %s" % a)
        return
    if not isinstance(e, list) or not e or _atom(e[0]) is None or _atom(e[0]) == "":
        raise _ModelProblem("an expression that is neither a word nor an application")
    h, n = _atom(e[0]), len(e) - 1
    if h not in _MX_ARITY:
        raise _ModelProblem("an operator the models' grammar does not have: %s" % h)
    lo, hi = _MX_ARITY[h]
    if n < lo or (hi is not None and n > hi):
        raise _ModelProblem("an application with the wrong number of arguments: %s" % h)
    for x in e[1:]:
        _mx_check(x, names)


def _mx_operand(v):
    if v > _MX_OPERAND_MAX or v < -_MX_OPERAND_MAX:
        raise _ModelProblem("an expression's value leaves the range the explicit reading computes in")
    return v


def _mx_eval(e, env):
    """A validated expression's value (a proposition is True/False). The short-circuits and the fold order
    are the Nitpick twin's pool's: `=` compares each operand with the first, `distinct` every pair in order,
    a comparison chain adjacent pairs, `+ - *` fold left with every operand held under 2^31 in magnitude."""
    a = _atom(e)
    if a is not None:
        if _mx_num(a):
            return int(a)
        if a == "true":
            return True
        if a == "false":
            return False
        return env[a]
    h, args = _atom(e[0]), e[1:]
    if h == "and":
        return all(_mx_eval(x, env) for x in args)
    if h == "or":
        return any(_mx_eval(x, env) for x in args)
    if h == "not":
        return not _mx_eval(args[0], env)
    if h == "=>":
        return (not _mx_eval(args[0], env)) or bool(_mx_eval(args[1], env))
    if h == "ite":
        return _mx_eval(args[1], env) if _mx_eval(args[0], env) else _mx_eval(args[2], env)
    if h == "=":
        for x in args[1:]:
            if _mx_eval(args[0], env) != _mx_eval(x, env):
                return False
        return True
    if h == "distinct":
        for i in range(len(args)):
            for j in range(i + 1, len(args)):
                if _mx_eval(args[i], env) == _mx_eval(args[j], env):
                    return False
        return True
    if h in ("<", "<=", ">", ">="):
        ok = {"<": lambda p, q: p < q, "<=": lambda p, q: p <= q,
              ">": lambda p, q: p > q, ">=": lambda p, q: p >= q}[h]
        for i in range(len(args) - 1):
            if not ok(_mx_eval(args[i], env), _mx_eval(args[i + 1], env)):
                return False
        return True
    if h == "-" and len(args) == 1:
        return -_mx_operand(_mx_eval(args[0], env))
    acc = _mx_eval(args[0], env)
    for x in args[1:]:
        y = _mx_eval(x, env)
        _mx_operand(acc)
        _mx_operand(y)
        acc = acc + y if h == "+" else (acc - y if h == "-" else acc * y)
    return acc


def _mx_kernel(form, names):
    w = _atom(form[1]) if len(form) > 1 else None
    args = [_atom(x) for x in form[2:]]
    for x in args:
        if x is None or x == "":
            raise _ModelProblem("a kernel rule's arguments are state variables and numerals")
        if _mx_num(x):
            if len(x) > 18:
                raise _ModelProblem("a numeral too large for the explicit reading: %s" % x)
        elif x not in names:
            raise _ModelProblem("a kernel rule's arguments are state variables and numerals")
    def word(x):
        return ("atom", x)
    written = {"futex-wait": 2, "futex-wake": 0, "eventfd-read": 0, "eventfd-write": 0, "spurious": 0, "signal": 0}
    arity = {"futex-wait": 3, "futex-wake": 1, "eventfd-read": 1, "eventfd-write": 1, "spurious": 1, "signal": 2}
    if w in arity and len(args) == arity[w] and args[written[w]] not in names:
        raise _ModelProblem("a kernel rule's arguments are state variables and numerals")
    if w == "futex-wait" and len(args) == 3:
        return None, [(args[2], [word("ite"), [word("="), word(args[0]), word(args[1])], word("1"), word("0")])]
    if w in ("futex-wake", "eventfd-read") and len(args) == 1:
        return None, [(args[0], word("0"))]
    if w == "eventfd-write" and len(args) == 1:
        return None, [(args[0], word("1"))]
    if w == "spurious" and len(args) == 1:
        return [word("="), word(args[0]), word("1")], [(args[0], word("0"))]
    if w == "signal" and len(args) == 2:
        return None, [(args[0], word(args[1]))]
    raise _ModelProblem("a kernel rule the library does not know: %s" % w)


def _mx_body(forms, names):
    gd, nxt = None, []
    for f in forms:
        h = _atom(f[0]) if isinstance(f, list) and f else None
        if h == "ir":
            continue
        if h == "guard":
            _mx_check(f[1], names)
            gd = f[1] if gd is None else [("atom", "and"), gd, f[1]]
        elif h == "next":
            for b in f[1:]:
                if _atom(b[0]) not in names:
                    raise _ModelProblem("a next binding names no state variable: %s" % _atom(b[0]))
                _mx_check(b[1], names)
                nxt.append((_atom(b[0]), b[1]))
        elif h == "kernel":
            g, b = _mx_kernel(f, names)
            if g is not None:
                gd = g if gd is None else [("atom", "and"), gd, g]
            nxt += b
        else:
            raise _ModelProblem("a form the models' grammar does not have inside a step: %s" % h)
    first = {}
    for v, e in nxt:
        if v not in names:
            raise _ModelProblem("a next binding names no state variable: %s" % v)
        first.setdefault(v, e)
    return gd, first


class ExplicitModel:
    pass


def read_model_explicit(text):
    """One model file as the explicit reading sees it (raises _ModelProblem)."""
    forms = sexpr(text)
    if len(forms) != 1 or not isinstance(forms[0], list) or _atom(forms[0][0]) != "model":
        raise _ModelProblem("a model file is one (model NAME ...) form")
    sx = forms[0]
    m = ExplicitModel()
    m.name = _atom(sx[1]); m.vars = []; m.lo = {}; m.hi = {}; m.threads = []; m.steps = []
    m.bad = []; m.controls = []; m.depth = None; m.preempt = None; m.init = None
    for f in sx[2:]:
        if isinstance(f, list) and f and _atom(f[0]) == "state":
            for v in f[1:]:
                n = _atom(v[0]) if isinstance(v, list) and len(v) > 0 else None
                lo = _atom(v[1]) if isinstance(v, list) and len(v) > 1 else None
                hi = _atom(v[2]) if isinstance(v, list) and len(v) > 2 else None
                if not n or not _mx_num(lo) or not _mx_num(hi) or len(lo) > 18 or len(hi) > 18:
                    raise _ModelProblem("a state variable is (NAME LO HI) with numeral bounds")
                m.vars.append(n); m.lo[n] = int(lo); m.hi[n] = int(hi)
    names = set(m.vars)
    for f in sx[2:]:
        h = _atom(f[0]) if isinstance(f, list) and f else None
        if h in ("of", "state"):
            continue
        if h == "init":
            _mx_check(f[1], names)
            m.init = f[1]
        elif h == "thread":
            ti = len(m.threads); m.threads.append(_atom(f[1]))
            for st in f[2:]:
                gd, nx = _mx_body(st[2:], names)
                m.steps.append((ti, _atom(st[1]), gd, nx))
        elif h == "bad":
            _mx_check(f[2], names)
            m.bad.append((_atom(f[1]), f[2]))
        elif h in ("depth", "preempt"):
            nt = _atom(f[1]) if len(f) > 1 else None
            if not _mx_num(nt) or len(nt) > 18:
                raise _ModelProblem("a model needs (state ...), (init ...), (depth K) and (preempt D)")
            if h == "depth":
                m.depth = int(nt)
            else:
                m.preempt = int(nt)
        elif h == "control":
            muts = []
            for mu in f[3:]:
                k = _atom(mu[0])
                if k == "replace":
                    gd, nx = _mx_body(mu[3:], names)
                    muts.append((k, _atom(mu[1]), _atom(mu[2]), gd, nx))
                elif k == "remove":
                    muts.append((k, _atom(mu[1]), _atom(mu[2]), None, None))
                else:
                    raise _ModelProblem("a mutation the models' grammar does not have: %s" % k)
            m.controls.append((_atom(f[1]), _atom(f[2]), muts))
        else:
            raise _ModelProblem("a form the models' grammar does not have: %s" % h)
    if m.init is None or m.depth is None or m.preempt is None or not m.vars or m.depth < 0 or m.preempt < 0:
        raise _ModelProblem("a model needs (state ...), (init ...), (depth K) and (preempt D)")
    return m


def _mx_steps_under(m, ctl):
    if ctl is None:
        return m.steps
    out = []
    for (ti, nm, gd, nx) in m.steps:
        hit = [mu for mu in ctl[2] if mu[1] in m.threads and m.threads.index(mu[1]) == ti and mu[2] == nm]
        if not hit:
            out.append((ti, nm, gd, nx))
            continue
        for mu in hit:
            if mu[0] == "replace":
                out.append((ti, nm, mu[3], mu[4]))
    return out


def _mx_initial(m):
    """The initial states: every assignment of the variables `init` leaves free, in declaration order."""
    pin = {}
    if isinstance(m.init, list) and _atom(m.init[0]) == "and":
        for c in m.init[1:]:
            if isinstance(c, list) and len(c) == 3 and _atom(c[0]) == "=" and _atom(c[1]) in m.lo and _mx_num(_atom(c[2])) and len(_atom(c[2])) <= 18:
                pin.setdefault(_atom(c[1]), int(_atom(c[2])))
    free = [v for v in m.vars if v not in pin]
    cur = [dict(pin)]
    for v in free:
        cur = [dict(c, **{v: x}) for c in cur for x in range(m.lo[v], m.hi[v] + 1)]
        if len(cur) > MODEL_STATE_CAP:
            raise _ModelProblem("too-large")
    return [tuple(c[v] for v in m.vars) for c in cur if _mx_eval(m.init, c)]


def explore_model(m, ctl=None):
    """(dist, minsw, inside, blocked): least depth per reachable state, least thread changes per reachable
    state (no step bound), the states reachable within (K, D), and the first variable (declaration order) each
    step can take out of its range from a reachable state."""
    from collections import deque
    steps = _mx_steps_under(m, ctl)
    V = m.vars
    blocked = {}
    cache = {}

    def succ(s):
        r = cache.get(s)
        if r is not None:
            return r
        env = dict(zip(V, s)); r = []
        for (ti, nm, gd, nx) in steps:
            if gd is not None and not _mx_eval(gd, env):
                continue
            ns = tuple(_mx_eval(nx[v], env) if v in nx else env[v] for v in V)
            out = [v for v, x in zip(V, ns) if not (m.lo[v] <= x <= m.hi[v])]
            if out:
                key = (ti, nm)
                vi = V.index(out[0])
                if key not in blocked or vi < blocked[key]:
                    blocked[key] = vi
                continue
            r.append((ti, ns))
        cache[s] = r
        return r

    prod = 1
    for v in V:
        radix = m.hi[v] - m.lo[v] + 1
        if radix < 1 or prod > _MX_PRODUCT_MAX // radix:
            raise _ModelProblem("too-large")     # the twin's state is one int64: the same refusal here
        prod *= radix
    inits = _mx_initial(m)
    dist = {s: 0 for s in inits}
    q = deque(inits)
    while q:
        s = q.popleft()
        for (_, ns) in succ(s):
            if ns not in dist:
                dist[ns] = dist[s] + 1
                q.append(ns)
                if len(dist) > MODEL_STATE_CAP:
                    raise _ModelProblem("too-large")
    INF = 10 ** 9
    sw = {}
    dq = deque()
    for s in inits:
        for (ti, ns) in succ(s):
            if sw.get((ns, ti), INF) > 0:
                sw[(ns, ti)] = 0
                dq.appendleft((ns, ti))
    while dq:
        (s, t) = dq.popleft()
        c = sw[(s, t)]
        for (ti, ns) in succ(s):
            nc = c + (0 if ti == t else 1)
            if sw.get((ns, ti), INF) > nc:
                sw[(ns, ti)] = nc
                (dq.appendleft if ti == t else dq.append)((ns, ti))
    minsw = {}
    for (s, t), c in sw.items():
        if c < minsw.get(s, INF):
            minsw[s] = c
    for s in inits:
        minsw[s] = 0
    layer = {}
    for s in inits:
        for (ti, ns) in succ(s):
            layer[(ns, ti)] = 0
    best = dict(layer)
    for _k in range(2, m.depth + 1):
        nxt = {}
        for (s, t), c in layer.items():
            for (ti, ns) in succ(s):
                nc = c + (0 if ti == t else 1)
                if nc < nxt.get((ns, ti), INF):
                    nxt[(ns, ti)] = nc
        layer = {}
        for key, c in nxt.items():
            if c < best.get(key, INF):
                best[key] = c
                layer[key] = c
    inside = set(inits)
    for (s, t), c in best.items():
        if c <= m.preempt:
            inside.add(s)
    return dist, minsw, inside, blocked


def model_facts(m):
    """(reachable, inside) for TCB.md's generated sentence."""
    dist, _, inside, _ = explore_model(m)
    return len(dist), len([s for s in dist if s in inside])


def check_model_explicit(mname, text, name="floor"):
    """One model's explicit reading: its findings, by name (the twin of `floor_explore_model`)."""
    head = "%s: runtime/models/%s: " % (name, mname)
    try:
        m = read_model_explicit(text)
        dist, minsw, inside, blocked = explore_model(m)
        fails = []
        V = m.vars
        for (ti, nm) in sorted(blocked, key=lambda k: [(s[0], s[1]) for s in m.steps].index(k)):
            fails.append(head + "floor-model-range-blocks: step `%s.%s` can take `%s` out of its range from a reachable state -- the unrolling drops that transition silently"
                         % (m.threads[ti], nm, V[blocked[(ti, nm)]]))
        for (bn, bp) in m.bad:
            hits = [s for s in dist if _mx_eval(bp, dict(zip(V, s)))]
            if not hits:
                continue
            where = ("INSIDE the model's bounds (K %d, D %d): a discharged row says the opposite, so one of the two readings is wrong"
                     if any(s in inside for s in hits) else
                     "outside the model's bounds (K %d, D %d): the bounded rows cannot see it") % (m.depth, m.preempt)
            fails.append(head + "floor-model-bad-reachable: `%s` is reachable -- least depth %d, least thread changes %d, %s"
                         % (bn, min(dist[s] for s in hits), min(minsw[s] for s in hits), where))
        bads = dict(m.bad)
        for ctl in m.controls:
            if ctl[1] not in bads:
                fails.append(head + "floor-model-unreadable: a control names no bad predicate: %s" % ctl[0])
                continue
            cdist, cminsw, cinside, _ = explore_model(m, ctl)
            hits = [s for s in cdist if _mx_eval(bads[ctl[1]], dict(zip(V, s)))]
            if any(s in cinside for s in hits):
                continue
            why = ("it is reachable nowhere" if not hits else
                   "it needs depth %d and %d thread change(s)" % (min(cdist[s] for s in hits), min(cminsw[s] for s in hits)))
            fails.append(head + "floor-model-control-unreachable: control `%s` does not reach `%s` inside the bounds (K %d, D %d) -- %s; the solver's `sat` says the opposite, so one of the two readings is wrong"
                         % (ctl[0], ctl[1], m.depth, m.preempt, why))
        return fails
    except _ModelProblem as e:
        if str(e) == "too-large":
            return [head + "floor-model-too-large: more than %d states -- the explicit reading refuses a model it cannot finish, and never skips one (D-295)" % MODEL_STATE_CAP]
        return [head + "floor-model-unreadable: %s" % e]
    except (ValueError, IndexError, TypeError) as e:
        return [head + "floor-model-unreadable: %s" % e]


def read_model_texts(root):
    """Every `runtime/models/*.model`, sorted by name: (name, text)."""
    import glob
    out = []
    for path in sorted(glob.glob(os.path.join(root, "runtime", "models", "*.model"))):
        out.append((os.path.basename(path)[:-len(".model")], open(path, encoding="utf-8").read()))
    return out


def check_models_explicit(root, name="floor"):
    """THE MODELS' SECOND READING (D-295): every model's whole reachable space searched."""
    fails = []
    for mname, text in read_model_texts(root):
        fails += check_model_explicit(mname, text, name)
    return fails


def disposition(sections, sym, cls, rows, models=()):
    """One symbol's disposition in D-288 §2.1's words (the twin of
    `floor_disposition`): `trusted (inline asm)`; `specified (N discharged, M
    residue)` from the committed floor manifest's `floor-spec` rows; the
    `residue (...)` and `boundary (...)` sentences; `modelled (a, b)` for the
    models whose steps name a block of the symbol (1.5.6 step 5); else the
    class default."""
    if cls == "asm":
        return "trusted (inline asm)"
    modelled = [mname for mname, pairs in models if any(s == sym for s, _ in pairs)]
    sec = dict(sections).get(sym)
    if sec is None:
        if modelled:
            return "modelled (%s)" % ", ".join(modelled)
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
    if modelled:
        parts.append("modelled (%s)" % ", ".join(modelled))
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


# --- the syscall boundary (1.5.6 step 6; D-288 §2.8) -------------------------------
#
# THE NUMBERS THE FLOOR ISSUES, with the names the kernel gives them. A
# number the floor uses and this table does not name is a finding: the
# kernel-effect table in `npkg/floor_smt.npk` has one row per number, and
# TCB.md's reader accepts those rows as what the kernel promises.
def kernel_effects():
    """THE KERNEL-EFFECT TABLE, READ FROM ITS ONE AUTHORITY (1.5.6b): the
    `kernel-effects` region of VERIFICATION_REFERENCE SS9.2, through the
    generator's own strict parser -- no second parser and no second list. Until
    1.5.6b this module carried a name table of its own, `npkg/floor.npk` a twin
    of it, and `npkg/floor_smt.npk` two effect tables; two of the rows were
    wrong and nothing held any of the four to another. nr -> the row:
    (nr, name, option arg, option values, effect, buffer arg, length, bound arg)."""
    import os, sys
    gen = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "generator")
    if gen not in sys.path:
        sys.path.insert(0, gen)
    import gen_tables
    return {r[0]: r for r in gen_tables.kernel_effect_rows(gen_tables.VERIF)}


class _Names(dict):
    """`SYSCALL_NAMES[n]`, filled from the authority on first use."""
    def _fill(self):
        if not dict.__len__(self):
            for nr, row in kernel_effects().items():
                dict.__setitem__(self, nr, row[1])
    def __contains__(self, k):
        self._fill(); return dict.__contains__(self, k)
    def get(self, k, d=None):
        self._fill(); return dict.get(self, k, d)
    def __getitem__(self, k):
        self._fill(); return dict.__getitem__(self, k)


SYSCALL_NAMES = _Names()


def syscall_map(floor_text):
    """Per define: the syscall numbers it issues DIRECTLY (`npk_sys6(NR, …)`)
    and the numbers it can reach transitively through the call graph. The
    walk is `_floor_classes`'s, over code lines only."""
    fns, order = parse_floor(floor_text)
    direct = {}
    graph = {}
    for name, fn in fns.items():
        nrs = set()
        callees = set()
        for b in fn.blocks:
            for line in b.lines:
                for m in re.finditer(r"call i64 @npk_sys6\(i64 (-?\d+)", line):
                    nrs.add(int(m.group(1)))
                for m in re.finditer(r"call[^@\n]*(@[\w.$-]+)", line):
                    if m.group(1) in fns:
                        callees.add(m.group(1))
        direct[name] = nrs
        graph[name] = callees
    trans = {}

    def walk(n, seen):
        if n in seen:
            return set()
        seen.add(n)
        out = set(direct[n])
        for c in graph[n]:
            out |= walk(c, seen)
        return out
    for name in fns:
        trans[name] = walk(name, set())
    return order, direct, trans


# The trap route's entries. EVERY symbol reaches `exit_group` through a trap,
# so a transitive set that counts the route says nothing about the symbol:
# the table reports what a symbol reaches ON ITS OWN PATHS and says "and the
# trap route" where it can also trap.
TRAP_ENTRIES = ("@npk_trap", "@npk_raise", "@npk_heap_badreq", "@npk_heap_oom", "@npk_heap_bad")


def syscall_rows(floor_text, classes):
    """TCB.md's syscall-table rows, in the floor's own order: every symbol
    that issues or reaches a syscall, its direct numbers and the numbers it
    reaches without going through the trap route, with a note when it can
    trap. A symbol that reaches none of either is not a row."""
    order, direct, trans = syscall_map(floor_text)
    own, traps = syscall_map_own(floor_text)
    def fmt(s):
        return ", ".join("%d %s" % (n, SYSCALL_NAMES.get(n, "?")) for n in sorted(s)) or "--"
    rows = []
    for name in order:
        # a symbol that issues nothing and reaches nothing ON ITS OWN paths is
        # not a row: the trap entries themselves reach the whole route, and a
        # row saying `-- | --` about them is noise, not a boundary.
        if not own[name] and not traps[name] and not direct[name]:
            continue
        reach = fmt(own[name])
        if traps[name]:
            reach = (reach + ", and the trap route") if own[name] else "the trap route only"
        rows.append("| `%s` | %s | %s | %s |" % (name, classes.get(name, "?"), fmt(direct[name]), reach))
    return rows


def syscall_map_own(floor_text):
    """Per define: the numbers it reaches WITHOUT passing through a trap
    entry, and whether it can reach one."""
    fns, order = parse_floor(floor_text)
    direct = {}
    graph = {}
    for name, fn in fns.items():
        nrs = set()
        callees = set()
        for b in fn.blocks:
            for line in b.lines:
                for m in re.finditer(r"call i64 @npk_sys6\(i64 (-?\d+)", line):
                    nrs.add(int(m.group(1)))
                for m in re.finditer(r"call[^@\n]*(@[\w.$-]+)", line):
                    if m.group(1) in fns:
                        callees.add(m.group(1))
        direct[name] = nrs
        graph[name] = callees
    own = {}
    traps = {}

    def walk(n, seen):
        if n in seen:
            return set(), False
        seen.add(n)
        out = set(direct[n])
        hit = False
        for c in graph[n]:
            if c in TRAP_ENTRIES:
                hit = True
                continue
            sub, subhit = walk(c, seen)
            out |= sub
            hit = hit or subhit
        return out, hit
    for name in fns:
        o, h = walk(name, set())
        own[name] = o
        traps[name] = h and name not in TRAP_ENTRIES
    return own, traps


def syscalls_region(floor_text, classes):
    """The whole marked region's body: the numbers once, then the table."""
    order, direct, trans = syscall_map(floor_text)
    used = sorted({n for s in direct.values() for n in s})
    head = ("The floor issues %d syscall numbers, and no others: %s. Each has one row in the\n"
            "kernel-effect table -- the `kernel-effects` region of VERIFICATION_REFERENCE §9.2, generated\n"
            "into `npkg/floor_kernel.npk` -- saying what it does to memory and to the result, and the\n"
            "belt holds that sentence: a number without a row, or a call site whose option the row does\n"
            "not speak for, is a finding. The rows that WRITE memory are held to the running kernel by\n"
            "`tests/backend/programs/kernel_effects.npk`; what no probe reaches is what a reader accepts (§5).\n"
            % (len(used), ", ".join("**%d** %s" % (n, SYSCALL_NAMES.get(n, "?")) for n in used)))
    return head + "\n" + "\n".join(["| symbol | class | issues | reaches |", "|---|---|---|---|"]
                                    + syscall_rows(floor_text, classes))


def check_syscall_names(floor_text):
    """Every number the floor issues has a ROW in the kernel-effect table (not
    merely a name: TCB.md's generated head said "each has one row" while
    `clone` and `execve` had none, until 1.5.6b gave them their `asm` rows),
    and every `npk_sys6` call site of the floor -- translated or not -- passes,
    where its row's effect depends on an option, a NUMERAL the row speaks for.
    `arch_prctl(ARCH_GET_FS)` and `prctl(PR_GET_NAME)` WRITE user memory where
    the options the floor passes do not; a row keyed by number alone would
    model either as writing nothing. `npkg/floor.npk` is the twin."""
    rows = kernel_effects()
    _, direct, _ = syscall_map(floor_text)
    used = sorted({n for s in direct.values() for n in s})
    fails = ["floor: floor-syscall-row: runtime/npkrt.ll issues syscall %d, which the kernel-effect table has no row "
             "for -- give it one in VERIFICATION_REFERENCE SS9.2's `kernel-effects` region and regenerate" % n
             for n in used if n not in rows]
    fns, order = parse_floor(floor_text)
    for fname in order:
        for b in fns[fname].blocks:
            for line in b.lines:
                cm = re.search(r'@npk_sys6\(([^)]*)\)', line)
                if not cm:
                    continue
                args = [a.strip().split(" ", 1)[-1].strip() for a in cm.group(1).split(",")]
                if not args or not re.fullmatch(r"\d+", args[0]) or int(args[0]) not in rows:
                    continue
                nr = int(args[0])
                oa, ovals = rows[nr][2], rows[nr][3]
                if oa == 0:
                    continue
                val = args[oa] if oa < len(args) else ""
                if not re.fullmatch(r"-?\d+", val) or int(val) not in ovals:
                    fails.append("floor: floor-syscall-option: `%s` issues syscall %d (%s) with argument %d = `%s`, which the "
                                 "kernel-effect table's row does not speak for -- an option the row does not list may "
                                 "write memory the row says it does not (add the option to the row, measured, or do "
                                 "not pass it)" % (fname, nr, rows[nr][1], oa, val))
    return fails


def residue_rows(spec_text, manifest_text, models=(), facts=()):
    """TCB.md's residue region (1.5.6 step 6; D-288 §2.10): every `budget`
    row by symbol and site -- the rows the profile did not decide, which the
    verdict rule keeps as residue rather than a proof -- then every
    `(residue "…")` sentence the spec carries, then the models' standing
    residue. One place that says what the floor's evidence does NOT cover."""
    lines = []
    rows = []
    for l in (manifest_text or "").splitlines():
        if not l.strip() or l.startswith("#"):
            continue
        p = l.split(" ", 5)
        if len(p) == 6 and p[3] == "budget":
            rows.append((p[5].strip(), p[1]))
    lines.append("**Rows the profile did not decide** (`budget`; the verdict rule keeps them as")
    lines.append("residue, never as a proof). %s:" % ("%d of them" % len(rows) if rows else "None"))
    lines.append("")
    by = {}
    for sym, kind in rows:
        by.setdefault(sym, 0)
        by[sym] += 1
    for sym in sorted(by):
        lines.append("- `%s` -- %d row(s); the section's `(residue ...)` sentence says why." % (sym, by[sym]))
    lines.append("")
    lines.append("**Sentences the spec carries**, each a claim NOT made:")
    lines.append("")
    for sym, sec in spec_sections(spec_text):
        for cl in sec:
            if isinstance(cl, list) and cl and _atom(cl[0]) == "residue" and len(cl) > 1:
                lines.append("- `%s` -- %s" % (sym, _string(cl[1])))
    lines.append("")
    # THE MODELS ARE READ TWICE (1.5.6b step 4d; D-295): the solver's bounded rows,
    # and the explicit-state search over each model's whole reachable space --
    # `facts` is that search's (name, K, D, reachable, inside) per model, and a
    # bad state reachable anywhere is a red run (`check_models_explicit`), which
    # is what lets this paragraph say "no bad state" without a bound.
    lines.append("**The models' residue** (VERIFICATION_REFERENCE SS9.4): every model is read twice --")
    lines.append("bounded, by the solver (its rows hold to their own depth and preemption bound and no")
    lines.append("further), and exhaustively, by explicit-state search over its whole reachable space")
    lines.append("(D-295), which is where the SAFETY claim rests: no bad state is reachable in any model")
    lines.append("below, inside its bounds or outside them. LIVENESS is not claimed at all -- that a due")
    lines.append("task is eventually run, and that the shared arena's walker stops spinning, need a")
    lines.append("fairness assumption neither reading can state.")
    if facts:
        lines.append("The models, each with its bounds and its reachable states (and how many of them the bounds reach): %s."
                     % ", ".join("`%s` (K %d, D %d; %d states, %d inside)" % f for f in facts))
    elif models:
        lines.append("The bounds in force: %s." % ", ".join("`%s`" % m[0] for m in models))
    return lines


def residue_region(spec_text, manifest_text, models=(), facts=()):
    return "\n".join(residue_rows(spec_text, manifest_text, models, facts))


def model_facts_all(root):
    """(name, K, D, reachable, inside) per model, sorted by name -- TCB.md's generated sentence. A model
    the explicit reading cannot read contributes nothing here; `check_models_explicit` names it."""
    out = []
    for mname, text in read_model_texts(root):
        try:
            m = read_model_explicit(text)
            reach, inside = model_facts(m)
            out.append((mname, m.depth, m.preempt, reach, inside))
        except (_ModelProblem, ValueError, IndexError, TypeError):
            continue
    return out


def tcb_rows(spec_text, classes, manifest_text, models=()):
    """TCB.md's floor-table rows, sorted: the table's `| symbol | class | disposition |` lines."""
    sections = spec_sections(spec_text)
    rows = manifest_rows_of(manifest_text) if manifest_text else []
    return sorted("| `%s` | %s | %s |" % (sym, cls, disposition(sections, sym, cls, rows, models))
                  for sym, cls in classes.items())


def tcb_region(spec_text, classes, manifest_text, models=()):
    """The whole marked region's body, header row included."""
    return "\n".join(["| symbol | class | disposition |", "|---|---|---|"] + tcb_rows(spec_text, classes, manifest_text, models))
