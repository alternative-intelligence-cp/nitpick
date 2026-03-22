// ═══════════════════════════════════════════════════════════════════════
// pipeline_helpers.cpp — Phase 5.4: Integration Pipeline
//
// Provides two capabilities:
// 1. AST Converter: Transforms ASTNodeCompat trees (from self-hosted parser)
//    into native C++ ASTNode class hierarchy (for C++ backend consumption)
// 2. AST Validator: Compares self-hosted parser output against C++ parser
//    output for the same source code, reporting structural mismatches
//
// This file is compiled into aria_runtime AND includes the C++ frontend
// sources for lexer, parser, and AST classes (which are LLVM-free).
// ═══════════════════════════════════════════════════════════════════════

#include "frontend/token.h"
#include "frontend/lexer/lexer.h"
#include "frontend/parser/parser.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/type.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

// ═══════════════════════════════════════════════════════════════════════
// ASTNodeCompat — mirrors the struct in ast_helpers.cpp / sema_helpers.cpp
// ═══════════════════════════════════════════════════════════════════════
struct ASTNodeCompat {
    int32_t type;
    int32_t line;
    int32_t col;
    char*   str_val;
    char*   str_val2;
    char*   str_val3;
    int64_t int_val;
    double  float_val;
    int32_t bool_val;
    int32_t flags;
    void**  children;
    int64_t child_count;
    int64_t child_cap;
};

// ═══════════════════════════════════════════════════════════════════════
// Self-hosted parser node type constants (must match parser.aria)
// ═══════════════════════════════════════════════════════════════════════
enum CompatNodeType {
    CNT_LITERAL          = 0,
    CNT_IDENTIFIER       = 1,
    CNT_BINARY_OP        = 2,
    CNT_UNARY_OP         = 3,
    CNT_CALL             = 4,
    CNT_INDEX            = 5,
    CNT_MEMBER_ACCESS    = 6,
    CNT_POINTER_MEMBER   = 7,
    CNT_LAMBDA           = 8,
    CNT_TEMPLATE_LITERAL = 9,
    CNT_RANGE            = 10,
    CNT_TERNARY          = 11,
    CNT_SAFE_NAV         = 12,
    CNT_NULL_COALESCE    = 13,
    CNT_PIPELINE         = 14,
    CNT_UNWRAP           = 15,
    CNT_ARRAY_LITERAL    = 16,
    CNT_OBJECT_LITERAL   = 17,
    CNT_AWAIT            = 18,
    CNT_MOVE             = 19,
    CNT_VECTOR_CONSTRUCTOR = 20,
    CNT_CAST             = 21,
    CNT_VAR_DECL         = 22,
    CNT_FUNC_DECL        = 23,
    CNT_STRUCT_DECL      = 24,
    CNT_ENUM_DECL        = 25,
    CNT_TYPE_DECL        = 26,
    CNT_OPAQUE_STRUCT    = 27,
    CNT_TRAIT_DECL       = 28,
    CNT_IMPL_DECL        = 29,
    CNT_RETURN           = 30,
    CNT_PASS             = 31,
    CNT_FAIL             = 32,
    CNT_BREAK            = 33,
    CNT_CONTINUE         = 34,
    CNT_DEFER            = 35,
    CNT_BLOCK            = 36,
    CNT_EXPRESSION_STMT  = 37,
    // 38 = gap
    CNT_IF               = 39,
    CNT_WHILE            = 40,
    CNT_FOR              = 41,
    CNT_LOOP             = 42,
    CNT_TILL             = 43,
    CNT_WHEN             = 44,
    CNT_PICK             = 45,
    CNT_PICK_CASE        = 46,
    CNT_FALL             = 47,
    CNT_TYPE_ANNOTATION  = 48,
    CNT_GENERIC_TYPE     = 49,
    CNT_ARRAY_TYPE       = 50,
    CNT_POINTER_TYPE     = 51,
    CNT_SAFE_REF_TYPE    = 52,
    CNT_OPTIONAL_TYPE    = 53,
    CNT_FUNCTION_TYPE    = 54,
    CNT_USE              = 55,
    CNT_MOD              = 56,
    CNT_EXTERN           = 57,
    CNT_PROGRAM          = 58,
    CNT_ASSIGNMENT       = 59,
    CNT_PARAMETER        = 60,
    CNT_ARGUMENT         = 61,
};

// Literal kind constants (stored in bool_val for NT_LITERAL)
enum CompatLitKind {
    CLIT_INT    = 1,
    CLIT_FLOAT  = 2,
    CLIT_STRING = 3,
    CLIT_BOOL   = 4,
    CLIT_NULL   = 5,
};

// Flag constants (match parser.aria)
static constexpr int32_t CFLAG_PUB            = 1;
static constexpr int32_t CFLAG_CONST          = 2;
static constexpr int32_t CFLAG_FIXED          = 4;
static constexpr int32_t CFLAG_ASYNC          = 8;
static constexpr int32_t CFLAG_EXTERN         = 16;
static constexpr int32_t CFLAG_UNCHECKED_CAST = 32;

// ═══════════════════════════════════════════════════════════════════════
// Helper: safe string extraction from ASTNodeCompat
// ═══════════════════════════════════════════════════════════════════════
static std::string safe_str(const char* s) {
    return s ? std::string(s) : "";
}

