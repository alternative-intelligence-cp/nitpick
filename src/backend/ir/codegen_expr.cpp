#include "backend/ir/codegen_expr.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include "frontend/token.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <stdexcept>

using namespace aria;
using namespace aria::frontend;

using namespace aria;
using namespace aria::backend;
using namespace aria::sema;

ExprCodegen::ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values) {}

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
    
    // The callee should be an identifier (function name)
    IdentifierExpr* callee_ident = dynamic_cast<IdentifierExpr*>(expr->callee.get());
    if (!callee_ident) {
        throw std::runtime_error("Function callee must be an identifier");
    }
    
    // Look up the function in the module
    llvm::Function* callee_func = module->getFunction(callee_ident->name);
    if (!callee_func) {
        throw std::runtime_error("Unknown function referenced: " + callee_ident->name);
    }
    
    // Verify argument count matches
    if (callee_func->arg_size() != expr->arguments.size()) {
        throw std::runtime_error("Incorrect number of arguments passed to function " + 
                                callee_ident->name + ": expected " + 
                                std::to_string(callee_func->arg_size()) + 
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
    return builder.CreateCall(callee_func, args, "calltmp");
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
