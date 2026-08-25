"""Subset-1 checker for the Nitpick bootstrap seed.

THROWAWAY (D-085).

    The seed lowers; stage 1 checks.  (SUBSET_1.md section 2)

This pass exists for two reasons and no others:

  1. To reject constructs outside subset 1 with NITPICK-RUNG-001, naming the
     construct and the rung that enables it. This must fire from HERE, never
     from the parser -- the parser never restricts, the backend does (D-085).

  2. To resolve names and compute enough type information that the emitter picks
     the right instruction and the right Result<T> instantiation.

It does NOT verify pick exhaustiveness, Result-handling discipline, escape
analysis, or definite assignment. All of those are implemented properly in the
compiler, where they are audited. The one exception is `fixed`, which is
enforced here because it costs almost nothing and guards our own source from the
first line -- parsing a safety annotation and ignoring it is the worse option.
"""

import syntax as S
import ntypes as T
import diag


class RungError(diag.NpkError):
    """NITPICK-RUNG-001 -- construct not supported at this backend rung.

    Raised from the CHECKER, never from the parser: the parser never restricts,
    the backend does (D-085).
    """

    def __init__(self, construct, rung, node):
        self.construct, self.rung, self.node = construct, rung, node
        super().__init__(diag.Diag(
            "NITPICK-RUNG-001", node._path, node._line, node._col,
            "%s is not supported at this backend rung; enabled by cycle %s"
            % (construct, rung), "check"))


class CheckError(diag.NpkError):
    def __init__(self, msg, node):
        super().__init__(diag.Diag("NITPICK-CHECK-001", node._path, node._line,
                                   node._col, msg, "check"))


class Scope:
    def __init__(self, parent=None):
        self.parent = parent
        self.names = {}          # name -> (Type, is_fixed)

    def declare(self, name, ty, is_fixed):
        self.names[name] = (ty, is_fixed)

    def lookup(self, name):
        s = self
        while s is not None:
            if name in s.names:
                return s.names[name]
            s = s.parent
        return None


# Supplied by the runtime floor (cycle 0.0.4), declared here so calls type-check.
# alloc/dalloc do NOT return Result -- MEMORY_REFERENCE writes them bare.
BUILTINS = {
    "alloc":         (T.Ptr(T.Prim("int8")), False),
    "wildx_alloc":   (T.Ptr(T.Prim("int8")), False),
    "wildx_seal":    (T.NIL, False),
    "wildx_call":    (T.I64, False),
    "wildx_free":    (T.NIL, False),
    "wild_live_count":  (T.I64, False),
    "mono_now": (T.I64, False),
    "wild_release_all": (T.NIL, False),
    "calloc":        (T.Ptr(T.Prim("int8")), False),
    "ralloc":        (T.Ptr(T.Prim("int8")), False),
    "dalloc":        (T.NIL, False),
    "string_concat": (T.STRING, True),
    "int_to_string": (T.STRING, True),
    # The fd quartet (D-141): one syscall each, Result-wrapped, E_EOF for
    # end-of-input. `open` returns the fd TYPE, not a number (D-042).
    "open":          (T.Prim("fd"), True),
    "close":         (T.NIL, True),
    "read":          (T.I64, True),
    "write":         (T.I64, True),
    "string_slice":  (T.STRING, True),
    "string_from_bytes": (T.STRING, False),
    "read_stdin":    (T.STRING, True),
    "to_cstring":    (T.CSTRING, True),
    "read_file":     (T.STRING, True),
    "path_exists":   (T.BOOL, False),
    # read_file's mirror (0.8.3): whole buffer to a path, Result<NIL>.
    "write_file":    (T.NIL, True),
}


class Program:
    """Everything resolved across the module set."""

    def __init__(self):
        self.structs = {}        # name -> {field: Type} (ordered)
        self.enums = {}          # name -> {variant: (index, payload Type or None)}
        self.funcs = {}          # name -> FuncDecl
        self.globals = {}        # name -> Type
        self.modules = []
        self.result_types = []   # every Result<T> instantiated, in first-seen order


