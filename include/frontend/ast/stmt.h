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
    std::vector<std::string> genericParams;  // For generics: func<T, U>
    
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
 * Represents: break;
 */
class BreakStmt : public ASTNode {
public:
    BreakStmt(int line = 0, int column = 0)
        : ASTNode(NodeType::BREAK, line, column) {}
    
    std::string toString() const override { return "Break"; }
};

/**
 * Continue statement node
 * Represents: continue;
 */
class ContinueStmt : public ASTNode {
public:
    ContinueStmt(int line = 0, int column = 0)
        : ASTNode(NodeType::CONTINUE, line, column) {}
    
    std::string toString() const override { return "Continue"; }
};

/**
 * Defer statement node
 * Represents: defer expr;
 */
class DeferStmt : public ASTNode {
public:
    ASTNodePtr expression;
    
    DeferStmt(ASTNodePtr expr, int line = 0, int column = 0)
        : ASTNode(NodeType::DEFER, line, column), expression(expr) {}
    
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
