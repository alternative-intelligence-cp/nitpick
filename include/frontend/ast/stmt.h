#ifndef ARIA_STMT_H
#define ARIA_STMT_H

#include "ast_node.h"
#include "expr.h"
#include <map>

namespace aria {

/**
 * Variable declaration statement node
 * Represents: type:name = value;
 */
class VarDeclStmt : public ASTNode {
public:
    std::string typeName;      // e.g., "int8", "string" (legacy, kept for simple types)
    ASTNodePtr typeNode;       // Type AST node (for complex types like arrays)
    std::string varName;
    ASTNodePtr initializer;    // Can be nullptr
    bool isWild;               // wild keyword (opt-out of GC)
    bool isWildx;              // wildx keyword (wild + executable memory)
    bool isConst;              // const keyword
    bool isFixed;              // fixed keyword (runtime immutability)
    bool isStack;              // stack keyword
    bool isGC;                 // gc keyword (explicit)
    
    // P0: Alignment attribute support - #[align(N)]
    // Used for cache line alignment (64), SIMD (16/32/64), and FFI compatibility
    uint64_t alignment = 0;    // 0 = natural alignment, >0 = explicit alignment in bytes
    
    // Borrow checker annotations (set during borrow checking phase)
    int scope_depth;           // Scope depth where variable is declared (for Appendage Theory)
    bool requires_drop;        // True if variable needs explicit cleanup (wild/stack)
    bool is_pinned_shadow;     // True if this variable is a pinning reference (#x)
    std::string pinned_target; // Name of the variable this one pins (if is_pinned_shadow)
    
    VarDeclStmt(const std::string& type, const std::string& name, 
                ASTNodePtr init = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::VAR_DECL, line, column),
          typeName(type), typeNode(nullptr), varName(name), initializer(init),
          isWild(false), isWildx(false), isConst(false), isFixed(false), isStack(false), isGC(false),
          scope_depth(0), requires_drop(false), is_pinned_shadow(false), pinned_target("") {}
    
    // Constructor with typeNode support
    VarDeclStmt(ASTNodePtr typeN, const std::string& name,
                ASTNodePtr init = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::VAR_DECL, line, column),
          typeName(typeN ? typeN->toString() : ""), typeNode(typeN), varName(name), initializer(init),
          isWild(false), isWildx(false), isConst(false), isFixed(false), isStack(false), isGC(false),
          scope_depth(0), requires_drop(false), is_pinned_shadow(false), pinned_target("") {}
    
    std::string toString() const override;
};

/**
 * Generic parameter information
 * Stores name and trait constraints for a type parameter
 * Example: T: Addable & Display
 */
struct GenericParamInfo {
    std::string name;                    // e.g., "T"
    std::vector<std::string> constraints; // Trait bounds: ["Addable", "Display"]
    
    GenericParamInfo(const std::string& n) : name(n) {}
    GenericParamInfo(const std::string& n, const std::vector<std::string>& c)
        : name(n), constraints(c) {}
    
    bool hasConstraints() const { return !constraints.empty(); }
};

/**
 * Function declaration statement node
 * Represents: func:name = returnType(params) { body };
 */
class FuncDeclStmt : public ASTNode {
public:
    std::string funcName;
    ASTNodePtr returnType;  // Changed from std::string to ASTNodePtr
    std::vector<ASTNodePtr> parameters;  // ParameterNode instances
    ASTNodePtr body;                      // BlockStmt
    bool isAsync;
    bool isPublic;
    bool isExtern;
    bool returnIsWild = false;
    bool returnIsWildx = false;
    
    // GPU/PTX Backend - Phase 3: Kernel Attributes
    bool isGPUKernel = false;  // #[gpu_kernel] - Entry point callable from host
    bool isGPUDevice = false;  // #[gpu_device] - Helper function on GPU only  // true if 'wild' qualifier on return type (for FFI)
    bool isInline = false;     // inline func: - hint to inline at call sites
    bool isNoInline = false;   // noinline func: - prevent inlining
    bool isComptime = false;   // comptime func: - evaluated at compile time when all args known
    std::vector<GenericParamInfo> genericParams;  // For generics: func<T: Trait, U>
    
