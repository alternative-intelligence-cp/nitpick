#ifndef ARIA_FRONTEND_AST_H
#define ARIA_FRONTEND_AST_H

#include <memory>
#include <string>
#include <vector>

namespace aria {
namespace frontend {

// Forward Declarations
class AstNode;
class AstVisitor;
class Expression;
class Statement;
class Block;
class VarDecl;
class FuncDecl;
class VarExpr;
class IntLiteral;
class FloatLiteral;
class StringLiteral;
class BinaryOp;
class UnaryOp;
class CallExpr;
class ReturnStmt;
class IfStmt;
class PickStmt;
class PickCase;
class WhenLoop;
class TillLoop;
class DeferStmt;

// Base AST Node
// All AST nodes inherit from this class
class AstNode {
public:
    virtual ~AstNode() = default;
    virtual void accept(AstVisitor& visitor) = 0;
};

// Visitor Pattern Interface
// Visitors traverse the AST and perform operations on nodes
class AstVisitor {
public:
    virtual ~AstVisitor() = default;

    // Expressions
    virtual void visit(VarExpr* node) = 0;
    virtual void visit(IntLiteral* node) = 0;
    virtual void visit(BinaryOp* node) = 0;
    virtual void visit(UnaryOp* node) = 0;
    virtual void visit(CallExpr* node) = 0;

    // Statements
    virtual void visit(VarDecl* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(Block* node) = 0;

    // Control Flow
    virtual void visit(PickStmt* node) = 0;
    virtual void visit(TillLoop* node) = 0;
    virtual void visit(DeferStmt* node) = 0;
};

// Block Statement
// Represents a sequence of statements
class Block : public AstNode {
public:
    std::vector<std::unique_ptr<AstNode>> statements;

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_H
