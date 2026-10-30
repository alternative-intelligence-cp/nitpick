#!/usr/bin/env python3 -I
"""kindtable167.py WORKTREE [--min N] -- THE MECHANICAL TABLE of expression kinds against each walker (management_6, 2026-10-29 06:41: "a script
that lists every ExprKind against each walker's arms"). The kinds and their shapes come from `expr_shape` in src/frontend/ast.npk; a walker is a
function of escape.npk, alias.npk or locks.npk that names ExprKind values (at least N of them, default 2). For each (kind, walker): `arm` = the
walker names the kind; `shape` = it does not, but the body calls a shape-generic fallback (`expr_shape`, `escape_children`, `acc_children`); `-` = no
arm and no shape fallback: the walker's DEFAULT answer (its last `pass`, printed per walker) decides -- the rows to read by hand. Statement kinds
against the statement walkers follow the same way (StmtKind), and then THE SLOT TABLE (landing 168): for each statement walker and each
statement kind it names, every expression accessor its arm reads (`stmt_cond`, `stmt_for_iter`, `stmt_counted_arg`, `pick_selector`, `arm_guard`,
`stmt_target`, ...) and whether the value reaches the walker's own expression walk -- a function of the same module taking the state and an
`ExprId` -- directly, through a local, or inside a helper the arm hands the statement to (`escape_counted(x, s)`); `READ` marks an accessor whose
value is read (a type question, a span) and never walked: landing 168's q1 -- `escape_stmt`'s `for` arm handed `stmt_for_iter` to
`for_binding_can_carry` alone, so a pick expression in a range bound was never entered and its arm's holders had no view ref. Output:
kindtable167.md in the current directory, or --out PATH."""
import os, re, sys
W = sys.argv[1]; MIN = int(sys.argv[sys.argv.index('--min') + 1]) if '--min' in sys.argv else 2
OUT = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else 'kindtable167.md'
def read(p): return open(p, encoding='utf-8').read().split('\n')
ast = read(os.path.join(W, 'src/frontend/ast.npk'))
kinds = []; shape = {}
for l in ast:
    m = re.search(r'if \(k == ExprKind\.(Expr[A-Za-z0-9_]+)\)\s*\{ pass \(raw EXPR_SHAPE_([A-Z]+)\(\)\); \}', l)
    if m: kinds.append(m.group(1)); shape[m.group(1)] = m.group(2)
def functions(path):
    out = {}; cur = None; body = []
    for l in read(path):
        m = re.match(r'(?:pub\s+)?func:([A-Za-z_][A-Za-z0-9_]*)\s*=', l)
        if m:
            if cur: out[cur] = body
            cur = m.group(1); body = [l]; continue
        if cur:
            body.append(l)
            if l.startswith('};'): out[cur] = body; cur = None; body = []
    if cur: out[cur] = body
    return out
def analyse(body, enum):
    text = '\n'.join(body)
    named = set(re.findall(enum + r'\.([A-Za-z0-9_]+)', text))
    fb = sorted(set(re.findall(r'\b(expr_shape|escape_children|acc_children|stmt_children)\s*\(', text)))
    passes = [re.sub(r'\s+', ' ', l.strip()) for l in body if re.match(r'\s*pass\b', l)]
    last = passes[-1][:60] if passes else '(no bare pass)'
    # A CAST PEELED THROUGH THE ONE HELPER (DEF-343, landing 167): `peel_ptr_casts` steps the two cast kinds
    # for the walker that calls it, so those two kinds are answered (`peel`) without a named arm.
    if re.search(r'\bpeel_ptr_casts\s*\(', text): named |= {'ExprCastExpr', 'ExprUncheckedCastExpr'}
    return named, fb, last
rows = []
files = [('escape', os.path.join(W, 'src/frontend/analysis/escape.npk')), ('alias', os.path.join(W, 'src/frontend/analysis/alias.npk')), ('locks', os.path.join(W, 'src/frontend/analysis/locks.npk'))]
walkers = []
for mod, p in files:
    for name, body in functions(p).items():
        named, fb, last = analyse(body, 'ExprKind')
        if len(named) >= MIN: walkers.append((mod, name, named, fb, last, len(body)))
walkers.sort(key=lambda w: (w[0], -len(w[2]), w[1]))
out = []
out.append(f"# kindtable167.md -- {len(kinds)} expression kinds (ast.npk's `expr_shape`) x {len(walkers)} kind-naming walkers (>= {MIN} kinds named) of escape.npk, alias.npk, locks.npk; tree {W}\n")
out.append("Legend: `arm` the walker names the kind; `shape` no arm, the body has a shape-generic fallback; `-` no arm, no fallback (the walker's default answer decides). Struck/unreachable: `ExprPipeExpr` (PARSE-014, D-362).\n")
out.append("## Walkers (module.function: kinds named / shape fallback / default = the body's last `pass`)\n")
for i, (mod, name, named, fb, last, n) in enumerate(walkers):
    out.append(f"{i+1}. `{mod}.{name}` ({n} lines): {len(named)} kinds named; fallback {', '.join(fb) if fb else 'NONE'}; default `{last}`")
