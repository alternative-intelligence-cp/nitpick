#include "backend/ir/codegen_expr.h"
#include "backend/ir/codegen_stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include "frontend/token.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <stdexcept>

using namespace aria;
using namespace aria::frontend;
using namespace aria::backend;
using namespace aria::sema;

ExprCodegen::ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values), stmt_codegen(nullptr) {}

void ExprCodegen::setStmtCodegen(StmtCodegen* stmt_gen) {
    stmt_codegen = stmt_gen;
}

// Helper: Get LLVM type from Aria type
llvm::Type* ExprCodegen::getLLVMType(Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(context);
    }
    
    // Get type name - for now, we only handle primitive types
    if (!type->isPrimitive()) {
        // Non-primitive types will be handled later
        return llvm::Type::getInt32Ty(context);  // Default
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    std::string type_name = prim_type->getName();
    
    // Primitive types
    if (type_name == "i8" || type_name == "tbb8") return llvm::Type::getInt8Ty(context);
    if (type_name == "i16" || type_name == "tbb16") return llvm::Type::getInt16Ty(context);
    if (type_name == "i32" || type_name == "tbb32") return llvm::Type::getInt32Ty(context);
    if (type_name == "i64" || type_name == "tbb64") return llvm::Type::getInt64Ty(context);
    if (type_name == "u8") return llvm::Type::getInt8Ty(context);
    if (type_name == "u16") return llvm::Type::getInt16Ty(context);
    if (type_name == "u32") return llvm::Type::getInt32Ty(context);
    if (type_name == "u64") return llvm::Type::getInt64Ty(context);
    if (type_name == "f32") return llvm::Type::getFloatTy(context);
    if (type_name == "f64") return llvm::Type::getDoubleTy(context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(context);
    if (type_name == "void") return llvm::Type::getVoidTy(context);
    
    // Pointer types (str, any reference)
    if (type_name == "str" || type_name.find('*') != std::string::npos) {
        return llvm::PointerType::get(context, 0);  // LLVM 20.x opaque pointers
    }
    
    // Default to i32 for unknown types (for now)
    return llvm::Type::getInt32Ty(context);
}

// Helper: Get size of Aria type in bytes
size_t ExprCodegen::getTypeSize(Type* type) {
    if (!type) return 0;
    
    if (!type->isPrimitive()) {
        return 8;  // Default pointer size
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    std::string type_name = prim_type->getName();
    
    if (type_name == "i8" || type_name == "u8" || type_name == "tbb8") return 1;
    if (type_name == "i16" || type_name == "u16" || type_name == "tbb16") return 2;
    if (type_name == "i32" || type_name == "u32" || type_name == "f32" || type_name == "tbb32") return 4;
    if (type_name == "i64" || type_name == "u64" || type_name == "f64" || type_name == "tbb64") return 8;
    if (type_name == "bool") return 1;
    if (type_name == "str") return 8;  // Pointer size on 64-bit
    
    return 8;  // Default pointer size
}

/**
 * Generate code for literal expressions
 * Handles: integers, floats, strings, booleans, null
 */
llvm::Value* ExprCodegen::codegenLiteral(LiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null literal expression");
    }
    
    // Integer literal
    if (std::holds_alternative<int64_t>(expr->value)) {
        int64_t val = std::get<int64_t>(expr->value);
        
        // Determine appropriate integer type based on value range
        // Default to i32 for small values, i64 for larger
        if (val >= INT32_MIN && val <= INT32_MAX) {
            return llvm::ConstantInt::get(context, llvm::APInt(32, val, true));
        } else {
            return llvm::ConstantInt::get(context, llvm::APInt(64, val, true));
        }
    }
    
    // Float literal
    if (std::holds_alternative<double>(expr->value)) {
        double val = std::get<double>(expr->value);
        return llvm::ConstantFP::get(context, llvm::APFloat(val));
    }
    
    // String literal
    if (std::holds_alternative<std::string>(expr->value)) {
        const std::string& str = std::get<std::string>(expr->value);
        
        // Create a global constant for the string
        llvm::Constant* str_constant = llvm::ConstantDataArray::getString(context, str, true);
        
        // Create a global variable to hold the string
        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *module,
            str_constant->getType(),
            true,  // isConstant
            llvm::GlobalValue::PrivateLinkage,
            str_constant,
            ".str"
        );
        
        // Return pointer to the string (cast to i8*)
        return builder.CreatePointerCast(gv, llvm::PointerType::get(context, 0));
    }
    
    // Boolean literal
    if (std::holds_alternative<bool>(expr->value)) {
        bool val = std::get<bool>(expr->value);
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), val ? 1 : 0);
    }
    
    // Null literal
    if (std::holds_alternative<std::monostate>(expr->value)) {
        // Null is represented as a null pointer
        return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
    }
    
    throw std::runtime_error("Unknown literal type");
}

