// sema_helpers.cpp — C runtime helpers for the Aria self-hosted type checker
// Provides opaque-handle TypeSystem, SymbolTable, and TypeChecker state
// that the Aria-side type_checker.aria calls via extern funcs.

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

// ============================================================================
// Type Representation
// ============================================================================

enum class TK : int32_t {
    PRIMITIVE = 0,
    POINTER   = 1,
    ARRAY     = 2,
    FUNCTION  = 3,
    STRUCT    = 4,
    OPTIONAL  = 5,
    RESULT    = 6,
    VECTOR    = 7,
    GENERIC   = 8,
    UNKNOWN   = 9,
    ERROR     = 10,
};

struct SemaType {
    TK kind;
    std::string name;        // "int32", "string", struct name, etc.
    int32_t bit_width;       // for primitives
    bool is_signed;
    bool is_floating;
    bool is_tbb;
    bool is_exotic;
    bool nodiscard;

    // Pointer
    SemaType* pointee;       // for POINTER
    bool is_wild;
    bool is_erased;

    // Array
    SemaType* element;       // for ARRAY
    int32_t array_size;      // -1 = dynamic

    // Function
    std::vector<SemaType*> param_types;
    SemaType* return_type;
    bool is_async;

    // Struct
    struct Field {
        std::string name;
        SemaType* type;
        bool is_public;
    };
    std::vector<Field> fields;

    // Optional/Result
    SemaType* wrapped;       // for OPTIONAL and RESULT

    // Vector
    SemaType* component;     // for VECTOR
    int32_t dimension;

    SemaType() : kind(TK::ERROR), bit_width(0), is_signed(false),
                 is_floating(false), is_tbb(false), is_exotic(false),
                 nodiscard(false), pointee(nullptr), is_wild(false),
                 is_erased(false), element(nullptr), array_size(-1),
                 return_type(nullptr), is_async(false), wrapped(nullptr),
                 component(nullptr), dimension(0) {}
};

// ============================================================================
// Symbol Representation
// ============================================================================

enum class SK : int32_t {
    VARIABLE  = 0,
    FUNCTION  = 1,
    PARAMETER = 2,
    TYPE      = 3,
    TRAIT     = 4,
    MODULE    = 5,
    CONSTANT  = 6,
};

struct SemaSymbol {
    std::string name;
    SK kind;
    SemaType* type;
    int32_t line;
    int32_t col;
    bool is_public;
    bool is_mutable;
    bool is_initialized;
};

// ============================================================================
// Scope
// ============================================================================

enum class ScopeK : int32_t {
    GLOBAL   = 0,
    FUNCTION = 1,
    BLOCK    = 2,
    STRUCT   = 3,
    MODULE   = 4,
};

struct SemaScope {
    ScopeK kind;
    SemaScope* parent;
    std::string name;
    int32_t depth;
    std::unordered_map<std::string, SemaSymbol*> symbols;
    std::vector<SemaScope*> children;
};

// ============================================================================
// TypeChecker State (top-level opaque handle)
// ============================================================================

struct TypeCheckerState {
    // Type cache
    std::unordered_map<std::string, SemaType*> primitive_cache;
    std::vector<SemaType*> all_types; // owns all types

    // Symbol table
    SemaScope* root_scope;
    SemaScope* current_scope;
    std::vector<SemaScope*> all_scopes; // owns all scopes
    std::vector<SemaSymbol*> all_symbols; // owns all symbols

    // Errors
    std::vector<std::string> errors;

    // Current function context
    SemaType* current_func_return_type;
    SemaType* current_func_value_type;

    // Function context save stack (for nested function checking)
    struct FuncContext { SemaType* ret; SemaType* val; };
    std::vector<FuncContext> func_context_stack;

    ~TypeCheckerState() {
        for (auto* t : all_types) delete t;
        for (auto* s : all_scopes) delete s;
        for (auto* sym : all_symbols) delete sym;
    }
};

// ============================================================================
// Primitive Type Registration
// ============================================================================

static SemaType* make_primitive(TypeCheckerState* tc, const char* name,
                                int32_t bits, bool sgn, bool flt,
                                bool tbb, bool exotic) {
    auto* t = new SemaType();
    t->kind = TK::PRIMITIVE;
    t->name = name;
    t->bit_width = bits;
    t->is_signed = sgn;
    t->is_floating = flt;
    t->is_tbb = tbb;
    t->is_exotic = exotic;
    tc->all_types.push_back(t);
    tc->primitive_cache[name] = t;
    return t;
}

