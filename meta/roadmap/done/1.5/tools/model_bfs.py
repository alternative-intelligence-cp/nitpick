#!/usr/bin/env python3
"""model_bfs.py -- an EXPLICIT-STATE reading of a floor protocol model (runtime/models/*.model), as a MEASUREMENT
instrument beside the SMT unrolling (npkg/floor_model.npk). It answers what the bounded rows cannot:

  * the model's whole reachable state space (no depth bound, no preemption bound), and whether ANY bad predicate is
    reachable in it -- so "unsat at K, D" can be compared with "unreachable, full stop (within the model)";
  * the DIAMETER (the largest least-number-of-steps over reachable states) and, per state, the least number of thread
    changes -- so whether the model's (depth K) (preempt D) covers every reachable state (R(K, D) == Reach);
  * per control: the least depth and thread changes at which its bad predicate becomes reachable.

It mirrors the unroller's semantics, read from floor_model.npk: one step per tick or a stutter; a step's `next`
expressions read the PRE-state, the first binding of a variable wins, unbound variables keep their values; the
ranges hold at every tick (a successor out of range is NOT a transition -- counted and reported, because a range
that blocks a step silently removes behaviour); the thread index of a stutter tick is free, so thread changes are
counted over the real steps only; bad predicates are read at ticks 1..K -- and a stutter keeps the initial state at
tick 1, so the initial state is read too.

Not a belt and not a verdict: the rows are z3's. Written at 1.5.6b step 2 (lead E-3), where its table for all seven
models is recorded (meta/roadmap/done/1.5/1.5.6b.md); whether it becomes a standing belt in both runners is S-75
(OPEN_DECISIONS SS2e). Kept here for the record, as this directory's other script is.

usage: python3 meta/roadmap/done/1.5/tools/model_bfs.py runtime/models/*.model
"""
import sys
from collections import deque

def tokenize(text):
    toks, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == ';':
            while i < n and text[i] != '\n':
                i += 1
        elif c in '()':
            toks.append(c); i += 1
        elif c == '"':
            j = i + 1
            while text[j] != '"':
                j += 1
            toks.append(('str', text[i + 1:j])); i = j + 1
        elif c.isspace():
            i += 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in '()':
                j += 1
            toks.append(text[i:j]); i = j
    return toks

def parse(toks):
    pos = 0
    def rd():
        nonlocal pos
        t = toks[pos]; pos += 1
        if t == '(':
            out = []
            while toks[pos] != ')':
                out.append(rd())
            pos += 1
            return out
        if t == ')':
            raise SystemExit("unbalanced )")
        return t
    out = rd()
    if pos != len(toks):
        raise SystemExit("trailing text after the model form")
    return out

def is_num(a):
    return isinstance(a, str) and (a.isdigit() or (a.startswith('-') and a[1:].isdigit()))

def ev(e, env):
    if isinstance(e, str):
        if is_num(e): return int(e)
        if e == 'true': return True
        if e == 'false': return False
        return env[e]
    h, a = e[0], e[1:]
    if h == 'and': return all(ev(x, env) for x in a)
    if h == 'or': return any(ev(x, env) for x in a)
    if h == 'not': return not ev(a[0], env)
    if h == '=>': return (not ev(a[0], env)) or ev(a[1], env)
    if h == 'ite': return ev(a[1], env) if ev(a[0], env) else ev(a[2], env)
    if h == '=':
        v = [ev(x, env) for x in a]; return all(x == v[0] for x in v)
    if h == 'distinct':
        v = [ev(x, env) for x in a]; return len(set(v)) == len(v)
    if h in ('<', '<=', '>', '>='):
        v = [ev(x, env) for x in a]
        ok = {'<': lambda p, q: p < q, '<=': lambda p, q: p <= q, '>': lambda p, q: p > q, '>=': lambda p, q: p >= q}[h]
        return all(ok(v[i], v[i + 1]) for i in range(len(v) - 1))
    if h == '+': return sum(ev(x, env) for x in a)
    if h == '-':
        v = [ev(x, env) for x in a]
        return -v[0] if len(v) == 1 else v[0] - sum(v[1:])
    if h == '*':
        r = 1
        for x in a: r *= ev(x, env)
        return r
    raise SystemExit("an operator this reader does not know: %r" % (h,))

def kernel_rule(form, names):
    w, args = form[1], form[2:]
    for x in args:
        if not (x in names or is_num(x)):
            raise SystemExit("a kernel rule names no state variable: %r" % (x,))
    if w == 'futex-wait': return None, [(args[2], ['ite', ['=', args[0], args[1]], '1', '0'])]
    if w in ('futex-wake', 'eventfd-read'): return None, [(args[0], '0')]
    if w == 'eventfd-write': return None, [(args[0], '1')]
    if w == 'spurious': return ['=', args[0], '1'], [(args[0], '0')]
    if w == 'signal': return None, [(args[0], args[1])]
    raise SystemExit("a kernel rule the library does not know: %r" % (w,))

