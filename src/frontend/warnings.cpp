/**
 * @file warnings.cpp
 * @brief Implementation of warning system for code quality analysis
 */

#include "frontend/warnings.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
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

// Helper: collect all identifier names referenced in an AST subtree
static void collectIdentifiers(const ASTNode* node, std::unordered_set<std::string>& names) {
    if (!node) return;
    switch (node->type) {
        case ASTNode::NodeType::IDENTIFIER: {
            auto* id = static_cast<const IdentifierExpr*>(node);
            names.insert(id->name);
            break;
        }
        case ASTNode::NodeType::BLOCK: {
            auto* blk = static_cast<const BlockStmt*>(node);
            for (const auto& s : blk->statements) collectIdentifiers(s.get(), names);
            break;
        }
        case ASTNode::NodeType::BINARY_OP: {
            auto* bin = static_cast<const BinaryExpr*>(node);
            collectIdentifiers(bin->left.get(), names);
            collectIdentifiers(bin->right.get(), names);
            break;
        }
        case ASTNode::NodeType::UNARY_OP: {
            auto* unary = static_cast<const UnaryExpr*>(node);
            collectIdentifiers(unary->operand.get(), names);
            break;
        }
        case ASTNode::NodeType::CALL: {
            auto* call = static_cast<const CallExpr*>(node);
            collectIdentifiers(call->callee.get(), names);
            for (const auto& arg : call->arguments) collectIdentifiers(arg.get(), names);
            break;
        }
        case ASTNode::NodeType::VAR_DECL: {
            auto* var = static_cast<const VarDeclStmt*>(node);
            if (var->initializer) collectIdentifiers(var->initializer.get(), names);
            break;
        }
        case ASTNode::NodeType::RETURN: {
            auto* ret = static_cast<const ReturnStmt*>(node);
            if (ret->value) collectIdentifiers(ret->value.get(), names);
            break;
        }
        case ASTNode::NodeType::IF: {
            auto* ifs = static_cast<const IfStmt*>(node);
            collectIdentifiers(ifs->condition.get(), names);
            collectIdentifiers(ifs->thenBranch.get(), names);
            if (ifs->elseBranch) collectIdentifiers(ifs->elseBranch.get(), names);
            break;
        }
        case ASTNode::NodeType::LOOP: {
            auto* loop = static_cast<const LoopStmt*>(node);
            collectIdentifiers(loop->start.get(), names);
            collectIdentifiers(loop->limit.get(), names);
            if (loop->step) collectIdentifiers(loop->step.get(), names);
            collectIdentifiers(loop->body.get(), names);
            break;
        }
        case ASTNode::NodeType::FUNC_DECL: {
            auto* fn = static_cast<const FuncDeclStmt*>(node);
            if (fn->body) collectIdentifiers(fn->body.get(), names);
            break;
        }
        case ASTNode::NodeType::EXPRESSION_STMT: {
            auto* expr = static_cast<const ExpressionStmt*>(node);
            collectIdentifiers(expr->expression.get(), names);
            break;
        }
        case ASTNode::NodeType::ASSIGNMENT: {
            auto* assign = static_cast<const AssignmentExpr*>(node);
            collectIdentifiers(assign->target.get(), names);
            collectIdentifiers(assign->value.get(), names);
            break;
        }
        case ASTNode::NodeType::MEMBER_ACCESS: {
            auto* ma = static_cast<const MemberAccessExpr*>(node);
            collectIdentifiers(ma->object.get(), names);
            break;
        }
        case ASTNode::NodeType::INDEX: {
            auto* idx = static_cast<const IndexExpr*>(node);
            collectIdentifiers(idx->array.get(), names);
            collectIdentifiers(idx->index.get(), names);
            break;
        }
        default:
            break;
    }
}

