#include "backend/ir/codegen_stmt.h"
#include "backend/ir/codegen_expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <stdexcept>

using namespace aria;
using namespace aria::backend;
using namespace aria::sema;

StmtCodegen::StmtCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values), expr_codegen(nullptr) {}

void StmtCodegen::setExprCodegen(ExprCodegen* expr_gen) {
    expr_codegen = expr_gen;
}

// Helper: Get LLVM type from Aria type string
llvm::Type* StmtCodegen::getLLVMTypeFromString(const std::string& type_name) {
    // Primitive types
    if (type_name == "i8") return llvm::Type::getInt8Ty(context);
    if (type_name == "i16") return llvm::Type::getInt16Ty(context);
    if (type_name == "i32") return llvm::Type::getInt32Ty(context);
    if (type_name == "i64") return llvm::Type::getInt64Ty(context);
    if (type_name == "u8") return llvm::Type::getInt8Ty(context);
    if (type_name == "u16") return llvm::Type::getInt16Ty(context);
    if (type_name == "u32") return llvm::Type::getInt32Ty(context);
    if (type_name == "u64") return llvm::Type::getInt64Ty(context);
    if (type_name == "f32") return llvm::Type::getFloatTy(context);
    if (type_name == "f64") return llvm::Type::getDoubleTy(context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(context);
    if (type_name == "void") return llvm::Type::getVoidTy(context);
    
    // Default to i32 for unknown types (will be handled by semantic analysis)
    return llvm::Type::getInt32Ty(context);
}

// Helper: Get LLVM type from Aria type
llvm::Type* StmtCodegen::getLLVMType(Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(context);
    }
    
    if (!type->isPrimitive()) {
        return llvm::Type::getInt32Ty(context);  // Default for non-primitive
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    return getLLVMTypeFromString(prim_type->getName());
}

// ============================================================================
// Phase 4.3.1: Variable Declaration Code Generation
// ============================================================================

/**
 * Generate code for variable declaration
 * 
 * Creates an alloca instruction to allocate space on the stack for the variable,
 * then optionally stores the initial value if an initializer is provided.
 * 
 * Example Aria code:
 *   i32:x = 42;
 * 
 * Generated LLVM IR:
 *   %x = alloca i32
 *   store i32 42, i32* %x
 * 
 * Allocation modes (future):
 * - stack: Explicit stack allocation (alloca)
 * - wild: Manual memory management (malloc/free)
 * - gc: Garbage collected (default, runtime support needed)
 */
void StmtCodegen::codegenVarDecl(VarDeclStmt* stmt) {
    // Get LLVM type from type string
    llvm::Type* var_type = getLLVMTypeFromString(stmt->typeName);
    
    // Get the current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Create alloca instruction for the variable
    // Allocas are created at the entry block of the function for optimization
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* alloca = tmp_builder.CreateAlloca(var_type, nullptr, stmt->varName);
    
    // Store the alloca in named_values so we can reference it later
    named_values[stmt->varName] = alloca;
    
    // If there's an initializer, generate code for it and store the result
    if (stmt->initializer) {
        if (!expr_codegen) {
            throw std::runtime_error("ExprCodegen not set in StmtCodegen");
        }
        
        // Generate code for initializer expression
        llvm::Value* init_value = expr_codegen->codegenExpressionNode(stmt->initializer.get(), expr_codegen);
        
        if (!init_value) {
            throw std::runtime_error("Failed to generate code for initializer expression");
        }
        
        // Store the initial value in the allocated variable
        builder.CreateStore(init_value, alloca);
    }
    
    // TODO: Handle allocation modes (stack, wild, gc)
    // For now, everything is allocated on the stack using alloca
    // - stmt->isStack: Already handled (uses alloca)
    // - stmt->isWild: Would use malloc/free (Phase 4.4+)
    // - stmt->isGC: Would use GC runtime (Phase 4.6+)
}

// ============================================================================
// Phase 4.3.2: Function Declaration Code Generation (TODO)
// ============================================================================

llvm::Function* StmtCodegen::codegenFuncDecl(FuncDeclStmt* stmt) {
    // TODO: Implement function declaration code generation
    // Will be implemented in Phase 4.3.2
    throw std::runtime_error("Function declaration code generation not yet implemented");
}

// ============================================================================
// Phase 4.3.3: If Statement Code Generation (TODO)
// ============================================================================

void StmtCodegen::codegenIf(IfStmt* stmt) {
    // TODO: Implement if statement code generation
    // Will be implemented in Phase 4.3.3
    throw std::runtime_error("If statement code generation not yet implemented");
}

// ============================================================================
// Phase 4.3.4: Loop Code Generation (TODO)
// ============================================================================

void StmtCodegen::codegenWhile(WhileStmt* stmt) {
    // TODO: Implement while loop code generation
    // Will be implemented in Phase 4.3.4
    throw std::runtime_error("While loop code generation not yet implemented");
}

void StmtCodegen::codegenFor(ForStmt* stmt) {
    // TODO: Implement for loop code generation
    // Will be implemented in Phase 4.3.4
    throw std::runtime_error("For loop code generation not yet implemented");
}

// ============================================================================
// Block and Expression Statement Code Generation
// ============================================================================

void StmtCodegen::codegenBlock(BlockStmt* stmt) {
    // Generate code for each statement in the block
    for (const auto& statement : stmt->statements) {
        codegenStatement(statement.get());
    }
}

void StmtCodegen::codegenReturn(ReturnStmt* stmt) {
    if (stmt->value) {
        if (!expr_codegen) {
            throw std::runtime_error("ExprCodegen not set in StmtCodegen");
        }
        
        // Generate code for return value expression
        llvm::Value* ret_value = expr_codegen->codegenExpressionNode(stmt->value.get(), expr_codegen);
        
        if (!ret_value) {
            throw std::runtime_error("Failed to generate code for return value");
        }
        
        builder.CreateRet(ret_value);
    } else {
        // Return void
        builder.CreateRetVoid();
    }
}

void StmtCodegen::codegenExpressionStmt(ExpressionStmt* stmt) {
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Generate code for the expression (result is discarded)
    expr_codegen->codegenExpressionNode(stmt->expression.get(), expr_codegen);
}

// ============================================================================
// Statement Dispatcher
// ============================================================================

void StmtCodegen::codegenStatement(ASTNode* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            codegenVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FUNC_DECL:
            codegenFuncDecl(static_cast<FuncDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::IF:
            codegenIf(static_cast<IfStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHILE:
            codegenWhile(static_cast<WhileStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BLOCK:
            codegenBlock(static_cast<BlockStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::RETURN:
            codegenReturn(static_cast<ReturnStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXPRESSION_STMT:
            codegenExpressionStmt(static_cast<ExpressionStmt*>(stmt));
            break;
        
        default:
            throw std::runtime_error("Unsupported statement type in codegen: " + 
                                     std::to_string(static_cast<int>(stmt->type)));
    }
}