/**
 * Generate code for identifier (variable reference)
 * Loads the value from the symbol table
 */
llvm::Value* ExprCodegen::codegenIdentifier(IdentifierExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null identifier expression");
    }
    
    // Look up the variable in the symbol table
    auto it = named_values.find(expr->name);
    if (it == named_values.end()) {
        throw std::runtime_error("Undefined variable: " + expr->name);
    }
    
    llvm::Value* var_ptr = it->second;
    
    // Check if this is an alloca (stack variable) that needs loading
    // In LLVM 20+ with opaque pointers, we use the alloca's allocated type
    if (llvm::isa<llvm::AllocaInst>(var_ptr)) {
        llvm::AllocaInst* alloca = llvm::cast<llvm::AllocaInst>(var_ptr);
        llvm::Type* allocated_type = alloca->getAllocatedType();
        
        // Create a load instruction
        return builder.CreateLoad(allocated_type, var_ptr, expr->name);
    }
    
    // If it's not an alloca, return the value directly
    // (could be a function parameter or constant)
    return var_ptr;
}

/**
 * Helper: Recursively generate code for any expression node
 * This is a simplified dispatcher for testing - full integration in Phase 4.3+
 */
llvm::Value* ExprCodegen::codegenExpressionNode(ASTNode* node, ExprCodegen* codegen) {
    if (!node) {
        throw std::runtime_error("Null expression node");
    }
    
    // Dispatch based on node type
    switch (node->type) {
        case ASTNode::NodeType::LITERAL:
            return codegen->codegenLiteral(static_cast<LiteralExpr*>(node));
        case ASTNode::NodeType::IDENTIFIER:
            return codegen->codegenIdentifier(static_cast<IdentifierExpr*>(node));
        case ASTNode::NodeType::BINARY_OP:
            return codegen->codegenBinary(static_cast<BinaryExpr*>(node));
        case ASTNode::NodeType::UNARY_OP:
            return codegen->codegenUnary(static_cast<UnaryExpr*>(node));
        case ASTNode::NodeType::CALL:
            return codegen->codegenCall(static_cast<CallExpr*>(node));
        case ASTNode::NodeType::TERNARY:
            return codegen->codegenTernary(static_cast<TernaryExpr*>(node));
        case ASTNode::NodeType::LAMBDA:
            return codegen->codegenLambda(static_cast<LambdaExpr*>(node));
        default:
            throw std::runtime_error("Unsupported expression node type in operation");
    }
}

/**
 * Generate code for binary operations
 * Handles: arithmetic, comparison, logical, bitwise operators
 */
