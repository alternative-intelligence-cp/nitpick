/**
 * Abstract Syntax Tree - Node Definitions
 * 
 * AST for the Process Orchestration Language (POL) - whitespace-insensitive
 * shell grammar with strong typing and brace-delimited structure.
 */

#pragma once

#include "parser/token.hpp"
#include <memory>
#include <vector>
#include <string>

namespace ariash {
namespace parser {

// Forward declarations
class ASTVisitor;

/**
 * Base AST Node
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
    
    SourceLocation location;
    
protected:
    ASTNode(SourceLocation loc) : location(loc) {}
};

// =============================================================================
// Expressions
// =============================================================================

class ExprNode : public ASTNode {
protected:
    ExprNode(SourceLocation loc) : ASTNode(loc) {}
};

/**
 * Literal expressions
 */
class IntegerLiteral : public ExprNode {
public:
    int64_t value;
    
    IntegerLiteral(int64_t val, SourceLocation loc)
        : ExprNode(loc), value(val) {}
    
    void accept(ASTVisitor& visitor) override;
};

class StringLiteral : public ExprNode {
public:
    std::string value;
    
    StringLiteral(const std::string& val, SourceLocation loc)
        : ExprNode(loc), value(val) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Variable reference
 */
class VariableExpr : public ExprNode {
public:
    std::string name;
    
    VariableExpr(const std::string& n, SourceLocation loc)
        : ExprNode(loc), name(n) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Binary operation (arithmetic, comparison, logical)
 */
class BinaryOpExpr : public ExprNode {
public:
    TokenType op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
    
