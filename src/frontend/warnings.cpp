/**
 * @file warnings.cpp
 * @brief Implementation of warning system for code quality analysis
 */

#include "frontend/warnings.h"
#include <algorithm>
#include <unordered_map>

namespace aria {

// ============================================================================
// WarningConfig Implementation
// ============================================================================

WarningConfig::WarningConfig() : warnings_as_errors_(false) {
    // Enable all warnings by default
    enableAll();
}

void WarningConfig::enable(WarningType type) {
    enabled_warnings_.insert(type);
}

void WarningConfig::disable(WarningType type) {
    enabled_warnings_.erase(type);
}

void WarningConfig::enableAll() {
    enabled_warnings_.insert(WarningType::UNUSED_VARIABLE);
    enabled_warnings_.insert(WarningType::UNUSED_PARAMETER);
    enabled_warnings_.insert(WarningType::UNUSED_FUNCTION);
    enabled_warnings_.insert(WarningType::DEAD_CODE);
    enabled_warnings_.insert(WarningType::UNREACHABLE_CODE);
    enabled_warnings_.insert(WarningType::IMPLICIT_CONVERSION);
    enabled_warnings_.insert(WarningType::EMPTY_BLOCK);
    enabled_warnings_.insert(WarningType::CONSTANT_CONDITION);
    enabled_warnings_.insert(WarningType::SHADOWING);
}

void WarningConfig::disableAll() {
    enabled_warnings_.clear();
}

bool WarningConfig::isEnabled(WarningType type) const {
    return enabled_warnings_.find(type) != enabled_warnings_.end();
}

void WarningConfig::setWarningsAsErrors(bool enabled) {
    warnings_as_errors_ = enabled;
}

bool WarningConfig::treatWarningsAsErrors() const {
    return warnings_as_errors_;
}

std::string WarningConfig::warningTypeToString(WarningType type) {
    switch (type) {
        case WarningType::UNUSED_VARIABLE:
            return "unused-variable";
        case WarningType::UNUSED_PARAMETER:
            return "unused-parameter";
        case WarningType::UNUSED_FUNCTION:
            return "unused-function";
        case WarningType::DEAD_CODE:
            return "dead-code";
        case WarningType::UNREACHABLE_CODE:
            return "unreachable-code";
        case WarningType::IMPLICIT_CONVERSION:
            return "implicit-conversion";
        case WarningType::EMPTY_BLOCK:
            return "empty-block";
        case WarningType::CONSTANT_CONDITION:
            return "constant-condition";
        case WarningType::SHADOWING:
            return "shadowing";
        default:
            return "unknown";
    }
}

// ============================================================================
// WarningAnalyzer Implementation
// ============================================================================

WarningAnalyzer::WarningAnalyzer(DiagnosticEngine& diags, WarningConfig& config)
    : diags_(diags), config_(config), warning_count_(0) {}

void WarningAnalyzer::analyze(const ASTNode* ast) {
    if (!ast) return;
    
    // NOTE: Full AST walking implementation will be added in future phases
    // when we need comprehensive semantic analysis for production use.
    // For now, this is a stub that supports the CLI interface.
    
    // Run all enabled analysis passes
    if (config_.isEnabled(WarningType::UNUSED_VARIABLE) ||
        config_.isEnabled(WarningType::UNUSED_PARAMETER)) {
        analyzeUnusedVariables(ast);
    }
    
    if (config_.isEnabled(WarningType::DEAD_CODE) ||
        config_.isEnabled(WarningType::UNREACHABLE_CODE)) {
        analyzeUnreachableCode(ast);
    }
    
    if (config_.isEnabled(WarningType::IMPLICIT_CONVERSION)) {
        analyzeImplicitConversions(ast);
    }
}

void WarningAnalyzer::analyzeUnusedVariables(const ASTNode* ast) {
    (void)ast; // Suppress unused parameter warning
    // Stub: Full implementation will walk AST to find unused variables
    // Will be implemented when we add comprehensive semantic analysis
}

void WarningAnalyzer::analyzeUnusedParameters(const FuncDeclStmt* func) {
    (void)func; // Suppress unused parameter warning
    if (!config_.isEnabled(WarningType::UNUSED_PARAMETER)) {
        return;
    }
    
    // Stub: Would analyze function body to see if parameters are used
}

void WarningAnalyzer::analyzeDeadCode(const BlockStmt* block) {
    (void)block; // Suppress unused parameter warning
    if (!config_.isEnabled(WarningType::DEAD_CODE)) {
        return;
    }
    
    // Stub: Would look for statements after terminal statements
}

void WarningAnalyzer::analyzeUnreachableCode(const ASTNode* ast) {
    (void)ast; // Suppress unused parameter warning
    if (!config_.isEnabled(WarningType::UNREACHABLE_CODE)) {
        return;
    }
    
    // Stub: Would check for code after returns, infinite loops, etc.
}

void WarningAnalyzer::analyzeImplicitConversions(const ASTNode* ast) {
    (void)ast; // Suppress unused parameter warning
    if (!config_.isEnabled(WarningType::IMPLICIT_CONVERSION)) {
        return;
    }
    
    // Stub: Would check for implicit type conversions that may lose precision
}

std::unordered_map<std::string, WarningAnalyzer::VariableUsage> 
WarningAnalyzer::buildUsageMap(const BlockStmt* block) {
    (void)block; // Suppress unused parameter warning
    std::unordered_map<std::string, VariableUsage> usage_map;
    // Stub: Would walk block to build usage map
    return usage_map;
}

void WarningAnalyzer::markUsed(
    std::unordered_map<std::string, VariableUsage>& usage_map,
    const std::string& name) {
    auto it = usage_map.find(name);
    if (it != usage_map.end()) {
        it->second.is_used = true;
    }
}

bool WarningAnalyzer::isTerminalStatement(const ASTNode* stmt) {
    (void)stmt; // Suppress unused parameter warning
    // Stub: Would check if statement is return, break, continue, or fail
    return false;
}

void WarningAnalyzer::emitWarning(WarningType type, const SourceLocation& loc,
                                 const std::string& message) {
    if (!config_.isEnabled(type)) {
        return;
    }
    
    warning_count_++;
    
    // Format warning message with type
    std::string formatted_message = "[" + WarningConfig::warningTypeToString(type) + "] " + message;
    
    if (config_.treatWarningsAsErrors()) {
        diags_.error(loc, formatted_message);
    } else {
        diags_.warning(loc, formatted_message);
    }
}

// ============================================================================
// WarningFlagParser Implementation
// ============================================================================

void WarningFlagParser::parseFlag(const std::string& flag, WarningConfig& config) {
    if (flag == "-Werror") {
        config.setWarningsAsErrors(true);
        return;
    }
    
    if (flag == "-Wall") {
        config.enableAll();
        return;
    }
    
    if (flag == "-Wno-all") {
        config.disableAll();
        return;
    }
    
    // Parse -W<warning> and -Wno-<warning>
    if (flag.size() > 2 && flag[0] == '-' && flag[1] == 'W') {
        std::string rest = flag.substr(2);
        bool disable = false;
        
        if (rest.size() > 3 && rest.substr(0, 3) == "no-") {
            disable = true;
            rest = rest.substr(3);
        }
        
        WarningType type = stringToWarningType(rest);
        if (type != static_cast<WarningType>(-1)) {
            if (disable) {
                config.disable(type);
            } else {
                config.enable(type);
            }
        }
    }
}

std::vector<std::string> WarningFlagParser::getSupportedFlags() {
    return {
        "-Wall",
        "-Werror",
        "-Wno-all",
        "-Wunused-variable",
        "-Wno-unused-variable",
        "-Wunused-parameter",
        "-Wno-unused-parameter",
        "-Wunused-function",
        "-Wno-unused-function",
        "-Wdead-code",
        "-Wno-dead-code",
        "-Wunreachable-code",
        "-Wno-unreachable-code",
        "-Wimplicit-conversion",
        "-Wno-implicit-conversion",
        "-Wempty-block",
        "-Wno-empty-block",
        "-Wconstant-condition",
        "-Wno-constant-condition",
        "-Wshadowing",
        "-Wno-shadowing"
    };
}

WarningType WarningFlagParser::stringToWarningType(const std::string& name) {
    if (name == "unused-variable") return WarningType::UNUSED_VARIABLE;
    if (name == "unused-parameter") return WarningType::UNUSED_PARAMETER;
    if (name == "unused-function") return WarningType::UNUSED_FUNCTION;
    if (name == "dead-code") return WarningType::DEAD_CODE;
    if (name == "unreachable-code") return WarningType::UNREACHABLE_CODE;
    if (name == "implicit-conversion") return WarningType::IMPLICIT_CONVERSION;
    if (name == "empty-block") return WarningType::EMPTY_BLOCK;
    if (name == "constant-condition") return WarningType::CONSTANT_CONDITION;
    if (name == "shadowing") return WarningType::SHADOWING;
    
    return static_cast<WarningType>(-1); // Unknown warning type
}

} // namespace aria