llvm::Value* ExprCodegen::codegenBinary(BinaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null binary expression");
    }
    
    // Generate code for left and right operands
    llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
    llvm::Value* right = codegenExpressionNode(expr->right.get(), this);
    
    if (!left || !right) {
        throw std::runtime_error("Failed to generate code for binary operation operands");
    }
    
    // Get the operator type
    TokenType op = expr->op.type;
    
    // Check if operands are floating point
    bool isFloat = left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy();
    
    // ARITHMETIC OPERATORS
    if (op == TokenType::TOKEN_PLUS) {
        if (isFloat) {
            return builder.CreateFAdd(left, right, "addtmp");
        } else {
            return builder.CreateAdd(left, right, "addtmp");
        }
    }
    
    if (op == TokenType::TOKEN_MINUS) {
        if (isFloat) {
            return builder.CreateFSub(left, right, "subtmp");
        } else {
            return builder.CreateSub(left, right, "subtmp");
        }
    }
    
    if (op == TokenType::TOKEN_STAR) {
        if (isFloat) {
            return builder.CreateFMul(left, right, "multmp");
        } else {
            return builder.CreateMul(left, right, "multmp");
        }
    }
    
    if (op == TokenType::TOKEN_SLASH) {
        if (isFloat) {
            return builder.CreateFDiv(left, right, "divtmp");
        } else {
            // For integers, use signed division
            return builder.CreateSDiv(left, right, "divtmp");
        }
    }
    
    if (op == TokenType::TOKEN_PERCENT) {
        if (isFloat) {
            return builder.CreateFRem(left, right, "modtmp");
        } else {
            return builder.CreateSRem(left, right, "modtmp");
        }
    }
    
    // COMPARISON OPERATORS
    if (op == TokenType::TOKEN_EQUAL_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOEQ(left, right, "eqtmp");
        } else {
            return builder.CreateICmpEQ(left, right, "eqtmp");
        }
    }
    
    if (op == TokenType::TOKEN_BANG_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpONE(left, right, "netmp");
        } else {
            return builder.CreateICmpNE(left, right, "netmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS) {
        if (isFloat) {
            return builder.CreateFCmpOLT(left, right, "lttmp");
        } else {
            return builder.CreateICmpSLT(left, right, "lttmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOLE(left, right, "letmp");
        } else {
            return builder.CreateICmpSLE(left, right, "letmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER) {
        if (isFloat) {
            return builder.CreateFCmpOGT(left, right, "gttmp");
        } else {
            return builder.CreateICmpSGT(left, right, "gttmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOGE(left, right, "getmp");
        } else {
            return builder.CreateICmpSGE(left, right, "getmp");
        }
    }
    
    // LOGICAL OPERATORS (short-circuit evaluation with phi nodes)
    if (op == TokenType::TOKEN_AND_AND) {
        // Convert to i1 if needed
        if (!left->getType()->isIntegerTy(1)) {
            left = builder.CreateICmpNE(left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
        }
        
        // Create blocks for short-circuit evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "and_eval_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "and_merge", func);
        
        // If left is false, skip right and return false
        builder.CreateCondBr(left, evalRightBB, mergeBB);
        
        // Evaluate right
        builder.SetInsertPoint(evalRightBB);
        if (!right->getType()->isIntegerTy(1)) {
            right = builder.CreateICmpNE(right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
        }
        builder.CreateBr(mergeBB);
        
        // Merge
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "and_result");
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), evalRightBB->getSinglePredecessor());
        phi->addIncoming(right, evalRightBB);
        
        return phi;
    }
    
    if (op == TokenType::TOKEN_OR_OR) {
        // Convert to i1 if needed
        if (!left->getType()->isIntegerTy(1)) {
            left = builder.CreateICmpNE(left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
        }
        
        // Create blocks for short-circuit evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "or_eval_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "or_merge", func);
        
        // If left is true, skip right and return true
        builder.CreateCondBr(left, mergeBB, evalRightBB);
        
        // Evaluate right
        builder.SetInsertPoint(evalRightBB);
        if (!right->getType()->isIntegerTy(1)) {
            right = builder.CreateICmpNE(right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
        }
        builder.CreateBr(mergeBB);
        
        // Merge
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "or_result");
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1), evalRightBB->getSinglePredecessor());
        phi->addIncoming(right, evalRightBB);
        
        return phi;
    }
    
    // BITWISE OPERATORS
    if (op == TokenType::TOKEN_AMPERSAND) {
        return builder.CreateAnd(left, right, "andtmp");
    }
    
    if (op == TokenType::TOKEN_PIPE) {
        return builder.CreateOr(left, right, "ortmp");
    }
    
    if (op == TokenType::TOKEN_CARET) {
        return builder.CreateXor(left, right, "xortmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_LEFT) {
        return builder.CreateShl(left, right, "shltmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_RIGHT) {
        // Arithmetic right shift (sign extension)
        return builder.CreateAShr(left, right, "shrtmp");
    }
    
    // Unknown operator
    throw std::runtime_error("Unknown binary operator: " + expr->op.lexeme);
}

/**
 * Generate code for unary operations
 * Handles: neg, not, address, deref
 */
llvm::Value* ExprCodegen::codegenUnary(UnaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null unary expression");
    }
    
    // Generate code for the operand recursively
    llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
    if (!operand) {
        throw std::runtime_error("Failed to generate code for unary operand");
    }
    
    TokenType op = expr->op.type;
    bool isFloat = operand->getType()->isFloatingPointTy();
    
    // Arithmetic negation: -x
    if (op == TokenType::TOKEN_MINUS) {
        if (isFloat) {
            return builder.CreateFNeg(operand, "negtmp");
        } else {
            return builder.CreateNeg(operand, "negtmp");
        }
    }
    
    // Logical NOT: !x
    if (op == TokenType::TOKEN_BANG) {
        // If operand is not i1, need to compare with zero
        if (operand->getType()->isIntegerTy(1)) {
            // Already boolean, just XOR with true
            return builder.CreateNot(operand, "nottmp");
        } else if (isFloat) {
            // Compare float with 0.0
            llvm::Value* zero = llvm::ConstantFP::get(operand->getType(), 0.0);
            return builder.CreateFCmpOEQ(operand, zero, "nottmp");
        } else {
            // Compare integer with 0
            llvm::Value* zero = llvm::ConstantInt::get(operand->getType(), 0);
            return builder.CreateICmpEQ(operand, zero, "nottmp");
        }
    }
    
    // Bitwise NOT: ~x
    if (op == TokenType::TOKEN_TILDE) {
        if (isFloat) {
            throw std::runtime_error("Bitwise NOT cannot be applied to floating-point types");
        }
        return builder.CreateNot(operand, "bnottmp");
    }
    
    // Address-of operator: @x
    if (op == TokenType::TOKEN_AT) {
        // For address-of, we need the address, not the loaded value
        // This would require integration with symbol table to get alloca
        // For now, we'll return the operand itself if it's already a pointer
        // Full implementation requires lvalue handling in Phase 4.3+
        throw std::runtime_error("Address-of operator (@) requires lvalue support (Phase 4.3+)");
    }
    
    // Dereference operator: * (when TOKEN_STAR used as unary)
    if (op == TokenType::TOKEN_STAR) {
        // Dereference a pointer: load from the pointer
        if (!operand->getType()->isPointerTy()) {
            throw std::runtime_error("Dereference operator (*) can only be applied to pointer types");
        }
        return builder.CreateLoad(llvm::Type::getInt32Ty(context), operand, "dereftmp");
    }
    
    // Increment/decrement operators (++, --)
    if (op == TokenType::TOKEN_PLUS_PLUS || op == TokenType::TOKEN_MINUS_MINUS) {
        throw std::runtime_error("Increment/decrement operators (++/--) require lvalue support (Phase 4.3+)");
    }
    
    throw std::runtime_error("Unknown unary operator: " + std::to_string(static_cast<int>(op)));
}

/**
 * Generate code for function calls
 */
llvm::Value* ExprCodegen::codegenCall(CallExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null call expression");
    }
    
    // The callee should be an identifier (function name or func variable)
    IdentifierExpr* callee_ident = dynamic_cast<IdentifierExpr*>(expr->callee.get());
    if (!callee_ident) {
        throw std::runtime_error("Function callee must be an identifier");
    }
    
    // Check if this is a direct function call or a closure call
    // Try to find a direct function first
    llvm::Function* direct_func = module->getFunction(callee_ident->name);
    
    // Check if it's a closure (func variable in named_values)
    auto it = named_values.find(callee_ident->name);
    bool is_closure_call = (direct_func == nullptr && it != named_values.end());
    
    if (is_closure_call) {
        // ====================================================================
        // CLOSURE CALLING CONVENTION (Fat Pointer Call)
        // ====================================================================
        // Fat pointer struct: { i8* method_ptr, i8* env_ptr }
        // Calling convention: call method_ptr(env_ptr, explicit_args...)
        
        llvm::Value* fat_ptr_alloca = it->second;
        
        // Load the fat pointer struct from memory
        // Define the fat pointer struct type
        std::vector<llvm::Type*> fat_ptr_fields = {
            llvm::PointerType::get(context, 0),  // method_ptr
            llvm::PointerType::get(context, 0)   // env_ptr
        };
        llvm::StructType* fat_ptr_type = llvm::StructType::get(context, fat_ptr_fields);
        
        llvm::Value* fat_ptr_value = builder.CreateLoad(fat_ptr_type, fat_ptr_alloca, "fat_ptr");
        
        // Extract method_ptr (field 0)
        llvm::Value* method_ptr = builder.CreateExtractValue(fat_ptr_value, 0, "method_ptr");
        
        // Extract env_ptr (field 1)
        llvm::Value* env_ptr = builder.CreateExtractValue(fat_ptr_value, 1, "env_ptr");
        
        // Evaluate explicit arguments
        std::vector<llvm::Value*> args;
        
        // Hidden first argument: env_ptr
        args.push_back(env_ptr);
        
        // Explicit arguments
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            llvm::Value* arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for closure argument " + std::to_string(i));
            }
            args.push_back(arg_value);
        }
        
        // Build function type for the call
        // Return type: For now assume i64, will need type inference later
        // Parameters: env_ptr (i8*) + explicit arg types
        std::vector<llvm::Type*> param_types;
        param_types.push_back(llvm::PointerType::get(context, 0));  // env_ptr
        for (size_t i = 1; i < args.size(); i++) {
            param_types.push_back(args[i]->getType());
        }
        
        // TODO: Determine actual return type from type system
        // For now, assume i64 return type
        llvm::Type* return_type = llvm::Type::getInt64Ty(context);
        
        llvm::FunctionType* closure_func_type = llvm::FunctionType::get(
            return_type,
            param_types,
            false  // not vararg
        );
        
        // Cast method_ptr (i8*) to typed function pointer
        llvm::Value* typed_func_ptr = builder.CreateBitCast(
            method_ptr,
            llvm::PointerType::get(closure_func_type, 0),
            "typed_method_ptr"
        );
        
        // Create indirect call through function pointer
        return builder.CreateCall(closure_func_type, typed_func_ptr, args, "closure_call");
        
    } else if (direct_func) {
        // ====================================================================
        // DIRECT FUNCTION CALL (Standard calling convention)
        // ====================================================================
        
        // Verify argument count matches
        if (direct_func->arg_size() != expr->arguments.size()) {
            throw std::runtime_error("Incorrect number of arguments passed to function " + 
                                    callee_ident->name + ": expected " + 
                                    std::to_string(direct_func->arg_size()) + 
                                    ", got " + std::to_string(expr->arguments.size()));
        }
        
        // Evaluate all arguments recursively
        std::vector<llvm::Value*> args;
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            llvm::Value* arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for argument " + std::to_string(i));
            }
            args.push_back(arg_value);
        }
        
        // Generate the call instruction
        return builder.CreateCall(direct_func, args, "calltmp");
        
    } else {
        throw std::runtime_error("Unknown function or closure: " + callee_ident->name);
    }
}