    BinaryOpExpr(TokenType operation, 
                 std::unique_ptr<ExprNode> l,
                 std::unique_ptr<ExprNode> r,
                 SourceLocation loc)
        : ExprNode(loc), op(operation), left(std::move(l)), right(std::move(r)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Unary operation (-, !)
 */
class UnaryOpExpr : public ExprNode {
public:
    TokenType op;
    std::unique_ptr<ExprNode> operand;
    
    UnaryOpExpr(TokenType operation,
                std::unique_ptr<ExprNode> expr,
                SourceLocation loc)
        : ExprNode(loc), op(operation), operand(std::move(expr)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Function call (also used for spawn())
 */
class CallExpr : public ExprNode {
public:
    std::string function;
    std::vector<std::unique_ptr<ExprNode>> arguments;
    
    CallExpr(const std::string& fn, SourceLocation loc)
        : ExprNode(loc), function(fn) {}
    
    void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// Statements
// =============================================================================

class StmtNode : public ASTNode {
protected:
    StmtNode(SourceLocation loc) : ASTNode(loc) {}
};

/**
 * Block of statements
 */
class BlockStmt : public StmtNode {
public:
    std::vector<std::unique_ptr<StmtNode>> statements;
    
    BlockStmt(SourceLocation loc) : StmtNode(loc) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Variable declaration
 */
class VarDeclStmt : public StmtNode {
public:
    std::string type;  // Type name (string instead of TokenType)
    std::string name;
    std::unique_ptr<ExprNode> initializer;
    
    VarDeclStmt(const std::string& t, const std::string& n, SourceLocation loc)
        : StmtNode(loc), type(t), name(n) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Assignment statement
 */
class AssignStmt : public StmtNode {
public:
    std::string variable;
    std::unique_ptr<ExprNode> value;
    
    AssignStmt(const std::string& var, std::unique_ptr<ExprNode> val, SourceLocation loc)
        : StmtNode(loc), variable(var), value(std::move(val)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * If statement
 */
class IfStmt : public StmtNode {
public:
    std::unique_ptr<ExprNode> condition;
    std::unique_ptr<StmtNode> thenBranch;
    std::unique_ptr<StmtNode> elseBranch;  // Optional
    
    IfStmt(std::unique_ptr<ExprNode> cond, std::unique_ptr<StmtNode> then, SourceLocation loc)
        : StmtNode(loc), condition(std::move(cond)), thenBranch(std::move(then)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * While loop
 */
class WhileStmt : public StmtNode {
public:
    std::unique_ptr<ExprNode> condition;
    std::unique_ptr<StmtNode> body;
    
    WhileStmt(std::unique_ptr<ExprNode> cond, std::unique_ptr<StmtNode> bod, SourceLocation loc)
        : StmtNode(loc), condition(std::move(cond)), body(std::move(bod)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * For loop (for x in expr)
 */
class ForStmt : public StmtNode {
public:
    std::string variable;
    std::unique_ptr<ExprNode> iterable;
    std::unique_ptr<StmtNode> body;
    
    ForStmt(const std::string& var, std::unique_ptr<ExprNode> iter, std::unique_ptr<StmtNode> bod, SourceLocation loc)
        : StmtNode(loc), variable(var), iterable(std::move(iter)), body(std::move(bod)) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Return statement
 */
class ReturnStmt : public StmtNode {
public:
    std::unique_ptr<ExprNode> value;  // Optional
    
    ReturnStmt(SourceLocation loc) : StmtNode(loc) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Expression statement - evaluate an expression (for REPL)
 */
class ExprStmt : public StmtNode {
public:
    std::unique_ptr<ExprNode> expression;
    
    ExprStmt(std::unique_ptr<ExprNode> expr, SourceLocation loc) 
        : StmtNode(loc), expression(std::move(expr)) {}
    
    void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// Process Orchestration (Shell-specific)
// =============================================================================

/**
 * Redirection type
 */
enum class RedirectionType {
    INPUT,    // <
    OUTPUT,   // >
    APPEND    // >>
};

/**
 * Redirection specification
 */
struct Redirection {
    RedirectionType type;
    std::string target;
};

/**
 * Simple command (executable + arguments)
 */
class CommandStmt : public StmtNode {
public:
    std::string executable;
    std::vector<std::string> arguments;  // Changed from unique_ptr<ExprNode>
    std::vector<Redirection> redirections;
    bool background;  // Run with & operator
    
    CommandStmt(const std::string& exe, SourceLocation loc)
        : StmtNode(loc), executable(exe), background(false) {}
    
    void accept(ASTVisitor& visitor) override;
};

/**
 * Pipeline (command | command | ...)
 */
class PipelineStmt : public StmtNode {
public:
    std::vector<std::unique_ptr<CommandStmt>> commands;
    
    PipelineStmt(SourceLocation loc) : StmtNode(loc) {}
    
    void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// Program (top-level)
// =============================================================================

/**
 * Complete program (list of statements)
 */
class Program : public ASTNode {
public:
    std::vector<std::unique_ptr<StmtNode>> statements;
    
    Program(SourceLocation loc = SourceLocation()) : ASTNode(loc) {}
    
    void accept(ASTVisitor& visitor) override;
};

// =============================================================================
// Visitor Pattern
// =============================================================================

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    
    // Expressions
    virtual void visit(IntegerLiteral& node) = 0;
    virtual void visit(StringLiteral& node) = 0;
    virtual void visit(VariableExpr& node) = 0;
    virtual void visit(BinaryOpExpr& node) = 0;
    virtual void visit(UnaryOpExpr& node) = 0;
    virtual void visit(CallExpr& node) = 0;
    
    // Statements
    virtual void visit(BlockStmt& node) = 0;
    virtual void visit(VarDeclStmt& node) = 0;
    virtual void visit(AssignStmt& node) = 0;
    virtual void visit(IfStmt& node) = 0;
    virtual void visit(WhileStmt& node) = 0;
    virtual void visit(ForStmt& node) = 0;
    virtual void visit(ReturnStmt& node) = 0;
    virtual void visit(ExprStmt& node) = 0;
    virtual void visit(CommandStmt& node) = 0;
    virtual void visit(PipelineStmt& node) = 0;
    virtual void visit(Program& node) = 0;
};

} // namespace parser
} // namespace ariash
