/**
 * @file safety_checker.cpp
 * @brief Implementation of Aria Safety Contract enforcement
 */

#include "frontend/sema/safety_checker.h"
#include "frontend/token.h"
#include <iostream>
#include <sstream>
#include <regex>

namespace aria {

SafetyChecker::SafetyChecker(SafetyLevel level) 
    : safety_level(level) {}

bool SafetyChecker::check(ASTNode* root, const std::string& file) {
    issues.clear();
    safety_acknowledgments.clear();
    sourceFile = file;
    
    if (!root) {
        return true;
    }
    
    checkNode(root);
    
    return !hasErrors();
}

void SafetyChecker::parseSafetyComments(const std::string& source) {
    // Scan source for "// SAFETY: <reason>" comments and record line → reason
    std::istringstream stream(source);
    std::string line;
    int lineNum = 1;
    
    while (std::getline(stream, line)) {
        // Match "// SAFETY:" anywhere in the line (allows trailing comments)
        auto pos = line.find("// SAFETY:");
        if (pos != std::string::npos) {
            std::string reason = line.substr(pos + 10); // Skip "// SAFETY:"
            // Trim leading whitespace from reason
            auto start = reason.find_first_not_of(" \t");
            if (start != std::string::npos) {
                reason = reason.substr(start);
            }
            safety_acknowledgments[lineNum] = reason;
        }
        lineNum++;
    }
}

bool SafetyChecker::hasErrors() const {
    if (safety_level == SafetyLevel::PERMISSIVE) {
        return false; // All issues are warnings only
    }
    
    // In STRICT/PARANOID mode, unacknowledged unsafe ops are errors
    for (const auto& issue : issues) {
        if (!issue.acknowledged && safety_level >= SafetyLevel::STRICT) {
            return true;
        }
    }
    
    return false;
}

void SafetyChecker::printIssues() const {
    for (const auto& issue : issues) {
        bool is_error = (safety_level >= SafetyLevel::STRICT && !issue.acknowledged);
        
        std::cerr << (is_error ? "error: " : "warning: ")
                  << "[safety] " << issue.message << std::endl;
        
        if (!issue.file.empty()) {
            std::cerr << "  --> " << issue.file << ":" 
                      << issue.line << ":" << issue.column << std::endl;
        }
        
        std::cerr << "   |" << std::endl;
        std::cerr << "   = note: " << unsafeOperationName(issue.operation) 
                  << " operation detected" << std::endl;
        
        if (!issue.suggestion.empty()) {
            std::cerr << "   = help: " << issue.suggestion << std::endl;
        }
        
        std::cerr << "   = help: " << getHelpText(issue.operation) << std::endl;
        
        if (safety_level >= SafetyLevel::STRICT && !issue.acknowledged) {
            std::cerr << "   = note: add 'SAFETY:' comment to acknowledge this risk" 
                      << std::endl;
        }
        
        std::cerr << std::endl;
    }
    
    if (!issues.empty()) {
        std::cerr << "For more information, see SAFETY.md" << std::endl;
        
        if (safety_level == SafetyLevel::PERMISSIVE) {
            std::cerr << "To enforce safety acknowledgments, use: --strict or --paranoid" 
                      << std::endl;
        }
    }
}

void SafetyChecker::checkNode(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case ASTNode::NodeType::PROGRAM: {
            ProgramNode* program = static_cast<ProgramNode*>(node);
            for (const auto& decl : program->declarations) {
                checkNode(decl.get());
            }
            break;
        }
        
        case ASTNode::NodeType::FUNC_DECL: {
            checkFuncDecl(static_cast<FuncDeclStmt*>(node));
            break;
        }
        
        case ASTNode::NodeType::VAR_DECL: {
            checkVarDecl(static_cast<VarDeclStmt*>(node));
            break;
        }
        
        default:
            // Check expressions and statements
            checkExpression(node);
            checkStatement(node);
            break;
    }
}

void SafetyChecker::checkFuncDecl(FuncDeclStmt* func) {
    if (!func) return;
    
    // Check for async functions — potential data races
    if (func->isAsync) {
        checkAsyncFunction(func);
    }
    
    // Check for extern functions — FFI boundary
    if (func->isExtern) {
        bool acknowledged = hasAcknowledgment(func->line);
        addIssue(
            UnsafeOperation::FFI_EXTERN,
            "extern function bypasses Aria safety guarantees",
            "validate all FFI inputs and outputs at the boundary",
            func->line,
            func->column
        );
        if (acknowledged) {
            issues.back().acknowledged = true;
        }
    }
    
    // Check function body
    if (func->body) {
        checkStatement(func->body.get());
    }
}

