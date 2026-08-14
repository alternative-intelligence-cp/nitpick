"""LLVM IR emitter for the Nitpick bootstrap seed.

THROWAWAY (D-085) -- but its OUTPUT is not throwaway in effect. The seed's IR has
to interoperate with stage 1's until the fixpoint closes, so every layout comes
from ntypes.py and must match the specification exactly.

Naive by design. No optimisation, no SSA construction: every local is an alloca
with loads and stores, and every expression is materialised as a value. `opt` and
`llc` do the rest, and this IR only ever has to compile the compiler once.

Determinism is a requirement, not a nicety (D-078): identical input must produce
byte-identical output, or D-085's stage-1/stage-2 fixpoint check cannot be run.
Everything below iterates in source order and numbers temporaries sequentially.
"""

import syntax as S
import ntypes as T
import check
import diag


class EmitError(diag.NpkError):
    def __init__(self, msg, node):
        super().__init__(diag.Diag("NITPICK-EMIT-001", node._path, node._line,
                                   node._col, msg, "emit"))


# Argument types of the runtime floor. The RETURN type is derived from
# check.BUILTINS below rather than written twice: declaring a runtime symbol as
# returning a bare string while the checker types it as Result<string> is exactly
# the kind of silent disagreement that produces IR llc rejects -- and it did.
RUNTIME_ARGS = {
    "alloc":         ["i64"],
    "calloc":        ["i64", "i64"],
    "ralloc":        ["ptr", "i64"],
    "dalloc":        ["ptr"],
    "string_concat": ["{ ptr, i64, i64 }", "{ ptr, i64, i64 }"],
    "int_to_string": ["i64"],
    "write_raw":     ["i32", "ptr", "i64"],
}


def _runtime():
    out = {}
    for name, args in RUNTIME_ARGS.items():
        ret_ty, wrapped = check.BUILTINS[name]
        if ret_ty == T.NIL and not wrapped:
            ret = "void"
        else:
            ret = T.llvm(T.ResultT(ret_ty) if wrapped else ret_ty)
        out[name] = ("@npk_" + name, ret, args)
    return out


RUNTIME = _runtime()


def sym(name):
    """Function symbol. `main` keeps its name for the linker; everything else is
    prefixed, so Nitpick names cannot collide with C runtime symbols."""
    return "@main" if name == "main" else "@npk_" + name


class Val:
    """An emitted value: its LLVM type text and its register or constant."""
    __slots__ = ("ty", "ref", "npk")

    def __init__(self, ty, ref, npk=None):
        self.ty = ty
        self.ref = ref
        self.npk = npk        # the Nitpick type, where known

    def __repr__(self):
        return "%s %s" % (self.ty, self.ref)