    // P1-4: Design by Contract - Formal verification support
    std::vector<ASTNodePtr> preconditions;   // requires clauses (checked on entry)
    std::vector<ASTNodePtr> postconditions;  // ensures clauses (checked on exit)
    
    FuncDeclStmt(const std::string& name, ASTNodePtr retType,
                 const std::vector<ASTNodePtr>& params, ASTNodePtr body,
                 int line = 0, int column = 0)
        : ASTNode(NodeType::FUNC_DECL, line, column),
          funcName(name), returnType(retType), parameters(params), body(body),
          isAsync(false), isPublic(false), isExtern(false) {}
    
    std::string toString() const override;
};

/**
 * Struct declaration node
 * Represents: struct Name { type:field1; type:field2; };
 * Or: struct<T, U> Name { *T:field1; *U:field2; };
 */
class StructDeclStmt : public ASTNode {
public:
    std::string structName;
    std::vector<ASTNodePtr> fields;  // VarDeclStmt instances (field declarations)
    std::vector<GenericParamInfo> genericParams;  // For generics: struct<T, U>
    
    // P0: Alignment attribute support - #[align(N)] for entire struct
    // Ensures all instances of this struct are aligned to N bytes
    uint64_t alignment = 0;    // 0 = natural alignment, >0 = explicit alignment in bytes
    
    StructDeclStmt(const std::string& name, const std::vector<ASTNodePtr>& fieldList,
                   int line = 0, int column = 0)
        : ASTNode(NodeType::STRUCT_DECL, line, column),
          structName(name), fields(fieldList) {}
    
    std::string toString() const override;
};

/**
 * Enum declaration node
 * Represents: enum:Name = { VARIANT1 = value1, VARIANT2 = value2, ... };
 */
class EnumDeclStmt : public ASTNode {
public:
    std::string enumName;
    std::map<std::string, int64_t> variants;  // variant name -> integer value
    
    EnumDeclStmt(const std::string& name, const std::map<std::string, int64_t>& variantMap,
                 int line = 0, int column = 0)
        : ASTNode(NodeType::ENUM_DECL, line, column),
          enumName(name), variants(variantMap) {}
    
    std::string toString() const override;
};

/**
 * Type declaration node
 * Represents: Type:Name = { func:create=...; func:destroy=...; struct:internal=...; struct:interface=...; };
 * Organizes functions and data into composable units (like classes without inheritance)
 * Desugars to combined struct + UFCS methods during semantic analysis
 */
class TypeDeclStmt : public ASTNode {
public:
    std::string typeName;
    
    // Required/Optional functions
    ASTNodePtr createFunc;   // func:create (constructor) - optional but recommended
    ASTNodePtr destroyFunc;  // func:destroy (destructor) - optional
    
    // Struct definitions (all optional)
    ASTNodePtr internalStruct;   // struct:internal (private members)
    ASTNodePtr interfaceStruct;  // struct:interface (public members)
    ASTNodePtr typeStruct;       // struct:type (static members)
    
    // Additional methods
    std::vector<ASTNodePtr> methods;  // All other func: declarations
    
    TypeDeclStmt(const std::string& name, int line = 0, int column = 0)
        : ASTNode(NodeType::TYPE_DECL, line, column),
          typeName(name),
          createFunc(nullptr),
          destroyFunc(nullptr),
          internalStruct(nullptr),
          interfaceStruct(nullptr),
          typeStruct(nullptr) {}
    
    std::string toString() const override;
};

/**
 * Function parameter node
 * Represents: type:name in function parameters
 */
class ParameterNode : public ASTNode {
public:
    ASTNodePtr typeNode;  // Changed from std::string to ASTNodePtr
    std::string paramName;
    ASTNodePtr defaultValue;  // Can be nullptr
    bool isWild = false;      // true if 'wild' qualifier present (for FFI)
    bool isWildx = false;     // true if 'wildx' qualifier present (wild + executable)
    
