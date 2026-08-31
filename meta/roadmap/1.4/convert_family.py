"""1.4.7 step 2: convert one hand-rolled growable array to `List<T>`.

NOT part of the build and not a dependency of anything -- a migration tool,
committed because twelve more families need it and it was written in a scratch
directory that does not survive its session.

Each of the seven ways a site can hide from a text search cost a build, an
assertion, or a STOP-THE-LINE to find, and each is closed here by construction:

  1. output truncated by `head`         -> the leftover assertion reads all of it
  2. access through the FIELD name      -> the rewrite is scoped by BINDING
  3. the unqualified error spelling     -> (a `failsafe` concern, not this tool)
  4. a struct LITERAL                   -> asserted after the rewrite
  5. a field read from ANOTHER FILE     -> src/, tools/ and tests/ are scanned
  6. the binding is a LOCAL, not a param-> the binding test reads the whole block
  7. the tool EDITING a site it does    -> the guard deletion is scoped and
     not own (the reverse direction --     boundaried like every other rewrite,
     family 10 stripped fninst_record's    and deletions must pair 1:1 with
     guard; 1.4.7.md "RESOLVED")           converted pushes

Usage, one family at a time, each followed by `quickemit` and then a FULL
harness run before its own commit:

    import sys; sys.path.insert(0, "meta/roadmap/1.4")
    from convert_family import convert
    print(convert(path="src/frontend/token.npk", struct="TokenList",
                  elem="Token", initcap=256, initfn="tokenlist_init",
                  growfn="tokenlist_grow", var="l", id_error=None,
                  count_fns=(), import_line='use "./list.npk".*;'))

`dry=True` rehearses without writing -- always rehearse first. `root=` points
it at a git worktree instead of the main tree. Pass `id_error` ONLY where the
table hands its count out as an int32 id, and prefer an error the module
already declares (a new one makes every reachable `failsafe` grow an arm).

STILL UNCONVERTED: FnInstTable, TokenList, and the ten parallel-array
families (InstanceTable landed with the STOP-THE-LINE resolution -- the
"use-after-free" was THIS TOOL deleting fninst_record's guard through the
unscoped, unboundaried step-4 pattern; fixed here, regression-tested against
the pre-conversion types.npk). Parameters for the first two are in 1.4.7.md.
"""
import os, re, sys

MAIN = "/home/randy/Workspace/REPOS/nitpick-native"


def _fn_blocks(s):
    """(start, end, header) for every `func:` body, by brace matching.

    Nitpick has no nested function declarations, so one pass suffices. String
    literals are skipped so a brace inside `"{"` cannot unbalance the walk.
    """
    for m in re.finditer(r'^(?:pub )?(?:async )?func:([a-z_0-9]+) = ([^\n{]*)', s, re.M):
        i = s.find("{", m.end() - len(m.group(2)))
        if i < 0:
            continue
        d, j, instr, esc = 0, i, False, False
        while j < len(s):
            c = s[j]
            if instr:
                if esc: esc = False
                elif c == "\\": esc = True
                elif c == '"': instr = False
            else:
                if c == '"': instr = True
                elif c == "{": d += 1
                elif c == "}":
                    d -= 1
                    if d == 0: break
            j += 1
        yield m.start(), j + 1, m.group(0)


def _scoped(s, struct, var, fn):
    """Apply `fn` only inside functions whose SIGNATURE names `struct`.

    Two families can share a file and a receiver name -- `ImplTable` and
    `BoundTable` in type_trait.npk both bind `t` -- so a blanket replacement of
    `t.items` corrupts the other one. The signature is what tells them apart.
    """
    # The header must bind THIS struct to THIS receiver name. "`struct` appears
    # somewhere in the header" is too loose: a function taking both tables
    # (`ImplTable->:i, BoundTable->:t`) would then have the wrong one's fields
    # rewritten, silently and plausibly.
    bind = re.compile(rf"\b{struct}\s*(?:->)?\s*:\s*{var}\b")
    out, last, touched = [], 0, 0
    for a, b, header in _fn_blocks(s):
        if not bind.search(s[a:b]):
            continue
        out.append(s[last:a])
        out.append(fn(s[a:b], header))
        last = b
        touched += 1
    out.append(s[last:])
    return "".join(out), touched


