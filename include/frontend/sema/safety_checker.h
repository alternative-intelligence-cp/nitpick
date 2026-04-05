/**
 * @file safety_checker.h
 * @brief Safety checking and warning system for Aria
 * 
 * Implements the Aria Safety Contract - explicit acknowledgment of unsafe operations.
 * See SAFETY.md for full documentation.
 */

#ifndef ARIA_SAFETY_CHECKER_H
#define ARIA_SAFETY_CHECKER_H

#include <string>
#include <vector>
#include <map>
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"

namespace aria {

/**
 * Safety levels for compilation
 */
enum class SafetyLevel {
    PERMISSIVE = 0,  // Warnings only (default)
    STRICT = 1,      // Warnings become errors without acknowledgment
    PARANOID = 2     // Maximum checks, all unsafe ops must be acknowledged
};

/**
 * Types of unsafe operations that require acknowledgment
 */
enum class UnsafeOperation {
    WILD_MEMORY,      // wild keyword - manual memory management
    ASYNC_CONCURRENCY, // async keyword - concurrent operations
    POINTER_OPS,      // @ operator - raw pointer manipulation
    FFI_EXTERN,       // extern keyword - foreign function interface
    WILDX_EXECUTABLE, // wildx keyword - executable memory
    POINTER_CAST,     // Explicit pointer type casts
    POINTER_ARITHMETIC // Pointer arithmetic operations
};

/**
 * Safety warning/error information
 */
struct SafetyIssue {
    UnsafeOperation operation;
    std::string message;
    std::string suggestion;
    std::string file;
    int line;
    int column;
    bool acknowledged;  // Has explicit SAFETY comment
};

/**
 * Safety checker for Aria programs
 * 
 * Validates that all unsafe operations are:
 * 1. Explicitly marked with unsafe keywords
 * 2. Acknowledged with SAFETY comments (in strict/paranoid mode)
 * 3. Properly documented
 */
class SafetyChecker {
public:
    SafetyChecker(SafetyLevel level = SafetyLevel::PERMISSIVE);
    
    /**
     * Check entire AST for safety issues
     * @param root Root AST node
     * @param sourceFile Path to the source file being checked
     * @return True if safe or warnings only, false if errors
     */
    bool check(ASTNode* root, const std::string& sourceFile = "");
    
    /**
     * Parse SAFETY acknowledgment comments from source text
     * Scans for lines matching "// SAFETY: <reason>" and records them
     * for acknowledgment matching.
     * @param source The raw source text of the file
     */
    void parseSafetyComments(const std::string& source);
    
    /**
     * Get all safety issues found
     */
    const std::vector<SafetyIssue>& getIssues() const { return issues; }
    
    /**
     * Check if there are any errors (vs just warnings)
     */
    bool hasErrors() const;
    
    /**
     * Print safety warnings/errors to stderr
     */
    void printIssues() const;
    
    /**
     * Set safety level
     */
    void setSafetyLevel(SafetyLevel level) { safety_level = level; }
    
private:
    SafetyLevel safety_level;
    std::string sourceFile;  // Source file path for diagnostics
    std::vector<SafetyIssue> issues;
    std::map<int, std::string> safety_acknowledgments; // line -> comment
    
    // AST traversal methods
    void checkNode(ASTNode* node);
    void checkFuncDecl(FuncDeclStmt* func);
    void checkVarDecl(VarDeclStmt* var);
    void checkExpression(ASTNode* expr);
    void checkStatement(ASTNode* stmt);
    
    // Specific safety checks
    void checkWildMemory(VarDeclStmt* var);
    void checkAsyncFunction(FuncDeclStmt* func);
    void checkPointerOperation(ASTNode* expr);
    void checkExternCall(CallExpr* call);
    
    // Helper methods
    bool hasAcknowledgment(int line);
    void addIssue(UnsafeOperation op, const std::string& msg, 
                  const std::string& suggestion, int line, int col);
    std::string unsafeOperationName(UnsafeOperation op) const;
    std::string getHelpText(UnsafeOperation op) const;
};

} // namespace aria

#endif // ARIA_SAFETY_CHECKER_H