// ═══════════════════════════════════════════════════════════════════════
// Helper: Create a Token from operator code stored in ASTNodeCompat
// The self-hosted parser stores operator token type as int_val.
// Since token type numbering matches the C++ TokenType enum, direct cast.
// ═══════════════════════════════════════════════════════════════════════
static aria::frontend::Token make_op_token(int64_t op_code, int line, int col) {
    auto tt = static_cast<aria::frontend::TokenType>(op_code);
    std::string lexeme = aria::frontend::tokenTypeToString(tt);
    return aria::frontend::Token(tt, lexeme, line, col);
}

// ═══════════════════════════════════════════════════════════════════════
// Helper: Map literal token type (stored in flags) to explicit_type string
// The self-hosted parser stores the LITERAL token type ID in flags.
// These use the self-hosted lexer's TK_INTEGER_* / TK_FLOAT_* constants.
// ═══════════════════════════════════════════════════════════════════════
static std::string token_type_to_suffix(int32_t token_type) {
    // Self-hosted lexer typed integer literal tokens (from lexer.aria)
    switch (token_type) {
        case 186: return "u8";     // TK_INTEGER_U8
        case 187: return "u16";    // TK_INTEGER_U16
        case 188: return "u32";    // TK_INTEGER_U32
        case 189: return "u64";    // TK_INTEGER_U64
        case 190: return "u128";   // TK_INTEGER_U128
        case 196: return "i8";     // TK_INTEGER_I8
        case 197: return "i16";    // TK_INTEGER_I16
        case 198: return "i32";    // TK_INTEGER_I32
        case 199: return "i64";    // TK_INTEGER_I64
        case 200: return "i128";   // TK_INTEGER_I128
        case 206: return "tbb8";   // TK_INTEGER_TBB8
        case 207: return "tbb16";  // TK_INTEGER_TBB16
        case 208: return "tbb32";  // TK_INTEGER_TBB32
        case 209: return "tbb64";  // TK_INTEGER_TBB64
        // Typed float literal tokens
        case 210: return "f32";    // TK_FLOAT_F32
        case 211: return "f64";    // TK_FLOAT_F64
        case 212: return "f128";   // TK_FLOAT_F128
        case 213: return "f256";   // TK_FLOAT_F256
        case 214: return "f512";   // TK_FLOAT_F512
        case 215: return "fix256"; // TK_FLOAT_FIX256
        // Untyped literals (TK_INTEGER=184, TK_FLOAT=185) — no suffix
        default: return "";
    }
}

// ═══════════════════════════════════════════════════════════════════════
// Forward declaration
// ═══════════════════════════════════════════════════════════════════════
static aria::ASTNodePtr convert_node(ASTNodeCompat* node);

// ═══════════════════════════════════════════════════════════════════════
// AST CONVERTER: ASTNodeCompat → native C++ ASTNode
// ═══════════════════════════════════════════════════════════════════════

static aria::ASTNodePtr convert_literal(ASTNodeCompat* n) {
    int line = n->line, col = n->col;
    int32_t lit_kind = n->bool_val;
    int32_t suffix_tok = n->flags;
    std::string explicit_type = token_type_to_suffix(suffix_tok);

    switch (lit_kind) {
        case CLIT_INT: {
            auto lit = std::make_shared<aria::LiteralExpr>(n->int_val, line, col);
            lit->explicit_type = explicit_type;
            return lit;
        }
        case CLIT_FLOAT: {
            auto lit = std::make_shared<aria::LiteralExpr>(n->float_val, line, col);
            lit->explicit_type = explicit_type;
            return lit;
        }
        case CLIT_STRING: {
            auto lit = std::make_shared<aria::LiteralExpr>(safe_str(n->str_val), line, col);
            return lit;
        }
        case CLIT_BOOL: {
            auto lit = std::make_shared<aria::LiteralExpr>(n->int_val != 0, line, col);
            return lit;
        }
        case CLIT_NULL: {
            auto lit = std::make_shared<aria::LiteralExpr>(std::monostate{}, line, col);
            return lit;
        }
        default: {
            // Unknown literal kind — fallback to null
            return std::make_shared<aria::LiteralExpr>(std::monostate{}, line, col);
        }
    }
}

static aria::ASTNodePtr convert_identifier(ASTNodeCompat* n) {
    return std::make_shared<aria::IdentifierExpr>(safe_str(n->str_val), n->line, n->col);
}

static aria::ASTNodePtr convert_binary_op(ASTNodeCompat* n) {
    // children[0] = left, children[1] = right
    // int_val = operator token type
    aria::ASTNodePtr left = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr right = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    auto op = make_op_token(n->int_val, n->line, n->col);
    return std::make_shared<aria::BinaryExpr>(left, op, right, n->line, n->col);
}

static aria::ASTNodePtr convert_unary_op(ASTNodeCompat* n) {
    // children[0] = operand, int_val = operator token, str_val = "pre"|"post"
    aria::ASTNodePtr operand = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    auto op = make_op_token(n->int_val, n->line, n->col);
    bool isPostfix = (safe_str(n->str_val) == "post");
    return std::make_shared<aria::UnaryExpr>(op, operand, isPostfix, n->line, n->col);
}

