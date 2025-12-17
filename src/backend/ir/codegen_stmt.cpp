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
// Phase 4.3.2: Function Declaration Code Generation
// ============================================================================

/**
 * Generate code for function declaration
 * 
 * Creates an LLVM function with proper signature (return type and parameters),
 * sets up entry block, generates code for function body, and verifies the result.
 * 
 * Example Aria code:
 *   func:add = i32(i32:a, i32:b) {
 *       pass(a + b);
 *   };
 * 
 * Generated LLVM IR:
 *   define i32 @add(i32 %a, i32 %b) {
 *   entry:
 *     %a.addr = alloca i32
 *     %b.addr = alloca i32
 *     store i32 %a, i32* %a.addr
 *     store i32 %b, i32* %b.addr
 *     %0 = load i32, i32* %a.addr
 *     %1 = load i32, i32* %b.addr
 *     %2 = add i32 %0, %1
 *     ret i32 %2
 *   }
 */
llvm::Function* StmtCodegen::codegenFuncDecl(FuncDeclStmt* stmt) {
    // Get return type
    llvm::Type* return_type = getLLVMTypeFromString(stmt->returnType);
    
    // Build parameter types
    std::vector<llvm::Type*> param_types;
    for (const auto& param : stmt->parameters) {
        ParameterNode* param_node = static_cast<ParameterNode*>(param.get());
        llvm::Type* param_type = getLLVMTypeFromString(param_node->typeName);
        param_types.push_back(param_type);
    }
    
    // Create function type
    llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
    
    // Create function
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        stmt->funcName,
        module
    );
    
    // Set parameter names and create allocas for them
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        ParameterNode* param_node = static_cast<ParameterNode*>(stmt->parameters[idx].get());
        arg.setName(param_node->paramName);
        idx++;
    }
    
    // Create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Save the old named_values (for nested functions/closures in the future)
    std::map<std::string, llvm::Value*> old_named_values = named_values;
    named_values.clear();
    
    // Create allocas for parameters and store their values
    // This allows parameters to be mutable (can be reassigned in function body)
    idx = 0;
    for (auto& arg : func->args()) {
        ParameterNode* param_node = static_cast<ParameterNode*>(stmt->parameters[idx].get());
        
        // Create alloca for this parameter
        llvm::AllocaInst* alloca = builder.CreateAlloca(arg.getType(), nullptr, param_node->paramName);
        
        // Store the parameter value
        builder.CreateStore(&arg, alloca);
        
        // Remember this allocation
        named_values[param_node->paramName] = alloca;
        
        idx++;
    }
    
    // Generate code for function body
    if (stmt->body) {
        BlockStmt* body_block = static_cast<BlockStmt*>(stmt->body.get());
        codegenBlock(body_block);
    }
    
    // If the last instruction isn't a terminator, add a default return
    llvm::BasicBlock* current_block = builder.GetInsertBlock();
    if (current_block && !current_block->getTerminator()) {
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            // Return zero/null for non-void functions without explicit return
            if (return_type->isIntegerTy()) {
                builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
            } else if (return_type->isFloatingPointTy()) {
                builder.CreateRet(llvm::ConstantFP::get(return_type, 0.0));
            } else {
                // For pointer or other types, return null
                builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(return_type)));
            }
        }
    }
    
    // Restore old named_values
    named_values = old_named_values;
    
    // Verify the function
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    if (llvm::verifyFunction(*func, &error_stream)) {
        error_stream.flush();
        throw std::runtime_error("Function verification failed: " + error_msg);
    }
    
    return func;
}

// ============================================================================
// Phase 4.3.3: If Statement Code Generation
// ============================================================================

