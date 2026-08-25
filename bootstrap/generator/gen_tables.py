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
        if m and m.group(1) in ("u", "i", "f", "tbb", "frac", "tfp", "char"):
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
              (r'^char\d+$', "TY_CHAR")]
    wl.append("")
    wl.append("// The TYPE a suffix names, as a `TY_*` kind.")
    wl.append("//")
    wl.append("// TY_INVALID for a suffix whose type has no kind yet -- `frac`, `tfp`")
    wl.append("// and `dim256` are REAL types arriving at a later rung, and the caller")
    wl.append("// names the rung rather than pretending the suffix is absent (D-085).")
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
              "any": "TY_ANY", "NIL": "TY_NIL"}
    # Constructors and aggregates: reached through their own type nodes or their
    # generic arguments, never as a bare name.
    NOT_SCALAR = {"Result", "Optional", "Handle", "arena", "shared_arena",
                  "atomic", "Future", "simd", "complex", "array", "buffer",
                  "vec2", "vec3", "vec9", "matrix", "tmatrix", "tensor",
                  "ttensor", "dyn", "func"}

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
          '// `frac`, `tfp`, `dim256` and the exotic bases. They are REAL types and',
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
        elif name in NOT_SCALAR:
            continue
        else:
            m = re.match(r'^(int|uint|tbb|flt|char)(\d+)$', name)
            if m:
                fam, bits = m.group(1), m.group(2)
                kind = {"int": "TY_INT", "uint": "TY_INT", "tbb": "TY_TBB",
                        "flt": "TY_FLOAT", "char": "TY_CHAR"}[fam]
                sgn = "true" if fam == "int" else "false"
                bt.append('    if (k == TokenKind.%s) { pass (raw bt_spec(true, raw %s(), %si32, %s, 0i32)); }'
                          % (v, kind, bits, sgn))
            else:
                # frac/tfp/dim256/trit/tryte/nit/nyte -- real types, later rung.
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
               "shared_arena": "TY_SHARED_ARENA"}
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
    br = open(os.path.join(ROOT, "meta", "specs", "BUILTIN_REFERENCE.md"),
              encoding="utf-8").read()
    # ONLY the marked regions define builtins (0.8.4). The first draft
    # scavenged every code-shaped token in the whole file, so `close(2)` in a
    # sentence about POSIX became a "builtin", and the entire nlibc API rode in
    # as compiler magic nobody could lower.
    br = "\n".join(re.findall(
        r"<!-- builtins:begin -->(.*?)<!-- builtins:end -->", br, re.S))
    names = []
    for m in re.finditer(r'^\|\s*`(\w+)`\s*\|', br, re.M):
        if m.group(1) not in names:
            names.append(m.group(1))
    for m in re.finditer(r'^\*\s+`(\w+)\(', br, re.M):
        if m.group(1) not in names:
            names.append(m.group(1))
    for m in re.finditer(r'`(\w+)\([^`]*\)`', br):
        if m.group(1) not in names:
            names.append(m.group(1))
    # `#`-prefixed forms are BuiltinExpr and never reach name resolution.
    names = [n for n in names if not n.startswith("_")]
    names.sort()

    bl = ["// Bare-name builtins.",
          "//",
          "// GENERATED by bootstrap/generator/gen_tables.py from",
          "// meta/specs/BUILTIN_REFERENCE.md.",
          "//",
          "// These are ordinary calls the compiler happens to provide -- they take",
          "// arguments, return Result<T>, and obey the same rules as any function",
          "// (AST_REFERENCE 3.3). The `#` sigil marks the OTHER kind, the ones the",
          "// compiler must treat specially.",
          "//",
          "// The resolver needs this list because a bare-name builtin is declared in no",
          "// module. Without it every `alloc` in every program resolves to nothing.",
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
    nf = []
    for m in re.finditer(r'^\|\s*`(\w+)[`(][^\n]*\|\s*([^|]*)\|\s*$', br, re.M):
        if "never fails" in m.group(2) and m.group(1) not in nf:
            nf.append(m.group(1))
    nf.sort()
    missing = [n for n in names if n not in nf
               and not re.search(r'^\|\s*`%s[`(][^\n]*may fail' % n, br, re.M)]
    if missing:
        raise SystemExit("BUILTIN_REFERENCE.md Fails column missing for: %s"
                         % ", ".join(missing))
    bl.append("")
    bl.append("// Which bare-name builtins are `never fails` (D-163) -- the reference's")
    bl.append("// Fails column, generated. The licence reads this where a declared")
    bl.append("// function's contract window would be read.")
    bl.append("pub func:builtin_never_fails = bool(string:name) {")
    for n in nf:
        bl.append('    if (raw string_eq(name, "%s")) { pass true; }' % n)
    bl.append("    pass false;")
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
    write("builtins.npk", "\n".join(bl) + "\n")
    print("builtins: %d bare, %d hash" % (len(names), len(hnames)))

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
