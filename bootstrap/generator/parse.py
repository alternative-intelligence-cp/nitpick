"""Recursive-descent parser for the Nitpick bootstrap seed.

THROWAWAY (D-085).

The parser accepts the WHOLE grammar it is given, including constructs outside
subset 1 -- generics, traits, async, comptime, floats, counted loops. Those are
rejected later by the checker with NITPICK-RUNG-001.

    The parser never restricts. The backend does.  (D-085)

That is not a stylistic choice. A partial grammar that gets re-widened rung by
rung is what ended nitpick-bootstrap, and tests/rejection/ exists to keep this
parser honest.

Precedence follows OP_REFERENCE.md section 0, including level 2 -- the Result
unary operators, which bind looser than postfix and tighter than cast (D-081).
"""

import lex
import diag
from lex import EOF, IDENT, KEYWORD, INT, FLOAT, CHAR, STRING, OP
import syntax as S


class ParseError(diag.NpkError):
    def __init__(self, msg, tok):
        super().__init__(diag.Diag("NITPICK-PARSE-001", tok.path, tok.line,
                                   tok.col, msg, "parse"))
        self.tok = tok


# Binary precedence, LOOSEST first. Levels are OP_REFERENCE section 0 with the
# Result-unary level (2) handled separately as a prefix, and assignment handled
# as a STATEMENT (D-060) rather than an operator.
#
# LOOSEST FIRST BECAUSE OF HOW `parse_binary` RECURSES: level N parses its
# operands at level N + 1, so the first row is the OUTERMOST -- the one that
# binds least tightly. Until 1.0.8 this list was written tightest-first under a
# comment saying so, which made `*` the loosest operator and `||` the tightest:
# `i + k <= n` compiled as `i + (k <= n)`, and a byte scan in a test ran past
# its buffer into a SIGBUS. Nothing in `src/` ever depended on it -- every
# expression there is parenthesised to one operator, and the stage-1/stage-2
# fixpoint (the real parser's precedence against the seed's) has been
# byte-identical throughout, which is the proof -- but every seed-compiled
# TEST written from now on would have had to know. The seed is a throwaway
# generator (D-085), so this is a regeneration, not a change to any artifact.
BINARY_LEVELS = [
    ["||"],
    ["&&"],
    ["|"],
    ["^"],
    ["&"],
    ["==", "!="],
    ["<", "<=", ">", ">=", "<=>"],
    ["<<", ">>"],
    ["+", "-"],
    ["*", "/", "%"],
]

ASSIGN_OPS = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}
RESULT_UNARY = {"raw", "drop", "relay", "await"}
MEMORY_QUALS = {"wild", "wildx", "stack", "fixed", "const", "nodrop"}
VISIBILITY = {"pub"}
FUNC_MODIFIERS = {"async", "comptime", "inline", "noinline"}