static void register_all_primitives(TypeCheckerState* tc) {
    // Signed integers
    make_primitive(tc, "int1",    1, true, false, false, false);
    make_primitive(tc, "int2",    2, true, false, false, false);
    make_primitive(tc, "int4",    4, true, false, false, false);
    make_primitive(tc, "int8",    8, true, false, false, false);
    make_primitive(tc, "int16",  16, true, false, false, false);
    make_primitive(tc, "int32",  32, true, false, false, false);
    make_primitive(tc, "int64",  64, true, false, false, false);
    make_primitive(tc, "int128",128, true, false, false, false);
    make_primitive(tc, "int256",256, true, false, false, false);
    make_primitive(tc, "int512",512, true, false, false, false);
    make_primitive(tc, "int1024",1024,true,false, false, false);
    make_primitive(tc, "int2048",2048,true,false, false, false);
    make_primitive(tc, "int4096",4096,true,false, false, false);

    // Unsigned integers
    make_primitive(tc, "uint1",    1, false, false, false, false);
    make_primitive(tc, "uint2",    2, false, false, false, false);
    make_primitive(tc, "uint4",    4, false, false, false, false);
    make_primitive(tc, "uint8",    8, false, false, false, false);
    make_primitive(tc, "uint16",  16, false, false, false, false);
    make_primitive(tc, "uint32",  32, false, false, false, false);
    make_primitive(tc, "uint64",  64, false, false, false, false);
    make_primitive(tc, "uint128",128, false, false, false, false);
    make_primitive(tc, "uint256",256, false, false, false, false);
    make_primitive(tc, "uint512",512, false, false, false, false);
    make_primitive(tc, "uint1024",1024,false,false,false, false);
    make_primitive(tc, "uint2048",2048,false,false,false, false);
    make_primitive(tc, "uint4096",4096,false,false,false, false);

    // Floating point
    make_primitive(tc, "flt32",  32, true, true, false, false);
    make_primitive(tc, "flt64",  64, true, true, false, false);
    make_primitive(tc, "flt128",128, true, true, false, false);
    make_primitive(tc, "flt256",256, true, true, false, false);
    make_primitive(tc, "flt512",512, true, true, false, false);

    // Fixed point
    make_primitive(tc, "fix256", 256, true, false, false, false);

    // TBB types
    make_primitive(tc, "tbb8",    8, true, false, true, false);
    make_primitive(tc, "tbb16",  16, true, false, true, false);
    make_primitive(tc, "tbb32",  32, true, false, true, false);
    make_primitive(tc, "tbb64",  64, true, false, true, false);

    // Frac types (stored as their component TBB width for bitWidth)
    make_primitive(tc, "frac8",   24, true, false, false, true);
    make_primitive(tc, "frac16",  48, true, false, false, true);
    make_primitive(tc, "frac32",  96, true, false, false, true);
    make_primitive(tc, "frac64", 192, true, false, false, true);

    // TFP types
    make_primitive(tc, "tfp32",  32, true, true, true, false);
    make_primitive(tc, "tfp64",  64, true, true, true, false);

    // Balanced ternary / nonary
    make_primitive(tc, "trit",    2, true, false, false, true);
    make_primitive(tc, "tryte",  16, true, false, false, true);
    make_primitive(tc, "nit",     4, true, false, false, true);
    make_primitive(tc, "nyte",   16, true, false, false, true);

    // Non-numeric primitives
    make_primitive(tc, "bool",    1, false, false, false, false);
    make_primitive(tc, "string",  0, false, false, false, false);
    make_primitive(tc, "void",    0, false, false, false, false);
    make_primitive(tc, "NIL",     0, false, false, false, false);
    make_primitive(tc, "dyn",     0, false, false, false, false);
    make_primitive(tc, "obj",     0, false, false, false, false);
}

// AriaString structure for returning strings to Aria
struct AriaString {
    char* data;
    int64_t length;
};

static AriaString* make_aria_string(const std::string& s) {
    auto* as = (AriaString*)malloc(sizeof(AriaString));
    as->data = (char*)malloc(s.size() + 1);
    memcpy(as->data, s.c_str(), s.size() + 1);
    as->length = (int64_t)s.size();
    return as;
}

// ============================================================================
// Extern C API — TypeChecker State Management
// ============================================================================

extern "C" {

// --- Debug helper ---
void tc_debug_msg(const char* msg) {
    fprintf(stderr, "[TC_DEBUG] %s\n", msg);
}

void tc_debug_int(const char* label, int32_t val) {
    fprintf(stderr, "[TC_DEBUG] %s = %d\n", label, val);
}

void tc_debug_ptr(const char* label, void* ptr) {
    fprintf(stderr, "[TC_DEBUG] %s = %p\n", label, ptr);
}

// --- TypeChecker lifecycle ---

void* tc_new() {
    auto* tc = new TypeCheckerState();
    register_all_primitives(tc);

    // Create root scope
    auto* root = new SemaScope();
    root->kind = ScopeK::GLOBAL;
    root->parent = nullptr;
    root->depth = 0;
    tc->root_scope = root;
    tc->current_scope = root;
    tc->all_scopes.push_back(root);

    tc->current_func_return_type = nullptr;
    tc->current_func_value_type = nullptr;

    return tc;
}

void tc_free(void* state) {
    delete (TypeCheckerState*)state;
}

// --- Error tracking ---

void tc_add_error(void* state, const char* msg) {
    auto* tc = (TypeCheckerState*)state;
    tc->errors.push_back(msg);
}

void tc_add_error_at(void* state, const char* msg, int32_t line, int32_t col) {
    auto* tc = (TypeCheckerState*)state;
    std::string full = "Line " + std::to_string(line) + ", Col " +
                       std::to_string(col) + ": " + msg;
    tc->errors.push_back(full);
}

int64_t tc_error_count(void* state) {
    return (int64_t)((TypeCheckerState*)state)->errors.size();
}

AriaString* tc_get_error(void* state, int64_t idx) {
    auto* tc = (TypeCheckerState*)state;
    if (idx < 0 || idx >= (int64_t)tc->errors.size())
        return make_aria_string("");
    return make_aria_string(tc->errors[idx]);
}

// --- Type creation / retrieval ---

void* tc_get_primitive(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->primitive_cache.find(name);
    if (it != tc->primitive_cache.end()) return it->second;
    return nullptr;
}

void* tc_get_error_type(void* state) {
    auto* tc = (TypeCheckerState*)state;
    // Return a singleton error type
    static SemaType* err = nullptr;
    if (!err) {
        err = new SemaType();
        err->kind = TK::ERROR;
        err->name = "<error>";
        tc->all_types.push_back(err);
    }
    return err;
}

void* tc_get_unknown_type(void* state) {
    auto* tc = (TypeCheckerState*)state;
    static SemaType* unk = nullptr;
    if (!unk) {
        unk = new SemaType();
        unk->kind = TK::UNKNOWN;
        unk->name = "unknown";
        tc->all_types.push_back(unk);
    }
    return unk;
}

void* tc_make_pointer_type(void* state, void* pointee, int32_t is_wild) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::POINTER;
    t->pointee = (SemaType*)pointee;
    t->is_wild = is_wild != 0;
    t->is_erased = false;
    auto* base = (SemaType*)pointee;
    t->name = (is_wild ? "wild " : "") + (base ? base->name : "?") + "->";
    tc->all_types.push_back(t);
    return t;
}

