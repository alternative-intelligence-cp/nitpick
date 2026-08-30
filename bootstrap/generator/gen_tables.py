#!/usr/bin/env python3
"""Regenerate the lexer's spec-derived tables.

Run from the repository root:  python3 bootstrap/generator/gen_tables.py

Writes three files, all derived from meta/specs/LEXICAL_REFERENCE.md:

    src/frontend/token_kind.npk   every token kind
    src/frontend/keywords.npk     keyword recognition, length-dispatched
    src/frontend/num_width.npk    numeric width suffixes

THROWAWAY tooling (D-085), but the generation itself is not a one-off: the specs
move -- six keywords were removed by D-041 and D-074 in a single day -- so
"generated from the spec once" is not a durable claim. This script makes it
repeatable, and bootstrap/harness/spec_coverage.py checks that the result still
matches.

Hand-typing two hundred variants is a good way to silently omit one, and an
omitted keyword lexes as an ordinary IDENTIFIER -- so the program still parses
and goes wrong somewhere else entirely.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPEC = os.path.join(ROOT, "meta", "specs", "LEXICAL_REFERENCE.md")
OUT = os.path.join(ROOT, "src", "frontend")

KEYWORD_GROUPS = [
    ("memory qualifiers",   "MemoryQualifier"),
    ("memory orderings",    "MemoryOrdering"),
    ("control flow",        "ControlFlow"),
    ("verification",        "VerificationKeyword"),
    ("async",               "AsyncKeyword"),
    ("modules",             "ModuleKeyword"),
    ("type keywords",       "TypeKeyword"),
    ("builtin types",       "BuiltinType"),
    ("builtin helpers",     "BuiltinHelper"),
    # BooleanLiteral and SentinelLiteral are defined in section 6.1 rather than
    # in the keyword productions, so a scan of section 4 alone misses them --
    # and `true` then lexes as an ORDINARY IDENTIFIER. They are spelled like
    # keywords and must be recognised like keywords.
    #
    # `unknown` is deliberately NOT here: the spec says it is a compiler-assigned
    # taint on Result.value, not something the programmer writes (TYPE_REFERENCE
    # section 27). `ERR` is, because it is the tbb sentinel and appears as a
    # pick match label (D-008).
    ("boolean literals",    "BooleanLiteral"),
    ("sentinel literals",   "SentinelLiteral"),
]

# Kinds that are not grammar terminals: structure, and the literal forms whose
# text is scanned rather than matched.
STRUCTURAL = [
    ("Eof",            "end of input"),
    ("Error",          "an invalid token; the lexer recovers past it"),
]
LITERALS = [
    ("IntLit",         "payload: the value"),
    ("FloatLit",       "payload: intern index of the text"),
    ("CharLit",        "payload: the code point"),
    ("StringLit",      "payload: intern index of the DECODED text"),
    ("RawStringLit",   'r"..." -- no escape processing (D-024)'),
    ("MultiStringLit", '"""...""" (D-024)'),
    # LEXICAL_REFERENCE 6.4 names five template tokens, not three. INTERP_START
    # and INTERP_END are separate kinds because a `}` that closes an
    # interpolation is not the same token as one that closes a block: after it
    # the lexer resumes TEMPLATE scanning rather than ordinary scanning.
    ("TemplateStart",  "` -- opens a template"),
    ("TemplatePart",   "literal text between interpolations"),
    ("InterpStart",    "&{ -- opens an interpolation"),
    ("InterpEnd",      "} -- closes an interpolation, resuming template mode"),
    ("TemplateEnd",    "` -- closes a template"),
    ("Ident",          "payload: intern index"),
]

OP_NAMES = {
    "+": "Plus", "-": "Minus", "*": "Star", "/": "Slash", "%": "Percent",
    "++": "PlusPlus", "--": "MinusMinus", "=": "Assign", "+=": "PlusAssign",
    "-=": "MinusAssign", "*=": "StarAssign", "/=": "SlashAssign",
    "%=": "PercentAssign", "&=": "AmpAssign", "|=": "PipeAssign",
    "^=": "CaretAssign", "<<=": "ShlAssign", ">>=": "ShrAssign",
    "==": "EqEq", "!=": "NotEq", "<": "Lt", "<=": "LtEq", ">": "Gt",
    ">=": "GtEq", "<=>": "Spaceship", "&&": "AndAnd", "||": "OrOr",
    "!": "Bang", "&": "Amp", "|": "Pipe", "^": "Caret", "~": "Tilde",
    "<<": "Shl", ">>": "Shr", "->": "ArrowTo", "<-": "ArrowBack",
    "=>": "FatArrow", "=>!": "FatArrowUnchecked", "@": "At", "$": "Dollar",
    "$$i": "BorrowImm", "$$m": "BorrowMut", "?": "Question",
    "?.": "QuestionDot", "??": "QuestionQuestion", "?!": "QuestionBang",
    "?|": "QuestionPipe", "_?": "UnderQuestion", "_!": "UnderBang",
    "_~": "UnderTilde", "_^": "UnderCaret", "!!!": "BangBangBang",
    "|>": "PipeFwd", "<|": "PipeBack", "..": "DotDot", "...": "DotDotDot",
    "..*": "DotDotStar", "..^": "DotDotCaret",
}
PUNCT_NAMES = {"(": "LParen", ")": "RParen", "{": "LBrace", "}": "RBrace",
               "[": "LBracket", "]": "RBracket", ".": "Dot", ",": "Comma",
               ":": "Colon", ";": "Semi", "`": "Backtick"}


# --- THE BUILTIN REFERENCE'S ROWS, PARSED (D-201, 1.4.2) ----------------------
#
# One row per builtin, and the row is the whole authority. Before this the same
# fact lived in four places -- the reference, `builtins.npk`'s name list, the
# checker's thirteen bespoke arms, and the emitter's `rt_sig` -- and nothing
# diffed them, so `read`'s parameter was documented as the untyped word `ptr` and
# the emitter's coercion was signedness-blind because no one had written the type
# down anywhere a compiler could read it.
#
# STRICT BY CONSTRUCTION: a row this cannot parse is a hard failure, not a
# skipped entry. A silently skipped builtin is one the checker types as UNKNOWN,
# which is exactly the state D-201 exists to end.

SPECIALS = {
    # Variadic: the syscall number and up to six register arguments (D-048).
    "sys",
    # Element from the TURBOFISH, not from an argument (D-187).
    "atomic_from_ptr",
    # The seven annotation-directed constructors: element, lock level, capacity
    # and party count all live in the TYPE the call is given (D-072/D-152/D-056).
    "arena_make", "shared_arena_make", "channel", "mutex", "rwlock", "condvar",
    "barrier",
}

# The one place a language type becomes an LLVM type. Small and closed on
# purpose -- the floor is the floor -- and every derivation it feeds is diffed
# against `npkrt.ll`'s own defines by `check_runtime_sigs_agree` on every run.
LL_OF = {
    "NIL": "void", "bool": "i8",
    "int8": "i8", "int16": "i16", "int32": "i32", "int64": "i64",
    "uint8": "i8", "uint16": "i16", "uint32": "i32", "uint64": "i64",
    "string": "{ ptr, i64, i64 }", "cstring": "{ ptr, i64 }",
    "buffer": "{ ptr, i64, i64 }",
    # The five kernel identifiers are one register-width number each (D-042).
    "fd": "i32", "pid": "i32", "tid": "i32", "uid": "i32", "gid": "i32",
    # An OwnedFd IS the descriptor -- the ownership is the compiler's bookkeeping,
    # not a field (D-185).
    "OwnedFd": "i32",
}

QUALIFIERS = ("wild ", "wildx ", "stack ", "const ", "fixed ")


class Param(object):
    __slots__ = ("type", "name", "move")

    def __init__(self, type, name, move):
        self.type, self.name, self.move = type, name, move


class Row(object):
    __slots__ = ("name", "params", "ret", "never_fails", "special", "abi")

    def __init__(self, name, params, ret, never_fails, special, abi):
        self.name, self.params, self.ret = name, params, ret
        self.never_fails, self.special, self.abi = never_fails, special, abi


# `->` IS AN ATOM, NOT A CLOSING BRACKET. The pointer suffix ends in `>`, so a
# naive depth counter reads `wild any->:ptr` as closing a generic that was never
# opened and every later depth test is off by one -- which is how the first run
# of this parser reported `any->:ptr` as a type with no LLVM shape.
def depth_steps(text):
    """Yield (index, char, depth-before-this-char), skipping `->` pairs."""
    depth, i = 0, 0
    while i < len(text):
        ch = text[i]
        if ch == "-" and text[i + 1:i + 2] == ">":
            yield i, "-", depth
            yield i + 1, ">", depth
            i += 2
            continue
        if ch in "<([":
            yield i, ch, depth
            depth += 1
        elif ch in ">)]":
            depth -= 1
            yield i, ch, depth
        else:
            yield i, ch, depth
        i += 1


def split_top(text):
    """Comma-split at nesting depth zero: `Mutex<T, LEVEL>` is one type."""
    out, cur = [], ""
    for _, ch, depth in depth_steps(text):
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def parse_param(text, where):
    move = False
    t = text.strip()
    if t.startswith("move "):
        move, t = True, t[5:].strip()
    # `int64:size` -- the name is documentation. A `:` inside a type is not a
    # thing the language has, so the LAST colon at depth zero is the divider.
    name, cut = "", -1
    for i, ch, depth in depth_steps(t):
        if ch == ":" and depth == 0:
            cut = i
    if cut >= 0:
        name = t[cut + 1:].strip()
        t = t[:cut].strip()
        if not re.match(r'^\w+$', name):
            raise SystemExit("BUILTIN_REFERENCE.md: %s has an unreadable "
                             "parameter name %r" % (where, name))
    if not t:
        raise SystemExit("BUILTIN_REFERENCE.md: %s has an empty parameter type"
                         % where)
    return Param(t, name, move)


def parse_signature(sig, where):
    if sig.count("→") != 1:
        raise SystemExit("BUILTIN_REFERENCE.md: %s's signature needs exactly one "
                         "U+2192 return arrow: %r" % (where, sig))
    lhs, ret = [p.strip() for p in sig.split("→")]
    if not ret:
        raise SystemExit("BUILTIN_REFERENCE.md: %s has no return type" % where)
    if lhs.startswith("(") and lhs.endswith(")"):
        inner = lhs[1:-1].strip()
        params = [parse_param(p, where) for p in split_top(inner)] if inner else []
    elif lhs:
        params = [parse_param(lhs, where)]
    else:
        raise SystemExit("BUILTIN_REFERENCE.md: %s has no parameter list -- write "
                         "`()` for none" % where)
    return params, ret


ABI_KEYS = ("sym", "ret", "args")


def parse_abi(cell, where):
    """The `**ABI:**` note: `inline`, `envelope`, and the three backticked keys."""
    if "**ABI:**" not in cell:
        return {}
    note = cell.split("**ABI:**", 1)[1].strip()
    abi = {}
    for m in re.finditer(r'(\w+)=`([^`]*)`|\b(inline|envelope)\b', note):
        if m.group(3):
            abi[m.group(3)] = True
            continue
        if m.group(1) not in ABI_KEYS:
            raise SystemExit("BUILTIN_REFERENCE.md: %s's ABI note uses the unknown "
                             "key `%s`" % (where, m.group(1)))
        abi[m.group(1)] = m.group(2)
    if not abi:
        raise SystemExit("BUILTIN_REFERENCE.md: %s carries an ABI note this cannot "
                         "read: %r" % (where, note))
    return abi


def builtin_rows(path):
    src = open(path, encoding="utf-8").read()
    marked = "\n".join(re.findall(
        r"<!-- builtins:begin -->(.*?)<!-- builtins:end -->", src, re.S))
    rows = {}
    for line in marked.split("\n"):
        m = re.match(r'^\|\s*`(\w+)`\s*\|', line)
        if not m:
            continue
        name = m.group(1)
        where = "`%s`" % name
        cells = line.split("|")
        if len(cells) != 6:
            raise SystemExit("BUILTIN_REFERENCE.md: %s's row has %d cells, not the "
                             "four of `| name | signature | notes | fails |`"
                             % (where, len(cells) - 2))
        sig = cells[2].strip()
        if not (sig.startswith("`") and sig.endswith("`")):
            raise SystemExit("BUILTIN_REFERENCE.md: %s's Signature cell is not one "
                             "backticked signature: %r" % (where, sig))
        params, ret = parse_signature(sig[1:-1].strip(), where)
        fails = cells[4]
        never = "never fails" in fails
        if not never and "may fail" not in fails:
            raise SystemExit("BUILTIN_REFERENCE.md: %s's Fails column says neither "
                             "`never fails` nor `may fail`" % where)
        # THE TWO COLUMNS ARE CHECKED AGAINST EACH OTHER. `Result<T>` in the
        # signature and "may fail" in the Fails column are the same fact stated
        # twice, and D-201 makes the first one load-bearing: a never-fails
        # builtin's call types as the BARE value, so a stray `Result<...>` here
        # would wrap ~1,700 sites that cannot fail.
        wrapped = ret.startswith("Result<")
        if wrapped != (not never):
            raise SystemExit("BUILTIN_REFERENCE.md: %s's signature returns %s and "
                             "its Fails column says %s -- one of the two is wrong"
                             % (where, ret, "never fails" if never else "may fail"))
        if wrapped:
            ret = ret[len("Result<"):-1].strip()
        special = name in SPECIALS
        if ("**SPECIAL**" in cells[3]) != special:
            raise SystemExit("BUILTIN_REFERENCE.md: %s is %smarked **SPECIAL** in "
                             "the reference and %sin the generator's list"
                             % (where, "" if not special else "not ",
                                "" if special else "not "))
        if name in rows:
            raise SystemExit("BUILTIN_REFERENCE.md: %s has two rows" % where)
        rows[name] = Row(name, params, ret, never, special,
                         parse_abi(cells[3], where))
    missing = sorted(SPECIALS - set(rows))
    if missing:
        raise SystemExit("the generator's SPECIALS names builtins the reference "
                         "does not: %s" % ", ".join(missing))

    # The emitter-only symbols: no language signature exists, so the ABI is all
    # there is to state (BUILTIN_REFERENCE section 2d).
    rt = []
    for region in re.findall(r"<!-- rtsyms:begin -->(.*?)<!-- rtsyms:end -->",
                             src, re.S):
        for line in region.split("\n"):
            m = re.match(r'^\|\s*`(\w+)`\s*\|\s*`(@\w+)`\s*\|\s*`([^`]+)`\s*\|'
                         r'\s*`([^`]*)`\s*\|\s*$', line)
            if line.strip().startswith("|") and not m:
                if not re.match(r'^\|\s*(Key|-+)', line.strip()):
                    raise SystemExit("BUILTIN_REFERENCE.md section 2d: this row is "
                                     "unreadable: %r" % line)
                continue
            if m:
                args = [a.strip() for a in m.group(4).split(",") if a.strip()]
                rt.append((m.group(1), m.group(2), m.group(3), False, "", args))
    if not rt:
        raise SystemExit("BUILTIN_REFERENCE.md: section 2d's rtsyms region is "
                         "empty -- the emitter would declare no arena accessors")
    return rows, rt


def ll_of(t, where):
    """A language type's LLVM shape. Qualifiers are documentation (parse_type)."""
    for q in QUALIFIERS:
        if t.startswith(q):
            t = t[len(q):].strip()
    if t.endswith("->"):
        return "ptr"
    if t in LL_OF:
        return LL_OF[t]
    raise SystemExit("BUILTIN_REFERENCE.md: %s's type `%s` has no LLVM shape -- "
                     "add it to gen_tables.LL_OF, or write an `ABI:` note"
                     % (where, t))


