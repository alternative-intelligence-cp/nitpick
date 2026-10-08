#!/usr/bin/env python3
"""sitenorm.py -- compare two emissions of one program with the error-origin SITE TABLE canonicalised.

Landing 97 edits the prelude (the frac section), which (a) adds sites to the prelude's part of the
table, so every later site id moves, and (b) shifts the LINE numbers of every prelude site below the
edit. Both move every program's emitted text. This tool replaces each site id by the site's identity
(path and line, a prelude line mapped from the base prelude to the new one through difflib), deletes
the table's globals, masks the lookup's bound, and reports whether anything ELSE differs.

usage: sitenorm.py BASE_PRELUDE NEW_PRELUDE PAIRS_DIR SUFFIX_BASE SUFFIX_NEW [--show N]
  PAIRS_DIR holds NAME<SUFFIX_BASE> and NAME<SUFFIX_NEW> for every program (e.g. ".base.ll"/".new.ll").
  Prints one line per pair: SAME NAME | DIFF NAME <n differing lines> | MISSING NAME, then a summary.
"""
import difflib, os, re, sys

RE_SITEP = re.compile(r'^@npk\.sitep\.(\d+) = internal constant \[\d+ x i8\] c"((?:[^"\\]|\\.)*)"\s*$', re.M)
RE_PATHS = re.compile(r'^@npk\.site\.paths = internal constant \[(\d+) x \{ ptr, i64 \}\] \[(.*)\]\s*$', re.M)
RE_LINES = re.compile(r'^@npk\.site\.lines = internal constant \[(\d+) x i32\] \[(.*)\]\s*$', re.M)
RE_ENTRY = re.compile(r'\{ ptr (null|@npk\.sitep\.(\d+)), i64 (\d+) \}')
RE_CHAIN = re.compile(r'(@npk_chain_(?:reset|push|site))\(i32 (\d+)\)')
RE_BOUND = re.compile(r'(%ok = icmp ult i32 %i, )\d+')
RE_GEP = re.compile(r'(getelementptr \[)\d+( x (?:i32|\{ ptr, i64 \})\], ptr @npk\.site\.(?:lines|paths),)')


def line_map(base_text, new_text):
    """base line -> new line for lines difflib pairs as equal; a changed line maps to None."""
    a = base_text.splitlines()
    b = new_text.splitlines()
    m = {}
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == 'equal':
            for k in range(i2 - i1):
                m[i1 + k + 1] = j1 + k + 1
    return m


def parse_table(text):
    sitep = {int(m.group(1)): m.group(2) for m in RE_SITEP.finditer(text)}
    paths, lines = [], []
    mp = RE_PATHS.search(text)
    if mp:
        for e in RE_ENTRY.finditer(mp.group(2)):
            paths.append(None if e.group(1) == 'null' else sitep.get(int(e.group(2)), '?'))
    ml = RE_LINES.search(text)
    if ml:
        lines = [int(x) for x in re.findall(r'i32 (\d+)', ml.group(2))]
    return paths, lines


def canon(text, prelude_map, is_base):
    paths, lines = parse_table(text)

    def ident(i):
        if i < 0 or i >= len(paths) or paths[i] is None:
            return f'<site {i}: none>'
        p = paths[i]
        ln = lines[i] if i < len(lines) else -1
        if p == 'prelude.npk' and is_base:
            ln2 = prelude_map.get(ln)
            ln = f'{ln2}' if ln2 is not None else f'changed-base-line-{ln}'
        return f'<{p}:{ln}>'

    out = []
    for line in text.split('\n'):
        if line.startswith('@npk.sitep.') or line.startswith('@npk.site.paths') or line.startswith('@npk.site.lines'):
            continue
        line = RE_CHAIN.sub(lambda m: m.group(1) + '(' + ident(int(m.group(2))) + ')', line)
        line = RE_BOUND.sub(r'\1#', line)
        line = RE_GEP.sub(r'\1#\2', line)
        out.append(line)
    return out, len(paths)


def main():
    args = sys.argv[1:]
    show = 0
    if '--show' in args:
        k = args.index('--show')
        show = int(args[k + 1])
        del args[k:k + 2]
    base_prel, new_prel, d, sb, sn = args
    pm = line_map(open(base_prel).read(), open(new_prel).read())
    names = sorted(f[:-len(sb)] for f in os.listdir(d) if f.endswith(sb))
    same = diff = missing = 0
    residue = []
    for n in names:
        fb, fn = os.path.join(d, n + sb), os.path.join(d, n + sn)
        if not os.path.exists(fn):
            missing += 1
            print('MISSING', n)
            continue
        tb, tn = open(fb, errors='replace').read(), open(fn, errors='replace').read()
        if tb == tn:
            same += 1
            print('IDENTICAL', n)
            continue
        cb, nb = canon(tb, pm, True)
        cn, nn = canon(tn, pm, False)
        if cb == cn:
            same += 1
            print('SAME', n, f'(sites {nb} -> {nn})')
        else:
            diff += 1
            dl = [l for l in difflib.unified_diff(cb, cn, lineterm='', n=0) if l[:1] in '+-' and not l.startswith(('+++', '---'))]
            residue.append(n)
            print('DIFF', n, len(dl), f'(sites {nb} -> {nn})')
            if show:
                for l in dl[:show]:
                    print('   ', l[:200])
    print(f'SUMMARY pairs={len(names)} same-after-canon={same} diff={diff} missing={missing}')
    print('RESIDUE', ' '.join(residue))


if __name__ == '__main__':
    main()