class Emitter:
    def __init__(self, program, checker):
        self.p = program
        self.ck = checker
        self.out = []
        self.strings = []      # (global name, bytes)
        self.n = 0             # temporary counter
        self.lbl = 0
        self.locals = {}       # name -> (ptr ref, Type)
        self.defers = []       # stack of lists of Block
        self.fn = None
        self.terminated = False

    # --- plumbing ------------------------------------------------------------

    def tmp(self):
        self.n += 1
        return "%%t%d" % self.n

    def label(self, base):
        self.lbl += 1
        return "%s%d" % (base, self.lbl)

    def w(self, line):
        self.out.append("  " + line)

    def raw(self, line):
        self.out.append(line)

    def start_block(self, name):
        self.raw("%s:" % name)
        self.terminated = False

    def br(self, name):
        if not self.terminated:
            self.w("br label %%%s" % name)
            self.terminated = True

    # --- module --------------------------------------------------------------

    def emit(self, module_id):
        self.raw("; ModuleID = '%s'" % module_id)
        self.raw('target triple = "x86_64-unknown-linux-gnu"')
        self.raw("")

        # named aggregate types, in source order
        for name, fields in self.p.structs.items():
            body = ", ".join(T.llvm(t) for t in fields.values()) or "i8"
            self.raw("%%%s = type { %s }" % (name, body))
        for name in self.p.enums:
            # A payload-less enum is a plain i32 and needs no type definition
            # (TYPE_REFERENCE 9.3). Only a tagged one gets a body, and the
            # single-word payload rule (SUBSET_1 1.2) fixes its shape.
            if T.ENUM_HAS_PAYLOAD.get(name):
                self.raw("%%%s = type { i32, i64 }" % name)
        if self.p.structs or self.p.enums:
            self.raw("")

        body_start = len(self.out)

        for m in self.p.modules:
            for item in m.items:
                if isinstance(item, S.FuncDecl) and item.body is not None:
                    self.func(item)

        # string literals and runtime declarations, hoisted above the bodies so
        # the module reads top-down
        head = []
        for gname, data in self.strings:
            enc = "".join(
                c if (32 <= ord(c) < 127 and c not in '"\\') else "\\%02X" % ord(c)
                for c in data)
            head.append('%s = private unnamed_addr constant [%d x i8] c"%s"'
                        % (gname, len(data), enc))
        if self.strings:
            head.append("")
        for _, (s, ret, args) in sorted(RUNTIME.items()):
            head.append("declare %s %s(%s)" % (ret, s, ", ".join(args)))
        head.append("declare void @npk_exit(i32)")
        head.append("")
        self.out[body_start:body_start] = head
        return "\n".join(self.out) + "\n"

    # --- functions -----------------------------------------------------------

    def fn_sig(self, fn):
        ret = self.ck.resolve_type(fn.ret)
        if fn.name in ("main", "failsafe"):
            return T.llvm(ret), ret, True
        return T.llvm(T.ResultT(ret)), ret, False

    def func(self, fn):
        self.fn = fn
        self.n = 0
        self.locals = {}
        self.defers = [[]]
        self.terminated = False

        ll_ret, ret_ty, bare = self.fn_sig(fn)
        self.fn_ret_ty = ret_ty
        self.fn_bare = bare
        self.fn_ll_ret = ll_ret

        params = []
        for i, p in enumerate(fn.params):
            pty = self.ck.resolve_type(p.type)
            params.append("%s %%a%d" % (T.llvm(pty), i))
        self.raw("define %s %s(%s) {" % (ll_ret, sym(fn.name), ", ".join(params)))
        self.start_block("entry")

        for i, p in enumerate(fn.params):
            pty = self.ck.resolve_type(p.type)
            slot = self.tmp()
            self.w("%s = alloca %s" % (slot, T.llvm(pty)))
            self.w("store %s %%a%d, ptr %s" % (T.llvm(pty), i, slot))
            self.locals[p.name] = (slot, pty)

        self.block(fn.body)

        if not self.terminated:
            # Falling off the end. Subset 1 sources always return explicitly;
            # this keeps the IR well-formed regardless.
            if bare:
                self.w("ret %s 0" % ll_ret)
            else:
                self.w("ret %s zeroinitializer" % ll_ret)
        self.raw("}")
        self.raw("")
        self.fn = None

    # --- statements ----------------------------------------------------------

    def block(self, blk, own_scope=True):
        saved = dict(self.locals) if own_scope else None
        self.defers.append([])
        for st in blk.stmts:
            if self.terminated:
                break
            self.stmt(st)
        frame = self.defers.pop()
        if not self.terminated:
            self.run_defers([frame])
        if own_scope:
            self.locals = saved

    def run_defers(self, frames):
        """`defer` runs on every NORMAL exit path -- scope end, return, pass,
        fail, relay, exit -- and never on a trap (D-014, D-080)."""
        for frame in reversed(frames):
            for d in reversed(frame):
                saved = dict(self.locals)
                self.block(d, own_scope=False)
                self.locals = saved

    def all_defers(self):
        return list(self.defers)

    def stmt(self, st):
        if isinstance(st, S.Block):
            self.block(st)

        elif isinstance(st, S.VarDecl):
            ty = self.ck.resolve_type(st.type)
            slot = self.tmp()
            self.w("%s = alloca %s" % (slot, T.llvm(ty)))
            if st.init is not None:
                v = self.expr(st.init, want=ty)
                self.w("store %s %s, ptr %s" % (v.ty, v.ref, slot))
            self.locals[st.name] = (slot, ty)

        elif isinstance(st, S.Assign):
            slot, ty = self.addr_of(st.target)
            if st.op == "=":
                v = self.expr(st.value, want=ty)
            else:
                cur = self.tmp()
                self.w("%s = load %s, ptr %s" % (cur, T.llvm(ty), slot))
                rhs = self.expr(st.value, want=ty)
                v = self.arith(st.op[:-1], Val(T.llvm(ty), cur, ty), rhs, ty)
            self.w("store %s %s, ptr %s" % (v.ty, v.ref, slot))

        elif isinstance(st, S.ExprStmt):
            self.expr(st.expr)

        elif isinstance(st, S.Discard):
            self.expr(st.expr)

        elif isinstance(st, S.If):
            self.emit_if(st)

        elif isinstance(st, S.While):
            head, body, done = self.label("wh.head"), self.label("wh.body"), self.label("wh.done")
            self.br(head)
            self.start_block(head)
            c = self.cond(st.cond)
            self.w("br i1 %s, label %%%s, label %%%s" % (c, body, done))
            self.terminated = True
            self.start_block(body)
            self.block(st.body)
            self.br(head)
            self.start_block(done)

        elif isinstance(st, S.Pick):
            self.emit_pick(st)

        elif isinstance(st, S.Pass):
            self.run_defers(self.all_defers())
            self.emit_return_ok(st.value)

        elif isinstance(st, S.Fail):
            self.run_defers(self.all_defers())
            e = self.expr(st.error, want=T.TBB32)
            self.emit_return_err(e.ref)

        elif isinstance(st, S.Return):
            self.run_defers(self.all_defers())
            self.emit_return_ok(st.value)

        elif isinstance(st, S.Exit):
            # exit is legal only in main / failsafe (D-013). Returning from
            # either IS process exit, so it lowers to a plain ret.
            self.run_defers(self.all_defers())
            v = self.expr(st.code, want=T.I32)
            self.w("ret %s %s" % (self.fn_ll_ret, v.ref))
            self.terminated = True

        elif isinstance(st, S.Trap):
            # A trap runs NO defers (D-014). Deliberately not calling
            # run_defers here -- that omission is the decision.
            #
            # failsafe's return value IS the process exit code: it must be
            # positive, because reaching failsafe means something failed and
            # returning 0 would be a contradiction (D-014). So the trap path
            # calls it and then exits with what it returned -- dropping that
            # value on the floor would discard the whole point of the handler.
            e = self.expr(st.error, want=T.TBB32)
            r = self.tmp()
            self.w("%s = call i32 @npk_failsafe(i32 %s)" % (r, e.ref))
            self.w("call void @npk_exit(i32 %s)" % r)
            self.w("unreachable")
            self.terminated = True

        elif isinstance(st, S.Defer):
            self.defers[-1].append(st.body)

        elif isinstance(st, (S.Break, S.Continue, S.Fall)):
            raise EmitError("`%s` is not lowered at this rung"
                            % type(st).__name__.lower(), st)
        else:
            raise EmitError("cannot lower %s" % type(st).__name__, st)

    def emit_if(self, st):
        then, done = self.label("if.then"), self.label("if.done")
        els = self.label("if.else") if st.else_branch is not None else done
        c = self.cond(st.cond)
        self.w("br i1 %s, label %%%s, label %%%s" % (c, then, els))
        self.terminated = True
        self.start_block(then)
        self.block(st.then_block)
        self.br(done)
        if st.else_branch is not None:
            self.start_block(els)
            self.stmt(st.else_branch)
            self.br(done)
        self.start_block(done)

    def emit_pick(self, st):
        sel_ty = self.ty(st.selector)
        sel = self.expr(st.selector)
        if (isinstance(sel_ty, T.Named) and sel_ty.name in self.p.enums
                and T.ENUM_HAS_PAYLOAD.get(sel_ty.name)):
            tag = self.tmp()
            self.w("%s = extractvalue %s %s, 0" % (tag, sel.ty, sel.ref))
            disc, disc_ty = tag, "i32"
        elif sel.ty == "i8":
            disc, disc_ty = sel.ref, "i8"
        else:
            disc, disc_ty = sel.ref, sel.ty

        done = self.label("pick.done")
        arms, default = [], None
        for arm in st.arms:
            lbl = self.label("pick.arm")
            if isinstance(arm.pattern, S.WildcardPat):
                default = (lbl, arm)
            else:
                arms.append((self.pattern_const(arm.pattern, sel_ty), lbl, arm))

        default_lbl = default[0] if default else done
        cases = " ".join("%s %s, label %%%s" % (disc_ty, c, l) for c, l, _ in arms)
        self.w("switch %s %s, label %%%s [ %s ]" % (disc_ty, disc, default_lbl, cases))
        self.terminated = True

        for _, lbl, arm in arms:
            self.start_block(lbl)
            saved = dict(self.locals)
            self.bind_payload(arm.pattern, sel)
            self.block(arm.body, own_scope=False)
            self.locals = saved
            self.br(done)
        if default:
            self.start_block(default[0])
            self.block(default[1].body)
            self.br(done)
        self.start_block(done)

    def pattern_const(self, pat, sel_ty):
        if isinstance(pat, S.LiteralPat):
            e = pat.expr
            if isinstance(e, S.IntLit):
                return str(e.value)
            if isinstance(e, S.BoolLit):
                return "1" if e.value else "0"
            if isinstance(e, S.CharLit):
                return str(ord(e.value))
            raise EmitError("unsupported pick literal", pat)
        if isinstance(pat, S.VariantPat):
            ename, vname = pat.path
            return str(self.p.enums[ename][vname][0])
        raise EmitError("unsupported pattern", pat)

    def bind_payload(self, pat, sel):
        if not isinstance(pat, S.VariantPat) or not pat.bindings:
            return
        ename, vname = pat.path
        _, payload_ty = self.p.enums[ename][vname]
        wide = self.tmp()
        self.w("%s = extractvalue %s %s, 1" % (wide, sel.ty, sel.ref))
        v = self.narrow(Val("i64", wide, T.I64), payload_ty)
        slot = self.tmp()
        self.w("%s = alloca %s" % (slot, T.llvm(payload_ty)))
        self.w("store %s %s, ptr %s" % (v.ty, v.ref, slot))
        self.locals[pat.bindings[0]] = (slot, payload_ty)

    # --- returns -------------------------------------------------------------

    def emit_return_ok(self, value_node):
        if self.fn_bare:
            v = self.expr(value_node, want=self.fn_ret_ty) if value_node else None
            self.w("ret %s %s" % (self.fn_ll_ret, v.ref if v else "0"))
            self.terminated = True
            return
        rt = T.ResultT(self.fn_ret_ty)
        ll = T.llvm(rt)
        if self.fn_ret_ty == T.NIL:
            self.w("ret %s { i32 0 }" % ll)          # Result<NIL> is {i32}
            self.terminated = True
            return
        v = self.expr(value_node, want=self.fn_ret_ty)
        a = self.tmp()
        self.w("%s = insertvalue %s undef, %s %s, 0" % (a, ll, v.ty, v.ref))
        b = self.tmp()
        self.w("%s = insertvalue %s %s, i32 0, 1" % (b, ll, a))
        self.w("ret %s %s" % (ll, b))
        self.terminated = True

    def emit_return_err(self, code_ref):
        """Build this function's Result carrying `code_ref` verbatim."""
        ll = T.llvm(T.ResultT(self.fn_ret_ty))
        if self.fn_bare:
            self.w("ret %s %s" % (self.fn_ll_ret, code_ref))
            self.terminated = True
            return
        if self.fn_ret_ty == T.NIL:
            a = self.tmp()
            self.w("%s = insertvalue %s undef, i32 %s, 0" % (a, ll, code_ref))
            self.w("ret %s %s" % (ll, a))
            self.terminated = True
            return
        a = self.tmp()
        self.w("%s = insertvalue %s undef, %s %s, 0"
               % (a, ll, T.llvm(self.fn_ret_ty), T.zero(self.fn_ret_ty)))
        b = self.tmp()
        self.w("%s = insertvalue %s %s, i32 %s, 1" % (b, ll, a, code_ref))
        self.w("ret %s %s" % (ll, b))
        self.terminated = True

    # --- expressions ---------------------------------------------------------

    def ty(self, node):
        """Type of an expression, in the emitter's scope.

        The checker's scope is torn down once checking finishes, so type queries
        during emission must be answered against the locals the EMITTER knows
        about. Rebuilt per call: a seed can afford it.
        """
        sc = check.Scope()
        for n, (_, t) in self.locals.items():
            sc.declare(n, t, False)
        self.ck.scope = sc
        return self.ck.type_of(node)

    def _addressable(self, e):
        """Is this expression rooted in a local? Only then does it have an address."""
        while isinstance(e, (S.Field, S.Index)):
            e = e.obj
        return isinstance(e, S.Ident) and e.name in self.locals

    def addr_of(self, e):
        """The ADDRESS of a place expression: Ident, Field, or Index.

        Everything else in the emitter is value-based, which was enough for the
        conformance suite but not for writing a compiler: `list.items[n] = d`
        needs a place, not a value. Added in 0.0.6, when the first real source
        demanded it.
        """
        if isinstance(e, S.Ident):
            slot, ty = self.lookup(e.name, e)
            return slot, ty

        if isinstance(e, S.Field):
            base, oty = self.addr_of(e.obj)
            # `.` auto-dereferences a pointer (D-006), so a pointer operand is
            # loaded first and the field taken from what it points at.
            if isinstance(oty, T.Ptr):
                loaded = self.tmp()
                self.w("%s = load ptr, ptr %s" % (loaded, base))
                base, oty = loaded, oty.elem
            if isinstance(oty, T.Prim) and oty.name == "string":
                idx = {"ptr": 0, "len": 1, "cap": 2}.get(e.name)
                if idx is None:
                    raise EmitError("string has no field %r" % e.name, e)
                fty = T.Ptr(T.Prim("int8")) if idx == 0 else T.I64
                r = self.tmp()
                self.w("%s = getelementptr { ptr, i64, i64 }, ptr %s, i32 0, i32 %d"
                       % (r, base, idx))
                return r, fty
            if not (isinstance(oty, T.Named) and oty.name in self.p.structs):
                raise EmitError("cannot take the address of field %r on %r"
                                % (e.name, oty), e)
            fields = self.p.structs[oty.name]
            if e.name not in fields:
                raise EmitError("no field %r on %s" % (e.name, oty.name), e)
            i = list(fields).index(e.name)
            r = self.tmp()
            self.w("%s = getelementptr %s, ptr %s, i32 0, i32 %d"
                   % (r, T.llvm(oty), base, i))
            return r, fields[e.name]

        if isinstance(e, S.Index):
            oty = self.ty(e.obj)
            idx = self.expr(e.index, want=T.I64)
            if isinstance(oty, T.Slice):
                v = self._expr(e.obj)
                base = self.tmp()
                self.w("%s = extractvalue %s %s, 0" % (base, v.ty, v.ref))
                r = self.tmp()
                self.w("%s = getelementptr %s, ptr %s, i64 %s"
                       % (r, T.llvm(oty.elem), base, idx.ref))
                return r, oty.elem
            if isinstance(oty, T.Ptr):
                v = self._expr(e.obj)
                r = self.tmp()
                self.w("%s = getelementptr %s, ptr %s, i64 %s"
                       % (r, T.llvm(oty.elem), v.ref, idx.ref))
                return r, oty.elem
            if isinstance(oty, T.Array):
                base, _ = self.addr_of(e.obj)
                r = self.tmp()
                self.w("%s = getelementptr %s, ptr %s, i64 0, i64 %s"
                       % (r, T.llvm(oty), base, idx.ref))
                return r, oty.elem
            raise EmitError("cannot index %r" % oty, e)

        raise EmitError("%s is not a place expression"
                        % type(e).__name__, e)

    def lookup(self, name, node):
        if name not in self.locals:
            raise EmitError("unknown name %r" % name, node)
        return self.locals[name]

    def cond(self, e):
        v = self.expr(e)
        if v.ty == "i1":
            return v.ref
        r = self.tmp()
        self.w("%s = icmp ne %s %s, 0" % (r, v.ty, v.ref))
        return r

    def expr(self, e, want=None):
        v = self._expr(e)
        if want is not None and v is not None:
            v = self.coerce(v, want)
        return v

    def coerce(self, v, want):
        wl = T.llvm(want)
        if v.ty == wl:
            return v
        if v.ty == "i1" and wl.startswith("i"):
            r = self.tmp()
            self.w("%s = zext i1 %s to %s" % (r, v.ref, wl))
            return Val(wl, r, want)
        if v.ty.startswith("i") and wl.startswith("i") and v.ty[1:].isdigit() and wl[1:].isdigit():
            return self.narrow(v, want)
        return Val(wl, v.ref, want)

    def narrow(self, v, want):
        wl = T.llvm(want)
        if v.ty == wl:
            return Val(wl, v.ref, want)
        a, b = int(v.ty[1:]), int(wl[1:])
        r = self.tmp()
        if a > b:
            self.w("%s = trunc %s %s to %s" % (r, v.ty, v.ref, wl))
        elif T.is_int(want) and T.is_signed(want):
            self.w("%s = sext %s %s to %s" % (r, v.ty, v.ref, wl))
        else:
            self.w("%s = zext %s %s to %s" % (r, v.ty, v.ref, wl))
        return Val(wl, r, want)

    def _expr(self, e):
        if isinstance(e, S.IntLit):
            ty = self.ty(e)
            return Val(T.llvm(ty), str(e.value), ty)
        if isinstance(e, S.BoolLit):
            return Val("i8", "1" if e.value else "0", T.BOOL)
        if isinstance(e, S.CharLit):
            return Val("i8", str(ord(e.value)), T.CHAR8)
        if isinstance(e, S.NilLit):
            return Val("i32", "0", T.NIL)
        if isinstance(e, S.NullLit):
            return Val("ptr", "null", T.Ptr(T.Prim("int8")))
        if isinstance(e, S.StringLit):
            return self.string_lit(e.value)

        if isinstance(e, S.Ident):
            slot, ty = self.lookup(e.name, e)
            r = self.tmp()
            self.w("%s = load %s, ptr %s" % (r, T.llvm(ty), slot))
            return Val(T.llvm(ty), r, ty)

        if isinstance(e, S.Cast):
            target = self.ck.resolve_type(e.target)
            v = self._expr(e.expr)
            return self.coerce(v, target)

        if isinstance(e, S.Unary):
            if e.op == "@":
                # `@x` yields a SECOND-CLASS borrow (D-004): it passes down the
                # call stack and never up. The seed lowers it as a plain address;
                # the escape analysis that enforces the rule is the compiler's
                # (cycle 0.5), not the seed's.
                addr, ty = self.addr_of(e.operand)
                return Val("ptr", addr, T.Ptr(ty))
            if e.op == "<-":
                addr = self._expr(e.operand)
                inner = self.ty(e.operand)
                elem = inner.elem if isinstance(inner, T.Ptr) else inner
                r = self.tmp()
                self.w("%s = load %s, ptr %s" % (r, T.llvm(elem), addr.ref))
                return Val(T.llvm(elem), r, elem)
            v = self._expr(e.operand)
            r = self.tmp()
            if e.op == "-":
                self.w("%s = sub %s 0, %s" % (r, v.ty, v.ref))
            elif e.op == "!":
                c = self.cond(e.operand)
                x = self.tmp()
                self.w("%s = xor i1 %s, true" % (x, c))
                self.w("%s = zext i1 %s to i8" % (r, x))
                return Val("i8", r, T.BOOL)
            elif e.op == "~":
                self.w("%s = xor %s %s, -1" % (r, v.ty, v.ref))
            else:
                raise EmitError("unary %r is not lowered at this rung" % e.op, e)
            return Val(v.ty, r, v.npk)

        if isinstance(e, S.Binary):
            return self.binary(e)

        if isinstance(e, S.ResultUnary):
            return self.result_unary(e)

        if isinstance(e, S.Call):
            return self.call(e)

        if isinstance(e, S.Field):
            return self.field(e)

        if isinstance(e, S.Index):
            return self.index(e)

        if isinstance(e, S.Builtin):
            if e.name == "size_of":
                # Ask LLVM for the size rather than computing layout by hand. A
                # hand-rolled size that disagrees with the one LLVM lays out
                # survives testing and corrupts memory later; this costs two
                # instructions that opt folds away.
                ll = T.llvm(e.size_type)
                g = self.tmp()
                self.w("%s = getelementptr %s, ptr null, i32 1" % (g, ll))
                r = self.tmp()
                self.w("%s = ptrtoint ptr %s to i64" % (r, g))
                return Val("i64", r, T.I64)
            raise EmitError("unsupported builtin #%s" % e.name, e)

        if isinstance(e, S.StructLit):
            ty = T.Named(e.name)
            ll = T.llvm(ty)
            fields = self.p.structs[e.name]
            cur = "undef"
            for i, (fname, _) in enumerate(fields.items()):
                src = next((v for n, v in e.fields if n == fname), None)
                if src is None:
                    raise EmitError("missing field %r" % fname, e)
                fv = self.expr(src, want=fields[fname])
                r = self.tmp()
                self.w("%s = insertvalue %s %s, %s %s, %d"
                       % (r, ll, cur, fv.ty, fv.ref, i))
                cur = r
            return Val(ll, cur, ty)

        if isinstance(e, S.ArrayLit):
            vals = [self._expr(x) for x in e.elems]
            elem = vals[0].npk if vals else T.I32
            ty = T.Array(elem, len(vals))
            ll = T.llvm(ty)
            cur = "undef"
            for i, v in enumerate(vals):
                r = self.tmp()
                self.w("%s = insertvalue %s %s, %s %s, %d"
                       % (r, ll, cur, v.ty, v.ref, i))
                cur = r
            return Val(ll, cur, ty)

        raise EmitError("cannot lower %s" % type(e).__name__, e)

    def string_lit(self, text):
        data = text.encode("utf-8").decode("latin-1")
        gname = "@.str.%d" % len(self.strings)
        self.strings.append((gname, data))
        a = self.tmp()
        self.w("%s = insertvalue { ptr, i64, i64 } undef, ptr %s, 0" % (a, gname))
        b = self.tmp()
        self.w("%s = insertvalue { ptr, i64, i64 } %s, i64 %d, 1" % (b, a, len(data)))
        c = self.tmp()
        self.w("%s = insertvalue { ptr, i64, i64 } %s, i64 %d, 2" % (c, b, len(data)))
        return Val("{ ptr, i64, i64 }", c, T.STRING)

    def binary(self, e):
        if e.op in ("&&", "||"):
            # Short-circuiting, without phi: a slot the two paths write.
            slot = self.tmp()
            self.w("%s = alloca i8" % slot)
            rhs, done = self.label("sc.rhs"), self.label("sc.done")
            l = self.cond(e.lhs)
            self.w("store i8 %s, ptr %s" % ("1" if e.op == "||" else "0", slot))
            if e.op == "&&":
                self.w("br i1 %s, label %%%s, label %%%s" % (l, rhs, done))
            else:
                self.w("br i1 %s, label %%%s, label %%%s" % (l, done, rhs))
            self.terminated = True
            self.start_block(rhs)
            r = self.cond(e.rhs)
            z = self.tmp()
            self.w("%s = zext i1 %s to i8" % (z, r))
            self.w("store i8 %s, ptr %s" % (z, slot))
            self.br(done)
            self.start_block(done)
            out = self.tmp()
            self.w("%s = load i8, ptr %s" % (out, slot))
            return Val("i8", out, T.BOOL)

        lt = self.ty(e.lhs) or T.I32
        lhs = self.expr(e.lhs)
        rhs = self.expr(e.rhs, want=lt if T.is_int(lt) else None)

        if e.op in ("==", "!=", "<", "<=", ">", ">="):
            signed = T.is_int(lt) and T.is_signed(lt)
            op = {"==": "eq", "!=": "ne",
                  "<": "slt" if signed else "ult", "<=": "sle" if signed else "ule",
                  ">": "sgt" if signed else "ugt", ">=": "sge" if signed else "uge"}[e.op]
            r = self.tmp()
            self.w("%s = icmp %s %s %s, %s" % (r, op, lhs.ty, lhs.ref, rhs.ref))
            z = self.tmp()
            self.w("%s = zext i1 %s to i8" % (z, r))
            return Val("i8", z, T.BOOL)

        return self.arith(e.op, lhs, rhs, lt)

    def arith(self, op, lhs, rhs, ty):
        signed = T.is_int(ty) and T.is_signed(ty)
        # Plain integers WRAP on overflow (D-037): no nsw/nuw, no check, no trap.
        name = {"+": "add", "-": "sub", "*": "mul",
                "/": "sdiv" if signed else "udiv",
                "%": "srem" if signed else "urem",
                "&": "and", "|": "or", "^": "xor",
                "<<": "shl", ">>": "ashr" if signed else "lshr"}[op]
        r = self.tmp()
        self.w("%s = %s %s %s, %s" % (r, name, lhs.ty, lhs.ref, rhs.ref))
        return Val(lhs.ty, r, ty)

    def result_unary(self, e):
        inner_ty = self.ty(e.operand)
        v = self._expr(e.operand)

        if e.op == "drop":
            return Val("i32", "0", T.NIL)

        if not isinstance(inner_ty, T.ResultT):
            return v          # already unwrapped; nothing to do

        if inner_ty.inner == T.NIL:
            value = Val("i32", "0", T.NIL)
        else:
            r = self.tmp()
            self.w("%s = extractvalue %s %s, 0" % (r, v.ty, v.ref))
            value = Val(T.llvm(inner_ty.inner), r, inner_ty.inner)

        if e.op == "raw":
            return value

        if e.op == "relay":
            # D-080: on error, return the SAME code from the enclosing function.
            # `defer` runs -- relay is a normal exit path, not a trap.
            idx = 0 if inner_ty.inner == T.NIL else 1
            code = self.tmp()
            self.w("%s = extractvalue %s %s, %d" % (code, v.ty, v.ref, idx))
            bad = self.tmp()
            self.w("%s = icmp ne i32 %s, 0" % (bad, code))
            err, ok = self.label("relay.err"), self.label("relay.ok")
            self.w("br i1 %s, label %%%s, label %%%s" % (bad, err, ok))
            self.terminated = True
            self.start_block(err)
            self.run_defers(self.all_defers())
            self.emit_return_err(code)
            self.start_block(ok)
            return value

        raise EmitError("unsupported Result operator %r" % e.op, e)

    def call(self, e):
        ctor = getattr(e, "enum_ctor", None)
        if ctor is not None:
            ename, _, tag, payload_ty = ctor
            if not T.ENUM_HAS_PAYLOAD.get(ename):
                return Val("i32", str(tag), T.Named(ename))
            ll = T.llvm(T.Named(ename))
            a = self.tmp()
            self.w("%s = insertvalue %s undef, i32 %d, 0" % (a, ll, tag))
            if payload_ty is not None and e.args:
                pv = self.expr(e.args[0], want=T.I64)
                b = self.tmp()
                self.w("%s = insertvalue %s %s, i64 %s, 1" % (b, ll, a, pv.ref))
            else:
                b = self.tmp()
                self.w("%s = insertvalue %s %s, i64 0, 1" % (b, ll, a))
            return Val(ll, b, T.Named(ename))

        if not isinstance(e.callee, S.Ident):
            raise EmitError("indirect calls are not lowered at this rung", e)
        name = e.callee.name

        if name in RUNTIME:
            s, ret, argtys = RUNTIME[name]
            args = []
            for a, at in zip(e.args, argtys):
                v = self._expr(a)
                args.append("%s %s" % (at, v.ref))
            if ret == "void":
                self.w("call void %s(%s)" % (s, ", ".join(args)))
                return Val("i32", "0", T.NIL)
            r = self.tmp()
            self.w("%s = call %s %s(%s)" % (r, ret, s, ", ".join(args)))
            npk = self.ty(e)
            return Val(ret, r, npk)

        fn = self.p.funcs.get(name)
        if fn is None:
            raise EmitError("unknown function %r" % name, e)
        ll_ret, ret_ty, bare = self.fn_sig(fn)
        args = []
        for a, p in zip(e.args, fn.params):
            pty = self.ck.resolve_type(p.type)
            v = self.expr(a, want=pty)
            args.append("%s %s" % (v.ty, v.ref))
        r = self.tmp()
        self.w("%s = call %s %s(%s)" % (r, ll_ret, sym(name), ", ".join(args)))
        return Val(ll_ret, r, ret_ty if bare else T.ResultT(ret_ty))

    def field(self, e):
        ctor = getattr(e, "enum_ctor", None)
        if ctor is not None:
            ename, _, tag, _ = ctor
            if not T.ENUM_HAS_PAYLOAD.get(ename):
                return Val("i32", str(tag), T.Named(ename))
            ll = T.llvm(T.Named(ename))
            a = self.tmp()
            self.w("%s = insertvalue %s undef, i32 %d, 0" % (a, ll, tag))
            b = self.tmp()
            self.w("%s = insertvalue %s %s, i64 0, 1" % (b, ll, a))
            return Val(ll, b, T.Named(ename))

        obj_ty = self.ty(e.obj)

        # `.` auto-dereferences a pointer (D-006), and a field of a place is
        # itself a place -- so take the address where one exists and load from
        # it. That covers `ptr.field`, which the value path cannot: there is no
        # aggregate value to extract from.
        if isinstance(obj_ty, T.Ptr) or self._addressable(e.obj):
            if not isinstance(obj_ty, T.ResultT):
                try:
                    addr, fty = self.addr_of(e)
                except EmitError:
                    pass
                else:
                    r = self.tmp()
                    self.w("%s = load %s, ptr %s" % (r, T.llvm(fty), addr))
                    return Val(T.llvm(fty), r, fty)

        obj = self._expr(e.obj)

        if isinstance(obj_ty, T.ResultT):
            idx = 0 if obj_ty.inner == T.NIL else 1
            if e.name == "error":
                r = self.tmp()
                self.w("%s = extractvalue %s %s, %d" % (r, obj.ty, obj.ref, idx))
                return Val("i32", r, T.TBB32)
            if e.name == "is_error":
                # Derived, never stored (D-069): is_error IS `error != 0`.
                r = self.tmp()
                self.w("%s = extractvalue %s %s, %d" % (r, obj.ty, obj.ref, idx))
                c = self.tmp()
                self.w("%s = icmp ne i32 %s, 0" % (c, r))
                z = self.tmp()
                self.w("%s = zext i1 %s to i8" % (z, c))
                return Val("i8", z, T.BOOL)
            if e.name == "value":
                r = self.tmp()
                self.w("%s = extractvalue %s %s, 0" % (r, obj.ty, obj.ref))
                return Val(T.llvm(obj_ty.inner), r, obj_ty.inner)

        if isinstance(obj_ty, T.Prim) and obj_ty.name == "string":
            idx = {"ptr": 0, "len": 1, "cap": 2}.get(e.name)
            if idx is None:
                raise EmitError("string has no field %r" % e.name, e)
            r = self.tmp()
            self.w("%s = extractvalue %s %s, %d" % (r, obj.ty, obj.ref, idx))
            fty = T.Ptr(T.Prim("int8")) if idx == 0 else T.I64
            return Val(T.llvm(fty), r, fty)

        if isinstance(obj_ty, T.Slice) and e.name == "len":
            r = self.tmp()
            self.w("%s = extractvalue %s %s, 1" % (r, obj.ty, obj.ref))
            return Val("i64", r, T.I64)

        if isinstance(obj_ty, T.Named) and obj_ty.name in self.p.structs:
            fields = self.p.structs[obj_ty.name]
            if e.name not in fields:
                raise EmitError("no field %r on %s" % (e.name, obj_ty.name), e)
            idx = list(fields).index(e.name)
            r = self.tmp()
            self.w("%s = extractvalue %s %s, %d" % (r, obj.ty, obj.ref, idx))
            return Val(T.llvm(fields[e.name]), r, fields[e.name])

        raise EmitError("cannot take field %r of %r" % (e.name, obj_ty), e)

    def index(self, e):
        obj_ty = self.ty(e.obj)

        if isinstance(e.index, S.Range):
            # Ranging an array or slice yields a slice: {ptr, i64} (D-070).
            lo = self.expr(e.index.lo, want=T.I64)
            hi = self.expr(e.index.hi, want=T.I64)
            base = self.base_ptr(e.obj, obj_ty)
            elem = obj_ty.elem
            gep = self.tmp()
            self.w("%s = getelementptr %s, ptr %s, i64 %s"
                   % (gep, T.llvm(elem), base, lo.ref))
            n = self.tmp()
            self.w("%s = sub i64 %s, %s" % (n, hi.ref, lo.ref))
            a = self.tmp()
            self.w("%s = insertvalue { ptr, i64 } undef, ptr %s, 0" % (a, gep))
            b = self.tmp()
            self.w("%s = insertvalue { ptr, i64 } %s, i64 %s, 1" % (b, a, n))
            return Val("{ ptr, i64 }", b, T.Slice(elem))

        gep, elem = self.addr_of(e)
        r = self.tmp()
        self.w("%s = load %s, ptr %s" % (r, T.llvm(elem), gep))
        return Val(T.llvm(elem), r, elem)

    def base_ptr(self, obj_node, obj_ty):
        """The data pointer of an array place or a slice value."""
        if isinstance(obj_ty, T.Slice):
            v = self._expr(obj_node)
            r = self.tmp()
            self.w("%s = extractvalue %s %s, 0" % (r, v.ty, v.ref))
            return r
        if isinstance(obj_ty, T.Array):
            base, _ = self.addr_of(obj_node)
            return base        # an alloca of [N x T] is already the element base
        raise EmitError("cannot index %r at this rung" % obj_ty, obj_node)


def emit_module(program, checker, module_id="npk"):
    return Emitter(program, checker).emit(module_id)