def runtime_entries(rows, rtsyms):
    """name -> (symbol, llvm return, wrapped, inner, [llvm args]), sorted.

    Derived from the language signature, with the reference's `ABI:` notes
    overriding where the symbol departs. A builtin marked `inline` has no floor
    symbol at all -- `own_fd` is a no-op, `string_is_empty` is a compare -- and
    contributes nothing here.
    """
    out = {}
    for name in sorted(rows):
        r = rows[name]
        if r.abi.get("inline"):
            continue
        where = "`%s`" % name
        sym = r.abi.get("sym", "@npk_" + name)
        wrapped = (not r.never_fails) or bool(r.abi.get("envelope"))
        if "ret" in r.abi:
            ret, inner = r.abi["ret"], ""
        else:
            succ = ll_of(r.ret, where)
            if not wrapped:
                ret, inner = succ, ""
            elif succ == "void":
                ret, inner = "{ i32 }", ""
            else:
                ret, inner = "{ %s, i32 }" % succ, succ
        if "args" in r.abi:
            args = [a.strip() for a in r.abi["args"].split(",") if a.strip()]
        else:
            args = [ll_of(p.type, where) for p in r.params]
        if len(args) > 4:
            raise SystemExit("BUILTIN_REFERENCE.md: %s takes %d arguments and "
                             "RtSig carries four" % (where, len(args)))
        out[name] = (sym, ret, wrapped, inner, args)
    for name, sym, ret, wrapped, inner, args in rtsyms:
        if name in out:
            raise SystemExit("BUILTIN_REFERENCE.md: `%s` is both a builtin and a "
                             "section 2d runtime symbol" % name)
        out[name] = (sym, ret, wrapped, inner, args)
    return out