def read_body(forms, names):
    """guard + ordered next bindings, in the order the forms appear (the unroller's first-binding-wins)."""
    gd, nxt = 'true', []
    for f in forms:
        if f[0] == 'ir': continue
        if f[0] == 'guard': gd = f[1] if gd == 'true' else ['and', gd, f[1]]
        elif f[0] == 'next': nxt += [(b[0], b[1]) for b in f[1:]]
        elif f[0] == 'kernel':
            g, b = kernel_rule(f, names)
            if g is not None: gd = g if gd == 'true' else ['and', gd, g]
            nxt += b
        else: raise SystemExit("a form this reader does not know inside a step: %r" % (f[0],))
    first = {}
    for v, e in nxt:
        if v not in names: raise SystemExit("a next binding names no state variable: %r" % (v,))
        first.setdefault(v, e)
    return gd, first

class Model: pass

def load(path):
    sx = parse(tokenize(open(path).read()))
    if sx[0] != 'model': raise SystemExit("not a model")
    m = Model(); m.name = sx[1]; m.vars = []; m.lo = {}; m.hi = {}; m.steps = []; m.threads = []
    m.bad = []; m.controls = []; m.depth = m.preempt = None; m.init = None
    for f in sx[2:]:
        if f[0] == 'state':
            for v in f[1:]:
                m.vars.append(v[0]); m.lo[v[0]] = int(v[1]); m.hi[v[0]] = int(v[2])
    names = set(m.vars)
    for f in sx[2:]:
        h = f[0]
        if h in ('of', 'state'): continue
        if h == 'init': m.init = f[1]
        elif h == 'thread':
            ti = len(m.threads); m.threads.append(f[1])
            for st in f[2:]:
                gd, nx = read_body(st[2:], names)
                m.steps.append((ti, st[1], gd, nx))
        elif h == 'bad': m.bad.append((f[1], f[2]))
        elif h == 'depth': m.depth = int(f[1])
        elif h == 'preempt': m.preempt = int(f[1])
        elif h == 'control':
            muts = []
            for mu in f[3:]:
                if mu[0] == 'replace':
                    gd, nx = read_body(mu[3:], names); muts.append(('replace', mu[1], mu[2], gd, nx))
                elif mu[0] == 'remove': muts.append(('remove', mu[1], mu[2], None, None))
                else: raise SystemExit("a mutation this reader does not know: %r" % (mu[0],))
            m.controls.append((f[1], f[2], muts))
        else: raise SystemExit("a form this reader does not know: %r" % (h,))
    return m

def steps_under(m, ctl):
    if ctl is None: return m.steps
    out = []
    for (ti, nm, gd, nx) in m.steps:
        hit = [mu for mu in ctl[2] if m.threads.index(mu[1]) == ti and mu[2] == nm]
        if not hit: out.append((ti, nm, gd, nx)); continue
        for mu in hit:
            if mu[0] == 'replace': out.append((ti, nm, mu[3], mu[4]))
    return out

def initial_states(m):
    # the init predicate may leave variables free: enumerate the ranges (tiny) and keep what satisfies it
    out, cur = [], [{}]
    for v in m.vars:
        cur = [dict(c, **{v: x}) for c in cur for x in range(m.lo[v], m.hi[v] + 1)]
        if len(cur) > 4_000_000: raise SystemExit("the state product is too large to enumerate for init")
    for c in cur:
        if ev(m.init, c): out.append(tuple(c[v] for v in m.vars))
    return out

def fast_initial(m):
    # every model in the tree pins each variable with (= v n) in a conjunction: read that directly, else enumerate
    if isinstance(m.init, list) and m.init[0] == 'and':
        pin = {}
        for c in m.init[1:]:
            if isinstance(c, list) and c[0] == '=' and c[1] in m.vars and is_num(c[2]): pin[c[1]] = int(c[2])
        if len(pin) == len(m.vars): return [tuple(pin[v] for v in m.vars)]
        # some variables are left free by init (a symbolic parameter, e.g. shared-arena's `widx`): enumerate THOSE only
        free = [v for v in m.vars if v not in pin]
        cur = [dict(pin)]
        for v in free:
            cur = [dict(c, **{v: x}) for c in cur for x in range(m.lo[v], m.hi[v] + 1)]
            if len(cur) > 1_000_000: raise SystemExit("too many free initial variables to enumerate")
        return [tuple(c[v] for v in m.vars) for c in cur if ev(m.init, c)]
    return initial_states(m)

