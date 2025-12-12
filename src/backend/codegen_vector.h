/**
 * src/backend/codegen_vector.h
 * 
 * Aria Vector CodeGen
 * LLVM IR generation for vector types with SIMD optimization
 */

#ifndef ARIA_BACKEND_CODEGEN_VECTOR_H
#define ARIA_BACKEND_CODEGEN_VECTOR_H

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <string>

namespace aria {
namespace backend {

/**
 * VectorLowerer - LLVM IR generation for vector operations
 * 
 * Generates efficient vector code with SIMD optimization where possible.
 * Uses LLVM fixed-size vector types for SIMD acceleration.
 */
class VectorLowerer {
private:
    llvm::LLVMContext& context;
    llvm::Module& module;
    llvm::IRBuilder<>& builder;
    
    // Cached vector types
    llvm::FixedVectorType* vec2_type = nullptr;
    llvm::FixedVectorType* vec3_type = nullptr;
    llvm::FixedVectorType* vec4_type = nullptr;
    llvm::FixedVectorType* dvec2_type = nullptr;
    llvm::FixedVectorType* dvec3_type = nullptr;
    llvm::FixedVectorType* dvec4_type = nullptr;
    llvm::FixedVectorType* ivec2_type = nullptr;
    llvm::FixedVectorType* ivec3_type = nullptr;
    llvm::FixedVectorType* ivec4_type = nullptr;
    
    // Runtime function declarations (for complex operations)
    llvm::Function* vec3_cross_fn = nullptr;
    llvm::Function* vec2_normalize_fn = nullptr;
    llvm::Function* vec3_normalize_fn = nullptr;
    llvm::Function* vec4_normalize_fn = nullptr;
    
    void initializeVectorTypes();
    void declareRuntimeFunctions();
    
public:
    VectorLowerer(llvm::LLVMContext& ctx, llvm::Module& mod, llvm::IRBuilder<>& b)
        : context(ctx), module(mod), builder(b) {
        initializeVectorTypes();
        declareRuntimeFunctions();
    }
    
    // Get LLVM vector types
    llvm::FixedVectorType* getVec2Type() { return vec2_type; }
    llvm::FixedVectorType* getVec3Type() { return vec3_type; }
    llvm::FixedVectorType* getVec4Type() { return vec4_type; }
    llvm::FixedVectorType* getDVec2Type() { return dvec2_type; }
    llvm::FixedVectorType* getDVec3Type() { return dvec3_type; }
    llvm::FixedVectorType* getDVec4Type() { return dvec4_type; }
    llvm::FixedVectorType* getIVec2Type() { return ivec2_type; }
    llvm::FixedVectorType* getIVec3Type() { return ivec3_type; }
    llvm::FixedVectorType* getIVec4Type() { return ivec4_type; }
    
    // Vector construction
    llvm::Value* createVectorLiteral(llvm::FixedVectorType* vec_type, 
                                     const std::vector<llvm::Value*>& components);
    llvm::Value* createVectorSplat(llvm::FixedVectorType* vec_type, llvm::Value* scalar);
    
    // Arithmetic operations (SIMD-accelerated)
    llvm::Value* createVectorAdd(llvm::Value* lhs, llvm::Value* rhs);
    llvm::Value* createVectorSub(llvm::Value* lhs, llvm::Value* rhs);
    llvm::Value* createVectorMul(llvm::Value* lhs, llvm::Value* rhs);  // Component-wise
    llvm::Value* createVectorDiv(llvm::Value* lhs, llvm::Value* rhs);  // Component-wise
    llvm::Value* createVectorScale(llvm::Value* vec, llvm::Value* scalar);
    llvm::Value* createVectorNegate(llvm::Value* vec);
    
    // Vector operations
    llvm::Value* createVectorDot(llvm::Value* lhs, llvm::Value* rhs);
    llvm::Value* createVectorCross(llvm::Value* lhs, llvm::Value* rhs);  // vec3 only
    llvm::Value* createVectorLength(llvm::Value* vec);
    llvm::Value* createVectorLengthSquared(llvm::Value* vec);
    llvm::Value* createVectorNormalize(llvm::Value* vec);
    llvm::Value* createVectorDistance(llvm::Value* lhs, llvm::Value* rhs);
    
    // Component access
    llvm::Value* createVectorExtractElement(llvm::Value* vec, unsigned index);
    llvm::Value* createVectorInsertElement(llvm::Value* vec, llvm::Value* value, unsigned index);
    
    // Swizzling (e.g., vec.xyzw, vec.rgba)
    llvm::Value* createVectorSwizzle(llvm::Value* vec, const std::vector<unsigned>& indices);
    
    // Comparison
    llvm::Value* createVectorEquals(llvm::Value* lhs, llvm::Value* rhs, float epsilon = 1e-6f);
    
private:
    // Helper: Get element type from vector type
    llvm::Type* getVectorElementType(llvm::FixedVectorType* vec_type);
    
    // Helper: Get vector size
    unsigned getVectorSize(llvm::FixedVectorType* vec_type);
    
    // Helper: Create horizontal reduction (sum all components)
    llvm::Value* createHorizontalAdd(llvm::Value* vec);
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_CODEGEN_VECTOR_H
