// codegen_expr_compound.cpp — Split from codegen_expr.cpp for parallel builds (v0.8.2)
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

using namespace aria;
using namespace aria::frontend;
using namespace aria::backend;
using namespace aria::sema;

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
    
    // Special handling for identifier bases: avoid loading the whole array value.
    // For fixed-size arrays [N x T], we need the raw alloca pointer, not the loaded value.
    if (expr->array->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->array.get());
        auto it = named_values.find(ident->name);
        if (it != named_values.end()) {
            llvm::Value* var = it->second;
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                llvm::Type* allocated_type = allocaInst->getAllocatedType();
                if (allocated_type->isArrayTy()) {
                    // Properly-typed fixed-size array [N x T]
                    auto* arrTy = llvm::cast<llvm::ArrayType>(allocated_type);
                    llvm::Type* elemType = arrTy->getElementType();
                    
                    llvm::Value* indexVal = codegenExpressionNode(expr->index.get(), this);
                    if (!indexVal) {
                        throw std::runtime_error("Failed to generate index expression");
                    }
                    if (!indexVal->getType()->isIntegerTy(64)) {
                        indexVal = builder.CreateSExtOrTrunc(indexVal,
                                       builder.getInt64Ty(), "idx.i64");
                    }
                    std::vector<llvm::Value*> gep_indices = {
                        llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                        indexVal
                    };
                    llvm::Value* elem_ptr = builder.CreateInBoundsGEP(
                        allocated_type, var, gep_indices, "array.index.ptr");
                    return builder.CreateLoad(elemType, elem_ptr, "array.index.val");
                }
            }
        }
    }

    // Multi-dimensional array read: matrix[i][j] or deeper.
    // Walk the nested INDEX chain to find the root IDENTIFIER and collect all
    // dimension indices (outermost index last in the collection vector).
    if (expr->array->type == ASTNode::NodeType::INDEX) {
        std::vector<ASTNode*> all_indices;
        all_indices.push_back(expr->index.get()); // outermost index of this pair
        ASTNode* chain = expr->array.get();
        while (chain->type == ASTNode::NodeType::INDEX) {
            IndexExpr* inner = static_cast<IndexExpr*>(chain);
            all_indices.push_back(inner->index.get());
            chain = inner->array.get();
        }
        if (chain->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* root_ident = static_cast<IdentifierExpr*>(chain);
            auto nd_it = named_values.find(root_ident->name);
            if (nd_it != named_values.end()) {
                if (auto* nd_alloca = llvm::dyn_cast<llvm::AllocaInst>(nd_it->second)) {
                    llvm::Type* nd_alloc_ty = nd_alloca->getAllocatedType();
                    if (nd_alloc_ty->isArrayTy()) {
                        // all_indices = [outerIdx, ..., innerIdx] due to walk order.
                        // Iterate in REVERSE so GEP receives indices outermost-first:
                        // for matrix[i][j]: all_indices=[j,i], GEP needs [0, i, j].
                        std::vector<llvm::Value*> gep_idx = {
                            llvm::ConstantInt::get(builder.getInt64Ty(), 0)
                        };
                        for (int k = (int)all_indices.size() - 1; k >= 0; --k) {
                            llvm::Value* idx = codegenExpressionNode(all_indices[k], this);
                            if (!idx) throw std::runtime_error("Failed to codegen array index");
                            if (!idx->getType()->isIntegerTy(64))
                                idx = builder.CreateSExtOrTrunc(idx, builder.getInt64Ty(), "idx.i64");
                            gep_idx.push_back(idx);
                        }
                        // Descend the LLVM type hierarchy (same reverse order) to find element type
                        llvm::Type* elem_ty = nd_alloc_ty;
                        bool valid = true;
                        for (size_t k = 0; k < all_indices.size(); ++k) {
                            if (!elem_ty->isArrayTy()) { valid = false; break; }
                            elem_ty = llvm::cast<llvm::ArrayType>(elem_ty)->getElementType();
                        }
                        if (valid) {
                            llvm::Value* ptr = builder.CreateInBoundsGEP(
                                nd_alloc_ty, nd_alloca, gep_idx, "arrnd.ptr");
                            return builder.CreateLoad(elem_ty, ptr, "arrnd.val");
                        }
                    }
                }
            }
        }
    }

    // Struct field array access: obj.array_field[i]
    // e.g., gl.groups[0], c.nums[i]
    // We need a pointer to the element, so we use struct GEP + array GEP instead
    // of loading the whole array value (which would give us a non-pointer).
    if (expr->array->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->array.get());
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* objIdent = static_cast<IdentifierExpr*>(memberExpr->object.get());
            auto obj_it = named_values.find(objIdent->name);
            if (obj_it != named_values.end()) {
                llvm::Value* structPtr = obj_it->second;
                llvm::Type* structAllocType = nullptr;
                if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(structPtr))
                    structAllocType = ai->getAllocatedType();
                else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(structPtr))
                    structAllocType = gv->getValueType();
                if (structAllocType && structAllocType->isStructTy()) {
                    int fieldIdx = -1;
                    if (type_system) {
                        auto ariaTypeIt = var_aria_types.find(objIdent->name);
                        if (ariaTypeIt != var_aria_types.end()) {
                            sema::StructType* ariaStructType =
                                type_system->getStructType(ariaTypeIt->second);
                            if (ariaStructType) {
                                fieldIdx = ariaStructType->getFieldIndex(memberExpr->member);
                            }
                        }
                    }
                    if (fieldIdx >= 0) {
                        llvm::StructType* llvmStructTy =
                            llvm::cast<llvm::StructType>(structAllocType);
                        llvm::Type* fieldLLVMType = llvmStructTy->getElementType(fieldIdx);
                        if (fieldLLVMType->isArrayTy()) {
                            auto* arrTy = llvm::cast<llvm::ArrayType>(fieldLLVMType);
                            llvm::Type* elemTy = arrTy->getElementType();
                            llvm::Value* fieldPtr = builder.CreateStructGEP(
                                structAllocType, structPtr, fieldIdx,
                                memberExpr->member + ".field.ptr");
                            llvm::Value* idxVal =
                                codegenExpressionNode(expr->index.get(), this);
                            if (!idxVal)
                                throw std::runtime_error(
                                    "Failed to codegen struct-field array index");
                            if (!idxVal->getType()->isIntegerTy(64))
                                idxVal = builder.CreateSExtOrTrunc(
                                    idxVal, builder.getInt64Ty(), "idx.i64");
                            std::vector<llvm::Value*> gepIdx = {
                                llvm::ConstantInt::get(builder.getInt64Ty(), 0), idxVal};
                            llvm::Value* elemPtr = builder.CreateInBoundsGEP(
                                fieldLLVMType, fieldPtr, gepIdx,
                                memberExpr->member + ".elem.ptr");
                            return builder.CreateLoad(elemTy, elemPtr,
                                                     memberExpr->member + ".elem");
                        }
                    }
                }
            }
        }
        // Fall through to general path for more complex cases
    }

    // General path: generate code for the array/vector
    llvm::Value* arrayVal = codegenExpressionNode(expr->array.get(), this);
    if (!arrayVal) {
        throw std::runtime_error("Failed to generate code for indexed object");
    }
    
    // Generate code for the index
    llvm::Value* indexVal = codegenExpressionNode(expr->index.get(), this);
    if (!indexVal) {
        throw std::runtime_error("Failed to generate code for index");
    }
    
    llvm::Type* arrayType = arrayVal->getType();
    
    // Handle pointer-to-array (e.g., int64[], string[])
    if (arrayType->isPointerTy()) {
        // Array indexing: arr[i]
        // Determine the element type from the Aria type tracked in var_aria_types.
        // For a wild T-> pointer, var_aria_types stores "T@"; strip the "@" suffix
        // to get T, then map to an LLVM type.  Fallback to i64 (old behaviour).
        llvm::Type* elemType = llvm::Type::getInt64Ty(context);  // fallback

        if (expr->array->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ptr_ident = static_cast<IdentifierExpr*>(expr->array.get());
            auto typeIt = var_aria_types.find(ptr_ident->name);
            if (typeIt != var_aria_types.end()) {
                std::string ariaElemType = typeIt->second;
                // Strip trailing "@" (pointer indicator) to get the value type
                if (!ariaElemType.empty() && ariaElemType.back() == '@') {
                    ariaElemType.pop_back();
                }
                if (!ariaElemType.empty()) {
                    llvm::Type* resolved = getLLVMTypeFromString(ariaElemType);
                    if (resolved) elemType = resolved;
                }
            }
        }

        // Create GEP to access arr[index]
        // But first: if elemType is a vector (SIMD alloca), load the vector
        // and extractelement instead of GEP-based element access
        if (elemType->isVectorTy()) {
            llvm::Value* vecVal = builder.CreateLoad(elemType, arrayVal, "vec.load");
            if (!indexVal->getType()->isIntegerTy(32)) {
                indexVal = builder.CreateIntCast(indexVal, builder.getInt32Ty(), true, "idx.i32");
            }
            return builder.CreateExtractElement(vecVal, indexVal, "vec.elem");
        }

        llvm::Value* elemPtr = builder.CreateGEP(elemType, arrayVal, indexVal, "array.index.ptr");

        // Load the value from the pointer
        return builder.CreateLoad(elemType, elemPtr, "array.index.val");
    }
    
    // Handle vector indexing
    if (arrayType->isVectorTy() || (arrayType->isStructTy() && arrayType->getStructNumElements() == 9)) {
        // For vec2/vec3/SIMD (LLVM FixedVectorType): use ExtractElement
        if (arrayType->isVectorTy()) {
            // LLVM requires index to be i32 for ExtractElement
            if (!indexVal->getType()->isIntegerTy(32)) {
                if (indexVal->getType()->isIntegerTy()) {
                    // Cast to i32
                    indexVal = builder.CreateIntCast(indexVal, builder.getInt32Ty(), 
                                                    true, "idx.i32");
                } else {
                    throw std::runtime_error("Vector index must be an integer type");
                }
            }
            return builder.CreateExtractElement(arrayVal, indexVal, "vec.index");
        }
        
        // For vec9 (struct): Extract with constant index
        // Note: vec9[i] where i is not constant requires a different approach
        if (llvm::ConstantInt* constIndex = llvm::dyn_cast<llvm::ConstantInt>(indexVal)) {
            int idx = constIndex->getSExtValue();
            return builder.CreateExtractValue(arrayVal, idx, "vec9.index");
        } else {
            // Dynamic index into vec9 struct - needs switch/select pattern
            throw std::runtime_error("Dynamic indexing into vec9 not yet implemented");
        }
    }
    
    // TODO: Implement array indexing when arrays are added
    throw std::runtime_error("Array indexing not yet implemented");
}