void* tc_make_erased_pointer(void* state) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::POINTER;
    t->pointee = nullptr;
    t->is_wild = false;
    t->is_erased = true;
    t->name = "?->";
    tc->all_types.push_back(t);
    return t;
}

void* tc_make_array_type(void* state, void* elem, int32_t size) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::ARRAY;
    t->element = (SemaType*)elem;
    t->array_size = size;
    auto* e = (SemaType*)elem;
    t->name = e->name + "[]";
    tc->all_types.push_back(t);
    return t;
}

void* tc_make_optional_type(void* state, void* wrapped) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::OPTIONAL;
    t->wrapped = (SemaType*)wrapped;
    auto* w = (SemaType*)wrapped;
    t->name = w->name + "?";
    t->nodiscard = true;
    tc->all_types.push_back(t);
    return t;
}

void* tc_make_result_type(void* state, void* value_type) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::RESULT;
    t->wrapped = (SemaType*)value_type;
    auto* v = (SemaType*)value_type;
    t->name = "Result<" + v->name + ">";
    t->nodiscard = true;
    tc->all_types.push_back(t);
    return t;
}

void* tc_make_func_type(void* state, int32_t /*param_count*/) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::FUNCTION;
    t->return_type = nullptr;
    t->is_async = false;
    // param_types will be added with tc_func_type_add_param
    tc->all_types.push_back(t);
    return t;
}

void tc_func_type_add_param(void* func_type, void* param_type) {
    auto* ft = (SemaType*)func_type;
    fprintf(stderr, "[TC_DEBUG] add_param: ptr=%p\n", param_type);
    ft->param_types.push_back((SemaType*)param_type);
}

void tc_func_type_set_return(void* func_type, void* ret_type) {
    auto* ft = (SemaType*)func_type;
    ft->return_type = (SemaType*)ret_type;
    // Build name with safety checks
    std::string name = "func(";
    for (size_t i = 0; i < ft->param_types.size(); i++) {
        if (i > 0) name += ", ";
        if (!ft->param_types[i]) {
            name += "<null>";
        } else {
            fprintf(stderr, "[TC_DEBUG] param[%zu] ptr=%p name_ptr=%p\n", i, (void*)ft->param_types[i], (void*)ft->param_types[i]->name.c_str());
            try {
                name += ft->param_types[i]->name;
            } catch (...) {
                name += "<corrupt>";
                fprintf(stderr, "[TC_DEBUG] param[%zu] name is CORRUPT\n", i);
            }
        }
    }
    name += ") -> ";
    if (!ft->return_type) {
        name += "<null>";
    } else {
        fprintf(stderr, "[TC_DEBUG] ret_type ptr=%p\n", (void*)ft->return_type);
        try {
            name += ft->return_type->name;
        } catch (...) {
            name += "<corrupt>";
            fprintf(stderr, "[TC_DEBUG] ret_type name is CORRUPT\n");
        }
    }
    ft->name = name;
}

void* tc_make_struct_type(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::STRUCT;
    t->name = name;
    tc->all_types.push_back(t);
    // Also register in primitive cache for lookup-by-name
    tc->primitive_cache[name] = t;
    return t;
}

void tc_struct_add_field(void* struct_type, const char* fname,
                         void* ftype, int32_t is_public) {
    auto* st = (SemaType*)struct_type;
    SemaType::Field f;
    f.name = fname;
    f.type = (SemaType*)ftype;
    f.is_public = is_public != 0;
    st->fields.push_back(f);
}

void* tc_make_vector_type(void* state, void* comp_type, int32_t dim) {
    auto* tc = (TypeCheckerState*)state;
    auto* t = new SemaType();
    t->kind = TK::VECTOR;
    t->component = (SemaType*)comp_type;
    t->dimension = dim;
    t->name = "vec" + std::to_string(dim);
    tc->all_types.push_back(t);
    return t;
}

// --- Type queries ---

int32_t tc_type_kind(void* type_handle) {
    if (!type_handle) return (int32_t)TK::ERROR;
    return (int32_t)((SemaType*)type_handle)->kind;
}

AriaString* tc_type_name(void* type_handle) {
    if (!type_handle) return make_aria_string("<null>");
    return make_aria_string(((SemaType*)type_handle)->name);
}

int32_t tc_type_bit_width(void* type_handle) {
    if (!type_handle) return 0;
    return ((SemaType*)type_handle)->bit_width;
}

int32_t tc_type_is_signed(void* type_handle) {
    if (!type_handle) return 0;
    return ((SemaType*)type_handle)->is_signed ? 1 : 0;
}

int32_t tc_type_is_floating(void* type_handle) {
    if (!type_handle) return 0;
    return ((SemaType*)type_handle)->is_floating ? 1 : 0;
}