/**
 * Generate code for ternary expressions (is ? :)
 * Syntax: is condition : true_value : false_value
 * Generates branching control flow with PHI node for result merging
 */
llvm::Value* ExprCodegen::codegenTernary(TernaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null ternary expression");
    }
    
    // Get current function for creating basic blocks
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Evaluate the condition
    llvm::Value* condition = codegenExpressionNode(expr->condition.get(), this);
    if (!condition) {
        throw std::runtime_error("Failed to generate code for ternary condition");
    }
    
    // If condition is not i1, convert to boolean (compare with zero)
    if (!condition->getType()->isIntegerTy(1)) {
        if (condition->getType()->isFloatingPointTy()) {
            llvm::Value* zero = llvm::ConstantFP::get(condition->getType(), 0.0);
            condition = builder.CreateFCmpONE(condition, zero, "ternary_cond");
        } else {
            llvm::Value* zero = llvm::ConstantInt::get(condition->getType(), 0);
            condition = builder.CreateICmpNE(condition, zero, "ternary_cond");
        }
    }
    
    // Create basic blocks for control flow
    llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(context, "ternary_true", func);
    llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(context, "ternary_false", func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "ternary_merge", func);
    
    // Branch based on condition
    builder.CreateCondBr(condition, true_bb, false_bb);
    
    // Generate code for true branch
    builder.SetInsertPoint(true_bb);
    llvm::Value* true_value = codegenExpressionNode(expr->trueValue.get(), this);
    if (!true_value) {
        throw std::runtime_error("Failed to generate code for ternary true value");
    }
    builder.CreateBr(merge_bb);
    // Update true_bb in case code generation changed the current block
    true_bb = builder.GetInsertBlock();
    
    // Generate code for false branch
    builder.SetInsertPoint(false_bb);
    llvm::Value* false_value = codegenExpressionNode(expr->falseValue.get(), this);
    if (!false_value) {
        throw std::runtime_error("Failed to generate code for ternary false value");
    }
    builder.CreateBr(merge_bb);
    // Update false_bb in case code generation changed the current block
    false_bb = builder.GetInsertBlock();
    
    // Verify both branches produce the same type
    if (true_value->getType() != false_value->getType()) {
        throw std::runtime_error("Ternary branches must produce values of the same type");
    }
    
    // Create merge point with PHI node
    builder.SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder.CreatePHI(true_value->getType(), 2, "ternary_result");
    phi->addIncoming(true_value, true_bb);
    phi->addIncoming(false_value, false_bb);
    
    return phi;
}