/**
 * Generate code for member access
 */
llvm::Value* ExprCodegen::codegenMemberAccess(MemberAccessExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null member access expression");
    }
    
    std::cerr << "[DEBUG codegenMemberAccess] member: " << expr->member << std::endl;
    
    // Generate code for the object
    llvm::Value* objectVal = codegenExpressionNode(expr->object.get(), this);
    if (!objectVal) {
        throw std::runtime_error("Failed to generate code for member access object");
    }
    
    std::cerr << "[DEBUG codegenMemberAccess] objectVal type: ";
    objectVal->getType()->print(llvm::errs());
    std::cerr << std::endl;
    
    // Handle pointer member access (ptr->member)
    // This is equivalent to (*ptr).member - dereference then access
    if (expr->isPointerAccess) {
        // objectVal should be a pointer type
        if (!objectVal->getType()->isPointerTy()) {
            throw std::runtime_error("Arrow operator (->) requires pointer type");
        }
        
        // With LLVM opaque pointers, we need to know the pointed-to type
        // For now, assume it's a struct pointer and load it
        // TODO: Get proper type from type system
        objectVal = builder.CreateLoad(objectVal->getType(), objectVal, "deref_for_member");
    }
    
    llvm::Type* objectType = objectVal->getType();
    
    // Handle Result type member access (.is_error, .value, .error)
    // Reference: include/runtime/result.h - layout is { T value, void* error, bool is_error }
    // The type checker has already validated that this is a Result type with valid members
    if (objectType->isStructTy() && objectType->getStructNumElements() == 3) {
        std::cerr << "[DEBUG_RESULT_ACCESS] Accessing member '" << expr->member << "' on 3-field struct" << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal is instruction: " << llvm::isa<llvm::Instruction>(objectVal) << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal is argument: " << llvm::isa<llvm::Argument>(objectVal) << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal type: ";
        objectVal->getType()->print(llvm::errs());
        std::cerr << std::endl;
        
        // Check if this looks like a Result struct based on member names
        // (The type checker already validated this is a Result type)
        if (expr->member == "is_error") {
            // Field 2: is_error (bool)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting is_error (field 2)" << std::endl;
            llvm::Value* result = builder.CreateExtractValue(objectVal, 2, "is_error");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
        else if (expr->member == "value") {
            // Field 0: value (T)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting value (field 0)" << std::endl;
            llvm::Value* result = builder.CreateExtractValue(objectVal, 0, "value");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
        else if (expr->member == "error") {
            // Field 1: error (stored as void* via inttoptr in fail())
            // Convert back to int32 error code (matches failsafe tbb32 convention)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting error (field 1)" << std::endl;
            llvm::Value* errorPtr = builder.CreateExtractValue(objectVal, 1, "error_ptr");
            // Convert ptr directly to int32 error code
            llvm::Value* result = builder.CreatePtrToInt(errorPtr, builder.getInt32Ty(), "error_code");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
    }
    
    // Handle vector component access (.x, .y, .z)
    // vec9 is an anonymous 9-element LLVM struct (no LLVM name).
    // User-defined 9-field structs (e.g. Wave9) ARE named; those must fall through
    // to the regular struct member-access path below.
    bool isAnonymousVec9Struct = objectType->isStructTy()
        && objectType->getStructNumElements() == 9
        && !llvm::cast<llvm::StructType>(objectType)->hasName();
    if (objectType->isVectorTy() || isAnonymousVec9Struct) {
        int index = -1;
        
        if (expr->member == "x") index = 0;
        else if (expr->member == "y") index = 1;
        else if (expr->member == "z") index = 2;
        else {
            throw std::runtime_error("Invalid vector component: " + expr->member);
        }
        
        // For vec2/vec3 (LLVM FixedVectorType): use ExtractElement
        if (objectType->isVectorTy()) {
            return builder.CreateExtractElement(objectVal, index, expr->member);
        }
        
        // For vec9 (struct): use ExtractValue
        if (objectType->isStructTy()) {
            return builder.CreateExtractValue(objectVal, index, expr->member);
        }
    }
    
    // For safe navigation (?.), we need to check if the object is null
    if (expr->isSafeNavigation) {
        // Create blocks for conditional access
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* checkBB = builder.GetInsertBlock();
        llvm::BasicBlock* accessBB = llvm::BasicBlock::Create(context, "safe_nav_access", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "safe_nav_merge", func);
        
        // Check if object is null
        llvm::Value* isNull;
        if (objectVal->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(objectVal, "is_null");
        } else {
            // For other types, assume not null (TODO: handle optional types)
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        }
        
        // Branch: if null, skip to merge with null result; otherwise access member
        builder.CreateCondBr(isNull, mergeBB, accessBB);
        
        // Access block: perform the actual member access
        builder.SetInsertPoint(accessBB);
        llvm::Value* memberVal = nullptr;
        
        // TODO: Implement actual member access based on type
        // For now, this is a placeholder that needs struct support
        // For structs, we would use getelementptr to get the field offset
        
        // Temporary: just return a zero value
        // In reality, we need to:
        // 1. Get the struct type from type system
        // 2. Find the member field index
        // 3. Use getelementptr to compute field address
        // 4. Load the value from that address
        llvm::Type* resultType = llvm::Type::getInt64Ty(context); // Placeholder
        memberVal = llvm::ConstantInt::get(resultType, 0);
        
        llvm::BasicBlock* accessEndBB = builder.GetInsertBlock();
        builder.CreateBr(mergeBB);
        
        // Merge block: use phi node to select between null and actual value
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "safe_nav_result");
        phi->addIncoming(llvm::ConstantInt::get(resultType, 0), checkBB); // NIL/null case
        phi->addIncoming(memberVal, accessEndBB); // Successful access
        
        return phi;
    }
    
    // Regular struct member access (. or ->)
    // Need to extract field from struct value
    if (objectType->isStructTy()) {
        llvm::StructType* structType = llvm::cast<llvm::StructType>(objectType);
        std::string structName = structType->hasName() ? structType->getName().str() : "";
        
        std::cerr << "[DEBUG codegenMemberAccess] Struct type: " << structName 
                  << ", num elements: " << structType->getNumElements() << std::endl;
        
        // Get the struct type info from type system to find field index
        // For now, we'll need to look up the field index based on the struct type name
        // This should consult the symbol table or type registry
        
        // Try to get field index from the object's expression type
        // First, check if the object is an identifier so we can look up its type
        IdentifierExpr* objIdent = dynamic_cast<IdentifierExpr*>(expr->object.get());
        if (objIdent) {
            // Look up the Aria type name for this variable
            auto typeIt = var_aria_types.find(objIdent->name);
            if (typeIt != var_aria_types.end()) {
                std::string ariaTypeName = typeIt->second;
                std::cerr << "[DEBUG codegenMemberAccess] Variable " << objIdent->name 
                          << " has Aria type: " << ariaTypeName << std::endl;
                
                // TODO: Look up struct definition in type registry to get field index
                // For now, hardcode based on common struct patterns
                
                // Try to get field index from type system
                if (type_system) {
                    Type* ariaType = type_system->getStructType(ariaTypeName);
                    if (ariaType && ariaType->getKind() == TypeKind::STRUCT) {
                        StructType* structType = static_cast<StructType*>(ariaType);
                        int fieldIndex = structType->getFieldIndex(expr->member);
                        if (fieldIndex >= 0) {
                            std::cerr << "[DEBUG codegenMemberAccess] Found field '" << expr->member 
                                     << "' at index " << fieldIndex << std::endl;
                            return builder.CreateExtractValue(objectVal, fieldIndex, expr->member);
                        } else {
                            throw std::runtime_error("Field '" + expr->member + "' not found in struct " + ariaTypeName);
                        }
                    }
                }
                
                // Try to get field index based on struct layout
                // Fallback: use LLVM struct name (if available) for type system lookup
                if (type_system && !structName.empty()) {
                    // LLVM struct names are usually "struct.AriaTypeName" — strip the prefix
                    std::string lookupName = structName;
                    if (lookupName.rfind("struct.", 0) == 0) {
                        lookupName = lookupName.substr(7);
                    }
                    Type* fallbackType = type_system->getStructType(lookupName);
                    if (fallbackType && fallbackType->getKind() == TypeKind::STRUCT) {
                        StructType* sType = static_cast<StructType*>(fallbackType);
                        int fidx = sType->getFieldIndex(expr->member);
                        if (fidx >= 0) {
                            std::cerr << "[DEBUG codegenMemberAccess] Fallback lookup found field '"
                                     << expr->member << "' at index " << fidx << std::endl;
                            return builder.CreateExtractValue(objectVal, fidx, expr->member);
                        }
                    }
                }
                
                // Last resort: single-field struct extraction
                if (structType->getNumElements() == 1) {
                    std::cerr << "[DEBUG codegenMemberAccess] Single-field struct, extracting field 0" << std::endl;
                    return builder.CreateExtractValue(objectVal, 0, expr->member);
                }
                
                // For multi-field structs without type info, try index-based lookup
                // using LLVM struct element count
                throw std::runtime_error("Struct member access requires type system registration (field: " + expr->member + ", struct: " + structName + ")");
            }
        }
        
        // Fallback for non-identifier objects or untrack types:
        // Try LLVM struct name → type system lookup
        if (type_system && !structName.empty()) {
            std::string lookupName = structName;
            if (lookupName.rfind("struct.", 0) == 0) {
                lookupName = lookupName.substr(7);
            }
            Type* fallbackType = type_system->getStructType(lookupName);
            if (fallbackType && fallbackType->getKind() == TypeKind::STRUCT) {
                StructType* sType = static_cast<StructType*>(fallbackType);
                int fidx = sType->getFieldIndex(expr->member);
                if (fidx >= 0) {
                    std::cerr << "[DEBUG codegenMemberAccess] Outer fallback found field '"
                             << expr->member << "' at index " << fidx << std::endl;
                    return builder.CreateExtractValue(objectVal, fidx, expr->member);
                }
            }
        }
        
        // Last resort: try to extract value at index 0 for single-field structs
        std::cerr << "[DEBUG codegenMemberAccess] Unknown struct type, attempting index 0" << std::endl;
        if (structType->getNumElements() > 0) {
            return builder.CreateExtractValue(objectVal, 0, expr->member);
        }
    }
    
    throw std::runtime_error("Member access not supported for type: " + expr->member);
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
            (void)captured;
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
        // Extract parameter type from AST
        if (auto paramNode = std::dynamic_pointer_cast<ParameterNode>(param)) {
            std::string paramTypeStr = paramNode->typeNode ? paramNode->typeNode->toString() : "void";
            llvm::Type* param_type = getLLVMTypeFromString(paramTypeStr);
            param_types.push_back(param_type);
        } else {
            throw std::runtime_error("Invalid parameter node in lambda");
        }
    }
    
    // Determine return type from explicit annotation (required by Aria specs)
    llvm::Type* return_type = llvm::Type::getVoidTy(context);
    if (!expr->returnTypeName.empty()) {
        return_type = getLLVMTypeFromString(expr->returnTypeName);
    } else {
        throw std::runtime_error("Lambda missing required return type annotation");
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

// ============================================================================
// Phase 4.5.3: Await Expression Code Generation (Coroutine Suspension)
// ============================================================================

/**
 * Generate code for await expression
 * 
 * Creates a suspension point in the coroutine where execution can be paused
 * and later resumed. The LLVM coroutine transformation pass will split this
 * into a state machine.
 * 
 * Algorithm (from research_029):
 * 1. Evaluate the operand (expression yielding Future<T>)
 * 2. Call @llvm.coro.save to capture coroutine state
 * 3. Call @llvm.coro.suspend with save token
 * 4. Switch on suspend result:
 *    - 0 (resume): Continue to resume block
 *    - 1 (destroy): Jump to cleanup
 * 5. Resume block: Extract value from Future and continue
 * 
 * Example Aria code:
 *   async func:fetchData = int64() {
 *       int64:result = await someAsyncOp();
 *       pass(result);
 *   };
 * 
 * Generated LLVM IR:
 *   %future = call i8* @someAsyncOp()
 *   %save = call token @llvm.coro.save(i8* %handle)
 *   %suspend = call i8 @llvm.coro.suspend(token %save, i1 false)
 *   switch i8 %suspend, label %suspend.resume [
 *     i8 1, label %coro.cleanup
 *   ]
 * suspend.resume:
 *   %result = ... extract value from future ...
 *   ; continue execution
 * 
 * Reference: research_029 Section 5 (Compiler Lowering & State Machine)
 */
llvm::Value* ExprCodegen::codegenAwait(ASTNode* node) {
    AwaitExpr* expr = static_cast<AwaitExpr*>(node);
    
    std::cerr << "[DEBUG AWAIT] Entering codegenAwait" << std::endl;
    
    // Check that we're in an async function context
    llvm::Function* current_func = builder.GetInsertBlock()->getParent();
    if (!current_func) {
        throw std::runtime_error("await outside of function context");
    }
    
    // Check if current function is async
    // Async functions have special metadata or naming convention
    // For now, check if function name contains "async" or has async attribute
    std::string func_name = std::string(current_func->getName());
    std::cerr << "[DEBUG AWAIT] Function name: " << func_name << std::endl;
    bool is_async = func_name.find("async") != std::string::npos;
    
    std::cerr << "[DEBUG AWAIT] is_async (from name): " << is_async << std::endl;
    
    // Also check function metadata for async marker (if set by frontend)
    if (!is_async && current_func->hasMetadata("aria.async")) {
        is_async = true;
        std::cerr << "[DEBUG AWAIT] is_async (from metadata): true" << std::endl;
    }
    
    if (!is_async) {
        // ERROR: await in non-async function
        // Print proper error message to stderr (user-facing)
        std::cerr << "ERROR: 'await' can only be used in async functions (found in '" 
                  << func_name << "')" << std::endl;
        std::cerr << "  Hint: Change 'func:" << func_name << "' to 'async func:" << func_name << "'" << std::endl;
        
        // Return a dummy value to prevent LLVM crash
        // Return i32 0 (arbitrary - compilation will fail anyway due to error message)
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    }
    
    std::cerr << "[DEBUG AWAIT] Async check passed, continuing..." << std::endl;
    
    // Step 1: Evaluate the operand (should return a coroutine handle ptr)
    llvm::Value* coro_handle = codegenExpressionNode(expr->operand.get(), this);
    
    if (!coro_handle || !coro_handle->getType()->isPointerTy()) {
        std::cerr << "ERROR: await operand must return a coroutine handle (ptr)" << std::endl;
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    }
    
    // Step 2: Resume the awaited coroutine (runs it to completion for sync model)
    llvm::Function* coro_resume = llvm::Intrinsic::getOrInsertDeclaration(
        module,
        llvm::Intrinsic::coro_resume
    );
    builder.CreateCall(coro_resume, {coro_handle});
    
    // Step 3: Extract Result<T> from the awaited coroutine's promise
    // Use @llvm.coro.promise(ptr handle, i32 align, i1 from=false) to get promise ptr
    llvm::Function* coro_promise_fn = llvm::Intrinsic::getOrInsertDeclaration(
        module,
        llvm::Intrinsic::coro_promise
    );
    
    llvm::Value* promise_ptr = builder.CreateCall(
        coro_promise_fn,
        {coro_handle,
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 8),
         llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0)},
        "await.promise.ptr"
    );
    
    // Determine the awaited function's Result type by looking up the called function
    llvm::Type* result_type = nullptr;
    
    // The operand is typically a function call — find the callee for its metadata
    if (expr->operand->type == ASTNode::NodeType::CALL) {
        CallExpr* callExpr = static_cast<CallExpr*>(expr->operand.get());
        if (callExpr->callee->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(callExpr->callee.get());
            llvm::Function* callee = module->getFunction(ident->name);
            if (callee && callee->hasMetadata("aria.async")) {
                // Reconstruct the Result<T> type from the async function's AST info
                // The async function's promise type was set during its codegen
                // For now, use llvm.coro.promise to get the raw promise ptr
                // and trust the caller knows the type
            }
        }
    }
    
    // If we can't determine the exact type, try to infer from the awaited function's
    // original return type. For now, use a generic Result<i32> as common case.
    // The caller can use .value / .is_error on the returned Result.
    if (!result_type) {
        // Build Result<i32> as default (most common async return type)
        std::vector<llvm::Type*> result_fields = {
            llvm::Type::getInt32Ty(context),     // value
            llvm::PointerType::get(context, 0),  // error ptr
            llvm::Type::getInt8Ty(context)        // is_error
        };
        result_type = llvm::StructType::get(context, result_fields);
    }
    
    // Load the Result from the promise.
    // With proper final suspend routing, coro.resume suspends the frame (case 0)
    // WITHOUT freeing it. The frame is still valid, so we can safely read the promise.
    llvm::Value* result = builder.CreateLoad(result_type, promise_ptr, "await.result");
    
    // Now destroy the coroutine frame (triggers case 1 → cleanup → free)
    llvm::Function* coro_destroy = llvm::Intrinsic::getOrInsertDeclaration(
        module,
        llvm::Intrinsic::coro_destroy
    );
    builder.CreateCall(coro_destroy, {coro_handle});
    
    return result;
}