def write_runtime(rows, rtsyms):
    ents = runtime_entries(rows, rtsyms)
    names = sorted(ents)
    o = ['// The runtime floor\'s signatures, as the emitter must declare and call them.',
         "//",
         "// GENERATED by bootstrap/generator/gen_tables.py from",
         "// meta/specs/BUILTIN_REFERENCE.md -- the builtin rows' signatures with their",
         "// `ABI:` notes applied, plus section 2d's emitter-only symbols.",
         "//",
         "// THIS WAS THE THIRD HAND-WRITTEN COPY OF ONE FACT until D-201 (1.4.2).",
         "// `runtime/npkrt.ll` DEFINES these functions; the seed\'s `RUNTIME` table",
         "// declared them for stage 1; this table declares them for the compiler --",
         "// and the reference documented them for a reader with nothing diffing it.",
         "// The seed\'s copy went with the seed at the 1.4.6 switch (D-205), so the",
         "// diff is two-way now. The reference is the source of this file, and",
         "// `check_runtime_sigs_agree` parses the defines out of `npkrt.ll` and compares",
         "// what is left on every full run. The runtime's own header records what a",
         "// disagreement does: \"declaring a runtime symbol as returning a bare string",
         "// while the checker types it as Result<string> is exactly the kind of silent",
         "// disagreement that produces IR llc rejects -- and it did.\"",
         "//",
         "// `wrapped` AND `inner` ARE STORED, NOT DERIVED HERE. Whether a return is a",
         "// `Result` decides whether `raw` extracts or passes through, and sniffing it",
         "// out of the type text (\"does it end in `, i32 }`\") would make `cstring`'s",
         "// `{ ptr, i64 }` one brace away from a wrong answer -- and `arena_alloc`'s",
         "// `Handle<T>` wears a Result's shape without being one.",
         "",
         "mod:ir_runtime;",
         "",
         'use "../../frontend/intern.npk".*;',
         "",
         "pub struct:RtSig = {",
         "    bool:found;",
         '    string:sym;       // "@npk_alloc"',
         '    string:ret;       // the FULL return as npkrt.ll defines it; "void" for none',
         "    bool:wrapped;     // the return is a Result and `raw`/`relay` must extract",
         '    string:inner;     // the value half when wrapped; "" when Result<NIL>',
         "    int32:argc;",
         "    string:a0;",
         "    string:a1;",
         "    string:a2;",
         "    string:a3;",
         "};",
         "",
         "func:rt = RtSig(move string:sym, move string:ret, bool:wrapped, move string:inner,",
         "                int32:argc, move string:a0, move string:a1, move string:a2,",
         "                move string:a3) never fails {",
         "    RtSig:s = RtSig{ found: true, sym: move(sym), ret: move(ret), wrapped: wrapped,",
         "                     inner: move(inner), argc: argc, a0: move(a0), a1: move(a1),",
         "                     a2: move(a2), a3: move(a3) };",
         "    pass s;",
         "};",
         "",
         "// The floor, one entry per symbol `npkrt.ll` defines for programs. `npk_start`",
         "// is the entry shim and not callable; `npk_exit` is here because `exit` lowers",
         "// to it (0.7.6), argc 1, void.",
         "pub func:rt_sig = RtSig(string:name) never fails {"]
    for n in names:
        sym, ret, wrapped, inner, args = ents[n]
        a = list(args) + ["", "", "", ""]
        o.append('    if (raw string_eq(name, "%s")) {' % n)
        o.append('        pass (raw rt("%s", "%s", %s, "%s", %di32,'
                 % (sym, ret, "true" if wrapped else "false", inner, len(args)))
        o.append('                     "%s", "%s", "%s", "%s"));'
                 % (a[0], a[1], a[2], a[3]))
        o.append("    }")
    o.append('    RtSig:none = RtSig{ found: false, sym: "", ret: "", wrapped: false,')
    o.append('                        inner: "", argc: 0i32, a0: "", a1: "", a2: "", a3: "" };')
    o.append("    pass none;")
    o.append("};")
    o.append("")
    o.append("// The floor, ITERABLE AND IN ONE FIXED ORDER -- the declare block of every")
    o.append("// emitted module walks this, so the order is part of D-078's byte-determinism.")
    o.append("pub func:rt_count = int32() never fails { pass %di32; };" % len(names))
    o.append("")
    o.append("pub func:rt_name_at = string(int32:i) never fails {")
    for i, n in enumerate(names[:-1]):
        o.append('    if (i == %di32) { pass "%s"; }' % (i, n))
    o.append('    pass "%s";' % names[-1])
    o.append("};")
    o.append("")
    o.append("// Cloned, not passed through (D-183): `s` is lent, and this runs a handful of")
    o.append("// times per runtime symbol while the declare block is written -- cold.")
    o.append("pub func:rt_arg = string(RtSig:s, int32:i) never fails {")
    o.append('    if (i == 0i32) { pass (string_concat(s.a0, "")); }')
    o.append('    if (i == 1i32) { pass (string_concat(s.a1, "")); }')
    o.append('    if (i == 2i32) { pass (string_concat(s.a2, "")); }')
    o.append('    pass (string_concat(s.a3, ""));')
    o.append("};")
    path = os.path.join(ROOT, "src", "backend", "ir", "ir_runtime.npk")
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(o) + "\n")
    print("runtime floor: %d symbols (%d builtins, %d emitter-only)"
          % (len(names), len(names) - len(rtsyms), len(rtsyms)))


def terminals(spec, header):
    m = re.search(r'^%s\s*::=(.*?)(?=\n\n|\n[A-Z][A-Za-z]*\s*::=|\n```)' % header,
                  spec, re.S | re.M)
    if not m:
        raise SystemExit("production not found in the spec: " + header)
    return re.findall(r'"([^"]+)"', m.group(1))


def kw_variant(k):
    return "Kw" + (k if k[0].isupper() else k.capitalize())


def width_variant(s):
    return "W" + (s.capitalize() if s[0].isalpha() else s)


