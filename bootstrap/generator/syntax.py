"""AST node set for the Nitpick bootstrap seed.

THROWAWAY (D-085). Mirrors meta/specs/AST_REFERENCE.md, restricted to subset 1
plus the constructs the parser must ACCEPT so the checker can reject them with
NITPICK-RUNG-001 -- the parser never restricts (D-085).

The seed's AST uses ordinary object references. The real compiler's AST uses
indices into concrete node arrays (SUBSET_1.md section 1.2), which is a better
representation and not one the seed needs.
"""


class Node:
    __slots__ = ("line", "col", "path")

    def _at(self, tok):
        self.line, self.col, self.path = tok.line, tok.col, tok.path
        return self

    def __repr__(self):
        fields = []
        for cls in type(self).__mro__:
            for s in getattr(cls, "__slots__", ()):
                if s in ("line", "col", "path"):
                    continue
                fields.append("%s=%r" % (s, getattr(self, s, None)))
        return "%s(%s)" % (type(self).__name__, ", ".join(fields))


def _mk(name, *slots):
    ns = {"__slots__": slots}

    def __init__(self, *args, **kw):
        for s, a in zip(slots, args):
            setattr(self, s, a)
        for k, v in kw.items():
            setattr(self, k, v)
    ns["__init__"] = __init__
    return type(name, (Node,), ns)


# --- types -------------------------------------------------------------------

NamedType   = _mk("NamedType", "name", "generic_args")   # int32, Result<T>, Foo<A,B>
PointerType = _mk("PointerType", "elem")                 # T->
SliceType   = _mk("SliceType", "elem")                   # T[]
ArrayType   = _mk("ArrayType", "elem", "size")           # T[N]
QualType    = _mk("QualType", "qual", "inner")           # wild T, stack T, fixed T

# --- declarations ------------------------------------------------------------

Module      = _mk("Module", "items", "path")
ImportDecl  = _mk("ImportDecl", "target", "kind")        # use "p.npk".*;
FuncDecl    = _mk("FuncDecl", "name", "visibility", "modifiers", "generics",
                  "params", "ret", "body")
ParamDecl   = _mk("ParamDecl", "type", "name")
StructDecl  = _mk("StructDecl", "name", "visibility", "generics", "fields")
FieldDecl   = _mk("FieldDecl", "type", "name")
EnumDecl    = _mk("EnumDecl", "name", "visibility", "generics", "variants")
EnumVariant = _mk("EnumVariant", "name", "payload", "value")
TraitDecl   = _mk("TraitDecl", "name", "visibility", "items")     # outside subset 1
ImplDecl    = _mk("ImplDecl", "trait_name", "type_name", "items")  # outside subset 1
GlobalDecl  = _mk("GlobalDecl", "name", "visibility", "quals", "type", "init")

# --- statements --------------------------------------------------------------

Block       = _mk("Block", "stmts")
VarDecl     = _mk("VarDecl", "quals", "type", "name", "init")
Assign      = _mk("Assign", "target", "op", "value")
ExprStmt    = _mk("ExprStmt", "expr")
If          = _mk("If", "cond", "then_block", "else_branch")
While       = _mk("While", "label", "cond", "body")
For         = _mk("For", "label", "binding", "iterable", "body")   # outside subset 1
Loop        = _mk("Loop", "label", "args", "body")                 # outside subset 1
Till        = _mk("Till", "label", "args", "body")                 # outside subset 1
Pick        = _mk("Pick", "selector", "arms")
PickArm     = _mk("PickArm", "label", "pattern", "body")
Break       = _mk("Break", "label")
Continue    = _mk("Continue", "label")
Pass        = _mk("Pass", "value")
Fail        = _mk("Fail", "error")
Return      = _mk("Return", "value")
Exit        = _mk("Exit", "code")
Trap        = _mk("Trap", "error")            # !!! code;
Defer       = _mk("Defer", "body")
Discard     = _mk("Discard", "expr")
Fall        = _mk("Fall", "label")

# --- patterns ----------------------------------------------------------------

WildcardPat = _mk("WildcardPat")                       # (*)
LiteralPat  = _mk("LiteralPat", "expr")                # (0i32)
VariantPat  = _mk("VariantPat", "path", "bindings")    # (Expr.IntLit(v))
StructPat   = _mk("StructPat", "name", "fields")       # (Point { x, y })

# --- expressions -------------------------------------------------------------

IntLit      = _mk("IntLit", "value", "width")
FloatLit    = _mk("FloatLit", "text", "width")   # text only; never evaluated
StringLit   = _mk("StringLit", "value")
CharLit     = _mk("CharLit", "value")
BoolLit     = _mk("BoolLit", "value")
NilLit      = _mk("NilLit")
NullLit     = _mk("NullLit")
Ident       = _mk("Ident", "name")
Unary       = _mk("Unary", "op", "operand")
Binary      = _mk("Binary", "op", "lhs", "rhs")
ResultUnary = _mk("ResultUnary", "op", "operand")   # raw / drop / relay / await
Cast        = _mk("Cast", "expr", "target", "unchecked")
Call        = _mk("Call", "callee", "generic_args", "args")
Field       = _mk("Field", "obj", "name")
Index       = _mk("Index", "obj", "index")
StructLit   = _mk("StructLit", "name", "fields")
ArrayLit    = _mk("ArrayLit", "elems")
Range       = _mk("Range", "lo", "hi", "exclusive")
Pipeline    = _mk("Pipeline", "op", "lhs", "rhs")
SafeUnwrap  = _mk("SafeUnwrap", "expr", "default")      # e ? d
Emphatic    = _mk("Emphatic", "expr", "code")           # e ?! code
Comptime    = _mk("Comptime", "expr")                   # outside subset 1
Ternary     = _mk("Ternary", "cond", "a", "b")          # is (c) : a : b