// ============================================================================
// Phase 2: Optional Types & Special Operators
// ============================================================================

/**
 * Get LLVM struct type for optional<T>: { i1 hasValue, T value }
 * Uses cached types to avoid recreation
 */
llvm::StructType* ExprCodegen::getOptionalType(llvm::Type* wrappedType) {
    // Create struct type: { i1 hasValue, T value }
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt1Ty(context),  // hasValue flag
        wrappedType                       // the wrapped value
    };
    
    // Create named struct for debugging
    std::string name = "optional." + std::string(wrappedType->getStructName());
    return llvm::StructType::create(context, fields, name);
}

/**
 * Create an optional value with hasValue=true
 */
llvm::Value* ExprCodegen::createOptionalSome(llvm::Value* value, llvm::Type* wrappedType) {
    llvm::StructType* optType = getOptionalType(wrappedType);
    
    // Create undef struct
    llvm::Value* opt = llvm::UndefValue::get(optType);
    
    // Set hasValue = true
    opt = builder.CreateInsertValue(opt, llvm::ConstantInt::getTrue(context), 0);
    
    // Set value
    opt = builder.CreateInsertValue(opt, value, 1);
    
    return opt;
}

/**
 * Create an optional value with hasValue=false (NIL)
 */
llvm::Value* ExprCodegen::createOptionalNone(llvm::Type* wrappedType) {
    llvm::StructType* optType = getOptionalType(wrappedType);
    
    // Create undef struct
    llvm::Value* opt = llvm::UndefValue::get(optType);
    
    // Set hasValue = false
    opt = builder.CreateInsertValue(opt, llvm::ConstantInt::getFalse(context), 0);
    
    // Value field is undef (won't be accessed)
    
    return opt;
}