void WarningAnalyzer::analyzeUnusedVariables(const ASTNode* ast) {
    if (!ast) return;
    
    switch (ast->type) {
        case ASTNode::NodeType::PROGRAM:
        case ASTNode::NodeType::BLOCK: {
            auto* blk = static_cast<const BlockStmt*>(ast);
            // Build usage map for this scope
            auto usage_map = buildUsageMap(blk);
            
            // Collect all referenced identifiers in the block
            std::unordered_set<std::string> referenced;
            collectIdentifiers(ast, referenced);
            
            // Mark used variables
            for (auto& [name, usage] : usage_map) {
                // A variable is "used" if it appears as an identifier anywhere
                // beyond its own initializer
                if (referenced.count(name)) {
                    usage.is_used = true;
                }
            }
            
            // Emit warnings for unused variables
            for (const auto& [name, usage] : usage_map) {
                if (!usage.is_used && !name.empty() && name[0] != '_') {
                    emitWarning(WarningType::UNUSED_VARIABLE, usage.declaration_loc,
                               "variable '" + name + "' is declared but never used");
                }
            }
            
            // Recurse into nested functions
            for (const auto& stmt : blk->statements) {
                if (stmt && stmt->type == ASTNode::NodeType::FUNC_DECL) {
                    auto* fn = static_cast<const FuncDeclStmt*>(stmt.get());
                    analyzeUnusedParameters(fn);
                    if (fn->body) analyzeUnusedVariables(fn->body.get());
                }
            }
            break;
        }
        default:
            break;
    }
}

void WarningAnalyzer::analyzeUnusedParameters(const FuncDeclStmt* func) {
    if (!func || !config_.isEnabled(WarningType::UNUSED_PARAMETER)) {
        return;
    }
    if (!func->body) return;  // extern functions have no body
    
    // Collect all identifiers used in the function body
    std::unordered_set<std::string> referenced;
    collectIdentifiers(func->body.get(), referenced);
    
    // Check each parameter
    for (const auto& param : func->parameters) {
        if (!param || param->type != ASTNode::NodeType::PARAMETER) continue;
        auto* p = static_cast<const ParameterNode*>(param.get());
        if (!p->paramName.empty() && p->paramName[0] != '_' && !referenced.count(p->paramName)) {
            SourceLocation loc("", p->line, p->column);
            emitWarning(WarningType::UNUSED_PARAMETER, loc,
                       "parameter '" + p->paramName + "' is unused in function '" + func->funcName + "'");
        }
    }
}

void WarningAnalyzer::analyzeDeadCode(const BlockStmt* block) {
    if (!block || !config_.isEnabled(WarningType::DEAD_CODE)) {
        return;
    }
    
    // Look for statements after terminal statements (return/pass/fail/break/continue)
    bool found_terminal = false;
    for (const auto& stmt : block->statements) {
        if (!stmt) continue;
        if (found_terminal) {
            SourceLocation loc("", stmt->line, stmt->column);
            emitWarning(WarningType::DEAD_CODE, loc,
                       "code after terminal statement will never be executed");
            break;  // Only warn once per block
        }
        if (isTerminalStatement(stmt.get())) {
            found_terminal = true;
        }
        // Recurse into nested blocks
        if (stmt->type == ASTNode::NodeType::BLOCK) {
            analyzeDeadCode(static_cast<const BlockStmt*>(stmt.get()));
        } else if (stmt->type == ASTNode::NodeType::FUNC_DECL) {
            auto* fn = static_cast<const FuncDeclStmt*>(stmt.get());
            if (fn->body && fn->body->type == ASTNode::NodeType::BLOCK) {
                analyzeDeadCode(static_cast<const BlockStmt*>(fn->body.get()));
            }
        } else if (stmt->type == ASTNode::NodeType::IF) {
            auto* ifs = static_cast<const IfStmt*>(stmt.get());
            if (ifs->thenBranch && ifs->thenBranch->type == ASTNode::NodeType::BLOCK)
                analyzeDeadCode(static_cast<const BlockStmt*>(ifs->thenBranch.get()));
            if (ifs->elseBranch && ifs->elseBranch->type == ASTNode::NodeType::BLOCK)
                analyzeDeadCode(static_cast<const BlockStmt*>(ifs->elseBranch.get()));
        }
    }
}