    ParameterNode(ASTNodePtr type, const std::string& name,
                  ASTNodePtr defVal = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::PARAMETER, line, column),
          typeNode(type), paramName(name), defaultValue(defVal) {}
    
    std::string toString() const override;
};

/**
 * Block statement node (code block)
 * Represents: { stmt1; stmt2; ... }
 */
class BlockStmt : public ASTNode {
public:
    std::vector<ASTNodePtr> statements;
    
    BlockStmt(const std::vector<ASTNodePtr>& stmts, int line = 0, int column = 0)
        : ASTNode(NodeType::BLOCK, line, column), statements(stmts) {}
    
    BlockStmt(int line = 0, int column = 0)
        : ASTNode(NodeType::BLOCK, line, column) {}
    
    std::string toString() const override;
};

/**
 * Expression statement node
 * Represents: any expression used as a statement
 */
class ExpressionStmt : public ASTNode {
public:
    ASTNodePtr expression;
    
    ExpressionStmt(ASTNodePtr expr, int line = 0, int column = 0)
        : ASTNode(NodeType::EXPRESSION_STMT, line, column), expression(expr) {}
    
    std::string toString() const override;
};

/**
 * Return statement node
 * Represents: return expr; or return;
 */
class ReturnStmt : public ASTNode {
public:
    ASTNodePtr value;  // Can be nullptr
    
    ReturnStmt(ASTNodePtr val = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::RETURN, line, column), value(val) {}
    
    std::string toString() const override;
};

/**
 * Pass statement node (result success)
 * Represents: pass(value);
 * Returns a result type with err=0 and val=value
 */
class PassStmt : public ASTNode {
public:
    ASTNodePtr value;  // The success value to return
    
    PassStmt(ASTNodePtr val, int line = 0, int column = 0)
        : ASTNode(NodeType::PASS, line, column), value(val) {}
    
    std::string toString() const override;
};

/**
 * Fail statement node (result error)
 * Represents: fail(error_code);
 * Returns a result type with err=error_code and val=0
 */
class FailStmt : public ASTNode {
public:
    ASTNodePtr errorCode;  // The error code to return
    
    FailStmt(ASTNodePtr code, int line = 0, int column = 0)
        : ASTNode(NodeType::FAIL, line, column), errorCode(code) {}
    
    std::string toString() const override;
};

/**
 * If statement node
 * Represents: if (condition) { thenBlock } else { elseBlock }
 */
class IfStmt : public ASTNode {
public:
    ASTNodePtr condition;
    ASTNodePtr thenBranch;    // BlockStmt or single statement
    ASTNodePtr elseBranch;    // Can be nullptr, or another IfStmt for else if
    
    IfStmt(ASTNodePtr cond, ASTNodePtr thenBlock, ASTNodePtr elseBlock = nullptr,
           int line = 0, int column = 0)
        : ASTNode(NodeType::IF, line, column),
          condition(cond), thenBranch(thenBlock), elseBranch(elseBlock) {}
    
    std::string toString() const override;
};

/**
 * While statement node
 * Represents: while (condition) { body }
 */
class WhileStmt : public ASTNode {
public:
    ASTNodePtr condition;
    ASTNodePtr body;
    
    WhileStmt(ASTNodePtr cond, ASTNodePtr bodyBlock, int line = 0, int column = 0)
        : ASTNode(NodeType::WHILE, line, column),
          condition(cond), body(bodyBlock) {}
    
    std::string toString() const override;
};

/**
 * For statement node
 * Represents: for (init; condition; update) { body }
 */
class ForStmt : public ASTNode {
public:
    // C-style for loop: for (init; cond; update) { body }
    ASTNodePtr initializer;   // Can be nullptr or VarDecl
    ASTNodePtr condition;
    ASTNodePtr update;
    ASTNodePtr body;
    
