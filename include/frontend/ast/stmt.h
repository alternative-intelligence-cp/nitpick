#ifndef ARIA_STMT_H
#define ARIA_STMT_H

#include "ast_node.h"
#include "expr.h"

namespace aria {

/**
 * Variable declaration statement node
 * Represents: type:name = value;
 */
class VarDeclStmt : public ASTNode {
public:
    std::string typeName;      // e.g., "int8", "string"
    std::string varName;
    ASTNodePtr initializer;    // Can be nullptr
    bool isWild;               // wild keyword (opt-out of GC)
    bool isConst;              // const keyword
    bool isStack;              // stack keyword
    bool isGC;                 // gc keyword (explicit)
    
    VarDeclStmt(const std::string& type, const std::string& name, 
                ASTNodePtr init = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::VAR_DECL, line, column),
          typeName(type), varName(name), initializer(init),
          isWild(false), isConst(false), isStack(false), isGC(false) {}
    
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
    std::string returnType;
    std::vector<ASTNodePtr> parameters;  // ParameterNode instances
    ASTNodePtr body;                      // BlockStmt
    bool isAsync;
    bool isPublic;
    bool isExtern;
    std::vector<GenericParamInfo> genericParams;  // For generics: func<T: Trait, U>
    
    FuncDeclStmt(const std::string& name, const std::string& retType,
                 const std::vector<ASTNodePtr>& params, ASTNodePtr body,
                 int line = 0, int column = 0)
        : ASTNode(NodeType::FUNC_DECL, line, column),
          funcName(name), returnType(retType), parameters(params), body(body),
          isAsync(false), isPublic(false), isExtern(false) {}
    
    std::string toString() const override;
};

/**
 * Function parameter node
 * Represents: type:name in function parameters
 */
class ParameterNode : public ASTNode {
public:
    std::string typeName;
    std::string paramName;
    ASTNodePtr defaultValue;  // Can be nullptr
    
    ParameterNode(const std::string& type, const std::string& name,
                  ASTNodePtr defVal = nullptr, int line = 0, int column = 0)
        : ASTNode(NodeType::PARAMETER, line, column),
          typeName(type), paramName(name), defaultValue(defVal) {}
    
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
    ASTNodePtr initializer;   // Can be nullptr or VarDecl
    ASTNodePtr condition;
    ASTNodePtr update;
    ASTNodePtr body;
    
    ForStmt(ASTNodePtr init, ASTNodePtr cond, ASTNodePtr upd, ASTNodePtr bodyBlock,
            int line = 0, int column = 0)
        : ASTNode(NodeType::FOR, line, column),
          initializer(init), condition(cond), update(upd), body(bodyBlock) {}
    
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