void WarningAnalyzer::analyzeUnreachableCode(const ASTNode* ast) {
    if (!ast || !config_.isEnabled(WarningType::UNREACHABLE_CODE)) {
        return;
    }
    
    // Delegate to dead code analysis for blocks
    if (ast->type == ASTNode::NodeType::BLOCK || ast->type == ASTNode::NodeType::PROGRAM) {
        analyzeDeadCode(static_cast<const BlockStmt*>(ast));
    }
    
    // Check for constant-false conditions in if statements
    if (ast->type == ASTNode::NodeType::IF) {
        auto* ifs = static_cast<const IfStmt*>(ast);
        if (ifs->condition && ifs->condition->type == ASTNode::NodeType::LITERAL) {
            auto* lit = static_cast<const LiteralExpr*>(ifs->condition.get());
            if (std::holds_alternative<bool>(lit->value)) {
                bool val = std::get<bool>(lit->value);
                if (!val && ifs->thenBranch) {
                    SourceLocation loc("", ifs->thenBranch->line, ifs->thenBranch->column);
                    emitWarning(WarningType::UNREACHABLE_CODE, loc,
                               "then-branch is unreachable (condition is always false)");
                }
                if (val && ifs->elseBranch) {
                    SourceLocation loc("", ifs->elseBranch->line, ifs->elseBranch->column);
                    emitWarning(WarningType::UNREACHABLE_CODE, loc,
                               "else-branch is unreachable (condition is always true)");
                }
                // Also emit constant-condition warning
                if (config_.isEnabled(WarningType::CONSTANT_CONDITION)) {
                    SourceLocation cloc("", ifs->condition->line, ifs->condition->column);
                    emitWarning(WarningType::CONSTANT_CONDITION, cloc,
                               "condition is always " + std::string(val ? "true" : "false"));
                }
            }
        }
        // Recurse into branches
        if (ifs->thenBranch) analyzeUnreachableCode(ifs->thenBranch.get());
        if (ifs->elseBranch) analyzeUnreachableCode(ifs->elseBranch.get());
    }
    
    // Recurse into blocks and functions
    if (ast->type == ASTNode::NodeType::BLOCK || ast->type == ASTNode::NodeType::PROGRAM) {
        auto* blk = static_cast<const BlockStmt*>(ast);
        for (const auto& stmt : blk->statements) {
            if (stmt) analyzeUnreachableCode(stmt.get());
        }
    } else if (ast->type == ASTNode::NodeType::FUNC_DECL) {
        auto* fn = static_cast<const FuncDeclStmt*>(ast);
        if (fn->body) analyzeUnreachableCode(fn->body.get());
    }
}

void WarningAnalyzer::analyzeImplicitConversions(const ASTNode* ast) {
    if (!ast || !config_.isEnabled(WarningType::IMPLICIT_CONVERSION)) {
        return;
    }
    
    // Aria follows Zero Implicit Conversion Policy — all conversions must be explicit.
    // This pass is a safety net: if any implicit conversions slip through sema,
    // they would be caught here. Currently a no-op since sema enforces this.
    // Future: walk binary expressions and assignments to verify type consistency.
}

std::unordered_map<std::string, WarningAnalyzer::VariableUsage> 
WarningAnalyzer::buildUsageMap(const BlockStmt* block) {
    std::unordered_map<std::string, VariableUsage> usage_map;
    if (!block) return usage_map;
    for (const auto& stmt : block->statements) {
        if (stmt && stmt->type == ASTNode::NodeType::VAR_DECL) {
            auto* var = static_cast<const VarDeclStmt*>(stmt.get());
            VariableUsage usage;
            usage.name = var->varName;
            usage.declaration_loc = SourceLocation("", var->line, var->column);
            usage.is_used = false;
            usage.is_parameter = false;
            usage_map[var->varName] = usage;
        }
    }
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
    if (!stmt) return false;
    switch (stmt->type) {
        case ASTNode::NodeType::RETURN:
        case ASTNode::NodeType::PASS:
        case ASTNode::NodeType::FAIL:
        case ASTNode::NodeType::BREAK:
        case ASTNode::NodeType::CONTINUE:
            return true;
        default:
            return false;
    }
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