    // Range-based for loop: for (var in range) { body }
    bool isRangeBased;        // true if range-based for loop
    std::string iteratorName; // Variable name for range iteration
    std::string iteratorType; // Type annotation for iterator (optional, can be empty)
    ASTNodePtr rangeExpr;     // Range expression (e.g., 0..10)
    
    // C-style constructor (4-6 params with last 2 optional)
    ForStmt(ASTNodePtr init, ASTNodePtr cond, ASTNodePtr upd, ASTNodePtr bodyBlock,
            int line = 0, int column = 0)
        : ASTNode(NodeType::FOR, line, column),
          initializer(std::move(init)), condition(std::move(cond)), 
          update(std::move(upd)), body(std::move(bodyBlock)),
          isRangeBased(false) {}
    
    // Range-based constructor (needs strings, so clearly different from C-style)
    // Use named params to avoid confusion: iterName and iterType are strings
    static std::unique_ptr<ForStmt> createRangeBased(
            const std::string& iterName, const std::string& iterType,
            ASTNodePtr range, ASTNodePtr bodyBlock, int line = 0, int column = 0) {
        auto stmt = std::make_unique<ForStmt>(nullptr, nullptr, nullptr, std::move(bodyBlock), line, column);
        stmt->isRangeBased = true;
        stmt->iteratorName = iterName;
        stmt->iteratorType = iterType;
        stmt->rangeExpr = std::move(range);
        return stmt;
    }
    
    std::string toString() const override;
};

/**
 * Break statement node
 * Represents: break; or break(label);
 */
class BreakStmt : public ASTNode {
public:
    std::string label;  // Empty string if unlabeled
    
    BreakStmt(const std::string& lbl = "", int line = 0, int column = 0)
        : ASTNode(NodeType::BREAK, line, column), label(lbl) {}
    
    std::string toString() const override;
};

/**
 * Continue statement node
 * Represents: continue; or continue(label);
 */
class ContinueStmt : public ASTNode {
public:
    std::string label;  // Empty string if unlabeled
    
    ContinueStmt(const std::string& lbl = "", int line = 0, int column = 0)
        : ASTNode(NodeType::CONTINUE, line, column), label(lbl) {}
    
    std::string toString() const override;
};

/**
 * Defer statement node
 * Represents: defer { block }
 * Block-scoped RAII cleanup - executes at scope exit in LIFO order
 */
class DeferStmt : public ASTNode {
public:
    ASTNodePtr block;  // BlockStmt to execute on scope exit
    
    DeferStmt(ASTNodePtr blk, int line = 0, int column = 0)
        : ASTNode(NodeType::DEFER, line, column), block(blk) {}
    
    std::string toString() const override;
};

/**
 * Till loop statement node
 * Represents: till(limit, step) { body }
 * Automatically tracks iteration via $ variable
 * Directionality: positive step counts up from 0, negative counts down from limit
 */
class TillStmt : public ASTNode {
public:
    ASTNodePtr limit;  // Iteration limit
    ASTNodePtr step;   // Step value (direction determined by sign)
    ASTNodePtr body;   // Loop body
    
    TillStmt(ASTNodePtr lim, ASTNodePtr st, ASTNodePtr b, int line = 0, int column = 0)
        : ASTNode(NodeType::TILL, line, column), limit(lim), step(st), body(b) {}
    
    std::string toString() const override;
};

/**
 * Loop statement node
 * Represents: loop(start, limit, step) { body }
 * Automatically tracks iteration via $ variable
 * Direction determined by start vs limit comparison
 */
class LoopStmt : public ASTNode {
public:
    ASTNodePtr start;  // Starting value
    ASTNodePtr limit;  // Limit value
    ASTNodePtr step;   // Step value (always positive magnitude)
    ASTNodePtr body;   // Loop body
    
    LoopStmt(ASTNodePtr st, ASTNodePtr lim, ASTNodePtr step_val, ASTNodePtr b, 
             int line = 0, int column = 0)
        : ASTNode(NodeType::LOOP, line, column), 
          start(st), limit(lim), step(step_val), body(b) {}
    