/**
 * Generate code for array indexing
 */
llvm::Value* ExprCodegen::codegenIndex(IndexExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null index expression");
    }
    
    // TODO: Implement later with array support
    throw std::runtime_error("Array indexing not yet implemented");
}

/**
 * Generate code for member access
 */
llvm::Value* ExprCodegen::codegenMemberAccess(MemberAccessExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null member access expression");
    }
    
    // TODO: Implement later with struct support
    throw std::runtime_error("Member access not yet implemented");
}

// ============================================================================
// Phase 4.5.2: LAMBDA/CLOSURE CODE GENERATION
// ============================================================================
//
// Implements fat pointer representation for closures: { method_ptr, env_ptr }
// Reference: research_016 (Functional Types)
//
// Fat Pointer Layout (16 bytes on 64-bit):
//   struct FuncFatPtr {
//       void* method_ptr;  // Pointer to lambda body machine code
//       void* env_ptr;     // Pointer to captured environment (or NULL)
//   };
//
// Calling Convention:
//   1. Load method_ptr into temp register
//   2. Load env_ptr into dedicated register (hidden first argument)
//   3. Call method_ptr with env_ptr + explicit arguments
//   4. Inside lambda: access captures via env_ptr offset
//
// Capture Strategies:
//   - BY_VALUE: Copy primitives into environment struct
//   - BY_REFERENCE: Store pointer to variable in environment
//   - BY_MOVE: Transfer ownership (invalidate original)
//
// ============================================================================

