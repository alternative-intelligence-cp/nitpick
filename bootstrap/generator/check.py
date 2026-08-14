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


class RungError(Exception):
    """NITPICK-RUNG-001 -- construct not supported at this backend rung."""

    def __init__(self, construct, rung, node):
        self.construct, self.rung, self.node = construct, rung, node
        loc = "%s:%d:%d" % (node._path, node._line, node._col)
        super().__init__("%s: NITPICK-RUNG-001: %s is not supported at this "
                         "backend rung; enabled by cycle %s" % (loc, construct, rung))


class CheckError(Exception):
    def __init__(self, msg, node):
        loc = "%s:%d:%d" % (node._path, node._line, node._col)
        super().__init__("%s: %s" % (loc, msg))


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
    "calloc":        (T.Ptr(T.Prim("int8")), False),
    "ralloc":        (T.Ptr(T.Prim("int8")), False),
    "dalloc":        (T.NIL, False),
    "string_concat": (T.STRING, True),
    "int_to_string": (T.STRING, True),
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
            return

        if isinstance(item, S.FuncDecl):
            if item.generics:
                raise RungError("generic function", "1.0", item)
            if "async" in item.modifiers:
                raise RungError("async function", "1.1", item)
            if "comptime" in item.modifiers:
                raise RungError("comptime function", "0.6", item)
            self.p.funcs[item.name] = item
            return

        if isinstance(item, S.GlobalDecl):
            self.p.globals[item.name] = self.resolve_type(item.type)
            return

        raise CheckError("unsupported top-level item %s" % type(item).__name__, item)

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
            if name in T.INT_TYPES or name in ("bool", "char8", "string", "NIL"):
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
            self._expr(st.expr)

        elif isinstance(st, S.If):
            self._expr(st.cond)
            self._block(st.then_block, Scope(self.scope))
            if st.else_branch is not None:
                self._stmt(st.else_branch)

        elif isinstance(st, S.While):
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
            raise RungError("the ?-family operators (? ?? ?! ?. ?|)", "0.9", e)
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
            return

        if isinstance(e, S.Binary):
            self._expr(e.lhs)
            self._expr(e.rhs)
            lt = self.type_of(e.lhs)
            if T.is_tbb(lt) and e.op not in ("==", "!="):
                # tbb32 is an error-code type here: comparison only. Arithmetic
                # would drag ERR, stickiness and saturation into the seed.
                raise RungError("arithmetic on a `tbb` type", "0.9", e)
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
        if isinstance(e, (S.IntLit, S.StringLit, S.CharLit, S.BoolLit,
                          S.NilLit, S.NullLit, S.Ident)):
            return

        raise CheckError("unsupported expression %s" % type(e).__name__, e)

    def _enum_ctor(self, field):
        """Field(Ident(Enum), Variant) -> (enum, variant, index, payload) or None."""
        if not isinstance(field.obj, S.Ident):
            return None
        variants = self.p.enums.get(field.obj.name)
        if variants is None or field.name not in variants:
            return None
        idx, payload = variants[field.name]
        return (field.obj.name, field.name, idx, payload)

    # --- typing --------------------------------------------------------------

    def type_of(self, e):
        """Best-effort type of an expression. Enough to choose instructions."""
        if isinstance(e, S.IntLit):
            return T.Prim({"i8": "int8", "i16": "int16", "i32": "int32",
                           "i64": "int64", "u8": "uint8", "u16": "uint16",
                           "u32": "uint32", "u64": "uint64"}.get(e.width, "int32"))
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
        if isinstance(e, S.Cast):
            return self.resolve_type(e.target)
        if isinstance(e, S.Binary):
            if e.op in ("==", "!=", "<", "<=", ">", ">=", "&&", "||"):
                return T.BOOL
            return self.type_of(e.lhs)
        if isinstance(e, S.Unary):
            return T.BOOL if e.op == "!" else self.type_of(e.operand)
        if isinstance(e, S.ResultUnary):
            inner = self.type_of(e.operand)
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
            if isinstance(obj, (T.Slice, T.Array)):
                return obj.elem
            return None
        if isinstance(e, S.StructLit):
            return T.Named(e.name)
        if isinstance(e, S.Range):
            return None
        return None