void SafetyChecker::checkVarDecl(VarDeclStmt* var) {
    if (!var) return;
    
    // Check for wild memory allocation — manual memory management
    if (var->isWild) {
        checkWildMemory(var);
    }
    
    // Check for wildx — executable memory (JIT, security implications)
    if (var->isWildx) {
        bool acknowledged = hasAcknowledgment(var->line);
        addIssue(
            UnsafeOperation::WILDX_EXECUTABLE,
            "wildx allocates executable memory — security risk",
            "ensure JIT-generated code is validated before execution",
            var->line,
            var->column
        );
        if (acknowledged) {
            issues.back().acknowledged = true;
        }
    }
    
    // Check initializer expression
    if (var->initializer) {
        checkExpression(var->initializer.get());
    }
}

void SafetyChecker::checkExpression(ASTNode* expr) {
    if (!expr) return;
    
    // Recursively check child expressions
    switch (expr->type) {
        case ASTNode::NodeType::BINARY_OP: {
            BinaryExpr* binop = static_cast<BinaryExpr*>(expr);
            checkExpression(binop->left.get());
            checkExpression(binop->right.get());
            break;
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            UnaryExpr* unary = static_cast<UnaryExpr*>(expr);
            
            // Check for unsafe pointer operations via operator token
            using TT = frontend::TokenType;
            TT opType = unary->op.type;
            
            if (opType == TT::TOKEN_AT) {
                // @ — address-of operator (raw pointer creation)
                bool acknowledged = hasAcknowledgment(unary->line);
                addIssue(
                    UnsafeOperation::POINTER_OPS,
                    "address-of operator (@) creates raw pointer",
                    "prefer borrow ($i/$m) instead of raw pointer when possible",
                    unary->line,
                    unary->column
                );
                if (acknowledged) {
                    issues.back().acknowledged = true;
                }
            } else if (opType == TT::TOKEN_LEFT_ARROW) {
                // <- — pointer dereference (value <- ptr)
                bool acknowledged = hasAcknowledgment(unary->line);
                addIssue(
                    UnsafeOperation::POINTER_OPS,
                    "pointer dereference (<-) bypasses bounds checking",
                    "ensure pointer is valid before dereferencing",
                    unary->line,
                    unary->column
                );
                if (acknowledged) {
                    issues.back().acknowledged = true;
                }
            }
            
            checkExpression(unary->operand.get());
            break;
        }
        
        case ASTNode::NodeType::CALL: {
            CallExpr* call = static_cast<CallExpr*>(expr);
            for (const auto& arg : call->arguments) {
                checkExpression(arg.get());
            }
            break;
        }
        
        default:
            break;
    }
}

void SafetyChecker::checkStatement(ASTNode* stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case ASTNode::NodeType::BLOCK: {
            BlockStmt* block = static_cast<BlockStmt*>(stmt);
            for (const auto& s : block->statements) {
                checkNode(s.get());
            }
            break;
        }
        
        case ASTNode::NodeType::IF: {
            IfStmt* ifStmt = static_cast<IfStmt*>(stmt);
            checkExpression(ifStmt->condition.get());
            checkStatement(ifStmt->thenBranch.get());
            if (ifStmt->elseBranch) {
                checkStatement(ifStmt->elseBranch.get());
            }
            break;
        }
        
        case ASTNode::NodeType::WHILE: {
            WhileStmt* whileStmt = static_cast<WhileStmt*>(stmt);
            checkExpression(whileStmt->condition.get());
            checkStatement(whileStmt->body.get());
            break;
        }
        
        case ASTNode::NodeType::FOR: {
            ForStmt* forStmt = static_cast<ForStmt*>(stmt);
            if (forStmt->initializer) checkStatement(forStmt->initializer.get());
            if (forStmt->condition) checkExpression(forStmt->condition.get());
            if (forStmt->update) checkStatement(forStmt->update.get());
            checkStatement(forStmt->body.get());
            break;
        }
        
        case ASTNode::NodeType::RETURN: {
            ReturnStmt* ret = static_cast<ReturnStmt*>(stmt);
            if (ret->value) {
                checkExpression(ret->value.get());
            }
            break;
        }
        
        case ASTNode::NodeType::EXPRESSION_STMT: {
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(stmt);
            checkExpression(exprStmt->expression.get());
            break;
        }
        
        default:
            break;
    }
}