/**
 * Check if optional hasValue (is not NIL)
 */
llvm::Value* ExprCodegen::isOptionalSome(llvm::Value* optional) {
    // Extract hasValue field (index 0)
    return builder.CreateExtractValue(optional, 0, "optional.hasValue");
}

/**
 * Extract value from optional (unwrap)
 * Note: Caller must check hasValue first!
 */
llvm::Value* ExprCodegen::unwrapOptional(llvm::Value* optional) {
    // Extract value field (index 1)
    return builder.CreateExtractValue(optional, 1, "optional.value");
}

/**
 * Generate code for vector constructor
 * vec2(x, y), vec3(x, y, z), vec9(c0, ..., c8)
 */
llvm::Value* ExprCodegen::codegenVectorConstructor(VectorConstructorExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null vector constructor expression");
    }
    
    int dimension = expr->dimension;
    
    // Generate code for all components
    std::vector<llvm::Value*> componentValues;
    llvm::Type* componentType = nullptr;
    
    for (const auto& comp : expr->components) {
        llvm::Value* val = codegenExpressionNode(comp.get(), this);
        if (!val) {
            throw std::runtime_error("Failed to generate code for vector component");
        }
        componentValues.push_back(val);
        
        // Get component type from first element
        if (!componentType) {
            componentType = val->getType();
        }
    }
    
    // For vec2 and vec3: Use LLVM FixedVectorType (SIMD)
    if (dimension == 2 || dimension == 3) {
        llvm::Type* vecType = llvm::FixedVectorType::get(componentType, dimension);
        
        // Start with undef vector
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        
        // Insert each component
        for (int i = 0; i < dimension; ++i) {
            vec = builder.CreateInsertElement(vec, componentValues[i], i, "vec.insert");
        }
        
        return vec;
    }
    
    // For vec9: Use struct with 9 components
    if (dimension == 9) {
        std::vector<llvm::Type*> componentTypes(9, componentType);
        llvm::StructType* vec9Type = llvm::StructType::get(context, componentTypes);
        
        // Start with undef struct
        llvm::Value* vec = llvm::UndefValue::get(vec9Type);
        
        // Insert each component
        for (int i = 0; i < 9; ++i) {
            vec = builder.CreateInsertValue(vec, componentValues[i], i, "vec9.insert");
        }
        
        return vec;
    }
    
    throw std::runtime_error("Unsupported vector dimension: " + std::to_string(dimension));
}

