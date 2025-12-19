/**
 * @file warnings.h
 * @brief Warning system for code quality analysis
 * 
 * Implements configurable warnings for:
 * - Unused variables
 * - Dead code detection
 * - Type mismatches (implicit conversions)
 * - Unreachable code
 * 
 * Integrates with DiagnosticEngine for consistent output
 */

#ifndef ARIA_WARNINGS_H
#define ARIA_WARNINGS_H

#include "diagnostics.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aria {

// Forward declarations (for future AST integration)
class ASTNode;
class BlockStmt;
class FuncDeclStmt;

/**
 * Warning categories that can be individually enabled/disabled
 */
enum class WarningType {
    UNUSED_VARIABLE,      // Variable declared but never used
    UNUSED_PARAMETER,     // Function parameter never used
    UNUSED_FUNCTION,      // Function declared but never called
    DEAD_CODE,            // Code after return/break/continue
    UNREACHABLE_CODE,     // Code that can never be executed
    IMPLICIT_CONVERSION,  // Type conversion that may lose precision
    EMPTY_BLOCK,          // Empty if/else/while block
    CONSTANT_CONDITION,   // if(true) or while(false)
    SHADOWING,            // Variable shadows outer scope variable
};

/**
 * Warning configuration and control
 */
class WarningConfig {
public:
    WarningConfig();
    
    // Enable/disable specific warnings
    void enable(WarningType type);
    void disable(WarningType type);
    void enableAll();
    void disableAll();
    
    // Check if warning is enabled
    bool isEnabled(WarningType type) const;
    
    // Treat warnings as errors
    void setWarningsAsErrors(bool enabled);
    bool treatWarningsAsErrors() const;
    
    // Convert warning type to string
    static std::string warningTypeToString(WarningType type);
    
private:
    std::unordered_set<WarningType> enabled_warnings_;
    bool warnings_as_errors_;
};

/**
 * Warning analysis pass that detects code quality issues
 */
class WarningAnalyzer {
public:
    WarningAnalyzer(DiagnosticEngine& diags, WarningConfig& config);
    
    // Analyze an AST and emit warnings
    // Note: Currently a stub - full AST analysis will be implemented in future phases
    void analyze(const ASTNode* ast);
    
    // Individual analysis passes (stubs for now)
    void analyzeUnusedVariables(const ASTNode* ast);
    void analyzeUnusedParameters(const FuncDeclStmt* func);
    void analyzeDeadCode(const BlockStmt* block);
    void analyzeUnreachableCode(const ASTNode* ast);
    void analyzeImplicitConversions(const ASTNode* ast);
    
    // Get warning count
    int getWarningCount() const { return warning_count_; }
    
private:
    DiagnosticEngine& diags_;
    WarningConfig& config_;
    int warning_count_;
    
    // Helper: Track variable usage
    struct VariableUsage {
        std::string name;
        SourceLocation declaration_loc;
        bool is_used;
        bool is_parameter;
    };
    
    // Helper: Build usage map for scope (stub)
    std::unordered_map<std::string, VariableUsage> buildUsageMap(const BlockStmt* block);
    
    // Helper: Mark variable as used
    void markUsed(std::unordered_map<std::string, VariableUsage>& usage_map, 
                  const std::string& name);
    
    // Helper: Check if statement is terminal (return/break/continue) (stub)
    bool isTerminalStatement(const ASTNode* stmt);
    
    // Helper: Emit warning through diagnostic system
    void emitWarning(WarningType type, const SourceLocation& loc, 
                    const std::string& message);
};

/**
 * Parse warning control flags from command line
 * Examples: -Wunused-variable, -Wno-dead-code, -Werror
 */
class WarningFlagParser {
public:
    static void parseFlag(const std::string& flag, WarningConfig& config);
    static std::vector<std::string> getSupportedFlags();
    
private:
    static WarningType stringToWarningType(const std::string& name);
};

} // namespace aria

#endif // ARIA_WARNINGS_H