static aria::ASTNodePtr convert_call(ASTNodeCompat* n) {
    // Self-hosted parser: children[0] = callee, children[1..N] = arguments
    aria::ASTNodePtr callee = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    std::vector<aria::ASTNodePtr> args;
    for (int64_t i = 1; i < n->child_count; ++i) {
        args.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    return std::make_shared<aria::CallExpr>(callee, args, n->line, n->col);
}

static aria::ASTNodePtr convert_index(ASTNodeCompat* n) {
    // children[0] = array, children[1] = index
    aria::ASTNodePtr arr = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr idx = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    return std::make_shared<aria::IndexExpr>(arr, idx, n->line, n->col);
}

static aria::ASTNodePtr convert_member_access(ASTNodeCompat* n, bool isPtr) {
    // children[0] = object, str_val = member name
    aria::ASTNodePtr obj = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::MemberAccessExpr>(obj, safe_str(n->str_val), isPtr, false, n->line, n->col);
}

static aria::ASTNodePtr convert_assignment(ASTNodeCompat* n) {
    // The C++ parser treats assignments as BinaryOp (lowest precedence),
    // so convert to BinaryExpr for compatibility.
    // children[0] = target, children[1] = value, int_val = operator token
    aria::ASTNodePtr target = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr value = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    auto op = make_op_token(n->int_val, n->line, n->col);
    return std::make_shared<aria::BinaryExpr>(target, op, value, n->line, n->col);
}

static aria::ASTNodePtr convert_cast(ASTNodeCompat* n) {
    // children[0] = target type, children[1] = expression
    // flags & CFLAG_UNCHECKED_CAST = unchecked
    aria::ASTNodePtr typeNode = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr expr = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    std::string targetType;
    if (typeNode) {
        auto* simple = dynamic_cast<aria::SimpleType*>(typeNode.get());
        if (simple) targetType = simple->typeName;
    }
    bool unchecked = (n->flags & CFLAG_UNCHECKED_CAST) != 0;
    return std::make_shared<aria::CastExpr>(expr, typeNode, targetType, unchecked, n->line, n->col);
}

static aria::ASTNodePtr convert_array_literal(ASTNodeCompat* n) {
    std::vector<aria::ASTNodePtr> elements;
    for (int64_t i = 0; i < n->child_count; ++i) {
        elements.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    return std::make_shared<aria::ArrayLiteralExpr>(elements, n->line, n->col);
}

static aria::ASTNodePtr convert_range(ASTNodeCompat* n) {
    // children[0] = start, children[1] = end
    // bool_val or flags indicates exclusive
    aria::ASTNodePtr start = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr end = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    bool exclusive = (n->bool_val != 0);
    return std::make_shared<aria::RangeExpr>(start, end, exclusive, n->line, n->col);
}

static aria::ASTNodePtr convert_ternary(ASTNodeCompat* n) {
    aria::ASTNodePtr cond = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr tval = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    aria::ASTNodePtr fval = (n->child_count > 2) ? convert_node((ASTNodeCompat*)n->children[2]) : nullptr;
    return std::make_shared<aria::TernaryExpr>(cond, tval, fval, n->line, n->col);
}

// ─── Type Annotations ───────────────────────────────────────────────
static aria::ASTNodePtr convert_type_annotation(ASTNodeCompat* n) {
    return std::make_shared<aria::SimpleType>(safe_str(n->str_val), n->line, n->col);
}

static aria::ASTNodePtr convert_pointer_type(ASTNodeCompat* n) {
    // children[0] = base type, str_val = "->" or "*"
    aria::ASTNodePtr base = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    auto pt = std::make_shared<aria::PointerType>(base, n->line, n->col);
    pt->isNative = (safe_str(n->str_val) == "->");
    return pt;
}

static aria::ASTNodePtr convert_array_type(ASTNodeCompat* n) {
    // children[0] = element type, children[1] = size (optional)
    aria::ASTNodePtr elem = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr size = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    return std::make_shared<aria::ArrayType>(elem, size, n->line, n->col);
}

static aria::ASTNodePtr convert_generic_type(ASTNodeCompat* n) {
    // str_val = base name, children = type arguments
    std::vector<aria::ASTNodePtr> typeArgs;
    for (int64_t i = 0; i < n->child_count; ++i) {
        typeArgs.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    return std::make_shared<aria::GenericType>(safe_str(n->str_val), typeArgs, n->line, n->col);
}

static aria::ASTNodePtr convert_optional_type(ASTNodeCompat* n) {
    aria::ASTNodePtr wrapped = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::OptionalTypeNode>(wrapped, n->line, n->col);
}

static aria::ASTNodePtr convert_safe_ref_type(ASTNodeCompat* n) {
    aria::ASTNodePtr base = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::SafeRefType>(base, n->line, n->col);
}

static aria::ASTNodePtr convert_function_type(ASTNodeCompat* n) {
    // children[0] = return type, children[1..N] = param types
    aria::ASTNodePtr ret = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    std::vector<aria::ASTNodePtr> params;
    for (int64_t i = 1; i < n->child_count; ++i) {
        params.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    return std::make_shared<aria::FunctionType>(ret, params, n->line, n->col);
}

// ─── Statements ─────────────────────────────────────────────────────
static aria::ASTNodePtr convert_var_decl(ASTNodeCompat* n) {
    // str_val = name, flags = modifiers, children[0] = type, children[1] = init (optional)
    std::string name = safe_str(n->str_val);
    aria::ASTNodePtr typeNode = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr init = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;

    auto decl = std::make_shared<aria::VarDeclStmt>(typeNode, name, init, n->line, n->col);

    // Decode flags
    if (n->flags & CFLAG_PUB)   decl->isWild = false; // pub doesn't set wild
    if (n->flags & CFLAG_CONST) decl->isConst = true;
    if (n->flags & CFLAG_FIXED) decl->isFixed = true;

    // Check type annotation str_val2 for memory qualifier
    std::string qualifier = safe_str(n->str_val2);
    if (qualifier == "wild" || qualifier == "wildx") decl->isWild = true;
    if (qualifier == "stack") decl->isStack = true;
    if (qualifier == "gc")    decl->isGC = true;

    return decl;
}

static aria::ASTNodePtr convert_parameter(ASTNodeCompat* n) {
    // str_val = name, children[0] = type, children[1] = default value (optional)
    aria::ASTNodePtr typeNode = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr defVal = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    auto param = std::make_shared<aria::ParameterNode>(typeNode, safe_str(n->str_val), defVal, n->line, n->col);
    return param;
}

static aria::ASTNodePtr convert_func_decl(ASTNodeCompat* n) {
    // str_val = name, flags = modifiers
    // children[0] = return type (NT_TYPE_ANNOTATION)
    // children[1..N-2] = parameters (NT_PARAMETER)
    // children[N-1] = body (NT_BLOCK) — absent for extern
    std::string name = safe_str(n->str_val);
    bool isExtern = (n->flags & CFLAG_EXTERN) != 0;
    bool isAsync  = (n->flags & CFLAG_ASYNC)  != 0;
    bool isPub    = (n->flags & CFLAG_PUB)    != 0;

    aria::ASTNodePtr retType = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;

    std::vector<aria::ASTNodePtr> params;
    aria::ASTNodePtr body = nullptr;

    if (isExtern) {
        // Extern: all children after [0] are parameters, no body
        for (int64_t i = 1; i < n->child_count; ++i) {
            params.push_back(convert_node((ASTNodeCompat*)n->children[i]));
        }
    } else {
        // Regular: children[1..N-2] = params, children[N-1] = body
        for (int64_t i = 1; i < n->child_count - 1; ++i) {
            params.push_back(convert_node((ASTNodeCompat*)n->children[i]));
        }
        if (n->child_count > 1) {
            body = convert_node((ASTNodeCompat*)n->children[n->child_count - 1]);
        }
    }

    auto func = std::make_shared<aria::FuncDeclStmt>(name, retType, params, body, n->line, n->col);
    func->isExtern = isExtern;
    func->isAsync  = isAsync;
    func->isPublic = isPub;
    return func;
}

static aria::ASTNodePtr convert_struct_decl(ASTNodeCompat* n) {
    // str_val = name, children = fields (NT_VAR_DECL)
    std::vector<aria::ASTNodePtr> fields;
    for (int64_t i = 0; i < n->child_count; ++i) {
        fields.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    auto decl = std::make_shared<aria::StructDeclStmt>(safe_str(n->str_val), fields, n->line, n->col);
    return decl;
}

static aria::ASTNodePtr convert_enum_decl(ASTNodeCompat* n) {
    // str_val = name, children = variant nodes (NT_VAR_DECL with name + value)
    std::map<std::string, int64_t> variants;
    for (int64_t i = 0; i < n->child_count; ++i) {
        auto* child = (ASTNodeCompat*)n->children[i];
        variants[safe_str(child->str_val)] = child->int_val;
    }
    return std::make_shared<aria::EnumDeclStmt>(safe_str(n->str_val), variants, n->line, n->col);
}

static aria::ASTNodePtr convert_block(ASTNodeCompat* n) {
    std::vector<aria::ASTNodePtr> stmts;
    for (int64_t i = 0; i < n->child_count; ++i) {
        auto s = convert_node((ASTNodeCompat*)n->children[i]);
        if (s) stmts.push_back(s);
    }
    return std::make_shared<aria::BlockStmt>(stmts, n->line, n->col);
}

static aria::ASTNodePtr convert_expression_stmt(ASTNodeCompat* n) {
    aria::ASTNodePtr expr = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::ExpressionStmt>(expr, n->line, n->col);
}

static aria::ASTNodePtr convert_pass(ASTNodeCompat* n) {
    aria::ASTNodePtr val = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::PassStmt>(val, n->line, n->col);
}

static aria::ASTNodePtr convert_fail(ASTNodeCompat* n) {
    aria::ASTNodePtr code = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::FailStmt>(code, n->line, n->col);
}

static aria::ASTNodePtr convert_return(ASTNodeCompat* n) {
    aria::ASTNodePtr val = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::ReturnStmt>(val, n->line, n->col);
}

static aria::ASTNodePtr convert_break(ASTNodeCompat* n) {
    return std::make_shared<aria::BreakStmt>(safe_str(n->str_val), n->line, n->col);
}

static aria::ASTNodePtr convert_continue(ASTNodeCompat* n) {
    return std::make_shared<aria::ContinueStmt>(safe_str(n->str_val), n->line, n->col);
}

static aria::ASTNodePtr convert_defer(ASTNodeCompat* n) {
    aria::ASTNodePtr blk = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    return std::make_shared<aria::DeferStmt>(blk, n->line, n->col);
}

// ─── Control Flow ───────────────────────────────────────────────────
static aria::ASTNodePtr convert_if(ASTNodeCompat* n) {
    // children[0] = condition, children[1] = then, children[2] = else (optional)
    aria::ASTNodePtr cond = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr then_b = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    aria::ASTNodePtr else_b = (n->child_count > 2) ? convert_node((ASTNodeCompat*)n->children[2]) : nullptr;
    return std::make_shared<aria::IfStmt>(cond, then_b, else_b, n->line, n->col);
}

static aria::ASTNodePtr convert_while(ASTNodeCompat* n) {
    // children[0] = condition, children[1] = body
    aria::ASTNodePtr cond = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr body = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    return std::make_shared<aria::WhileStmt>(cond, body, n->line, n->col);
}

static aria::ASTNodePtr convert_for(ASTNodeCompat* n) {
    // Range-based: str_val = iterator name, children[0] = collection, children[1] = body
    std::string iterName = safe_str(n->str_val);
    aria::ASTNodePtr collection = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr body = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;

    // Create as range-based for
    auto stmt = aria::ForStmt::createRangeBased(iterName, "", collection, body, n->line, n->col);
    return std::shared_ptr<aria::ForStmt>(stmt.release());
}

static aria::ASTNodePtr convert_loop(ASTNodeCompat* n) {
    // children[0] = start, children[1] = limit, children[2] = step, children[3] = body
    aria::ASTNodePtr start = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr limit = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    aria::ASTNodePtr step  = (n->child_count > 2) ? convert_node((ASTNodeCompat*)n->children[2]) : nullptr;
    aria::ASTNodePtr body  = (n->child_count > 3) ? convert_node((ASTNodeCompat*)n->children[3]) : nullptr;
    return std::make_shared<aria::LoopStmt>(start, limit, step, body, n->line, n->col);
}

static aria::ASTNodePtr convert_pick(ASTNodeCompat* n) {
    // children[0] = selector, children[1..N] = cases
    aria::ASTNodePtr selector = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    std::vector<aria::ASTNodePtr> cases;
    for (int64_t i = 1; i < n->child_count; ++i) {
        cases.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    return std::make_shared<aria::PickStmt>(selector, cases, n->line, n->col);
}

static aria::ASTNodePtr convert_pick_case(ASTNodeCompat* n) {
    // str_val = label, children[0] = pattern, children[1] = body
    aria::ASTNodePtr pattern = (n->child_count > 0) ? convert_node((ASTNodeCompat*)n->children[0]) : nullptr;
    aria::ASTNodePtr body = (n->child_count > 1) ? convert_node((ASTNodeCompat*)n->children[1]) : nullptr;
    return std::make_shared<aria::PickCase>(safe_str(n->str_val), pattern, body, false, n->line, n->col);
}

// ─── Modules ────────────────────────────────────────────────────────
static aria::ASTNodePtr convert_use(ASTNodeCompat* n) {
    // str_val = module path
    std::vector<std::string> path;
    path.push_back(safe_str(n->str_val));
    return std::make_shared<aria::UseStmt>(path, n->line, n->col);
}

static aria::ASTNodePtr convert_extern(ASTNodeCompat* n) {
    // children = extern declarations (usually FUNC_DECL with FLAG_EXTERN)
    std::vector<aria::ASTNodePtr> decls;
    for (int64_t i = 0; i < n->child_count; ++i) {
        decls.push_back(convert_node((ASTNodeCompat*)n->children[i]));
    }
    auto ext = std::make_shared<aria::ExternStmt>("", n->line, n->col);
    ext->declarations = decls;
    return ext;
}

static aria::ASTNodePtr convert_program(ASTNodeCompat* n) {
    std::vector<aria::ASTNodePtr> decls;
    for (int64_t i = 0; i < n->child_count; ++i) {
        auto child = convert_node((ASTNodeCompat*)n->children[i]);
        if (child) decls.push_back(child);
    }
    return std::make_shared<aria::ProgramNode>(decls, n->line, n->col);
}

// ═══════════════════════════════════════════════════════════════════════
// Main converter dispatch
// ═══════════════════════════════════════════════════════════════════════
static aria::ASTNodePtr convert_node(ASTNodeCompat* n) {
    if (!n) return nullptr;

    switch (n->type) {
        // Expressions
        case CNT_LITERAL:          return convert_literal(n);
        case CNT_IDENTIFIER:       return convert_identifier(n);
        case CNT_BINARY_OP:        return convert_binary_op(n);
        case CNT_UNARY_OP:         return convert_unary_op(n);
        case CNT_CALL:             return convert_call(n);
        case CNT_INDEX:            return convert_index(n);
        case CNT_MEMBER_ACCESS:    return convert_member_access(n, false);
        case CNT_POINTER_MEMBER:   return convert_member_access(n, true);
        case CNT_ASSIGNMENT:       return convert_assignment(n);
        case CNT_CAST:             return convert_cast(n);
        case CNT_ARRAY_LITERAL:    return convert_array_literal(n);
        case CNT_RANGE:            return convert_range(n);
        case CNT_TERNARY:          return convert_ternary(n);

        // Type nodes
        case CNT_TYPE_ANNOTATION:  return convert_type_annotation(n);
        case CNT_POINTER_TYPE:     return convert_pointer_type(n);
        case CNT_ARRAY_TYPE:       return convert_array_type(n);
        case CNT_GENERIC_TYPE:     return convert_generic_type(n);
        case CNT_OPTIONAL_TYPE:    return convert_optional_type(n);
        case CNT_SAFE_REF_TYPE:    return convert_safe_ref_type(n);
        case CNT_FUNCTION_TYPE:    return convert_function_type(n);

        // Declarations
        case CNT_VAR_DECL:         return convert_var_decl(n);
        case CNT_FUNC_DECL:        return convert_func_decl(n);
        case CNT_STRUCT_DECL:      return convert_struct_decl(n);
        case CNT_ENUM_DECL:        return convert_enum_decl(n);
        case CNT_PARAMETER:        return convert_parameter(n);

        // Statements
        case CNT_BLOCK:            return convert_block(n);
        case CNT_EXPRESSION_STMT:  return convert_expression_stmt(n);
        case CNT_PASS:             return convert_pass(n);
        case CNT_FAIL:             return convert_fail(n);
        case CNT_RETURN:           return convert_return(n);
        case CNT_BREAK:            return convert_break(n);
        case CNT_CONTINUE:         return convert_continue(n);
        case CNT_DEFER:            return convert_defer(n);

        // Control flow
        case CNT_IF:               return convert_if(n);
        case CNT_WHILE:            return convert_while(n);
        case CNT_FOR:              return convert_for(n);
        case CNT_LOOP:             return convert_loop(n);
        case CNT_PICK:             return convert_pick(n);
        case CNT_PICK_CASE:        return convert_pick_case(n);

        // Modules
        case CNT_USE:              return convert_use(n);
        case CNT_EXTERN:           return convert_extern(n);
        case CNT_PROGRAM:          return convert_program(n);

        default: {
            fprintf(stderr, "[pipeline] Warning: unhandled node type %d\n", n->type);
            return nullptr;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
// AST Serialization — canonical string for comparison
// ═══════════════════════════════════════════════════════════════════════

// Serialize an ASTNodeCompat tree to a canonical string
static void serialize_compat(ASTNodeCompat* n, std::ostringstream& out, int depth) {
    if (!n) { out << "(null)"; return; }

    out << "(" << n->type;
    if (n->str_val && n->str_val[0])   out << " s=\"" << n->str_val << "\"";
    if (n->str_val2 && n->str_val2[0]) out << " s2=\"" << n->str_val2 << "\"";
    if (n->int_val != 0)                out << " i=" << n->int_val;
    if (n->float_val != 0.0)            out << " f=" << n->float_val;
    if (n->bool_val != 0)               out << " b=" << n->bool_val;
    if (n->flags != 0)                  out << " fl=" << n->flags;

    for (int64_t i = 0; i < n->child_count; ++i) {
        out << " ";
        serialize_compat((ASTNodeCompat*)n->children[i], out, depth + 1);
    }
    out << ")";
}

// Serialize native ASTNode to canonical string for comparison
static void serialize_native(aria::ASTNode* n, std::ostringstream& out, int depth);

static void serialize_native(aria::ASTNode* n, std::ostringstream& out, int depth) {
    if (!n) { out << "(null)"; return; }

    int type_code = static_cast<int>(n->type);
    out << "(" << type_code;

    // Extract relevant fields based on node type
    switch (n->type) {
        case aria::ASTNode::NodeType::LITERAL: {
            auto* lit = dynamic_cast<aria::LiteralExpr*>(n);
            if (lit) {
                std::visit([&out](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, int64_t>)
                        out << " i=" << arg;
                    else if constexpr (std::is_same_v<T, double>)
                        out << " f=" << arg;
                    else if constexpr (std::is_same_v<T, std::string>)
                        out << " s=\"" << arg << "\"";
                    else if constexpr (std::is_same_v<T, bool>)
                        out << " b=" << (arg ? 1 : 0);
                }, lit->value);
                if (!lit->explicit_type.empty())
                    out << " t=\"" << lit->explicit_type << "\"";
            }
            break;
        }
        case aria::ASTNode::NodeType::IDENTIFIER: {
            auto* id = dynamic_cast<aria::IdentifierExpr*>(n);
            if (id) out << " s=\"" << id->name << "\"";
            break;
        }
        case aria::ASTNode::NodeType::BINARY_OP: {
            auto* bin = dynamic_cast<aria::BinaryExpr*>(n);
            if (bin) {
                out << " op=" << static_cast<int>(bin->op.type);
                out << " ";
                serialize_native(bin->left.get(), out, depth + 1);
                out << " ";
                serialize_native(bin->right.get(), out, depth + 1);
            }
            break;
        }
        case aria::ASTNode::NodeType::UNARY_OP: {
            auto* un = dynamic_cast<aria::UnaryExpr*>(n);
            if (un) {
                out << " op=" << static_cast<int>(un->op.type);
                out << " ";
                serialize_native(un->operand.get(), out, depth + 1);
            }
            break;
        }
        case aria::ASTNode::NodeType::CALL: {
            auto* call = dynamic_cast<aria::CallExpr*>(n);
            if (call) {
                out << " ";
                serialize_native(call->callee.get(), out, depth + 1);
                for (auto& arg : call->arguments) {
                    out << " ";
                    serialize_native(arg.get(), out, depth + 1);
                }
            }
            break;
        }
        case aria::ASTNode::NodeType::VAR_DECL: {
            auto* v = dynamic_cast<aria::VarDeclStmt*>(n);
            if (v) {
                out << " s=\"" << v->varName << "\"";
                if (v->typeNode) { out << " "; serialize_native(v->typeNode.get(), out, depth + 1); }
                if (v->initializer) { out << " "; serialize_native(v->initializer.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::FUNC_DECL: {
            auto* f = dynamic_cast<aria::FuncDeclStmt*>(n);
            if (f) {
                out << " s=\"" << f->funcName << "\"";
                if (f->returnType) { out << " "; serialize_native(f->returnType.get(), out, depth + 1); }
                for (auto& p : f->parameters) { out << " "; serialize_native(p.get(), out, depth + 1); }
                if (f->body) { out << " "; serialize_native(f->body.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::STRUCT_DECL: {
            auto* s = dynamic_cast<aria::StructDeclStmt*>(n);
            if (s) {
                out << " s=\"" << s->structName << "\"";
                for (auto& ff : s->fields) { out << " "; serialize_native(ff.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::PARAMETER: {
            auto* p = dynamic_cast<aria::ParameterNode*>(n);
            if (p) {
                out << " s=\"" << p->paramName << "\"";
                if (p->typeNode) { out << " "; serialize_native(p->typeNode.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::BLOCK: {
            auto* blk = dynamic_cast<aria::BlockStmt*>(n);
            if (blk) {
                for (auto& s : blk->statements) { out << " "; serialize_native(s.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::PROGRAM: {
            auto* prog = dynamic_cast<aria::ProgramNode*>(n);
            if (prog) {
                for (auto& d : prog->declarations) { out << " "; serialize_native(d.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::PASS: {
            auto* p = dynamic_cast<aria::PassStmt*>(n);
            if (p && p->value) { out << " "; serialize_native(p->value.get(), out, depth + 1); }
            break;
        }
        case aria::ASTNode::NodeType::FAIL: {
            auto* f = dynamic_cast<aria::FailStmt*>(n);
            if (f && f->errorCode) { out << " "; serialize_native(f->errorCode.get(), out, depth + 1); }
            break;
        }
        case aria::ASTNode::NodeType::RETURN: {
            auto* r = dynamic_cast<aria::ReturnStmt*>(n);
            if (r && r->value) { out << " "; serialize_native(r->value.get(), out, depth + 1); }
            break;
        }
        case aria::ASTNode::NodeType::IF: {
            auto* i = dynamic_cast<aria::IfStmt*>(n);
            if (i) {
                if (i->condition) { out << " "; serialize_native(i->condition.get(), out, depth + 1); }
                if (i->thenBranch) { out << " "; serialize_native(i->thenBranch.get(), out, depth + 1); }
                if (i->elseBranch) { out << " "; serialize_native(i->elseBranch.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::WHILE: {
            auto* w = dynamic_cast<aria::WhileStmt*>(n);
            if (w) {
                if (w->condition) { out << " "; serialize_native(w->condition.get(), out, depth + 1); }
                if (w->body) { out << " "; serialize_native(w->body.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::TYPE_ANNOTATION: {
            auto* ta = dynamic_cast<aria::SimpleType*>(n);
            if (ta) out << " s=\"" << ta->typeName << "\"";
            break;
        }
        case aria::ASTNode::NodeType::POINTER_TYPE: {
            auto* pt = dynamic_cast<aria::PointerType*>(n);
            if (pt && pt->baseType) { out << " "; serialize_native(pt->baseType.get(), out, depth + 1); }
            break;
        }
        case aria::ASTNode::NodeType::ASSIGNMENT: {
            auto* a = dynamic_cast<aria::AssignmentExpr*>(n);
            if (a) {
                out << " op=" << static_cast<int>(a->op.type);
                if (a->target) { out << " "; serialize_native(a->target.get(), out, depth + 1); }
                if (a->value) { out << " "; serialize_native(a->value.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::INDEX: {
            auto* ix = dynamic_cast<aria::IndexExpr*>(n);
            if (ix) {
                if (ix->array) { out << " "; serialize_native(ix->array.get(), out, depth + 1); }
                if (ix->index) { out << " "; serialize_native(ix->index.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::MEMBER_ACCESS:
        case aria::ASTNode::NodeType::POINTER_MEMBER: {
            auto* ma = dynamic_cast<aria::MemberAccessExpr*>(n);
            if (ma) {
                out << " s=\"" << ma->member << "\"";
                if (ma->object) { out << " "; serialize_native(ma->object.get(), out, depth + 1); }
            }
            break;
        }
        case aria::ASTNode::NodeType::EXPRESSION_STMT: {
            auto* es = dynamic_cast<aria::ExpressionStmt*>(n);
            if (es && es->expression) { out << " "; serialize_native(es->expression.get(), out, depth + 1); }
            break;
        }
        default:
            // For unhandled types, just output the type code
            break;
    }

    out << ")";
}

// ═══════════════════════════════════════════════════════════════════════
// Structural comparison: count mismatches between compat and native ASTs
// ═══════════════════════════════════════════════════════════════════════
static int compare_trees(ASTNodeCompat* compat, aria::ASTNode* native, int depth = 0) {
    if (!compat && !native) return 0;
    if (!compat || !native) return 1;

    int mismatches = 0;

    // Compare node types (directly matching since enum values correspond)
    int compat_type = compat->type;
    int native_type = static_cast<int>(native->type);
    if (compat_type != native_type) {
        mismatches++;
    }

    // Compare names for named nodes
    switch (compat->type) {
        case CNT_IDENTIFIER: {
            auto* id = dynamic_cast<aria::IdentifierExpr*>(native);
            if (id && safe_str(compat->str_val) != id->name) mismatches++;
            break;
        }
        case CNT_FUNC_DECL: {
            auto* f = dynamic_cast<aria::FuncDeclStmt*>(native);
            if (f && safe_str(compat->str_val) != f->funcName) mismatches++;
            break;
        }
        case CNT_VAR_DECL: {
            auto* v = dynamic_cast<aria::VarDeclStmt*>(native);
            if (v && safe_str(compat->str_val) != v->varName) mismatches++;
            break;
        }
        case CNT_TYPE_ANNOTATION: {
            auto* ta = dynamic_cast<aria::SimpleType*>(native);
            if (ta && safe_str(compat->str_val) != ta->typeName) mismatches++;
            break;
        }
        default: break;
    }

    return mismatches;
}

// ═══════════════════════════════════════════════════════════════════════
// Stored state for pipeline operations
// ═══════════════════════════════════════════════════════════════════════
struct PipelineState {
    std::shared_ptr<aria::ASTNode> native_ast;
    std::vector<std::string> errors;
};

// AriaString wrapper (matches the runtime ABI format)
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

// ═══════════════════════════════════════════════════════════════════════
// C Extern API — callable from Aria programs
// ═══════════════════════════════════════════════════════════════════════

extern "C" {

// Convert ASTNodeCompat tree to native C++ AST
// Returns an opaque handle to PipelineState
void* pipeline_convert(void* compat_root) {
    auto* state = new PipelineState();
    if (compat_root) {
        state->native_ast = convert_node((ASTNodeCompat*)compat_root);
    }
    return state;
}

// Get the converted native AST as a string (for debugging/comparison)
AriaString* pipeline_native_to_string(void* state_ptr) {
    auto* state = (PipelineState*)state_ptr;
    if (!state || !state->native_ast) {
        return make_aria_string("(null)");
    }
    return make_aria_string(state->native_ast->toString());
}

// Free pipeline state
void pipeline_free(void* state_ptr) {
    delete (PipelineState*)state_ptr;
}

// ─── Validation API ─────────────────────────────────────────────────

// Parse source with C++ parser, convert to canonical string
AriaString* pipeline_cpp_parse_to_string(const char* source) {
    std::string src(source);
    aria::frontend::Lexer lexer(src);
    auto tokens = lexer.tokenize();

    aria::Parser parser(tokens);
    auto ast = parser.parse();

    if (!ast) {
        std::string err = "(parse_error";
        for (auto& e : parser.getErrors()) err += " " + e;
        err += ")";
        return make_aria_string(err);
    }

    std::ostringstream oss;
    serialize_native(ast.get(), oss, 0);
    return make_aria_string(oss.str());
}

// Serialize self-hosted ASTNodeCompat tree to canonical string
AriaString* pipeline_compat_to_string(void* compat_root) {
    if (!compat_root) return make_aria_string("(null)");

    std::ostringstream oss;
    serialize_compat((ASTNodeCompat*)compat_root, oss, 0);
    return make_aria_string(oss.str());
}

// Validate: parse with C++ parser, convert self-hosted AST to native,
// then compare the two native ASTs via canonical serialization.
// Returns 0 for perfect match, positive count of differences otherwise.
int32_t pipeline_validate(const char* source, void* compat_root) {
    if (!source || !compat_root) return -1;

    // Convert self-hosted AST to native
    auto self_ast = convert_node((ASTNodeCompat*)compat_root);
    if (!self_ast) return -3;

    std::ostringstream self_oss;
    serialize_native(self_ast.get(), self_oss, 0);

    // Parse with C++ parser
    std::string src(source);
    aria::frontend::Lexer lexer(src);
    auto tokens = lexer.tokenize();
    aria::Parser parser(tokens);
    auto cpp_ast = parser.parse();
    if (!cpp_ast) return -2;

    std::ostringstream cpp_oss;
    serialize_native(cpp_ast.get(), cpp_oss, 0);

    // Compare serialized forms
    return (self_oss.str() == cpp_oss.str()) ? 0 : 1;
}

// Deeper validation: serialize BOTH ASTs and compare strings
// Returns 1 if identical, 0 if different
int32_t pipeline_validate_deep(const char* source, void* compat_root) {
    if (!source || !compat_root) return 0;

    // Serialize self-hosted AST
    auto self_ast = convert_node((ASTNodeCompat*)compat_root);
    if (!self_ast) return 0;

    std::ostringstream self_oss;
    serialize_native(self_ast.get(), self_oss, 0);

    // Serialize C++ AST
    std::string src(source);
    aria::frontend::Lexer lexer(src);
    auto tokens = lexer.tokenize();
    aria::Parser parser(tokens);
    auto cpp_ast = parser.parse();
    if (!cpp_ast) return 0;

    std::ostringstream cpp_oss;
    serialize_native(cpp_ast.get(), cpp_oss, 0);

    return (self_oss.str() == cpp_oss.str()) ? 1 : 0;
}

// Get the C++ parser's canonical AST string for a source (for debugging)
AriaString* pipeline_cpp_ast_string(const char* source) {
    return pipeline_cpp_parse_to_string(source);
}

// Get the self-hosted parser's converted native AST string (for debugging)
AriaString* pipeline_self_ast_string(void* compat_root) {
    if (!compat_root) return make_aria_string("(null)");

    auto native = convert_node((ASTNodeCompat*)compat_root);
    if (!native) return make_aria_string("(convert_failed)");

    std::ostringstream oss;
    serialize_native(native.get(), oss, 0);
    return make_aria_string(oss.str());
}

} // extern "C"