/**
 * Generate code for lambda expression (closure)
 * Creates a fat pointer with method_ptr and env_ptr
 * 
 * Example Aria code:
 *   func:add = int8(int8:a, int8:b) { return a + b; };
 *   
 *   int8:x = 10;
 *   func:addX = int8(int8:y) { return x + y; };  // Captures x
 *
 * Generated LLVM IR (non-capturing):
 *   %lambda_body_1 = function returning i8, taking (i8*, i8, i8)
 *   %fat_ptr = alloca { i8*, i8* }
 *   %method_ptr = bitcast %lambda_body_1 to i8*
 *   %gep_0 = getelementptr { i8*, i8* }, %fat_ptr, 0, 0
 *   store i8* %method_ptr, i8** %gep_0
 *   %gep_1 = getelementptr { i8*, i8* }, %fat_ptr, 0, 1
 *   store i8* null, i8** %gep_1  ; No environment
 *   
 * Generated LLVM IR (capturing x):
 *   %env = alloca { i8 }  ; Environment with one i8 capture
 *   %x_val = load i8, i8* %x
 *   %env_field_0 = getelementptr { i8 }, %env, 0, 0
 *   store i8 %x_val, i8* %env_field_0
 *   %lambda_body_2 = function returning i8, taking (i8*, i8)
 *   %fat_ptr = alloca { i8*, i8* }
 *   %method_ptr = bitcast %lambda_body_2 to i8*
 *   %gep_0 = getelementptr { i8*, i8* }, %fat_ptr, 0, 0
 *   store i8* %method_ptr, i8** %gep_0
 *   %env_ptr = bitcast { i8 }* %env to i8*
 *   %gep_1 = getelementptr { i8*, i8* }, %fat_ptr, 0, 1
 *   store i8* %env_ptr, i8** %gep_1
 */
