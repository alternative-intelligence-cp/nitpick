#ifndef ARIA_SEMA_CLOSURE_ANALYZER_H
#define ARIA_SEMA_CLOSURE_ANALYZER_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "symbol_table.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

namespace aria {
namespace sema {

/**
 * ClosureAnalyzer - Analyzes lambda expressions to detect captures
 * 
 * Phase 4.5.2: Closure Implementation
 * Based on research_016_functional_types.txt
 * 
 * Responsibilities:
 * - Walk lambda body to find variable references
 * - Identify which variables are captured from outer scopes
 * - Determine capture mode (BY_VALUE, BY_REFERENCE, BY_MOVE)
 * - Populate LambdaExpr::capturedVars
 * - Validate lifetime constraints (Appendage Theory)
 * 
 * Capture Mode Rules:
 * - BY_VALUE: Primitives (int, float) and immutable captures (default)
 * - BY_REFERENCE: Mutable references (&mut), variables modified in lambda
 * - BY_MOVE: Ownership transfer (wild pointers, large buffers)
 * 
 * Appendage Theory:
 * - Closure (Appendage) cannot outlive captured variables (Host)
 * - Stack closures cannot escape their scope
 * - Heap closures require promoted environments
 */
class ClosureAnalyzer {
private:
    SymbolTable* symbolTable;
    std::vector<std::string> errors;
    
    // Current lambda being analyzed
    LambdaExpr* currentLambda;
    
    // Parameter names for current lambda (not captures)
    std::unordered_set<std::string> parameterNames;
    
    // Local variables declared within the lambda body (not captures)
    std::unordered_set<std::string> localVariables;
    
    // Captured variable names and their usage info
    struct CaptureInfo {
        std::string name;
        bool isMutated;      // Modified in lambda body
        bool isAddressTaken; // Address-of operator (@) used
        int usageCount;
    };
    std::unordered_map<std::string, CaptureInfo> captures;
    
public:
    ClosureAnalyzer(SymbolTable* symTable);
    
    /**
     * Analyze a lambda expression to detect and classify captures
     * 
     * Process:
     * 1. Collect parameter names (these are NOT captures)
     * 2. Walk the lambda body AST
     * 3. For each identifier reference:
     *    a. Skip if it's a parameter
     *    b. Skip if it's a local variable declared in lambda
     *    c. Check if it's from an outer scope (captured)
     * 4. Determine capture mode based on usage
     * 5. Populate lambda->capturedVars
     * 
     * @param lambda The lambda expression to analyze
     * @return true if analysis succeeds, false if errors
     */
    bool analyzeLambda(LambdaExpr* lambda);
    
    /**
     * Infer return type of lambda from its body
     * 
     * Walks the lambda body to find all return/pass statements,
     * collects their types, and infers a unified return type.
     * If the lambda already has an explicit return type annotation,
     * this validates it. Otherwise, it sets the inferred type.
     * 
     * @param lambda The lambda expression to infer type for
     * @return true if inference succeeds, false if errors
     */
    bool inferReturnType(LambdaExpr* lambda);
    
    /**
     * Get accumulated errors
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
private:
    /**
     * Walk AST node to find all identifier references
     */
    void walkNode(ASTNode* node);
    
    /**
     * Handle identifier expression - check if it's a capture
     */
    void handleIdentifier(IdentifierExpr* expr);
    
    /**
     * Handle assignment - mark captured variable as mutated
     */
    void handleAssignment(AssignmentExpr* expr);
    
    /**
     * Handle unary @ operator - mark as address-taken
     */
    void handleAddressOf(UnaryExpr* expr);
    
    /**
     * Handle variable declaration - add to local variables
     */
    void handleVarDecl(VarDeclStmt* stmt);
    
    /**
     * Check if identifier is from outer scope (potential capture)
     */
    bool isFromOuterScope(const std::string& name);
    
    /**
     * Determine capture mode based on usage patterns
     */
    LambdaExpr::CaptureMode determineCaptureMode(const CaptureInfo& info);
    
    /**
     * Check if type should be captured by value
     * Primitives and small immutable types
     */
    bool shouldCaptureByValue(const std::string& varName);
    
    /**
     * Validate lifetime constraints (Appendage Theory)
     * Closure cannot outlive captured variables
     */
    bool validateLifetimes();
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_CLOSURE_ANALYZER_H
