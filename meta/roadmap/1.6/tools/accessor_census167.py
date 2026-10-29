#!/usr/bin/env python3
"""accessor_census167.py WORKTREE -- the accessor-by-kind census (management_6, 2026-10-29 07:52): for every statement and
expression ACCESSOR call in escape.npk, alias.npk and locks.npk, the node kind(s) the enclosing guard tests, checked against the
accessor's OWNING kinds -- the kinds whose parser construction (parse_stmt.npk, parse_expr.npk, parse_pick.npk) fills the slot the
accessor reads with what the accessor says it returns. A MISMATCH row is a call whose guard admits a kind the accessor does not own
(DEF-337's then-branch fault: `stmt_body`, the WhileStmt header's slot 0, read on a StmtIfStmt). An UNGUARDED row is a call with no
kind test on its variable in any enclosing block (a shape-based or contract-based read): listed for hand reading, not a verdict.
RAW rows are direct slot reads (`s.a`, `n.b`, ...) under their guard, listed."""
import os, re, sys
W = sys.argv[1]
FE = os.path.join(W, 'src/frontend')
def rd(p): return open(p, encoding='utf-8').read()

# ---- the slot-category table, from the parser's construction sites (parse_stmt.npk, parse_expr.npk, parse_pick.npk; lines cited) ----
# categories: expr (an ExprId), stmt (a StmtId), type (a TypeId), decl (a DeclId), pat (a PatId), win (an expression/statement window start), twin (a TYPE window start), cnt (a window
# count), gcount (a generic count), none (0); payload: name, op, label, quals, text, incl, mut, gcount, bool, none.
ps = rd(os.path.join(FE, 'parse_stmt.npk'))
value_kinds = set(re.findall(r'p_value_stmt\(p, sp, StmtKind\.(\w+)', ps)) | set(re.findall(r'p_paren_stmt\(p, sp, StmtKind\.(\w+)', ps))
label_kinds = set(re.findall(r'p_label_stmt\(p, sp, StmtKind\.(\w+)', ps))
pe = rd(os.path.join(FE, 'parse_expr.npk'))
kw_operand = set(re.findall(r'p_keyword_operand\(p, sp, ExprKind\.(\w+)', pe))
K = {}
def kinds(names, a, b, c, pl):
    for n in names: K[n] = (a, b, c, pl)