out.append("\n## The matrix (columns = the walkers' numbers above)\n")
hdr = "| kind | shape | " + " | ".join(str(i + 1) for i in range(len(walkers))) + " | no-arm-no-fallback |"
out.append(hdr); out.append("|---|---|" + "---|" * len(walkers) + "---|")
gaps_per_walker = {i: [] for i in range(len(walkers))}
for k in kinds:
    cells = []; gaps = 0
    for i, (mod, name, named, fb, last, n) in enumerate(walkers):
        if k in named: cells.append('arm')
        elif fb: cells.append('shape')
        else: cells.append('-'); gaps += 1; gaps_per_walker[i].append(k)
    out.append(f"| `{k}` | {shape[k]} | " + " | ".join(cells) + f" | {gaps} |")
out.append("\n## Per walker: the kinds with no arm and no fallback (to read by hand: which default answers them, open or closed)\n")
for i, (mod, name, named, fb, last, n) in enumerate(walkers):
    g = gaps_per_walker[i]
    if g: out.append(f"- `{mod}.{name}` default `{last}`: {len(g)} kinds -- " + ", ".join(g))
# statement kinds
stmt_kinds = []
for mod, p in files[:2]:
    for name, body in functions(p).items():
        for k in re.findall(r'StmtKind\.(Stmt[A-Za-z0-9_]+)', '\n'.join(body)): 
            if k not in stmt_kinds: stmt_kinds.append(k)
ak = read(os.path.join(W, 'src/frontend/ast_kind.npk'))
declared = re.findall(r'\b(Stmt[A-Z][A-Za-z0-9_]*)\b', '\n'.join(ak))
for k in declared:
    if k not in stmt_kinds and k != 'StmtKind': stmt_kinds.append(k)
swalkers = []
for mod, p in files[:2]:
    for name, body in functions(p).items():
        named, fb, last = analyse(body, 'StmtKind')
        if len(named) >= 3: swalkers.append((mod, name, named, last))
swalkers.sort(key=lambda w: (w[0], -len(w[2])))
out.append(f"\n## Statement kinds ({len(stmt_kinds)}) x statement walkers (>= 3 StmtKind values named)\n")
out.append("| stmt kind | " + " | ".join(f"`{m}.{n}`" for m, n, _, _ in swalkers) + " |"); out.append("|---|" + "---|" * len(swalkers))
for k in stmt_kinds:
    out.append(f"| `{k}` | " + " | ".join('arm' if k in named else '-' for _, _, named, _ in swalkers) + " |")
out.append("\nStatement-nesting EXPRESSION kinds (a statement walker that never enters an expression misses the statements under them): `ExprPickExpr` (arms with bodies); any other kind whose children include a StmtId is listed by `ast.npk` -- read `expr_shape`'s CUSTOM kinds: " + ", ".join(k for k in kinds if shape[k] == 'CUSTOM') + ".")
# THE SLOT TABLE: accessors read by each statement walker's arm, walked or READ.
def arms_of(body):
    """(kinds, arm text) per `if (… s.kind == StmtKind.X …) { … }` block, brace-balanced; a block naming several kinds is one arm."""
    text = '\n'.join(body); res = []; i = 0
    for m in re.finditer(r'if \(([^{]*?StmtKind\.[^{]*?)\)\s*\{', text):
        if m.start() < i: continue
        ks = re.findall(r'StmtKind\.(Stmt[A-Za-z0-9_]+)', m.group(1))
        depth = 1; j = m.end()
        while j < len(text) and depth > 0:
            depth += {'{': 1, '}': -1}.get(text[j], 0); j += 1
        res.append((ks, text[m.end():j - 1])); i = j
    return res
def expr_entries(funcs, state_ty):
    """The module's expression WALKS: a function of (state, ExprId) that names two or more ExprKind values, or hands its
    ExprId on to one that does (`escape_untracked` -> `escape_expr`), to a fixed point. A reader of (state, ExprId) that
    names no kind and walks nothing -- a type question such as `for_binding_can_carry` -- is not one."""
    cand = {}
    for name, body in funcs.items():
        m = re.match(r'(?:pub\s+)?func:' + name + r'\s*=\s*[^(]*\(' + state_ty + r'->:[a-z]+,\s*ExprId:([a-z_0-9]+)', body[0])
        if m: cand[name] = (m.group(1), '\n'.join(body))
    ents = {n for n, (arg, text) in cand.items() if len(set(re.findall(r'ExprKind\.([A-Za-z0-9_]+)', text))) >= 2}
    while True:
        more = {n for n, (arg, text) in cand.items() if n not in ents and any(re.search(r'\b' + e + r'\(x,\s*' + arg + r'\b', text) for e in ents)}
        if not more: break
        ents |= more
    return ents