void SafetyChecker::checkWildMemory(VarDeclStmt* var) {
    bool acknowledged = hasAcknowledgment(var->line);
    
    addIssue(
        UnsafeOperation::WILD_MEMORY,
        "wild memory allocation without automatic management",
        "consider using 'defer aria.free(...)' for automatic cleanup",
        var->line,
        var->column
    );
    
    if (acknowledged) {
        issues.back().acknowledged = true;
    }
}

void SafetyChecker::checkAsyncFunction(FuncDeclStmt* func) {
    bool acknowledged = hasAcknowledgment(func->line);
    
    addIssue(
        UnsafeOperation::ASYNC_CONCURRENCY,
        "async function may introduce data races",
        "ensure proper synchronization for shared mutable state",
        func->line,
        func->column
    );
    
    if (acknowledged) {
        issues.back().acknowledged = true;
    }
}

void SafetyChecker::checkPointerOperation(ASTNode* expr) {
    if (!expr) return;
    bool acknowledged = hasAcknowledgment(expr->line);
    addIssue(
        UnsafeOperation::POINTER_OPS,
        "raw pointer operation detected",
        "ensure pointer validity and bounds are checked manually",
        expr->line,
        expr->column
    );
    if (acknowledged) {
        issues.back().acknowledged = true;
    }
}

void SafetyChecker::checkExternCall(CallExpr* call) {
    if (!call) return;
    bool acknowledged = hasAcknowledgment(call->line);
    addIssue(
        UnsafeOperation::FFI_EXTERN,
        "call to extern function — FFI boundary",
        "validate all data crossing the FFI boundary",
        call->line,
        call->column
    );
    if (acknowledged) {
        issues.back().acknowledged = true;
    }
}

bool SafetyChecker::hasAcknowledgment(int line) {
    return safety_acknowledgments.find(line) != safety_acknowledgments.end();
}

void SafetyChecker::addIssue(UnsafeOperation op, const std::string& msg,
                            const std::string& suggestion, int line, int col) {
    SafetyIssue issue;
    issue.operation = op;
    issue.message = msg;
    issue.suggestion = suggestion;
    issue.file = sourceFile;  // Set from check() parameter
    issue.line = line;
    issue.column = col;
    issue.acknowledged = false;
    
    issues.push_back(issue);
}

std::string SafetyChecker::unsafeOperationName(UnsafeOperation op) const {
    switch (op) {
        case UnsafeOperation::WILD_MEMORY:
            return "wild memory";
        case UnsafeOperation::ASYNC_CONCURRENCY:
            return "async/concurrency";
        case UnsafeOperation::POINTER_OPS:
            return "pointer operation";
        case UnsafeOperation::FFI_EXTERN:
            return "FFI/extern";
        case UnsafeOperation::WILDX_EXECUTABLE:
            return "executable memory";
        case UnsafeOperation::POINTER_CAST:
            return "pointer cast";
        case UnsafeOperation::POINTER_ARITHMETIC:
            return "pointer arithmetic";
        default:
            return "unsafe operation";
    }
}

std::string SafetyChecker::getHelpText(UnsafeOperation op) const {
    switch (op) {
        case UnsafeOperation::WILD_MEMORY:
            return "wild memory must be manually freed - you are responsible for deallocation";
        case UnsafeOperation::ASYNC_CONCURRENCY:
            return "async code may race without proper synchronization - see docs/concurrency";
        case UnsafeOperation::POINTER_OPS:
            return "pointer operations bypass bounds checking - ensure validity manually";
        case UnsafeOperation::FFI_EXTERN:
            return "extern functions bypass Aria safety - validate all FFI boundaries";
        case UnsafeOperation::WILDX_EXECUTABLE:
            return "executable memory has security implications - ensure proper validation";
        case UnsafeOperation::POINTER_CAST:
            return "pointer casts can violate type safety - verify correctness";
        case UnsafeOperation::POINTER_ARITHMETIC:
            return "pointer arithmetic can cause buffer overflows - validate bounds";
        default:
            return "see SAFETY.md for details";
    }
}

} // namespace aria
