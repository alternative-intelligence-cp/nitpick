// codegen_expr_binary.cpp — Split from codegen_expr.cpp for parallel builds (v0.8.2)
#include "backend/ir/codegen_expr.h"
#include "backend/ir/codegen_stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include "frontend/token.h"
#include "semantic/literal_converter.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include "debug_log.h"

using namespace npk;
using namespace npk::frontend;
using namespace npk::backend;
using namespace npk::sema;

/**
 * Generate code for binary operations
 * Handles: arithmetic, comparison, logical, bitwise operators
 * Phase 1 Task 1: TBB arithmetic automatically lowered to safe runtime functions
 */
llvm::Value* ExprCodegen::codegenBinary(BinaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null binary expression");
    }

    // Get the operator type early for TBB check
    TokenType op = expr->op.type;
    
    // ========================================================================
    // TBB AUTOMATIC LOWERING (Phase 1 Task 1)
    // Check if this is a TBB arithmetic operation and use safe runtime functions
    // ========================================================================
    std::string leftTBBType = getExprTBBTypeName(expr->left.get());
    std::string rightTBBType = getExprTBBTypeName(expr->right.get());
    
    ARIA_DBG_STREAM << "[DEBUG] TBB types: left='" << leftTBBType << "', right='" << rightTBBType << "'" << std::endl;

    if (!leftTBBType.empty() && !rightTBBType.empty() && leftTBBType == rightTBBType) {
        // Both operands are the same TBB type - use safe TBB arithmetic
        // Only for arithmetic operations: +, -, *, /, %
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT) {

            ARIA_DBG_STREAM << "[TBB] Lowering " << leftTBBType << " arithmetic (intrinsic)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for TBB operation operands");
            }

            // Generate TBB runtime function call
            llvm::Value* result = generateTBBBinaryOp(leftTBBType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if TBB call failed
        }
    }

    // ========================================================================
    // EXOTIC TYPE AUTOMATIC LOWERING
    // Check if this is an exotic type arithmetic operation (tryte/nyte)
    // ========================================================================
    std::string leftExoticType = getExprExoticTypeName(expr->left.get());
    std::string rightExoticType = getExprExoticTypeName(expr->right.get());
    
    ARIA_DBG_STREAM << "[DEBUG] Exotic types: left='" << leftExoticType << "', right='" << rightExoticType << "'" << std::endl;

    if (!leftExoticType.empty() && !rightExoticType.empty() && leftExoticType == rightExoticType) {
        // Both operands are the same exotic type - use runtime functions
        // Only for arithmetic operations: +, -, *, /, %
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT) {

            ARIA_DBG_STREAM << "[EXOTIC] Lowering " << leftExoticType << " arithmetic (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for exotic operation operands");
            }

            // Generate exotic runtime function call
            llvm::Value* result = generateExoticBinaryOp(leftExoticType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if exotic call failed
        }
    }

    // ========================================================================
    // NUMERIC TYPE AUTOMATIC LOWERING (Session 3)
    // Check if this is a numeric type arithmetic operation (frac*, tfp*, vec9)
    // ========================================================================
    std::string leftNumericType = getExprNumericTypeName(expr->left.get());
    std::string rightNumericType = getExprNumericTypeName(expr->right.get());
    
    ARIA_DBG_STREAM << "[DEBUG] Numeric types: left='" << leftNumericType << "', right='" << rightNumericType << "'" << std::endl;

    if (!leftNumericType.empty() && !rightNumericType.empty() && leftNumericType == rightNumericType) {
        // Both operands are the same numeric type - use runtime functions
        // For arithmetic operations: +, -, *, /
        // And comparison operations: ==, !=, <, <=, >, >=
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_EQUAL_EQUAL || op == TokenType::TOKEN_BANG_EQUAL ||
            op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_LESS_EQUAL ||
            op == TokenType::TOKEN_GREATER || op == TokenType::TOKEN_GREATER_EQUAL) {

            ARIA_DBG_STREAM << "[NUMERIC] Lowering " << leftNumericType << " arithmetic (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for numeric operation operands");
            }

            // Generate numeric runtime function call
            llvm::Value* result = generateNumericBinaryOp(leftNumericType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if numeric call failed
        }
    }

    // ========================================================================
    // LBIM TYPE AUTOMATIC LOWERING (ARIA-024, ARIA-025)
    // Check if this is a LBIM type arithmetic operation (int128/256/512/1024, uint*, fix256)
    // Large integers stored as structs - MUST use runtime library
    // ========================================================================
    std::string leftLBIMType = getExprLBIMTypeName(expr->left.get());
    std::string rightLBIMType = getExprLBIMTypeName(expr->right.get());
    
    ARIA_DBG_STREAM << "[DEBUG] LBIM types: left='" << leftLBIMType << "', right='" << rightLBIMType << "'" << std::endl;

    if (!leftLBIMType.empty() && !rightLBIMType.empty() && leftLBIMType == rightLBIMType) {
        // Both operands are the same LBIM type - use runtime functions
        // For arithmetic operations: +, -, *, /, %
        // And comparison operations: ==, !=, <, <=, >, >=
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT ||
            op == TokenType::TOKEN_EQUAL_EQUAL || op == TokenType::TOKEN_BANG_EQUAL ||
            op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_LESS_EQUAL ||
            op == TokenType::TOKEN_GREATER || op == TokenType::TOKEN_GREATER_EQUAL ||
            op == TokenType::TOKEN_SPACESHIP ||
            op == TokenType::TOKEN_AMPERSAND ||
            op == TokenType::TOKEN_PIPE ||
            op == TokenType::TOKEN_CARET) {

            ARIA_DBG_STREAM << "[LBIM] Lowering " << leftLBIMType << " operation (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for LBIM operation operands");
            }

            // Generate LBIM runtime function call
            llvm::Value* result = generateLBIMBinaryOp(leftLBIMType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if LBIM call failed
        }
    }

    // Generate code for left and right operands
    llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
    llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

    if (!left || !right) {
        throw std::runtime_error("Failed to generate code for binary operation operands");
    }

    // SPECIAL HANDLING: NIL comparison with optional types
    // If one side is NIL literal and other is optional, create proper OptionalNone struct
    auto* leftLiteral = dynamic_cast<LiteralExpr*>(expr->left.get());
    auto* rightLiteral = dynamic_cast<LiteralExpr*>(expr->right.get());
    bool leftIsNIL = (leftLiteral && leftLiteral->explicit_type == "NIL");
    bool rightIsNIL = (rightLiteral && rightLiteral->explicit_type == "NIL");
    
    if (leftIsNIL || rightIsNIL) {
        // Check if the other operand is an optional (struct with 2 fields: {i1, T})
        llvm::Type* wrappedType = nullptr;
        
        if (leftIsNIL && right->getType()->isStructTy()) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(right->getType());
            if (structType->getNumElements() == 2 && 
                structType->getElementType(0)->isIntegerTy(1)) {
                // Right is optional - replace left NIL with OptionalNone
                wrappedType = structType->getElementType(1);
                left = createOptionalNone(wrappedType);
                ARIA_DBG_STREAM << "[DEBUG] Replaced left NIL with OptionalNone for comparison" << std::endl;
            }
        } else if (rightIsNIL && left->getType()->isStructTy()) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(left->getType());
            if (structType->getNumElements() == 2 && 
                structType->getElementType(0)->isIntegerTy(1)) {
                // Left is optional - replace right NIL with OptionalNone
                wrappedType = structType->getElementType(1);
                right = createOptionalNone(wrappedType);
                ARIA_DBG_STREAM << "[DEBUG] Replaced right NIL with OptionalNone for comparison" << std::endl;
            }
        }

        // Handle pointer-NIL comparison: NIL generates as empty struct {},
        // but when compared to a pointer we need null pointer instead
        if (leftIsNIL && right->getType()->isPointerTy()) {
            left = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
            ARIA_DBG_STREAM << "[DEBUG] Replaced left NIL with null pointer for pointer comparison" << std::endl;
        } else if (rightIsNIL && left->getType()->isPointerTy()) {
            right = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
            ARIA_DBG_STREAM << "[DEBUG] Replaced right NIL with null pointer for pointer comparison" << std::endl;
        }

        // Handle integer-NIL comparison: NIL sentinel for integers is 0
        if (leftIsNIL && right->getType()->isIntegerTy()) {
            left = llvm::ConstantInt::get(right->getType(), 0);
            ARIA_DBG_STREAM << "[DEBUG] Replaced left NIL with 0 for integer comparison" << std::endl;
        } else if (rightIsNIL && left->getType()->isIntegerTy()) {
            right = llvm::ConstantInt::get(left->getType(), 0);
            ARIA_DBG_STREAM << "[DEBUG] Replaced right NIL with 0 for integer comparison" << std::endl;
        }
    }

    // Check if operands are vectors
    bool leftIsVector = left->getType()->isVectorTy();
    bool rightIsVector = right->getType()->isVectorTy();
    bool isVector = leftIsVector || rightIsVector;
    (void)isVector;
    
    ARIA_DBG_STREAM << "[BROADCAST DEBUG] leftIsVector=" << leftIsVector 
              << ", rightIsVector=" << rightIsVector << std::endl;
    
    // For vector-scalar operations, broadcast scalar to vector
    if (leftIsVector && !rightIsVector) {
        ARIA_DBG_STREAM << "[BROADCAST] Broadcasting right scalar to vector" << std::endl;
        // Right is scalar, left is vector - broadcast right
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(left->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        ARIA_DBG_STREAM << "[BROADCAST] numElements=" << numElements << std::endl;
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, right, i);
        }
        right = vec;
        rightIsVector = true;
        ARIA_DBG_STREAM << "[BROADCAST] Broadcast complete" << std::endl;
    } else if (!leftIsVector && rightIsVector) {
        ARIA_DBG_STREAM << "[BROADCAST] Broadcasting left scalar to vector" << std::endl;
        // Left is scalar, right is vector - broadcast left
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(right->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        ARIA_DBG_STREAM << "[BROADCAST] numElements=" << numElements << std::endl;
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, left, i);
        }
        left = vec;
        leftIsVector = true;
        ARIA_DBG_STREAM << "[BROADCAST] Broadcast complete" << std::endl;
    }
    
    // Auto-unwrap Result<T> structs in binary expressions.
    // Aria functions return { T, ptr, i8 } but binary ops need raw T.
    auto isResultStruct = [](llvm::Type* ty) -> bool {
        if (!ty->isStructTy()) return false;
        llvm::StructType* st = llvm::cast<llvm::StructType>(ty);
        return st->getNumElements() == 3 &&
               st->getElementType(1)->isPointerTy() &&
               st->getElementType(2)->isIntegerTy(8);
    };
    if (isResultStruct(left->getType()) && !isResultStruct(right->getType())) {
        left = builder.CreateExtractValue(left, {0}, "unwrap_result_lhs");
        ARIA_DBG_STREAM << "[DEBUG] Auto-unwrapped Result<T> on left operand (codegen_expr)" << std::endl;
    }
    if (isResultStruct(right->getType()) && !isResultStruct(left->getType())) {
        right = builder.CreateExtractValue(right, {0}, "unwrap_result_rhs");
        ARIA_DBG_STREAM << "[DEBUG] Auto-unwrapped Result<T> on right operand (codegen_expr)" << std::endl;
    }

    // Check if operands are floating point (check AFTER broadcast)
    bool isFloat = false;
    if (leftIsVector) {
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(left->getType());
        isFloat = vecType->getElementType()->isFloatingPointTy();
    } else if (rightIsVector) {
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(right->getType());
        isFloat = vecType->getElementType()->isFloatingPointTy();
    } else {
        // Both are scalars - check either one
        isFloat = left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy();
    }
    
    // DEBUG: Print types