ACCESSOR = re.compile(r'raw (stmt_[a-z_]+|pick_[a-z_]+|arm_[a-z_]+|pickx_[a-z_]+)\s*\(')
def slot_rows(mod, funcs, walker, state_ty):
    ents = expr_entries(funcs, state_ty); rows = []
    for ks, arm in arms_of(funcs[walker]):
        # one level of helpers handed the statement: `name(x, s)` or `name(x, s, …)` -- their bodies join the arm
        text = arm
        for h in set(re.findall(r'\b([a-z_]+)\(x, s[,)]', arm)):
            if h in funcs and h != walker: text += '\n' + '\n'.join(funcs[h])
        found = {}
        for m in ACCESSOR.finditer(text):
            acc = m.group(1)
            if acc in ('stmt_is_none', 'stmt_carries_expr', 'stmt_list_start', 'stmt_list_count', 'pick_arm_count', 'pick_arm_start', 'pickx_arm_count', 'pickx_arm_start', 'stmt_counted_argc', 'stmt_name', 'stmt_for_binding', 'arm_body', 'pick_arm_at'): continue
            # the call's text, balanced
            j = m.end(); depth = 1
            while j < len(text) and depth > 0:
                depth += {'(': 1, ')': -1}.get(text[j], 0); j += 1
            call = text[m.start():j]
            # walked directly: an expression entry's argument list holds this call
            pre = text[:m.start()]
            # EVERY walk the accessor's value reaches: the call itself as an entry's argument, or through the local it
            # is bound to (`T:name = <call>;` then `entry(x, name`), each with the `if` that guards it, if one does.
            walks = []
            for e in sorted(ents):
                if re.search(r'\b' + e + r'\(x,\s*(raw\s*)?$', pre[-80:]): walks.append((m.start(), e))
            mb = re.search(r'([A-Za-z0-9_]+):([a-z_0-9]+)\s*=\s*$', pre[-60:])
            if mb:
                loc = mb.group(2)
                for e in sorted(ents):
                    for mw in re.finditer(r'\b' + e + r'\(x,\s*' + loc + r'\b', text): walks.append((mw.start(), e))
            # A WALK UNDER A GUARD (landing 168's q1): the walk call sits in an `if (cond) { … }` block of the arm whose
            # condition is not a none-test -- the slot is walked only when the condition holds, and the reader must
            # ask whether every program reaches it. `for_binding_can_carry` guarded `stmt_for_iter`'s walk at build 16.
            def guard_of(at):
                depth = 0; k = at
                while k > 0:
                    k -= 1
                    if text[k] == '}': depth += 1
                    elif text[k] == '{':
                        if depth == 0:
                            # the LAST `if (` before the brace, its condition balanced, nothing but space up to the brace
                            win = text[max(0, k - 400):k]; starts = [mm.end() for mm in re.finditer(r'\bif\s*\(', win)]
                            if starts:
                                q = starts[-1]; dp = 1
                                while q < len(win) and dp > 0:
                                    dp += {'(': 1, ')': -1}.get(win[q], 0); q += 1
                                cond = win[starts[-1]:q - 1]
                                if win[q:].strip() == '' and 'is_none' not in cond: return re.sub(r'\s+', ' ', cond.strip())
                            return ''
                        depth -= 1
                return ''
            walks = sorted(set(walks)); judged = [(at, e, guard_of(at)) for at, e in walks]
            free = [w for w in judged if not w[2]]
            direct = bool(judged)
            if free: via, guard = ', '.join(sorted({w[1] for w in free})), ''
            elif judged: via, guard = judged[0][1], judged[0][2]
            else: via, guard = '', ''
            verdict = ('walked (`' + via + '`) under `if (' + guard + ')`') if (direct and guard) else (('walked (`' + via.replace(', ', '`, `') + '`)') if direct else 'READ')
            prev = found.get(acc)
            if prev is None or prev == 'READ' or (direct and 'under' not in verdict): found[acc] = verdict
        rows.append((ks, found))
    return rows
out.append("\n## The slot table (landing 168): expression accessors each statement walker's arm reads, `walked` into the module's expression walk or `READ` only\n")
for mod, p in files[:2]:
    funcs = functions(p); state = 'Alias' if mod == 'alias' else 'Escape'
    for walker in ('alias_stmt', 'prescan_stmt') if mod == 'alias' else ('escape_stmt',):
        if walker not in funcs: continue
        out.append(f"### `{mod}.{walker}`\n"); out.append("| stmt kinds | accessors (walked / READ) |"); out.append("|---|---|")
        for ks, found in slot_rows(mod, funcs, walker, state):
            cells = ', '.join(f"`{a}`: {w}" for a, w in sorted(found.items())) or '(none)'
            out.append(f"| {', '.join('`' + k + '`' for k in ks)} | {cells} |")
        out.append('')
open(OUT, 'w').write('\n'.join(out) + '\n')
print(f"{len(kinds)} kinds, {len(walkers)} expression walkers, {len(stmt_kinds)} statement kinds, {len(swalkers)} statement walkers -> {OUT} ({len(out)} lines)")
for i, (mod, name, named, fb, last, n) in enumerate(walkers):
    g = gaps_per_walker[i]
    print(f"  {i+1:2d}. {mod}.{name}: named {len(named)}, fallback {','.join(fb) if fb else 'NONE'}, no-arm-no-fallback {len(g)}, default {last[:50]!r}")
