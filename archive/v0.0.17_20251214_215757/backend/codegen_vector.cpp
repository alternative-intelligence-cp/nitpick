/**
 * src/backend/codegen_vector.cpp
 * 
 * Aria Vector CodeGen - Implementation
 * LLVM IR generation with SIMD optimization
 */

#include "codegen_vector.h"
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IntrinsicsX86.h>
#include <llvm/IR/Constants.h>

namespace aria {
namespace backend {

void VectorLowerer::initializeVectorTypes() {
    // Float vectors (32-bit)
    vec2_type = llvm::FixedVectorType::get(llvm::Type::getFloatTy(context), 2);
    vec3_type = llvm::FixedVectorType::get(llvm::Type::getFloatTy(context), 3);
    vec4_type = llvm::FixedVectorType::get(llvm::Type::getFloatTy(context), 4);
    
    // Double vectors (64-bit)
    dvec2_type = llvm::FixedVectorType::get(llvm::Type::getDoubleTy(context), 2);
    dvec3_type = llvm::FixedVectorType::get(llvm::Type::getDoubleTy(context), 3);
    dvec4_type = llvm::FixedVectorType::get(llvm::Type::getDoubleTy(context), 4);
    
    // Integer vectors (32-bit)
    ivec2_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(context), 2);
    ivec3_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(context), 3);
    ivec4_type = llvm::FixedVectorType::get(llvm::Type::getInt32Ty(context), 4);
}

void VectorLowerer::declareRuntimeFunctions() {
    // Vec3 cross product (complex operation, use runtime)
    llvm::FunctionType* cross_ty = llvm::FunctionType::get(
        vec3_type,
        {vec3_type, vec3_type},
        false
    );
    vec3_cross_fn = llvm::Function::Create(
        cross_ty,
        llvm::Function::ExternalLinkage,
        "_aria_vec3_cross",
        module
    );
    
    // Normalize functions (require sqrt, use runtime for simplicity)
    llvm::FunctionType* norm2_ty = llvm::FunctionType::get(
        vec2_type, {vec2_type}, false
    );
    vec2_normalize_fn = llvm::Function::Create(
        norm2_ty,
        llvm::Function::ExternalLinkage,
        "_aria_vec2_normalize",
        module
    );
    
    llvm::FunctionType* norm3_ty = llvm::FunctionType::get(
        vec3_type, {vec3_type}, false
    );
    vec3_normalize_fn = llvm::Function::Create(
        norm3_ty,
        llvm::Function::ExternalLinkage,
        "_aria_vec3_normalize",
        module
    );
    
    llvm::FunctionType* norm4_ty = llvm::FunctionType::get(
        vec4_type, {vec4_type}, false
    );
    vec4_normalize_fn = llvm::Function::Create(
        norm4_ty,
        llvm::Function::ExternalLinkage,
        "_aria_vec4_normalize",
        module
    );
}

llvm::Value* VectorLowerer::createVectorLiteral(
    llvm::FixedVectorType* vec_type,
    const std::vector<llvm::Value*>& components
) {
    llvm::Value* result = llvm::UndefValue::get(vec_type);
    
    for (unsigned i = 0; i < components.size(); ++i) {
        result = builder.CreateInsertElement(result, components[i], i);
    }
    
    return result;
}

llvm::Value* VectorLowerer::createVectorSplat(llvm::FixedVectorType* vec_type, llvm::Value* scalar) {
    unsigned size = vec_type->getNumElements();
    llvm::Value* result = llvm::UndefValue::get(vec_type);
    
    for (unsigned i = 0; i < size; ++i) {
        result = builder.CreateInsertElement(result, scalar, i);
    }
    
    return result;
}

// ========== Arithmetic Operations (SIMD-accelerated) ==========

llvm::Value* VectorLowerer::createVectorAdd(llvm::Value* lhs, llvm::Value* rhs) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(lhs->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    if (elem_type->isFloatingPointTy()) {
        return builder.CreateFAdd(lhs, rhs, "vec.add");
    } else {
        return builder.CreateAdd(lhs, rhs, "ivec.add");
    }
}

llvm::Value* VectorLowerer::createVectorSub(llvm::Value* lhs, llvm::Value* rhs) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(lhs->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    if (elem_type->isFloatingPointTy()) {
        return builder.CreateFSub(lhs, rhs, "vec.sub");
    } else {
        return builder.CreateSub(lhs, rhs, "ivec.sub");
    }
}

llvm::Value* VectorLowerer::createVectorMul(llvm::Value* lhs, llvm::Value* rhs) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(lhs->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    if (elem_type->isFloatingPointTy()) {
        return builder.CreateFMul(lhs, rhs, "vec.mul");
    } else {
        return builder.CreateMul(lhs, rhs, "ivec.mul");
    }
}

llvm::Value* VectorLowerer::createVectorDiv(llvm::Value* lhs, llvm::Value* rhs) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(lhs->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    if (elem_type->isFloatingPointTy()) {
        return builder.CreateFDiv(lhs, rhs, "vec.div");
    } else {
        return builder.CreateSDiv(lhs, rhs, "ivec.div");
    }
}

llvm::Value* VectorLowerer::createVectorScale(llvm::Value* vec, llvm::Value* scalar) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(vec->getType());
    llvm::Value* scalar_vec = createVectorSplat(vec_type, scalar);
    return createVectorMul(vec, scalar_vec);
}

llvm::Value* VectorLowerer::createVectorNegate(llvm::Value* vec) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(vec->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    if (elem_type->isFloatingPointTy()) {
        return builder.CreateFNeg(vec, "vec.neg");
    } else {
        return builder.CreateNeg(vec, "ivec.neg");
    }
}