int32_t tc_type_is_tbb(void* type_handle) {
    if (!type_handle) return 0;
    return ((SemaType*)type_handle)->is_tbb ? 1 : 0;
}

int32_t tc_type_is_exotic(void* type_handle) {
    if (!type_handle) return 0;
    return ((SemaType*)type_handle)->is_exotic ? 1 : 0;
}

int32_t tc_type_is_numeric(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::PRIMITIVE) return 0;
    return (t->bit_width > 0 && t->name != "bool") ? 1 : 0;
}

int32_t tc_type_is_integer(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::PRIMITIVE) return 0;
    return (t->bit_width > 0 && !t->is_floating && t->name != "bool") ? 1 : 0;
}

int32_t tc_types_equal(void* a, void* b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    auto* ta = (SemaType*)a;
    auto* tb = (SemaType*)b;
    if (ta->kind != tb->kind) return 0;

    switch (ta->kind) {
        case TK::PRIMITIVE: return ta->name == tb->name ? 1 : 0;
        case TK::POINTER:
            if (ta->is_erased && tb->is_erased) return 1;
            return tc_types_equal(ta->pointee, tb->pointee);
        case TK::ARRAY:
            return (tc_types_equal(ta->element, tb->element) &&
                    ta->array_size == tb->array_size) ? 1 : 0;
        case TK::OPTIONAL:
        case TK::RESULT:
            return tc_types_equal(ta->wrapped, tb->wrapped);
        case TK::STRUCT: return ta->name == tb->name ? 1 : 0;
        case TK::VECTOR:
            return (tc_types_equal(ta->component, tb->component) &&
                    ta->dimension == tb->dimension) ? 1 : 0;
        case TK::FUNCTION: {
            if (!tc_types_equal(ta->return_type, tb->return_type)) return 0;
            if (ta->param_types.size() != tb->param_types.size()) return 0;
            for (size_t i = 0; i < ta->param_types.size(); i++) {
                if (!tc_types_equal(ta->param_types[i], tb->param_types[i])) return 0;
            }
            return 1;
        }
        default: return 0;
    }
}

// Type coercion — can source be implicitly converted to target?
int32_t tc_can_coerce(void* source, void* target) {
    if (tc_types_equal(source, target)) return 1;
    if (!source || !target) return 0;
    auto* s = (SemaType*)source;
    auto* t = (SemaType*)target;

    // Pointer coercion: any pointer -> erased pointer (?->)
    if (s->kind == TK::POINTER && t->kind == TK::POINTER && t->is_erased)
        return 1;

    // Same numeric types with same signedness and width
    if (s->kind == TK::PRIMITIVE && t->kind == TK::PRIMITIVE) {
        // Integer widening (int8 -> int16 -> int32 -> int64)
        if (s->is_signed && t->is_signed && !s->is_floating && !t->is_floating &&
            s->bit_width > 0 && t->bit_width > 0 && s->bit_width <= t->bit_width)
            return 1;
        // Unsigned widening
        if (!s->is_signed && !t->is_signed && !s->is_floating && !t->is_floating &&
            s->bit_width > 0 && t->bit_width > 0 && s->bit_width <= t->bit_width)
            return 1;
        // Float widening (flt32 -> flt64)
        if (s->is_floating && t->is_floating &&
            s->bit_width > 0 && t->bit_width > 0 && s->bit_width <= t->bit_width)
            return 1;
    }

    // Result<T> where T coerces (for return type checking)
    if (s->kind == TK::RESULT && t->kind == TK::RESULT) {
        return tc_can_coerce(s->wrapped, t->wrapped);
    }

    return 0;
}

// --- Pointer type queries ---

void* tc_pointer_pointee(void* type_handle) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::POINTER) return nullptr;
    return t->pointee;
}

int32_t tc_pointer_is_wild(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    return (t->kind == TK::POINTER && t->is_wild) ? 1 : 0;
}

// --- Array type queries ---

void* tc_array_element(void* type_handle) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::ARRAY) return nullptr;
    return t->element;
}

int32_t tc_array_size(void* type_handle) {
    if (!type_handle) return -1;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::ARRAY) return -1;
    return t->array_size;
}

// --- Function type queries ---

void* tc_func_return_type(void* type_handle) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::FUNCTION) return nullptr;
    return t->return_type;
}

int32_t tc_func_param_count(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::FUNCTION) return 0;
    return (int32_t)t->param_types.size();
}

void* tc_func_param_type(void* type_handle, int32_t idx) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::FUNCTION || idx < 0 || idx >= (int32_t)t->param_types.size())
        return nullptr;
    return t->param_types[idx];
}

// --- Struct type queries ---

int32_t tc_struct_field_count(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::STRUCT) return 0;
    return (int32_t)t->fields.size();
}

AriaString* tc_struct_field_name(void* type_handle, int32_t idx) {
    if (!type_handle) return make_aria_string("");
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::STRUCT || idx < 0 || idx >= (int32_t)t->fields.size())
        return make_aria_string("");
    return make_aria_string(t->fields[idx].name);
}

void* tc_struct_field_type(void* type_handle, int32_t idx) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::STRUCT || idx < 0 || idx >= (int32_t)t->fields.size())
        return nullptr;
    return t->fields[idx].type;
}

void* tc_struct_field_by_name(void* type_handle, const char* fname) {
    if (!type_handle || !fname) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::STRUCT) return nullptr;
    for (const auto& f : t->fields) {
        if (f.name == fname) return f.type;
    }
    return nullptr;
}