// ============================================================================
// Cast Expression Code Generation (Zero Implicit Conversion)
// ============================================================================

/**
 * Generate code for cast expressions (@cast and @cast_unchecked)
 * Handles: safe widening, checked narrowing, unchecked truncation
 */
llvm::Value* ExprCodegen::codegenCast(CastExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null cast expression");
    }
    
    // Generate code for the expression being cast
    llvm::Value* sourceValue = codegenExpressionNode(expr->expression.get(), this);
    if (!sourceValue) {
        throw std::runtime_error("Failed to generate code for cast source expression");
    }
    
    // Get LLVM types
    llvm::Type* sourceLLVMType = sourceValue->getType();
    llvm::Type* targetLLVMType = getLLVMTypeFromString(expr->targetType);
    
    if (!targetLLVMType) {
        throw std::runtime_error("Unknown target type in cast: " + expr->targetType);
    }
    
    // If types are already the same, no cast needed
    if (sourceLLVMType == targetLLVMType) {
        return sourceValue;
    }
    
    // Determine cast operation based on type categories
    bool sourceIsInt = sourceLLVMType->isIntegerTy();
    bool targetIsInt = targetLLVMType->isIntegerTy();
    bool sourceIsFloat = sourceLLVMType->isFloatingPointTy();
    bool targetIsFloat = targetLLVMType->isFloatingPointTy();
    
    // Integer to Integer casts
    if (sourceIsInt && targetIsInt) {
        unsigned sourceBits = sourceLLVMType->getIntegerBitWidth();
        unsigned targetBits = targetLLVMType->getIntegerBitWidth();
        
        if (targetBits > sourceBits) {
            // Widening: safe cast - use sign/zero extension
            // Determine if source type is signed based on Aria type name
            bool isSigned = (expr->targetType.find("int") == 0 || 
                           expr->targetType.find("i8") == 0 ||
                           expr->targetType.find("i16") == 0 ||
                           expr->targetType.find("i32") == 0 ||
                           expr->targetType.find("i64") == 0);
            
            if (isSigned) {
                return builder.CreateSExt(sourceValue, targetLLVMType, "cast.sext");
            } else {
                return builder.CreateZExt(sourceValue, targetLLVMType, "cast.zext");
            }
        } else {
            // Narrowing: potentially unsafe
            if (expr->isUnchecked) {
                // @cast_unchecked: just truncate
                return builder.CreateTrunc(sourceValue, targetLLVMType, "cast.trunc");
            } else {
                // @cast: checked narrowing — truncate then round-trip back to detect overflow
                llvm::Value* truncated = builder.CreateTrunc(sourceValue, targetLLVMType, "cast.checked_trunc");
                
                // Determine signedness of target type
                bool targetSigned = (expr->targetType.find("int") == 0 ||
                                    expr->targetType.find("i8") == 0 ||
                                    expr->targetType.find("i16") == 0 ||
                                    expr->targetType.find("i32") == 0 ||
                                    expr->targetType.find("i64") == 0);
                
                // Round-trip: extend back to source width
                llvm::Value* roundTrip;
                if (targetSigned) {
                    roundTrip = builder.CreateSExt(truncated, sourceLLVMType, "cast.rt_sext");
                } else {
                    roundTrip = builder.CreateZExt(truncated, sourceLLVMType, "cast.rt_zext");
                }
                
                // Compare: if round-trip != original, overflow occurred
                llvm::Value* overflowed = builder.CreateICmpNE(sourceValue, roundTrip, "cast.overflow");
                
                llvm::Function* func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(context, "cast.overflow_panic", func);
                llvm::BasicBlock* okBB = llvm::BasicBlock::Create(context, "cast.ok", func);
                
                builder.CreateCondBr(overflowed, overflowBB, okBB);
                
                // Overflow path: panic
                builder.SetInsertPoint(overflowBB);
                llvm::Function* panicFn = module->getFunction("aria_panic_overflow");
                if (!panicFn) {
                    llvm::FunctionType* panicType = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                        false
                    );
                    panicFn = llvm::Function::Create(
                        panicType, llvm::Function::ExternalLinkage,
                        "aria_panic_overflow", module
                    );
                }
                llvm::Value* panicMsg = builder.CreateGlobalString(
                    "Integer overflow in checked cast to " + expr->targetType,
                    "cast_overflow_msg"
                );
                builder.CreateCall(panicFn, { panicMsg });
                builder.CreateUnreachable();
                
                // OK path: use truncated value
                builder.SetInsertPoint(okBB);
                return truncated;
            }
        }
    }
    
    // Float to Float casts
    if (sourceIsFloat && targetIsFloat) {
        unsigned sourceBits = sourceLLVMType->getPrimitiveSizeInBits();
        unsigned targetBits = targetLLVMType->getPrimitiveSizeInBits();
        
        if (targetBits > sourceBits) {
            // Widening: f32 -> f64 (safe)
            return builder.CreateFPExt(sourceValue, targetLLVMType, "cast.fpext");
        } else {
            // Narrowing: f64 -> f32
            if (expr->isUnchecked) {
                // @cast_unchecked: just truncate
                return builder.CreateFPTrunc(sourceValue, targetLLVMType, "cast.fptrunc");
            } else {
                // @cast: checked narrowing — truncate and check for infinity overflow
                llvm::Value* truncated = builder.CreateFPTrunc(sourceValue, targetLLVMType, "cast.checked_fptrunc");
                
                // Check if result became infinity when source wasn't
                llvm::Value* srcIsInf = builder.CreateFCmpOEQ(
                    sourceValue,
                    llvm::ConstantFP::getInfinity(sourceLLVMType),
                    "cast.src_is_posinf"
                );
                llvm::Value* srcIsNegInf = builder.CreateFCmpOEQ(
                    sourceValue,
                    llvm::ConstantFP::getInfinity(sourceLLVMType, true),
                    "cast.src_is_neginf"
                );
                llvm::Value* srcWasInf = builder.CreateOr(srcIsInf, srcIsNegInf, "cast.src_was_inf");
                
                llvm::Value* dstIsInf = builder.CreateFCmpOEQ(
                    truncated,
                    llvm::ConstantFP::getInfinity(targetLLVMType),
                    "cast.dst_is_posinf"
                );
                llvm::Value* dstIsNegInf = builder.CreateFCmpOEQ(
                    truncated,
                    llvm::ConstantFP::getInfinity(targetLLVMType, true),
                    "cast.dst_is_neginf"
                );
                llvm::Value* dstBecameInf = builder.CreateOr(dstIsInf, dstIsNegInf, "cast.dst_became_inf");
                
                // Overflow if dst is inf but src was not inf
                llvm::Value* srcNotInf = builder.CreateNot(srcWasInf, "cast.src_not_inf");
                llvm::Value* overflowed = builder.CreateAnd(dstBecameInf, srcNotInf, "cast.fp_overflow");
                
                llvm::Function* func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* overflowBB = llvm::BasicBlock::Create(context, "cast.fp_overflow_panic", func);
                llvm::BasicBlock* okBB = llvm::BasicBlock::Create(context, "cast.fp_ok", func);
                
                builder.CreateCondBr(overflowed, overflowBB, okBB);
                
                // Overflow path: panic
                builder.SetInsertPoint(overflowBB);
                llvm::Function* panicFn = module->getFunction("aria_panic_overflow");
                if (!panicFn) {
                    llvm::FunctionType* panicType = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                        false
                    );
                    panicFn = llvm::Function::Create(
                        panicType, llvm::Function::ExternalLinkage,
                        "aria_panic_overflow", module
                    );
                }
                llvm::Value* panicMsg = builder.CreateGlobalString(
                    "Float overflow in checked cast to " + expr->targetType,
                    "cast_fp_overflow_msg"
                );
                builder.CreateCall(panicFn, { panicMsg });
                builder.CreateUnreachable();
                
                // OK path: use truncated value
                builder.SetInsertPoint(okBB);
                return truncated;
            }
        }
    }
    
    // Integer to Float
    if (sourceIsInt && targetIsFloat) {
        // Determine if source is signed
        bool isSigned = true; // Default to signed
        // TODO: Get signedness from type system
        
        if (isSigned) {
            return builder.CreateSIToFP(sourceValue, targetLLVMType, "cast.sitofp");
        } else {
            return builder.CreateUIToFP(sourceValue, targetLLVMType, "cast.uitofp");
        }
    }
    
    // Float to Integer
    if (sourceIsFloat && targetIsInt) {
        // Determine if target is signed
        bool isSigned = (expr->targetType.find("int") == 0 || 
                       expr->targetType[0] == 'i');
        
        if (expr->isUnchecked) {
            // @cast_unchecked: direct conversion (may overflow)
            if (isSigned) {
                return builder.CreateFPToSI(sourceValue, targetLLVMType, "cast.fptosi");
            } else {
                return builder.CreateFPToUI(sourceValue, targetLLVMType, "cast.fptoui");
            }
        } else {
            // @cast: checked conversion
            // TODO: Add range check (ensure float value fits in target int range)
            if (isSigned) {
                return builder.CreateFPToSI(sourceValue, targetLLVMType, "cast.checked_fptosi");
            } else {
                return builder.CreateFPToUI(sourceValue, targetLLVMType, "cast.checked_fptoui");
            }
        }
    }
    
    // Pointer to Pointer casts (void* ↔ wild T->)
    // In LLVM opaque pointer model (20.x+), all pointers are compatible "ptr" type
    // Just return the source value - no bitcast needed with opaque pointers
    if (sourceLLVMType->isPointerTy() && targetLLVMType->isPointerTy()) {
        return sourceValue;  // No-op: ptr to ptr is identity in modern LLVM
    }
    
    throw std::runtime_error("Unsupported cast between types: " + 
                           std::string(sourceLLVMType->isIntegerTy() ? "int" : "float") +
                           " -> " + expr->targetType);
}