    std::string toString() const override;
};

/**
 * When loop statement node
 * Represents: when(condition) { body } then { then_block } end { end_block }
 * Tri-state: then executes on normal completion, end on break or initial false
 */
class WhenStmt : public ASTNode {
public:
    ASTNodePtr condition;     // Loop condition
    ASTNodePtr body;          // Loop body
    ASTNodePtr then_block;    // Executed on normal completion (optional)
    ASTNodePtr end_block;     // Executed on break or no execution (optional)
    
    WhenStmt(ASTNodePtr cond, ASTNodePtr b, ASTNodePtr then_blk, ASTNodePtr end_blk,
             int line = 0, int column = 0)
        : ASTNode(NodeType::WHEN, line, column),
          condition(cond), body(b), then_block(then_blk), end_block(end_blk) {}
    
    std::string toString() const override;
};

/**
 * Pick case node (individual case in pick statement)
 * Represents: pattern { body } or label:pattern { body } or (!) { unreachable }
 */
class PickCase : public ASTNode {
public:
    std::string label;         // Optional label (empty if no label)
    ASTNodePtr pattern;        // Pattern expression: (< 10), (9), (*), (!), etc.
    ASTNodePtr body;           // Case body block
    bool is_unreachable;       // True if pattern is (!)
    
    PickCase(const std::string& lbl, ASTNodePtr patt, ASTNodePtr b, bool unreachable = false,
             int line = 0, int column = 0)
        : ASTNode(NodeType::PICK_CASE, line, column),
          label(lbl), pattern(patt), body(b), is_unreachable(unreachable) {}
    
    std::string toString() const override;
};

/**
 * Pick statement node (pattern matching)
 * Represents: pick(selector) { case1, case2, ... }
 */
class PickStmt : public ASTNode {
public:
    ASTNodePtr selector;              // Expression being matched
    std::vector<ASTNodePtr> cases;    // Vector of PickCase nodes
    
    PickStmt(ASTNodePtr sel, const std::vector<ASTNodePtr>& cs,
             int line = 0, int column = 0)
        : ASTNode(NodeType::PICK, line, column),
          selector(sel), cases(cs) {}
    
    std::string toString() const override;
};

/**
 * Fall statement node (explicit fallthrough in pick)
 * Represents: fall(label);
 */
class FallStmt : public ASTNode {
public:
    std::string target_label;     // Label to fall through to
    
    FallStmt(const std::string& label, int line = 0, int column = 0)
        : ASTNode(NodeType::FALL, line, column),
          target_label(label) {}
    
    std::string toString() const override;
};

/**
 * Use statement node (module import)
 * Represents: use path.to.module;
 *             use path.{item1, item2};
 *             use path.*;
 *             use "file.aria" as alias;
 */
class UseStmt : public ASTNode {
public:
    std::vector<std::string> path;    // ["std", "io"] for use std.io;
    std::vector<std::string> items;   // ["array", "map"] for use std.{array, map};
    bool isWildcard;                  // true for use math.*;
    std::string alias;                // "utils" for use "./file.aria" as utils;
    bool isFilePath;                  // true if path is a file path (quoted string)
    
    UseStmt(const std::vector<std::string>& p, int line = 0, int column = 0)
        : ASTNode(NodeType::USE, line, column),
          path(p), isWildcard(false), isFilePath(false) {}
    
    std::string toString() const override;
};

/**
 * Module statement node (module definition)
 * Represents: mod name;                  (external file module)
 *             mod name { ... }           (inline module)
 *             pub mod name;              (public module)
 */
class ModStmt : public ASTNode {
public:
    std::string name;                     // Module name
    bool isPublic;                        // true if pub mod
    bool isInline;                        // true if inline module { }
    std::vector<ASTNodePtr> body;         // Statements inside inline module
    
    ModStmt(const std::string& n, int line = 0, int column = 0)
        : ASTNode(NodeType::MOD, line, column),
          name(n), isPublic(false), isInline(false) {}
    