#ifdef ARIA_DEBUG_CODEGEN
    std::cerr << "Binary op: left type = ";
    left->getType()->print(llvm::errs());
    std::cerr << ", right type = ";
    right->getType()->print(llvm::errs());
    std::cerr << ", isFloat = " << isFloat << std::endl;
#endif
    
    // Coerce integer operands to matching types
    // Integer literals default to i64 but variables may be i8/i16/i32
    if (!isFloat && !leftIsVector && !rightIsVector &&
        left->getType()->isIntegerTy() && right->getType()->isIntegerTy() &&
        left->getType() != right->getType()) {
        unsigned leftBits = left->getType()->getIntegerBitWidth();
        unsigned rightBits = right->getType()->getIntegerBitWidth();
        if (leftBits < rightBits) {
            // Widen left to match right (preserves precision)
            left = builder.CreateSExt(left, right->getType(), "sext_lhs");
        } else {
            // Widen right to match left
            right = builder.CreateSExt(right, left->getType(), "sext_rhs");
        }
    }
    
    // ARITHMETIC OPERATORS
    if (op == TokenType::TOKEN_PLUS) {
        // String concatenation: string + string calls npk_string_concat_simple
        // which takes AriaString* pointers and returns AriaString* (aborts on error)
        if (left->getType()->isPointerTy() || right->getType()->isPointerTy()) {
            llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.NpkString");
            if (!ariaStrType) {
                ariaStrType = llvm::StructType::create(context,
                    {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                     llvm::Type::getInt64Ty(context)},
                    "struct.NpkString");
            }
            llvm::Type* strPtrTy = llvm::PointerType::get(ariaStrType, 0);
            llvm::FunctionType* concatFT = llvm::FunctionType::get(
                strPtrTy, {strPtrTy, strPtrTy}, false);
            llvm::FunctionCallee concatFn = module->getOrInsertFunction("npk_string_concat_simple", concatFT);
            // left and right are already AriaString* pointers from codegenBinary
            return builder.CreateCall(concatFn, {left, right}, "str.concat");
        }
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
    
    // Detect unsigned types for division, modulo, and comparison operators
    bool isUnsigned = false;
    auto isUnsignedTypeName = [](const std::string& n) {
        return n == "uint1" || n == "uint2" || n == "uint4"
            || n == "uint8" || n == "uint16" || n == "uint32" || n == "uint64"
            || n == "uint128" || n == "uint256" || n == "uint512"
            || n == "u8" || n == "u16" || n == "u32" || n == "u64";
    };
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->left.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && isUnsignedTypeName(it->second)) isUnsigned = true;
    }
    if (!isUnsigned && expr->right->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->right.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && isUnsignedTypeName(it->second)) isUnsigned = true;
    }
    if (!isUnsigned && leftLiteral && isUnsignedTypeName(leftLiteral->explicit_type)) isUnsigned = true;
    if (!isUnsigned && rightLiteral && isUnsignedTypeName(rightLiteral->explicit_type)) isUnsigned = true;

    if (op == TokenType::TOKEN_SLASH) {
        if (isFloat) {
            return builder.CreateFDiv(left, right, "divtmp");
        } else if (isUnsigned) {
            return builder.CreateUDiv(left, right, "divtmp");
        } else {
            return builder.CreateSDiv(left, right, "divtmp");
        }
    }
    
    if (op == TokenType::TOKEN_PERCENT) {
        if (isFloat) {
            return builder.CreateFRem(left, right, "modtmp");
        } else if (isUnsigned) {
            return builder.CreateURem(left, right, "modtmp");
        } else {
            return builder.CreateSRem(left, right, "modtmp");
        }
    }
    
    // COMPARISON OPERATORS

    // Detect string types for comparison operators
    bool isString = false;
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->left.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && it->second == "string") isString = true;
    }
    if (!isString && expr->right->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->right.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && it->second == "string") isString = true;
    }
    // Also detect string literals (they produce pointer types for AriaString)
    if (!isString && leftLiteral && leftLiteral->explicit_type == "string") isString = true;
    if (!isString && rightLiteral && rightLiteral->explicit_type == "string") isString = true;

    // String comparison helper: loads AriaString structs from pointers and calls runtime
    auto getStrType = [&]() -> llvm::StructType* {
        llvm::StructType* st = llvm::StructType::getTypeByName(context, "struct.NpkString");
        if (!st) {
            st = llvm::StructType::create(context,
                {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                 llvm::Type::getInt64Ty(context)},
                "struct.NpkString");
        }
        return st;
    };

    auto stringCompareCall = [&]() -> llvm::Value* {
        llvm::StructType* ariaStrType = getStrType();
        llvm::Function* cmpFunc = module->getFunction("npk_string_compare_str");
        if (!cmpFunc) {
            llvm::FunctionType* cmpFT = llvm::FunctionType::get(
                builder.getInt32Ty(), {ariaStrType, ariaStrType}, false);
            cmpFunc = llvm::Function::Create(cmpFT, llvm::Function::ExternalLinkage,
                "npk_string_compare_str", module);
        }
        llvm::Value* lStr = left->getType()->isPointerTy()
            ? builder.CreateLoad(ariaStrType, left, "str.cmp.lhs")
            : left;
        llvm::Value* rStr = right->getType()->isPointerTy()
            ? builder.CreateLoad(ariaStrType, right, "str.cmp.rhs")
            : right;
        return builder.CreateCall(cmpFunc, {lStr, rStr}, "str.cmp");
    };

    auto stringEqualsCall = [&]() -> llvm::Value* {
        llvm::StructType* ariaStrType = getStrType();
        llvm::Function* eqFunc = module->getFunction("npk_string_equals");
        if (!eqFunc) {
            llvm::FunctionType* eqFT = llvm::FunctionType::get(
                builder.getInt1Ty(), {ariaStrType, ariaStrType}, false);
            eqFunc = llvm::Function::Create(eqFT, llvm::Function::ExternalLinkage,
                "npk_string_equals", module);
        }
        llvm::Value* lStr = left->getType()->isPointerTy()
            ? builder.CreateLoad(ariaStrType, left, "str.eq.lhs")
            : left;
        llvm::Value* rStr = right->getType()->isPointerTy()
            ? builder.CreateLoad(ariaStrType, right, "str.eq.rhs")
            : right;
        return builder.CreateCall(eqFunc, {lStr, rStr}, "str.eq");
    };

    if (op == TokenType::TOKEN_EQUAL_EQUAL) {
        if (isString) {
            return stringEqualsCall();
        }
        // Debug: Check type compatibility
#ifdef ARIA_DEBUG_CODEGEN
        std::cerr << "[DEBUG COMPARISON] Left type: ";
        left->getType()->print(llvm::errs());
        std::cerr << ", Right type: ";
        right->getType()->print(llvm::errs());
        std::cerr << std::endl;
#endif
        
        if (isFloat) {
            return builder.CreateFCmpOEQ(left, right, "eqtmp");
        } else {
            return builder.CreateICmpEQ(left, right, "eqtmp");
        }
    }
    
    if (op == TokenType::TOKEN_BANG_EQUAL) {
        if (isString) {
            llvm::Value* eq = stringEqualsCall();
            return builder.CreateNot(eq, "str.ne");
        }
        if (isFloat) {
            return builder.CreateFCmpUNE(left, right, "netmp");
        } else {
            return builder.CreateICmpNE(left, right, "netmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS) {
        if (isString) {
            llvm::Value* cmp = stringCompareCall();
            return builder.CreateICmpSLT(cmp, builder.getInt32(0), "str.lt");
        }
        if (isFloat) {
            return builder.CreateFCmpOLT(left, right, "lttmp");
        } else if (isUnsigned) {
            return builder.CreateICmpULT(left, right, "lttmp");
        } else {
            return builder.CreateICmpSLT(left, right, "lttmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS_EQUAL) {
        if (isString) {
            llvm::Value* cmp = stringCompareCall();
            return builder.CreateICmpSLE(cmp, builder.getInt32(0), "str.le");
        }
        if (isFloat) {
            return builder.CreateFCmpOLE(left, right, "letmp");
        } else if (isUnsigned) {
            return builder.CreateICmpULE(left, right, "letmp");
        } else {
            return builder.CreateICmpSLE(left, right, "letmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER) {
        if (isString) {
            llvm::Value* cmp = stringCompareCall();
            return builder.CreateICmpSGT(cmp, builder.getInt32(0), "str.gt");
        }
#ifdef ARIA_DEBUG_CODEGEN
        std::cerr << "[COMPARISON >] About to create ICmp/FCmp" << std::endl;
        std::cerr << "[COMPARISON >] isFloat=" << isFloat << std::endl;
        std::cerr << "[COMPARISON >] Left LLVM type: ";
        left->getType()->print(llvm::errs());
        std::cerr << std::endl;
        std::cerr << "[COMPARISON >] Right LLVM type: ";
        right->getType()->print(llvm::errs());
        std::cerr << std::endl;
#endif
        
        if (isFloat) {
            return builder.CreateFCmpOGT(left, right, "gttmp");
        } else if (isUnsigned) {
            return builder.CreateICmpUGT(left, right, "gttmp");
        } else {
            return builder.CreateICmpSGT(left, right, "gttmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER_EQUAL) {
        if (isString) {
            llvm::Value* cmp = stringCompareCall();
            return builder.CreateICmpSGE(cmp, builder.getInt32(0), "str.ge");
        }
        if (isFloat) {
            return builder.CreateFCmpOGE(left, right, "getmp");
        } else if (isUnsigned) {
            return builder.CreateICmpUGE(left, right, "getmp");
        } else {
            return builder.CreateICmpSGE(left, right, "getmp");
        }
    }
    
    // SPACESHIP OPERATOR (<=>)
    // Three-way comparison: returns -1 if left < right, 0 if equal, 1 if left > right
    if (op == TokenType::TOKEN_SPACESHIP) {
        if (isString) {
            llvm::Value* cmp = stringCompareCall();
            return builder.CreateSExt(cmp, llvm::Type::getInt64Ty(context), "str.spaceship");
        }
        llvm::Value* ltCmp, *gtCmp;
        
        if (isFloat) {
            ltCmp = builder.CreateFCmpOLT(left, right, "spaceship.lt");
            gtCmp = builder.CreateFCmpOGT(left, right, "spaceship.gt");
        } else {
            ltCmp = builder.CreateICmpSLT(left, right, "spaceship.lt");
            gtCmp = builder.CreateICmpSGT(left, right, "spaceship.gt");
        }
        
        // Use select instructions:
        // result = select(lt, -1, select(gt, 1, 0))
        llvm::Value* negOne = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), -1);
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
        llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1);
        
        llvm::Value* gtResult = builder.CreateSelect(gtCmp, one, zero, "spaceship.gt_sel");
        llvm::Value* result = builder.CreateSelect(ltCmp, negOne, gtResult, "spaceship.result");
        
        return result;
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
    
    // NULL COALESCING OPERATOR (??) with short-circuit evaluation
    // Pattern: optional ?? default returns value if has value, otherwise default
    if (op == TokenType::TOKEN_NULL_COALESCE) {
        // Save left value and block
        llvm::Value* leftVal = left;
        (void)leftVal;
        llvm::BasicBlock* leftBB = builder.GetInsertBlock();
        
        // Create blocks for conditional evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* useRightBB = llvm::BasicBlock::Create(context, "coalesce_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "coalesce_merge", func);
        
        // Check if left is optional type
        llvm::Value* isNull;
        llvm::Value* unwrappedLeft;
        
        if (left->getType()->isStructTy()) {
            // Left is optional type { i1 hasValue, T value }
            // Extract hasValue field
            llvm::Value* hasValue = isOptionalSome(left);
            isNull = builder.CreateNot(hasValue, "isNone");
            
            // Extract value for use if has value
            unwrappedLeft = unwrapOptional(left);
        } else if (left->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(left, "isnull");
            unwrappedLeft = left;
        } else if (left->getType()->isIntegerTy()) {
            // For integers, check against 0 (NIL sentinel)
            isNull = builder.CreateICmpEQ(left, llvm::ConstantInt::get(left->getType(), 0), "isnull");
            unwrappedLeft = left;
        } else {
            // For other types, assume not null
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            unwrappedLeft = left;
        }
        
        // If left is null, evaluate and use right; otherwise use unwrapped left
        builder.CreateCondBr(isNull, useRightBB, mergeBB);
        
        // Evaluate right side only if left was null
        builder.SetInsertPoint(useRightBB);
        llvm::Value* rightVal = right;
        llvm::BasicBlock* rightBB = builder.GetInsertBlock();
        builder.CreateBr(mergeBB);
        
        // Merge with phi node - use unwrapped type
        builder.SetInsertPoint(mergeBB);
        llvm::Type* resultType = unwrappedLeft->getType();
        llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "coalesce_result");
        phi->addIncoming(unwrappedLeft, leftBB);
        phi->addIncoming(rightVal, rightBB);
        
        return phi;
    }
    
    // BITWISE OPERATORS
    // Guard: LBIM extended types (int2048/int4096/uint2048/uint4096) are stored as
    // structs but were not caught by getExprLBIMTypeName above. Route them to runtime.
    auto lbimBitwiseGuard = [&](const char* opSuffix) -> llvm::Value* {
        (void)opSuffix;
        if (!left->getType()->isStructTy()) return nullptr;
        auto* st = llvm::cast<llvm::StructType>(left->getType());
        std::string sname = st->getName().str();
        // Struct name is e.g. "struct.int2048" — drop "struct." prefix
        if (sname.size() > 7) {
            std::string lbimTypeName = sname.substr(7);
            llvm::Value* res = generateLBIMBinaryOp(lbimTypeName, op, left, right);
            if (res) return res;
        }
        return nullptr;
    };

    if (op == TokenType::TOKEN_AMPERSAND) {
        if (auto* r = lbimBitwiseGuard("_and")) return r;
        return builder.CreateAnd(left, right, "andtmp");
    }
    
    if (op == TokenType::TOKEN_PIPE) {
        if (auto* r = lbimBitwiseGuard("_or")) return r;
        return builder.CreateOr(left, right, "ortmp");
    }
    
    if (op == TokenType::TOKEN_CARET) {
        if (auto* r = lbimBitwiseGuard("_xor")) return r;
        return builder.CreateXor(left, right, "xortmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_LEFT) {
        return builder.CreateShl(left, right, "shltmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_RIGHT) {
        // Determine signedness from Aria type to pick arithmetic vs logical shift right.
        // Signed types use AShr (sign-extends), unsigned use LShr (zero-fills).
        bool isSigned = false;
        if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->left.get());
            auto it = var_aria_types.find(ident->name);
            if (it != var_aria_types.end()) {
                const std::string& typeName = it->second;
                // Signed types: int8, int16, int32, int64, nit (NOT uint*)
                isSigned = (typeName.find("int") == 0 || typeName == "nit");
            }
        } else if (expr->left->type == ASTNode::NodeType::LITERAL) {
            // Literal type suffix determines signedness
            LiteralExpr* lit = static_cast<LiteralExpr*>(expr->left.get());
            if (!lit->explicit_type.empty()) {
                isSigned = (lit->explicit_type.find("int") == 0 || lit->explicit_type == "nit");
            }
        }
        if (isSigned) {
            return builder.CreateAShr(left, right, "ashrtmp");
        }
        return builder.CreateLShr(left, right, "shrtmp");
    }
    
    // SAFE NAVIGATION OPERATOR (?.)
    // Pattern: obj?.member returns optional<T> (Some(member) if obj not NIL, None otherwise)
    if (op == TokenType::TOKEN_SAFE_NAV) {
        // Check if left (object) is NIL/null
        llvm::Value* isNull;
        
        if (left->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(left, "obj.isnull");
        } else if (left->getType()->isStructTy()) {
            // If left is already optional, check hasValue
            isNull = builder.CreateNot(isOptionalSome(left), "obj.isnone");
        } else {
            // For primitive types (shouldn't normally happen), assume not null
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        }
        
        // Create basic blocks for conditional member access
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* accessBB = llvm::BasicBlock::Create(context, "safeNav.access", func);
        llvm::BasicBlock* nilBB = llvm::BasicBlock::Create(context, "safeNav.nil", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "safeNav.merge", func);
        
        // Branch based on null check
        builder.CreateCondBr(isNull, nilBB, accessBB);
        
        // Access block: evaluate member access (right side)
        builder.SetInsertPoint(accessBB);
        llvm::Value* memberVal = right;  // Right is the member access result
        
        // Wrap result in optional Some
        llvm::Value* someResult = createOptionalSome(memberVal, memberVal->getType());
        builder.CreateBr(mergeBB);
        llvm::BasicBlock* afterAccessBB = builder.GetInsertBlock();
        
        // NIL block: return None
        builder.SetInsertPoint(nilBB);
        llvm::Value* noneResult = createOptionalNone(memberVal->getType());
        builder.CreateBr(mergeBB);
        
        // Merge results
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(someResult->getType(), 2, "safeNav.result");
        phi->addIncoming(someResult, afterAccessBB);
        phi->addIncoming(noneResult, nilBB);
        
        return phi;
    }
    
    // PIPELINE FORWARD OPERATOR (|>)
    // The parser should rewrite all pipeline expressions into CallExpr nodes.
    // If a BinaryExpr with |> reaches codegen, something went wrong in parsing.
    if (op == TokenType::TOKEN_PIPE_RIGHT) {
        throw std::runtime_error(
            "Pipeline operator |> reached binary codegen — should have been "
            "rewritten to CallExpr by parser. This is an internal compiler bug.");
    }
    
    // PIPELINE BACKWARD OPERATOR (<|)
    // Same as above — parser should rewrite these.
    if (op == TokenType::TOKEN_PIPE_LEFT) {
        throw std::runtime_error(
            "Pipeline operator <| reached binary codegen — should have been "
            "rewritten to CallExpr by parser. This is an internal compiler bug.");
    }
    
    // Unknown operator
    throw std::runtime_error("Unknown binary operator: " + expr->op.lexeme);
}

/**
 * Generate code for unary operations
 * Handles: neg, not, address, deref
 * Phase 1 Task 1: TBB negation automatically lowered to safe runtime function
 */
llvm::Value* ExprCodegen::codegenUnary(UnaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null unary expression");
    }

    TokenType op = expr->op.type;

    // ========================================================================
    // ========================================================================
    // TBB UNARY NEGATION LOWERING (Optimized Intrinsic Version)
    // Inline negation with sticky error check - no overflow possible
    // ========================================================================
    if (op == TokenType::TOKEN_MINUS) {
        std::string operandTBBType = getExprTBBTypeName(expr->operand.get());
        if (!operandTBBType.empty()) {
            ARIA_DBG_STREAM << "[TBB] Lowering " << operandTBBType << " negation (intrinsic)" << std::endl;

            llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
            if (!operand) {
                throw std::runtime_error("Failed to generate code for TBB negation operand");
            }

            // Sticky error check
            llvm::Type* type = operand->getType();
            llvm::Value* sentinel = getTBBSentinel(type);
            llvm::Value* isErr = builder.CreateICmpEQ(operand, sentinel, "is_err");
            
            // Negation is safe with symmetric range:
            // -127 → +127, +127 → -127 (no overflow)
            llvm::Value* rawRes = builder.CreateNeg(operand, "neg_raw");
            
            // Return sentinel if input was ERR, else return negated value
            return builder.CreateSelect(isErr, sentinel, rawRes, "tbb_neg");
        }
    }

    // B11 FIX: Handle @ (address-of / function reference) BEFORE generic operand
    // codegen, since @func_name references a module function, not a named_values variable.
    if (op == TokenType::TOKEN_AT) {
        if (expr->operand->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->operand.get());

            // First check named_values (local variables)
            auto it = named_values.find(ident->name);
            if (it != named_values.end()) {
                llvm::Value* address = it->second;
                if (!llvm::isa<llvm::AllocaInst>(address)) {
                    llvm::AllocaInst* tmp = builder.CreateAlloca(
                        address->getType(), nullptr, ident->name + ".addr");
                    builder.CreateStore(address, tmp);
                    return tmp;
                }
                return address;
            }

            // Then check module-level functions — @func_name returns fat pointer {funcptr, null}
            // Generate a thunk that matches closure calling convention (env_ptr, args...)
            llvm::Function* func = module->getFunction(ident->name);
            if (func) {
                // Create thunk: (ptr env, original_params...) → call original(params...)
                std::string thunkName = "__thunk_" + ident->name;
                llvm::Function* thunk = module->getFunction(thunkName);
                if (!thunk) {
                    // Build thunk param types: env_ptr (ptr) + original params
                    std::vector<llvm::Type*> thunkParams;
                    thunkParams.push_back(llvm::PointerType::get(context, 0)); // env
                    for (auto& arg : func->args()) {
                        thunkParams.push_back(arg.getType());
                    }
                    llvm::FunctionType* thunkFT = llvm::FunctionType::get(
                        func->getReturnType(), thunkParams, false);
                    thunk = llvm::Function::Create(thunkFT, llvm::Function::InternalLinkage,
                        thunkName, module);

                    // Generate thunk body: forward args (skip env) to original function
                    llvm::BasicBlock* savedBB = builder.GetInsertBlock();
                    llvm::BasicBlock* thunkEntry = llvm::BasicBlock::Create(context, "entry", thunk);
                    builder.SetInsertPoint(thunkEntry);

                    std::vector<llvm::Value*> fwdArgs;
                    auto targ = thunk->arg_begin();
                    targ->setName("env");
                    ++targ; // skip env
                    for (; targ != thunk->arg_end(); ++targ) {
                        fwdArgs.push_back(&*targ);
                    }
                    llvm::Value* result = builder.CreateCall(func, fwdArgs, "fwd");
                    builder.CreateRet(result);

                    builder.SetInsertPoint(savedBB);
                }

                // Build fat pointer {thunk, null_env}
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                llvm::StructType* fatPtrTy = llvm::StructType::get(context, {ptrTy, ptrTy});
                llvm::Value* thunkAsPtr = builder.CreateBitCast(thunk, ptrTy);
                llvm::Value* nullEnv = llvm::ConstantPointerNull::get(
                    llvm::cast<llvm::PointerType>(ptrTy));
                llvm::Value* fat = llvm::UndefValue::get(fatPtrTy);
                fat = builder.CreateInsertValue(fat, thunkAsPtr, 0, "fp_method");
                fat = builder.CreateInsertValue(fat, nullEnv, 1, "fp_env");
                return fat;
            }

            throw std::runtime_error("Variable or function '" + ident->name + "' not found for address-of (@)");
        }

        // For non-identifier operands, codegen normally then take address
        llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
        if (!operand) {
            throw std::runtime_error("Failed to generate code for @ operand");
        }
        throw std::runtime_error("Address-of operator (@) requires lvalue - only variables supported currently");
    }

    // Generate code for the operand recursively
    llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
    if (!operand) {
        throw std::runtime_error("Failed to generate code for unary operand");
    }

    bool isFloat = operand->getType()->isFloatingPointTy();

    // Arithmetic negation: -x
    if (op == TokenType::TOKEN_MINUS) {
        // ARIA SAFETY FIX: Check for exotic types first (runtime negation)
        std::string exoticType = getExprExoticTypeName(expr->operand.get());
        if (!exoticType.empty()) {
            ARIA_DBG_STREAM << "[EXOTIC] Generating negation for " << exoticType << " type" << std::endl;
            sema::PrimitiveType tempType(exoticType);
            return ternary_codegen.generateNeg(operand, &tempType);
        }
        
        if (isFloat) {
            return builder.CreateFNeg(operand, "negtmp");
        } else {
            return builder.CreateNeg(operand, "negtmp");
        }
    }
    
    // Logical NOT: !x
    if (op == TokenType::TOKEN_BANG) {
        // ARIA SAFETY FIX: Check for exotic types first (Kleene logic NOT)
        std::string exoticType = getExprExoticTypeName(expr->operand.get());
        if (!exoticType.empty()) {
            ARIA_DBG_STREAM << "[EXOTIC] Generating NOT for " << exoticType << " type" << std::endl;
            sema::PrimitiveType tempType(exoticType);
            return ternary_codegen.generateNot(operand, &tempType);
        }
        
        // Standard boolean NOT for non-exotic types
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
    
    // Address-of operator: @x (already handled above before operand codegen)
    // This path is unreachable for TOKEN_AT since it's handled early.
    
    // Dereference operator (blueprint style): value <- ptr
    // Arrow points FROM pointer TO value (showing data flow direction)
    if (op == TokenType::TOKEN_LEFT_ARROW) {
        // Dereference a pointer: load the value it points to
        if (!operand->getType()->isPointerTy()) {
            throw std::runtime_error("Dereference operator (<-) can only be applied to pointer types");
        }
        llvm::Type* pointeeType = nullptr;
        if (expr->operand->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->operand.get());
            auto ariaTypeIt = var_aria_types.find(ident->name);
            if (ariaTypeIt != var_aria_types.end()) {
                std::string pointeeName = ariaTypeIt->second;
                if (!pointeeName.empty() &&
                    (pointeeName.back() == '@' || pointeeName.back() == '*')) {
                    pointeeName.pop_back();
                    pointeeType = getLLVMTypeFromString(pointeeName);
                } else if (pointeeName.size() > 2 &&
                           pointeeName.substr(pointeeName.size() - 2) == "->") {
                    pointeeName = pointeeName.substr(0, pointeeName.size() - 2);
                    pointeeType = getLLVMTypeFromString(pointeeName);
                }
            }
        }
        if (expr->operand->type == ASTNode::NodeType::UNARY_OP) {
            UnaryExpr* operandUnary = static_cast<UnaryExpr*>(expr->operand.get());
            if (operandUnary->op.type == TokenType::TOKEN_AT &&
                operandUnary->operand &&
                operandUnary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(operandUnary->operand.get());
                auto targetIt = named_values.find(ident->name);
                if (targetIt != named_values.end()) {
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(targetIt->second)) {
                        pointeeType = allocaInst->getAllocatedType();
                    } else if (auto* global = llvm::dyn_cast<llvm::GlobalVariable>(targetIt->second)) {
                        pointeeType = global->getValueType();
                    }
                }
            }
        }
        if (!pointeeType) {
            pointeeType = llvm::Type::getInt32Ty(context);
        }
        return builder.CreateLoad(pointeeType, operand, "deref");
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
        // Require an identifier lvalue so we can find the alloca to write back
        if (expr->operand->type != ASTNode::NodeType::IDENTIFIER) {
            throw std::runtime_error("++ and -- require an identifier lvalue");
        }
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->operand.get());
        auto it = named_values.find(ident->name);
        if (it == named_values.end()) {
            throw std::runtime_error("++ / -- : variable '" + ident->name + "' not found");
        }
        llvm::Value* alloca_ptr = it->second;
        llvm::Value* oldVal = operand;  // already loaded above
        llvm::Value* one;
        if (oldVal->getType()->isFloatingPointTy()) {
            one = llvm::ConstantFP::get(oldVal->getType(), 1.0);
        } else {
            one = llvm::ConstantInt::get(oldVal->getType(), 1);
        }
        llvm::Value* newVal;
        if (op == TokenType::TOKEN_PLUS_PLUS) {
            newVal = oldVal->getType()->isFloatingPointTy()
                ? builder.CreateFAdd(oldVal, one, "inctmp")
                : builder.CreateAdd(oldVal, one, "inctmp");
        } else {
            newVal = oldVal->getType()->isFloatingPointTy()
                ? builder.CreateFSub(oldVal, one, "dectmp")
                : builder.CreateSub(oldVal, one, "dectmp");
        }
        builder.CreateStore(newVal, alloca_ptr);
        // Postfix: return old value; Prefix: return new value
        return expr->isPostfix ? oldVal : newVal;
    }
    
    throw std::runtime_error("Unknown unary operator: " + std::to_string(static_cast<int>(op)));
}

