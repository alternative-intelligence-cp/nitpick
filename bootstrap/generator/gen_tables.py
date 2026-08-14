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
    kl = ['''use "token_kind.npk".*;
use "intern.npk".*;

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
    wl.append('use "intern.npk".*;')
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
    write("num_width.npk", '''// Numeric literal width suffixes.
//
// GENERATED by bootstrap/generator/gen_tables.py from
// meta/specs/LEXICAL_REFERENCE.md section 6.2 (TypeSuffix).
//
// A literal's width is part of its TYPE, not part of its value, which is why a
// token carries it as a separate field rather than folding it into the payload.

''' + "\n".join(wl) + "\n")

    print("token kinds: %d  keywords: %d  operators: %d  punctuation: %d  widths: %d"
          % (idx, len(kw_all), len(ops), len(punct), len(sfx)))
    return 0


def write(name, text):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as fh:
        fh.write(text)


if __name__ == "__main__":
    sys.exit(main())