def explore(m, ctl):
    steps = steps_under(m, ctl)
    V = m.vars
    blocked = {}
    succ_cache = {}
    def succ(s):
        r = succ_cache.get(s)
        if r is not None: return r
        env = dict(zip(V, s)); r = []
        for (ti, nm, gd, nx) in steps:
            if gd != 'true' and not ev(gd, env): continue
            ns = tuple(ev(nx[v], env) if v in nx else env[v] for v in V)
            if any(not (m.lo[v] <= x <= m.hi[v]) for v, x in zip(V, ns)):
                blocked[(m.threads[ti], nm)] = blocked.get((m.threads[ti], nm), 0) + 1; continue
            r.append((ti, nm, ns))
        succ_cache[s] = r
        return r
    inits = fast_initial(m)
    dist = {s: 0 for s in inits}; q = deque(inits)
    while q:
        s = q.popleft()
        for (_, _, ns) in succ(s):
            if ns not in dist: dist[ns] = dist[s] + 1; q.append(ns)
    # least thread changes with NO step bound: 0-1 BFS over (state, last thread)
    INF = 10 ** 9
    sw = {}; dq = deque()
    for s in inits:
        for (ti, _, ns) in succ(s):
            if sw.get((ns, ti), INF) > 0: sw[(ns, ti)] = 0; dq.appendleft((ns, ti))
    while dq:
        (s, t) = dq.popleft(); c = sw[(s, t)]
        for (ti, _, ns) in succ(s):
            nc = c + (0 if ti == t else 1)
            if sw.get((ns, ti), INF) > nc:
                sw[(ns, ti)] = nc
                (dq.appendleft if ti == t else dq.append)((ns, ti))
    minsw = {}
    for (s, t), c in sw.items():
        if c < minsw.get(s, INF): minsw[s] = c
    for s in inits: minsw[s] = 0
    # R(K, D): least thread changes within at most k real steps, layer by layer
    K, D = m.depth, m.preempt
    layer = {}
    for s in inits:
        for (ti, _, ns) in succ(s):
            layer[(ns, ti)] = 0
    best = dict(layer); first_k = {}
    for (s, t), c in layer.items(): first_k.setdefault(s, (1, c))
    for k in range(2, K + 1):
        nxt = {}
        for (s, t), c in layer.items():
            for (ti, _, ns) in succ(s):
                nc = c + (0 if ti == t else 1)
                if nc < nxt.get((ns, ti), INF): nxt[(ns, ti)] = nc
        layer = {}
        for key, c in nxt.items():
            if c < best.get(key, INF): best[key] = c; layer[key] = c
    inR = set(inits)
    for (s, t), c in best.items():
        if c <= D: inR.add(s)
    return dist, minsw, inR, blocked, best

def bounded_least(m, ctl, pred):
    """under a control: the least depth and the least thread changes at which pred is reachable with NO bound, and
    whether some state satisfying it lies inside R(K, D) -- which is what z3's `sat` for the control needs."""
    dist, minsw, inR, blocked, best = explore(m, ctl)
    V = m.vars
    hits = [s for s in dist if ev(pred, dict(zip(V, s)))]
    if not hits: return None
    k = min(dist[s] for s in hits); c = min(minsw[s] for s in hits)
    within = any(s in inR for s in hits)
    return k, c, within

def report(path):
    m = load(path)
    dist, minsw, inR, blocked, best = explore(m, None)
    V = m.vars
    diam = max(dist.values()); maxsw = max(minsw.values())
    print("== %s   K=%d D=%d   threads %s" % (m.name, m.depth, m.preempt, ", ".join(m.threads)))
    print("   reachable states (no bound): %d;  diameter %d step(s);  largest least-thread-changes %d" % (len(dist), diam, maxsw))
    miss = [s for s in dist if s not in inR]
    print("   R(K, D) covers %d of %d reachable state(s)%s" % (len(dist) - len(miss), len(dist),
          "  -- THE BOUNDS ARE COMPLETE for state predicates" if not miss else "  -- %d state(s) lie OUTSIDE the bounds" % len(miss)))
    if blocked:
        print("   steps a RANGE blocked (behaviour silently removed): %s" % ", ".join("%s.%s x%d" % (t, n, c) for (t, n), c in sorted(blocked.items())))
    for (bn, bp) in m.bad:
        hits = [s for s in dist if ev(bp, dict(zip(V, s)))]
        if hits:
            s = min(hits, key=lambda x: dist[x])
            print("   BAD %-32s REACHABLE with no bound: least depth %d, least changes %d, inside R(K,D): %s  e.g. %s" % (
                  bn, dist[s], min(minsw[x] for x in hits), any(x in inR for x in hits), dict(zip(V, s))))
        else:
            print("   bad %-32s unreachable in the whole state space" % bn)
    for ctl in m.controls:
        bp = dict(m.bad)[ctl[1]]
        r = bounded_least(m, ctl, bp)
        if r is None: print("   CONTROL %-28s -> %s: BLIND -- unreachable with no bound" % (ctl[0], ctl[1]))
        else: print("   control %-28s -> %-28s least depth %2d, least changes %d, inside R(K,D): %s" % (ctl[0], ctl[1], r[0], r[1], r[2]))

for p in sys.argv[1:]:
    report(p)