    std::string toString() const override;
};

/**
 * Extern block statement node (FFI declarations)
 * Represents: extern "libname" { declarations }
 *             extern "libc" { func:malloc = void*(uint64:size); }
 */
class ExternStmt : public ASTNode {
public:
    std::string libraryName;              // "libc", "kernel32", etc.
    std::vector<ASTNodePtr> declarations; // Function/variable declarations
    
    ExternStmt(const std::string& libName, int line = 0, int column = 0)
        : ASTNode(NodeType::EXTERN, line, column),
          libraryName(libName) {}
    
    std::string toString() const override;
};

/**
 * Opaque struct declaration node (FFI)
 * Represents: opaque struct:Name;
 * Used for C types where the internal structure is unknown/opaque
 */
class OpaqueStructDecl : public ASTNode {
public:
    std::string structName;               // Name of the opaque type

    OpaqueStructDecl(const std::string& name, int line = 0, int column = 0)
        : ASTNode(NodeType::OPAQUE_STRUCT, line, column),
          structName(name) {}

    std::string toString() const override;
};

/**
 * Trait method signature (WP 005: Trait System)
 * Represents a method signature in a trait declaration
 */
struct TraitMethod {
    std::string name;
    std::vector<ParameterNode> parameters;
    std::string returnType;
    bool autoWrap = false;

    TraitMethod(const std::string& n, std::vector<ParameterNode> params, const std::string& ret)
        : name(n), parameters(std::move(params)), returnType(ret) {}
};

/**
 * Trait declaration node (WP 005: Trait System)
 * Represents: trait:Name = { func:method = returnType(params); ... };
 * Example: trait:Drawable = { func:draw = void(self); func:area = flt64(self); };
 */
class TraitDeclStmt : public ASTNode {
public:
    std::string traitName;
    std::vector<TraitMethod> methods;
    std::vector<std::string> superTraits;  // Trait inheritance

    TraitDeclStmt(const std::string& name, std::vector<TraitMethod> m, int line = 0, int column = 0)
        : ASTNode(NodeType::TRAIT_DECL, line, column),
          traitName(name), methods(std::move(m)) {}

    std::string toString() const override;
};

/**
 * Trait implementation node (WP 005: Trait System)
 * Represents: impl:Trait:for:Type = { func:method = returnType(params) { body }; ... };
 * Example: impl:Drawable:for:Circle = { func:draw = void(self) { ... }; };
 */
class ImplDeclStmt : public ASTNode {
public:
    std::string traitName;
    std::string typeName;
    std::vector<ASTNodePtr> methods;  // FuncDeclStmt instances

    ImplDeclStmt(const std::string& trait, const std::string& type,
                 std::vector<ASTNodePtr> m, int line = 0, int column = 0)
        : ASTNode(NodeType::IMPL_DECL, line, column),
          traitName(trait), typeName(type), methods(std::move(m)) {}

    std::string toString() const override;
};

/**
 * Compile-time block statement
 * Represents: comptime { ... }
 * All statements inside are evaluated at compile time by the const evaluator.
 * Results are embedded as constants in the output.
 */
class ComptimeBlockStmt : public ASTNode {
public:
    ASTNodePtr body;  // BlockStmt containing the comptime code
    
    ComptimeBlockStmt(ASTNodePtr block, int line = 0, int column = 0)
        : ASTNode(NodeType::COMPTIME_BLOCK, line, column),
          body(std::move(block)) {}
    
    std::string toString() const override;
};

/**
 * Program node (root of AST)
 * Represents: entire program
 */
class ProgramNode : public ASTNode {
public:
    std::vector<ASTNodePtr> declarations;
    
    ProgramNode(const std::vector<ASTNodePtr>& decls, int line = 0, int column = 0)
        : ASTNode(NodeType::PROGRAM, line, column), declarations(decls) {}
    
    ProgramNode(int line = 0, int column = 0)
        : ASTNode(NodeType::PROGRAM, line, column) {}
    
    std::string toString() const override;
};

} // namespace aria

#endif // ARIA_STMT_H