kinds(['StmtBlockStmt'], 'none', 'win', 'cnt', 'none')                       # parse_stmt.npk:511, 540, 545
kinds(['StmtDeferStmt'], 'stmt', 'none', 'none', 'none')                      # 577
kinds(['StmtExprStmt'], 'expr', 'none', 'none', 'none')                       # 675 (a = 0 for an empty statement), 707
kinds(['StmtAssignStmt'], 'expr', 'expr', 'none', 'op')                       # 697, 702
kinds(value_kinds, 'expr', 'none', 'none', 'none')                            # p_value_stmt 723, p_paren_stmt 733
kinds(label_kinds, 'none', 'none', 'none', 'label')                           # p_label_stmt 750
kinds(['StmtVarDeclStmt'], 'type', 'win', 'cnt', 'name')                      # 783
kinds(['StmtIfStmt'], 'expr', 'stmt', 'stmt', 'none')                         # 806
kinds(['StmtWhileStmt'], 'expr', 'win', 'cnt', 'label')                       # 836
kinds(['StmtForStmt'], 'decl', 'win', 'cnt', 'label')                         # 877
kinds(['StmtWhenStmt'], 'expr', 'win', 'cnt', 'label')                        # 931
kinds(['StmtLoopStmt', 'StmtTillStmt'], 'stmt', 'win', 'cnt', 'label')        # 963
kinds(['StmtPickStmt'], 'expr', 'win', 'cnt', 'none')                         # parse_pick.npk:512
kinds(['StmtPickArm'], 'pat', 'expr', 'stmt', 'label')                        # 399 (guard 0), 430
kinds(['ExprTernaryExpr'], 'expr', 'expr', 'expr', 'none')                    # parse_expr.npk:181
kinds(['ExprSafeUnwrapExpr', 'ExprDefaultsExpr', 'ExprNullCoalesceExpr', 'ExprEmphaticUnwrapExpr', 'ExprEmphaticCoalesceExpr'], 'expr', 'expr', 'none', 'none')  # 199, 210, 244
kinds(['ExprRangeExpr'], 'expr', 'expr', 'none', 'incl')                      # 311
kinds(['ExprBinaryExpr'], 'expr', 'expr', 'none', 'op')                       # 314
kinds(['ExprAddressOfExpr', 'ExprDerefExpr', 'ExprSpreadExpr'], 'expr', 'none', 'none', 'none')   # 355, 361, 379
kinds(['ExprBorrowExpr'], 'expr', 'none', 'none', 'mut')                      # 367, 373
kinds(['ExprUnaryExpr', 'ExprPostfixExpr'], 'expr', 'none', 'none', 'op')     # 385-397, 529, 536
kinds(['ExprDynCastExpr'], 'expr', 'twin', 'cnt', 'quals')                    # 445 (b/c = a window of TRAIT TYPES, not expressions)
kinds(['ExprCastExpr', 'ExprUncheckedCastExpr'], 'expr', 'type', 'none', 'quals')   # 449
kinds(['ExprPipeExpr'], 'expr', 'expr', 'none', 'none')                       # 485 (struck, PARSE-014; still built)
kinds(['ExprRawUnwrapExpr', 'ExprDropExpr', 'ExprRelayExpr', 'ExprAwaitExpr'], 'expr', 'none', 'none', 'none')   # 509
kinds(['ExprMemberAccessExpr', 'ExprSafeNavExpr'], 'expr', 'none', 'none', 'name')   # 552, 560
kinds(['ExprIndexExpr'], 'expr', 'expr', 'none', 'none')                      # 567
kinds(['ExprMethodCallExpr'], 'expr', 'win', 'cnt', 'name')                   # 623 (a = the receiver)
kinds(['ExprCallExpr'], 'expr', 'win', 'cnt', 'gcount')                       # 649 (a = the callee)
kinds(['ExprStringLiteral', 'ExprRawStringLiteral', 'ExprBlockStringLiteral'], 'none', 'none', 'none', 'text')   # 707-717
kinds(['ExprBoolLiteral'], 'none', 'none', 'none', 'bool')                    # 722, 727
kinds(['ExprIterationVarExpr', 'ExprResultValueExpr', 'ExprSentinelLiteral'], 'none', 'none', 'none', 'none')   # 732, 743, 878-891
kinds(['ExprOldExpr', 'ExprMoveExpr', 'ExprComptimeExpr'] + sorted(kw_operand), 'expr', 'none', 'none', 'none')   # 757, 769, 777, 1003
kinds(['ExprBuiltinExpr'], 'gcount', 'win', 'cnt', 'name')                    # 812
kinds(['ExprArrayLiteralExpr', 'ExprTemplateLiteral', 'ExprVectorCtorExpr'], 'none', 'win', 'cnt', 'none')   # 831, 872, 926
kinds(['ExprStructLiteralExpr'], 'none', 'win', 'cnt', 'name')                # 1119
kinds(['ExprIdentifierExpr'], 'none', 'none', 'none', 'name')                 # 988
kinds(['ExprResultLiteralExpr'], 'expr', 'expr', 'none', 'none')              # 1098
kinds(['ExprPickExpr'], 'expr', 'win', 'cnt', 'none')                         # parse_pick.npk:522
kinds(['ExprIntLiteral', 'ExprCharLiteral', 'ExprFloatLiteral'], 'none', 'none', 'none', 'text')   # ast_add_expr_w sites
# accessors: (slot, category); 'hdr' = a window read through s_at (slot b must be a window)
A = {
 'stmt_cond': ('a', 'expr'), 'stmt_then': ('b', 'stmt'), 'stmt_else': ('c', 'stmt'), 'stmt_target': ('a', 'expr'), 'stmt_assigned': ('b', 'expr'),
 'stmt_operand': ('a', 'expr'), 'stmt_expr': ('a', 'expr'), 'stmt_list_start': ('b', 'win'), 'stmt_list_count': ('c', 'cnt'),
 'stmt_decl_type': ('a', 'type'), 'stmt_for_binding': ('a', 'decl'), 'stmt_counted_body': ('a', 'stmt'),
 'pick_selector': ('a', 'expr'), 'pick_arm_start': ('b', 'win'), 'pick_arm_count': ('c', 'cnt'),
 'arm_pattern': ('a', 'pat'), 'arm_guard': ('b', 'expr'), 'arm_body': ('c', 'stmt'),
 'stmt_op': ('p', 'op'), 'stmt_label': ('p', 'label'), 'stmt_name': ('p', 'name'), 'arm_label': ('p', 'label'),
 'expr_lhs': ('a', 'expr'), 'expr_operand': ('a', 'expr'), 'expr_callee': ('a', 'expr'), 'pickx_selector': ('a', 'expr'),
 'expr_rhs': ('b', 'expr'), 'expr_cast_type': ('b', 'type'), 'expr_third': ('c', 'expr'),
 'expr_list_start': ('b', 'win'), 'pickx_arm_start': ('b', 'win'), 'expr_list_count': ('c', 'cnt'), 'pickx_arm_count': ('c', 'cnt'),
 'builtin_generic_count': ('a', 'gcount'), 'expr_generic_count': ('p', 'gcount'), 'builtin_name': ('p', 'name'), 'expr_name': ('p', 'name'),
 'expr_op': ('p', 'op'), 'expr_cast_quals': ('p', 'quals'), 'borrow_is_mut': ('p', 'mut'),
}
for h in ['stmt_decl_init', 'stmt_quals', 'stmt_decl_limit', 'stmt_decl_attr_count', 'stmt_decl_attr', 'stmt_body', 'stmt_while_invariant',
          'stmt_while_measure', 'stmt_for_iter', 'stmt_for_body', 'stmt_for_invariant', 'stmt_for_measure', 'stmt_counted_invariant',
          'stmt_counted_argc', 'stmt_counted_measure', 'stmt_counted_arg', 'stmt_when_body', 'stmt_when_then', 'stmt_when_end',
          'stmt_when_invariant', 'stmt_when_measure']:
    A[h] = ('b', 'win')
