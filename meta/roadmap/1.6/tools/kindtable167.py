#!/usr/bin/env python3 -I
"""kindtable167.py WORKTREE [--min N] -- THE MECHANICAL TABLE of expression kinds against each walker (management_6, 2026-10-29 06:41: "a script
that lists every ExprKind against each walker's arms"). The kinds and their shapes come from `expr_shape` in src/frontend/ast.npk; a walker is a
function of escape.npk, alias.npk or locks.npk that names ExprKind values (at least N of them, default 2). For each (kind, walker): `arm` = the
walker names the kind; `shape` = it does not, but the body calls a shape-generic fallback (`expr_shape`, `escape_children`, `acc_children`); `-` = no
arm and no shape fallback: the walker's DEFAULT answer (its last `pass`, printed per walker) decides -- the rows to read by hand. Statement kinds
against the statement walkers follow the same way (StmtKind). Output: kindtable167.md beside this script."""
import os, re, sys
W = sys.argv[1]; MIN = int(sys.argv[sys.argv.index('--min') + 1]) if '--min' in sys.argv else 2
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
open('kindtable167.md', 'w').write('\n'.join(out) + '\n')
print(f"{len(kinds)} kinds, {len(walkers)} expression walkers, {len(stmt_kinds)} statement kinds, {len(swalkers)} statement walkers -> kindtable167.md ({len(out)} lines)")
for i, (mod, name, named, fb, last, n) in enumerate(walkers):
    g = gaps_per_walker[i]
    print(f"  {i+1:2d}. {mod}.{name}: named {len(named)}, fallback {','.join(fb) if fb else 'NONE'}, no-arm-no-fallback {len(g)}, default {last[:50]!r}")