// --- Optional/Result wrapped type ---

void* tc_wrapped_type(void* type_handle) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind == TK::OPTIONAL || t->kind == TK::RESULT) return t->wrapped;
    return nullptr;
}

// --- Vector type queries ---

void* tc_vector_component(void* type_handle) {
    if (!type_handle) return nullptr;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::VECTOR) return nullptr;
    return t->component;
}

int32_t tc_vector_dimension(void* type_handle) {
    if (!type_handle) return 0;
    auto* t = (SemaType*)type_handle;
    if (t->kind != TK::VECTOR) return 0;
    return t->dimension;
}

// ============================================================================
// Scope / Symbol Table Operations
// ============================================================================

void tc_push_scope(void* state, int32_t scope_kind, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto* s = new SemaScope();
    s->kind = (ScopeK)scope_kind;
    s->parent = tc->current_scope;
    s->name = name ? name : "";
    s->depth = tc->current_scope->depth + 1;
    tc->current_scope->children.push_back(s);
    tc->all_scopes.push_back(s);
    tc->current_scope = s;
}

void tc_pop_scope(void* state) {
    auto* tc = (TypeCheckerState*)state;
    if (tc->current_scope->parent) {
        tc->current_scope = tc->current_scope->parent;
    }
}

int32_t tc_define_symbol(void* state, const char* name, int32_t kind,
                          void* type_handle, int32_t line, int32_t col) {
    auto* tc = (TypeCheckerState*)state;
    // Check for duplicate in current scope
    auto it = tc->current_scope->symbols.find(name);
    if (it != tc->current_scope->symbols.end()) {
        return 0; // already defined
    }
    auto* sym = new SemaSymbol();
    sym->name = name;
    sym->kind = (SK)kind;
    sym->type = (SemaType*)type_handle;
    sym->line = line;
    sym->col = col;
    sym->is_public = false;
    sym->is_mutable = true;
    sym->is_initialized = false;
    tc->current_scope->symbols[name] = sym;
    tc->all_symbols.push_back(sym);
    return 1; // success
}

void tc_symbol_set_public(void* state, const char* name, int32_t is_pub) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->current_scope->symbols.find(name);
    if (it != tc->current_scope->symbols.end()) {
        it->second->is_public = is_pub != 0;
    }
}

void tc_symbol_set_mutable(void* state, const char* name, int32_t is_mut) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->current_scope->symbols.find(name);
    if (it != tc->current_scope->symbols.end()) {
        it->second->is_mutable = is_mut != 0;
    }
}

void tc_symbol_set_initialized(void* state, const char* name, int32_t is_init) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->current_scope->symbols.find(name);
    if (it != tc->current_scope->symbols.end()) {
        it->second->is_initialized = is_init != 0;
    }
}

// Lookup symbol in current scope only
void* tc_lookup_symbol(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->current_scope->symbols.find(name);
    if (it != tc->current_scope->symbols.end()) return it->second;
    return nullptr;
}

// Resolve symbol by walking up scope chain (current + parents)
void* tc_resolve_symbol(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    SemaScope* scope = tc->current_scope;
    while (scope) {
        auto it = scope->symbols.find(name);
        if (it != scope->symbols.end()) return it->second;
        scope = scope->parent;
    }
    return nullptr;
}

// Get symbol's type
void* tc_symbol_type(void* sym_handle) {
    if (!sym_handle) return nullptr;
    return ((SemaSymbol*)sym_handle)->type;
}

// Get symbol kind
int32_t tc_symbol_kind(void* sym_handle) {
    if (!sym_handle) return -1;
    return (int32_t)((SemaSymbol*)sym_handle)->kind;
}

AriaString* tc_symbol_name(void* sym_handle) {
    if (!sym_handle) return make_aria_string("");
    return make_aria_string(((SemaSymbol*)sym_handle)->name);
}

int32_t tc_symbol_is_mutable(void* sym_handle) {
    if (!sym_handle) return 0;
    return ((SemaSymbol*)sym_handle)->is_mutable ? 1 : 0;
}

int32_t tc_symbol_is_initialized(void* sym_handle) {
    if (!sym_handle) return 0;
    return ((SemaSymbol*)sym_handle)->is_initialized ? 1 : 0;
}

// --- Function context ---

void tc_set_func_return_type(void* state, void* type_handle) {
    ((TypeCheckerState*)state)->current_func_return_type = (SemaType*)type_handle;
}

void* tc_get_func_return_type(void* state) {
    return ((TypeCheckerState*)state)->current_func_return_type;
}

void tc_set_func_value_type(void* state, void* type_handle) {
    ((TypeCheckerState*)state)->current_func_value_type = (SemaType*)type_handle;
}

void* tc_get_func_value_type(void* state) {
    return ((TypeCheckerState*)state)->current_func_value_type;
}

// ============================================================================
// Binary Operator Type Checking Helper
// ============================================================================