def main():
    spec = open(SPEC, encoding="utf-8").read()

    # --- token_kind.npk ------------------------------------------------------
    lines, idx = [], 0

    def emit(name, comment=None):
        nonlocal idx
        lines.append("    %-26s = %di32;%s"
                     % (name, idx, ("   // " + comment) if comment else ""))
        idx += 1

    lines.append("pub enum:TokenKind = {")
    lines.append("    // --- structural ---------------------------------------------------")
    for n, c in STRUCTURAL:
        emit(n, c)
    lines.append("")
    lines.append("    // --- literals -----------------------------------------------------")
    for n, c in LITERALS:
        emit(n, c)

    kw_all = []
    for title, prod in KEYWORD_GROUPS:
        ks = [k for k in terminals(spec, prod) if k not in kw_all]
        kw_all += ks
        lines.append("")
        lines.append("    // --- %s %s" % (title, "-" * max(0, 62 - len(title))))
        for k in ks:
            emit(kw_variant(k), k)

    lines.append("")
    lines.append("    // --- the compiler-directive sigil (D-020) -------------------------")
    emit("Hash", "# -- not a value operator")
    lines.append("")
    lines.append("    // --- operators ----------------------------------------------------")
    ops = terminals(spec, "Operator")
    for o in ops:
        emit("Op" + OP_NAMES[o], o)
    lines.append("")
    lines.append("    // --- punctuation --------------------------------------------------")
    punct = terminals(spec, "Punctuation")
    for o in punct:
        emit("P" + PUNCT_NAMES[o], o)
    lines.append("};")

    write("token_kind.npk", '''// Token kinds.
//
// GENERATED by bootstrap/generator/gen_tables.py from
// meta/specs/LEXICAL_REFERENCE.md sections 4, 5 and 6. Do not edit by hand;
// re-run the generator and let bootstrap/harness/spec_coverage.py check it.
//
// EVERY keyword and operator has a kind, including those for constructs no
// backend rung can lower yet. They still LEX; capability rejection belongs in
// the backend (D-085). A lexer that recognises only what today's rung supports
// is a partial grammar by another name.

''' + "\n".join(lines) + "\n")

    # --- keywords.npk --------------------------------------------------------
    by_len = {}
    for k in kw_all:
        by_len.setdefault(len(k), []).append(k)
    kl = ['''use "./token_kind.npk".*;
use "./intern.npk".*;

pub func:keyword_kind_of = TokenKind(string:text) {
    int64:n = text.len;''']
    for n in sorted(by_len):
        kl.append("    if (n == %di64) {" % n)
        for k in sorted(by_len[n]):
            kl.append('        if (raw string_eq(text, "%s")) { pass TokenKind.%s; }'
                      % (k, kw_variant(k)))
        kl.append("    }")
    kl.append('''
    // Not a keyword. The caller keeps it as an identifier.
    pass TokenKind.Ident;
};

pub func:is_keyword = bool(string:text) {
    TokenKind:k = raw keyword_kind_of(text);
    pass (k != TokenKind.Ident);
};''')
    write("keywords.npk", '''// Keyword recognition.
//
// GENERATED by bootstrap/generator/gen_tables.py from
// meta/specs/LEXICAL_REFERENCE.md section 4.
//
// Keywords are recognised AFTER a whole identifier has been scanned, never by
// prefix matching -- so `iffy` is an identifier and not `if` followed by `fy`.
// Dispatching on LENGTH first turns 154 comparisons into a handful.

''' + "\n".join(kl) + "\n")

    # --- num_width.npk -------------------------------------------------------
    sfx = terminals(spec, "TypeSuffix")
    wl = ["pub enum:NumWidth = {",
          "    WNone                = 0i32;   // no suffix written"]
    for i, s in enumerate(sfx, start=1):
        wl.append("    %-20s = %di32;   // %s" % (width_variant(s), i, s))
    wl.append("};")
    wl.append("")
    wl.append('use "./intern.npk".*;')
    wl.append('use "./types.npk".*;')
    wl.append("")
    wl.append("pub func:num_width_of = NumWidth(string:text) {")
    wl.append("    int64:n = text.len;")
    wb = {}
    for s in sfx:
        wb.setdefault(len(s), []).append(s)
    for n in sorted(wb):
        wl.append("    if (n == %di64) {" % n)
        for s in sorted(wb[n]):
            wl.append('        if (raw string_eq(text, "%s")) { pass NumWidth.%s; }'
                      % (s, width_variant(s)))
        wl.append("    }")
    wl.append("    pass NumWidth.WNone;")
    wl.append("};")
    wl.append("")
    wl.append("pub func:num_width_is_float = bool(NumWidth:w) {")
    for s in sfx:
        if s.startswith("f") or s.startswith("tfp") or s == "dim256":
            wl.append("    if (w == NumWidth.%s) { pass true; }" % width_variant(s))
    wl.append("    pass false;")
    wl.append("};")

    # A suffix names a TYPE, so it has to hand back one -- bits and signedness.
    # Derived from the suffix text rather than a second table: `u32` is 32 bits
    # and unsigned and says so.
    wl.append("")
    wl.append("// The bits a suffix names, or 0 for none. `42i32` is 32.")
    wl.append("pub func:num_width_bits = int32(NumWidth:w) {")
    for s2 in sfx:
        m = re.match(r'^([a-z]+)(\d+)$', s2)
        # 1.3.3 (D-196): `dim256` carries its 256 like the tfp suffixes do.
        if m and m.group(1) in ("u", "i", "f", "tbb", "frac", "tfp", "dim", "char"):
            wl.append("    if (w == NumWidth.%s) { pass %si32; }"
                      % (width_variant(s2), m.group(2)))
    wl.append("    pass 0i32;")
    wl.append("};")
    wl.append("")
    wl.append("// `i` is signed, `u` is not. A suffix that names neither is not an")
    wl.append("// integer suffix and the caller has already checked that.")
    wl.append("pub func:num_width_signed = bool(NumWidth:w) {")
    for s2 in sfx:
        if re.match(r'^i\d+$', s2):
            wl.append("    if (w == NumWidth.%s) { pass true; }" % width_variant(s2))
    wl.append("    pass false;")
    wl.append("};")
    wl.append("")
    wl.append("// An INTEGER suffix specifically -- `i32`/`u32`, not `f64` or `tbb32`.")
    wl.append("pub func:num_width_is_integer = bool(NumWidth:w) {")
    for s2 in sfx:
        if re.match(r'^[iu]\d+$', s2):
            wl.append("    if (w == NumWidth.%s) { pass true; }" % width_variant(s2))
    wl.append("    pass false;")
    wl.append("};")

    # WHICH TYPE A SUFFIX NAMES, as a TY_* kind rather than a yes/no per family.
    #
    # `num_width_is_integer` and `num_width_is_float` between them classified
    # twenty-six of the forty-five suffixes, and the checker treated the rest as
    # "no suffix at all". So `1tbb8` -- a literal the language plainly has --
    # reported "this literal has no width suffix", and `1.5tfp256` typed as a
    # `flt256` because the float predicate had swept `tfp` and `dim256` in with
    # the floats.
    #
    # One function, derived from the suffix text, with every suffix accounted
    # for. Emitted BY NAME so there is no numeric coupling to types.npk.
    FAMILY = [(r'^u\d+$', "TY_INT"), (r'^i\d+$', "TY_INT"),
              (r'^f\d+$', "TY_FLOAT"), (r'^tbb\d+$', "TY_TBB"),
              # 1.3.2 (D-195): the tfp suffixes name the twisted fixed kind.
              (r'^tfp\d+$', "TY_TFP"),
              # 1.3.3 (D-196): the dim suffix -- tfp256 with a unit tail.
              (r'^dim256$', "TY_DIM"),
              (r'^char\d+$', "TY_CHAR")]
    wl.append("")
    wl.append("// The TYPE a suffix names, as a `TY_*` kind.")
    wl.append("//")
    wl.append("// TY_INVALID for a suffix whose type has no kind yet -- the `frac`")
    wl.append("// widths are REAL types arriving at a later rung (1.3.5), and the")
    wl.append("// caller names the rung rather than pretending the suffix is absent")
    wl.append("// (D-085).")
    wl.append("pub func:num_width_kind = int32(NumWidth:w) {")
    for s2 in sfx:
        for pat, ty in FAMILY:
            if re.match(pat, s2):
                wl.append("    if (w == NumWidth.%s) { pass (raw %s()); }"
                          % (width_variant(s2), ty))
                break
    wl.append("    pass 0i32;")
    wl.append("};")
    write("num_width.npk", '''// Numeric literal width suffixes.
//
// GENERATED by bootstrap/generator/gen_tables.py from
// meta/specs/LEXICAL_REFERENCE.md section 6.2 (TypeSuffix).
//
// A literal's width is part of its TYPE, not part of its value, which is why a
// token carries it as a separate field rather than folding it into the payload.

''' + "\n".join(wl) + "\n")

    # --- operators.npk -------------------------------------------------------
    # Every operator, punctuation mark, and the compiler sigil, matched
    # LONGEST FIRST so `<=>` beats `<=`, `..*` beats `..`, and `=>!` beats `=>`.
    all_ops = [(o, "Op" + OP_NAMES[o]) for o in ops]
    all_ops += [(o, "P" + PUNCT_NAMES[o]) for o in punct]
    all_ops += [("#", "Hash")]
    ob = {}
    for text, kind in all_ops:
        ob.setdefault(len(text), []).append((text, kind))

    ol = []
    ol.append('use "./token_kind.npk".*;')
    ol.append('use "./intern.npk".*;')
    ol.append('')
    ol.append('pub struct:OpMatch = {')
    ol.append('    bool:found;')
    ol.append('    TokenKind:kind;')
    ol.append('    int64:length;')
    ol.append('};')
    ol.append('')
    ol.append('pub func:op_match = OpMatch(string:text, int64:at) {')
    ol.append('    OpMatch:none = OpMatch{ found: false, kind: TokenKind.Error, length: 0i64 };')
    for n in sorted(ob, reverse=True):
        ol.append('')
        ol.append('    if ((at + %di64) <= text.len) {' % n)
        # `at + n <= len` is checked on the line above, so the slice cannot
        # fail; `?!` states that (D-163 rule 2's spelling for a provably-dead
        # branch), which is what lets `op_match` be `never fails`.
        ol.append('        string:s%d = raw slice_proven(text, at, at + %di64);' % (n, n))
        for text, kind in sorted(ob[n]):
            esc = text.replace('\\', '\\\\').replace('"', '\\"')
            ol.append('        if (raw string_eq(s%d, "%s")) {' % (n, esc))
            ol.append('            OpMatch:m%s = OpMatch{ found: true, kind: TokenKind.%s, length: %di64 };'
                      % (kind, kind, n))
            ol.append('            pass m%s;' % kind)
            ol.append('        }')
        ol.append('    }')
    ol.append('')
    ol.append('    pass none;')
    ol.append('};')

    op_header = (
        "// Operator and punctuation matching.\n//\n"
        "// GENERATED by bootstrap/generator/gen_tables.py from\n"
        "// meta/specs/LEXICAL_REFERENCE.md section 5.\n//\n"
        "// LONGEST MATCH FIRST, which is the whole content of the table: `<=>` must\n"
        "// beat `<=`, `..*` must beat `..`, `=>!` must beat `=>`, and `!!!` must beat\n"
        "// `!`. Trying shortest-first would silently produce a different program.\n//\n"
        "// The positional `!` rule (D-046) falls out of this for free: `!!!` matches as\n"
        "// one token, `!=` as one, and a lone `!` as negation. There is no `!!`\n"
        "// operator -- after D-001 removed sys!!! and asm!!! the tier marker\n"
        "// distinguished nothing -- so `!!` is simply two negations, which is not an\n"
        "// error at all.\n\n")
    write("operators.npk", op_header + "\n".join(ol) + "\n")

    # --- token_name.npk ------------------------------------------------------
    # A kind -> spelling table, for diagnostics. "expected `;`, found `}`" is a
    # far better message than "expected token 231", and the parser has no other
    # way to say it: the token carries a kind, not the text it came from.
    nl = []
    nl.append('use "./token_kind.npk".*;')
    nl.append('')
    nl.append('// `pass` from each arm rather than `give`: expression-`pick` (D-059) is')
    nl.append('// outside subset 1, and returning directly is equivalent here.')
    nl.append('pub func:token_kind_name = string(TokenKind:k) {')
    nl.append('    pick (k) {')
    named = [(n, c) for n, c in STRUCTURAL] + [(n, c) for n, c in LITERALS]
    arms = []
    for n, c in named:
        arms.append((n, n))
    for k in kw_all:
        arms.append((kw_variant(k), k))
    arms.append(("Hash", "#"))
    for o in ops:
        arms.append(("Op" + OP_NAMES[o], o))
    for o in punct:
        arms.append(("P" + PUNCT_NAMES[o], o))
    for i, (variant, text) in enumerate(arms):
        esc = text.replace("\\", "\\\\").replace('"', '\\"')
        sep = "," if i + 1 < len(arms) else ""
        nl.append('        (TokenKind.%s) { pass "%s"; }%s' % (variant, esc, sep))
    nl.append('    }')
    nl.append('    pass "<unknown>";')
    nl.append('};')
    # A builtin type name lexes as a KEYWORD, not an identifier, so the type
    # parser needs to know which keywords may begin a type. Without this it
    # would reject `int32` as "expected a type".
    nl.append('')
    nl.append('pub func:token_kind_is_builtin_type = bool(TokenKind:k) {')
    for b in terminals(spec, "BuiltinType"):
        nl.append('    if (k == TokenKind.%s) { pass true; }' % kw_variant(b))
    nl.append('    pass false;')
    nl.append('};')
    write("token_name.npk",
          "// Token spellings, for diagnostics.\n//\n"
          "// GENERATED by bootstrap/generator/gen_tables.py.\n//\n"
          "// \"expected `;`, found `}`\" is a far better message than \"expected token\n"
          "// 231\", and the parser has no other way to say it: a token carries a kind,\n"
          "// not the text it was scanned from.\n\n"
          + "\n".join(nl) + "\n")

    # --- ast_kind.npk --------------------------------------------------------
    # Node kinds, by the section of AST_REFERENCE.md they are defined in. The
    # document's own structure decides the category; guessing from a name suffix
    # would put IntLiteral in the wrong one.
    ref = open(os.path.join(ROOT, "meta", "specs", "AST_REFERENCE.md"),
               encoding="utf-8").read()
    SECT = {"1": "Decl", "2": "Stmt", "3": "Expr", "4": "Type", "5": "Verify"}
    cur, cats = None, {}
    for ln in ref.split("\n"):
        m = re.match(r'^# (\d)\.', ln)
        if m:
            cur = SECT.get(m.group(1))
            continue
        if ln.startswith("# 7.") or ln.startswith("# 8."):
            cur = None
            continue
        m = re.match(r'^\|\s*\*{0,2}`(\w+)`\*{0,2}\s*\|', ln)
        if m and cur:
            cats.setdefault(cur, [])
            if m.group(1) not in cats[cur]:
                cats[cur].append(m.group(1))

    # PickPattern is a variant list in prose rather than a table (2.1).
    cats["Pat"] = ["Value", "Range", "StructDestructure", "EnumDestructure",
                   "ErrPattern", "Wildcard"]

    # Sub-structures defined as code blocks rather than table rows -- 1.1, 1.2,
    # 2.1, 6. They are real nodes and need kinds; the table scan cannot see them.
    #
    # `FailsOn` and `NeverFails` are two kinds rather than one `FailureContract`
    # with a flag, matching PickPattern's six arms. D-002 wants "this C function
    # is infallible" to be an auditable claim, and a shared kind would make an
    # unwritten contract and a written `never fails` the same node.
    #
    # `Attribute` lives in the declaration array because that array is already
    # where declaration sub-structures live.
    cats["Decl"] += ["GenericParam", "ParamDecl", "FieldDecl", "EnumVariant",
                     "ExternFn", "VariadicSpec", "FailsOn", "NeverFails",
                     "Attribute"]
    cats["Stmt"] += ["PickArm"]

    al = []
    counts = {}
    for cat in ("Decl", "Stmt", "Expr", "Type", "Verify", "Pat"):
        kinds = cats.get(cat, [])
        counts[cat] = len(kinds)
        al.append("pub enum:%sKind = {" % cat)
        al.append("    %-28s = 0i32;   // no node / not yet set" % (cat + "None"))
        for i, k in enumerate(kinds, start=1):
            al.append("    %-28s = %di32;" % (cat + k, i))
        al.append("};")
        al.append("")
        # The HIGHEST kind in each enum. A walker that has to prove it classifies
        # every kind needs an upper bound, and one derived from the same list the
        # enum came from cannot fall behind it.
        al.append("pub func:%s_KIND_MAX = int32() { pass %di32; };"
                  % (cat.upper(), len(kinds)))
        al.append("")

    ast_header = (
        "// AST node kinds.\n//\n"
        "// GENERATED by bootstrap/generator/gen_tables.py from\n"
        "// meta/specs/AST_REFERENCE.md, categorised by the SECTION each node is\n"
        "// defined in rather than by its name -- IntLiteral is an expression, and a\n"
        "// name-suffix guess would file it somewhere else.\n//\n"
        "// Every kind exists, including those for constructs no backend rung can\n"
        "// lower. The parser never restricts (D-085).\n//\n"
        "// Each enum reserves 0 for \"none\", so a zeroed node is inert rather than\n"
        "// silently claiming to be whichever kind happens to be first.\n\n")
    write("ast_kind.npk", ast_header + "\n".join(al) + "\n")
    print("ast kinds: " + "  ".join("%s=%d" % (k, v) for k, v in counts.items()))

    # --- builtin_types.npk ----------------------------------------------------
    # Every BuiltinType keyword mapped to what it IS. The names encode it --
    # `uint64` is an unsigned 64-bit integer and says so -- so this is derived
    # rather than hand-listed, and a type added to the production cannot be
    # silently missing from the resolver.
    #
    # It emits TY_* by NAME, not by number, so there is no numeric coupling
    # between this table and types.npk to drift.
    KERNEL = {"fd": "KERNEL_FD", "pid": "KERNEL_PID", "tid": "KERNEL_TID",
              "uid": "KERNEL_UID", "gid": "KERNEL_GID"}
    SIMPLE = {"bool": "TY_BOOL", "string": "TY_STRING", "cstring": "TY_CSTRING",
              "any": "TY_ANY", "NIL": "TY_NIL",
              # 1.1.12b (D-185): the owning descriptor -- concrete, scalar-shaped.
              "OwnedFd": "TY_OWNEDFD",
              # 1.3.7 (D-200/#23): the managed owning byte cell -- a bare
              # type, string's typed twin; it was mis-filed as generic.
              "buffer": "TY_BUFFER"}
    # 1.3.4 (D-197): the balanced ternary/nonary bases -- name -> (bits, base).
    # The digit count derives from the pair (base 3: 8 bits = 1 digit, 16 = 10;
    # base 9: 8 = 1, 16 = 5), so the spec's `which` slot carries the BASE.
    TERNARY = {"trit": (8, 3), "tryte": (16, 3), "nit": (8, 9), "nyte": (16, 9)}
    # Constructors and aggregates: reached through their own type nodes or their
    # generic arguments, never as a bare name.
    NOT_SCALAR = {"Result", "Optional", "Handle", "arena", "shared_arena",
                  "atomic", "Channel", "Future", "simd", "complex", "array", "buffer",
                  "Mutex", "Guard", "RwLock", "RGuard", "CondVar", "Barrier",
                  "vec2", "vec3", "vec9", "matrix", "tmatrix", "tensor",
                  "ttensor", "dyn", "func",
                  # "buffer" moved to SIMPLE at 1.3.7 -- it is a bare type.
                  # 1.3.3 (D-196): `dim256<Unit>` -- the one builtin whose
                  # argument is a UNIT NAME, not a type; resolve_type owns it.
                  "dim256"}

    bt = ['// Builtin type keywords, mapped to what they are.',
          '//',
          '// GENERATED by bootstrap/generator/gen_tables.py from',
          '// meta/specs/LEXICAL_REFERENCE.md.',
          '//',
          '// The names encode their own meaning -- `uint64` is an unsigned 64-bit',
          '// integer and says so -- so this is derived rather than hand-listed. A',
          '// type added to the BuiltinType production cannot then be silently',
          '// missing from the resolver.',
          '',
          'use "./token_kind.npk".*;',
          'use "./types.npk".*;',
          '',
          '// `known` is false for a name the type table has no kind for yet --',
          '// `frac` and the ternary/nonary bases. They are REAL types and',
          '// arrive at a later rung; the resolver names the rung rather than',
          '// pretending the name is unknown (D-085).',
          'pub struct:BuiltinTypeSpec = {',
          '    bool:known;',
          '    int32:kind;',
          '    int32:bits;',
          '    bool:signed;',
          '    int32:which;      // kernel-identifier index',
          '};',
          '',
          'func:bt_spec = BuiltinTypeSpec(bool:known, int32:kind, int32:bits,',
          '                               bool:signed, int32:which) {',
          '    BuiltinTypeSpec:s = BuiltinTypeSpec{ known: known, kind: kind,',
          '                                         bits: bits, signed: signed,',
          '                                         which: which };',
          '    pass s;',
          '};',
          '',
          'pub func:builtin_type_spec = BuiltinTypeSpec(TokenKind:k) {']
    for name in terminals(spec, "BuiltinType"):
        v = kw_variant(name)
        if name in KERNEL:
            bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(true, raw TY_KERNEL(), 32i32, false, raw %s())); }'
                      % (v, KERNEL[name]))
        elif name in SIMPLE:
            bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(true, raw %s(), 0i32, false, 0i32)); }'
                      % (v, SIMPLE[name]))
        elif name in TERNARY:
            tbits, tbase = TERNARY[name]
            bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(true, raw TY_TERN(), %di32, true, %di32)); }'
                      % (v, tbits, tbase))
        elif name in NOT_SCALAR:
            continue
        else:
            m = re.match(r'^(int|uint|tbb|tfp|flt|frac|char)(\d+)$', name)
            if m:
                fam, bits = m.group(1), m.group(2)
                kind = {"int": "TY_INT", "uint": "TY_INT", "tbb": "TY_TBB",
                        # 1.3.2 (D-195): twisted fixed point, native iN.
                        "tfp": "TY_TFP",
                        # 1.3.5 (D-198): exact rationals -- bits is the
                        # COMPONENT width of the {iN, iN, uN} triple.
                        "frac": "TY_FRAC",
                        "flt": "TY_FLOAT", "char": "TY_CHAR"}[fam]
                sgn = "true" if fam == "int" else "false"
                bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(true, raw %s(), %si32, %s, 0i32)); }'
                          % (v, kind, bits, sgn))
            else:
                # frac/trit/tryte/nit/nyte -- real types, later rung.
                bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(false, 0i32, 0i32, false, 0i32)); }'
                          % v)
    bt.append('    pass (raw bt_spec(false, 0i32, 0i32, false, 0i32));')
    bt.append('};')

    # THE GENERIC CONSTRUCTORS -- `Result<T>`, `Optional<T>` and the rest of
    # NOT_SCALAR. They are builtin type NAMES that take arguments, so they reach
    # the resolver as a named type with a generic window and match nothing in the
    # scalar table above. Before this existed they fell through to the user-name
    # lookup and `Result<int32>` -- which D-091 requires to stay writable, and
    # which the compiler's own sources use throughout -- reported "there is no
    # type named" against a token kind reinterpreted as an intern index.
    # `arena`, `Handle` and `shared_arena` have real kinds now (their `dyn`/
    # collection lowerings landed); the rest of NOT_SCALAR still arrives at a
    # later rung and is `builtin_is_generic` only.
    GENERIC = {"Result": "TY_RESULT", "Optional": "TY_OPTIONAL",
               "arena": "TY_ARENA", "Handle": "TY_HANDLE",
               "shared_arena": "TY_SHARED_ARENA",
               # 1.1.10 (D-182): `atomic<T>` is storage with an operation set,
               # `Channel<T, LEVEL, CAP>` an opaque handle. Both belong HERE
               # rather than in the generated file — an edit to that file is
               # erased by the next regeneration, which is how the atomic
               # mapping went missing between two green runs.
               "atomic": "TY_ATOMIC", "Channel": "TY_CHANNEL",
               # 1.1.11 (D-056): the sync primitives are annotation-read
               # constructors exactly as Channel is.
               "Mutex": "TY_MUTEX", "Guard": "TY_GUARD",
               "RwLock": "TY_RWLOCK", "RGuard": "TY_RGUARD",
               "CondVar": "TY_CONDVAR", "Barrier": "TY_BARRIER",
               # 1.3.1 (D-135/D-194): the vector mechanism.
               "simd": "TY_SIMD",
               # 1.3.3 (D-196): twisted fixed point with a unit vector.
               "dim256": "TY_DIM",
               # 1.3.6 (D-199): the complex numbers -- {T, T} over the four
               # ratified element types.
               "complex": "TY_COMPLEX"}
    bt.append('')
    bt.append('// A builtin type name that takes GENERIC ARGUMENTS.')
    bt.append('//')
    bt.append('// `TY_INVALID` means the name is a real constructor whose kind')
    bt.append('// arrives at a later rung -- `Handle`, `arena`, `simd`, `atomic`.')
    bt.append('// `builtin_is_generic` tells that apart from a name that is not a')
    bt.append('// builtin at all, so the resolver can name the rung instead of')
    bt.append('// claiming the type does not exist (D-085).')
    bt.append('pub func:builtin_generic_kind = int32(TokenKind:k) {')
    for name in sorted(GENERIC):
        if name in set(terminals(spec, "BuiltinType")):
            bt.append('    if (k == TokenKind.%s) { pass (raw %s()); }'
                      % (kw_variant(name), GENERIC[name]))
    bt.append('    pass 0i32;')
    bt.append('};')
    bt.append('')
    bt.append('pub func:builtin_is_generic = bool(TokenKind:k) {')
    for name in sorted(NOT_SCALAR):
        if name in set(terminals(spec, "BuiltinType")):
            bt.append('    if (k == TokenKind.%s) { pass true; }' % kw_variant(name))
    bt.append('    pass false;')
    bt.append('};')
    write("builtin_types.npk", "\n".join(bt) + "\n")

    # --- the seed's own keyword set -------------------------------------------
    # bootstrap/generator/lex.py used to carry a hand-written copy of this list,
    # and it DIVERGED THREE TIMES: `ok` and `limit` were missing (0.2.7), `Type`
    # stayed after D-088 removed it, and every builtin type name was absent, so
    # the seed accepted `int32:fd = …` where the real parser refuses it.
    #
    # Each divergence let the seed accept a program stage 1 rejects, which is the
    # dangerous direction: source written against the seed then fails at
    # self-hosting, in the stage with the worst diagnostics, long after it was
    # written. Generating both from one source removes the class.
    kw_out = ["# Keywords, GENERATED by bootstrap/generator/gen_tables.py from",
              "# meta/specs/LEXICAL_REFERENCE.md. Do not edit by hand.",
              "#",
              "# The seed reserves EVERY keyword the language does, including those",
              "# outside subset 1 -- they still lex as keywords so the parser can accept",
              "# them and the checker can refuse them by rung (D-085).",
              "",
              "KEYWORDS = {"]
    for k in sorted(kw_all):
        kw_out.append('    "%s",' % k)
    kw_out.append("}")
    with open(os.path.join(ROOT, "bootstrap", "generator", "seed_keywords.py"),
              "w", encoding="utf-8") as fh:
        fh.write("\n".join(kw_out) + "\n")
    print("seed keywords: %d" % len(kw_all))

    # --- builtins.npk ---------------------------------------------------------
    # The BARE-NAME builtins: `alloc`, `string_concat`, `sys`, `ok`, `is_err`.
    # AST_REFERENCE section 3.3 draws the line -- a `#` sigil marks something the
    # compiler treats specially, while these are ordinary calls the compiler
    # happens to provide. So they parse as an ordinary CallExpr over an
    # IdentifierExpr, and the RESOLVER has to know them: they are declared in no
    # module, and without this every `alloc` in every program resolves to nothing.
    #
    # Generated rather than hand-listed for the same reason the token table is.
    # A name added to the reference and not to the compiler is a builtin nobody
    # can call; a name here and not in the reference is one nobody can look up.
    #
    # SINCE 1.4.2 (D-201) the same rows carry the SIGNATURE, and the checker types
    # every regular builtin call from what this emits. One row per builtin, and
    # the row is the whole authority: the name list, the never-fails licence, the
    # parameter and return types, and the symbol ABI the emitter declares all come
    # out of it, so no two of them can drift apart.
    rows, rtsyms = builtin_rows(os.path.join(ROOT, "meta", "specs",
                                             "BUILTIN_REFERENCE.md"))
    names = sorted(rows)

    bl = ["// Bare-name builtins, and their signatures.",
          "//",
          "// GENERATED by bootstrap/generator/gen_tables.py from",
          "// meta/specs/BUILTIN_REFERENCE.md.",
          "//",
          "// These are ordinary calls the compiler happens to provide -- they take",
          "// arguments, obey the same rules as any function (AST_REFERENCE 3.3), and",
          "// since D-201 they are TYPED like one: a `never fails` builtin's call has",
          "// the bare value's type, a may-fail builtin's a `Result<T>`. The `#` sigil",
          "// marks the OTHER kind, the ones the compiler must treat specially.",
          "//",
          "// The resolver needs the name list because a bare-name builtin is declared",
          "// in no module. Without it every `alloc` in every program resolves to",
          "// nothing.",
          "",
          # `string_eq` lives in intern.npk. The seed's one-namespace hid the
          # missing import for six cycles; the first self-check run (0.8.0)
          # reported all forty-seven uses at once.
          'use "./intern.npk".*;',
          "",
          "pub func:is_builtin_name = bool(string:name) {"]
    for n in names:
        bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
    bl.append("};")
    # THE `fails` COLUMN (D-163, 1.1.0). A builtin is declared in no module, so
    # the `never fails` licence cannot read a declaration for it -- it reads
    # THIS, generated from the reference's own Fails column, which was filled by
    # reading each builtin's floor signature (a `{ T, i32 }` return is may-fail;
    # a plain value or void is not -- traps are not Result errors, D-150).
    nf = [n for n in names if rows[n].never_fails]
    bl.append("")
    bl.append("// Which bare-name builtins are `never fails` (D-163) -- the reference's")
    bl.append("// Fails column, generated. The licence reads this where a declared")
    bl.append("// function's contract window would be read, and since D-201 it also")
    bl.append("// decides the CALL'S TYPE: bare value here, `Result<T>` otherwise.")
    bl.append("pub func:builtin_never_fails = bool(string:name) {")
    for n in nf:
        bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
    bl.append("};")
    # THE NINE IRREGULARS (D-201.3). `sys` is variadic, `atomic_from_ptr` reads
    # its element from a turbofish, and the seven annotation-directed
    # constructors read theirs from the type the call is given -- none of which a
    # signature can state. They keep their bespoke `type_call` arms; this is what
    # tells the regular path to leave them alone, and what makes "a builtin with
    # neither a signature nor a special arm" a generation failure rather than a
    # silent UNKNOWN.
    bl.append("")
    bl.append("// The irregulars (D-201): typed by a bespoke `type_call` arm, because no")
    bl.append("// signature can state what they do -- `sys` is variadic, `atomic_from_ptr`")
    bl.append("// reads its element from a turbofish, and the seven constructors read")
    bl.append("// theirs from the annotation the call is given.")
    bl.append("pub func:builtin_sig_special = bool(string:name) {")
    for n in names:
        if rows[n].special:
            bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
    bl.append("};")
    bl.append("")
    bl.append("// How many parameters a regular builtin takes. A special's answer is 0 and")
    bl.append("// means nothing -- ask `builtin_sig_special` first.")
    bl.append("pub func:builtin_sig_count = int32(string:name) {")
    for n in names:
        if not rows[n].special and rows[n].params:
            bl.append('    if (raw string_eq(name, "%s")) { pass %di32; }'
                      % (n, len(rows[n].params)))
    bl.append("    pass 0i32;")
    bl.append("};")
    bl.append("")
    bl.append("// The i-th parameter's TYPE TEXT, for the checker's resolver to intern.")
    bl.append("// The memory qualifier rides along as documentation exactly as it does in")
    bl.append("// source -- qualifiers are not part of a type (parse_type.npk).")
    bl.append("pub func:builtin_sig_param = string(string:name, int32:i) {")
    for n in names:
        if rows[n].special or not rows[n].params:
            continue
        bl.append('    if (raw string_eq(name, "%s")) {' % n)
        for i, p in enumerate(rows[n].params):
            bl.append('        if (i == %di32) { pass "%s"; }' % (i, p.type))
        bl.append("    }")
    bl.append('    pass "";')
    bl.append("};")
    bl.append("")
    bl.append("// Which parameters CONSUME (D-183). `release_fd` is the whole list: it")
    bl.append("// takes the owner apart, so the caller may not still hold it.")
    bl.append("pub func:builtin_sig_param_move = bool(string:name, int32:i) {")
    for n in names:
        if rows[n].special:
            continue
        mv = [i for i, p in enumerate(rows[n].params) if p.move]
        if not mv:
            continue
        bl.append('    if (raw string_eq(name, "%s")) {' % n)
        for i in mv:
            bl.append('        if (i == %di32) { pass true; }' % i)
        bl.append("    }")
    bl.append("    pass false;")
    bl.append("};")
    bl.append("")
    bl.append("// The SUCCESS type's text. A may-fail builtin's call types as")
    bl.append("// `Result<this>`; a never-fails one's as this, bare (D-201 §4).")
    bl.append("pub func:builtin_sig_ret = string(string:name) {")
    for n in names:
        if rows[n].special:
            continue
        bl.append('    if (raw string_eq(name, "%s")) { pass "%s"; }'
                  % (n, rows[n].ret))
    bl.append('    pass "";')
    bl.append("};")
    # THE `#`-SIGIL BUILTINS, which are a different list and a different question.
    # A bare-name builtin is looked up because it is declared in no module; a
    # `#`-name is looked up because `#foo(...)` is ALSO how a macro is invoked
    # (D-046), so after expansion a surviving `#name` is either one of these or a
    # macro nobody declared. Without the list there is no way to tell those apart,
    # and a mistyped macro name compiles to nothing at all.
    # Scraped from the "Macro | Return | Description" table ALONE. The section
    # opens with a table of syntactic FORMS -- `#name<T>(...)` versus `#[name]` --
    # and reading that one too made `name` a builtin.
    br = open(os.path.join(ROOT, "meta", "specs", "BUILTIN_REFERENCE.md"),
              encoding="utf-8").read()
    br = "\n".join(re.findall(
        r"<!-- builtins:begin -->(.*?)<!-- builtins:end -->", br, re.S))
    hsec = br.split("| Macro | Return | Description |", 1)
    hnames = []
    if len(hsec) == 2:
        for m in re.finditer(r'^\|\s*`#(\w+)[<(`]', hsec[1].split("\n---", 1)[0], re.M):
            if m.group(1) not in hnames:
                hnames.append(m.group(1))
    hnames.sort()
    bl.append("")
    bl.append("// The `#`-sigil builtins -- the OTHER kind, the ones the compiler treats")
    bl.append("// specially. `#foo(...)` is also how a macro is invoked (D-046), so this is")
    bl.append("// what tells a builtin that survived expansion from a macro that was never")
    bl.append("// declared. `caller` is deliberately ABSENT: it is legal only inside a macro")
    bl.append("// body, expansion consumes it, and one that reaches here is one written")
    bl.append("// somewhere it means nothing.")
    bl.append("pub func:is_hash_builtin = bool(string:name) {")
    for n in hnames:
        bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
    bl.append("};")

    # DOES THIS BUILTIN HAND BACK `wild` STORAGE (D-223)? The escape analysis
    # asks, because a borrow may not be stored in a wild slot and the whitelist
    # of things that legitimately may is exactly "what the reference says
    # returns wild". Generated over BOTH tables: `builtin_sig_ret` cannot serve
    # alone, because a SPECIAL builtin is generic in `T` and carries no
    # concrete return row -- and `#ptr_add<T>` and `#wild_ptr<T>`, the two
    # pointer-returning specials, are precisely where wild pointers come from.
    hret = {}
    if len(hsec) == 2:
        for m in re.finditer(r'^\|\s*`#(\w+)[^|]*\|\s*`([^`]*)`',
                             hsec[1].split("\n---", 1)[0], re.M):
            hret[m.group(1)] = m.group(2).strip()
    wild_names = sorted(
        {n for n in names if rows[n].ret.startswith(("wild ", "wildx "))}
        | {n for n, r in hret.items() if r.startswith(("wild ", "wildx "))})
    bl.append("")
    bl.append("// Every builtin whose REFERENCE ROW declares a `wild` (or `wildx`) return —")
    bl.append("// the whitelist D-223's BORROW-011 consults. Both tables feed it: a bare")
    bl.append("// builtin's Signature column and a `#`-sigil one's Return column, because a")
    bl.append("// SPECIAL builtin is generic in `T` and has no concrete signature row, and")
    bl.append("// the two pointer-returning specials are where wild pointers come FROM.")
    bl.append("pub func:builtin_returns_wild = bool(string:name) {")
    for n in wild_names:
        bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
    bl.append("};")
    write("builtins.npk", "\n".join(bl) + "\n")
    print("builtins: %d bare (%d never-fails, %d special), %d hash"
          % (len(names), len(nf), sum(1 for n in names if rows[n].special),
             len(hnames)))

    # --- ir_runtime.npk -------------------------------------------------------
    write_runtime(rows, rtsyms)

    # --- prelude_source.npk ---------------------------------------------------
    # THE PRELUDE IS NITPICK SOURCE, and the compiler carries it as a string.
    #
    # Generated rather than hand-escaped for the reason the token table is: a
    # constant written twice is one that can differ in one of the places. And the
    # ORIGINAL stays readable — `src/prelude/prelude.npk` is an ordinary source
    # file, checked by the real parser like every other, rather than a blob nobody
    # can see inside.
    #
    # ONE LINE WITH `\n` ESCAPES, because the real lexer REFUSES a newline inside a
    # plain string literal ("newline in a string literal") while the seed's accepts
    # one. A multi-line literal here would compile through the seed and be rejected
    # by the compiler it builds, which is the divergence that bites at self-hosting.
    # The block form `"""` is the other way round: the real lexer has it and the
    # seed does not.
    pre_path = os.path.join(ROOT, "src", "prelude", "prelude.npk")
    pre = open(pre_path, encoding="utf-8").read()
    esc = pre.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    pl = ["// The prelude's source, as the compiler carries it.",
          "//",
          "// GENERATED by bootstrap/generator/gen_tables.py from",
          "// src/prelude/prelude.npk. Edit that file, not this one.",
          "//",
          "// One line with escapes, because the real lexer refuses a newline inside a",
          "// plain string literal and the seed has no block-string form -- so this is the",
          "// one spelling both accept. The text is decoded before it is lexed, so spans",
          "// inside the prelude point at real lines.",
          "",
          'pub func:PRELUDE_SOURCE = string() { pass "%s"; };' % esc]
    write("prelude_source.npk", "\n".join(pl) + "\n")
    print("prelude: %d bytes" % len(pre))

    print("token kinds: %d  keywords: %d  operators: %d  punctuation: %d  widths: %d"
          % (idx, len(kw_all), len(ops), len(punct), len(sfx)))
    return 0


NF_SIG = re.compile(r'^((?:pub )?func:\w+ = [\w\[\]<>-]+\([^)]*\))(\s*)\{', re.M)

def write(name, text):
    # EVERY GENERATED FUNCTION IS `never fails` (D-163, 1.1.1): each is a pure
    # table lookup or a constant -- no `fail`, no `relay`, no I/O -- and the
    # checker re-verifies the claim on every build. Emitted here, in one place,
    # so a regeneration can never silently strip the licence the tree relies on.
    text = NF_SIG.sub(lambda m: m.group(1) + " never fails" + m.group(2) + "{"
                      if "never fails" not in m.group(1) else m.group(0), text)
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as fh:
        fh.write(text)


if __name__ == "__main__":
    sys.exit(main())