class Parser:
    def __init__(self, toks):
        self.toks = toks
        self.i = 0

    # --- token helpers -------------------------------------------------------

    def peek(self, k=0):
        j = min(self.i + k, len(self.toks) - 1)
        return self.toks[j]

    def at(self, text, kind=None):
        t = self.peek()
        return t.text == text and (kind is None or t.kind == kind)

    def at_kind(self, kind):
        return self.peek().kind == kind

    def next(self):
        t = self.toks[self.i]
        if t.kind != EOF:
            self.i += 1
        return t

    def accept(self, text):
        if self.at(text):
            return self.next()
        return None

    def expect(self, text):
        if not self.at(text):
            raise ParseError("expected %r, found %r" % (text, self.peek().text), self.peek())
        return self.next()

    def expect_ident(self):
        """A BINDING name. A reserved word is not one.

        This used to accept any KEYWORD token, which made the seed more
        permissive than the real parser in the dangerous direction: `int32:ok`
        and `int32:limit` compiled here and are refused there, so source written
        against the seed would have failed at self-hosting, in a stage with far
        worse diagnostics. The real parser found two such files the first time it
        was pointed at the suites (0.2.7).

        The seed must never accept what stage 1 rejects.
        """
        t = self.peek()
        if t.kind != IDENT:
            raise ParseError("expected an identifier, found %r" % t.text, t)
        return self.next()

    def expect_type_name(self):
        """A TYPE name, where a keyword is legitimate.

        `int32`, `string`, `NIL`, `Result`, `Self` all lex as keywords and are
        all real type names -- which is why this is a separate function rather
        than a loosening of the one above. The two positions want different
        things, and merging them cost the seed its agreement with stage 1.
        """
        t = self.peek()
        if t.kind not in (IDENT, KEYWORD):
            raise ParseError("expected a type name, found %r" % t.text, t)
        return self.next()

    # --- module --------------------------------------------------------------

    def parse_module(self, path):
        items = []
        while not self.at_kind(EOF):
            items.append(self.parse_item())
        return S.Module(items, path)

    def parse_item(self):
        start = self.peek()
        vis = "private"
        if self.at("pub"):
            self.next()
            vis = "pub"

        if self.at("use"):
            return self.parse_import(start)

        if self.at("mod"):
            return self.parse_mod(start)

        mods = set()
        while self.peek().text in FUNC_MODIFIERS:
            mods.add(self.next().text)

        if self.at("func"):
            return self.parse_func(start, vis, mods)
        if self.at("struct"):
            return self.parse_struct(start, vis)
        if self.at("enum"):
            return self.parse_enum(start, vis)
        if self.at("trait"):
            return self.parse_trait(start, vis)
        if self.at("impl"):
            return self.parse_impl(start)
        if self.peek().text in MEMORY_QUALS or self.at_kind(IDENT) or self.at_kind(KEYWORD):
            return self.parse_global(start, vis)

        raise ParseError("expected a declaration, found %r" % self.peek().text, self.peek())

    def parse_mod(self, start):
        # `mod:name;` names the file (the real loader checks it; the seed does
        # not). `mod:name = { items };` is an INLINE module (0.9.6) -- parsed
        # here so the checker can refuse it, never the parser (D-085): the
        # rejection suite compiles under both compilers, and the seed builds
        # nothing that uses one.
        self.expect("mod")
        self.expect(":")
        name = self.expect_ident().text
        items = []
        inline = False
        if self.accept("="):
            inline = True
            self.expect("{")
            while not self.at("}"):
                items.append(self.parse_item())
            self.expect("}")
        self.expect(";")
        return S.ModDecl(name, items, inline)._at(start)

    def parse_import(self, start):
        self.expect("use")
        t = self.peek()
        if t.kind == STRING:
            target = self.next().value
        else:
            parts = [self.expect_ident().text]
            while self.accept("."):
                if self.at("*"):
                    break
                parts.append(self.expect_ident().text)
            target = ".".join(parts)
        kind = "wildcard"
        if self.accept("."):
            if self.accept("*"):
                kind = "wildcard"
            elif self.at("{"):
                self.next()
                while not self.at("}"):
                    self.expect_ident()
                    if not self.accept(","):
                        break
                self.expect("}")
                kind = "selective"
            else:
                self.expect_ident()
                kind = "single"
        self.expect(";")
        return S.ImportDecl(target, kind)._at(start)

    # --- declarations --------------------------------------------------------

    def parse_generics(self):
        """<T>, <T: A & B>, <T, comptime int32:LEVEL> -- accepted, rejected later."""
        if not self.at("<"):
            return []
        self.next()
        params = []
        while not self.at(">"):
            if self.at("comptime"):
                self.next()
                ty = self.parse_type()
                self.expect(":")
                params.append(("value", self.expect_ident().text, ty))
            else:
                name = self.expect_ident().text
                bounds = []
                if self.accept(":"):
                    bounds.append(self.parse_type())
                    while self.accept("&"):
                        bounds.append(self.parse_type())
                params.append(("type", name, bounds))
            if not self.accept(","):
                break
        self.expect(">")
        return params

    def parse_func(self, start, vis, mods):
        self.expect("func")
        self.expect(":")
        name = self.expect_ident().text
        generics = self.parse_generics()
        self.expect("=")
        ret = self.parse_type()
        self.expect("(")
        params = []
        while not self.at(")"):
            # `limit<Rule>` on a parameter -- parsed and carried; the checker
            # refuses it naming 1.3 (0.9.0).
            p_limit = self.accept("limit") is not None
            if p_limit:
                self.expect("<")
                self.expect_ident()
                self.expect(">")
            ptype = self.parse_type()
            self.expect(":")
            # `Type:_~name` -- the declaration-site discard (D-089). The seed
            # records it and does nothing with it: enforcing "a discarded
            # parameter may not be read" is the compiler's job, and the seed
            # lowers rather than checks (SUBSET_1 section 2).
            discarded = self.accept("_~") is not None
            pname = self.expect_ident().text
            prm = S.ParamDecl(ptype, pname, discarded)
            prm.limit_marker = p_limit
            params.append(prm)
            if not self.accept(","):
                break
        self.expect(")")
        # Contracts -- `requires` / `ensures` / `acquires` -- PARSE here and are
        # refused by the checker, never by the parser (D-085; 0.9.0 added the
        # carriers to the rejection suite). The clause expression is consumed
        # and dropped: the seed carries only the fact, the way it carries
        # `async` -- the real frontend is what reads contracts.
        has_contract = False
        while True:
            if self.accept("requires") or self.accept("ensures"):
                self.parse_expr()
                has_contract = True
                continue
            if self.accept("acquires"):
                self.accept("<=")
                self.parse_expr()
                has_contract = True
                continue
            # `never fails` (D-163) -- parsed and carried as a fact, WITHOUT
            # has_contract: the checker rung-refuses the verification contracts
            # as 1.3 work, and this one changes no lowering. The real frontend
            # is what enforces it.
            if self.accept("never"):
                self.expect("fails")
                continue
            break
        body = None
        if self.at("{"):
            body = self.parse_block()
        self.expect(";")
        node = S.FuncDecl(name, vis, mods, generics, params, ret, body)._at(start)
        node.has_contract = has_contract
        return node

    def parse_struct(self, start, vis):
        self.expect("struct")
        self.expect(":")
        name = self.expect_ident().text
        generics = self.parse_generics()
        self.expect("=")
        self.expect("{")
        fields = []
        while not self.at("}"):
            ftype = self.parse_type()
            self.expect(":")
            fname = self.expect_ident().text
            fields.append(S.FieldDecl(ftype, fname))
            self.expect(";")
        self.expect("}")
        self.expect(";")
        return S.StructDecl(name, vis, generics, fields)._at(start)

    def parse_enum(self, start, vis):
        self.expect("enum")
        self.expect(":")
        name = self.expect_ident().text
        generics = self.parse_generics()
        self.expect("=")
        self.expect("{")
        variants = []
        while not self.at("}"):
            vname = self.expect_ident().text
            payload, value = None, None
            if self.accept("("):
                payload = [self.parse_type()]
                while self.accept(","):
                    payload.append(self.parse_type())
                self.expect(")")
            elif self.accept("="):
                value = self.parse_expr()
            variants.append(S.EnumVariant(vname, payload, value))
            self.expect(";")
        self.expect("}")
        self.expect(";")
        return S.EnumDecl(name, vis, generics, variants)._at(start)

    def parse_trait(self, start, vis):
        self.expect("trait")
        self.expect(":")
        name = self.expect_ident().text
        self.expect("=")
        self.expect("{")
        items = []
        while not self.at("}"):
            items.append(self.parse_item())
        self.expect("}")
        self.expect(";")
        return S.TraitDecl(name, vis, items)._at(start)

    def parse_impl(self, start):
        self.expect("impl")
        self.expect(":")
        # SLOT 1 IS THE TYPE, with or without a trait (D-031). This used to read
        # the first segment as the trait and accept `impl:Trait:for:Type`, which
        # is FORMAL_DRAFT 13's superseded form -- the connector went entirely
        # because `for` already means "iterate over", and the type went first so
        # that `impl:Point` and `impl:Message:Serializable` put the same thing in
        # the same place.
        #
        # Nothing here compiles an `impl` -- traits are outside subset 1 and the
        # checker refuses them -- but a grammar that states the old syntax is a
        # wrong claim about the language sitting in the tree.
        type_name = self.expect_type_name().text
        trait_name = None
        if self.accept(":"):
            trait_name = self.expect_type_name().text
        self.expect("=")
        self.expect("{")
        items = []
        while not self.at("}"):
            items.append(self.parse_item())
        self.expect("}")
        self.expect(";")
        return S.ImplDecl(trait_name, type_name, items)._at(start)

    def parse_global(self, start, vis):
        quals = []
        while self.peek().text in MEMORY_QUALS:
            quals.append(self.next().text)
        ty = self.parse_type()
        self.expect(":")
        name = self.expect_ident().text
        init = None
        if self.accept("="):
            init = self.parse_expr()
        self.expect(";")
        return S.GlobalDecl(name, vis, quals, ty, init)._at(start)

    # --- types ---------------------------------------------------------------

    def parse_type(self):
        quals = []
        while self.peek().text in MEMORY_QUALS:
            quals.append(self.next().text)

        t = self.expect_type_name()
        generic_args = []
        if self.at("<"):
            # A type-argument list. Subset 1 has no user generics, but Result<T>
            # is builtin, so this always parses.
            self.next()
            while not self.at(">"):
                generic_args.append(self.parse_type())
                if not self.accept(","):
                    break
            self.expect(">")
        ty = S.NamedType(t.text, generic_args)._at(t)

        while True:
            if self.at("->"):
                self.next()
                ty = S.PointerType(ty)._at(t)
            elif self.at("["):
                self.next()
                if self.at("]"):
                    self.next()
                    ty = S.SliceType(ty)._at(t)
                else:
                    # An array size is a count in TYPE position, written bare:
                    # `int32[4]` (TYPE_REFERENCE 9.2). The expression-position
                    # suffix rule does not apply here.
                    if self.at_kind(INT):
                        n = self.next()
                        size = S.IntLit(n.value, n.width)._at(n)
                    else:
                        size = self.parse_expr()
                    self.expect("]")
                    ty = S.ArrayType(ty, size)._at(t)
            else:
                break

        for q in reversed(quals):
            ty = S.QualType(q, ty)._at(t)
        return ty

    # --- statements ----------------------------------------------------------

    def parse_block(self):
        start = self.expect("{")
        stmts = []
        while not self.at("}"):
            stmts.append(self.parse_stmt())
        self.expect("}")
        return S.Block(stmts)._at(start)

    def _looks_like_decl(self):
        """Distinguish `int32:x = ...;` from an expression statement.

        A declaration is <quals> <type> ':' <ident>. Scanning for the ':' that
        follows a type is enough: an expression statement never has one at depth
        zero before its terminating ';'.
        """
        j = self.i
        toks = self.toks
        if toks[j].text in MEMORY_QUALS:
            return True
        if toks[j].kind not in (IDENT, KEYWORD):
            return False
        depth = 0
        while j < len(toks) and toks[j].kind != EOF:
            tx = toks[j].text
            if tx in ("(", "[", "{"):
                depth += 1
            elif tx in (")", "]", "}"):
                if depth == 0:
                    return False
                depth -= 1
            elif tx == "<" and depth == 0:
                depth += 1          # type-argument list
            elif tx == ">" and depth > 0:
                depth -= 1
            elif depth == 0 and tx == ":":
                return True
            elif depth == 0 and tx in (";", "=", ".", ",") or tx in ASSIGN_OPS:
                return False
            j += 1
        return False

    def parse_stmt(self):
        t = self.peek()
        tx = t.text

        if tx == "{":
            return self.parse_block()
        if tx == "if":
            return self.parse_if()
        if tx == "while":
            return self.parse_while(None)
        if tx in ("for", "loop", "till"):
            return self.parse_counted(None)
        if tx == "pick":
            return self.parse_pick()
        if tx == "defer":
            self.next()
            return S.Defer(self.parse_block())._at(t)
        if tx == "pass":
            self.next()
            val = None if self.at(";") else self.parse_expr()
            self.expect(";")
            return S.Pass(val)._at(t)
        if tx == "fail":
            self.next()
            e = self.parse_expr()
            self.expect(";")
            return S.Fail(e)._at(t)
        if tx == "return":
            self.next()
            val = None if self.at(";") else self.parse_expr()
            self.expect(";")
            return S.Return(val)._at(t)
        if tx == "exit":
            self.next()
            e = self.parse_expr()
            self.expect(";")
            return S.Exit(e)._at(t)
        if tx == "!!!":
            self.next()
            e = self.parse_expr()
            self.expect(";")
            return S.Trap(e)._at(t)
        if tx == "break":
            self.next()
            lbl = self.expect_ident().text if self.at_kind(IDENT) else None
            self.expect(";")
            return S.Break(lbl)._at(t)
        if tx == "continue":
            self.next()
            lbl = self.expect_ident().text if self.at_kind(IDENT) else None
            self.expect(";")
            return S.Continue(lbl)._at(t)
        if tx == "fall":
            self.next()
            lbl = self.expect_ident().text
            self.expect(";")
            return S.Fall(lbl)._at(t)
        if tx == "discard":
            self.next()
            self.expect("(")
            e = self.parse_expr()
            self.expect(")")
            self.expect(";")
            return S.Discard(e)._at(t)

        # labelled loop:  outer: while (...) { ... }
        if t.kind == IDENT and self.peek(1).text == ":" and \
                self.peek(2).text in ("while", "for", "loop", "till"):
            label = self.next().text
            self.next()
            if self.at("while"):
                return self.parse_while(label)
            return self.parse_counted(label)

        if self._looks_like_decl():
            return self.parse_vardecl()

        expr = self.parse_expr()
        if self.peek().text in ASSIGN_OPS:
            op = self.next().text
            val = self.parse_expr()
            self.expect(";")
            # Assignment is a STATEMENT, not an expression (D-060).
            return S.Assign(expr, op, val)._at(t)
        self.expect(";")
        return S.ExprStmt(expr)._at(t)

    def parse_vardecl(self):
        t = self.peek()
        quals = []
        while self.peek().text in MEMORY_QUALS:
            quals.append(self.next().text)
        # `limit<Rule>` binds to the declaration, with the qualifiers (0.9.0:
        # parsed, carried, refused by the checker naming 1.3).
        v_limit = self.accept("limit") is not None
        if v_limit:
            self.expect("<")
            self.expect_ident()
            self.expect(">")
        ty = self.parse_type()
        self.expect(":")
        name = self.expect_ident().text
        init = None
        if self.accept("="):
            init = self.parse_expr()
        self.expect(";")
        node = S.VarDecl(quals, ty, name, init)._at(t)
        node.limit_marker = v_limit
        return node

    def parse_if(self):
        t = self.expect("if")
        self.expect("(")
        cond = self.parse_expr()
        self.expect(")")
        then_block = self.parse_block()
        else_branch = None
        if self.accept("else"):
            else_branch = self.parse_if() if self.at("if") else self.parse_block()
        return S.If(cond, then_block, else_branch)._at(t)

    def parse_while(self, label):
        t = self.expect("while")
        self.expect("(")
        cond = self.parse_expr()
        self.expect(")")
        # `invariant e1, e2` between the head and the body -- parsed, carried,
        # refused by the checker naming 1.3 (0.9.0).
        inv = self.accept("invariant") is not None
        if inv:
            self.parse_expr()
            while self.accept(","):
                self.parse_expr()
        node = S.While(label, cond, self.parse_block())._at(t)
        node.invariant_marker = inv
        return node

    def parse_counted(self, label):
        """for / loop / till -- outside subset 1, parsed so the checker rejects."""
        t = self.next()
        kind = t.text
        binding, iterable, args = None, None, []
        self.expect("(")
        if kind == "for":
            binding_type = self.parse_type()
            self.expect(":")
            bname = self.expect_ident().text
            binding = S.ParamDecl(binding_type, bname, False)
            self.expect("in")
            iterable = self.parse_expr()
        else:
            while not self.at(")"):
                args.append(self.parse_expr())
                if not self.accept(","):
                    break
        self.expect(")")
        body = self.parse_block()
        if kind == "for":
            return S.For(label, binding, iterable, body)._at(t)
        if kind == "loop":
            return S.Loop(label, args, body)._at(t)
        return S.Till(label, args, body)._at(t)

    def parse_pick(self):
        t = self.expect("pick")
        self.expect("(")
        sel = self.parse_expr()
        self.expect(")")
        self.expect("{")
        arms = []
        while not self.at("}"):
            label = None
            if self.peek().kind == IDENT and self.peek(1).text == ":":
                label = self.next().text
                self.next()
            self.expect("(")
            pat = self.parse_pattern()
            self.expect(")")
            arms.append(S.PickArm(label, pat, self.parse_block()))
            if not self.accept(","):
                break
        self.expect("}")
        return S.Pick(sel, arms)._at(t)

    def parse_pattern(self):
        t = self.peek()
        if self.at("*"):
            self.next()
            return S.WildcardPat()._at(t)
        if t.kind in (IDENT, KEYWORD) and self.peek(1).text in (".", "{", "("):
            parts = [self.expect_ident().text]
            while self.accept("."):
                parts.append(self.expect_ident().text)
            if self.accept("("):
                binds = []
                while not self.at(")"):
                    binds.append(self.expect_ident().text)
                    if not self.accept(","):
                        break
                self.expect(")")
                return S.VariantPat(parts, binds)._at(t)
            if self.accept("{"):
                fields = []
                while not self.at("}"):
                    fields.append(self.expect_ident().text)
                    if not self.accept(","):
                        break
                self.expect("}")
                return S.StructPat(".".join(parts), fields)._at(t)
            return S.VariantPat(parts, [])._at(t)
        return S.LiteralPat(self.parse_expr())._at(t)

    # --- expressions ---------------------------------------------------------

    def parse_expr(self):
        return self.parse_ternary()

    def parse_ternary(self):
        t = self.peek()
        if self.at("is") and self.peek(1).text == "(":
            self.next()
            self.expect("(")
            cond = self.parse_expr()
            self.expect(")")
            self.expect(":")
            a = self.parse_expr()
            self.expect(":")
            b = self.parse_expr()
            return S.Ternary(cond, a, b)._at(t)
        return self.parse_defaults()

    def parse_defaults(self):
        e = self.parse_coalesce()
        while self.peek().text in ("?|", "defaults"):
            t = self.next()
            e = S.SafeUnwrap(e, self.parse_coalesce())._at(t)
        return e

    def parse_coalesce(self):
        e = self.parse_binary(0)
        while self.peek().text in ("??", "?", "?!"):
            t = self.next()
            rhs = self.parse_binary(0)
            e = (S.Emphatic(e, rhs) if t.text == "?!" else S.SafeUnwrap(e, rhs))._at(t)
        return e

    def parse_binary(self, level):
        if level >= len(BINARY_LEVELS):
            return self.parse_range()
        e = self.parse_binary(level + 1)
        while self.peek().kind == OP and self.peek().text in BINARY_LEVELS[level]:
            t = self.next()
            e = S.Binary(t.text, e, self.parse_binary(level + 1))._at(t)
        return e

    def parse_range(self):
        e = self.parse_unary()
        if self.peek().text in ("..", "..."):
            t = self.next()
            return S.Range(e, self.parse_unary(), t.text == "...")._at(t)
        return e

    def parse_unary(self):
        t = self.peek()
        if t.kind == OP and t.text in ("!", "~", "-", "@", "<-", "$$i", "$$m", "++", "--"):
            self.next()
            return S.Unary(t.text, self.parse_unary())._at(t)
        return self.parse_cast()

    def parse_cast(self):
        e = self.parse_result_unary()
        while self.peek().text in ("=>", "=>!"):
            t = self.next()
            e = S.Cast(e, self.parse_type(), t.text == "=>!")._at(t)
        return e

    def parse_result_unary(self):
        """Precedence level 2 (D-081).

        Looser than postfix -- `raw a.eq(b)` takes the whole call.
        Tighter than cast and pipeline -- it is the VALUE that gets cast or
        piped, a Result being meaningless to either.
        """
        t = self.peek()
        if t.kind == KEYWORD and t.text in RESULT_UNARY:
            self.next()
            return S.ResultUnary(t.text, self.parse_result_unary())._at(t)
        if t.kind == OP and t.text in ("_!", "_?", "_^"):
            self.next()
            op = {"_!": "raw", "_?": "drop", "_^": "relay"}[t.text]
            return S.ResultUnary(op, self.parse_result_unary())._at(t)
        if t.kind == OP and t.text == "_~":
            self.next()
            return S.Discard(self.parse_result_unary())._at(t)
        return self.parse_pipeline()

    def parse_pipeline(self):
        e = self.parse_postfix()
        while self.peek().text in ("|>", "<|"):
            t = self.next()
            e = S.Pipeline(t.text, e, self.parse_postfix())._at(t)
        return e

    def parse_postfix(self):
        e = self.parse_primary()
        while True:
            t = self.peek()
            if t.text == ".":
                self.next()
                e = S.Field(e, self.expect_ident().text)._at(t)
            elif t.text == "?.":
                self.next()
                e = S.Field(e, self.expect_ident().text)._at(t)
            elif t.text == "[":
                self.next()
                idx = self.parse_expr()
                self.expect("]")
                e = S.Index(e, idx)._at(t)
            elif t.text == "(":
                self.next()
                args = []
                while not self.at(")"):
                    args.append(self.parse_expr())
                    if not self.accept(","):
                        break
                self.expect(")")
                e = S.Call(e, [], args)._at(t)
            elif t.text == "::" and self.peek(1).text == "<":
                # turbofish -- the only expression-position form (D-064)
                self.next()
                self.next()
                gargs = []
                while not self.at(">"):
                    gargs.append(self.parse_type())
                    if not self.accept(","):
                        break
                self.expect(">")
                self.expect("(")
                args = []
                while not self.at(")"):
                    args.append(self.parse_expr())
                    if not self.accept(","):
                        break
                self.expect(")")
                e = S.Call(e, gargs, args)._at(t)
            elif t.text in ("++", "--"):
                self.next()
                e = S.Unary("post" + t.text, e)._at(t)
            else:
                break
        return e

    def parse_primary(self):
        t = self.peek()

        if t.kind == INT:
            self.next()
            if t.width is None:
                # --extra-picky literal-suffixes, applied unconditionally to our
                # own sources: every integer literal in EXPRESSION position
                # carries an explicit width, so sizing is never inferred in the
                # code that can least afford ambiguity. Array sizes in type
                # position are exempt -- see parse_type.
                raise ParseError("integer literal needs a width suffix, e.g. 42i32", t)
            return S.IntLit(t.value, t.width)._at(t)
        if t.kind == FLOAT:
            self.next()
            return S.FloatLit(t.value, t.width)._at(t)
        if t.kind == STRING:
            self.next()
            return S.StringLit(t.value)._at(t)
        if t.kind == CHAR:
            self.next()
            return S.CharLit(t.value)._at(t)

        if t.text in ("true", "false"):
            self.next()
            return S.BoolLit(t.text == "true")._at(t)
        if t.text == "NIL":
            self.next()
            return S.NilLit()._at(t)
        if t.text == "NULL":
            self.next()
            return S.NullLit()._at(t)

        if t.text == "comptime" and self.peek(1).text == "(":
            self.next()
            self.next()
            e = self.parse_expr()
            self.expect(")")
            return S.Comptime(e)._at(t)

        if t.text == "move" and self.peek(1).text == "(":
            self.next()
            self.next()
            e = self.parse_expr()
            self.expect(")")
            return S.Unary("move", e)._at(t)

        if t.text == "#":
            # The compiler-directive sigil (D-020). `#name<T>(...)` keeps bare
            # brackets because the sigil is itself the disambiguator -- `#size_of`
            # cannot be a variable, so `<` after it is unambiguously a
            # type-argument list (D-064).
            self.next()
            name = self.expect_ident().text
            gargs = []
            if self.accept("<"):
                while not self.at(">"):
                    gargs.append(self.parse_type())
                    if not self.accept(","):
                        break
                self.expect(">")
            args = []
            if self.accept("("):
                while not self.at(")"):
                    args.append(self.parse_expr())
                    if not self.accept(","):
                        break
                self.expect(")")
            return S.Builtin(name, gargs, args)._at(t)

        if t.text == "$":
            # the counted-loop iteration variable (D-060 lists it as an
            # expression form). The loops themselves are outside subset 1.
            self.next()
            return S.Ident("$")._at(t)

        if t.text == "(":
            self.next()
            e = self.parse_expr()
            self.expect(")")
            return e

        if t.text == "[":
            self.next()
            elems = []
            while not self.at("]"):
                elems.append(self.parse_expr())
                if not self.accept(","):
                    break
            self.expect("]")
            return S.ArrayLit(elems)._at(t)

        if t.kind in (IDENT, KEYWORD):
            name = self.next().text
            # Struct literal:  Point{ x: 1i32, y: 2i32 }
            if self.at("{") and self._struct_lit_ahead():
                self.next()
                fields = []
                while not self.at("}"):
                    fname = self.expect_ident().text
                    self.expect(":")
                    fields.append((fname, self.parse_expr()))
                    if not self.accept(","):
                        break
                self.expect("}")
                return S.StructLit(name, fields)._at(t)
            return S.Ident(name)._at(t)

        raise ParseError("expected an expression, found %r" % t.text, t)

    def _struct_lit_ahead(self):
        """`Name {` is a struct literal only if it is `{ ident : ...` or `{}`."""
        return (self.peek(1).text == "}" or
                (self.peek(1).kind in (IDENT, KEYWORD) and self.peek(2).text == ":"))


def parse_source(src, path):
    return Parser(lex.lex(src, path)).parse_module(path)