def convert(path, struct, elem, initcap, initfn, growfn, var,
            id_error=None, count_fns=(), import_line=None, dry=False, root=MAIN,
            extra_files=None, reserve_fns=()):
    path = os.path.join(root, path)
    s = open(path, encoding="utf-8").read()
    orig = s

    if import_line and import_line not in s:
        anchor = re.search(r'^use "[^"]+"\.\*;', s, re.M)
        assert anchor, f"{path}: no `use` line to anchor the import to"
        s = s[:anchor.start()] + import_line + "\n" + s[anchor.start():]

    # 1. the struct's three fields become one list
    old = (f"pub struct:{struct} = {{\n"
           f"    wild {elem}->:items;\n"
           f"    int32:count;\n"
           f"    int32:cap;\n}};")
    old64 = old.replace("int32:count", "int64:count").replace("int32:cap", "int64:cap")
    if old in s:   s = s.replace(old, f"pub struct:{struct} = {{\n    List<{elem}>:v;\n}};")
    elif old64 in s: s = s.replace(old64, f"pub struct:{struct} = {{\n    List<{elem}>:v;\n}};")
    else: raise AssertionError(f"{path}: {struct} struct block not found")

    # 2. the init body becomes one list_init
    m = re.search(rf"(pub )?func:{initfn} = {struct}\(\) never fails \{{.*?\n\}};",
                  s, re.S)
    assert m, f"{path}: {initfn} not found"
    pub = m.group(1) or ""
    s = (s[:m.start()]
         + f"{pub}func:{initfn} = {struct}() never fails {{\n"
           f"    pass {struct}{{ v: raw list_init::<{elem}>({initcap}i64) }};\n}};"
         + s[m.end():])

    # 3. the grow function goes; a guarded narrow replaces it where the table
    #    hands its count out as an int32 id.
    m = re.search(rf"\n(pub )?func:{growfn} = NIL\({struct}->:\w+\) never fails \{{.*?\n\}};\n",
                  s, re.S)
    assert m, f"{path}: {growfn} not found"
    guard = ""
    if id_error:
        guard = (f"\n// An index into this table is int32 BY DESIGN -- that is what every\n"
                 f"// caller holds -- so the list's int64 count narrows to become one. The\n"
                 f"// narrowing is GUARDED rather than spelled `=>!`: a silent truncation\n"
                 f"// would give two entries the same index, and `{id_error}` is already\n"
                 f"// this module's declared defect, so no `failsafe` grows an arm.\n"
                 f"func:{struct.lower()}_id = int32(int64:n) never fails {{\n"
                 f"    if (n > 2147483647i64) {{ !!! {id_error}; }}\n"
                 f"    pass (n =>! int32);\n}};\n")
    s = s[:m.start()] + guard + s[m.end():]

    # 4. the push: the guard-and-grow goes, the store and the increment become
    #    one `list_push`. SCOPED, AND THE GROW NAME IS BOUNDARIED -- this
    #    deletion is the one rewrite that ran unscoped on the whole file, and
    #    it is the seventh way to miss a site AND the entire STOP-THE-LINE:
    #    family 10's `inst_grow\(t\)` matched INSIDE `fninst_record`'s
    #    `drop fninst_grow(t);` ("fninst_grow" ends in "inst_grow"; same file,
    #    same receiver name), so the tool stripped a live guard from a table it
    #    was not converting, and the un-grown table's push overflowed its block
    #    (see 1.4.7.md, "RESOLVED"). Three fixes, each sufficient alone: the
    #    deletion runs under the same binding test as every other rewrite, the
    #    name cannot match inside a longer one (`\b` -- `_` is a word char, so
    #    `\binst_grow` cannot start inside `fninst_grow`), and every deleted
    #    guard must pair with a converted push (asserted after step 4's store
    #    rewrite below).
    pat = re.compile(rf"    if \({var}\.count >= {var}\.cap\) \{{[^}}]*?\b{growfn}\({var}\);\s*\}}\n", re.S)
    deleted = [0]
    reserved = [0]
    def _guard_del(block, _hdr=""):
        # A GUARD EITHER PAIRS WITH AN APPEND OR IS A NAMED RESERVE. Family 12
        # is where this stopped being hypothetical: `TokenList` has two
        # legitimate guard-and-grow sites, because `tokenlist_split_shr` grows
        # and then SHIFTS THE TAIL UP to insert a second `>`, which has no
        # `items[count] = v; count = count + 1` for the push rewrite to match.
        # Deleting that guard would leave the shift writing past the end at
        # `count == cap` -- the family-10 defect, reached from the other side.
        # So the caller NAMES such functions and the guard becomes an explicit
        # `list_reserve`, which is what it always meant.
        if any(re.search(rf"func:{fn}\b", _hdr) for fn in reserve_fns):
            block, k = pat.subn(
                f"    drop list_reserve(@{var}.v, 1i64);\n", block)
            reserved[0] += k
            return block
        block, k = pat.subn("", block)
        deleted[0] += k
        return block
    s, _ = _scoped(s, struct, var, _guard_del)
    n = deleted[0]
    assert n >= 1, f"{path}: no guard-and-grow call site found for {growfn}"
    # the definition went in step 3 and the call sites just above; a surviving
    # WORD-boundary reference means the conversion missed one (a substring
    # inside a longer name -- `fninst_grow` after converting `inst_grow` -- is
    # another family's function and is fine).
    assert not re.search(rf"\b{growfn}\b", s), \
        f"{path}: {growfn} still referenced after conversion"
    # SCOPED, like every other rewrite below: two families in one file can share
    # a receiver name, and this pattern is generic enough to match the wrong
    # one. `ImplTable` and `BoundTable` in type_trait.npk both bind `t`, and an
    # unscoped pass converted `bound_push` while claiming to convert `impl_push`.
    pat2 = re.compile(rf"    {var}\.items\[{var}\.count(?: => int64)?\] = (\w+);\n"
                      rf"(    int32:(\w+) = {var}\.count;\n)?"
                      rf"    {var}\.count = {var}\.count \+ 1i(?:32|64);\n")
    def repl(m):
        val, idline, idname = m.group(1), m.group(2), m.group(3)
        out = ""
        if idline:
            out += f"    int32:{idname} = raw {struct.lower()}_id({var}.v.count);\n"
        out += f"    drop list_push(@{var}.v, {val});\n"
        return out
    counted = [0]
    def _push(block, _hdr=""):
        block, k = pat2.subn(repl, block)
        counted[0] += k
        return block
    s, _ = _scoped(s, struct, var, _push)
    n2 = counted[0]
    assert n2 >= 1, f"{path}: no store+increment found for {var}"
    # Every deleted guard pairs with a converted push in the same family. The
    # collateral deletion reported itself in the return string of the family-10
    # run -- "2 guard site(s), 1 push site(s)" -- and nothing refused the
    # mismatch. Now something does.
    assert reserved[0] == len(reserve_fns), (
        f"{path}: {reserved[0]} reserve rewrite(s) for {len(reserve_fns)} named "
        f"function(s) -- a named reserve site whose guard was not found")
    assert n == n2, (f"{path}: {n} guard deletion(s) vs {n2} push conversion(s)"
                     f" -- a deleted guard without its converted push means the"
                     f" tool edited a site it does not own")

    # 5. the count accessors narrow through the guard
    if id_error and count_fns:
        def _cnt(block, _hdr=""):
            return block.replace(f"pass {var}.count;",
                                 f"pass (raw {struct.lower()}_id({var}.v.count));")
        s, _ = _scoped(s, struct, var, _cnt)

    # 6. everything else. THE LOOP COUNTERS STAY int32 where they are indices:
    #    the comparison widens (`=>` is the lossless direction) rather than the
    #    counter narrowing.
    def _rewrite(block, _hdr=""):
        block = re.sub(rf"\bwhile \((\w+) < {var}\.count\)", rf"while ((\1 => int64) < {var}.v.count)", block)
        block = re.sub(rf"\bif \((\w+) >= {var}\.count\)",   rf"if ((\1 => int64) >= {var}.v.count)", block)
        block = re.sub(rf"\bif \((\w+) < {var}\.count\)",    rf"if ((\1 => int64) < {var}.v.count)", block)
        block = block.replace(f"{var}.items[", f"{var}.v.items[")
        block = block.replace(f"{var}.count", f"{var}.v.count").replace(f"{var}.cap", f"{var}.v.cap")
        return block.replace(f"{var}.v.v.", f"{var}.v.")
    s, touched = _scoped(s, struct, var, _rewrite)
    assert touched, f"{path}: no function signature mentions {struct}"

    # 7. refuse to write an incomplete conversion
    # THE SAME binding test the rewrite used. When these two disagreed, the
    # check reported lines from functions the rewrite had correctly skipped --
    # `t` bound to a different table in the same file -- and refused to write a
    # conversion that was in fact complete. It failed safe, but a check that
    # scopes differently from the change it is checking is not checking it.
    _bind = re.compile(rf"\b{struct}\s*(?:->)?\s*:\s*{var}\b")
    left = []
    for a, b, header in _fn_blocks(s):
        if not _bind.search(s[a:b]):
            continue
        for l in s[a:b].split("\n"):
            if re.search(rf"\b{var}\.(items|count|cap)\b", l):
                left.append(l)
    assert not left, f"{path}: unconverted sites remain:\n" + "\n".join(left)

    # A STRUCT LITERAL is a third place the old field names live, and it is not
    # inside any `{var}.` expression so neither check above sees it. Family 7's
    # `graph_init` built one by hand and TYPE-026 caught it; for these families
    # the literal is inside the `init` this converter rewrites wholesale, but
    # the assertion costs nothing and the next family may differ.
    for m in re.finditer(rf"{struct}\{{", s):
        tail = s[m.end():m.end() + 400]
        bad = [f for f in ("items:", "count:", "cap:") if f in tail.split("}")[0]]
        assert not bad, f"{path}: {struct} literal still names {bad}"
    assert s != orig
    if not dry:
        open(path, "w", encoding="utf-8").write(s)

    # OTHER FILES THAT BIND THIS STRUCT. A family's fields are not confined to
    # the file that declares them: `impl_scope_of` in emit_program.npk and a
    # sibling in ir_expr.npk both take `ImplTable->:t` and read `t.count`.
    # Single-file scoping cannot see that, and the leftover assertion below was
    # checking only the declaring file -- so it passed while the tree did not
    # build. The compiler found it in fifteen seconds; this makes the converter
    # find it instead.
    # SELF-DISCOVERING. Naming the other files by hand means reading them off a
    # compiler error, which is one round trip per family and only finds the
    # ones that happen to break the build first. Scanning for every file that
    # mentions the struct closes the class instead: a file that never binds it
    # simply yields no rewrite.
    if extra_files is None:
        extra_files = []
        for base in ("src", "tools", "tests"):
            for dirpath, _, names in os.walk(os.path.join(root, base)):
                for nm in names:
                    if not nm.endswith(".npk"):
                        continue
                    fp = os.path.join(dirpath, nm)
                    if os.path.abspath(fp) == os.path.abspath(path):
                        continue
                    try:
                        if struct in open(fp, encoding="utf-8").read():
                            extra_files.append(os.path.relpath(fp, root))
                    except OSError:
                        pass
        extra_files.sort()

    extras = []
    for ef in extra_files:
        ep = os.path.join(root, ef)
        es = open(ep, encoding="utf-8").read()
        eo = es
        es, touched_e = _scoped(es, struct, var, _rewrite)
        left_e = []
        for a, b, header in _fn_blocks(es):
            if not _bind.search(es[a:b]):
                continue
            for l in es[a:b].split("\n"):
                if re.search(rf"\b{var}\.(items|count|cap)\b", l):
                    left_e.append(l)
        assert not left_e, f"{ep}: unconverted sites remain:\n" + "\n".join(left_e)
        if es != eo:
            extras.append((ep, es, touched_e))
    if not dry:
        for ep, es, _ in extras:
            open(ep, "w", encoding="utf-8").write(es)
    return (f"{struct}: converted ({n} guard site(s), {n2} push site(s)"
            + (f", {len(extras)} other file(s)" if extras else "") + ")")