llvm::Value* ExprCodegen::codegenLambda(LambdaExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null lambda expression");
    }
    
    // ========================================================================
    // STEP 1: CREATE ENVIRONMENT STRUCT FOR CAPTURED VARIABLES
    // ========================================================================
    
    llvm::StructType* env_struct_type = nullptr;
    llvm::Value* env_alloca = nullptr;
    
    if (!expr->capturedVars.empty()) {
        // Build environment struct type: { type0, type1, ... }
        std::vector<llvm::Type*> env_field_types;
        
        for (const auto& captured : expr->capturedVars) {
            // For now, assume all captures are i64 (will be refined later)
            // TODO: Determine actual type from symbol table
            llvm::Type* field_type = llvm::Type::getInt64Ty(context);
            env_field_types.push_back(field_type);
        }
        
        // Create anonymous struct type for environment
        env_struct_type = llvm::StructType::create(context, env_field_types, "lambda_env");
        
        // Allocate environment on stack
        env_alloca = builder.CreateAlloca(env_struct_type, nullptr, "env");
        
        // Populate environment with captured values
        for (size_t i = 0; i < expr->capturedVars.size(); ++i) {
            const auto& captured = expr->capturedVars[i];
            
            // Look up captured variable in symbol table
            auto it = named_values.find(captured.name);
            if (it == named_values.end()) {
                throw std::runtime_error("Captured variable not found: " + captured.name);
            }
            
            llvm::Value* captured_value = it->second;
            
            // Handle capture mode
            if (captured.mode == LambdaExpr::CaptureMode::BY_VALUE) {
                // Load value and store into environment
                // Assuming it's an alloca
                if (llvm::isa<llvm::AllocaInst>(captured_value)) {
                    llvm::AllocaInst* alloca = llvm::cast<llvm::AllocaInst>(captured_value);
                    llvm::Type* allocated_type = alloca->getAllocatedType();
                    llvm::Value* loaded_val = builder.CreateLoad(allocated_type, captured_value, captured.name + "_val");
                    
                    // Get pointer to environment field
                    llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                    builder.CreateStore(loaded_val, env_field_ptr);
                } else {
                    // Direct value, store as-is
                    llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                    builder.CreateStore(captured_value, env_field_ptr);
                }
            } else if (captured.mode == LambdaExpr::CaptureMode::BY_REFERENCE) {
                // Store pointer to variable
                llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                // Cast to i64* and store (pointer to original variable)
                llvm::Value* ptr_as_i64 = builder.CreatePtrToInt(captured_value, llvm::Type::getInt64Ty(context));
                builder.CreateStore(ptr_as_i64, env_field_ptr);
            } else {
                // BY_MOVE: Transfer ownership (for now, treat like BY_VALUE)
                throw std::runtime_error("BY_MOVE capture not yet implemented");
            }
        }
    }
    
    // ========================================================================
    // STEP 2: GENERATE LAMBDA FUNCTION BODY
    // ========================================================================
    
    // Generate unique name for lambda function
    static int lambda_counter = 0;
    std::string lambda_name = "lambda_" + std::to_string(lambda_counter++);
    
    // Build parameter types: env_ptr (i8*) + explicit parameters
    std::vector<llvm::Type*> param_types;
    param_types.push_back(llvm::PointerType::get(context, 0));  // i8* env_ptr (hidden first parameter)
    
    for (const auto& param : expr->parameters) {
        // TODO: Get actual parameter type from AST
        // For now, assume all parameters are i64
        param_types.push_back(llvm::Type::getInt64Ty(context));
    }
    
    // Determine return type
    llvm::Type* return_type = llvm::Type::getVoidTy(context);
    if (!expr->returnTypeName.empty()) {
        // Map Aria type to LLVM type
        if (expr->returnTypeName == "int8" || expr->returnTypeName == "tbb8") {
            return_type = llvm::Type::getInt8Ty(context);
        } else if (expr->returnTypeName == "int32" || expr->returnTypeName == "tbb32") {
            return_type = llvm::Type::getInt32Ty(context);
        } else if (expr->returnTypeName == "int64" || expr->returnTypeName == "tbb64") {
            return_type = llvm::Type::getInt64Ty(context);
        } else if (expr->returnTypeName == "void") {
            return_type = llvm::Type::getVoidTy(context);
        }
        // TODO: Handle all types
    }
    
    // Create function type
    llvm::FunctionType* lambda_func_type = llvm::FunctionType::get(return_type, param_types, false);
    
    // Create lambda function
    llvm::Function* lambda_func = llvm::Function::Create(
        lambda_func_type,
        llvm::Function::InternalLinkage,  // Lambda bodies are internal
        lambda_name,
        module
    );
    
    // Create entry basic block
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(context, "entry", lambda_func);
    
    // Save current insertion point and named_values (lexical scope)
    llvm::BasicBlock* saved_insert_block = builder.GetInsertBlock();
    std::map<std::string, llvm::Value*> saved_named_values = named_values;
    named_values.clear();
    
    // Set insertion point to lambda body
    builder.SetInsertPoint(entry_block);
    
    // ========================================================================
    // STEP 2a: MAP LAMBDA PARAMETERS
    // ========================================================================
    
    // First argument is the hidden env_ptr (i8*)
    auto arg_it = lambda_func->arg_begin();
    llvm::Argument* env_arg = &(*arg_it);
    env_arg->setName("env");
    ++arg_it;
    
    // Map explicit parameters to allocas
    // This allows parameters to be mutable inside the lambda body
    size_t param_idx = 0;
    for (; arg_it != lambda_func->arg_end(); ++arg_it, ++param_idx) {
        llvm::Argument* arg = &(*arg_it);
        
        // Get parameter name from AST
        if (param_idx < expr->parameters.size()) {
            // Extract parameter name from ParameterNode
            ParameterNode* param_node = static_cast<ParameterNode*>(expr->parameters[param_idx].get());
            std::string param_name = param_node->paramName;
            
            arg->setName(param_name);
            
            // Create alloca for parameter
            llvm::AllocaInst* param_alloca = builder.CreateAlloca(arg->getType(), nullptr, param_name);
            builder.CreateStore(arg, param_alloca);
            
            // Add to named_values so lambda body can reference it
            named_values[param_name] = param_alloca;
        }
    }
    
    // ========================================================================
    // STEP 2b: MAP CAPTURED VARIABLES (Extract from environment)
    // ========================================================================
    
    // If we have captured variables, extract them from the environment pointer
    if (!expr->capturedVars.empty() && env_struct_type) {
        // Cast env_ptr (i8*) back to the environment struct type
        llvm::Value* env_ptr_typed = builder.CreateBitCast(
            env_arg,
            llvm::PointerType::get(env_struct_type, 0),
            "env_typed"
        );
        
        // Extract each captured variable from the environment
        for (size_t i = 0; i < expr->capturedVars.size(); ++i) {
            const auto& captured = expr->capturedVars[i];
            
            // Get pointer to field in environment struct
            llvm::Value* field_ptr = builder.CreateStructGEP(
                env_struct_type,
                env_ptr_typed,
                i,
                captured.name + "_ptr"
            );
            
            if (captured.mode == LambdaExpr::CaptureMode::BY_VALUE) {
                // BY_VALUE: Load the value from environment
                llvm::Type* field_type = env_struct_type->getElementType(i);
                llvm::Value* captured_value = builder.CreateLoad(field_type, field_ptr, captured.name);
                
                // Create alloca and store value (so it can be mutable in lambda)
                llvm::AllocaInst* capture_alloca = builder.CreateAlloca(field_type, nullptr, captured.name);
                builder.CreateStore(captured_value, capture_alloca);
                
                // Add to named_values
                named_values[captured.name] = capture_alloca;
                
            } else if (captured.mode == LambdaExpr::CaptureMode::BY_REFERENCE) {
                // BY_REFERENCE: Environment contains pointer to original variable
                // Load the pointer from environment (stored as i64)
                llvm::Value* ptr_as_i64 = builder.CreateLoad(
                    llvm::Type::getInt64Ty(context),
                    field_ptr,
                    captured.name + "_ptr_val"
                );
                
                // Convert i64 back to pointer
                // TODO: Get actual type from symbol table
                llvm::Value* original_ptr = builder.CreateIntToPtr(
                    ptr_as_i64,
                    llvm::PointerType::get(context, 0),
                    captured.name + "_ptr"
                );
                
                // Add to named_values (as pointer, so loads/stores go to original)
                named_values[captured.name] = original_ptr;
                
            } else {
                // BY_MOVE: Not yet implemented
                throw std::runtime_error("BY_MOVE capture mode not yet implemented in lambda body");
            }
        }
    }
    
    // ========================================================================
    // STEP 2c: GENERATE LAMBDA BODY
    // ========================================================================
    
    if (expr->body && stmt_codegen) {
        // Generate code for lambda body using StmtCodegen
        BlockStmt* body_block = static_cast<BlockStmt*>(expr->body.get());
        stmt_codegen->codegenBlock(body_block);
        
        // If body doesn't have a terminator, add default return
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder.CreateRetVoid();
            } else {
                // Return zero/null for non-void functions without explicit return
                if (return_type->isIntegerTy()) {
                    builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
                } else if (return_type->isFloatingPointTy()) {
                    builder.CreateRet(llvm::ConstantFP::get(return_type, 0.0));
                } else {
                    builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(return_type)));
                }
            }
        }
    } else {
        // No body or no stmt_codegen - generate placeholder return
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
        }
    }
    
    // Restore insertion point and named_values (return to outer scope)
    if (saved_insert_block) {
        builder.SetInsertPoint(saved_insert_block);
    }
    named_values = saved_named_values;
    
    // ========================================================================
    // STEP 3: CREATE FAT POINTER STRUCT
    // ========================================================================
    
    // Define fat pointer struct type: { i8* method_ptr, i8* env_ptr }
    std::vector<llvm::Type*> fat_ptr_fields = {
        llvm::PointerType::get(context, 0),  // method_ptr (function pointer as i8*)
        llvm::PointerType::get(context, 0)   // env_ptr (environment pointer as i8*)
    };
    llvm::StructType* fat_ptr_type = llvm::StructType::create(context, fat_ptr_fields, "func_fat_ptr");
    
    // Allocate fat pointer on stack
    llvm::Value* fat_ptr_alloca = builder.CreateAlloca(fat_ptr_type, nullptr, "fat_ptr");
    
    // Store method_ptr (function pointer)
    llvm::Value* method_ptr_field = builder.CreateStructGEP(fat_ptr_type, fat_ptr_alloca, 0, "method_ptr_field");
    llvm::Value* func_ptr_as_i8 = builder.CreateBitCast(lambda_func, llvm::PointerType::get(context, 0));
    builder.CreateStore(func_ptr_as_i8, method_ptr_field);
    
    // Store env_ptr (environment pointer or NULL)
    llvm::Value* env_ptr_field = builder.CreateStructGEP(fat_ptr_type, fat_ptr_alloca, 1, "env_ptr_field");
    if (env_alloca) {
        // We have captured variables, store environment pointer
        llvm::Value* env_ptr_as_i8 = builder.CreateBitCast(env_alloca, llvm::PointerType::get(context, 0));
        builder.CreateStore(env_ptr_as_i8, env_ptr_field);
    } else {
        // No captures, store NULL
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
        builder.CreateStore(null_ptr, env_ptr_field);
    }
    
    // Return fat pointer (as struct value, not pointer)
    // Load the struct from stack
    llvm::Value* fat_ptr_value = builder.CreateLoad(fat_ptr_type, fat_ptr_alloca, "fat_ptr_val");
    
    return fat_ptr_value;
}

