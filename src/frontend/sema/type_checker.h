/**
 * src/frontend/sema/type_checker.h
 *
 * Aria Type Checker
 * Version: 0.0.6
 *
 * Performs type checking on the AST.
 */

#ifndef ARIA_FRONTEND_SEMA_TYPE_CHECKER_H
#define ARIA_FRONTEND_SEMA_TYPE_CHECKER_H

#include "../ast.h"
#include "../ast/expr.h"
#include "../ast/stmt.h"
#include "types.h"
#include <vector>
#include <string>
#include <memory>

namespace aria {
namespace sema {

// Type checking result
struct TypeCheckResult {
    bool success = true;
    std::vector<std::string> errors;
};

// Type checker visitor
class TypeChecker : public frontend::AstVisitor {
private:
    std::unique_ptr<SymbolTable> symbols;
    std::vector<std::string> errors;
    std::shared_ptr<Type> current_expr_type;  // Type of last visited expression

public:
    TypeChecker() : symbols(std::make_unique<SymbolTable>()) {
        // Initialize built-in types
    }

    // Get type checking results
    TypeCheckResult getResult() {
        TypeCheckResult result;
        result.success = errors.empty();
        result.errors = errors;
        return result;
    }

    // Visitor methods for expressions
    void visit(frontend::VarExpr* node) override;
    void visit(frontend::IntLiteral* node) override;
    void visit(frontend::FloatLiteral* node) override;
    void visit(frontend::BoolLiteral* node) override;
    void visit(frontend::NullLiteral* node) override;
    void visit(frontend::StringLiteral* node) override;
    void visit(frontend::TemplateString* node) override;
    void visit(frontend::TernaryExpr* node) override;
    void visit(frontend::BinaryOp* node) override;
    void visit(frontend::UnaryOp* node) override;
    void visit(frontend::CallExpr* node) override;
    void visit(frontend::LambdaExpr* node) override;

    // Visitor methods for statements
    void visit(frontend::VarDecl* node) override;
    void visit(frontend::ReturnStmt* node) override;
    void visit(frontend::IfStmt* node) override;
    void visit(frontend::Block* node) override;

    // Control flow
    void visit(frontend::PickStmt* node) override;
    void visit(frontend::TillLoop* node) override;
    void visit(frontend::WhenLoop* node) override;
    void visit(frontend::DeferStmt* node) override;
    void visit(frontend::ForLoop* node) override;
    void visit(frontend::WhileLoop* node) override;
    void visit(frontend::BreakStmt* node) override;
    void visit(frontend::ContinueStmt* node) override;
    
    // New expression types
    void visit(frontend::WhenExpr* node) override;
    void visit(frontend::AwaitExpr* node) override;
    void visit(frontend::ObjectLiteral* node) override;
    void visit(frontend::MemberAccess* node) override;
    void visit(frontend::UnwrapExpr* node) override;

private:
    void addError(const std::string& msg) {
        errors.push_back(msg);
    }

    std::shared_ptr<Type> getExpressionType(frontend::Expression* expr);
    bool checkTypeCompatibility(const Type& expected, const Type& actual);
};

// Main entry point for type checking
TypeCheckResult checkTypes(frontend::Block* ast);

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_TYPE_CHECKER_H
