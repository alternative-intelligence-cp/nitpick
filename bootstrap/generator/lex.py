"""Subset-1 lexer for the Nitpick bootstrap seed.

THROWAWAY. This is not the compiler. It exists to compile the compiler's own
sources until stage 1 can compile them, and is deleted at self-hosting (D-085).

Spec: meta/specs/LEXICAL_REFERENCE.md, restricted to meta/SUBSET_1.md.
"""

# --- token kinds -------------------------------------------------------------

EOF = "EOF"
IDENT = "IDENT"
KEYWORD = "KEYWORD"
INT = "INT"          # value carries the suffix-stripped digits + width
FLOAT = "FLOAT"      # SUBSET_1: lexed, stored as TEXT, never evaluated
CHAR = "CHAR"
STRING = "STRING"
OP = "OP"

# Subset 1 keywords only. The full set is LEXICAL_REFERENCE.md section 4; anything
# outside subset 1 still lexes as a keyword so the PARSER can accept it and the
# CHECKER can reject it with NITPICK-RUNG-001 (D-085: the parser never restricts).
KEYWORDS = {
    # declarations
    "func", "struct", "enum", "pub", "use", "mod", "extern", "opaque",
    "trait", "impl", "Type", "assoc", "Self", "macro", "derive",
    # control flow
    "if", "else", "while", "for", "loop", "till", "when", "then", "end",
    "pick", "fall", "where", "give", "break", "continue", "return",
    "pass", "fail", "exit", "raw", "drop", "nodrop", "ok", "defaults",
    "discard", "move", "relay",
    # memory
    "wild", "wildx", "stack", "defer",
    # types
    "const", "fixed", "Rules", "limit",
    # async / meta — outside subset 1, still lexed
    "async", "await", "comptime", "inline", "noinline", "cfg", "as",
    # verification — outside subset 1, still lexed
    "prove", "assert_static", "requires", "ensures", "invariant",
    "fails", "on", "with", "never",
    # helpers
    "is", "in", "is_err",
    # literals
    "true", "false", "NIL", "NULL", "unknown",
}

# Longest-first: the lexer must try "..*" before "..", "=>!" before "=>",
# and "_^" before an identifier starting with "_" (LEXICAL_REFERENCE section 3).
OPERATORS = [
    "..*", "..^", "...", "!!!",
    "$$i", "$$m",
    "<<=", ">>=", "=>!",
    "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
    "==", "!=", "<=", ">=", "<=>", "&&", "||", "<<", ">>",
    "->", "<-", "=>", "|>", "<|", "..",
    "?.", "??", "?!", "?|", "_?", "_!", "_~", "_^",
    "::", ":",
    "+", "-", "*", "/", "%", "=", "<", ">", "!", "&", "|", "^", "~",
    "@", "$", "?", "#", "(", ")", "{", "}", "[", "]", ".", ",", ";", "`",
]
# "<=>" must beat "<=", and "::" must beat ":".
OPERATORS.sort(key=len, reverse=True)

INT_SUFFIXES = ["i8", "i16", "i32", "i64", "i128", "i256",
                "u8", "u16", "u32", "u64", "u128", "u256"]
FLOAT_SUFFIXES = ["f32", "f64", "f128", "f256", "f512"]
INT_SUFFIXES.sort(key=len, reverse=True)
FLOAT_SUFFIXES.sort(key=len, reverse=True)


import diag


class LexError(diag.NpkError):
    def __init__(self, msg, line, col, path):
        super().__init__(diag.Diag("NITPICK-LEX-001", path, line, col, msg, "lex"))


class Token:
    __slots__ = ("kind", "text", "value", "width", "line", "col", "path")

    def __init__(self, kind, text, line, col, path, value=None, width=None):
        self.kind = kind
        self.text = text
        self.value = value     # ints: the numeric value; strings/chars: decoded
        self.width = width     # ints: "i32"; floats: the suffix
        self.line = line
        self.col = col
        self.path = path

    def __repr__(self):
        return "Token(%s,%r,%d:%d)" % (self.kind, self.text, self.line, self.col)


def _is_ident_start(c):
    return c.isascii() and (c.isalpha() or c == "_")


def _is_ident_part(c):
    return c.isascii() and (c.isalnum() or c == "_")