// ========== Vector Operations ==========

llvm::Value* VectorLowerer::createVectorDot(llvm::Value* lhs, llvm::Value* rhs) {
    // Dot product: multiply component-wise, then horizontal add
    llvm::Value* mul = createVectorMul(lhs, rhs);
    return createHorizontalAdd(mul);
}

llvm::Value* VectorLowerer::createVectorCross(llvm::Value* lhs, llvm::Value* rhs) {
    // Cross product only for vec3
    // Call runtime function for simplicity
    return builder.CreateCall(vec3_cross_fn, {lhs, rhs}, "vec3.cross");
}

llvm::Value* VectorLowerer::createVectorLengthSquared(llvm::Value* vec) {
    // length^2 = dot(v, v)
    return createVectorDot(vec, vec);
}

llvm::Value* VectorLowerer::createVectorLength(llvm::Value* vec) {
    // length = sqrt(dot(v, v))
    llvm::Value* len_sq = createVectorLengthSquared(vec);
    
    // Use LLVM sqrt intrinsic
    llvm::Function* sqrt_fn = llvm::Intrinsic::getOrInsertDeclaration(
        &module,
        llvm::Intrinsic::sqrt,
        {len_sq->getType()}
    );
    
    return builder.CreateCall(sqrt_fn, {len_sq}, "vec.length");
}

llvm::Value* VectorLowerer::createVectorNormalize(llvm::Value* vec) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(vec->getType());
    unsigned size = vec_type->getNumElements();
    
    // Call appropriate runtime normalize function
    if (size == 2) {
        return builder.CreateCall(vec2_normalize_fn, {vec}, "vec2.normalize");
    } else if (size == 3) {
        return builder.CreateCall(vec3_normalize_fn, {vec}, "vec3.normalize");
    } else if (size == 4) {
        return builder.CreateCall(vec4_normalize_fn, {vec}, "vec4.normalize");
    }
    
    // Fallback: inline normalization
    llvm::Value* len = createVectorLength(vec);
    llvm::Value* len_vec = createVectorSplat(vec_type, len);
    return createVectorDiv(vec, len_vec);
}

llvm::Value* VectorLowerer::createVectorDistance(llvm::Value* lhs, llvm::Value* rhs) {
    llvm::Value* diff = createVectorSub(lhs, rhs);
    return createVectorLength(diff);
}

// ========== Component Access ==========

llvm::Value* VectorLowerer::createVectorExtractElement(llvm::Value* vec, unsigned index) {
    return builder.CreateExtractElement(vec, index, "vec.extract");
}

llvm::Value* VectorLowerer::createVectorInsertElement(llvm::Value* vec, llvm::Value* value, unsigned index) {
    return builder.CreateInsertElement(vec, value, index, "vec.insert");
}

// ========== Swizzling ==========

llvm::Value* VectorLowerer::createVectorSwizzle(llvm::Value* vec, const std::vector<unsigned>& indices) {
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(vec->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    // Create result vector type (unused, shufflevector infers type)
    // auto* result_type = llvm::FixedVectorType::get(elem_type, indices.size());
    
    // Use shufflevector for efficient swizzling
    std::vector<int> mask;
    for (unsigned idx : indices) {
        mask.push_back(static_cast<int>(idx));
    }
    
    // Pad mask if needed for shufflevector
    llvm::Value* undef = llvm::UndefValue::get(vec_type);
    return builder.CreateShuffleVector(vec, undef, mask, "vec.swizzle");
}

// ========== Comparison ==========

llvm::Value* VectorLowerer::createVectorEquals(llvm::Value* lhs, llvm::Value* rhs, float epsilon) {
    // Compare component-wise with epsilon tolerance
    llvm::Value* diff = createVectorSub(lhs, rhs);
    
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(lhs->getType());
    auto* elem_type = getVectorElementType(vec_type);
    
    llvm::Value* epsilon_val = llvm::ConstantFP::get(elem_type, epsilon);
    llvm::Value* epsilon_vec = createVectorSplat(vec_type, epsilon_val);
    
    // abs(diff) < epsilon for each component
    llvm::Function* fabs_fn = llvm::Intrinsic::getOrInsertDeclaration(
        &module,
        llvm::Intrinsic::fabs,
        {vec_type}
    );
    
    llvm::Value* abs_diff = builder.CreateCall(fabs_fn, {diff});
    llvm::Value* cmp = builder.CreateFCmpOLT(abs_diff, epsilon_vec, "vec.cmp");
    
    // All components must be true
    return createHorizontalAdd(builder.CreateSExt(cmp, vec_type));
}

// ========== Helpers ==========

llvm::Type* VectorLowerer::getVectorElementType(llvm::FixedVectorType* vec_type) {
    return vec_type->getElementType();
}

unsigned VectorLowerer::getVectorSize(llvm::FixedVectorType* vec_type) {
    return vec_type->getNumElements();
}

llvm::Value* VectorLowerer::createHorizontalAdd(llvm::Value* vec) {
    // Sum all components of the vector
    auto* vec_type = llvm::cast<llvm::FixedVectorType>(vec->getType());
    unsigned size = vec_type->getNumElements();
    auto* elem_type = getVectorElementType(vec_type);
    
    llvm::Value* sum = builder.CreateExtractElement(vec, (uint64_t)0);
    
    for (unsigned i = 1; i < size; ++i) {
        llvm::Value* elem = builder.CreateExtractElement(vec, i);
        if (elem_type->isFloatingPointTy()) {
            sum = builder.CreateFAdd(sum, elem);
        } else {
            sum = builder.CreateAdd(sum, elem);
        }
    }
    
    return sum;
}

} // namespace backend
} // namespace aria