# the header accessors read a FIXED index of the window: the kinds whose window has that meaning (parse_stmt.npk's header comments)
HDR_OWNER = {'stmt_decl_init': {'StmtVarDeclStmt'}, 'stmt_quals': {'StmtVarDeclStmt'}, 'stmt_decl_limit': {'StmtVarDeclStmt'},
             'stmt_decl_attr_count': {'StmtVarDeclStmt'}, 'stmt_decl_attr': {'StmtVarDeclStmt'},
             'stmt_body': {'StmtWhileStmt'}, 'stmt_while_invariant': {'StmtWhileStmt'}, 'stmt_while_measure': {'StmtWhileStmt'},
             'stmt_for_iter': {'StmtForStmt'}, 'stmt_for_body': {'StmtForStmt'}, 'stmt_for_invariant': {'StmtForStmt'}, 'stmt_for_measure': {'StmtForStmt'},
             'stmt_counted_invariant': {'StmtLoopStmt', 'StmtTillStmt'}, 'stmt_counted_argc': {'StmtLoopStmt', 'StmtTillStmt'},
             'stmt_counted_measure': {'StmtLoopStmt', 'StmtTillStmt'}, 'stmt_counted_arg': {'StmtLoopStmt', 'StmtTillStmt'},
             'stmt_when_body': {'StmtWhenStmt'}, 'stmt_when_then': {'StmtWhenStmt'}, 'stmt_when_end': {'StmtWhenStmt'},
             'stmt_when_invariant': {'StmtWhenStmt'}, 'stmt_when_measure': {'StmtWhenStmt'}}
def owns(acc, kind):
    """Does `kind` own the slot `acc` reads, with the category `acc` returns?"""
    if kind not in K: return None   # an unknown kind (not built in the parser files read): reported as such
    slot, cat = A[acc]
    if acc in HDR_OWNER: return kind in HDR_OWNER[acc]
    idx = {'a': 0, 'b': 1, 'c': 2, 'p': 3}[slot]
    return K[kind][idx] == cat
OWN = {a: set(k for k in K if owns(a, k)) for a in A}

ACC = re.compile(r'\b(' + '|'.join(sorted(OWN, key=len, reverse=True)) + r')\s*\(')
KT = re.compile(r'\b([A-Za-z_][A-Za-z_0-9]*)\.kind\s*(==|!=)\s*(StmtKind|ExprKind)\.(\w+)')
RAW = re.compile(r'\b([a-z][a-z0-9_]*)\.(a|b|c)\b')

