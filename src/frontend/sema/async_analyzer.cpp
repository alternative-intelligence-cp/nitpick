#include "frontend/sema/async_analyzer.h"
#include <sstream>

namespace aria {
namespace sema {

AsyncSemanticAnalyzer::AsyncSemanticAnalyzer()
    : inAsyncContext(false), currentFunctionName("") {}

AsyncSemanticAnalyzer::~AsyncSemanticAnalyzer() {}

void AsyncSemanticAnalyzer::error(const std::string& message) {
    errors.push_back(message);
}

void AsyncSemanticAnalyzer::analyze(ASTNodePtr root) {
    if (!root) return;
    
    // Traverse the AST based on node type
    if (root->type == ASTNode::NodeType::PROGRAM) {
        auto program = std::static_pointer_cast<ProgramNode>(root);
        for (auto& decl : program->declarations) {
            if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                auto funcDecl = std::static_pointer_cast<FuncDeclStmt>(decl);
                analyzeFuncDecl(funcDecl.get());
            }
        }
    }
}

void AsyncSemanticAnalyzer::analyzeFuncDecl(FuncDeclStmt* funcDecl) {
    if (!funcDecl) return;
    
    // Save previous async context
    bool prevAsyncContext = inAsyncContext;
    std::string prevFunctionName = currentFunctionName;
    
    // Set new context
    inAsyncContext = funcDecl->isAsync;
    currentFunctionName = funcDecl->funcName;
    
    // Analyze function body
    if (funcDecl->body) {
        analyzeStatement(funcDecl->body);
    }
    
    // Restore previous context
    inAsyncContext = prevAsyncContext;
    currentFunctionName = prevFunctionName;
}

void AsyncSemanticAnalyzer::analyzeStatement(ASTNodePtr stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case ASTNode::NodeType::BLOCK: {
            auto block = std::static_pointer_cast<BlockStmt>(stmt);
            for (auto& s : block->statements) {
                analyzeStatement(s);
            }
            break;
        }
        
        case ASTNode::NodeType::VAR_DECL: {
            auto varDecl = std::static_pointer_cast<VarDeclStmt>(stmt);
            if (varDecl->initializer) {
                analyzeExpression(varDecl->initializer);
            }
            break;
        }
        
        case ASTNode::NodeType::IF: {
            auto ifStmt = std::static_pointer_cast<IfStmt>(stmt);
            analyzeExpression(ifStmt->condition);
            analyzeStatement(ifStmt->thenBranch);
            if (ifStmt->elseBranch) {
                analyzeStatement(ifStmt->elseBranch);
            }
            break;
        }
        
        case ASTNode::NodeType::WHILE: {
            auto whileStmt = std::static_pointer_cast<WhileStmt>(stmt);
            analyzeExpression(whileStmt->condition);
            analyzeStatement(whileStmt->body);
            break;
        }
        
        case ASTNode::NodeType::FOR: {
            auto forStmt = std::static_pointer_cast<ForStmt>(stmt);
            if (forStmt->initializer) analyzeStatement(forStmt->initializer);
            if (forStmt->condition) analyzeExpression(forStmt->condition);
            if (forStmt->update) analyzeExpression(forStmt->update);
            analyzeStatement(forStmt->body);
            break;
        }
        
        case ASTNode::NodeType::RETURN: {
            auto returnStmt = std::dynamic_pointer_cast<ReturnStmt>(stmt);
            if (returnStmt->value) {
                analyzeExpression(returnStmt->value);
            }
            break;
        }
        
        case ASTNode::NodeType::EXPRESSION_STMT: {
            auto exprStmt = std::static_pointer_cast<ExpressionStmt>(stmt);
            analyzeExpression(exprStmt->expression);
            break;
        }
        
        default:
            // Other statement types don't contain expressions we need to analyze
            break;
    }
}

void AsyncSemanticAnalyzer::analyzeExpression(ASTNodePtr expr) {
    if (!expr) return;
    
    switch (expr->type) {
        case ASTNode::NodeType::AWAIT: {
            auto awaitExpr = std::static_pointer_cast<AwaitExpr>(expr);
            analyzeAwaitExpr(awaitExpr.get());
            break;
        }
        
        case ASTNode::NodeType::BINARY_OP: {
            auto binaryExpr = std::static_pointer_cast<BinaryExpr>(expr);
            analyzeExpression(binaryExpr->left);
            analyzeExpression(binaryExpr->right);
            break;
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            auto unaryExpr = std::static_pointer_cast<UnaryExpr>(expr);
            analyzeExpression(unaryExpr->operand);
            break;
        }
        
        case ASTNode::NodeType::CALL: {
            auto callExpr = std::static_pointer_cast<CallExpr>(expr);
            analyzeExpression(callExpr->callee);
            for (auto& arg : callExpr->arguments) {
                analyzeExpression(arg);
            }
            break;
        }
        
        case ASTNode::NodeType::MEMBER_ACCESS: {
            auto memberExpr = std::static_pointer_cast<MemberAccessExpr>(expr);
            analyzeExpression(memberExpr->object);
            break;
        }
        
        case ASTNode::NodeType::TERNARY: {
            auto ternaryExpr = std::static_pointer_cast<TernaryExpr>(expr);
            analyzeExpression(ternaryExpr->condition);
            analyzeExpression(ternaryExpr->trueValue);
            analyzeExpression(ternaryExpr->falseValue);
            break;
        }
        
        case ASTNode::NodeType::LAMBDA: {
            // Lambda functions can potentially be async themselves
            // For now, we don't support async lambdas, so just analyze the body
            auto lambda = std::static_pointer_cast<LambdaExpr>(expr);
            if (lambda->body) {
                analyzeStatement(lambda->body);
            }
            break;
        }
        
        default:
            // Literals, identifiers, etc. don't need async analysis
            break;
    }
}

void AsyncSemanticAnalyzer::analyzeAwaitExpr(AwaitExpr* awaitExpr) {
    if (!awaitExpr) return;
    
    // Check: await must be in async context (research_029 Section 3.2)
    if (!inAsyncContext) {
        std::ostringstream oss;
        oss << "E_ASYNC_OUTSIDE_CONTEXT: 'await' can only be used inside async functions or async blocks";
        if (!currentFunctionName.empty()) {
            oss << " (found in non-async function '" << currentFunctionName << "')";
        }
        oss << " at line " << awaitExpr->line;
        error(oss.str());
    }
    
    // Analyze the operand expression (recursively check for nested awaits)
    if (awaitExpr->operand) {
        analyzeExpression(awaitExpr->operand);
    }
    
    // Future trait checking will be done in type checking phase
    // For now, we just validate the context
}

} // namespace sema
} // namespace aria