// Returns the result type for a binary operation, or nullptr on error.
// op_token: token type of the operator
void* tc_check_binary_op(void* state, int32_t op_token,
                          void* left_type, void* right_type) {
    (void)state; // used only via tc_* calls
    auto* lt = (SemaType*)left_type;
    auto* rt = (SemaType*)right_type;

    if (!lt || !rt || lt->kind == TK::ERROR || rt->kind == TK::ERROR)
        return tc_get_error_type(state);

    // Token type constants (must match lexer.aria token ordering)
    // Arithmetic: + - * / %
    // Comparison: == != < <= > >=
    // Logical: && ||
    // Bitwise: & | ^ << >>

    // Arithmetic operators
    const int TK_PLUS = 124, TK_MINUS = 125, TK_STAR = 126,
              TK_SLASH = 127, TK_PERCENT = 128;
    // Comparison
    const int TK_EQUAL_EQUAL = 137, TK_BANG_EQUAL = 138,
              TK_LESS = 139, TK_LESS_EQUAL = 140,
              TK_GREATER = 141, TK_GREATER_EQUAL = 142;
    // Logical
    const int TK_AMP_AMP = 144, TK_PIPE_PIPE = 145;
    // Bitwise
    const int TK_AMP = 149, TK_PIPE = 150, TK_CARET = 151,
              TK_LEFT_SHIFT = 153, TK_RIGHT_SHIFT = 154;

    bool is_arith = (op_token == TK_PLUS || op_token == TK_MINUS ||
                     op_token == TK_STAR || op_token == TK_SLASH ||
                     op_token == TK_PERCENT);
    bool is_comparison = (op_token == TK_EQUAL_EQUAL || op_token == TK_BANG_EQUAL ||
                          op_token == TK_LESS || op_token == TK_LESS_EQUAL ||
                          op_token == TK_GREATER || op_token == TK_GREATER_EQUAL);
    bool is_logical = (op_token == TK_AMP_AMP || op_token == TK_PIPE_PIPE);
    bool is_bitwise = (op_token == TK_AMP || op_token == TK_PIPE ||
                       op_token == TK_CARET || op_token == TK_LEFT_SHIFT ||
                       op_token == TK_RIGHT_SHIFT);

    // String concatenation: string + string -> string
    if (op_token == TK_PLUS && lt->name == "string" && rt->name == "string") {
        return tc_get_primitive(state, "string");
    }

    if (is_arith) {
        if (lt->kind != TK::PRIMITIVE || rt->kind != TK::PRIMITIVE) {
            tc_add_error(state, "Arithmetic operators require numeric types");
            return tc_get_error_type(state);
        }
        if (!tc_type_is_numeric(lt) || !tc_type_is_numeric(rt)) {
            tc_add_error(state, "Arithmetic operators require numeric types");
            return tc_get_error_type(state);
        }
        // Types must match (Zero Implicit Conversion)
        if (!tc_types_equal(lt, rt)) {
            tc_add_error(state, "Arithmetic operands must have matching types");
            return tc_get_error_type(state);
        }
        return lt; // same type
    }

    if (is_comparison) {
        if (!tc_types_equal(lt, rt)) {
            tc_add_error(state, "Comparison operands must have matching types");
            return tc_get_error_type(state);
        }
        return tc_get_primitive(state, "bool");
    }

    if (is_logical) {
        // Strict bool (no truthiness)
        auto* bool_t = (SemaType*)tc_get_primitive(state, "bool");
        if (!tc_types_equal(lt, bool_t) || !tc_types_equal(rt, bool_t)) {
            tc_add_error(state, "Logical operators require bool operands");
            return tc_get_error_type(state);
        }
        return bool_t;
    }

    if (is_bitwise) {
        // UNSIGNED MANDATE: bitwise ops require unsigned types
        if (lt->kind != TK::PRIMITIVE || rt->kind != TK::PRIMITIVE) {
            tc_add_error(state, "Bitwise operators require unsigned integer types");
            return tc_get_error_type(state);
        }
        if (lt->is_signed || rt->is_signed || lt->is_floating || rt->is_floating) {
            tc_add_error(state, "Bitwise operators require unsigned integer types");
            return tc_get_error_type(state);
        }
        if (!tc_types_equal(lt, rt)) {
            tc_add_error(state, "Bitwise operands must have matching types");
            return tc_get_error_type(state);
        }
        return lt;
    }

    tc_add_error(state, "Unknown binary operator");
    return tc_get_error_type(state);
}

// ============================================================================
// Unary Operator Type Checking Helper
// ============================================================================

void* tc_check_unary_op(void* state, int32_t op_token, void* operand_type) {
    auto* ot = (SemaType*)operand_type;

    if (!ot || ot->kind == TK::ERROR) return tc_get_error_type(state);

    const int TK_MINUS = 125, TK_BANG = 146, TK_TILDE = 152;

    if (op_token == TK_MINUS) {
        // Negation: must be signed numeric
        if (ot->kind == TK::PRIMITIVE && ot->bit_width > 0 && ot->is_signed)
            return ot;
        tc_add_error(state, "Negation requires signed numeric type");
        return tc_get_error_type(state);
    }

    if (op_token == TK_BANG) {
        // Logical NOT: must be bool
        if (ot->name == "bool") return ot;
        tc_add_error(state, "Logical NOT requires bool type");
        return tc_get_error_type(state);
    }

    if (op_token == TK_TILDE) {
        // Bitwise NOT: must be unsigned integer
        if (ot->kind == TK::PRIMITIVE && !ot->is_signed && !ot->is_floating && ot->bit_width > 0)
            return ot;
        tc_add_error(state, "Bitwise NOT requires unsigned integer type");
        return tc_get_error_type(state);
    }

    return ot; // default: same type
}

// ============================================================================
// Type suffix → type name mapping (for literal type inference)
// ============================================================================