def join_conds(lines):
    """Join a line whose parentheses are unbalanced with the following lines (multi-line `if (` conditions)."""
    out = []; i = 0
    while i < len(lines):
        l = lines[i]; n0 = i
        while l.count('(') > l.count(')') and i + 1 < len(lines) and not l.rstrip().endswith('{'):
            i += 1; l = l + ' ' + lines[i].strip()
        out.append((n0 + 1, l)); i += 1
    return out

def strip_comment(l):
    j = l.find('//'); return l if j < 0 else l[:j]

def census(path):
    rows = []
    lines = rd(path).split('\n')
    stack = []   # entries: dict(var, kinds, neg) per open brace; None for a block with no kind test
    known = [{}]  # per-depth maps var -> kinds established by an early-exit negative test
    for ln, raw in join_conds(lines):
        l = strip_comment(raw)
        # an early-exit negative test on ONE line: `if (v.kind != K.X) { pass …; }` -> v is X afterwards in this block
        m = KT.search(l)
        one_line = l.count('{') == 1 and l.count('}') == 1 and l.strip().startswith('if')
        if m and one_line and m.group(2) == '!=' and re.search(r'\{\s*(pass|continue|break)\b', l):
            known[-1].setdefault(m.group(1), set()).update({m.group(4)})
        # a positive test on ONE line: `if (v.kind == K.X) { …acc(v)… }` -- the accessor calls on the line are guarded by X
        inline_guard = None
        if m and one_line and m.group(2) == '==':
            ks = {g[3] for g in KT.findall(l) if g[1] == '==' and g[0] == m.group(1)}
            inline_guard = (m.group(1), ks)
        # accessor calls on this line
        for am in ACC.finditer(l):
            acc = am.group(1)
            # the argument variable: the first argument that is not an ast handle
            depth = 0; j = am.end(); start = j; args = []
            while j < len(l):
                c = l[j]
                if c == '(': depth += 1
                elif c == ')':
                    if depth == 0: args.append(l[start:j]); break
                    depth -= 1
                elif c == ',' and depth == 0: args.append(l[start:j]); start = j + 1
                j += 1
            var = None
            for a in args:
                a = a.strip()
                if a in ('x.ast', 'e.ast', 'ast', 'p.ast', 'x', 'e', 'p') or a.startswith('raw ') or a.startswith('StmtId{') or a.startswith('ExprId{'): continue
                mm = re.match(r'^([A-Za-z_][A-Za-z_0-9]*)$', a)
                var = mm.group(1) if mm else a; break
            # the guard: inline, else the innermost enclosing block testing this var, else a known early-exit
            guard = None; how = ''
            if inline_guard and var == inline_guard[0]: guard, how = inline_guard[1], 'inline'
            if guard is None:
                for ent in reversed(stack):
                    if ent and ent['var'] == var: guard, how = ent['kinds'], 'block'; break
            if guard is None:
                for km in reversed(known):
                    if var in km: guard, how = km[var], 'early-exit'; break
            own = OWN[acc]
            if guard is None: rows.append(('UNGUARDED', ln, acc, var, '-', '', l.strip()[:120]))
            else:
                bad = sorted(k for k in guard if k not in own)
                if bad:
                    slot, cat = A[acc]
                    held = ','.join(('%s holds %s' % (k, (K[k][{'a':0,'b':1,'c':2,'p':3}[slot]] if k in K else 'UNKNOWN KIND'))) for k in bad)
                    rows.append(('MISMATCH', ln, acc, var, '+'.join(sorted(guard)), '%s wants %s in slot %s; %s' % (acc, cat, slot, held), l.strip()[:120]))
                else: rows.append(('ok', ln, acc, var, '+'.join(sorted(guard)), '', ''))
        # raw slot reads under a guard
        for rm in RAW.finditer(l):
            v, slot = rm.group(1), rm.group(2)
            if v in ('x', 'e', 'p', 'r', 'cr', 'pr', 'vr', 'vr2', 'q'): pass
            guard = None
            for ent in reversed(stack):
                if ent and ent['var'] == v: guard = ent['kinds']; break
            if guard is not None or inline_guard and inline_guard[0] == v:
                if guard is None: guard = inline_guard[1]
                rows.append(('RAW', ln, '.' + slot, v, '+'.join(sorted(guard)), '', l.strip()[:120]))
        # the brace stack
        opens = l.count('{'); closes = l.count('}')
        if opens > closes:
            ent = None
            if m and l.strip().startswith('if') and m.group(2) == '==':
                ks = {g[3] for g in KT.findall(l) if g[1] == '==' and g[0] == m.group(1)}
                if ks and ('||' in l or len(ks) == 1) and '&&' not in l.replace(m.group(0), ''):
                    ent = {'var': m.group(1), 'kinds': ks}
            for _ in range(opens - closes): stack.append(ent); known.append({}); ent = None
        elif closes > opens:
            for _ in range(closes - opens):
                if stack: stack.pop()
                if len(known) > 1: known.pop()
        if re.match(r'^(pub )?func:', l): stack.clear(); known[:] = [{}]
    return rows

