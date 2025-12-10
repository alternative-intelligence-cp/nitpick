/**
 * src/backend/monomorphization.cpp
 *
 * Monomorphization Engine Implementation
 */

#include "monomorphization.h"
#include "../frontend/ast/expr.h"
#include "../frontend/ast/control_flow.h"
#include "../frontend/ast/loops.h"
#include "../frontend/ast/defer.h"
#include <sstream>
#include <stdexcept>

namespace aria {
namespace backend {

// Generate specialized function name with type mangling
std::string Monomorphizer::generateSpecializedName(
    const std::string& trait_name,
    const std::string& type_name,
    const std::string& method_name
) {
    std::stringstream ss;
    ss << trait_name << "_" << type_name << "_" << method_name;
    return ss.str();
}

// Clone expression (deep copy for AST specialization)
std::unique_ptr<frontend::Expression> Monomorphizer::cloneExpr(frontend::Expression* expr) {
    if (!expr) return nullptr;
    
    // Use dynamic_cast to determine expression type and clone appropriately
    
    // Literals
    if (auto* intLit = dynamic_cast<frontend::IntLiteral*>(expr)) {
        return std::make_unique<frontend::IntLiteral>(intLit->value);
    }
    
    if (auto* floatLit = dynamic_cast<frontend::FloatLiteral*>(expr)) {
        return std::make_unique<frontend::FloatLiteral>(floatLit->value);
    }
    
    if (auto* boolLit = dynamic_cast<frontend::BoolLiteral*>(expr)) {
        return std::make_unique<frontend::BoolLiteral>(boolLit->value);
    }
    
    if (auto* strLit = dynamic_cast<frontend::StringLiteral*>(expr)) {
        return std::make_unique<frontend::StringLiteral>(strLit->value);
    }
    
    if (auto* nullLit = dynamic_cast<frontend::NullLiteral*>(expr)) {
        return std::make_unique<frontend::NullLiteral>();
    }
    
    // Variables
    if (auto* varExpr = dynamic_cast<frontend::VarExpr*>(expr)) {
        return std::make_unique<frontend::VarExpr>(varExpr->name);
    }
    
    // Binary operations
    if (auto* binOp = dynamic_cast<frontend::BinaryOp*>(expr)) {
        return std::make_unique<frontend::BinaryOp>(
            binOp->op,
            cloneExpr(binOp->left.get()),
            cloneExpr(binOp->right.get())
        );
    }
    
    // Unary operations
    if (auto* unOp = dynamic_cast<frontend::UnaryOp*>(expr)) {
        return std::make_unique<frontend::UnaryOp>(
            unOp->op,
            cloneExpr(unOp->operand.get())
        );
    }
    
    // Function calls
    if (auto* callExpr = dynamic_cast<frontend::CallExpr*>(expr)) {
        std::unique_ptr<frontend::CallExpr> cloned;
        if (!callExpr->function_name.empty()) {
            cloned = std::make_unique<frontend::CallExpr>(callExpr->function_name);
        } else if (callExpr->callee) {
            cloned = std::make_unique<frontend::CallExpr>(cloneExpr(callExpr->callee.get()));
        } else {
            cloned = std::make_unique<frontend::CallExpr>("");
        }
        for (const auto& arg : callExpr->arguments) {
            cloned->arguments.push_back(cloneExpr(arg.get()));
        }
        cloned->type_arguments = callExpr->type_arguments;
        return cloned;
    }
    
    // For other expression types, return nullptr
    // This is safe because monomorphization only needs to clone function bodies
    // which typically contain basic expressions
    return nullptr;
}

// Clone statement (deep copy)
std::unique_ptr<frontend::Statement> Monomorphizer::cloneStmt(frontend::Statement* stmt) {
    if (!stmt) return nullptr;
    
    // Return statement
    if (auto* retStmt = dynamic_cast<frontend::ReturnStmt*>(stmt)) {
        return std::make_unique<frontend::ReturnStmt>(cloneExpr(retStmt->value.get()));
    }
    
    // Variable declaration
    if (auto* varDecl = dynamic_cast<frontend::VarDecl*>(stmt)) {
        auto cloned = std::make_unique<frontend::VarDecl>(
            varDecl->type,
            varDecl->name,
            cloneExpr(varDecl->initializer.get())
        );
        cloned->is_const = varDecl->is_const;
        cloned->is_stack = varDecl->is_stack;
        cloned->is_wild = varDecl->is_wild;
        cloned->is_wildx = varDecl->is_wildx;
        cloned->generic_params = varDecl->generic_params;
        return cloned;
    }
    
    // Expression statement
    if (auto* exprStmt = dynamic_cast<frontend::ExpressionStmt*>(stmt)) {
        return std::make_unique<frontend::ExpressionStmt>(cloneExpr(exprStmt->expression.get()));
    }
    
    // If statement
    if (auto* ifStmt = dynamic_cast<frontend::IfStmt*>(stmt)) {
        return std::make_unique<frontend::IfStmt>(
            cloneExpr(ifStmt->condition.get()),
            cloneBlock(ifStmt->then_block.get()),
            cloneBlock(ifStmt->else_block.get())
        );
    }
    
    // Block
    if (auto* block = dynamic_cast<frontend::Block*>(stmt)) {
        // Block cloning returns unique_ptr<Block>, need to cast through AstNode
        return std::unique_ptr<frontend::Statement>(dynamic_cast<frontend::Statement*>(cloneBlock(block).release()));
    }
    
    // For other statement types, return nullptr
    return nullptr;
}

// Clone block (deep copy)
std::unique_ptr<frontend::Block> Monomorphizer::cloneBlock(frontend::Block* block) {
    if (!block) return nullptr;
    
    auto new_block = std::make_unique<frontend::Block>();
    
    for (const auto& stmt : block->statements) {
        // Block->statements are AstNode*, try to cast to Statement*
        if (auto* statement = dynamic_cast<frontend::Statement*>(stmt.get())) {
            new_block->statements.push_back(cloneStmt(statement));
        }
    }
    
    return new_block;
}

// Clone function declaration
std::unique_ptr<frontend::FuncDecl> Monomorphizer::cloneFuncDecl(frontend::FuncDecl* original) {
    if (!original) return nullptr;
    
    // Deep clone parameters (FuncParam has unique_ptr, not copyable)
    std::vector<frontend::FuncParam> cloned_params;
    for (const auto& param : original->parameters) {
        cloned_params.emplace_back(
            param.type,
            param.name,
            param.default_value ? cloneExpr(param.default_value.get()) : nullptr
        );
    }
    
    // FuncDecl constructor requires name, generics, parameters, return_type, body
    auto cloned = std::make_unique<frontend::FuncDecl>(
        original->name,
        original->generics,
        std::move(cloned_params),
        original->return_type,
        cloneBlock(original->body.get())
    );
    
    // Copy additional properties
    cloned->is_pub = original->is_pub;
    cloned->is_async = original->is_async;
    cloned->auto_wrap = original->auto_wrap;
    
    // Clone body
    cloned->body = cloneBlock(original->body.get());
    
    return cloned;
}

// Get or create specialized function
std::string Monomorphizer::getOrCreateSpecialization(
    const std::string& trait_name,
    const std::string& type_name,
    const std::string& method_name
) {
    // Check if specialization already exists
    auto key = std::make_tuple(trait_name, type_name, method_name);
    auto it = context.specialization_map.find(key);
    
    if (it != context.specialization_map.end()) {
        return it->second;  // Return existing specialization name
    }
    
    // Find the impl for this trait and type
    auto range = context.impl_table.equal_range(trait_name);
    frontend::ImplDecl* target_impl = nullptr;
    
    for (auto impl_it = range.first; impl_it != range.second; ++impl_it) {
        if (impl_it->second->type_name == type_name) {
            target_impl = impl_it->second;
            break;
        }
    }
    
    if (!target_impl) {
        std::stringstream ss;
        ss << "No implementation of trait '" << trait_name 
           << "' found for type '" << type_name << "'";
        throw std::runtime_error(ss.str());
    }
    
    // Find the method in the impl
    frontend::FuncDecl* method = nullptr;
    for (const auto& m : target_impl->methods) {
        if (m->name == method_name) {
            method = m.get();
            break;
        }
    }
    
    if (!method) {
        std::stringstream ss;
        ss << "Method '" << method_name << "' not found in impl of trait '" 
           << trait_name << "' for type '" << type_name << "'";
        throw std::runtime_error(ss.str());
    }
    
    // Generate specialized name
    std::string specialized_name = generateSpecializedName(trait_name, type_name, method_name);
    
    // Clone the method
    auto specialized_func = cloneFuncDecl(method);
    specialized_func->name = specialized_name;
    
    // Register specialization
    context.specialization_map[key] = specialized_name;
    context.specialized_functions.push_back(std::move(specialized_func));
    
    return specialized_name;
}

// Monomorphize all implementations
std::vector<frontend::FuncDecl*> Monomorphizer::monomorphizeAll() {
    std::vector<frontend::FuncDecl*> result;
    
    // For each implementation, create specializations for all methods
    for (const auto& [trait_name, impl] : context.impl_table) {
        for (const auto& method : impl->methods) {
            std::string specialized_name = getOrCreateSpecialization(
                trait_name,
                impl->type_name,
                method->name
            );
            
            // Find the specialized function we just created
            for (const auto& func : context.specialized_functions) {
                if (func->name == specialized_name) {
                    result.push_back(func.get());
                    break;
                }
            }
        }
    }
    
    return result;
}

} // namespace backend
} // namespace aria
