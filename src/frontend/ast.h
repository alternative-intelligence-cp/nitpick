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
class StructDecl;
class AsyncBlock;
class VarExpr;
class IntLiteral;
class BoolLiteral;
class NullLiteral;
class FloatLiteral;
class StringLiteral;
class TemplateString;
class TernaryExpr;
class BinaryOp;
class UnaryOp;
class CallExpr;
class ReturnStmt;
class ExpressionStmt;
class IfStmt;
class PickStmt;
class PickCase;
class FallStmt;
class WhenLoop;
class TillLoop;
class DeferStmt;
class ForLoop;
class WhileLoop;
class BreakStmt;
class ContinueStmt;
class WhenExpr;
class AwaitExpr;
class UnwrapExpr;
class ObjectLiteral;
class ArrayLiteral;
class IndexExpr;
class MemberAccess;
class LambdaExpr;
class CastExpr;
class UseStmt;
class ModDef;
class ExternBlock;

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
    virtual void visit(FloatLiteral* node) = 0;
    virtual void visit(BoolLiteral* node) = 0;
    virtual void visit(NullLiteral* node) = 0;
    virtual void visit(StringLiteral* node) = 0;
    virtual void visit(TemplateString* node) = 0;
    virtual void visit(TernaryExpr* node) = 0;
    virtual void visit(BinaryOp* node) = 0;
    virtual void visit(UnaryOp* node) = 0;
    virtual void visit(CallExpr* node) = 0;
    virtual void visit(ObjectLiteral* node) { /* default: do nothing */ }
    virtual void visit(MemberAccess* node) { /* default: do nothing */ }
    virtual void visit(ArrayLiteral* node) { /* default: do nothing */ }
    virtual void visit(IndexExpr* node) { /* default: do nothing */ }
    virtual void visit(UnwrapExpr* node) { /* default: do nothing */ }
    virtual void visit(LambdaExpr* node) { /* default: do nothing */ }
    virtual void visit(CastExpr* node) { /* default: do nothing */ }

    // Statements
    virtual void visit(VarDecl* node) = 0;
    virtual void visit(FuncDecl* node) { /* default: do nothing */ }
    virtual void visit(StructDecl* node) { /* default: do nothing */ }
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(ExpressionStmt* node) { /* default: visit expression */ }
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(Block* node) = 0;

    // Control Flow
    virtual void visit(PickStmt* node) = 0;
    virtual void visit(FallStmt* node) { /* default: do nothing */ }
    virtual void visit(TillLoop* node) = 0;
    virtual void visit(WhenLoop* node) = 0;
    virtual void visit(DeferStmt* node) = 0;
    
    // New loop types (Bugs #67-68)
    virtual void visit(ForLoop* node) { /* default: do nothing */ }
    virtual void visit(WhileLoop* node) { /* default: do nothing */ }
    virtual void visit(BreakStmt* node) { /* default: do nothing */ }
    virtual void visit(ContinueStmt* node) { /* default: do nothing */ }
    
    // New expression types (Bugs #69-70)
    virtual void visit(WhenExpr* node) { /* default: do nothing */ }
    virtual void visit(AwaitExpr* node) { /* default: do nothing */ }
    
    // Module system (Bugs #73-75)
    virtual void visit(UseStmt* node) { /* default: do nothing */ }
    virtual void visit(ModDef* node) { /* default: do nothing */ }
    virtual void visit(ExternBlock* node) { /* default: do nothing */ }
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