/**
 * Generate code for object literal expressions
 * Handles: {val1, val2, ...} syntax for struct-like initialization
 * Used for vec9, frac types, and other exotic composite types
 */
llvm::Value* ExprCodegen::codegenObjectLiteral(ObjectLiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null object literal expression");
    }
    
    ARIA_DBG_STREAM << "[DEBUG] codegenObjectLiteral: fields=" << expr->fields.size() 
              << ", type_name=" << expr->type_name << std::endl;
    
    // For vec9 and other array-like types: create an array/struct with field values
    if (expr->fields.size() == 9) {
        // vec9: [9 x i64]
        llvm::ArrayType* arrayType = llvm::ArrayType::get(builder.getInt64Ty(), 9);
        llvm::Value* arrayAlloca = builder.CreateAlloca(arrayType, nullptr, "vec9.alloca");
        
        // Initialize each element
        for (size_t i = 0; i < expr->fields.size(); i++) {
            llvm::Value* elemValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!elemValue) {
                throw std::runtime_error("Failed to generate code for vec9 element " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (elemValue->getType() != builder.getInt64Ty()) {
                elemValue = builder.CreateSExtOrTrunc(elemValue, builder.getInt64Ty(), "elem.ext");
            }
            
            // Get pointer to array element
            std::vector<llvm::Value*> indices = {
                llvm::ConstantInt::get(builder.getInt32Ty(), 0),
                llvm::ConstantInt::get(builder.getInt32Ty(), i)
            };
            llvm::Value* elemPtr = builder.CreateGEP(arrayType, arrayAlloca, indices, "elem.ptr");
            builder.CreateStore(elemValue, elemPtr);
        }
        
        // Load the complete array value
        return builder.CreateLoad(arrayType, arrayAlloca, "vec9.value");
    }
    
    // For frac types (3 fields): {whole, num, denom}
    if (expr->fields.size() == 3) {
        // Create struct with 3 i64 fields
        llvm::StructType* structType = llvm::StructType::get(
            context,
            {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()},
            false
        );
        
        llvm::Value* structAlloca = builder.CreateAlloca(structType, nullptr, "frac.alloca");
        
        // Initialize each field
        for (size_t i = 0; i < 3; i++) {
            llvm::Value* fieldValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!fieldValue) {
                throw std::runtime_error("Failed to generate code for frac field " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (fieldValue->getType() != builder.getInt64Ty()) {
                fieldValue = builder.CreateSExtOrTrunc(fieldValue, builder.getInt64Ty(), "field.ext");
            }
            
            llvm::Value* fieldPtr = builder.CreateStructGEP(structType, structAlloca, i, "field.ptr");
            builder.CreateStore(fieldValue, fieldPtr);
        }
        
        // Load the complete struct value
        return builder.CreateLoad(structType, structAlloca, "frac.value");
    }
    
    // For tfp types (2 fields): {exponent, mantissa}
    if (expr->fields.size() == 2) {
        // Create struct with 2 i64 fields
        llvm::StructType* structType = llvm::StructType::get(
            context,
            {builder.getInt64Ty(), builder.getInt64Ty()},
            false
        );
        
        llvm::Value* structAlloca = builder.CreateAlloca(structType, nullptr, "tfp.alloca");
        
        // Initialize each field
        for (size_t i = 0; i < 2; i++) {
            llvm::Value* fieldValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!fieldValue) {
                throw std::runtime_error("Failed to generate code for tfp field " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (fieldValue->getType() != builder.getInt64Ty()) {
                fieldValue = builder.CreateSExtOrTrunc(fieldValue, builder.getInt64Ty(), "field.ext");
            }
            
            llvm::Value* fieldPtr = builder.CreateStructGEP(structType, structAlloca, i, "field.ptr");
            builder.CreateStore(fieldValue, fieldPtr);
        }
        
        // Load the complete struct value
        return builder.CreateLoad(structType, structAlloca, "tfp.value");
    }
    
    throw std::runtime_error("Unsupported object literal field count: " + 
                           std::to_string(expr->fields.size()));
}

// ============================================================================
// GPU/PTX Backend - Phase 4: GPU Intrinsics
// ============================================================================

/**
 * Generate code for GPU intrinsics
 * 
 * Maps Aria gpu.* calls to LLVM NVVM intrinsics for CUDA code generation.
 * These functions can only be used inside GPU kernels (marked with #[gpu_kernel]).
 * 
 * Supported intrinsics:
 * - gpu.thread_id() -> llvm.nvvm.read.ptx.sreg.tid.x (thread index in block)
 * - gpu.thread_id_y() -> llvm.nvvm.read.ptx.sreg.tid.y
 * - gpu.thread_id_z() -> llvm.nvvm.read.ptx.sreg.tid.z
 * - gpu.block_id() -> llvm.nvvm.read.ptx.sreg.ctaid.x (block index in grid)
 * - gpu.block_id_y() -> llvm.nvvm.read.ptx.sreg.ctaid.y
 * - gpu.block_id_z() -> llvm.nvvm.read.ptx.sreg.ctaid.z
 * - gpu.block_dim() -> llvm.nvvm.read.ptx.sreg.ntid.x (threads per block)
 * - gpu.block_dim_y() -> llvm.nvvm.read.ptx.sreg.ntid.y
 * - gpu.block_dim_z() -> llvm.nvvm.read.ptx.sreg.ntid.z
 * - gpu.grid_dim() -> llvm.nvvm.read.ptx.sreg.nctaid.x (blocks in grid)
 * - gpu.grid_dim_y() -> llvm.nvvm.read.ptx.sreg.nctaid.y
 * - gpu.grid_dim_z() -> llvm.nvvm.read.ptx.sreg.nctaid.z
 * - gpu.sync_threads() -> llvm.nvvm.barrier0 (block-level synchronization)
 * 
 * Example Aria code:
 * ```aria
 * #[gpu_kernel]
 * func:vector_add = void(int32:n) {
 *     int32:tid = gpu.thread_id();
 *     int32:bid = gpu.block_id();
 *     int32:idx = bid * gpu.block_dim() + tid;
 *     
 *     if (idx < n) {
 *         // Process element idx
 *     }
 *     
 *     gpu.sync_threads();  // Wait for all threads
 * }
 * ```
 * 
 * Generated LLVM IR:
 * ```llvm
 * %tid = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
 * %bid = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
 * call void @llvm.nvvm.barrier0()
 * ```
 */
llvm::Value* ExprCodegen::codegenGPUIntrinsic(const std::string& intrinsic_name,
                                               CallExpr* call_expr) {
    std::cerr << "[GPU] Generating intrinsic: gpu." << intrinsic_name << std::endl;
    
    // Helper lambda to get or declare NVVM intrinsic
    auto getOrDeclareNVVMIntrinsic = [this](const std::string& name, llvm::Type* returnType) -> llvm::Function* {
        if (llvm::Function* existing = module->getFunction(name)) {
            return existing;
        }
        llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, {}, false);
        return llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, module);
    };
    
    llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
    llvm::Type* voidType = llvm::Type::getVoidTy(context);
    
    // Thread indexing intrinsics (X dimension - most common)
    if (intrinsic_name == "thread_id") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.x");
    }
    
    if (intrinsic_name == "thread_id_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.y");
    }
    
    if (intrinsic_name == "thread_id_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.z");
    }
    
    // Block indexing intrinsics
    if (intrinsic_name == "block_id") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.x");
    }
    
    if (intrinsic_name == "block_id_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.y");
    }
    
    if (intrinsic_name == "block_id_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.z");
    }
    
    // Block dimension intrinsics
    if (intrinsic_name == "block_dim") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.x");
    }
    
    if (intrinsic_name == "block_dim_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.y");
    }
    
    if (intrinsic_name == "block_dim_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.z");
    }
    
    // Grid dimension intrinsics
    if (intrinsic_name == "grid_dim") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.x");
    }
    
    if (intrinsic_name == "grid_dim_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.y");
    }
    
    if (intrinsic_name == "grid_dim_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.z");
    }
    
    // Synchronization intrinsics
    if (intrinsic_name == "sync_threads") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.sync_threads() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.barrier0", voidType);
        builder.CreateCall(intrinsic);
        // Barrier intrinsic returns void, return nullptr for void expressions
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // Unknown GPU intrinsic
    throw std::runtime_error("Unknown GPU intrinsic: gpu." + intrinsic_name +
                           "\nSupported: thread_id, thread_id_y, thread_id_z, " +
                           "block_id, block_id_y, block_id_z, " +
                           "block_dim, block_dim_y, block_dim_z, " +
                           "grid_dim, grid_dim_y, grid_dim_z, " +
                           "sync_threads");
}