/**
 * Generate code for if/else statement
 * 
 * Creates conditional branching using LLVM basic blocks:
 * - Evaluates condition expression
 * - Creates then_block for true branch
 * - Creates else_block for false branch (optional)
 * - Creates merge_block to continue execution after if/else
 * 
 * Example Aria code (simple if):
 *   if (x > 10) {
 *       y = x + 1;
 *   }
 * 
 * Generated LLVM IR:
 *   %cond = icmp sgt i32 %x, 10
 *   br i1 %cond, label %if.then, label %if.merge
 * if.then:
 *   %add = add i32 %x, 1
 *   store i32 %add, i32* %y
 *   br label %if.merge
 * if.merge:
 *   ...
 * 
 * Example Aria code (if/else):
 *   if (x > 10) {
 *       y = x + 1;
 *   } else {
 *       y = x - 1;
 *   }
 * 
 * Generated LLVM IR:
 *   %cond = icmp sgt i32 %x, 10
 *   br i1 %cond, label %if.then, label %if.else
 * if.then:
 *   %add = add i32 %x, 1
 *   store i32 %add, i32* %y
 *   br label %if.merge
 * if.else:
 *   %sub = sub i32 %x, 1
 *   store i32 %sub, i32* %y
 *   br label %if.merge
 * if.merge:
 *   ...
 * 
 * Note: TBB sentinel checking will be added in Phase 4.5+
 */
void StmtCodegen::codegenIf(IfStmt* stmt) {
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Get current function for basic block creation
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Generate code for condition expression
    llvm::Value* cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
    
    if (!cond_value) {
        throw std::runtime_error("Failed to generate code for if condition");
    }
    
    // Convert condition to boolean (i1) if needed
    // LLVM expects i1 for branch instructions
    if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
        // Compare against zero for integer types
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder.CreateICmpNE(
                cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0),
                "tobool"
            );
        } else if (cond_value->getType()->isFloatingPointTy()) {
            // Compare against 0.0 for floating point types
            cond_value = builder.CreateFCmpONE(
                cond_value,
                llvm::ConstantFP::get(cond_value->getType(), 0.0),
                "tobool"
            );
        } else {
            throw std::runtime_error("Cannot convert condition value to boolean");
        }
    }
    
    // Create basic blocks
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context, "if.then", func);
    llvm::BasicBlock* else_block = nullptr;
    llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(context, "if.merge");
    
    // If there's an else branch, create else block
    if (stmt->elseBranch) {
        else_block = llvm::BasicBlock::Create(context, "if.else");
        builder.CreateCondBr(cond_value, then_block, else_block);
    } else {
        // No else branch, jump to merge if condition is false
        builder.CreateCondBr(cond_value, then_block, merge_block);
    }
    
    // Generate code for then branch
    builder.SetInsertPoint(then_block);
    
    if (stmt->thenBranch->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->thenBranch.get()));
    } else {
        // Single statement (not a block)
        codegenStatement(stmt->thenBranch.get());
    }
    
    // Add branch to merge block if then branch doesn't have terminator
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(merge_block);
    }
    
    // Generate code for else branch if present
    if (else_block) {
        else_block->insertInto(func);
        builder.SetInsertPoint(else_block);
        
        if (stmt->elseBranch->type == ASTNode::NodeType::BLOCK) {
            codegenBlock(static_cast<BlockStmt*>(stmt->elseBranch.get()));
        } else {
            // Single statement or another if statement (else if)
            codegenStatement(stmt->elseBranch.get());
        }
        
        // Add branch to merge block if else branch doesn't have terminator
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(merge_block);
        }
    }
    
    // Add merge block to function and set it as insertion point
    merge_block->insertInto(func);
    builder.SetInsertPoint(merge_block);
}

// ============================================================================
// ============================================================================
// Phase 4.3.4: Loop Statement Code Generation
// ============================================================================

/**
 * Generate code for while loop
 * 
 * Creates a loop with condition check and body. Uses three basic blocks:
 * - while.cond: Checks loop condition
 * - while.body: Executes loop body
 * - while.end: Continuation after loop
 * 
 * Example Aria code:
 *   i32:i = 0;
 *   while (i < 10) {
 *       i = i + 1;
 *   }
 * 
 * Generated LLVM IR:
 *   br label %while.cond
 * 
 * while.cond:
 *   %i.val = load i32, i32* %i
 *   %cmp = icmp slt i32 %i.val, 10
 *   br i1 %cmp, label %while.body, label %while.end
 * 
 * while.body:
 *   ; body statements
 *   br label %while.cond
 * 
 * while.end:
 *   ; continue execution
 */
