"""Type model and layout for the Nitpick bootstrap seed.

Named ntypes rather than types: Python imports its own `types` module during
interpreter startup, so a local types.py is silently shadowed by the cached
stdlib one no matter what sys.path says.

THROWAWAY (D-085) -- but the LAYOUTS here are not throwaway in effect. The seed's
output has to interoperate with stage 1's until the fixpoint closes, so every
layout below must match the specification exactly:

    Result<T>   {T, i32}            D-069 -- no is_error field; .is_error is
                                    derived as `error != 0`
    Result<NIL> {i32}               D-084 -- NIL is zero-sized
    slice T[]   {ptr, i64}          D-070 -- bounds live on the array type,
                                    which is why pointers stay one word
    string      {ptr, i64, i64}
    pointer     ptr                 D-038 -- thin, one word, no bounds metadata
    enum        {i32, i64}          SUBSET_1 -- single-word payloads only
    T[N]        [N x T]             value type, static bounds
    bool        i8                  stored as a byte, i1 only in conditions
"""

INT_TYPES = {
    "int8": ("i8", True), "int16": ("i16", True),
    "int32": ("i32", True), "int64": ("i64", True),
    "uint8": ("i8", False), "uint16": ("i16", False),
    "uint32": ("i32", False), "uint64": ("i64", False),
    # tbb32 is admitted as an ERROR-CODE type only (SUBSET_1 section 1.1):
    # assignment, comparison, passing -- never arithmetic. That single
    # restriction keeps ERR, stickiness and saturation out of the seed.
    "tbb32": ("i32", True),
}

# Outside subset 1. Named so the checker can say which rung enables them.
OUT_OF_SUBSET_TYPES = {
    "flt32": "0.9", "flt64": "0.9", "flt128": "0.9", "flt256": "0.9", "flt512": "0.9",
    "int128": "0.9", "int256": "0.9", "int512": "0.9",
    "int1024": "0.9", "int2048": "0.9", "int4096": "0.9",
    "uint128": "0.9", "uint256": "0.9", "uint512": "0.9",
    "uint1024": "0.9", "uint2048": "0.9", "uint4096": "0.9",
    "tbb8": "0.9", "tbb16": "0.9", "tbb64": "0.9", "tbb128": "0.9", "tbb256": "0.9",
    "frac8": "0.9", "frac16": "0.9", "frac32": "0.9", "frac64": "0.9",
    "tfp32": "0.9", "tfp64": "0.9", "tfp128": "0.9", "tfp256": "0.9",
    "dim256": "0.9",
    "char16": "0.9", "char32": "0.9",
    "Optional": "1.0", "arena": "1.0", "shared_arena": "1.0", "Handle": "1.0",
    "atomic": "1.1", "Future": "1.1", "simd": "0.9", "complex": "0.9",
    "matrix": "0.9", "tensor": "0.9", "vec2": "0.9", "vec3": "0.9",
    "dyn": "1.0", "any": "1.0", "buffer": "0.9",
}


class Type:
    __slots__ = ()

    def __eq__(self, other):
        return type(self) is type(other) and self._key() == other._key()

    def __hash__(self):
        return hash((type(self).__name__, self._key()))

    def _key(self):
        return ()


class Prim(Type):
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name

    def _key(self):
        return (self.name,)

    def __repr__(self):
        return self.name


class Named(Type):
    """A user struct or enum."""
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name

    def _key(self):
        return (self.name,)

    def __repr__(self):
        return self.name


class Ptr(Type):
    __slots__ = ("elem",)

    def __init__(self, elem):
        self.elem = elem

    def _key(self):
        return (self.elem,)

    def __repr__(self):
        return "%r->" % self.elem


class Slice(Type):
    __slots__ = ("elem",)

    def __init__(self, elem):
        self.elem = elem

    def _key(self):
        return (self.elem,)

    def __repr__(self):
        return "%r[]" % self.elem


class Array(Type):
    __slots__ = ("elem", "size")

    def __init__(self, elem, size):
        self.elem = elem
        self.size = size

    def _key(self):
        return (self.elem, self.size)

    def __repr__(self):
        return "%r[%d]" % (self.elem, self.size)


class ResultT(Type):
    __slots__ = ("inner",)

    def __init__(self, inner):
        self.inner = inner

    def _key(self):
        return (self.inner,)

    def __repr__(self):
        return "Result<%r>" % self.inner


NIL = Prim("NIL")
BOOL = Prim("bool")
CHAR8 = Prim("char8")
STRING = Prim("string")
TBB32 = Prim("tbb32")
I32 = Prim("int32")
I64 = Prim("int64")


def is_int(t):
    return isinstance(t, Prim) and t.name in INT_TYPES


def is_tbb(t):
    return isinstance(t, Prim) and t.name.startswith("tbb")


def int_bits(t):
    return int(INT_TYPES[t.name][0][1:])


def is_signed(t):
    return INT_TYPES[t.name][1]


def llvm(t):
    """The LLVM type text for a Nitpick type."""
    if isinstance(t, Prim):
        if t.name in INT_TYPES:
            return INT_TYPES[t.name][0]
        if t.name == "bool":
            return "i8"          # stored as a byte; i1 only inside conditions
        if t.name == "char8":
            return "i8"
        if t.name == "string":
            return "{ ptr, i64, i64 }"
        if t.name == "NIL":
            return "void"        # only ever appears as Result<NIL>, handled below
        raise KeyError(t.name)
    if isinstance(t, Ptr):
        return "ptr"             # thin, one word (D-038)
    if isinstance(t, Slice):
        return "{ ptr, i64 }"    # bounds on the array type (D-070)
    if isinstance(t, Array):
        return "[%d x %s]" % (t.size, llvm(t.elem))
    if isinstance(t, ResultT):
        if t.inner == NIL:
            return "{ i32 }"     # NIL is zero-sized (D-084)
        return "{ %s, i32 }" % llvm(t.inner)
    if isinstance(t, Named):
        return "%%%s" % t.name
    raise TypeError(repr(t))


def zero(t):
    """The LLVM zero value for a type, used for the value slot of a failed Result."""
    if isinstance(t, Prim) and (t.name in INT_TYPES or t.name in ("bool", "char8")):
        return "0"
    if isinstance(t, Ptr):
        return "null"
    return "zeroinitializer"
