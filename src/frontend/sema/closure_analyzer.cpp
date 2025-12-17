#include "frontend/sema/closure_analyzer.h"
#include <algorithm>

namespace aria {
namespace sema {

ClosureAnalyzer::ClosureAnalyzer(SymbolTable* symTable)
    : symbolTable(symTable), currentLambda(nullptr) {}

bool ClosureAnalyzer::analyzeLambda(LambdaExpr* lambda) {
    if (!lambda) return false;
    
    currentLambda = lambda;
    errors.clear();
    parameterNames.clear();
    localVariables.clear();
    captures.clear();
    
    // Step 1: Collect parameter names (these are NOT captures)
    for (const auto& param : lambda->parameters) {
        if (auto paramNode = std::dynamic_pointer_cast<ParameterNode>(param)) {
            parameterNames.insert(paramNode->paramName);
        }
    }
    
    // Step 2: Walk the lambda body to find all references
    if (lambda->body) {
        walkNode(lambda->body.get());
    }
    
    // Step 3: Determine capture modes and populate lambda->capturedVars
    for (const auto& [name, info] : captures) {
        LambdaExpr::CaptureMode mode = determineCaptureMode(info);
        lambda->capturedVars.emplace_back(name, mode, nullptr);
    }
    
    // Step 4: Validate lifetime constraints
    if (!validateLifetimes()) {
        return false;
    }
    
    return errors.empty();
}

void ClosureAnalyzer::walkNode(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case ASTNode::NodeType::IDENTIFIER:
            handleIdentifier(static_cast<IdentifierExpr*>(node));
            break;
            
        case ASTNode::NodeType::ASSIGNMENT:
            handleAssignment(static_cast<AssignmentExpr*>(node));
            break;
            
        case ASTNode::NodeType::UNARY_OP: {
            auto unary = static_cast<UnaryExpr*>(node);
            if (unary->op.type == frontend::TokenType::TOKEN_AT) {
                handleAddressOf(unary);
            }
            // Recurse into operand
            walkNode(unary->operand.get());
            break;
        }
            
        case ASTNode::NodeType::VAR_DECL:
            handleVarDecl(static_cast<VarDeclStmt*>(node));
            break;
            
        case ASTNode::NodeType::BINARY_OP: {
            auto binary = static_cast<BinaryExpr*>(node);
            walkNode(binary->left.get());
            walkNode(binary->right.get());
            break;
        }
            
        case ASTNode::NodeType::CALL: {
            auto call = static_cast<CallExpr*>(node);
            walkNode(call->callee.get());
            for (const auto& arg : call->arguments) {
                walkNode(arg.get());
            }
            break;
        }
            
        case ASTNode::NodeType::BLOCK: {
            auto block = static_cast<BlockStmt*>(node);
            for (const auto& stmt : block->statements) {
                walkNode(stmt.get());
            }
            break;
        }
            
        case ASTNode::NodeType::IF: {
            auto ifStmt = static_cast<IfStmt*>(node);
            walkNode(ifStmt->condition.get());
            walkNode(ifStmt->thenBranch.get());
            if (ifStmt->elseBranch) {
                walkNode(ifStmt->elseBranch.get());
            }
            break;
        }
            
        case ASTNode::NodeType::WHILE: {
            auto whileStmt = static_cast<WhileStmt*>(node);
            walkNode(whileStmt->condition.get());
            walkNode(whileStmt->body.get());
            break;
        }
            
        case ASTNode::NodeType::RETURN: {
            auto retStmt = static_cast<ReturnStmt*>(node);
            if (retStmt->value) {
                walkNode(retStmt->value.get());
            }
            break;
        }
            
        case ASTNode::NodeType::EXPRESSION_STMT: {
            auto exprStmt = static_cast<ExpressionStmt*>(node);
            walkNode(exprStmt->expression.get());
            break;
        }
            
        // Add more cases as needed for other node types
        default:
            // For now, ignore other node types
            break;
    }
}

void ClosureAnalyzer::handleIdentifier(IdentifierExpr* expr) {
    if (!expr) return;
    
    const std::string& name = expr->name;
    
    // Skip if it's a parameter
    if (parameterNames.count(name) > 0) {
        return;
    }
    
    // Skip if it's a local variable declared in the lambda
    if (localVariables.count(name) > 0) {
        return;
    }
    
    // Check if it's from an outer scope
    if (isFromOuterScope(name)) {
        // This is a capture!
        if (captures.count(name) == 0) {
            captures[name] = CaptureInfo{name, false, false, 0};
        }
        captures[name].usageCount++;
    }
}

void ClosureAnalyzer::handleAssignment(AssignmentExpr* expr) {
    if (!expr) return;
    
    // Mark target as mutated if it's a capture
    if (auto target = std::dynamic_pointer_cast<IdentifierExpr>(expr->target)) {
        const std::string& name = target->name;
        if (captures.count(name) > 0) {
            captures[name].isMutated = true;
        }
    }
    
    // Walk the value expression
    walkNode(expr->value.get());
}

void ClosureAnalyzer::handleAddressOf(UnaryExpr* expr) {
    if (!expr || !expr->operand) return;
    
    // Mark operand as address-taken if it's a capture
    if (auto ident = std::dynamic_pointer_cast<IdentifierExpr>(expr->operand)) {
        const std::string& name = ident->name;
        if (captures.count(name) > 0) {
            captures[name].isAddressTaken = true;
        }
    }
}

void ClosureAnalyzer::handleVarDecl(VarDeclStmt* stmt) {
    if (!stmt) return;
    
    // Add to local variables (these are NOT captures)
    localVariables.insert(stmt->varName);
    
    // Walk initializer if present
    if (stmt->initializer) {
        walkNode(stmt->initializer.get());
    }
}

bool ClosureAnalyzer::isFromOuterScope(const std::string& name) {
    // Check if variable exists in symbol table
    // For now, simplified: assume all non-parameter, non-local identifiers are captures
    // Proper implementation would check symbol table scope depth
    return symbolTable && symbolTable->lookup(name) != nullptr;
}

LambdaExpr::CaptureMode ClosureAnalyzer::determineCaptureMode(const CaptureInfo& info) {
    // Rule 1: If mutated or address taken → BY_REFERENCE
    if (info.isMutated || info.isAddressTaken) {
        return LambdaExpr::CaptureMode::BY_REFERENCE;
    }
    
    // Rule 2: Check if should capture by value (primitives, small types)
    if (shouldCaptureByValue(info.name)) {
        return LambdaExpr::CaptureMode::BY_VALUE;
    }
    
    // Default: BY_VALUE for immutable captures
    return LambdaExpr::CaptureMode::BY_VALUE;
}

bool ClosureAnalyzer::shouldCaptureByValue(const std::string& varName) {
    // Look up variable type in symbol table
    if (!symbolTable) return true;
    
    auto symbol = symbolTable->lookup(varName);
    if (!symbol) return true;
    
    // For now, simplified: capture primitives by value
    // Proper implementation would check:
    // - Primitive types (int8, int64, flt32, etc.) → BY_VALUE
    // - Large structs, arrays → BY_REFERENCE
    // - Wild pointers → BY_MOVE (ownership transfer)
    
    const std::string& typeName = symbol->type->name;
    
    // Primitives
    if (typeName.find("int") != std::string::npos ||
        typeName.find("flt") != std::string::npos ||
        typeName.find("tbb") != std::string::npos ||
        typeName == "bool") {
        return true;
    }
    
    return false;
}

bool ClosureAnalyzer::validateLifetimes() {
    // Appendage Theory validation:
    // - Closure lifetime must not exceed captured variable lifetimes
    // - Stack closures cannot be returned
    // - Captured stack variables → closure must stay on stack
    
    // For now, simplified validation
    // Proper implementation would:
    // 1. Check if lambda is returned from function
    // 2. Check if any captures are stack variables
    // 3. Error if stack closure tries to escape
    
    // This will be properly implemented when we integrate with borrow checker
    return true;
}

} // namespace sema
} // namespace aria