void StmtCodegen::codegenWhile(WhileStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "while.cond", func);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "while.body");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "while.end");
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Generate condition check
    builder.SetInsertPoint(cond_block);
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    llvm::Value* cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
    
    if (!cond_value) {
        throw std::runtime_error("Failed to generate code for while condition");
    }
    
    // Convert non-boolean conditions to boolean
    if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
        if (cond_value->getType()->isIntegerTy()) {
            // Integer: non-zero is true
            cond_value = builder.CreateICmpNE(cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0), "tobool");
        } else if (cond_value->getType()->isFloatingPointTy()) {
            // Float: non-zero is true
            cond_value = builder.CreateFCmpONE(cond_value,
                llvm::ConstantFP::get(cond_value->getType(), 0.0), "tobool");
        }
    }
    
    // Conditional branch: if true goto body, else goto end
    builder.CreateCondBr(cond_value, body_block, end_block);
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    // Generate body statements
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Loop back to condition check (if no terminator already present)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(cond_block);
    }
    
    // Set insertion point to end block for continuation
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for for loop
 * 
 * Creates a loop with initialization, condition, update, and body. Uses four phases:
 * 1. Initialization: Execute init statement (optional)
 * 2. Condition: Check loop condition
 * 3. Body: Execute loop body
 * 4. Update: Execute update expression and loop back to condition
 * 
 * Example Aria code:
 *   for (i32:i = 0; i < 10; i = i + 1) {
 *       print(i);
 *   }
 * 
 * Generated LLVM IR:
 *   ; init
 *   %i = alloca i32
 *   store i32 0, i32* %i
 *   br label %for.cond
 * 
 * for.cond:
 *   %i.val = load i32, i32* %i
 *   %cmp = icmp slt i32 %i.val, 10
 *   br i1 %cmp, label %for.body, label %for.end
 * 
 * for.body:
 *   ; body statements
 *   br label %for.inc
 * 
 * for.inc:
 *   %next = add i32 %i.val, 1
 *   store i32 %next, i32* %i
 *   br label %for.cond
 * 
 * for.end:
 *   ; continue execution
 */
void StmtCodegen::codegenFor(ForStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "for.cond");
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "for.body");
    llvm::BasicBlock* inc_block = llvm::BasicBlock::Create(context, "for.inc");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "for.end");
    
    // Generate initialization statement (if present)
    if (stmt->initializer) {
        codegenStatement(stmt->initializer.get());
    }
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Generate condition check
    cond_block->insertInto(func);
    builder.SetInsertPoint(cond_block);
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // If no condition, treat as infinite loop (condition = true)
    llvm::Value* cond_value = nullptr;
    if (stmt->condition) {
        cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
        
        if (!cond_value) {
            throw std::runtime_error("Failed to generate code for for loop condition");
        }
        
        // Convert non-boolean conditions to boolean
        if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
            if (cond_value->getType()->isIntegerTy()) {
                cond_value = builder.CreateICmpNE(cond_value,
                    llvm::ConstantInt::get(cond_value->getType(), 0), "tobool");
            } else if (cond_value->getType()->isFloatingPointTy()) {
                cond_value = builder.CreateFCmpONE(cond_value,
                    llvm::ConstantFP::get(cond_value->getType(), 0.0), "tobool");
            }
        }
        
        builder.CreateCondBr(cond_value, body_block, end_block);
    } else {
        // No condition = infinite loop (always branch to body)
        builder.CreateBr(body_block);
    }
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Branch to increment block (if no terminator already present)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(inc_block);
    }
    
    // Generate increment/update
    inc_block->insertInto(func);
    builder.SetInsertPoint(inc_block);
    
    if (stmt->update) {
        // Generate update expression (result is discarded)
        expr_codegen->codegenExpressionNode(stmt->update.get(), expr_codegen);
    }
    
    // Loop back to condition
    builder.CreateBr(cond_block);
    
    // Set insertion point to end block for continuation
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
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
        
        case ASTNode::NodeType::FOR:
            codegenFor(static_cast<ForStmt*>(stmt));
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