AriaString* tc_suffix_to_type_name(const char* suffix) {
    if (!suffix || !suffix[0]) return make_aria_string("int32"); // default int

    std::string s(suffix);
    // u8 -> uint8, u16 -> uint16, etc.
    if (s[0] == 'u' && s.size() >= 2 && std::isdigit(s[1]))
        return make_aria_string("uint" + s.substr(1));
    // i8 -> int8, i16 -> int16, etc.
    if (s[0] == 'i' && s.size() >= 2 && std::isdigit(s[1]))
        return make_aria_string("int" + s.substr(1));
    // f32 -> flt32, f64 -> flt64
    if (s[0] == 'f' && s.size() >= 2 && std::isdigit(s[1]))
        return make_aria_string("flt" + s.substr(1));
    // tbb, fix — already correct
    if (s.substr(0, 3) == "tbb" || s.substr(0, 3) == "fix")
        return make_aria_string(s);

    return make_aria_string(s);
}

// ============================================================================
// Token ID → type name mapping (for self-hosted parser literal nodes)
// The self-hosted parser stores the token type in flags, not suffix in str2
// ============================================================================

AriaString* tc_token_to_type_name(int32_t token_id) {
    // Plain untyped literals — return empty string (no explicit type)
    // TK_INTEGER=184, TK_FLOAT=185
    if (token_id == 184 || token_id == 185) return make_aria_string("");

    switch (token_id) {
        // Unsigned integers (186-195)
        case 186: return make_aria_string("uint8");
        case 187: return make_aria_string("uint16");
        case 188: return make_aria_string("uint32");
        case 189: return make_aria_string("uint64");
        case 190: return make_aria_string("uint128");
        case 191: return make_aria_string("uint256");
        case 192: return make_aria_string("uint512");
        case 193: return make_aria_string("uint1024");
        case 194: return make_aria_string("uint2048");
        case 195: return make_aria_string("uint4096");
        // Signed integers (196-205)
        case 196: return make_aria_string("int8");
        case 197: return make_aria_string("int16");
        case 198: return make_aria_string("int32");
        case 199: return make_aria_string("int64");
        case 200: return make_aria_string("int128");
        case 201: return make_aria_string("int256");
        case 202: return make_aria_string("int512");
        case 203: return make_aria_string("int1024");
        case 204: return make_aria_string("int2048");
        case 205: return make_aria_string("int4096");
        // TBB integers (206-209)
        case 206: return make_aria_string("tbb8");
        case 207: return make_aria_string("tbb16");
        case 208: return make_aria_string("tbb32");
        case 209: return make_aria_string("tbb64");
        // Floats (210-215)
        case 210: return make_aria_string("flt32");
        case 211: return make_aria_string("flt64");
        case 212: return make_aria_string("flt128");
        case 213: return make_aria_string("flt256");
        case 214: return make_aria_string("flt512");
        case 215: return make_aria_string("fix256");
        default: return make_aria_string(""); // unknown token — no explicit type
    }
}

// ============================================================================
// Type lookup by name (unified: tries primitive cache, then struct)
// ============================================================================

void* tc_lookup_type(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto it = tc->primitive_cache.find(name);
    if (it != tc->primitive_cache.end()) return it->second;
    return nullptr; // not found
}

// Resolve a type name, wrap in Result if has_body, and store both
// value_type and actual_ret in the state. Returns actual_ret.
void* tc_resolve_func_ret(void* state, const char* type_name, int32_t has_body) {
    fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: type_name='%s' has_body=%d\n", type_name ? type_name : "NULL", has_body);
    auto* tc = (TypeCheckerState*)state;
    SemaType* value_type = nullptr;
    auto it = tc->primitive_cache.find(type_name);
    if (it != tc->primitive_cache.end()) {
        value_type = it->second;
        fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: found type '%s'\n", value_type->name.c_str());
    } else {
        fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: type not found, using void\n");
        auto vit = tc->primitive_cache.find("void");
        value_type = (vit != tc->primitive_cache.end()) ? vit->second : nullptr;
    }
    tc->current_func_value_type = value_type;
    if (has_body && value_type) {
        fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: making result type\n");
        auto* result_t = (SemaType*)tc_make_result_type(state, value_type);
        fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: done, returning result_t=%p\n", result_t);
        tc->current_func_return_type = result_t;
        return result_t;
    }
    fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret: returning value_type=%p\n", (void*)value_type);
    tc->current_func_return_type = value_type;
    return value_type;
}

// Same as above but takes a pre-resolved type handle instead of a name.
// Sets current_func_value_type and current_func_return_type in state. Returns actual_ret.
void* tc_prep_func_return(void* state, void* value_type, int32_t has_body) {
    auto* tc = (TypeCheckerState*)state;
    tc->current_func_value_type = (SemaType*)value_type;
    if (has_body && value_type) {
        auto* result_t = (SemaType*)tc_make_result_type(state, value_type);
        tc->current_func_return_type = result_t;
        return result_t;
    }
    tc->current_func_return_type = (SemaType*)value_type;
    return value_type;
}

// Resolve function return type from an AST node (TYPE_ANNOTATION).
// Reads str_val from the node directly on C side — avoids Aria string passing.
// Same semantics as tc_resolve_func_ret but takes an AST node handle.
void* tc_resolve_func_ret_node(void* state, void* ast_node, int32_t has_body) {
    // Extract type name from AST node's str_val field
    struct ASTNodeCompat {
        int32_t type; int32_t line; int32_t col;
        char* str_val; char* str_val2; char* str_val3;
        int64_t int_val; double float_val; int32_t bool_val; int32_t flags;
    };
    auto* node = (ASTNodeCompat*)ast_node;
    const char* type_name = (node && node->str_val) ? node->str_val : "void";
    fprintf(stderr, "[TC_DEBUG] tc_resolve_func_ret_node: type_name='%s' has_body=%d\n", type_name, has_body);
    return tc_resolve_func_ret(state, type_name, has_body);
}