# ---- the expr_shape table (ast.npk) against the slot-category table: a kind mis-shaped is a child every shape-fallback walker misses ----
ast = rd(os.path.join(FE, 'ast.npk'))
shape = dict(re.findall(r'if \(k == ExprKind\.(\w+)\)\s*\{ pass \(raw EXPR_SHAPE_(\w+)\(\)\); \}', ast))
print("## expr_shape table against the parser's slots:", len(shape), "kinds shaped,", len([k for k in K if k.startswith('Expr')]), "expression kinds built")
want = {'LEAF': lambda a,b,c: a != 'expr' and b != 'expr' and c != 'expr' and b != 'win',
        'ONE': lambda a,b,c: a == 'expr' and b != 'expr' and b != 'win',
        'TWO': lambda a,b,c: a == 'expr' and b == 'expr' and c != 'expr',
        'THREE': lambda a,b,c: a == 'expr' and b == 'expr' and c == 'expr',
        'LIST': lambda a,b,c: a != 'expr' and b == 'win' and c == 'cnt',
        'ONELIST': lambda a,b,c: a == 'expr' and b == 'win' and c == 'cnt',
        'CUSTOM': lambda a,b,c: True}
for k in sorted(k for k in K if k.startswith('Expr')):
    a, b, c, pl = K[k]
    sh = shape.get(k)
    if sh is None: print(f"  SHAPE-NONE {k}: built with slots ({a}, {b}, {c}) but unclassified by expr_shape (EXPR_SHAPE_NONE -- 'a bug')"); continue
    if not want[sh](a, b, c): print(f"  SHAPE-MISMATCH {k}: expr_shape says {sh} but the parser fills ({a}, {b}, {c})")
for k in sorted(shape):
    if k not in K: print(f"  SHAPE-UNBUILT {k}: shaped {shape[k]} but no construction site read in parse_expr.npk/parse_pick.npk (an ast_add_expr_w site or another file)")
print("  (shape check done)")

tot = {'MISMATCH': 0, 'UNGUARDED': 0, 'ok': 0, 'RAW': 0}
for f in ('escape.npk', 'alias.npk', 'locks.npk'):
    p = os.path.join(FE, 'analysis', f)
    rows = census(p)
    c = {k: sum(1 for r in rows if r[0] == k) for k in tot}
    for k in tot: tot[k] += c[k]
    print(f"\n## {f}: {c['ok']} ok, {c['MISMATCH']} MISMATCH, {c['UNGUARDED']} unguarded, {c['RAW']} raw slot reads")
    for r in rows:
        if r[0] == 'MISMATCH': print(f"  MISMATCH {f}:{r[1]} {r[2]}({r[3]}) guard={r[4]} not-owned={r[5]} | {r[6]}")
    for r in rows:
        if r[0] == 'UNGUARDED': print(f"  unguarded {f}:{r[1]} {r[2]}({r[3]}) | {r[6]}")
    for r in rows:
        if r[0] == 'RAW': print(f"  raw {f}:{r[1]} {r[3]}{r[2]} guard={r[4]} | {r[6]}")
print(f"\nTOTAL: {tot['ok']} ok, {tot['MISMATCH']} MISMATCH, {tot['UNGUARDED']} unguarded, {tot['RAW']} raw slot reads")
print("kinds in the table:", len(K), "; value kinds", sorted(value_kinds), "; label kinds", sorted(label_kinds), "; keyword-operand kinds", sorted(kw_operand))