class Checker:
    def __init__(self):
        self.p = Program()
        self.fn = None           # current FuncDecl
        self.scope = None

    # --- entry ---------------------------------------------------------------

    def check(self, modules):
        T.reset_enums()
        self.p.modules = modules
        for m in modules:
            for item in m.items:
                self._collect(item)
        for m in modules:
            for item in m.items:
                if isinstance(item, S.FuncDecl):
                    self._check_func(item)
        return self.p

    # --- collection ----------------------------------------------------------

    def _collect(self, item):
        if isinstance(item, S.ImportDecl):
            if item.kind != "wildcard":
                raise RungError("selective or aliased import", "0.3", item)
            return

        if isinstance(item, S.TraitDecl):
            raise RungError("trait declaration", "1.0", item)
        if isinstance(item, S.ImplDecl):
            raise RungError("impl block", "1.0", item)

        if isinstance(item, S.StructDecl):
            if item.generics:
                raise RungError("generic struct", "1.0", item)
            fields = {}
            for f in item.fields:
                fields[f.name] = self.resolve_type(f.type)
            self.p.structs[item.name] = fields
            return

        if isinstance(item, S.ModDecl):
            # `mod:x;` names the file -- nothing to collect. An INLINE module
            # body (0.9.6) refuses: the seed builds nothing that uses one, and
            # the rejection suite needs the refusal, not a crash.
            if item.inline:
                raise RungError("an inline module body", "0.9", item)
            return

        if isinstance(item, S.EnumDecl):
            if item.generics:
                raise RungError("generic enum", "1.0", item)
            variants = {}
            for idx, v in enumerate(item.variants):
                payload = None
                if v.payload:
                    if len(v.payload) > 1:
                        raise RungError("enum variant with more than one payload "
                                        "field", "0.9", item)
                    payload = self.resolve_type(v.payload[0])
                # The tag is the DECLARED value where one is given -- `Red = 5i32`
                # must lower to 5, not to its position in the list.
                tag = idx
                if v.value is not None:
                    if not isinstance(v.value, S.IntLit):
                        raise CheckError("enum value must be an integer literal", item)
                    tag = v.value.value
                variants[v.name] = (tag, payload)
            self.p.enums[item.name] = variants
            T.ENUM_HAS_PAYLOAD[item.name] = any(
                p is not None for _, p in variants.values())
            return

        if isinstance(item, S.FuncDecl):
            if item.generics:
                raise RungError("generic function", "1.0", item)
            if "async" in item.modifiers:
                raise RungError("async function", "1.1", item)
            if "comptime" in item.modifiers:
                raise RungError("comptime function", "0.6", item)
            # The verification carriers (0.9.0, LIVE-1's repair): refused the
            # way the real backend refuses them, so the rejection suite holds
            # for BOTH compilers. The seed refuses `acquires` too -- it has no
            # lock-level analysis, so accepting one would be accepting a claim
            # nothing here checks (no seed-compiled source uses it).
            if getattr(item, "has_contract", False):
                raise RungError("a `requires`/`ensures` contract", "1.3", item)
            if any(getattr(p, "limit_marker", False) for p in item.params):
                raise RungError("a `limit<Rules>` constraint", "1.3", item)
            self._claim(self.p.funcs, item.name, item, "function")
            return

        if isinstance(item, S.GlobalDecl):
            self.p.globals[item.name] = self.resolve_type(item.type)
            return

        raise CheckError("unsupported top-level item %s" % type(item).__name__, item)


    def _claim(self, table, name, item, what):
        """One name, one definition.

        The seed used to write `self.p.funcs[name] = item` and let the last
        definition win. That is not a lax rule, it is a silent one: 0.4.3 added a
        `type_range` to type_cast.npk while type_expr.npk already had one, and
        the seed picked whichever it saw last. It happened to fail in `llc`
        because the two signatures differed -- had they matched, the compiler
        would have been built calling the wrong function, correctly, forever.

        The real resolver reports this as an ambiguous glob import
        (NITPICK-RESOLVE-004). The seed has no module namespaces to reason with,
        so it applies the blunt rule: within one compilation group, a name is
        claimed once. Every source this seed compiles glob-imports its
        dependencies, so any duplicate here is a genuine collision.
        """
        prev = table.get(name)
        if prev is not None:
            raise CheckError(
                "%s `%s` is defined twice in this compilation; the real "
                "resolver reports an ambiguous glob import here, and the seed "
                "cannot tell which one you meant" % (what, name), item)
        table[name] = item

    # --- types ---------------------------------------------------------------

    def resolve_type(self, node):
        if isinstance(node, S.QualType):
            # wild / stack / fixed on a type are lifetime and mutability
            # annotations; the seed lowers them all identically. Stage 1 is
            # where they mean something.
            return self.resolve_type(node.inner)

        if isinstance(node, S.PointerType):
            return T.Ptr(self.resolve_type(node.elem))
        if isinstance(node, S.SliceType):
            return T.Slice(self.resolve_type(node.elem))
        if isinstance(node, S.ArrayType):
            size = node.size
            if not isinstance(size, S.IntLit):
                raise CheckError("array size must be an integer literal", node)
            return T.Array(self.resolve_type(node.elem), size.value)

        if isinstance(node, S.NamedType):
            name = node.name
            if name == "Result":
                if len(node.generic_args) != 1:
                    raise CheckError("Result takes exactly one type argument", node)
                return self.result_of(self.resolve_type(node.generic_args[0]))
            if name in T.OUT_OF_SUBSET_TYPES:
                raise RungError("the type %s" % name, T.OUT_OF_SUBSET_TYPES[name], node)
            if node.generic_args:
                raise RungError("generic type instantiation", "1.0", node)
            if name in T.INT_TYPES or name in ("bool", "char8", "string", "cstring", "NIL"):
                return T.Prim(name)
            if name in self.p.structs or name in self.p.enums:
                return T.Named(name)
            # Forward reference: structs and enums are collected before any
            # function body is checked, so an unknown name here is genuinely
            # unknown.
            return T.Named(name)

        raise CheckError("unsupported type node %s" % type(node).__name__, node)

    def result_of(self, inner):
        r = T.ResultT(inner)
        if r not in self.p.result_types:
            self.p.result_types.append(r)
        return r

    # --- functions -----------------------------------------------------------

    def _check_func(self, fn):
        self.fn = fn
        self.scope = Scope()
        for p in fn.params:
            self.scope.declare(p.name, self.resolve_type(p.type), False)

        ret = self.resolve_type(fn.ret)
        # Every function returns Result<T> except main and failsafe (D-013).
        fn.ret_type = ret
        fn.is_bare = fn.name in ("main", "failsafe")
        if not fn.is_bare:
            self.result_of(ret)

        if fn.body is not None:
            self._block(fn.body, Scope(self.scope))
        self.fn = None

    def _block(self, block, scope):
        prev, self.scope = self.scope, scope
        for st in block.stmts:
            self._stmt(st)
        self.scope = prev

    # --- statements ----------------------------------------------------------

    def _stmt(self, st):
        if isinstance(st, S.Block):
            self._block(st, Scope(self.scope))

        elif isinstance(st, S.VarDecl):
            if getattr(st, "limit_marker", False):
                raise RungError("a `limit<Rules>` constraint", "1.3", st)
            ty = self.resolve_type(st.type)
            if st.init is not None:
                self._expr(st.init)
            is_fixed = "fixed" in (st.quals or []) or \
                       (isinstance(st.type, S.QualType) and st.type.qual == "fixed")
            self.scope.declare(st.name, ty, is_fixed)
            st.var_type = ty

        elif isinstance(st, S.Assign):
            if isinstance(st.target, S.Ident):
                found = self.scope.lookup(st.target.name)
                if found is None:
                    raise CheckError("assignment to unknown name %r" % st.target.name, st)
                if found[1]:
                    # `fixed` is enforced rather than parsed-and-ignored.
                    raise CheckError("cannot assign to `fixed` binding %r"
                                     % st.target.name, st)
            self._expr(st.target)
            self._expr(st.value)

        elif isinstance(st, S.ExprStmt):
            # D-163 rule 6: the value-less statement forms are a CLOSED list.
            # `drop f();` / `relay f();` / `f() ?! c;` / `f() ?| d;` each deal
            # with the outcome; a bare call discards a Result with no keyword,
            # `raw f();` discards the unwrapped value, and a bare value
            # statement throws away a computation. The real checker refuses the
            # same shapes (TYPE-039); the seed refusing them too is what keeps
            # the two frontends agreeing on what compiles.
            e = st.expr
            keyworded = (isinstance(e, S.ResultUnary) and e.op in ("drop", "relay")) \
                or isinstance(e, (S.Emphatic, S.SafeUnwrap))
            if not keyworded:
                if isinstance(e, S.ResultUnary) and e.op == "raw":
                    raise CheckError("`raw f()` in statement position discards "
                                     "the unwrapped value; run it for effect "
                                     "with `drop f();` (D-163)", st)
                raise CheckError("a bare expression statement discards its "
                                 "outcome (D-163): `drop` it, `relay` it, or "
                                 "handle it with `?|`, `?!`, `is_err`, or a "
                                 "`pick`", st)
            self._expr(st.expr)

        elif isinstance(st, S.If):
            self._expr(st.cond)
            self._block(st.then_block, Scope(self.scope))
            if st.else_branch is not None:
                self._stmt(st.else_branch)

        elif isinstance(st, S.While):
            if getattr(st, "invariant_marker", False):
                raise RungError("a loop `invariant`", "1.3", st)
            self._expr(st.cond)
            self._block(st.body, Scope(self.scope))

        elif isinstance(st, S.For):
            raise RungError("the `for` loop", "0.9", st)
        elif isinstance(st, S.Loop):
            raise RungError("the `loop` construct", "0.9", st)
        elif isinstance(st, S.Till):
            raise RungError("the `till` construct", "0.9", st)

        elif isinstance(st, S.Pick):
            self._expr(st.selector)
            for arm in st.arms:
                if arm.guard is not None:
                    raise RungError("a `where` guard", "0.9", arm.guard)
                s = Scope(self.scope)
                self._pattern(arm.pattern, st.selector, s)
                self._block(arm.body, s)

        elif isinstance(st, (S.Pass, S.Return)):
            if st.value is not None:
                self._expr(st.value)
        elif isinstance(st, S.Fail):
            self._expr(st.error)
        elif isinstance(st, S.Exit):
            self._expr(st.code)
        elif isinstance(st, S.Trap):
            self._expr(st.error)
        elif isinstance(st, S.Defer):
            self._block(st.body, Scope(self.scope))
        elif isinstance(st, S.Discard):
            self._expr(st.expr)
        elif isinstance(st, (S.Break, S.Continue, S.Fall)):
            pass
        else:
            raise CheckError("unsupported statement %s" % type(st).__name__, st)

    def _pattern(self, pat, selector, scope):
        if isinstance(pat, S.WildcardPat):
            return
        if isinstance(pat, S.LiteralPat):
            self._expr(pat.expr)
            return
        if isinstance(pat, S.StructPat):
            raise RungError("struct destructuring in `pick`", "0.9", pat)
        if isinstance(pat, S.VariantPat):
            if len(pat.path) != 2:
                raise CheckError("expected Enum.Variant in a pick pattern", pat)
            ename, vname = pat.path
            variants = self.p.enums.get(ename)
            if variants is None:
                raise CheckError("unknown enum %r" % ename, pat)
            if vname not in variants:
                raise CheckError("enum %s has no variant %r" % (ename, vname), pat)
            _, payload = variants[vname]
            if pat.bindings:
                if payload is None:
                    raise CheckError("variant %s.%s carries no payload"
                                     % (ename, vname), pat)
                if len(pat.bindings) != 1:
                    raise CheckError("one binding per payload", pat)
                scope.declare(pat.bindings[0], payload, False)
            return
        raise CheckError("unsupported pattern %s" % type(pat).__name__, pat)

    # --- expressions ---------------------------------------------------------

    def _expr(self, e):
        """Walk for rung violations and annotate what the emitter needs."""
        if isinstance(e, S.FloatLit):
            raise RungError("float literals", "0.9", e)
        if isinstance(e, S.Comptime):
            raise RungError("comptime(...)", "0.6", e)
        if isinstance(e, (S.SafeUnwrap, S.Emphatic)):
            # `e ?| d` / `e ?! c` (1.1.1): src needs both -- D-163's rewrite
            # rules prescribe `?!` for a provably-dead failure branch, and the
            # seed compiles src. The operand and the default/code are walked;
            # the real frontend is what enforces the fine rules.
            self._expr(e.operand if hasattr(e, "operand") else e.expr)
            self._expr(e.default if isinstance(e, S.SafeUnwrap) else e.code)
            return
        if isinstance(e, S.Ternary):
            raise RungError("the ternary `is (c) : a : b`", "0.9", e)
        if isinstance(e, S.Pipeline):
            raise RungError("the pipeline operators |> and <|", "0.9", e)

        if isinstance(e, S.ResultUnary):
            if e.op == "await":
                raise RungError("await", "1.1", e)
            self._expr(e.operand)
            return

        if isinstance(e, S.Call):
            if e.generic_args:
                raise RungError("turbofish generic call", "1.0", e)
            if isinstance(e.callee, S.Field):
                # Two things parse identically here: enum-variant construction
                # `Expr.IntLit(42i64)` and a UFCS method call `p.magnitude()`.
                # Only the receiver tells them apart.
                ctor = self._enum_ctor(e.callee)
                if ctor is None:
                    # `.` field access itself is IN subset 1; only the
                    # method-call form is out.
                    raise RungError("UFCS method call", "1.0", e)
                e.enum_ctor = ctor
                for a in e.args:
                    self._expr(a)
                return
            for a in e.args:
                self._expr(a)
            if isinstance(e.callee, S.Ident):
                e.target = e.callee.name
                b = BUILTINS.get(e.target)
                if b is not None and b[1]:
                    self.result_of(b[0])

                # A CALL PASSES AS MANY ARGUMENTS AS THE FUNCTION DECLARES.
                #
                # NOTHING CHECKED THIS, and the consequence was not a missing
                # diagnostic -- it was GARBAGE. A call with one argument too few
                # emits a call with one operand too few, and the callee reads
                # whatever was in that register or stack slot. The value it gets
                # depends on the binary's layout, so the symptom moves when
                # anything unrelated changes size.
                #
                # That is D-127: a `type_cast.npk` helper called `etyper_init` with
                # nine arguments where it takes ten, and the missing `ExprTypes->`
                # read as a small integer. Two days were spent on it as an
                # out-of-bounds WRITE -- adding a size header to the allocator made
                # it crash, so the allocator looked guilty -- and no memory was ever
                # corrupted. `valgrind` named it in seconds once it was pointed at
                # the right binary.
                #
                # The seed is throwaway (D-085) and this check is not: a tool that
                # silently miscompiles the compiler is worse than no tool.
                fn = self.p.funcs.get(e.target)
                if fn is not None and len(e.args) != len(fn.params):
                    raise CheckError(
                        "`%s` takes %d argument(s) and this passes %d"
                        % (e.target, len(fn.params), len(e.args)), e)
            return

        if isinstance(e, S.IntLit):
            # A suffix-form base -- `0FFhex`, `1T0t`, `2An` -- reaches here with no
            # value, because the lexer records the literal and does not evaluate
            # it (the route floats already take). Refusing it HERE and not in the
            # lexer is D-085: the front of the compiler never restricts, so a
            # construct outside the rung is a checker diagnostic naming the rung.
            if e.value is None:
                raise RungError("a suffix-form numeric base (hex/oct/ternary/"
                                "nonary); subset 1 lowers decimal only "
                                "(D-147 removed the 0x/0b prefixes)", "0.9", e)
            return

        if isinstance(e, S.Binary):
            self._expr(e.lhs)
            self._expr(e.rhs)
            lt = self.type_of(e.lhs)
            if T.is_tbb(lt) and e.op not in ("==", "!="):
                # tbb32 is an error-code type here: comparison only. Arithmetic
                # would drag ERR, stickiness and saturation into the seed.
                raise RungError("arithmetic on a `tbb` type", "0.9", e)

            # Every operator below `&&`/`||` lowers to ONE LLVM instruction over
            # the left operand's type, and those instructions accept only `iN`
            # (plus `ptr`, for icmp). Without this the seed accepted
            # `"a" + "b"` -- which is REAL Nitpick, since `+` concatenates
            # strings (TYPE_REFERENCE section 4) -- and emitted
            # `add { ptr, i64, i64 }`, invalid IR that llc rejected a stage
            # later with no idea which source line meant it.
            #
            # The seed does not lower it, and the place to say so is here, where
            # the message can name the operator and the type.
            if lt is not None and e.op not in ("&&", "||"):
                ordered = e.op in ("==", "!=", "<", "<=", ">", ">=")
                ok = T.is_ll_scalar_int(lt)
                if ordered and T.is_ll_pointer(lt):
                    ok = True
                if not ok:
                    raise RungError("`%s` on `%r`" % (e.op, lt), "0.9", e)
            return

        if isinstance(e, S.Unary):
            if e.op == "move":
                raise RungError("move(...)", "0.9", e)
            self._expr(e.operand)
            return

        if isinstance(e, S.Cast):
            self._expr(e.expr)
            self.resolve_type(e.target)
            return

        if isinstance(e, S.Field):
            ctor = self._enum_ctor(e)
            if ctor is not None:
                e.enum_ctor = ctor      # payload-less variant used as a value
                return
            self._expr(e.obj)
            return
        if isinstance(e, S.Index):
            self._expr(e.obj)
            self._expr(e.index)
            return
        if isinstance(e, S.Range):
            self._expr(e.lo)
            self._expr(e.hi)
            return
        if isinstance(e, S.StructLit):
            for _, v in e.fields:
                self._expr(v)
            return
        if isinstance(e, S.ArrayLit):
            for v in e.elems:
                self._expr(v)
            return
        if isinstance(e, S.Builtin):
            if e.name != "size_of":
                raise RungError("the #%s builtin" % e.name, "0.6", e)
            if len(e.generic_args) != 1 or e.args:
                raise CheckError("#size_of takes one type argument and no "
                                 "value arguments", e)
            e.size_type = self.resolve_type(e.generic_args[0])
            return

        if isinstance(e, (S.IntLit, S.StringLit, S.CharLit, S.BoolLit,
                          S.NilLit, S.NullLit, S.Ident)):
            return

        raise CheckError("unsupported expression %s" % type(e).__name__, e)

    def _enum_ctor(self, field):
        """Field(Ident(Enum), Variant) -> (enum, variant, tag, payload) or None."""
        if not isinstance(field.obj, S.Ident):
            return None
        variants = self.p.enums.get(field.obj.name)
        if variants is None:
            return None
        if field.name not in variants:
            # The enum exists but has no such variant. Falling through to "this
            # is a field access" would report `unknown name 'NumWidth'`, which
            # points at the wrong half of the expression entirely.
            raise CheckError("enum %s has no variant %r"
                             % (field.obj.name, field.name), field)
        tag, payload = variants[field.name]
        return (field.obj.name, field.name, tag, payload)

    # --- typing --------------------------------------------------------------

    def type_of(self, e):
        """Best-effort type of an expression. Enough to choose instructions."""
        if isinstance(e, S.IntLit):
            return T.Prim({"i8": "int8", "i16": "int16", "i32": "int32",
                           "i64": "int64", "u8": "uint8", "u16": "uint16",
                           "u32": "uint32", "u64": "uint64",
                           "tbb32": "tbb32"}.get(e.width, "int32"))
        if isinstance(e, S.BoolLit):
            return T.BOOL
        if isinstance(e, S.CharLit):
            return T.CHAR8
        if isinstance(e, S.StringLit):
            return T.STRING
        if isinstance(e, S.NilLit):
            return T.NIL
        if isinstance(e, S.Ident):
            found = self.scope.lookup(e.name) if self.scope else None
            if found:
                return found[0]
            if e.name in self.p.globals:
                return self.p.globals[e.name]
            return None
        if isinstance(e, S.Builtin):
            return T.I64
        if isinstance(e, S.Cast):
            return self.resolve_type(e.target)
        if isinstance(e, S.Binary):
            if e.op in ("==", "!=", "<", "<=", ">", ">=", "&&", "||"):
                return T.BOOL
            return self.type_of(e.lhs)
        if isinstance(e, S.Unary):
            if e.op == "!":
                return T.BOOL
            if e.op == "@":
                return T.Ptr(self.type_of(e.operand))
            if e.op == "<-":
                inner = self.type_of(e.operand)
                return inner.elem if isinstance(inner, T.Ptr) else inner
            return self.type_of(e.operand)
        if isinstance(e, S.ResultUnary):
            inner = self.type_of(e.operand)
            if isinstance(inner, T.ResultT):
                return inner.inner
            return inner
        if isinstance(e, (S.SafeUnwrap, S.Emphatic)):
            inner = self.type_of(e.expr)
            if isinstance(inner, T.ResultT):
                return inner.inner
            return inner
        if isinstance(e, S.Call) and getattr(e, "enum_ctor", None) is not None:
            return T.Named(e.enum_ctor[0])
        if isinstance(e, S.Call) and isinstance(e.callee, S.Ident):
            fn = self.p.funcs.get(e.callee.name)
            if fn is not None:
                r = self.resolve_type(fn.ret)
                return r if fn.name in ("main", "failsafe") else T.ResultT(r)
            b = BUILTINS.get(e.callee.name)
            if b is not None:
                return self.result_of(b[0]) if b[1] else b[0]
            return None
        if isinstance(e, S.Field):
            ctor = getattr(e, "enum_ctor", None)
            if ctor is not None:
                return T.Named(ctor[0])
            obj = self.type_of(e.obj)
            if isinstance(obj, T.Ptr):
                obj = obj.elem
            if isinstance(obj, T.Prim) and obj.name == "string":
                # A STRING'S BYTES ARE `uint8`, as the real checker types them
                # (type_access.npk, 0.7.8): a code unit is 0..255. The seed said
                # `int8` until 1.0.8, so `(s.ptr[i]) => int64` sign-extended a
                # UTF-8 lead byte to a negative number in seed-built code.
                if e.name == "ptr":
                    return T.Ptr(T.Prim("uint8"))
                if e.name in ("len", "cap"):
                    return T.I64
            if isinstance(obj, T.Prim) and obj.name == "cstring":
                # {ptr, i64} and no cap: a cstring is never grown and is not ours
                # to free (D-049).
                if e.name == "ptr":
                    return T.Ptr(T.Prim("uint8"))
                if e.name == "len":
                    return T.I64
            if isinstance(obj, T.Slice) and e.name == "len":
                return T.I64
            if isinstance(obj, T.ResultT):
                if e.name == "error":
                    return T.TBB32
                if e.name == "is_error":
                    return T.BOOL          # derived: error != 0 (D-069)
                if e.name == "value":
                    return obj.inner
            if isinstance(obj, T.Named):
                fields = self.p.structs.get(obj.name)
                if fields and e.name in fields:
                    return fields[e.name]
            return None
        if isinstance(e, S.Index):
            obj = self.type_of(e.obj)
            if isinstance(obj, (T.Slice, T.Array, T.Ptr)):
                return obj.elem
            return None
        if isinstance(e, S.StructLit):
            return T.Named(e.name)
        if isinstance(e, S.Range):
            return None
        return None
