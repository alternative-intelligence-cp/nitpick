#ifndef ARIA_SEMA_ASYNC_ANALYZER_H
#define ARIA_SEMA_ASYNC_ANALYZER_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include <string>
#include <vector>
#include <memory>

namespace aria {
namespace sema {

/**
 * AsyncSemanticAnalyzer - Validates async/await semantics
 * 
 * Phase 4.5.3: Async/Await Semantic Analysis
 * 
 * Based on research_029_async_await_system.txt:
 * - Section 3.2: "await is only valid within an async function or async block. 
 *   Usage elsewhere results in a compile-time error (E_ASYNC_OUTSIDE_CONTEXT)"
 * - Section 4.1: "The expression following await must evaluate to a type that 
 *   implements the Future trait"
 * 
 * Responsibilities:
 * - Validate await expressions only appear in async functions/blocks
 * - Check that await operands implement Future trait (deferred to later phase)
 * - Track async context during AST traversal
 * 
 * Error Codes:
 * - E_ASYNC_OUTSIDE_CONTEXT: await used outside async function
 * - E_ASYNC_NON_FUTURE: await operand does not implement Future trait
 */
class AsyncSemanticAnalyzer {
private:
    std::vector<std::string> errors;
    
    // Track whether we're currently inside an async function
    bool inAsyncContext;
    
    // Current function name (for error messages)
    std::string currentFunctionName;
    
    /**
     * Analyze an expression for async/await violations
     */
    void analyzeExpression(ASTNodePtr expr);
    
    /**
     * Analyze a statement for async/await violations
     */
    void analyzeStatement(ASTNodePtr stmt);
    
    /**
     * Analyze an await expression
     * - Check: Must be in async context
     * - Check: Operand must be Future (deferred to type system phase)
     */
    void analyzeAwaitExpr(AwaitExpr* awaitExpr);
    
public:
    AsyncSemanticAnalyzer();
    ~AsyncSemanticAnalyzer();
    
    /**
     * Analyze a program's async/await semantics
     * Entry point for semantic analysis
     */
    void analyze(ASTNodePtr root);
    
    /**
     * Analyze a function declaration
     * Sets async context if function is async
     */
    void analyzeFuncDecl(FuncDeclStmt* funcDecl);
    
    /**
     * Check if any errors occurred
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Get accumulated errors
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Clear all errors
     */
    void clearErrors() { errors.clear(); }
    
    /**
     * Add an error message
     */
    void error(const std::string& message);
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_ASYNC_ANALYZER_H