// Resolve function return type from a FUNC_DECL AST node.
// Reads child[0] of the func_decl, checks if TYPE_ANNOTATION, extracts type name.
void* tc_resolve_func_ret_from_decl(void* state, void* func_decl_node, int32_t has_body) {
    struct ASTNodeCompat {
        int32_t type; int32_t line; int32_t col;
        char* str_val; char* str_val2; char* str_val3;
        int64_t int_val; double float_val; int32_t bool_val; int32_t flags;
        void** children; int64_t child_count; int64_t child_cap;
    };
    auto* node = (ASTNodeCompat*)func_decl_node;
    const char* type_name = "void";
    if (node && node->child_count > 0 && node->children && node->children[0]) {
        auto* child = (ASTNodeCompat*)node->children[0];
        // NT_TYPE_ANNOTATION = 48
        if (child->type == 48 && child->str_val) {
            type_name = child->str_val;
        }
    }
    return tc_resolve_func_ret(state, type_name, has_body);
}

// Resolve the declared type of a VAR_DECL AST node.
// Checks child[0] for type annotation, falls back to str2 (legacy type name).
// Returns the SemaType* or nullptr if unresolved.
void* tc_resolve_var_type(void* state, void* ast_node) {
    struct ASTNodeCompat {
        int32_t type; int32_t line; int32_t col;
        char* str_val; char* str_val2; char* str_val3;
        int64_t int_val; double float_val; int32_t bool_val; int32_t flags;
        void** children; int64_t child_count; int64_t child_cap;
    };
    auto* tc = (TypeCheckerState*)state;
    auto* node = (ASTNodeCompat*)ast_node;
    if (!node) return nullptr;

    // Check child[0] for type annotation
    if (node->child_count > 0 && node->children && node->children[0]) {
        auto* child = (ASTNodeCompat*)node->children[0];
        // NT_TYPE_ANNOTATION = 48
        if (child->type == 48 && child->str_val) {
            auto it = tc->primitive_cache.find(child->str_val);
            if (it != tc->primitive_cache.end()) {
                return it->second;
            }
        }
        // NT_POINTER_TYPE = 51 — return a pointer-like type
        if (child->type == 51) {
            auto it = tc->primitive_cache.find("wild_ptr");
            if (it != tc->primitive_cache.end()) return it->second;
            // Fallback: return a generic pointer type
            auto it2 = tc->primitive_cache.find("int8");
            if (it2 != tc->primitive_cache.end()) return it2->second;
        }
    }

    // Fallback: try str2 (legacy type name)
    if (node->str_val2 && strlen(node->str_val2) > 0) {
        auto it = tc->primitive_cache.find(node->str_val2);
        if (it != tc->primitive_cache.end()) {
            return it->second;
        }
    }

    return nullptr;
}

// Null pointer handle
void* tc_null_ptr() {
    return nullptr;
}

// Check if a pointer is null (returns 1 for null, 0 for non-null)
int32_t tc_is_null(void* ptr) {
    return (ptr == nullptr) ? 1 : 0;
}

// Save current function context onto stack
void tc_save_func_context(void* state) {
    auto* tc = (TypeCheckerState*)state;
    tc->func_context_stack.push_back({tc->current_func_return_type, tc->current_func_value_type});
}

// Restore function context from stack
void tc_restore_func_context(void* state) {
    auto* tc = (TypeCheckerState*)state;
    if (!tc->func_context_stack.empty()) {
        auto ctx = tc->func_context_stack.back();
        tc->func_context_stack.pop_back();
        tc->current_func_return_type = ctx.ret;
        tc->current_func_value_type = ctx.val;
    }
}

// Resolve parameter type from AST node (same as tc_resolve_var_type but for params)
void* tc_resolve_param_type(void* state, void* param_node) {
    struct ASTNodeCompat {
        int32_t type; int32_t line; int32_t col;
        char* str_val; char* str_val2; char* str_val3;
        int64_t int_val; double float_val; int32_t bool_val; int32_t flags;
        void** children; int64_t child_count; int64_t child_cap;
    };
    auto* tc = (TypeCheckerState*)state;
    auto* node = (ASTNodeCompat*)param_node;
    if (!node) return nullptr;

    // Check child[0] for type annotation
    if (node->child_count > 0 && node->children && node->children[0]) {
        auto* child = (ASTNodeCompat*)node->children[0];
        // NT_TYPE_ANNOTATION = 48
        if (child->type == 48 && child->str_val) {
            auto it = tc->primitive_cache.find(child->str_val);
            if (it != tc->primitive_cache.end()) {
                return it->second;
            }
        }
    }

    // Fallback: try str2
    if (node->str_val2 && strlen(node->str_val2) > 0) {
        auto it = tc->primitive_cache.find(node->str_val2);
        if (it != tc->primitive_cache.end()) {
            return it->second;
        }
    }

    // Default: void
    auto it = tc->primitive_cache.find("void");
    return (it != tc->primitive_cache.end()) ? it->second : nullptr;
}

// Check if a symbol exists (returns 1 if exists, 0 if not)
int32_t tc_symbol_exists(void* state, const char* name) {
    auto* tc = (TypeCheckerState*)state;
    auto* scope = tc->current_scope;
    while (scope) {
        auto it = scope->symbols.find(name);
        if (it != scope->symbols.end()) return 1;
        scope = scope->parent;
    }
    return 0;
}

} // extern "C"