def lex(src, path="<input>"):
    """Return a list of Tokens ending in EOF. Raises LexError on bad input."""
    toks = []
    i, n = 0, len(src)
    line, bol = 1, 0

    def col(at):
        return at - bol + 1

    while i < n:
        c = src[i]

        # whitespace
        if c in " \t\r":
            i += 1
            continue
        if c == "\n":
            i += 1
            line += 1
            bol = i
            continue

        # comments. Block comments do NOT nest (LEXICAL_REFERENCE section 2).
        if src.startswith("//", i):
            while i < n and src[i] != "\n":
                i += 1
            continue
        if src.startswith("/*", i):
            start_line, start_col = line, col(i)
            i += 2
            while i < n and not src.startswith("*/", i):
                if src[i] == "\n":
                    line += 1
                    bol = i + 1
                i += 1
            if i >= n:
                raise LexError("unterminated block comment", start_line, start_col, path)
            i += 2
            continue

        start, scol = i, col(i)

        # string literal
        if c == '"':
            i += 1
            buf = []
            while i < n and src[i] != '"':
                if src[i] == "\\":
                    if i + 1 >= n:
                        raise LexError("unterminated escape", line, scol, path)
                    esc = src[i + 1]
                    buf.append({"n": "\n", "t": "\t", "r": "\r", "0": "\0",
                                "\\": "\\", '"': '"', "'": "'"}.get(esc, esc))
                    i += 2
                    continue
                if src[i] == "\n":
                    raise LexError("newline in string literal", line, scol, path)
                buf.append(src[i])
                i += 1
            if i >= n:
                raise LexError("unterminated string literal", line, scol, path)
            i += 1
            toks.append(Token(STRING, src[start:i], line, scol, path, value="".join(buf)))
            continue

        # char literal
        if c == "'":
            i += 1
            if i < n and src[i] == "\\":
                esc = src[i + 1] if i + 1 < n else ""
                val = {"n": "\n", "t": "\t", "r": "\r", "0": "\0",
                       "\\": "\\", '"': '"', "'": "'"}.get(esc, esc)
                i += 2
            else:
                val = src[i] if i < n else ""
                i += 1
            if i >= n or src[i] != "'":
                raise LexError("unterminated character literal", line, scol, path)
            i += 1
            toks.append(Token(CHAR, src[start:i], line, scol, path, value=val))
            continue

        # numeric literal
        if c.isdigit():
            j = i
            radix = 10
            if src.startswith("0x", j) or src.startswith("0X", j):
                radix, j = 16, j + 2
                while j < n and (src[j].isalnum() or src[j] == "_"):
                    j += 1
            elif src.startswith("0b", j) or src.startswith("0B", j):
                radix, j = 2, j + 2
                while j < n and (src[j].isalnum() or src[j] == "_"):
                    j += 1
            else:
                while j < n and (src[j].isdigit() or src[j] == "_"):
                    j += 1
                # A float only if the dot is followed by a digit: "0...4" is a
                # range, not a malformed float.
                if j < n and src[j] == "." and j + 1 < n and src[j + 1].isdigit():
                    j += 1
                    while j < n and (src[j].isdigit() or src[j] == "_"):
                        j += 1
                    while j < n and _is_ident_part(src[j]):
                        j += 1
                    text = src[i:j]
                    suf = next((s for s in FLOAT_SUFFIXES if text.endswith(s)), None)
                    if suf is None:
                        raise LexError("float literal needs a width suffix, e.g. 1.5f64",
                                       line, scol, path)
                    # SUBSET_1: stored as TEXT. The seed never evaluates a float.
                    toks.append(Token(FLOAT, text, line, scol, path,
                                      value=text[:-len(suf)], width=suf))
                    i = j
                    continue
                while j < n and _is_ident_part(src[j]):
                    j += 1

            text = src[i:j]
            suf = next((s for s in INT_SUFFIXES if text.endswith(s)), None)
            # An unsuffixed literal is lexed with width=None. Whether that is
            # legal depends on POSITION, which only the parser knows: subset 1
            # requires a suffix in expression position, but an array size in
            # TYPE position is a bare count -- `int32[4]`, as TYPE_REFERENCE 9.2
            # writes it. Keeping the lexer positionless is why this is not an
            # error here.
            digits = (text[:-len(suf)] if suf else text).replace("_", "")
            if radix == 16:
                val = int(digits[2:] or "0", 16)
            elif radix == 2:
                val = int(digits[2:] or "0", 2)
            else:
                val = int(digits or "0", 10)
            toks.append(Token(INT, text, line, scol, path, value=val, width=suf))
            i = j
            continue

        # operators BEFORE identifiers: "_?", "_!", "_~", "_^" are operators even
        # though they begin with an underscore (LEXICAL_REFERENCE section 3).
        op = next((o for o in OPERATORS if src.startswith(o, i)), None)
        if op is not None and (op[0] != "_" or not (i + len(op) < n and _is_ident_part(src[i + len(op)]))):
            toks.append(Token(OP, op, line, scol, path))
            i += len(op)
            continue

        # identifier or keyword
        if _is_ident_start(c):
            j = i
            while j < n and _is_ident_part(src[j]):
                j += 1
            text = src[i:j]
            toks.append(Token(KEYWORD if text in KEYWORDS else IDENT,
                              text, line, scol, path))
            i = j
            continue

        if op is not None:
            toks.append(Token(OP, op, line, scol, path))
            i += len(op)
            continue

        raise LexError("unexpected character %r" % c, line, scol, path)

    toks.append(Token(EOF, "", line, col(i), path))
    return toks