// ============================================================================
// SPECIAL OPERATORS - FUTURE IMPLEMENTATION NOTES
// ============================================================================
//
// The following special operators from research_026 require additional language
// features to be fully implemented:
//
// 1. Unwrap Operator (?) - Postfix unary operator
//    - Requires: result<T> type implementation
//    - Purpose: Early return on error, monadic bind operation
//    - Will be implemented with: Phase 4.5+ (Result type support)
//
// 2. Safe Navigation Operator (?.)
//    - Requires: Null pointer tracking, optional types
//    - Purpose: Safe member/method access with null propagation
//    - Implementation: Branching control flow similar to ternary
//    - Will be implemented with: Phase 4.4+ (Struct/member access) + null handling
//
// 3. Null Coalescing Operator (??)
//    - Requires: Null value representation, optional types
//    - Purpose: Provide default value when expression is null/ERR
//    - Implementation: Similar to ternary with null check
//    - Will be implemented with: Phase 4.4+ (null handling)
//
// 4. Pipeline Operators (|>, <|)
//    - Forward pipeline (|>): data |> func desugars to func(data)
//    - Reverse pipeline (<|): func <| data desugars to func(data)
//    - Requires: AST transformation during parsing (desugaring)
//    - Will be implemented with: Phase 2+ (Parser enhancement for operator desugaring)
//
// 5. Range Operators (.., ...)
//    - Inclusive range (..): start..end includes both boundaries
//    - Exclusive range (...): start...end excludes end
//    - Requires: Range type implementation, iterator support
//    - Will be implemented with: Phase 4.7+ (Range and iterator support)
//
// Note: The ternary operator (is ? :) has been implemented in Phase 4.2.6
//       as it only requires basic control flow without additional type system
//       features.
//
// ============================================================================
