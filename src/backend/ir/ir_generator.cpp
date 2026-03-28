#include "backend/ir/ir_generator.h"
#include "backend/ir/codegen_expr.h"  // For template literal codegen
#include "frontend/ast/ast_node.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/type.h"  // For SimpleType
#include "frontend/token.h"
#include "frontend/sema/type.h"  // Full type definitions needed
#include "frontend/sema/generic_resolver.h"  // For Specialization
#include "semantic/literal_converter.h"  // Phase 3.2.5: High-precision literals
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DerivedTypes.h>  // For FunctionType, StructType, etc.
#include <llvm/IR/DataLayout.h>     // For getTypeAllocSize
#include <llvm/IR/DebugLoc.h>       // For debug locations
#include <llvm/BinaryFormat/Dwarf.h>  // For DWARF constants
#include <iostream>  // For debug output
#include <sstream>   // For istringstream (generic name mangling)
#include <cassert>

namespace aria {
using namespace sema;  // Now inside aria namespace

IRGenerator::IRGenerator(const std::string& module_name, bool enable_debug)
    : context(), 
      module(std::make_unique<llvm::Module>(module_name.empty() ? "aria_module" : module_name, context)),
      builder(context),
      type_system(nullptr),
      tbb_codegen(context, builder),
      ternary_codegen(context, builder),
      di_builder(nullptr),
      di_compile_unit(nullptr),
      di_file(nullptr),
      debug_enabled(enable_debug),
      current_module_name(""),
      current_func_is_async(false),
      current_coro_suspend_block(nullptr),
      current_async_promise(nullptr),
      current_async_result_type(nullptr),
      current_func_decl(nullptr) {
    // Set source filename for better debug info
    module->setSourceFileName(module_name.empty() ? "aria_module" : module_name);

    // Set module reference for TernaryCodegen to declare runtime intrinsics
    // (ARIA-013: Balanced Ternary and Nonary Intrinsics)
    ternary_codegen.setModule(module.get());

    if (debug_enabled) {
        // Create DIBuilder (initialization deferred to initDebugInfo)
        di_builder = std::make_unique<llvm::DIBuilder>(*module);
    }
}

void IRGenerator::setTypeSystem(TypeSystem* ts) {
    type_system = ts;
}

// Helper: Promote int64 literal to LBIM struct (int128/256/512/1024/2048/4096)
llvm::Value* IRGenerator::promoteToLBIMStruct(llvm::Value* literal, llvm::Type* targetType) {
    // Verify target is a struct type
    if (!targetType->isStructTy()) {
        return literal;  // Not an LBIM type, no promotion needed
    }
    
    llvm::StructType* structType = llvm::cast<llvm::StructType>(targetType);
    std::string structName = structType->getName().str();
    
    // Check if this is an LBIM struct (int128, uint128, flt256, flt512, etc.)
    bool isIntLBIM = (structName.find("struct.int") == 0 || structName.find("struct.uint") == 0);
    bool isFltLBIM = (structName == "struct.flt256" || structName == "struct.flt512");
    if (!isIntLBIM && !isFltLBIM) {
        return literal;  // Not an LBIM type
    }
    
    // For float LBIM types, convert float literal to its bitwise i64 representation
    if (isFltLBIM && literal->getType()->isFloatingPointTy()) {
        // Bitcast the float to an integer of same width, then extend to i64
        llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
        unsigned floatBits = literal->getType()->getScalarSizeInBits();
        llvm::Type* intEquivType = llvm::Type::getIntNTy(context, floatBits);
        llvm::Value* asInt = builder.CreateBitCast(literal, intEquivType, "flt_to_bits");
        if (floatBits < 64) {
            asInt = builder.CreateZExt(asInt, i64Type, "flt_bits_i64");
        }
        // Now treat as integer promotion: store bits in limb 0, zero the rest
        literal = asInt;
    }
    
    // Extract the literal value as int64
    if (!literal->getType()->isIntegerTy()) {
        std::cerr << "[ERROR] promoteToLBIMStruct: literal is not an integer type" << std::endl;
        return literal;
    }
    
    // Determine the number of limbs based on struct name
    int numLimbs = 0;
    if (structName.find("128") != std::string::npos) numLimbs = 2;
    else if (structName.find("256") != std::string::npos) numLimbs = 4;
    else if (structName.find("512") != std::string::npos) numLimbs = 8;
    else if (structName.find("1024") != std::string::npos) numLimbs = 16;
    else if (structName.find("2048") != std::string::npos) numLimbs = 32;
    else if (structName.find("4096") != std::string::npos) numLimbs = 64;
    else {
        std::cerr << "[ERROR] promoteToLBIMStruct: unknown LBIM type " << structName << std::endl;
        return literal;
    }
    
    std::cerr << "[LBIM_PROMOTE] Promoting literal to " << structName << " (" << numLimbs << " limbs)" << std::endl;
    
    // Create a zero-initialized struct on the stack
    llvm::Value* structAlloca = builder.CreateAlloca(targetType, nullptr, "lbim_promoted");
    
    // Initialize all limbs to 0
    llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
    llvm::Value* zero = llvm::ConstantInt::get(i64Type, 0);
    
    for (int i = 0; i < numLimbs; i++) {
        // Get pointer to limbs[i]: GEP into struct, then into array, then index i
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // struct index
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // array field index
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)   // limb index
        };
        llvm::Value* limbPtr = builder.CreateGEP(targetType, structAlloca, indices, "limb_ptr");
        builder.CreateStore(zero, limbPtr);
    }
    
    // For inputs wider than i64 (e.g. i128, i256), extract each 64-bit limb via shift+trunc.
    // This correctly handles large values that don't fit in a single i64.
    if (literal->getType()->getIntegerBitWidth() > 64) {
        unsigned inBits = literal->getType()->getIntegerBitWidth();
        for (unsigned i = 0; i < (unsigned)numLimbs; ++i) {
            llvm::Value* limbVal;
            if (i * 64 < inBits) {
                if (i == 0) {
                    limbVal = builder.CreateTrunc(literal, i64Type, "wide_lo");
                } else {
                    llvm::Value* shifted = builder.CreateLShr(
                        literal,
                        llvm::ConstantInt::get(literal->getType(), (uint64_t)i * 64),
                        "wide_shift");
                    limbVal = builder.CreateTrunc(shifted, i64Type, "wide_limb");
                }
            } else {
                limbVal = llvm::ConstantInt::get(i64Type, 0);
            }
            std::vector<llvm::Value*> wIndices = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
            };
            llvm::Value* wLimbPtr = builder.CreateGEP(targetType, structAlloca, wIndices, "wide_limb_ptr");
            builder.CreateStore(limbVal, wLimbPtr);
        }
        llvm::Value* result = builder.CreateLoad(targetType, structAlloca, "lbim_promoted_val");
        return result;
    }

    // Convert literal to i64 (sign-extend if necessary)
    llvm::Value* literalI64 = literal;
    if (literal->getType() != i64Type) {
        if (literal->getType()->getIntegerBitWidth() < 64) {
            // Sign-extend for signed types
            literalI64 = builder.CreateSExt(literal, i64Type, "literal_i64");
        } else {
            // Truncate if somehow larger
            literalI64 = builder.CreateTrunc(literal, i64Type, "literal_i64");
        }
    }
    
    // Store the literal value in limbs[0] (LSB)
    std::vector<llvm::Value*> indices0 = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // struct index
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // array field index  
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)   // limb 0
    };
    llvm::Value* limb0Ptr = builder.CreateGEP(targetType, structAlloca, indices0, "limb0_ptr");
    builder.CreateStore(literalI64, limb0Ptr);
    
    // If the literal is negative (signed), sign-extend by filling high limbs with all 1s
    if (structName.find("uint") != std::string::npos || isFltLBIM) {
        // Unsigned type or float LBIM - already zero-extended, done
        llvm::Value* result = builder.CreateLoad(targetType, structAlloca, "lbim_promoted_val");
        return result;
    }
    
    // Signed type - check if negative and sign-extend
    llvm::Value* isNegative = builder.CreateICmpSLT(
        literalI64,
        llvm::ConstantInt::get(i64Type, 0),
        "is_negative"
    );
    
    std::cerr << "[LBIM_PROMOTE] Literal is signed type, checking if negative..." << std::endl;
    if (llvm::ConstantInt* CI = llvm::dyn_cast<llvm::ConstantInt>(literalI64)) {
        std::cerr << "[LBIM_PROMOTE] Literal constant value: " << CI->getSExtValue() << std::endl;
    }
    
    // Create blocks for sign extension
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* signExtendBB = llvm::BasicBlock::Create(context, "sign_extend", func);
    llvm::BasicBlock* continueBB = llvm::BasicBlock::Create(context, "continue", func);
    
    builder.CreateCondBr(isNegative, signExtendBB, continueBB);
    
    // Sign extend block: fill high limbs with 0xFFFFFFFFFFFFFFFF
    builder.SetInsertPoint(signExtendBB);
    llvm::Value* allOnes = llvm::ConstantInt::get(i64Type, 0xFFFFFFFFFFFFFFFFULL);
    for (int i = 1; i < numLimbs; i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
        };
        llvm::Value* limbPtr = builder.CreateGEP(targetType, structAlloca, indices, "limb_ptr");
        builder.CreateStore(allOnes, limbPtr);
    }
    builder.CreateBr(continueBB);
    
    // Continue block: load and return the struct
    builder.SetInsertPoint(continueBB);
    llvm::Value* result = builder.CreateLoad(targetType, structAlloca, "lbim_promoted_val");
    return result;
}

void IRGenerator::setCurrentModuleName(const std::string& module_name) {
    current_module_name = module_name;
}

llvm::Type* IRGenerator::mapType(Type* aria_type) {
    if (!aria_type) {
        return builder.getVoidTy();
    }
    
    // Check cache first
    std::string type_name = aria_type->toString();
    auto it = type_map.find(type_name);
    if (it != type_map.end()) {
        return it->second;
    }
    
    llvm::Type* llvm_type = nullptr;
    
    // Map based on type kind
    switch (aria_type->getKind()) {
        case TypeKind::PRIMITIVE: {
            auto* prim = static_cast<PrimitiveType*>(aria_type);
            std::string prim_name = prim->getName();
            
            // NIL type: Unit type (empty struct) for "no value"
            if (prim_name == "NIL") {
                llvm::StructType* nilType = llvm::StructType::getTypeByName(context, "struct.NIL");
                if (!nilType) {
                    nilType = llvm::StructType::create(context, {}, "struct.NIL");
                }
                llvm_type = nilType;
            }
            // Boolean type
            else if (prim_name == "bool") {
                llvm_type = builder.getInt1Ty();
            }
            // Fraction types: {whole, numerator, denominator} (all same TBB width)
            else if (prim_name == "frac8") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt8Ty(),  // whole (tbb8)
                    builder.getInt8Ty(),  // numerator (tbb8)
                    builder.getInt8Ty()   // denominator (tbb8)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            else if (prim_name == "frac16") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt16Ty(),  // whole (tbb16)
                    builder.getInt16Ty(),  // numerator (tbb16)
                    builder.getInt16Ty()   // denominator (tbb16)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            else if (prim_name == "frac32") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt32Ty(),  // whole (tbb32)
                    builder.getInt32Ty(),  // numerator (tbb32)
                    builder.getInt32Ty()   // denominator (tbb32)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            else if (prim_name == "frac64") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt64Ty(),  // whole (tbb64)
                    builder.getInt64Ty(),  // numerator (tbb64)
                    builder.getInt64Ty()   // denominator (tbb64)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            // Twisted Floating Point types: {tbb16 exp, tbbN mant}
            else if (prim_name == "tfp32") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt16Ty(),  // exponent (tbb16)
                    builder.getInt16Ty()   // mantissa (tbb16)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            else if (prim_name == "tfp64") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt16Ty(),  // exponent (tbb16)
                    builder.getInt64Ty()   // mantissa (tbb48 in i64, upper 16 bits unused)
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            // Vec9: 16-element toroidal vector (16 x tbb32 = 512 bits)
            else if (prim_name == "vec9") {
                llvm_type = llvm::ArrayType::get(builder.getInt32Ty(), 16);
            }
            // TMatrix: Composite structure with metadata + wild pointer
            else if (prim_name == "tmatrix") {
                std::vector<llvm::Type*> fields = {
                    builder.getInt64Ty(),                // width
                    builder.getInt64Ty(),                // height
                    builder.getInt32Ty(),                // element_size
                    builder.getInt32Ty(),                // padding/alignment
                    llvm::PointerType::get(context, 0)   // wild data pointer
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            // TTensor: 9D toroidal tensor with metadata + wild pointer
            else if (prim_name == "ttensor") {
                std::vector<llvm::Type*> fields = {
                    llvm::ArrayType::get(builder.getInt64Ty(), 9),  // dimensions[9]
                    builder.getInt32Ty(),                // element_size
                    builder.getInt32Ty(),                // padding/alignment
                    llvm::PointerType::get(context, 0)   // wild data pointer
                };
                llvm_type = llvm::StructType::get(context, fields);
            }
            // Floating point types
            else if (prim->isFloatingType()) {
                switch (prim->getBitWidth()) {
                    case 32:  llvm_type = builder.getFloatTy(); break;
                    case 64:  llvm_type = builder.getDoubleTy(); break;
                    case 128: llvm_type = llvm::Type::getFP128Ty(context); break;
                    default:
                        // For 256/512-bit floats, use integer for now
                        llvm_type = builder.getIntNTy(prim->getBitWidth());
                        break;
                }
            }
            // String type - stored as pointer to AriaString struct {char* data, int64 length}
            // Consistent with mapTypeFromName("string") which returns ptr to struct.AriaString.
            // Without this case, string falls to getIntNTy(0) = i0 (bitWidth == 0 for non-numeric),
            // which corrupts array-of-string field types (e.g. string[8] → [8 x i0] instead of [8 x ptr]).
            else if (prim_name == "string") {
                llvm_type = llvm::PointerType::get(context, 0);
            }
            // Integer and TBB types (TBB uses same representation as int)
            else {
                llvm_type = builder.getIntNTy(prim->getBitWidth());
            }
            break;
        }
        
        case TypeKind::POINTER: {
            auto* ptr_type = static_cast<sema::PointerType*>(aria_type);
            llvm::Type* pointee = mapType(ptr_type->getPointeeType());
            (void)pointee;  // LLVM opaque pointers don't use pointee type directly
            // LLVM uses opaque pointers in newer versions
            llvm_type = llvm::PointerType::get(context, 0);
            break;
        }
        
        case TypeKind::ARRAY: {
            auto* arr_type = static_cast<sema::ArrayType*>(aria_type);
            llvm::Type* elem_type = mapType(arr_type->getElementType());
            if (arr_type->getSize() > 0) {
                // Fixed-size array
                llvm_type = llvm::ArrayType::get(elem_type, arr_type->getSize());
            } else {
                // Dynamic array (represented as pointer)
                llvm_type = llvm::PointerType::get(context, 0);
            }
            break;
        }
        
        case TypeKind::VECTOR: {
            // Vector types (vec2, vec3, vec9, etc.) - SIMD vectors
            // Reference: research_015
            auto* vec_type = static_cast<VectorType*>(aria_type);
            llvm::Type* component_type = mapType(vec_type->getComponentType());
            int dimension = vec_type->getDimension();
            
            // LLVM fixed vectors (for dimensions 2, 3, 4, 8, 16)
            // For vec9, we'll use a struct with 9 components instead
            if (dimension == 9) {
                // vec9 is special - create struct of 9 components
                std::vector<llvm::Type*> components(9, component_type);
                llvm_type = llvm::StructType::get(context, components);
            } else {
                // Standard LLVM fixed vector
                llvm_type = llvm::FixedVectorType::get(component_type, dimension);
            }
            break;
        }
        
        case TypeKind::SIMD: {
            // SIMD types: simd<T, N> for explicit vectorization
            // P1-2 Phase 3: Map to LLVM vector types <N x element_type>
            // Example: simd<int32, 4> → <4 x i32> (SSE)
            //          simd<int64, 8> → <8 x i64> (AVX-512)
            auto* simd_type = static_cast<SimdType*>(aria_type);
            llvm::Type* element_type = mapType(simd_type->getElementType());
            size_t lane_count = simd_type->getLaneCount();
            
            // Create LLVM fixed vector type
            // LLVM's FixedVectorType maps directly to hardware SIMD registers
            llvm_type = llvm::FixedVectorType::get(element_type, lane_count);
            break;
        }
        
        case TypeKind::FUNCTION: {
            // Function types: func(params) -> return
            // Reference: research_016
            auto* func_type = static_cast<sema::FunctionType*>(aria_type);
            
            // Map return type
            llvm::Type* return_type = mapType(func_type->getReturnType());
            
            // Map parameter types
            std::vector<llvm::Type*> param_types;
            for (Type* param : func_type->getParamTypes()) {
                param_types.push_back(mapType(param));
            }
            
            // Create LLVM function type
            llvm_type = llvm::FunctionType::get(
                return_type,
                param_types,
                func_type->isVariadicFunction()  // isVarArg
            );
            break;
        }
        
        case TypeKind::STRUCT: {
            // Struct types with fields
            // Reference: research_015
            auto* struct_type = static_cast<StructType*>(aria_type);
            
            // Check if this struct type already exists in the LLVM module
            // (avoids creating duplicate types when called from pre-pass and main-pass)
            llvm::StructType* existing = llvm::StructType::getTypeByName(context, struct_type->getName());
            if (existing) {
                llvm_type = existing;
                break;
            }
            
            // Map all field types
            std::vector<llvm::Type*> field_types;
            for (const auto& field : struct_type->getFields()) {
                field_types.push_back(mapType(field.type));
            }
            
            // Create LLVM struct type
            // Use identified struct for named types
            llvm_type = llvm::StructType::create(
                context,
                field_types,
                struct_type->getName(),
                struct_type->isPackedStruct()  // isPacked
            );
            break;
        }
        
        case TypeKind::UNION: {
            // Union types - represented as struct with largest variant + tag
            // Reference: research_015
            auto* union_type = static_cast<UnionType*>(aria_type);
            
            // Find largest variant type
            llvm::Type* largest_type = builder.getInt8Ty();  // Minimum size
            size_t max_size = 1;
            
            for (const auto& variant : union_type->getVariants()) {
                llvm::Type* variant_llvm = mapType(variant.type);
                size_t variant_size = module->getDataLayout().getTypeAllocSize(variant_llvm);
                if (variant_size > max_size) {
                    max_size = variant_size;
                    largest_type = variant_llvm;
                }
            }
            
            // Union = { tag: i32, data: largest_type }
            std::vector<llvm::Type*> union_fields = {
                builder.getInt32Ty(),  // Tag field
                largest_type            // Data field (largest variant)
            };
            
            llvm_type = llvm::StructType::create(
                context,
                union_fields,
                union_type->getName()
            );
            break;
        }
        
        case TypeKind::RESULT: {
            // Result type for error handling: result<T>
            // Runtime layout: { T value, void* error, bool is_error }
            // Reference: include/runtime/result.h (AriaResultPtr, AriaResultI64, etc.)
            auto* result_type = static_cast<ResultType*>(aria_type);
            
            llvm::Type* value_type = mapType(result_type->getValueType());
            
            std::vector<llvm::Type*> result_fields = {
                value_type,                         // value (field 0)
                llvm::PointerType::get(context, 0), // error (field 1) - void* pointer
                builder.getInt8Ty()                 // is_error (field 2) - i8 (avoids x86-64 sret ABI issue with i1 in mixed SSE+INT structs)
            };
            
            llvm_type = llvm::StructType::get(context, result_fields);
            break;
        }
        
        case TypeKind::HANDLE: {
            // Handle type for generational arena: Handle<T>
            // Runtime layout: { size_t index, uint32_t generation }
            // Reference: include/runtime/gen_arena.h (aria_handle)
            // P1-3 Phase 3: Handle operations & generation checks
            auto* handle_type = static_cast<HandleType*>(aria_type);
            (void)handle_type;
            
            std::vector<llvm::Type*> handle_fields = {
                builder.getInt64Ty(),  // index (field 0) - size_t (usize)
                builder.getInt32Ty()   // generation (field 1) - uint32_t (u32)
            };
            
            llvm_type = llvm::StructType::get(context, handle_fields);
            break;
        }
        
        case TypeKind::GENERIC: {
            // Generic types should be monomorphized before codegen
            // If we see one here, it's an error in the compiler pipeline
            // For now, treat as opaque pointer
            llvm_type = llvm::PointerType::get(context, 0);
            break;
        }
        
        case TypeKind::OPTIONAL: {
            // Optional<T>: { i1 hasValue, T value }
            auto* opt = static_cast<OptionalType*>(aria_type);
            llvm::Type* wrapped = mapType(opt->getWrappedType());
            llvm_type = llvm::StructType::get(context, {builder.getInt1Ty(), wrapped});
            break;
        }
        
        case TypeKind::ANY: {
            // any: Type-erased pointer — opaque pointer (safe void* replacement)
            // Internally represented as { ptr data, i64 size }
            // For wild any, just a bare pointer (no size tracking)
            llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
            llvm::Type* sizeTy = builder.getInt64Ty();
            llvm_type = llvm::StructType::get(context, {ptrTy, sizeTy});
            break;
        }
        
        case TypeKind::UNKNOWN:
        case TypeKind::ERROR:
        default:
            // Unknown or error types - use void
            llvm_type = builder.getVoidTy();
            break;
    }
    
    // Cache the mapping
    if (llvm_type) {
        type_map[type_name] = llvm_type;
    }
    
    return llvm_type;
}

// ============================================================================
// Optional Type Helper Methods (Session 23 - Phase 2)
// ============================================================================

llvm::Value* IRGenerator::createOptionalSome(llvm::Value* value) {
    // Create optional struct: { i1 true, T value }
    llvm::Type* valueType = value->getType();
    llvm::StructType* optionalType = llvm::StructType::get(context, {builder.getInt1Ty(), valueType});
    
    // Create the struct with hasValue=true and the actual value
    llvm::Value* hasValue = builder.getInt1(true);
    llvm::Value* optStruct = llvm::UndefValue::get(optionalType);
    optStruct = builder.CreateInsertValue(optStruct, hasValue, {0}, "opt.has_value");
    optStruct = builder.CreateInsertValue(optStruct, value, {1}, "opt.some");
    
    return optStruct;
}

llvm::Value* IRGenerator::createOptionalNone(llvm::Type* optionalType) {
    // Create optional struct: { i1 false, T undef }
    // optionalType should already be the full struct type { i1, T }
    llvm::Value* hasValue = builder.getInt1(false);
    llvm::Value* optStruct = llvm::UndefValue::get(optionalType);
    optStruct = builder.CreateInsertValue(optStruct, hasValue, {0}, "opt.none");

    return optStruct;
}

// ============================================================================
// ARIA-024: Limb-Based Integral Model (LBIM) Helper Methods
// Implements arithmetic on struct-wrapped limb arrays to bypass LLVM bugs
// See: github.com/llvm/llvm-project/issues/68751, #56351
// ============================================================================

unsigned IRGenerator::isLBIMType(llvm::Type* type) {
    // Check if type is a struct type
    if (!type->isStructTy()) {
        std::cerr << "[DEBUG] isLBIMType: type is NOT a struct" << std::endl;
        return 0;
    }

    llvm::StructType* structType = llvm::cast<llvm::StructType>(type);

    // Check the struct name
    if (!structType->hasName()) {
        std::cerr << "[DEBUG] isLBIMType: struct has NO name" << std::endl;
        return 0;
    }

    llvm::StringRef name = structType->getName();
    std::cerr << "[DEBUG] isLBIMType: checking struct name='" << name.str() << "'" << std::endl;

    if (name == "struct.int128" || name == "struct.uint128") {
        return 2;  // 2 x i64 limbs
    }
    if (name == "struct.int256" || name == "struct.uint256") {
        return 4;  // 4 x i64 limbs
    }
    if (name == "struct.int512" || name == "struct.uint512") {
        return 8;  // 8 x i64 limbs
    }
    if (name == "struct.int1024" || name == "struct.uint1024") {
        return 16;  // 16 x i64 limbs (ARIA-025: post-quantum crypto)
    }
    if (name == "struct.int2048" || name == "struct.uint2048") {
        return 32;  // 32 x i64 limbs
    }
    if (name == "struct.int4096" || name == "struct.uint4096") {
        std::cerr << "[DEBUG] isLBIMType: MATCHED int4096/uint4096, returning 64" << std::endl;
        return 64;  // 64 x i64 limbs
    }
    if (name == "struct.flt256") {
        std::cerr << "[DEBUG] isLBIMType: MATCHED flt256, returning 4" << std::endl;
        return 4;  // 4 x i64 limbs
    }
    if (name == "struct.flt512") {
        std::cerr << "[DEBUG] isLBIMType: MATCHED flt512, returning 8" << std::endl;
        return 8;  // 8 x i64 limbs
    }

    std::cerr << "[DEBUG] isLBIMType: no match, returning 0" << std::endl;
    return 0;
}

bool IRGenerator::isFix256Type(llvm::Type* type) {
    // Check if type is fix256 (Q128.128 deterministic fixed-point)
    if (!type->isStructTy()) {
        return false;
    }

    llvm::StructType* structType = llvm::cast<llvm::StructType>(type);

    if (!structType->hasName()) {
        return false;
    }

    llvm::StringRef name = structType->getName();
    return name == "struct.fix256";
}

llvm::Value* IRGenerator::generateLBIMAdd(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Ripple-carry addition for LBIM types
    // Algorithm:
    //   carry = 0
    //   for i in 0..numLimbs:
    //     (sum, overflow1) = limb_L[i] + limb_R[i]
    //     (result, overflow2) = sum + carry
    //     carry = overflow1 | overflow2
    //     result_limbs[i] = result

    llvm::Type* i64Ty = builder.getInt64Ty();
    llvm::Type* i1Ty = builder.getInt1Ty();
    (void)i1Ty;
    llvm::Type* structType = L->getType();

    // If R is not the same LBIM struct type, convert it
    if (R->getType() != structType) {
        // R is a scalar (likely int64), zero-extend to LBIM struct
        llvm::Value* rConverted = llvm::UndefValue::get(structType);
        // Put R value in limb[0], rest are zeros
        if (R->getType()->isIntegerTy()) {
            // Ensure it's i64
            if (R->getType() != i64Ty) {
                R = builder.CreateZExtOrTrunc(R, i64Ty, "r.to_i64");
            }
            rConverted = builder.CreateInsertValue(rConverted, R, {0, 0}, "r.limb0");
            // Fill remaining limbs with zeros
            for (unsigned i = 1; i < numLimbs; ++i) {
                rConverted = builder.CreateInsertValue(rConverted, builder.getInt64(0), {0, i});
            }
            R = rConverted;
        } else {
            // Type mismatch - should not happen if type checker is correct
            // Fall back to error
            return nullptr;
        }
    }

    // Get the overflow intrinsic
    llvm::Function* uaddOverflow = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::uadd_with_overflow, {i64Ty});

    // Start with zero carry
    llvm::Value* carry = builder.getInt64(0);

    // Build result struct
    llvm::Value* result = llvm::UndefValue::get(structType);

    for (unsigned i = 0; i < numLimbs; ++i) {
        // Extract limbs: L->limbs[i], R->limbs[i]
        // The struct is { [N x i64] }, so we use indices {0, i}
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "limbR");

        // Step 1: Add the two limbs
        llvm::Value* addResult1 = builder.CreateCall(uaddOverflow, {limbL, limbR}, "add1");
        llvm::Value* sum1 = builder.CreateExtractValue(addResult1, {0}, "sum1");
        llvm::Value* overflow1 = builder.CreateExtractValue(addResult1, {1}, "ovf1");

        // Step 2: Add the carry
        llvm::Value* addResult2 = builder.CreateCall(uaddOverflow, {sum1, carry}, "add2");
        llvm::Value* finalSum = builder.CreateExtractValue(addResult2, {0}, "sum2");
        llvm::Value* overflow2 = builder.CreateExtractValue(addResult2, {1}, "ovf2");

        // Combine overflows for new carry (either overflow sets carry)
        llvm::Value* anyOverflow = builder.CreateOr(overflow1, overflow2, "anyovf");
        carry = builder.CreateZExt(anyOverflow, i64Ty, "carry");

        // Store result limb
        result = builder.CreateInsertValue(result, finalSum, {0, i}, "res.limb");
    }

    return result;
}

llvm::Value* IRGenerator::generateLBIMSub(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Borrow-chain subtraction for LBIM types
    // Algorithm:
    //   borrow = 0
    //   for i in 0..numLimbs:
    //     (diff, underflow1) = limb_L[i] - limb_R[i]
    //     (result, underflow2) = diff - borrow
    //     borrow = underflow1 | underflow2
    //     result_limbs[i] = result

    llvm::Type* i64Ty = builder.getInt64Ty();
    llvm::Type* structType = L->getType();

    // If R is not the same LBIM struct type, convert it
    if (R->getType() != structType) {
        // R is a scalar (likely int64), zero-extend to LBIM struct
        llvm::Value* rConverted = llvm::UndefValue::get(structType);
        if (R->getType()->isIntegerTy()) {
            // Ensure it's i64
            if (R->getType() != i64Ty) {
                R = builder.CreateZExtOrTrunc(R, i64Ty, "r.to_i64");
            }
            rConverted = builder.CreateInsertValue(rConverted, R, {0, 0}, "r.limb0");
            // Fill remaining limbs with zeros
            for (unsigned i = 1; i < numLimbs; ++i) {
                rConverted = builder.CreateInsertValue(rConverted, builder.getInt64(0), {0, i});
            }
            R = rConverted;
        } else {
            return nullptr;
        }
    }

    // Get the overflow intrinsic for subtraction
    llvm::Function* usubOverflow = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::usub_with_overflow, {i64Ty});

    // Start with zero borrow
    llvm::Value* borrow = builder.getInt64(0);

    // Build result struct
    llvm::Value* result = llvm::UndefValue::get(structType);

    for (unsigned i = 0; i < numLimbs; ++i) {
        // Extract limbs
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "limbR");

        // Step 1: Subtract the two limbs
        llvm::Value* subResult1 = builder.CreateCall(usubOverflow, {limbL, limbR}, "sub1");
        llvm::Value* diff1 = builder.CreateExtractValue(subResult1, {0}, "diff1");
        llvm::Value* underflow1 = builder.CreateExtractValue(subResult1, {1}, "udf1");

        // Step 2: Subtract the borrow
        llvm::Value* subResult2 = builder.CreateCall(usubOverflow, {diff1, borrow}, "sub2");
        llvm::Value* finalDiff = builder.CreateExtractValue(subResult2, {0}, "diff2");
        llvm::Value* underflow2 = builder.CreateExtractValue(subResult2, {1}, "udf2");

        // Combine underflows for new borrow
        llvm::Value* anyUnderflow = builder.CreateOr(underflow1, underflow2, "anyudf");
        borrow = builder.CreateZExt(anyUnderflow, i64Ty, "borrow");

        // Store result limb
        result = builder.CreateInsertValue(result, finalDiff, {0, i}, "res.limb");
    }

    return result;
}

llvm::Value* IRGenerator::generateLBIMEQ(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Inline equality comparison for LBIM types
    // Compare all limbs and AND the results together
    // This is deterministic and avoids external function calls

    // Bug #23: if one operand is a flat integer (e.g. a literal), promote it to
    // the LBIM struct type so both ExtractValue calls below succeed.
    if (L->getType()->isStructTy() && !R->getType()->isStructTy())
        R = promoteToLBIMStruct(R, L->getType());
    else if (!L->getType()->isStructTy() && R->getType()->isStructTy())
        L = promoteToLBIMStruct(L, R->getType());

    llvm::Value* result = builder.getInt1(true);  // Start with true
    
    for (unsigned i = 0; i < numLimbs; ++i) {
        // Extract limb i from both operands
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "eq.limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "eq.limbR");
        
        // Compare limbs for equality
        llvm::Value* limbEq = builder.CreateICmpEQ(limbL, limbR, "eq.limb");
        
        // AND with accumulated result
        result = builder.CreateAnd(result, limbEq, "eq.acc");
    }
    
    return result;
}

llvm::Value* IRGenerator::generateLBIMULT(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Unsigned less than: compare from most significant limb to least
    // If L[i] < R[i], return true
    // If L[i] > R[i], return false
    // If L[i] == R[i], continue to next limb
    // If all equal, return false

    // Bug #23: normalize flat-integer operands to LBIM struct before comparison
    if (L->getType()->isStructTy() && !R->getType()->isStructTy())
        R = promoteToLBIMStruct(R, L->getType());
    else if (!L->getType()->isStructTy() && R->getType()->isStructTy())
        L = promoteToLBIMStruct(L, R->getType());

    llvm::Function* func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* resultBlock = llvm::BasicBlock::Create(context, "cmp.result", func);

    // Allocate result variable
    llvm::Value* resultVar = builder.CreateAlloca(builder.getInt1Ty(), nullptr, "cmp.res");
    builder.CreateStore(builder.getInt1(false), resultVar);  // Default: not less than

    // Compare from most significant to least significant
    for (int i = numLimbs - 1; i >= 0; --i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, (unsigned)i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, (unsigned)i}, "limbR");

        llvm::Value* isLt = builder.CreateICmpULT(limbL, limbR, "limb.ult");
        llvm::Value* isGt = builder.CreateICmpUGT(limbL, limbR, "limb.ugt");

        // If L[i] < R[i], result is true, jump to result
        llvm::BasicBlock* ltBlock = llvm::BasicBlock::Create(context, "cmp.lt", func);
        llvm::BasicBlock* notLtBlock = llvm::BasicBlock::Create(context, "cmp.notlt", func);

        builder.CreateCondBr(isLt, ltBlock, notLtBlock);

        builder.SetInsertPoint(ltBlock);
        builder.CreateStore(builder.getInt1(true), resultVar);
        builder.CreateBr(resultBlock);

        builder.SetInsertPoint(notLtBlock);

        // If L[i] > R[i], result is false (default), jump to result
        llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create(context, "cmp.continue", func);
        builder.CreateCondBr(isGt, resultBlock, continueBlock);

        builder.SetInsertPoint(continueBlock);
    }

    // All limbs were equal, result remains false
    builder.CreateBr(resultBlock);

    builder.SetInsertPoint(resultBlock);
    return builder.CreateLoad(builder.getInt1Ty(), resultVar, "cmp.final");
}

llvm::Value* IRGenerator::generateLBIMSLT(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Signed less than
    // For large structs (>16 limbs), use runtime scmp function for ABI compatibility

    // Bug #23: normalize flat-integer operands to LBIM struct before comparison
    if (L->getType()->isStructTy() && !R->getType()->isStructTy())
        R = promoteToLBIMStruct(R, L->getType());
    else if (!L->getType()->isStructTy() && R->getType()->isStructTy())
        L = promoteToLBIMStruct(L, R->getType());

    if (numLimbs > 16) {
        // Use runtime function: int32_t aria_lbim_scmpN(const aria_intN_t* a, const aria_intN_t* b)
        llvm::Type* structType = L->getType();
        llvm::Type* ptrType = llvm::PointerType::get(structType, 0);
        
        std::string funcName;
        switch (numLimbs) {
            case 32: funcName = "aria_lbim_scmp2048"; break;
            case 64: funcName = "aria_lbim_scmp4096"; break;
            default:
                llvm::errs() << "LBIM slt: unsupported limb count " << numLimbs << "\n";
                return builder.getInt1(false);
        }
        
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getInt32Ty(), {ptrType, ptrType}, false);
        
        llvm::Function* cmpFunc = module->getFunction(funcName);
        if (!cmpFunc) {
            cmpFunc = llvm::Function::Create(
                funcType, llvm::Function::ExternalLinkage, funcName, module.get());
        }
        
        // Create allocas and store the struct values
        llvm::Value* allocaL = builder.CreateAlloca(structType, nullptr, "slt_arg_l");
        llvm::Value* allocaR = builder.CreateAlloca(structType, nullptr, "slt_arg_r");
        builder.CreateStore(L, allocaL);
        builder.CreateStore(R, allocaR);
        
        // Call scmp and check if result < 0
        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {allocaL, allocaR}, "scmp");
        llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
        return builder.CreateICmpSLT(cmpResult, zero, "slt_result");
    }
    
    // For small structs, use inline comparison
    // Signed less than: compare high limb as signed, rest as unsigned
    // For signed comparison, the high limb determines sign
    // Algorithm:
    //   1. Compare high limbs as signed
    //   2. If high limbs differ, that determines the result
    //   3. If high limbs equal, compare remaining limbs as unsigned

    llvm::Function* func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* resultBlock = llvm::BasicBlock::Create(context, "cmp.result", func);

    llvm::Value* resultVar = builder.CreateAlloca(builder.getInt1Ty(), nullptr, "cmp.res");
    builder.CreateStore(builder.getInt1(false), resultVar);

    // Extract high limbs for signed comparison
    unsigned highIdx = numLimbs - 1;
    llvm::Value* highL = builder.CreateExtractValue(L, {0, highIdx}, "highL");
    llvm::Value* highR = builder.CreateExtractValue(R, {0, highIdx}, "highR");

    // Signed comparison of high limbs
    llvm::Value* highLt = builder.CreateICmpSLT(highL, highR, "high.slt");
    llvm::Value* highGt = builder.CreateICmpSGT(highL, highR, "high.sgt");
    llvm::Value* highEq = builder.CreateICmpEQ(highL, highR, "high.eq");
    (void)highEq;

    llvm::BasicBlock* highLtBlock = llvm::BasicBlock::Create(context, "high.lt", func);
    llvm::BasicBlock* highNotLtBlock = llvm::BasicBlock::Create(context, "high.notlt", func);

    builder.CreateCondBr(highLt, highLtBlock, highNotLtBlock);

    builder.SetInsertPoint(highLtBlock);
    builder.CreateStore(builder.getInt1(true), resultVar);
    builder.CreateBr(resultBlock);

    builder.SetInsertPoint(highNotLtBlock);

    llvm::BasicBlock* highEqBlock = llvm::BasicBlock::Create(context, "high.eq", func);
    builder.CreateCondBr(highGt, resultBlock, highEqBlock);

    builder.SetInsertPoint(highEqBlock);

    // High limbs are equal, compare remaining limbs as unsigned
    for (int i = numLimbs - 2; i >= 0; --i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, (unsigned)i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, (unsigned)i}, "limbR");

        llvm::Value* isLt = builder.CreateICmpULT(limbL, limbR, "limb.ult");
        llvm::Value* isGt = builder.CreateICmpUGT(limbL, limbR, "limb.ugt");

        llvm::BasicBlock* ltBlock = llvm::BasicBlock::Create(context, "cmp.lt", func);
        llvm::BasicBlock* notLtBlock = llvm::BasicBlock::Create(context, "cmp.notlt", func);

        builder.CreateCondBr(isLt, ltBlock, notLtBlock);

        builder.SetInsertPoint(ltBlock);
        builder.CreateStore(builder.getInt1(true), resultVar);
        builder.CreateBr(resultBlock);

        builder.SetInsertPoint(notLtBlock);

        llvm::BasicBlock* continueBlock = llvm::BasicBlock::Create(context, "cmp.continue", func);
        builder.CreateCondBr(isGt, resultBlock, continueBlock);

        builder.SetInsertPoint(continueBlock);
    }

    // All limbs were equal
    builder.CreateBr(resultBlock);

    builder.SetInsertPoint(resultBlock);
    return builder.CreateLoad(builder.getInt1Ty(), resultVar, "cmp.final");
}

llvm::Value* IRGenerator::generateLBIMMul(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Schoolbook multiplication for LBIM types
    // Uses safe i128 intermediate operations to avoid IPSCCP crashes
    //
    // Algorithm: For N limbs, compute lower N limbs of product
    //   For i in 0..N-1:
    //     For j in 0..N-1:
    //       If i+j < N:
    //         (lo, hi) = L[i] * R[j]  (64x64 -> 128 bit)
    //         Result[i+j] += lo (with carry)
    //         Result[i+j+1] += hi (with carry, if i+j+1 < N)

    llvm::Type* i64Ty = builder.getInt64Ty();
    llvm::Type* structType = L->getType();

    // If R is not the same LBIM struct type, convert it
    if (R->getType() != structType) {
        llvm::Value* rConverted = llvm::UndefValue::get(structType);
        if (R->getType()->isIntegerTy()) {
            if (R->getType() != i64Ty) {
                R = builder.CreateZExtOrTrunc(R, i64Ty, "r.to_i64");
            }
            rConverted = builder.CreateInsertValue(rConverted, R, {0, 0}, "r.limb0");
            for (unsigned i = 1; i < numLimbs; ++i) {
                rConverted = builder.CreateInsertValue(rConverted, builder.getInt64(0), {0, i});
            }
            R = rConverted;
        } else {
            return nullptr;
        }
    }

    // Get the overflow intrinsic for accumulation
    llvm::Function* uaddOverflow = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::uadd_with_overflow, {i64Ty});

    // Allocate result limbs initialized to zero
    std::vector<llvm::Value*> resultLimbs(numLimbs);
    for (unsigned k = 0; k < numLimbs; ++k) {
        resultLimbs[k] = builder.getInt64(0);
    }

    // Schoolbook multiplication: O(N^2) limb multiplications
    for (unsigned i = 0; i < numLimbs; ++i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "mulL");

        for (unsigned j = 0; j < numLimbs; ++j) {
            unsigned k = i + j;  // Target limb index

            // Skip if result limb would be beyond our fixed width
            if (k >= numLimbs) {
                continue;
            }

            llvm::Value* limbR = builder.CreateExtractValue(R, {0, j}, "mulR");

            // Perform 64x64 -> 128 bit multiplication using ONLY i64 operations
            // Decompose: a = a_hi*2^32 + a_lo, b = b_hi*2^32 + b_lo
            // Result: a*b = (a_hi*b_hi)*2^64 + (a_hi*b_lo + a_lo*b_hi)*2^32 + (a_lo*b_lo)
            
            llvm::Type* i32Ty = builder.getInt32Ty();
            
            // Split limbL into high and low 32 bits
            llvm::Value* aLo = builder.CreateTrunc(limbL, i32Ty, "a.lo");
            llvm::Value* aHiShift = builder.CreateLShr(limbL, 32, "a.hi.shift");
            llvm::Value* aHi = builder.CreateTrunc(aHiShift, i32Ty, "a.hi");
            
            // Split limbR into high and low 32 bits
            llvm::Value* bLo = builder.CreateTrunc(limbR, i32Ty, "b.lo");
            llvm::Value* bHiShift = builder.CreateLShr(limbR, 32, "b.hi.shift");
            llvm::Value* bHi = builder.CreateTrunc(bHiShift, i32Ty, "b.hi");
            
            // Extend to i64 for multiplication
            llvm::Value* aLo64 = builder.CreateZExt(aLo, i64Ty, "a.lo64");
            llvm::Value* aHi64 = builder.CreateZExt(aHi, i64Ty, "a.hi64");
            llvm::Value* bLo64 = builder.CreateZExt(bLo, i64Ty, "b.lo64");
            llvm::Value* bHi64 = builder.CreateZExt(bHi, i64Ty, "b.hi64");
            
            // Four 32x32 multiplications (results fit in 64 bits)
            llvm::Value* ll = builder.CreateMul(aLo64, bLo64, "prod.ll");  // Low * Low
            llvm::Value* lh = builder.CreateMul(aLo64, bHi64, "prod.lh");  // Low * High
            llvm::Value* hl = builder.CreateMul(aHi64, bLo64, "prod.hl");  // High * Low
            llvm::Value* hh = builder.CreateMul(aHi64, bHi64, "prod.hh");  // High * High
            
            // Combine results:
            // prodLo = ll_lo + (lh_lo + hl_lo) << 32
            // prodHi = hh + lh_hi + hl_hi + carry from middle additions
            
            // Middle term: (lh + hl) << 32
            llvm::Value* mid1 = builder.CreateCall(uaddOverflow, {lh, hl}, "mid.add");
            llvm::Value* midSum = builder.CreateExtractValue(mid1, {0}, "mid.sum");
            llvm::Value* midOvf = builder.CreateExtractValue(mid1, {1}, "mid.ovf");
            
            // Split middle sum into high and low 32 bits
            llvm::Value* midLo = builder.CreateShl(midSum, 32, "mid.lo.shift");
            llvm::Value* midHi = builder.CreateLShr(midSum, 32, "mid.hi");
            
            // Add overflow from middle to high part (overflow * 2^64 = overflow * 2^32 in high word)
            llvm::Value* midOvf64 = builder.CreateZExt(midOvf, i64Ty, "mid.ovf64");
            llvm::Value* midOvfShift = builder.CreateShl(midOvf64, 32, "mid.ovf.shift");
            
            // Compute low 64 bits: ll + (midSum << 32)
            llvm::Value* loAdd = builder.CreateCall(uaddOverflow, {ll, midLo}, "lo.add");
            llvm::Value* prodLo = builder.CreateExtractValue(loAdd, {0}, "prod.lo");
            llvm::Value* loCarry = builder.CreateExtractValue(loAdd, {1}, "lo.carry");
            llvm::Value* loCarry64 = builder.CreateZExt(loCarry, i64Ty, "lo.carry64");
            
            // Compute high 64 bits: hh + (midSum >> 32) + loCarry + midOvf
            llvm::Value* hi1 = builder.CreateAdd(hh, midHi, "hi.1");
            llvm::Value* hi2 = builder.CreateAdd(hi1, loCarry64, "hi.2");
            llvm::Value* prodHi = builder.CreateAdd(hi2, midOvfShift, "prod.hi");

            // Accumulate low part to Result[k] with carry propagation
            llvm::Value* carry = builder.getInt64(0);

            // Add prodLo to resultLimbs[k]
            llvm::Value* addRes1 = builder.CreateCall(uaddOverflow, {resultLimbs[k], prodLo}, "acc1");
            resultLimbs[k] = builder.CreateExtractValue(addRes1, {0}, "acc1.sum");
            llvm::Value* ovf1 = builder.CreateExtractValue(addRes1, {1}, "acc1.ovf");
            carry = builder.CreateZExt(ovf1, i64Ty, "carry1");

            // Propagate carry through higher limbs
            for (unsigned c = k + 1; c < numLimbs; ++c) {
                llvm::Value* addCarry = builder.CreateCall(uaddOverflow, {resultLimbs[c], carry}, "carry.add");
                resultLimbs[c] = builder.CreateExtractValue(addCarry, {0}, "carry.sum");
                llvm::Value* carryOvf = builder.CreateExtractValue(addCarry, {1}, "carry.ovf");
                carry = builder.CreateZExt(carryOvf, i64Ty, "carry.next");
            }

            // Accumulate high part to Result[k+1] if within bounds
            if (k + 1 < numLimbs) {
                carry = builder.getInt64(0);

                // Add prodHi to resultLimbs[k+1]
                llvm::Value* addRes2 = builder.CreateCall(uaddOverflow, {resultLimbs[k + 1], prodHi}, "acc2");
                resultLimbs[k + 1] = builder.CreateExtractValue(addRes2, {0}, "acc2.sum");
                llvm::Value* ovf2 = builder.CreateExtractValue(addRes2, {1}, "acc2.ovf");
                carry = builder.CreateZExt(ovf2, i64Ty, "carry2");

                // Propagate carry through higher limbs
                for (unsigned c = k + 2; c < numLimbs; ++c) {
                    llvm::Value* addCarry = builder.CreateCall(uaddOverflow, {resultLimbs[c], carry}, "carry.add");
                    resultLimbs[c] = builder.CreateExtractValue(addCarry, {0}, "carry.sum");
                    llvm::Value* carryOvf = builder.CreateExtractValue(addCarry, {1}, "carry.ovf");
                    carry = builder.CreateZExt(carryOvf, i64Ty, "carry.next");
                }
            }
        }
    }

    // Build result struct from limbs
    llvm::Value* result = llvm::UndefValue::get(structType);
    for (unsigned k = 0; k < numLimbs; ++k) {
        result = builder.CreateInsertValue(result, resultLimbs[k], {0, k}, "mul.limb");
    }

    return result;
}

llvm::Value* IRGenerator::generateLBIMDiv(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // LBIM signed division by calling runtime intrinsic
    // Uses aria_lbim_sdivN where N is the bit width (128/256/512/1024/2048/4096)
    //
    // BUG-027 FIX: Match the C ABI for each runtime function:
    //   int128-int1024 (lbim.cpp): takes structs by value → use sret + byval
    //   int2048-int4096 (lbim_extended.cpp): takes const ptr → use sret + ptr

    // Promote scalar operands to LBIM struct type (e.g., int4096:x / 3)
    llvm::Type* structType = L->getType()->isStructTy() ? L->getType() : R->getType();
    if (!L->getType()->isStructTy() && R->getType()->isStructTy())
        L = promoteToLBIMStruct(L, structType);
    else if (L->getType()->isStructTy() && !R->getType()->isStructTy())
        R = promoteToLBIMStruct(R, structType);

    llvm::Type* ptrType = llvm::PointerType::getUnqual(structType);
    llvm::Type* voidType = builder.getVoidTy();
    bool usePointers = (numLimbs > 16);  // int2048, int4096 C functions take const ptr

    // Determine the runtime function name based on number of limbs
    std::string funcName;
    switch (numLimbs) {
        case 2: funcName = "aria_lbim_sdiv128"; break;
        case 4: funcName = "aria_lbim_sdiv256"; break;
        case 8: funcName = "aria_lbim_sdiv512"; break;
        case 16: funcName = "aria_lbim_sdiv1024"; break;
        case 32: funcName = "aria_lbim_sdiv2048"; break;
        case 64: funcName = "aria_lbim_sdiv4096"; break;
        default:
            llvm::errs() << "LBIM div: unsupported limb count " << numLimbs << "\n";
            return llvm::UndefValue::get(structType);
    }

    // All variants use: void func(ptr sret, ptr, ptr)
    // The difference is byval attribute on params 1,2 for by-value C functions
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        voidType, {ptrType, ptrType, ptrType}, false);

    llvm::Function* divFunc = module->getFunction(funcName);
    if (!divFunc) {
        divFunc = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, funcName, module.get());
        divFunc->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
        divFunc->addParamAttr(0, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        if (!usePointers) {
            // By-value C functions: byval copies struct to stack (matches C ABI MEMORY class)
            divFunc->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            divFunc->addParamAttr(1, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
            divFunc->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
            divFunc->addParamAttr(2, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        }
    }

    // Create allocas for result and arguments
    llvm::Value* resultAlloca = builder.CreateAlloca(structType, nullptr, "div_result");
    llvm::Value* allocaL = builder.CreateAlloca(structType, nullptr, "div_arg_l");
    llvm::Value* allocaR = builder.CreateAlloca(structType, nullptr, "div_arg_r");
    builder.CreateStore(L, allocaL);
    builder.CreateStore(R, allocaR);

    llvm::CallInst* call = builder.CreateCall(divFunc, {resultAlloca, allocaL, allocaR});
    call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
    call->addParamAttr(0, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
    if (!usePointers) {
        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
        call->addParamAttr(1, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
        call->addParamAttr(2, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
    }

    return builder.CreateLoad(structType, resultAlloca, "lbim.div");
}

llvm::Value* IRGenerator::generateLBIMMod(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // LBIM signed modulo — same dual-ABI approach as generateLBIMDiv.

    // Promote scalar operands to LBIM struct type
    llvm::Type* structType = L->getType()->isStructTy() ? L->getType() : R->getType();
    if (!L->getType()->isStructTy() && R->getType()->isStructTy())
        L = promoteToLBIMStruct(L, structType);
    else if (L->getType()->isStructTy() && !R->getType()->isStructTy())
        R = promoteToLBIMStruct(R, structType);

    llvm::Type* ptrType = llvm::PointerType::getUnqual(structType);
    llvm::Type* voidType = builder.getVoidTy();
    bool usePointers = (numLimbs > 16);

    std::string funcName;
    switch (numLimbs) {
        case 2: funcName = "aria_lbim_smod128"; break;
        case 4: funcName = "aria_lbim_smod256"; break;
        case 8: funcName = "aria_lbim_smod512"; break;
        case 16: funcName = "aria_lbim_smod1024"; break;
        case 32: funcName = "aria_lbim_smod2048"; break;
        case 64: funcName = "aria_lbim_smod4096"; break;
        default:
            llvm::errs() << "LBIM mod: unsupported limb count " << numLimbs << "\n";
            return llvm::UndefValue::get(structType);
    }

    llvm::FunctionType* funcType = llvm::FunctionType::get(
        voidType, {ptrType, ptrType, ptrType}, false);

    llvm::Function* modFunc = module->getFunction(funcName);
    if (!modFunc) {
        modFunc = llvm::Function::Create(
            funcType, llvm::Function::ExternalLinkage, funcName, module.get());
        modFunc->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
        modFunc->addParamAttr(0, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        if (!usePointers) {
            modFunc->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            modFunc->addParamAttr(1, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
            modFunc->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
            modFunc->addParamAttr(2, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        }
    }

    llvm::Value* resultAlloca = builder.CreateAlloca(structType, nullptr, "mod_result");
    llvm::Value* allocaL = builder.CreateAlloca(structType, nullptr, "mod_arg_l");
    llvm::Value* allocaR = builder.CreateAlloca(structType, nullptr, "mod_arg_r");
    builder.CreateStore(L, allocaL);
    builder.CreateStore(R, allocaR);

    llvm::CallInst* call = builder.CreateCall(modFunc, {resultAlloca, allocaL, allocaR});
    call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
    call->addParamAttr(0, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
    if (!usePointers) {
        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
        call->addParamAttr(1, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
        call->addParamAttr(2, llvm::Attribute::getWithAlignment(context, llvm::Align(8)));
    }

    return builder.CreateLoad(structType, resultAlloca, "lbim.mod");
}

llvm::Value* IRGenerator::generateLBIMAnd(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Limb-wise bitwise AND for LBIM struct types
    llvm::Type* structType = L->getType();
    llvm::Value* result = llvm::UndefValue::get(structType);
    for (unsigned i = 0; i < numLimbs; ++i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "limbR");
        llvm::Value* andVal = builder.CreateAnd(limbL, limbR, "lbim.and");
        result = builder.CreateInsertValue(result, andVal, {0, i}, "res.limb");
    }
    return result;
}

llvm::Value* IRGenerator::generateLBIMOr(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Limb-wise bitwise OR for LBIM struct types
    llvm::Type* structType = L->getType();
    llvm::Value* result = llvm::UndefValue::get(structType);
    for (unsigned i = 0; i < numLimbs; ++i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "limbR");
        llvm::Value* orVal = builder.CreateOr(limbL, limbR, "lbim.or");
        result = builder.CreateInsertValue(result, orVal, {0, i}, "res.limb");
    }
    return result;
}

llvm::Value* IRGenerator::generateLBIMXor(llvm::Value* L, llvm::Value* R, unsigned numLimbs) {
    // Limb-wise bitwise XOR for LBIM struct types
    llvm::Type* structType = L->getType();
    llvm::Value* result = llvm::UndefValue::get(structType);
    for (unsigned i = 0; i < numLimbs; ++i) {
        llvm::Value* limbL = builder.CreateExtractValue(L, {0, i}, "limbL");
        llvm::Value* limbR = builder.CreateExtractValue(R, {0, i}, "limbR");
        llvm::Value* xorVal = builder.CreateXor(limbL, limbR, "lbim.xor");
        result = builder.CreateInsertValue(result, xorVal, {0, i}, "res.limb");
    }
    return result;
}

// ============================================================================
// Unknown-Safe Arithmetic (Layer 1 Safety)
// Prevents undefined behavior by returning Unknown sentinel values
// ============================================================================

llvm::Value* IRGenerator::generateSafeSDiv(llvm::Value* L, llvm::Value* R, const std::string& name) {
    // Safe signed division: checks for divide-by-zero and returns Unknown sentinel
    // Unknown sentinel = signed maximum value (INT_MAX for the given bit width)
    
    llvm::Type* resultType = L->getType();
    llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(resultType);
    
    if (!intType) {
        if (resultType->isFloatingPointTy())
            return builder.CreateFDiv(L, R, name);
        return builder.CreateSDiv(L, R, name);
    }
    
    unsigned bitWidth = intType->getBitWidth();
    
    // Check if divisor is zero
    llvm::Value* isZero = builder.CreateICmpEQ(R, llvm::ConstantInt::get(intType, 0), "div.iszero");
    
    // Generate Unknown sentinel (signed maximum)
    llvm::Value* unknownSentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(bitWidth));
    
    // Perform actual division
    llvm::Value* divResult = builder.CreateSDiv(L, R, name);
    
    // Select: if divisor is zero, return Unknown; otherwise return division result
    return builder.CreateSelect(isZero, unknownSentinel, divResult, "safe." + name);
}

llvm::Value* IRGenerator::generateSafeSRem(llvm::Value* L, llvm::Value* R, const std::string& name) {
    // Safe signed modulo: checks for divide-by-zero and returns Unknown sentinel
    // Unknown sentinel = signed maximum value (INT_MAX for the given bit width)
    
    llvm::Type* resultType = L->getType();
    llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(resultType);
    
    if (!intType) {
        // Fallback for non-integer types - just do regular modulo
        return builder.CreateSRem(L, R, name);
    }
    
    unsigned bitWidth = intType->getBitWidth();
    
    // Check if divisor is zero
    llvm::Value* isZero = builder.CreateICmpEQ(R, llvm::ConstantInt::get(intType, 0), "mod.iszero");
    
    // Generate Unknown sentinel (signed maximum)
    llvm::Value* unknownSentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(bitWidth));
    
    // Perform actual modulo
    llvm::Value* modResult = builder.CreateSRem(L, R, name);
    
    // Select: if divisor is zero, return Unknown; otherwise return modulo result
    return builder.CreateSelect(isZero, unknownSentinel, modResult, "safe." + name);
}

llvm::Value* IRGenerator::generateSafeAdd(llvm::Value* L, llvm::Value* R, const std::string& name) {
    // Safe signed addition: checks for overflow and returns Unknown sentinel
    // Uses LLVM's llvm.sadd.with.overflow intrinsic
    
    llvm::Type* resultType = L->getType();
    llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(resultType);
    
    if (!intType) {
        if (resultType->isFloatingPointTy())
            return builder.CreateFAdd(L, R, name);
        return builder.CreateAdd(L, R, name);
    }
    
    unsigned bitWidth = intType->getBitWidth();
    
    // CRITICAL FIX: Ensure R has the same type as L
    // Integer literals default to i64, but intrinsics require matching types
    if (L->getType() != R->getType()) {
        if (R->getType()->isIntegerTy()) {
            llvm::IntegerType* RIntTy = llvm::cast<llvm::IntegerType>(R->getType());
            if (RIntTy->getBitWidth() > bitWidth) {
                // R is wider - truncate
                R = builder.CreateTrunc(R, intType, "trunc_rhs");
            } else if (RIntTy->getBitWidth() < bitWidth) {
                // R is narrower - extend
                R = builder.CreateSExt(R, intType, "sext_rhs");
            }
        }
    }
    
    // Get the overflow intrinsic: llvm.sadd.with.overflow
    llvm::Function* saddIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::sadd_with_overflow, {intType});
    
    // Call the intrinsic - returns {result, overflow_bit}
    llvm::Value* saddResult = builder.CreateCall(saddIntrinsic, {L, R}, "sadd");
    
    // Extract the actual result and overflow flag
    llvm::Value* result = builder.CreateExtractValue(saddResult, 0, name);
    llvm::Value* overflow = builder.CreateExtractValue(saddResult, 1, "add.overflow");
    
    // Generate Unknown sentinel (signed maximum)
    llvm::Value* unknownSentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(bitWidth));
    
    // Select: if overflow, return Unknown; otherwise return actual result
    return builder.CreateSelect(overflow, unknownSentinel, result, "safe." + name);
}

llvm::Value* IRGenerator::generateSafeSub(llvm::Value* L, llvm::Value* R, const std::string& name) {
    // Safe signed subtraction: checks for overflow and returns Unknown sentinel
    // Uses LLVM's llvm.ssub.with.overflow intrinsic
    
    llvm::Type* resultType = L->getType();
    llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(resultType);
    
    if (!intType) {
        if (resultType->isFloatingPointTy())
            return builder.CreateFSub(L, R, name);
        return builder.CreateSub(L, R, name);
    }
    
    unsigned bitWidth = intType->getBitWidth();
    
    // Get the overflow intrinsic: llvm.ssub.with.overflow
    llvm::Function* ssubIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::ssub_with_overflow, {intType});
    
    // Call the intrinsic - returns {result, overflow_bit}
    llvm::Value* ssubResult = builder.CreateCall(ssubIntrinsic, {L, R}, "ssub");
    
    // Extract the actual result and overflow flag
    llvm::Value* result = builder.CreateExtractValue(ssubResult, 0, name);
    llvm::Value* overflow = builder.CreateExtractValue(ssubResult, 1, "sub.overflow");
    
    // Generate Unknown sentinel (signed maximum)
    llvm::Value* unknownSentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(bitWidth));
    
    // Select: if overflow, return Unknown; otherwise return actual result
    return builder.CreateSelect(overflow, unknownSentinel, result, "safe." + name);
}

llvm::Value* IRGenerator::generateSafeMul(llvm::Value* L, llvm::Value* R, const std::string& name) {
    // Safe signed multiplication: checks for overflow and returns Unknown sentinel
    // Uses LLVM's llvm.smul.with.overflow intrinsic
    
    llvm::Type* resultType = L->getType();
    llvm::IntegerType* intType = llvm::dyn_cast<llvm::IntegerType>(resultType);
    
    if (!intType) {
        if (resultType->isFloatingPointTy())
            return builder.CreateFMul(L, R, name);
        return builder.CreateMul(L, R, name);
    }
    
    unsigned bitWidth = intType->getBitWidth();
    
    // CRITICAL FIX: Ensure R has the same type as L
    // Integer literals default to i64, but intrinsics require matching types
    if (L->getType() != R->getType()) {
        if (R->getType()->isIntegerTy()) {
            llvm::IntegerType* RIntTy = llvm::cast<llvm::IntegerType>(R->getType());
            if (RIntTy->getBitWidth() > bitWidth) {
                // R is wider - truncate
                R = builder.CreateTrunc(R, intType, "trunc_rhs");
            } else if (RIntTy->getBitWidth() < bitWidth) {
                // R is narrower - extend
                R = builder.CreateSExt(R, intType, "sext_rhs");
            }
        }
    }
    
    // Get the overflow intrinsic: llvm.smul.with.overflow
    llvm::Function* smulIntrinsic = llvm::Intrinsic::getOrInsertDeclaration(
        module.get(), llvm::Intrinsic::smul_with_overflow, {intType});
    
    // Call the intrinsic - returns {result, overflow_bit}
    llvm::Value* smulResult = builder.CreateCall(smulIntrinsic, {L, R}, "smul");
    
    // Extract the actual result and overflow flag
    llvm::Value* result = builder.CreateExtractValue(smulResult, 0, name);
    llvm::Value* overflow = builder.CreateExtractValue(smulResult, 1, "mul.overflow");
    
    // Generate Unknown sentinel (signed maximum)
    llvm::Value* unknownSentinel = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(bitWidth));
    
    // Select: if overflow, return Unknown; otherwise return actual result
    return builder.CreateSelect(overflow, unknownSentinel, result, "safe." + name);
}

// Helper function to map type name strings to LLVM types
llvm::Type* IRGenerator::mapTypeFromName(const std::string& type_name) {
    // Check for void
    if (type_name == "void") {
        return builder.getVoidTy();
    }
    
    // Check for Result types (e.g., "Result<int32>", "Result<string>")
    // Result types are represented as structs: { T value, ptr error, i1 is_error }
    // Reference: include/runtime/result.h - layout is { T value, void* error, bool is_error }
    if (type_name.size() > 7 && type_name.substr(0, 7) == "Result<" && type_name.back() == '>') {
        std::string value_type_str = type_name.substr(7, type_name.size() - 8);
        llvm::Type* valueType = mapTypeFromName(value_type_str);
        return llvm::StructType::get(context, {
            valueType,                          // Field 0: value (T)
            llvm::PointerType::get(context, 0), // Field 1: error (void*)
            builder.getInt8Ty()                 // Field 2: is_error (i8)
        });
    }
    
    // Check for optional types (e.g., "int64?", "string?")
    // Optional types are represented as structs: { i1 hasValue, T value }
    if (type_name.size() >= 2 && type_name.back() == '?') {
        std::string wrapped_type = type_name.substr(0, type_name.size() - 1);
        llvm::Type* valueType = mapTypeFromName(wrapped_type);
        return llvm::StructType::get(context, {builder.getInt1Ty(), valueType});
    }
    
    // ARIA-P3: ?@ / ?* — type-erased pointer (maps to LLVM opaque ptr)
    // "?" is the type-slot occupant meaning "type at this address is unknown".
    // ?@  serialised from ?->  (fat erased, Aria code)
    // ?*  serialised from ?*   (thin erased, extern/FFI)
    // Both compile to llvm::PointerType::get(context, 0) — same as every other ptr.
    if (type_name == "?@" || type_name == "?*") {
        return llvm::PointerType::get(context, 0);
    }
    
    // any — type-erased pointer (safe void* replacement)
    // Represented as { ptr data, i64 size } for runtime safety checks
    if (type_name == "any" || type_name == "wild any" || type_name == "wildx any") {
        llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
        llvm::Type* sizeTy = builder.getInt64Ty();
        return llvm::StructType::get(context, {ptrTy, sizeTy});
    }

    // Check for pointer types (e.g., "int64@", "string@")
    // Arrays are represented as pointers in the type system
    if (type_name.size() >= 1 && type_name.back() == '@') {
        // Pointer type (including arrays)
        return llvm::PointerType::get(context, 0);
    }
    
    // Boolean
    if (type_name == "bool") {
        return builder.getInt1Ty();
    }
    
    // NIL type - unit type (empty struct)
    if (type_name == "NIL") {
        llvm::StructType* nilType = llvm::StructType::getTypeByName(context, "struct.NIL");
        if (!nilType) {
            nilType = llvm::StructType::create(context, {}, "struct.NIL");
        }
        return nilType;
    }
    
    // Floating point types
    if (type_name == "flt16" || type_name == "float16") return builder.getHalfTy();
    if (type_name == "flt32" || type_name == "float32") return builder.getFloatTy();
    if (type_name == "flt64" || type_name == "float64") return builder.getDoubleTy();
    if (type_name == "flt128" || type_name == "float128") return llvm::Type::getFP128Ty(context);
    // ARIA-017: Extended precision floats (library-based, stored as limb arrays)
    // flt256: 4 x i64 limbs (256 bits), 32-byte aligned
    // flt512: 8 x i64 limbs (512 bits), 64-byte aligned
    if (type_name == "flt256") {
        llvm::StructType* flt256Struct = llvm::StructType::getTypeByName(context, "struct.flt256");
        if (!flt256Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 4);
            flt256Struct = llvm::StructType::create(context, {limbArray}, "struct.flt256");
        }
        return flt256Struct;
    }
    if (type_name == "flt512") {
        llvm::StructType* flt512Struct = llvm::StructType::getTypeByName(context, "struct.flt512");
        if (!flt512Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 8);
            flt512Struct = llvm::StructType::create(context, {limbArray}, "struct.flt512");
        }
        return flt512Struct;
    }
    
    // Integer types - signed
    if (type_name == "int1") return builder.getInt1Ty();
    if (type_name == "int2") return builder.getIntNTy(2);
    if (type_name == "int4") return builder.getIntNTy(4);
    if (type_name == "int8") return builder.getInt8Ty();
    if (type_name == "int16") return builder.getInt16Ty();
    if (type_name == "int32") return builder.getInt32Ty();
    if (type_name == "int64") return builder.getInt64Ty();

    // ARIA-024: Large integers use Limb-Based Integral Model (LBIM)
    // Stored as structs of i64 limbs to bypass LLVM IPSCCP/ConstraintElimination bugs
    // See: github.com/llvm/llvm-project/issues/68751, #56351
    if (type_name == "int128" || type_name == "uint128") {
        llvm::StructType* int128Struct = llvm::StructType::getTypeByName(context, "struct.int128");
        if (!int128Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 2);
            int128Struct = llvm::StructType::create(context, {limbArray}, "struct.int128");
        }
        return int128Struct;
    }
    if (type_name == "int256" || type_name == "uint256") {
        llvm::StructType* int256Struct = llvm::StructType::getTypeByName(context, "struct.int256");
        if (!int256Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 4);
            int256Struct = llvm::StructType::create(context, {limbArray}, "struct.int256");
        }
        return int256Struct;
    }
    if (type_name == "int512" || type_name == "uint512") {
        llvm::StructType* int512Struct = llvm::StructType::getTypeByName(context, "struct.int512");
        if (!int512Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 8);
            int512Struct = llvm::StructType::create(context, {limbArray}, "struct.int512");
        }
        return int512Struct;
    }
    if (type_name == "int1024" || type_name == "uint1024") {
        llvm::StructType* int1024Struct = llvm::StructType::getTypeByName(context, "struct.int1024");
        if (!int1024Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 16);
            int1024Struct = llvm::StructType::create(context, {limbArray}, "struct.int1024");
        }
        return int1024Struct;
    }
    if (type_name == "int2048" || type_name == "uint2048") {
        llvm::StructType* int2048Struct = llvm::StructType::getTypeByName(context, "struct.int2048");
        if (!int2048Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 32);
            int2048Struct = llvm::StructType::create(context, {limbArray}, "struct.int2048");
        }
        return int2048Struct;
    }
    if (type_name == "int4096" || type_name == "uint4096") {
        llvm::StructType* int4096Struct = llvm::StructType::getTypeByName(context, "struct.int4096");
        if (!int4096Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 64);
            int4096Struct = llvm::StructType::create(context, {limbArray}, "struct.int4096");
        }
        return int4096Struct;
    }
    
    // ARIA-025: fix256 deterministic fixed-point (Q128.128 for physics simulations)
    // See Report 7 § "Deterministic Physics", Report 8 § "fix256 Runtime"
    if (type_name == "fix256") {
        llvm::StructType* fix256Struct = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Struct) {
            // fix256 uses 4-limb structure: [2 x i64] integer part + [2 x i64] fractional part
            llvm::Type* limbArray = llvm::ArrayType::get(builder.getInt64Ty(), 4);
            fix256Struct = llvm::StructType::create(context, {limbArray}, "struct.fix256");
        }
        return fix256Struct;
    }

    // Integer types - unsigned (standard sizes)
    if (type_name == "uint1") return builder.getInt1Ty();
    if (type_name == "uint2") return builder.getIntNTy(2);
    if (type_name == "uint4") return builder.getIntNTy(4);
    if (type_name == "uint8") return builder.getInt8Ty();
    if (type_name == "uint16") return builder.getInt16Ty();
    if (type_name == "uint32") return builder.getInt32Ty();
    if (type_name == "uint64") return builder.getInt64Ty();

    // TBB types - Ternary Balanced Binary (symmetric signed integers with ERR sentinel)
    if (type_name == "tbb8") return builder.getInt8Ty();
    if (type_name == "tbb16") return builder.getInt16Ty();
    if (type_name == "tbb32") return builder.getInt32Ty();
    if (type_name == "tbb64") return builder.getInt64Ty();
    
    // Fraction types: {whole, numerator, denominator} (all same TBB width)
    // Exact rational arithmetic - mathematically precise (no floating-point drift)
    if (type_name == "frac8") {
        std::vector<llvm::Type*> fields = {
            builder.getInt8Ty(),   // whole (tbb8)
            builder.getInt8Ty(),   // numerator (tbb8)
            builder.getInt8Ty()    // denominator (tbb8)
        };
        return llvm::StructType::get(context, fields);
    }
    if (type_name == "frac16") {
        std::vector<llvm::Type*> fields = {
            builder.getInt16Ty(),  // whole (tbb16)
            builder.getInt16Ty(),  // numerator (tbb16)
            builder.getInt16Ty()   // denominator (tbb16)
        };
        return llvm::StructType::get(context, fields);
    }
    if (type_name == "frac32") {
        std::vector<llvm::Type*> fields = {
            builder.getInt32Ty(),  // whole (tbb32)
            builder.getInt32Ty(),  // numerator (tbb32)
            builder.getInt32Ty()   // denominator (tbb32)
        };
        return llvm::StructType::get(context, fields);
    }
    if (type_name == "frac64") {
        std::vector<llvm::Type*> fields = {
            builder.getInt64Ty(),  // whole (tbb64)
            builder.getInt64Ty(),  // numerator (tbb64)
            builder.getInt64Ty()   // denominator (tbb64)
        };
        return llvm::StructType::get(context, fields);
    }
    
    // Twisted Floating Point types: {tbb16 exp, tbbN mant}
    // Deterministic IEEE-free floating point with cross-platform bit-exact reproducibility
    if (type_name == "tfp32") {
        std::vector<llvm::Type*> fields = {
            builder.getInt16Ty(),  // exponent (tbb16)
            builder.getInt16Ty()   // mantissa (tbb16)
        };
        return llvm::StructType::get(context, fields);
    }
    if (type_name == "tfp64") {
        std::vector<llvm::Type*> fields = {
            builder.getInt16Ty(),  // exponent (tbb16)
            builder.getInt64Ty()   // mantissa (tbb48 stored in i64, upper 16 bits unused)
        };
        return llvm::StructType::get(context, fields);
    }
    
    // TMatrix: Sentinel-aware matrix (for 9D toroidal memory system)
    // Wild pointer + metadata for safe matrix operations
    if (type_name == "tmatrix") {
        std::vector<llvm::Type*> fields = {
            builder.getInt64Ty(),                // width
            builder.getInt64Ty(),                // height
            builder.getInt32Ty(),                // element_size
            builder.getInt32Ty(),                // padding/alignment
            llvm::PointerType::get(context, 0)   // wild data pointer
        };
        return llvm::StructType::get(context, fields);
    }
    
    // TTensor: 9D toroidal tensor (for 9D toroidal memory system)
    // Wild pointer + 9D dimensional metadata
    if (type_name == "ttensor") {
        std::vector<llvm::Type*> fields = {
            llvm::ArrayType::get(builder.getInt64Ty(), 9),  // dimensions[9]
            builder.getInt32Ty(),                // element_size
            builder.getInt32Ty(),                // padding/alignment
            llvm::PointerType::get(context, 0)   // wild data pointer
        };
        return llvm::StructType::get(context, fields);
    }
    
    // Balanced Ternary types
    // Research-based: i8 storage allows ERR sentinel (-128) and overflow detection
    if (type_name == "trit") return builder.getInt8Ty();    // Single trit (-1, 0, 1, ERR=-128) in i8
    if (type_name == "tryte") return builder.getInt16Ty();  // 10 trits in 16 bits
    
    // Nonary types  
    if (type_name == "nit") return builder.getInt8Ty();     // Single nit (-4 to 4, ERR=-128) in i8
    if (type_name == "nyte") return builder.getInt16Ty();   // 5 nits in 16 bits
    
    // String type - represented as pointer to AriaString struct {char* data, int64 length}
    // BUGFIX (Feb 2026): Was returning i8*, but AriaString is a struct - caused corruption
    if (type_name == "string") {
        llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!ariaStringType) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                llvm::Type::getInt64Ty(context)
            };
            ariaStringType = llvm::StructType::create(context, fields, "struct.AriaString");
        }
        return llvm::PointerType::get(ariaStringType, 0);
    }
    
    // Vector types (vec2, vec3, vec9)
    // Also support component-typed vectors: dvec2, fvec2, ivec2, etc.
    if (type_name == "vec2" || type_name == "dvec2") {
        // vec2 / dvec2 with flt64 components
        return llvm::FixedVectorType::get(builder.getDoubleTy(), 2);
    }
    if (type_name == "fvec2") {
        // fvec2 with flt32 components
        return llvm::FixedVectorType::get(builder.getFloatTy(), 2);
    }
    if (type_name == "ivec2") {
        // ivec2 with int32 components
        return llvm::FixedVectorType::get(builder.getInt32Ty(), 2);
    }
    if (type_name == "vec3" || type_name == "dvec3") {
        // vec3 / dvec3 with flt64 components
        return llvm::FixedVectorType::get(builder.getDoubleTy(), 3);
    }
    if (type_name == "fvec3") {
        // fvec3 with flt32 components
        return llvm::FixedVectorType::get(builder.getFloatTy(), 3);
    }
    if (type_name == "ivec3") {
        // ivec3 with int32 components
        return llvm::FixedVectorType::get(builder.getInt32Ty(), 3);
    }
    if (type_name == "vec9" || type_name == "dvec9") {
        // vec9: 16-element toroidal vector (16 x tbb32 = 512 bits, 64-byte aligned)
        // Used for 9D toroidal memory addressing
        return llvm::ArrayType::get(builder.getInt32Ty(), 16);
    }
    if (type_name == "fvec9") {
        // fvec9 with flt32 components (struct of 9 floats)
        std::vector<llvm::Type*> components(9, builder.getFloatTy());
        return llvm::StructType::get(context, components);
    }
    if (type_name == "ivec9") {
        // ivec9 with int32 components (struct of 9 ints)
        std::vector<llvm::Type*> components(9, builder.getInt32Ty());
        return llvm::StructType::get(context, components);
    }
    
    // SIMD types: simd<T, N> for explicit vectorization (P1-2 Phase 3)
    // Example: simd<int32, 4> -> <4 x i32> (SSE)
    //          simd<int64, 8> -> <8 x i64> (AVX-512)
    //          simd<fix256, 16> -> <16 x struct.fix256>
    if (type_name.rfind("simd<", 0) == 0 && type_name.back() == '>') {
        // Parse "simd<elementType, laneCount>"
        size_t comma_pos = type_name.find(',');
        if (comma_pos != std::string::npos) {
            // Extract element type: "simd<int32, 4>" -> "int32"
            std::string elem_type_str = type_name.substr(5, comma_pos - 5);
            // Trim whitespace
            elem_type_str.erase(0, elem_type_str.find_first_not_of(" \t"));
            elem_type_str.erase(elem_type_str.find_last_not_of(" \t") + 1);
            
            // Extract lane count: "simd<int32, 4>" -> "4"
            std::string lane_count_str = type_name.substr(comma_pos + 1, type_name.length() - comma_pos - 2);
            // Trim whitespace
            lane_count_str.erase(0, lane_count_str.find_first_not_of(" \t"));
            lane_count_str.erase(lane_count_str.find_last_not_of(" \t") + 1);
            
            // Convert lane count to integer
            size_t lane_count = std::stoull(lane_count_str);
            
            // Recursively get element type
            llvm::Type* elem_type = mapTypeFromName(elem_type_str);
            
            // Create LLVM fixed vector type
            // Maps directly to hardware SIMD registers (SSE, AVX, AVX-512)
            return llvm::FixedVectorType::get(elem_type, lane_count);
        }
    }
    
    // Check for custom types (structs, unions, etc.) if TypeSystem is available
    if (type_system) {
        // Look up struct type by exact name first
        Type* aria_type = type_system->getStructType(type_name);
        if (aria_type) {
            return mapType(aria_type);
        }
        
        // Handle generic type names: "Name<arg1, arg2>" -> mangled "Name_arg1_arg2"
        // GenericType::toString() produces e.g. "Vec<int8>" but the type checker
        // registers the struct under the mangled name "Vec_int8" in structCache.
        size_t angle_pos = type_name.find('<');
        if (angle_pos != std::string::npos && type_name.back() == '>') {
            std::string baseName = type_name.substr(0, angle_pos);
            std::string argsStr = type_name.substr(angle_pos + 1, type_name.size() - angle_pos - 2);
            
            // Build mangled name: baseName_arg1_arg2_...
            std::string mangledName = baseName;
            size_t start = 0;
            int depth = 0;
            for (size_t i = 0; i <= argsStr.size(); i++) {
                if (i == argsStr.size() || (argsStr[i] == ',' && depth == 0)) {
                    std::string arg = argsStr.substr(start, i - start);
                    // Trim whitespace
                    arg.erase(0, arg.find_first_not_of(" \t"));
                    arg.erase(arg.find_last_not_of(" \t") + 1);
                    if (!arg.empty()) {
                        mangledName += "_" + arg;
                    }
                    start = i + 1;
                } else if (argsStr[i] == '<') {
                    depth++;
                } else if (argsStr[i] == '>') {
                    depth--;
                }
            }
            
            aria_type = type_system->getStructType(mangledName);
            if (aria_type) {
                return mapType(aria_type);
            }
        }
    }

    // Fixed-size array types: elementType[N] (e.g., "int32[4]", "flt64[8]")
    // Must be checked last so named struct types win over generic parsing.
    {
        auto bracket_pos = type_name.rfind('[');
        if (bracket_pos != std::string::npos && type_name.back() == ']') {
            std::string elem_str = type_name.substr(0, bracket_pos);
            std::string size_str = type_name.substr(bracket_pos + 1,
                                                     type_name.size() - bracket_pos - 2);
            if (!size_str.empty()) {
                // Sized array: int32[4] -> [4 x i32]
                // Also handle typeNode->toString() form: int32[Literal(4)]
                llvm::Type* elem_type = mapTypeFromName(elem_str);
                // Try plain integer first
                try {
                    uint64_t arr_size = std::stoull(size_str);
                    return llvm::ArrayType::get(elem_type, arr_size);
                } catch (...) {}
                // Try Literal(N) form that typeNode->toString() emits for parameter array types
                if (size_str.size() > 9 &&
                    size_str.substr(0, 8) == "Literal(" &&
                    size_str.back() == ')') {
                    std::string inner = size_str.substr(8, size_str.size() - 9);
                    try {
                        uint64_t arr_size = std::stoull(inner);
                        return llvm::ArrayType::get(elem_type, arr_size);
                    } catch (...) {}
                }
                // Fall through to default
            } else {
                // Dynamic / unsized array: int32[] -> opaque pointer
                return llvm::PointerType::get(context, 0);
            }
        }
    }

    // Default to i32 for unknown types (temporary)
    return builder.getInt32Ty();
}

} // namespace aria

// Define methods outside namespace to avoid ambiguity

llvm::Value* aria::IRGenerator::codegen(aria::ASTNode* node) {
    if (!node) {
        return nullptr;
    }
    
    // Walk the module AST and generate code for all non-generic functions
    if (node->type == ASTNode::NodeType::PROGRAM) {
        ProgramNode* program = static_cast<ProgramNode*>(node);
        
        // Process all declarations recursively (handles nested modules)
        processModuleDeclarations(program->declarations, "");
    }
    
    // Extract module base name from full path (e.g., "utils" from "tests/integration/utils.aria")
    // Use the explicitly-set current module name (set per-import in main.cpp) when available.
    // Falling back to the LLVM module name would always return the *main* input file's name,
    // causing imported modules to incorrectly generate a "main" stub that shadows the real one.
    std::string module_name;
    if (!current_module_name.empty()) {
        module_name = current_module_name;
    } else {
        module_name = module->getName().str();
        size_t last_slash = module_name.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            module_name = module_name.substr(last_slash + 1);
        }
        size_t last_dot = module_name.find_last_of('.');
        if (last_dot != std::string::npos) {
            module_name = module_name.substr(0, last_dot);
        }
    }
    
    // Create a module init function (or main if module name suggests it's the main file)
    bool is_main_module = (module_name.find("main") != std::string::npos || 
                           module_name == "hello" ||
                           module_name.find("generics") != std::string::npos ||
                           module_name.find("_test") != std::string::npos);
    std::string func_name = is_main_module ? "main" : ("__" + module_name + "_init");
    
    // Check if function already exists (we generated it from AST above)
    if (module->getFunction(func_name)) {
        // Function already generated from AST, just return it
        return module->getFunction(func_name);
    }
    
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        builder.getInt32Ty(),  // return int32
        false  // not vararg
    );
    
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        func_name,
        module.get()
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Return 0 - actual function generation happens above and via codegenSpecializedFunctions
    builder.CreateRet(builder.getInt32(0));
    
    return func;
}

llvm::Module* aria::IRGenerator::getModule() {
    return module.get();
}

std::unique_ptr<llvm::Module> aria::IRGenerator::takeModule() {
    return std::move(module);
}

void aria::IRGenerator::dump() {
    module->print(llvm::outs(), nullptr);
}

// =============================================================================
// Generic Function Code Generation
// =============================================================================

size_t aria::IRGenerator::codegenSpecializedFunctions(
    const std::vector<sema::Specialization*>& specializations) {
    
    if (specializations.empty()) {
        return 0;
    }
    
    size_t generated = 0;
    
    // =========================================================================
    // PASS 1: Forward-declare all specialized functions
    // This ensures any specialized function can reference any other specialized
    // function (transitive monomorphization), regardless of ordering.
    // =========================================================================
    struct SpecFuncInfo {
        llvm::Function* func;
        llvm::StructType* resultType;
        llvm::Type* innerType;
        const sema::Specialization* spec;
    };
    std::vector<SpecFuncInfo> specFuncs;
    
    for (const auto* spec : specializations) {
        if (!spec || !spec->funcDecl) {
            continue;
        }
        
        FuncDeclStmt* funcDecl = spec->funcDecl;
        
        // Determine inner value type using proper type mapping
        std::string innerTypeStr = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
        llvm::Type* innerType = mapTypeFromName(innerTypeStr);
        
        // Aria functions return Result<T> = {T value, error* err, i1 is_error}
        std::vector<llvm::Type*> resultFields = {
            innerType,
            llvm::PointerType::get(context, 0),
            llvm::Type::getInt8Ty(context)
        };
        llvm::StructType* resultType = llvm::StructType::get(context, resultFields);
        
        // Build parameter types with proper type mapping
        std::vector<llvm::Type*> paramTypes;
        for (const auto& param : funcDecl->parameters) {
            ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
            if (pnode->typeNode && pnode->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                paramTypes.push_back(llvm::StructType::get(context, {ptrTy, ptrTy}));
            } else {
                std::string paramTypeStr = pnode->typeNode ? pnode->typeNode->toString() : "void";
                paramTypes.push_back(mapTypeFromName(paramTypeStr));
            }
        }
        
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            resultType, paramTypes, false);
        
        // Forward-declare the function (no body yet)
        llvm::Function* func = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            spec->mangledName,
            module.get()
        );
        
        specFuncs.push_back({func, resultType, innerType, spec});
    }
    
    // =========================================================================
    // PASS 2: Generate function bodies
    // All specialized functions are now declared, so cross-references resolve.
    // =========================================================================
    for (auto& info : specFuncs) {
        const auto* spec = info.spec;
        FuncDeclStmt* funcDecl = spec->funcDecl;
        llvm::Function* func = info.func;
        llvm::StructType* resultType = info.resultType;
        llvm::Type* innerType = info.innerType;
        
        // P1-4: Track current function declaration for contract checking
        current_func_decl = funcDecl;
        
        // Create entry basic block
        llvm::BasicBlock* entryBB = llvm::BasicBlock::Create(
            context,
            "entry",
            func
        );
        builder.SetInsertPoint(entryBB);
        
        // Set parameter names and add to symbol table for codegen
        unsigned idx = 0;
        for (auto& arg : func->args()) {
            if (idx < funcDecl->parameters.size()) {
                ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[idx].get());
                arg.setName(param->paramName);

                // For array-type and vector-type parameters: store into a local alloca so that element
                // access via codegenIndex (which dyn_cast<AllocaInst>s) and extractelement work correctly.
                if (arg.getType()->isArrayTy() || arg.getType()->isVectorTy()) {
                    llvm::AllocaInst* param_alloca = builder.CreateAlloca(
                        arg.getType(), nullptr, param->paramName + "_arr_alloca");
                    builder.CreateStore(&arg, param_alloca);
                    named_values[param->paramName] = param_alloca;
                } else {
                    named_values[param->paramName] = &arg;
                }

                // Track Aria type name for UFCS method resolution
                if (param->typeNode) {
                    std::string typeStr = param->typeNode->toString();
                    var_aria_types[param->paramName] = typeStr;
                    
                    // CRITICAL FIX: Register parameter type in value_types map
                    // This enables member access on struct parameters (e.g., self.field)
                    Type* paramType = nullptr;
                    
                    // Check for vector types FIRST (before primitives, since getPrimitiveType creates entries!)
                    if (typeStr.find("vec") != std::string::npos) {
                        Type* componentType = type_system->getPrimitiveType("flt64");
                        int dimension = 2;
                        if (typeStr.find("vec2") != std::string::npos) dimension = 2;
                        else if (typeStr.find("vec3") != std::string::npos) dimension = 3;
                        else if (typeStr.find("vec9") != std::string::npos) dimension = 9;
                        if (typeStr[0] == 'd') componentType = type_system->getPrimitiveType("flt64");
                        else if (typeStr[0] == 'f') componentType = type_system->getPrimitiveType("flt32");
                        else if (typeStr[0] == 'i') componentType = type_system->getPrimitiveType("int32");
                        else if (typeStr[0] == 'v') componentType = type_system->getPrimitiveType("flt64");
                        paramType = type_system->getVectorType(componentType, dimension);
                    }
                    if (!paramType) {
                        paramType = type_system->getStructType(typeStr);
                    }
                    if (!paramType) {
                        paramType = type_system->getPrimitiveType(typeStr);
                    }
                    if (paramType) {
                        value_types[&arg] = paramType;
                        // Also register the alloca if we created one
                        if (named_values[param->paramName] != &arg) {
                            value_types[named_values[param->paramName]] = paramType;
                        }
                    }
                }

                // Emit debug info for parameter
                if (debug_enabled && di_builder && getCurrentDebugScope()) {
                    std::string typeStr = param->typeNode ? param->typeNode->toString() : "int32";
                    llvm::DIType* di_type = nullptr;
                    if (type_system) {
                        auto* aria_type_for_debug = type_system->getPrimitiveType(typeStr);
                        if (aria_type_for_debug) di_type = mapDebugType(aria_type_for_debug);
                    }
                    if (!di_type) {
                        unsigned bits = arg.getType()->getPrimitiveSizeInBits();
                        if (bits == 0) bits = module->getDataLayout().getTypeSizeInBits(arg.getType());
                        di_type = di_builder->createBasicType(typeStr, bits, llvm::dwarf::DW_ATE_signed);
                    }
                    auto* di_param = di_builder->createParameterVariable(
                        getCurrentDebugScope(), param->paramName, idx + 1, di_file,
                        funcDecl->line, di_type);
                    llvm::Value* storage = named_values[param->paramName];
                    if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(storage)) {
                        di_builder->insertDeclare(ai, di_param, di_builder->createExpression(),
                            llvm::DILocation::get(context, funcDecl->line, 0, getCurrentDebugScope()),
                            builder.GetInsertBlock());
                    } else {
                        di_builder->insertDbgValueIntrinsic(storage, di_param, di_builder->createExpression(),
                            llvm::DILocation::get(context, funcDecl->line, 0, getCurrentDebugScope()),
                            builder.GetInsertBlock());
                    }
                }
            }
            idx++;
        }
        
        // P1-4: Generate precondition checks (requires clauses)
        // Check contracts at function entry, before body executes
        if (!funcDecl->preconditions.empty()) {
            for (size_t i = 0; i < funcDecl->preconditions.size(); i++) {
                ASTNodePtr precond = funcDecl->preconditions[i];
                
                // Generate the condition expression
                llvm::Value* condValue = codegenExpression(precond.get());
                
                if (condValue) {
                    // Create basic blocks for contract pass/fail
                    llvm::BasicBlock* contractOkBB = llvm::BasicBlock::Create(context, "contract.ok", func);
                    llvm::BasicBlock* contractFailBB = llvm::BasicBlock::Create(context, "contract.fail", func);
                    
                    // Branch based on contract condition
                    builder.CreateCondBr(condValue, contractOkBB, contractFailBB);
                    
                    // Contract failed: create error Result and return
                    builder.SetInsertPoint(contractFailBB);
                    
                    // Create error message (simplified for now)
                    llvm::Value* errorMsg = builder.CreateGlobalString(
                        "Precondition #" + std::to_string(i + 1) + " failed", 
                        "contract.err.msg");
                    
                    // Create error Result: {val: undef, err: msg, is_error: true}
                    llvm::Value* errorResult = llvm::UndefValue::get(resultType);
                    errorResult = builder.CreateInsertValue(errorResult,
                        llvm::Constant::getNullValue(innerType), 0, "err.val");
                    errorResult = builder.CreateInsertValue(errorResult,
                        errorMsg, 1, "err.ptr");
                    errorResult = builder.CreateInsertValue(errorResult,
                        builder.getInt8(1), 2, "err.is_error");
                    builder.CreateRet(errorResult);
                    
                    // Contract passed: continue to next check or body
                    builder.SetInsertPoint(contractOkBB);
                }
            }
        }
        
        // Generate function body
        if (funcDecl->body) {
            llvm::Value* bodyVal = codegenStatement(funcDecl->body.get());
            (void)bodyVal;
            
            // If body didn't already create a terminator, add one
            // For Result<T> returns, create default success Result
            if (!builder.GetInsertBlock()->getTerminator()) {
                llvm::Value* defaultResult = llvm::UndefValue::get(resultType);
                defaultResult = builder.CreateInsertValue(defaultResult, 
                    llvm::Constant::getNullValue(innerType), 0, "default.val");
                defaultResult = builder.CreateInsertValue(defaultResult,
                    llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)), 1, "default.err");
                defaultResult = builder.CreateInsertValue(defaultResult,
                    builder.getInt8(0), 2, "default.is_error");
                builder.CreateRet(defaultResult);
            }
        } else {
            // No body - create default success Result
            llvm::Value* defaultResult = llvm::UndefValue::get(resultType);
            defaultResult = builder.CreateInsertValue(defaultResult, 
                llvm::Constant::getNullValue(innerType), 0, "default.val");
            defaultResult = builder.CreateInsertValue(defaultResult,
                llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)), 1, "default.err");
            defaultResult = builder.CreateInsertValue(defaultResult,
                builder.getInt8(0), 2, "default.is_error");
            builder.CreateRet(defaultResult);
        }
        
        // Clear parameter names from symbol table
        for (const auto& param : funcDecl->parameters) {
            ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
            named_values.erase(pnode->paramName);
        }
        
        // P1-4: Clear current function declaration
        current_func_decl = nullptr;
        
        generated++;
    }
    
    return generated;
}

// =============================================================================
// Module Declaration Processing (Recursive)
// =============================================================================

void aria::IRGenerator::processModuleDeclarations(const std::vector<std::shared_ptr<ASTNode>>& declarations,
                                                    const std::string& modulePrefix) {
    // -------------------------------------------------------------------------
    // GLOBAL PRE-PASS: Create LLVM GlobalVariables for all module-level VAR_DECL
    // nodes BEFORE any function bodies are generated.  Function bodies that
    // reference module-level globals rely on named_values being populated; if
    // the globals are not registered first their references silently produce
    // nullptr and the whole function body is skipped.
    // -------------------------------------------------------------------------
    for (const auto& decl : declarations) {
        if (!decl || decl->type != ASTNode::NodeType::VAR_DECL) continue;
        VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(decl.get());

        // Idempotent: skip if already registered
        if (named_values.count(varDecl->varName)) continue;

        std::string typeStr = varDecl->typeNode ? varDecl->typeNode->toString() : "int32";
        llvm::Type* varType = mapTypeFromName(typeStr);
        if (!varType) varType = llvm::Type::getInt32Ty(context);

        llvm::Constant* initVal = nullptr;
        if (varDecl->initializer && varDecl->initializer->type == ASTNode::NodeType::LITERAL) {
            LiteralExpr* lit = static_cast<LiteralExpr*>(varDecl->initializer.get());
            if (std::holds_alternative<bool>(lit->value)) {
                initVal = llvm::ConstantInt::get(varType, std::get<bool>(lit->value) ? 1 : 0);
            } else if (std::holds_alternative<int64_t>(lit->value)) {
                initVal = llvm::ConstantInt::get(varType, (uint64_t)std::get<int64_t>(lit->value), true);
            } else if (std::holds_alternative<double>(lit->value)) {
                initVal = llvm::ConstantFP::get(varType, std::get<double>(lit->value));
            } else if (std::holds_alternative<std::string>(lit->value)) {
                const std::string& s = std::get<std::string>(lit->value);
                if (s != "unknown" && s != "ERR") {
                    llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                    if (!ariaStrType) {
                        ariaStrType = llvm::StructType::create(context,
                            {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                             llvm::Type::getInt64Ty(context)},
                            "struct.AriaString");
                    }
                    llvm::Constant* strData = llvm::ConstantDataArray::getString(context, s, true);
                    llvm::GlobalVariable* strDataGV = new llvm::GlobalVariable(
                        *module, strData->getType(), true,
                        llvm::GlobalValue::PrivateLinkage, strData, ".gv_str_data");
                    std::vector<llvm::Constant*> strFields = {
                        llvm::ConstantExpr::getPointerCast(strDataGV,
                            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)),
                        llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), (uint64_t)s.length())
                    };
                    llvm::GlobalVariable* strGV = new llvm::GlobalVariable(
                        *module, ariaStrType, true,
                        llvm::GlobalValue::PrivateLinkage,
                        llvm::ConstantStruct::get(ariaStrType, strFields), ".gv_str");
                    initVal = strGV;
                }
            }
        }
        if (!initVal) {
            initVal = llvm::Constant::getNullValue(varType);
        }

        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *module, varType, varDecl->isConst,
            llvm::GlobalValue::ExternalLinkage, initVal, varDecl->varName);
        named_values[varDecl->varName] = gv;
        var_aria_types[varDecl->varName] = typeStr;
        std::cerr << "[GLOBAL PRE-PASS] Registered '" << varDecl->varName
                  << "' type=" << typeStr << std::endl;
    }

    // -------------------------------------------------------------------------
    // PRE-PASS: Forward-declare all non-generic functions so mutual recursion
    // works: when function A's body is generated and calls function B, B's
    // declaration must already exist in the LLVM module.
    // -------------------------------------------------------------------------
    for (const auto& decl : declarations) {
        if (!decl || decl->type != ASTNode::NodeType::FUNC_DECL) continue;
        FuncDeclStmt* fd = static_cast<FuncDeclStmt*>(decl.get());

        // Skip generics (specialised by monomorphisation) and already-declared
        if (!fd->genericParams.empty()) continue;
        std::string pre_name = modulePrefix.empty() ? fd->funcName
                                                     : modulePrefix + "." + fd->funcName;
        if (module->getFunction(pre_name)) continue;   // already declared

        // Build parameter type list
        std::vector<llvm::Type*> pre_params;
        for (const auto& param : fd->parameters) {
            ParameterNode* pn = static_cast<ParameterNode*>(param.get());
            if (pn->isWild) {
                pre_params.push_back(builder.getPtrTy());
            } else if (pn->typeNode && pn->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                // Function pointer params are fat pointers: {ptr, ptr} (method_ptr, env_ptr)
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                pre_params.push_back(llvm::StructType::get(context, {ptrTy, ptrTy}));
            } else {
                std::string pstr = pn->typeNode ? pn->typeNode->toString() : "void";
                pre_params.push_back(mapTypeFromName(pstr));
            }
        }

        // Build return type (Result<T> wrapper for regular functions)
        llvm::Type* pre_ret;
        if (fd->returnIsWild) {
            pre_ret = builder.getPtrTy();
        } else {
            std::string rstr = fd->returnType ? fd->returnType->toString() : "void";
            llvm::Type* inner = mapTypeFromName(rstr);
            if (!fd->body || fd->funcName == "main" || fd->isAsync || inner->isVoidTy()) {
                pre_ret = fd->isAsync
                    ? llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)
                    : inner;
            } else {
                std::vector<llvm::Type*> rf = {
                    inner,
                    llvm::PointerType::get(context, 0),
                    builder.getInt8Ty()
                };
                pre_ret = llvm::StructType::get(context, rf);
            }
        }

        // v0.2.7: main() always uses C-standard (i32, ptr) signature for argc/argv
        if (fd->funcName == "main") {
            pre_params.clear();
            pre_params.push_back(builder.getInt32Ty());  // argc
            pre_params.push_back(builder.getPtrTy());    // argv (char**)
        }

        llvm::FunctionType* pre_ft = llvm::FunctionType::get(pre_ret, pre_params, false);
        llvm::Function::Create(pre_ft, llvm::Function::ExternalLinkage, pre_name, module.get());
    }
    // -------------------------------------------------------------------------
    // MAIN PASS: Generate full function bodies (declarations already exist)
    // -------------------------------------------------------------------------

    for (const auto& decl : declarations) {
        if (!decl) continue;
        
        // Skip USE statements (structural only)
        if (decl->type == ASTNode::NodeType::USE) {
            continue;
        }
        
        // Handle MOD statements (possibly nested)
        if (decl->type == ASTNode::NodeType::MOD) {
            ModStmt* modStmt = static_cast<ModStmt*>(decl.get());
            
            // Build qualified module name
            std::string qualifiedModName = modulePrefix.empty() 
                ? modStmt->name 
                : modulePrefix + "." + modStmt->name;
            
            // Process inline module body recursively
            if (modStmt->isInline) {
                processModuleDeclarations(modStmt->body, qualifiedModName);
            }
            // External module references don't generate code
            continue;
        }
        
        // Handle ENUM_DECL statements - register enum constants before processing functions
        if (decl->type == ASTNode::NodeType::ENUM_DECL) {
            EnumDeclStmt* enumDecl = static_cast<EnumDeclStmt*>(decl.get());

            // Register each variant in the enum_constants map
            for (const auto& [variantName, variantValue] : enumDecl->variants) {
                std::string fullName = enumDecl->enumName + "." + variantName;
                enum_constants[fullName] = variantValue;
            }
            continue;
        }

        // Handle TYPE_DECL statements - Type Oriented Programming (TOP)
        // Process desugared components through normal code generation
        if (decl->type == ASTNode::NodeType::TYPE_DECL) {
            TypeDeclStmt* typeDecl = static_cast<TypeDeclStmt*>(decl.get());
            
            // The struct was already registered during type checking
            // Generate code for the methods by calling codegen on each
            std::vector<std::shared_ptr<ASTNode>> methods;
            if (typeDecl->createFunc) {
                methods.push_back(typeDecl->createFunc);
            }
            if (typeDecl->destroyFunc) {
                methods.push_back(typeDecl->destroyFunc);
            }
            for (const auto& method : typeDecl->methods) {
                methods.push_back(method);
            }
            
            // Process all methods through the same module declaration processing
            processModuleDeclarations(methods, modulePrefix);
            continue;
        }

        // Handle EXTERN statements - generate external function declarations
        if (decl->type == ASTNode::NodeType::EXTERN) {
            ExternStmt* externStmt = static_cast<ExternStmt*>(decl.get());
            
            std::cerr << "[DEBUG EXTERN] Processing extern block with " 
                      << externStmt->declarations.size() << " declarations" << std::endl;

            // Helper lambda to map FFI type names to LLVM types
            auto mapFFIType = [this](ASTNode* typeNode) -> llvm::Type* {
                if (!typeNode) return builder.getVoidTy();

                // Handle SimpleType (type name as string)
                if (typeNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    SimpleType* simpleType = static_cast<SimpleType*>(typeNode);
                    std::string typeName = simpleType->typeName;

                    // Check for pointer types (ending with *)
                    bool isPointer = false;
                    while (!typeName.empty() && typeName.back() == '*') {
                        isPointer = true;
                        typeName.pop_back();
                    }

                    // Map base type
                    llvm::Type* baseType = nullptr;
                    if (typeName == "void") baseType = builder.getVoidTy();
                    else if (typeName == "int8" || typeName == "byte") baseType = builder.getInt8Ty();
                    else if (typeName == "int16") baseType = builder.getInt16Ty();
                    else if (typeName == "int32") baseType = builder.getInt32Ty();
                    else if (typeName == "int64") baseType = builder.getInt64Ty();
                    else if (typeName == "float32" || typeName == "flt32") baseType = builder.getFloatTy();
                    else if (typeName == "float64" || typeName == "flt64") baseType = builder.getDoubleTy();
                    else {
                        // Unknown type (opaque) - treat as ptr
                        return builder.getPtrTy();
                    }

                    if (isPointer || baseType == builder.getVoidTy()) {
                        return builder.getPtrTy();
                    }
                    return baseType;
                }

                // Default to ptr for unknown types
                return builder.getPtrTy();
            };

            for (const auto& externDecl : externStmt->declarations) {
                if (externDecl->type == ASTNode::NodeType::FUNC_DECL) {
                    // Generate extern function declaration
                    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(externDecl.get());

                    // Get return type - check for wild qualifier
                    llvm::Type* returnType;
                    if (funcDecl->returnIsWild) {
                        // Wild pointers always map to ptr regardless of base type
                        returnType = builder.getPtrTy();
                    } else {
                        returnType = mapFFIType(funcDecl->returnType.get());
                    }

                    // Get parameter types - check each for wild qualifier
                    std::vector<llvm::Type*> paramTypes;
                    for (const auto& param : funcDecl->parameters) {
                        if (param->type == ASTNode::NodeType::PARAMETER) {
                            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                            llvm::Type* paramType;
                            if (paramNode->isWild) {
                                // Wild pointers always map to ptr
                                paramType = builder.getPtrTy();
                            } else {
                                paramType = mapFFIType(paramNode->typeNode.get());
                            }
                            paramTypes.push_back(paramType);
                        }
                    }

                    // Create function type
                    llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);

                    // Create or update function declaration
                    llvm::Function* func = module->getFunction(funcDecl->funcName);
                    if (!func) {
                        // Create new extern function
                        std::cerr << "[DEBUG EXTERN] Creating NEW extern function: " 
                                  << funcDecl->funcName << " with External linkage" << std::endl;
                        llvm::Function::Create(
                            funcType,
                            llvm::Function::ExternalLinkage,
                            funcDecl->funcName,
                            module.get()
                        );
                    } else {
                        // P0: Update linkage to external for existing functions
                        // This handles the case where a function was referenced before its extern declaration
                        std::cerr << "[DEBUG EXTERN] UPDATING linkage for existing function: " 
                                  << funcDecl->funcName << " from " 
                                  << (func->hasInternalLinkage() ? "Internal" : "External")
                                  << " to External" << std::endl;
                        func->setLinkage(llvm::Function::ExternalLinkage);
                    }

                    // FFI-STRING-RETURN: Track extern functions that return string type.
                    // Used by codegen_expr to wrap raw char* returns into AriaString structs.
                    if (funcDecl->returnType &&
                        funcDecl->returnType->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                        SimpleType* retType = static_cast<SimpleType*>(funcDecl->returnType.get());
                        if (retType->typeName == "string") {
                            var_aria_types["__ffi_ret_" + funcDecl->funcName] = "string";
                        }
                    }
                }
                // OPAQUE_STRUCT and STRUCT_DECL in extern don't need special handling here
                // They're used for type resolution which happens at call sites
            }
            continue;
        }

        // Handle module-level VAR_DECL (global variables and constants).
        // These become LLVM GlobalVariable objects accessible to all functions in the
        // module. Without this, any function that references a module-level global would
        // find nothing in named_values and silently produce broken/empty IR.
        if (decl->type == ASTNode::NodeType::VAR_DECL) {
            VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(decl.get());

            // Skip if already registered (e.g. pre-pass or repeated codegen call)
            if (named_values.count(varDecl->varName)) continue;

            // Determine LLVM type from the declared Aria type
            std::string typeStr = varDecl->typeNode ? varDecl->typeNode->toString() : "int32";
            llvm::Type* varType = mapTypeFromName(typeStr);
            if (!varType) varType = llvm::Type::getInt32Ty(context);

            // Compute a constant initializer from the literal, if present.
            // Module-level initializers must be compile-time constants (LLVM requirement).
            llvm::Constant* initVal = nullptr;
            if (varDecl->initializer && varDecl->initializer->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(varDecl->initializer.get());
                if (std::holds_alternative<bool>(lit->value)) {
                    initVal = llvm::ConstantInt::get(varType, std::get<bool>(lit->value) ? 1 : 0);
                } else if (std::holds_alternative<int64_t>(lit->value)) {
                    initVal = llvm::ConstantInt::get(varType, (uint64_t)std::get<int64_t>(lit->value), true);
                } else if (std::holds_alternative<double>(lit->value)) {
                    initVal = llvm::ConstantFP::get(varType, std::get<double>(lit->value));
                } else if (std::holds_alternative<std::string>(lit->value)) {
                    // String global: create a constant AriaString and use its address as init.
                    const std::string& s = std::get<std::string>(lit->value);
                    if (s != "unknown" && s != "ERR") {
                        llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                        if (!ariaStrType) {
                            ariaStrType = llvm::StructType::create(context,
                                {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                                 llvm::Type::getInt64Ty(context)},
                                "struct.AriaString");
                        }
                        llvm::Constant* strData = llvm::ConstantDataArray::getString(context, s, true);
                        llvm::GlobalVariable* strDataGV = new llvm::GlobalVariable(
                            *module, strData->getType(), true,
                            llvm::GlobalValue::PrivateLinkage, strData, ".gv_str_data");
                        std::vector<llvm::Constant*> strFields = {
                            llvm::ConstantExpr::getPointerCast(strDataGV,
                                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)),
                            llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), (uint64_t)s.length())
                        };
                        llvm::GlobalVariable* strGV = new llvm::GlobalVariable(
                            *module, ariaStrType, true,
                            llvm::GlobalValue::PrivateLinkage,
                            llvm::ConstantStruct::get(ariaStrType, strFields), ".gv_str");
                        initVal = strGV;  // ptr to global AriaString
                    }
                }
            }
            if (!initVal) {
                initVal = llvm::Constant::getNullValue(varType);
            }

            // Create the LLVM GlobalVariable with ExternalLinkage so cross-module
            // calls can resolve it; mark const if declared const in Aria source.
            llvm::GlobalVariable* gv = new llvm::GlobalVariable(
                *module, varType, varDecl->isConst,
                llvm::GlobalValue::ExternalLinkage, initVal, varDecl->varName);

            // Register in runtime symbol tables
            named_values[varDecl->varName] = gv;
            var_aria_types[varDecl->varName] = typeStr;
            std::cerr << "[GLOBAL VAR] Registered '" << varDecl->varName
                      << "' type=" << typeStr << std::endl;
            continue;
        }

        // Handle FUNC_DECL statements
        if (decl->type == ASTNode::NodeType::FUNC_DECL) {
            FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());
            
            // P1-4: Track current function declaration for contract checking
            current_func_decl = funcDecl;
            
            // Skip generic functions (handled by monomorphization)
            if (!funcDecl->genericParams.empty()) {
                current_func_decl = nullptr;
                continue;
            }
            
            // Create function signature with proper type mapping
            std::vector<llvm::Type*> param_types;
            for (const auto& param : funcDecl->parameters) {
                ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                
                // Check for wild qualifier (FFI pointers)
                if (pnode->isWild) {
                    // Wild pointers always map to LLVM ptr regardless of base type
                    param_types.push_back(builder.getPtrTy());
                } else if (pnode->typeNode && pnode->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                    // Function pointer params are fat pointers: {ptr, ptr} (method_ptr, env_ptr)
                    llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                    param_types.push_back(llvm::StructType::get(context, {ptrTy, ptrTy}));
                } else {
                    std::string paramTypeStr = pnode->typeNode ? pnode->typeNode->toString() : "void";
                    param_types.push_back(mapTypeFromName(paramTypeStr));
                }
            }
            
            // Get the declared return type
            llvm::Type* value_type;
            if (funcDecl->returnIsWild) {
                // Wild pointers always map to LLVM ptr regardless of base type
                value_type = builder.getPtrTy();
            } else {
                std::string returnTypeStr = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
                value_type = mapTypeFromName(returnTypeStr);
            }
            
            // All Aria functions return Result<T> except:
            // 1. extern functions (no body)
            // 2. main function (C ABI compatibility)
            // 3. void functions (void type should only exist in extern, but legacy code uses it)
            llvm::Type* actual_return_type;
            if (!funcDecl->body || funcDecl->funcName == "main" || value_type->isVoidTy()) {
                // Extern functions, main, and void functions return raw type
                actual_return_type = value_type;
            } else {
                // Regular functions return Result{value, error*, is_error}
                std::vector<llvm::Type*> result_fields = {
                    value_type,                         // value (field 0)
                    llvm::PointerType::get(context, 0), // error (field 1) - void*
                    builder.getInt8Ty()                 // is_error (field 2) - i8
                };
                actual_return_type = llvm::StructType::get(context, result_fields);
            }
            
            // Phase 4.6: Async functions return i8* (coroutine handle)
            // Save the Result<T> type for promise-based value passing before override
            if (funcDecl->isAsync) {
                current_async_result_type = actual_return_type;  // Result{T, ptr, i8}
                actual_return_type = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
                std::cerr << "[DEBUG] Async function detected: " << funcDecl->funcName << ", changing return type to i8*" << std::endl;
            }
            
            // v0.2.7: main() always uses C-standard (i32, ptr) signature for argc/argv
            // User-declared parameters are mapped to these LLVM args; if none declared,
            // the args still exist but aren't bound to named variables.
            std::vector<llvm::Type*> actual_param_types = param_types;
            if (funcDecl->funcName == "main") {
                actual_param_types.clear();
                actual_param_types.push_back(builder.getInt32Ty());  // argc
                actual_param_types.push_back(builder.getPtrTy());    // argv (char**)
            }

            llvm::FunctionType* func_type = llvm::FunctionType::get(actual_return_type, actual_param_types, false);
            
            // Determine function name (qualified if in a module)
            std::string func_name = modulePrefix.empty() 
                ? funcDecl->funcName 
                : modulePrefix + "." + funcDecl->funcName;
            
            // Determine linkage based on pub modifier
            // Special case: main function and module init always have external linkage
            // Temporary: Until pub keyword is implemented, all functions in modules are external
            llvm::Function::LinkageTypes linkage;
            if (funcDecl->isExtern || funcDecl->funcName == "main" || funcDecl->funcName.find("__") == 0) {
                // Extern functions, main, and module init functions are always external
                linkage = llvm::Function::ExternalLinkage;
            } else if (funcDecl->isGPUKernel) {
                // GPU/PTX Phase 3: GPU kernels MUST have external linkage
                // Host code needs to find kernel entry points for cudaLaunchKernel()
                linkage = llvm::Function::ExternalLinkage;
            } else if (!current_module_name.empty()) {
                // Functions in modules are external (until pub keyword is implemented)
                // This allows cross-module function calls
                linkage = llvm::Function::ExternalLinkage;
            } else {
                // Regular functions use pub modifier
                linkage = funcDecl->isPublic 
                    ? llvm::Function::ExternalLinkage 
                    : llvm::Function::InternalLinkage;
            }
            
            // Create LLVM function (reuse forward declaration if pre-pass created it)
            llvm::Function* func = module->getFunction(func_name);
            if (!func) {
                func = llvm::Function::Create(
                    func_type,
                    linkage,
                    func_name,
                    module.get()
                );
            }

            // Phase 4.6: Mark async functions with presplitcoroutine attribute
            // This tells CoroSplit that this function contains coroutine intrinsics
            if (funcDecl->isAsync) {
                func->addFnAttr(llvm::Attribute::PresplitCoroutine);
                // Set aria.async metadata so await expressions can verify async context
                llvm::MDNode* async_md = llvm::MDNode::get(context,
                    llvm::ConstantAsMetadata::get(
                        llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1)));
                func->setMetadata("aria.async", async_md);
            }
            
            // GPU/PTX Backend - Phase 3: Add NVVM kernel metadata for GPU kernels
            // This tells NVPTX backend to emit ".entry" instead of ".func"
            if (funcDecl->isGPUKernel) {
                // Create metadata: {function, "kernel", i32 1}
                llvm::Metadata* md_args[] = {
                    llvm::ValueAsMetadata::get(func),
                    llvm::MDString::get(context, "kernel"),
                    llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(
                        llvm::Type::getInt32Ty(context), 1))
                };
                llvm::MDNode* kernel_md = llvm::MDNode::get(context, md_args);
                
                // Get or create nvvm.annotations named metadata
                llvm::NamedMDNode* nvvm_annotations = 
                    module->getOrInsertNamedMetadata("nvvm.annotations");
                nvvm_annotations->addOperand(kernel_md);
            }

            // v0.2.12: Inline/noinline function attributes
            if (funcDecl->isInline) {
                func->addFnAttr(llvm::Attribute::InlineHint);
                func->addFnAttr(llvm::Attribute::AlwaysInline);
            }
            if (funcDecl->isNoInline) {
                func->addFnAttr(llvm::Attribute::NoInline);
            }

            // Skip body generation if no body (extern declaration)
            if (!funcDecl->body) {
                // FFI-STRING-RETURN: Track standalone extern functions that return string type.
                // This handles `extern func:name = string(...)` syntax (not in extern block).
                if (funcDecl->isExtern && funcDecl->returnType &&
                    funcDecl->returnType->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    SimpleType* retType = static_cast<SimpleType*>(funcDecl->returnType.get());
                    if (retType->typeName == "string") {
                        var_aria_types["__ffi_ret_" + funcDecl->funcName] = "string";
                    }
                }
                continue;
            }
            
            // Create entry block
            llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
            builder.SetInsertPoint(entry);
            
            // Create debug info for this function
            if (debug_enabled && di_builder && di_file) {
                // Create subroutine type (simplified - just return type for now)
                llvm::SmallVector<llvm::Metadata*, 8> di_param_types;
                di_param_types.push_back(nullptr); // return type placeholder
                llvm::DISubroutineType* di_func_type = di_builder->createSubroutineType(
                    di_builder->getOrCreateTypeArray(di_param_types));
                
                llvm::DISubprogram* di_sp = di_builder->createFunction(
                    getCurrentDebugScope() ? getCurrentDebugScope() : di_file,
                    funcDecl->funcName,      // name
                    func_name,               // linkage name
                    di_file,
                    funcDecl->line,          // line number
                    di_func_type,
                    funcDecl->line,          // scope line
                    llvm::DINode::FlagPrototyped,
                    llvm::DISubprogram::SPFlagDefinition
                );
                func->setSubprogram(di_sp);
                pushDebugScope(di_sp);
                setDebugLocation(funcDecl->line, funcDecl->column);
            }
            
            // Initialize GC for main function before any user code runs
            if (funcDecl->funcName == "main") {
                // Declare aria_gc_init(size_t nursery_size, size_t old_gen_threshold)
                llvm::FunctionType* gc_init_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {builder.getInt64Ty(), builder.getInt64Ty()},
                    false
                );
                llvm::Function* gc_init = llvm::dyn_cast<llvm::Function>(
                    module->getOrInsertFunction("aria_gc_init", gc_init_type).getCallee()
                );
                
                // Call aria_gc_init(0, 0) to use default sizes (4MB nursery, 64MB old gen)
                builder.CreateCall(gc_init, {builder.getInt64(0), builder.getInt64(0)});

                // v0.2.7: Initialize command-line argument storage
                // aria_args_init(int32_t argc, char** argv)
                llvm::FunctionType* args_init_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {builder.getInt32Ty(), builder.getPtrTy()},
                    false
                );
                llvm::Function* args_init = llvm::dyn_cast<llvm::Function>(
                    module->getOrInsertFunction("aria_args_init", args_init_type).getCallee()
                );
                // main's first two LLVM args are always argc (i32) and argv (ptr)
                auto main_arg_iter = func->arg_begin();
                llvm::Value* argc_val = &*main_arg_iter++;
                llvm::Value* argv_val = &*main_arg_iter;
                builder.CreateCall(args_init, {argc_val, argv_val});

                // v0.2.7: Initialize six-stream I/O (FDs 0-5)
                // aria_streams_init(void)
                llvm::FunctionType* streams_init_type = llvm::FunctionType::get(
                    builder.getVoidTy(), false
                );
                llvm::Function* streams_init = llvm::dyn_cast<llvm::Function>(
                    module->getOrInsertFunction("aria_streams_init", streams_init_type).getCallee()
                );
                builder.CreateCall(streams_init, {});
            }
            
            // Phase 4.6: Async function coroutine setup
            llvm::Value* coro_id = nullptr;
            llvm::Value* coro_handle = nullptr;
            llvm::BasicBlock* coro_suspend_block = nullptr;
            llvm::BasicBlock* coro_cleanup_block = nullptr;
            
            if (funcDecl->isAsync) {
                // 0. Allocate promise area for Result<T> in the coroutine frame
                current_async_promise = builder.CreateAlloca(
                    current_async_result_type, nullptr, "coro.promise");
                current_async_promise->setAlignment(llvm::Align(8));

                // 1. Generate coroutine ID: token @llvm.coro.id(i32 align, i8* promise, i8* coroaddr, i8* fnaddr)
                llvm::Function* coroIdFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_id
                );
                
                llvm::Value* align = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 8);
                llvm::Value* null_ptr = llvm::ConstantPointerNull::get(
                    llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)
                );
                
                coro_id = builder.CreateCall(
                    coroIdFunc,
                    {align, current_async_promise, null_ptr, null_ptr},
                    "coro.id"
                );
                
                // 2. Get coroutine frame size
                llvm::Function* coroSizeFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_size,
                    {llvm::Type::getInt64Ty(context)}
                );
                llvm::Value* coro_size = builder.CreateCall(coroSizeFunc, {}, "coro.size");
                
                // 3. Allocate coroutine frame using malloc (not GC — coro frames
                //    have explicit lifetime managed by coro.destroy)
                llvm::Function* coro_alloc_func = module->getFunction("malloc");
                if (!coro_alloc_func) {
                    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        {llvm::Type::getInt64Ty(context)},
                        false
                    );
                    coro_alloc_func = llvm::Function::Create(
                        malloc_type,
                        llvm::Function::ExternalLinkage,
                        "malloc",
                        module.get()
                    );
                }
                llvm::Value* coro_mem = builder.CreateCall(coro_alloc_func, {coro_size}, "coro.alloc");
                
                // 4. Begin coroutine
                llvm::Function* coroBeginFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_begin
                );
                coro_handle = builder.CreateCall(coroBeginFunc, {coro_id, coro_mem}, "coro.handle");

                // Create blocks for coroutine structure
                llvm::BasicBlock* init_suspend_block = llvm::BasicBlock::Create(context, "coro.init.suspend", func);
                llvm::BasicBlock* coro_body_block = llvm::BasicBlock::Create(context, "coro.body", func);
                coro_suspend_block = llvm::BasicBlock::Create(context, "coro.final.suspend", func);
                coro_cleanup_block = llvm::BasicBlock::Create(context, "coro.cleanup", func);

                // 5. Initial suspend (required by LLVM coroutine lowering)
                llvm::Function* coroSaveFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_save
                );
                llvm::Value* init_save_token = builder.CreateCall(coroSaveFunc, {coro_handle}, "coro.init.save");

                llvm::Function* coroSuspendFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_suspend
                );
                llvm::Value* init_is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);  // false = not final
                llvm::Value* init_suspend_result = builder.CreateCall(
                    coroSuspendFunc,
                    {init_save_token, init_is_final},
                    "coro.init.suspend.result"
                );

                // Switch on initial suspend result:
                //   -1 (255 unsigned) -> suspend (return handle)
                //    0 -> resume (continue to body)
                //    1 -> cleanup (destroy immediately)
                llvm::SwitchInst* init_switch = builder.CreateSwitch(init_suspend_result, init_suspend_block, 2);
                init_switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), coro_body_block);
                init_switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1), coro_cleanup_block);

                // Initial suspend block: return handle (coroutine is lazy - suspended until resumed)
                builder.SetInsertPoint(init_suspend_block);
                builder.CreateRet(coro_handle);

                // Continue building in body block
                builder.SetInsertPoint(coro_body_block);

                // Set async context for return statement handling
                current_func_is_async = true;
                current_coro_suspend_block = coro_suspend_block;
            }
            
            // Map parameters to symbol table
            size_t idx = 0;
            for (auto& arg : func->args()) {
                if (idx < funcDecl->parameters.size()) {
                    ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[idx].get());
                    arg.setName(param->paramName);
                    
                    // CRITICAL FIX: Struct parameters must be copied to allocas!
                    // LLVM's ABI may pass structs in registers or via hidden pointer.
                    // To ensure consistent access semantics, we copy all struct params to stack allocas.
                    // This matches Clang's behavior and ensures member access works correctly.
                    llvm::Value* param_storage = nullptr;
                    if (arg.getType()->isStructTy() || arg.getType()->isArrayTy() || arg.getType()->isVectorTy()) {
                        // Struct, array, and vector parameters must be copied to a local alloca.
                        // Structs: LLVM ABI may pass in registers or via hidden pointer.
                        // Arrays: cannot be GEP'd as values — need an addressable alloca
                        //         so that element access (arr[i]) can use two-index inbounds GEP.
                        // Vectors: extractelement needs an addressable alloca, not a raw register arg.
                        if (arg.getType()->isArrayTy()) {
                            std::cerr << "[DEBUG_ARRAY_PARAM] Creating alloca for array parameter: " << param->paramName << std::endl;
                        } else {
                            std::cerr << "[DEBUG_STRUCT_PARAM] Creating alloca for struct parameter: " << param->paramName << std::endl;
                        }
                        
                        // Create alloca at function entry for the parameter
                        llvm::AllocaInst* param_alloca = builder.CreateAlloca(arg.getType(), nullptr, param->paramName + "_alloca");
                        
                        // Copy the argument value into the alloca
                        builder.CreateStore(&arg, param_alloca);
                        
                        // Store the ALLOCA (not the argument) in named_values
                        // This ensures element access loads from a properly-typed alloca
                        param_storage = param_alloca;
                        named_values[param->paramName] = param_alloca;
                    } else {
                        // For non-struct/non-array types (primitives, pointers), store argument directly
                        param_storage = &arg;
                        named_values[param->paramName] = &arg;
                    }

                    // Track Aria type name for UFCS method resolution
                    if (param->typeNode) {
                        std::string typeStr = param->typeNode->toString();
                        
                        // FUNCTION_TYPE params: register as "func_ptr:retType" so closure
                        // call path in codegen_expr recognizes them as callable fat pointers
                        if (param->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                            aria::FunctionType* fnType = static_cast<aria::FunctionType*>(param->typeNode.get());
                            std::string retStr = fnType->returnType ? fnType->returnType->toString() : "void";
                            var_aria_types[param->paramName] = "func_ptr:" + retStr;
                        } else {
                            var_aria_types[param->paramName] = typeStr;
                        }
                        
                        std::cerr << "[DEBUG] Registering parameter '" << param->paramName << "' with type '" << typeStr << "'" << std::endl;
                        
                        // CRITICAL FIX: Register parameter type in value_types map
                        // This enables member access on struct parameters (e.g., self.field, result.value)
                        
                        // PHASE 4 FIX: Try to resolve as aria::sema::Type first
                        aria::sema::Type* paramType = nullptr;
                        
                        // Check for vector types FIRST (before primitives, since getPrimitiveType creates entries!)
                        if (typeStr.find("vec") != std::string::npos) {
                            aria::sema::Type* componentType = type_system->getPrimitiveType("flt64");
                            int dimension = 2;
                            if (typeStr.find("vec2") != std::string::npos) dimension = 2;
                            else if (typeStr.find("vec3") != std::string::npos) dimension = 3;
                            else if (typeStr.find("vec9") != std::string::npos) dimension = 9;
                            if (typeStr[0] == 'd') componentType = type_system->getPrimitiveType("flt64");
                            else if (typeStr[0] == 'f') componentType = type_system->getPrimitiveType("flt32");
                            else if (typeStr[0] == 'i') componentType = type_system->getPrimitiveType("int32");
                            else if (typeStr[0] == 'v') componentType = type_system->getPrimitiveType("flt64");
                            paramType = type_system->getVectorType(componentType, dimension);
                        }
                        // PHASE 4 CRITICAL FIX: Check for Result<T> BEFORE primitive type!
                        // getPrimitiveType() accepts any string and creates a PrimitiveType,
                        // so "Result<int32>" would wrongly become PRIMITIVE instead of RESULT
                        if (typeStr.find("Result<") == 0) {
                            std::cerr << "[DEBUG] Detected Result type parameter: " << typeStr << std::endl;
                            // Extract inner type: "Result<int32>" -> "int32"
                            size_t start = typeStr.find('<') + 1;
                            size_t end = typeStr.find('>');
                            if (end != std::string::npos && end > start) {
                                std::string innerTypeStr = typeStr.substr(start, end - start);
                                std::cerr << "[DEBUG] Result inner type: " << innerTypeStr << std::endl;
                                aria::sema::Type* innerType = type_system->getPrimitiveType(innerTypeStr);
                                if (!innerType) {
                                    innerType = type_system->getStructType(innerTypeStr);
                                }
                                if (innerType) {
                                    std::cerr << "[DEBUG] Found inner type, calling getResultType()" << std::endl;
                                    paramType = type_system->getResultType(innerType);
                                    if (paramType) {
                                        std::cerr << "[DEBUG] getResultType() returned type with kind: " << static_cast<int>(paramType->getKind()) << std::endl;
                                    } else {
                                        std::cerr << "[DEBUG] ERROR: getResultType() returned nullptr!" << std::endl;
                                    }
                                } else {
                                    std::cerr << "[DEBUG] ERROR: Could not find inner type: " << innerTypeStr << std::endl;
                                }
                            }
                        }
                        
                        // Try struct type (if not already resolved as Result)
                        if (!paramType) {
                            paramType = type_system->getStructType(typeStr);
                            if (paramType) {
                                std::cerr << "[DEBUG] Found as struct type" << std::endl;
                            }
                        }
                        
                        // Try primitive type (last resort - only for actual primitives like int32)
                        if (!paramType) {
                            paramType = type_system->getPrimitiveType(typeStr);
                            if (paramType) {
                                std::cerr << "[DEBUG] Found as primitive type" << std::endl;
                            }
                        }
                        
                        if (paramType) {
                            // CRITICAL: Register BOTH the argument AND the alloca (if created) in value_types
                            value_types[&arg] = paramType;
                            if (param_storage != &arg) {
                                // For struct params, also register the alloca
                                value_types[param_storage] = paramType;
                            }
                            std::cerr << "[DEBUG] Registered '" << param->paramName << "' in value_types" << std::endl;
                        } else {
                            std::cerr << "[DEBUG] WARNING: Could not register type for '" << param->paramName << "'" << std::endl;
                        }
                    }
                }
                idx++;
            }

            // P1-4: Generate precondition checks (requires clauses)
            if (!funcDecl->preconditions.empty()) {
                for (size_t i = 0; i < funcDecl->preconditions.size(); i++) {
                    ASTNodePtr precond = funcDecl->preconditions[i];
                    
                    llvm::Value* condValue = codegenExpression(precond.get());
                    
                    if (condValue) {
                        llvm::BasicBlock* contractOkBB = llvm::BasicBlock::Create(context, "contract.ok", func);
                        llvm::BasicBlock* contractFailBB = llvm::BasicBlock::Create(context, "contract.fail", func);
                        
                        builder.CreateCondBr(condValue, contractOkBB, contractFailBB);
                        
                        builder.SetInsertPoint(contractFailBB);
                        
                        if (actual_return_type->isStructTy()) {
                            llvm::Value* errorMsg = builder.CreateGlobalString(
                                "Precondition #" + std::to_string(i + 1) + " failed in " + func_name);
                            llvm::Value* errorResult = llvm::UndefValue::get(actual_return_type);
                            errorResult = builder.CreateInsertValue(errorResult,
                                llvm::Constant::getNullValue(actual_return_type->getStructElementType(0)), 0);
                            errorResult = builder.CreateInsertValue(errorResult, errorMsg, 1);
                            errorResult = builder.CreateInsertValue(errorResult, builder.getInt8(1), 2);
                            builder.CreateRet(errorResult);
                        }
                        
                        builder.SetInsertPoint(contractOkBB);
                    }
                }
            }

            // Generate function body
            std::cerr << "[DEBUG] Generating body for function: " << func_name << std::endl;
            if (!funcDecl->body) {
                std::cerr << "[DEBUG] WARNING: Function " << func_name << " has NULL body!" << std::endl;
            } else {
                std::cerr << "[DEBUG] Function " << func_name << " body node type: " << static_cast<int>(funcDecl->body->type) << std::endl;
            }
            
            llvm::Value* bodyVal = codegenStatement(funcDecl->body.get());
            (void)bodyVal;  // Suppress unused warning
            
            // Add terminator if missing
            if (!builder.GetInsertBlock()->getTerminator()) {
                if (funcDecl->isAsync) {
                    // Async function: Jump to final suspend instead of returning directly
                    builder.CreateBr(coro_suspend_block);
                } else if (actual_return_type->isVoidTy()) {
                    builder.CreateRetVoid();
                } else {
                    // CRITICAL FIX: Check if return type is struct (Result<T>)
                    if (actual_return_type->isStructTy()) {
                        // Create a zero-initialized Result struct for default return
                        llvm::Constant* zero_result = llvm::ConstantStruct::get(
                            llvm::cast<llvm::StructType>(actual_return_type),
                            {
                                llvm::Constant::getNullValue(actual_return_type->getStructElementType(0)), // value
                                llvm::Constant::getNullValue(actual_return_type->getStructElementType(1)), // error*
                                llvm::ConstantInt::get(actual_return_type->getStructElementType(2), 0)     // is_error = false
                            }
                        );
                        builder.CreateRet(zero_result);
                    } else {
                        // Integer/pointer return type
                        builder.CreateRet(llvm::ConstantInt::get(actual_return_type, 0));
                    }
                }
            }
            
            // Phase 4.6: Generate coroutine suspend and cleanup blocks for async functions
            if (funcDecl->isAsync) {
                // Final suspend point (where all returns converge)
                builder.SetInsertPoint(coro_suspend_block);
                
                // Save coroutine state before final suspend
                llvm::Function* coroSaveFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_save
                );
                llvm::Value* save_token = builder.CreateCall(coroSaveFunc, {coro_handle}, "coro.save");
                
                // Final suspend: i8 @llvm.coro.suspend(token save, i1 final)
                llvm::Function* coroSuspendFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_suspend
                );
                llvm::Value* is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
                llvm::Value* suspend_result = builder.CreateCall(
                    coroSuspendFunc,
                    {save_token, is_final},
                    "coro.suspend.result"
                );
                
                // Switch on final suspend result:
                // Case 0 (default): suspended — frame stays alive for caller to
                //   read the promise, then coro.destroy frees it.
                // Case 1: destroy was called — go to cleanup to free the frame.
                llvm::BasicBlock* coro_ret_block = llvm::BasicBlock::Create(
                    context, "coro.ret", func);
                llvm::SwitchInst* suspend_switch = builder.CreateSwitch(suspend_result, coro_ret_block, 2);
                // 0 -> ret (suspended, frame alive)
                suspend_switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 0), coro_ret_block);
                // 1 -> cleanup (destroy called, free frame)
                suspend_switch->addCase(llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1), coro_cleanup_block);

                // Cleanup block: free coroutine frame (only reached via coro.destroy)
                builder.SetInsertPoint(coro_cleanup_block);
                
                // Get memory to free
                llvm::Function* coroFreeFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_free
                );
                llvm::Value* free_mem = builder.CreateCall(coroFreeFunc, {coro_id, coro_handle}, "coro.free.mem");
                
                // Free the memory (direct free — matches malloc in coro setup)
                llvm::Function* coro_free_func = module->getFunction("free");
                if (!coro_free_func) {
                    llvm::FunctionType* free_type = llvm::FunctionType::get(
                        llvm::Type::getVoidTy(context),
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
                        false
                    );
                    coro_free_func = llvm::Function::Create(
                        free_type,
                        llvm::Function::ExternalLinkage,
                        "free",
                        module.get()
                    );
                }
                builder.CreateCall(coro_free_func, {free_mem});
                // Fall through to coro.ret
                builder.CreateBr(coro_ret_block);
                
                // Return block: end coroutine and return handle
                builder.SetInsertPoint(coro_ret_block);
                llvm::Function* coroEndFunc = llvm::Intrinsic::getOrInsertDeclaration(
                    module.get(),
                    llvm::Intrinsic::coro_end
                );
                llvm::Value* unwind = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
                llvm::Value* token_none = llvm::ConstantTokenNone::get(context);
                builder.CreateCall(coroEndFunc, {coro_handle, unwind, token_none});
                
                // Return coroutine handle
                builder.CreateRet(coro_handle);
            }

            // Reset async context after function generation
            current_func_is_async = false;
            current_coro_suspend_block = nullptr;
            current_async_promise = nullptr;
            current_async_result_type = nullptr;

            // Pop debug scope for this function
            if (debug_enabled && di_builder && di_file) {
                popDebugScope();
                clearDebugLocation();
            }

            // P1-4: Clear current function declaration
            current_func_decl = nullptr;

            // Clean up symbol table
            for (const auto& param : funcDecl->parameters) {
                ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                named_values.erase(pnode->paramName);
            }
        }

        // WP 005: Handle TRAIT_DECL statements
        // Trait declarations are compile-time only - no IR generated
        if (decl->type == ASTNode::NodeType::TRAIT_DECL) {
            continue;
        }

        // WP 005: Handle IMPL_DECL statements
        // Generate mangled functions for each method: TypeName_methodName
        if (decl->type == ASTNode::NodeType::IMPL_DECL) {
            ImplDeclStmt* impl = static_cast<ImplDeclStmt*>(decl.get());

            for (const auto& methodNode : impl->methods) {
                FuncDeclStmt* funcDecl = dynamic_cast<FuncDeclStmt*>(methodNode.get());
                if (!funcDecl) continue;

                // Mangle name: TypeName_methodName
                std::string mangledName = impl->typeName + "_" + funcDecl->funcName;

                // Create function signature
                std::vector<llvm::Type*> param_types;
                for (const auto& param : funcDecl->parameters) {
                    ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                    std::string paramTypeStr = pnode->typeNode ? pnode->typeNode->toString() : "void";
                    param_types.push_back(mapTypeFromName(paramTypeStr));
                }

                std::string returnTypeStr = funcDecl->returnType ? funcDecl->returnType->toString() : "void";
                llvm::Type* inner_return_type = mapTypeFromName(returnTypeStr);

                // Impl methods return Result<T> just like regular functions
                llvm::Type* return_type;
                llvm::StructType* result_struct_type = nullptr;
                if (inner_return_type->isVoidTy()) {
                    return_type = inner_return_type;
                } else {
                    std::vector<llvm::Type*> result_fields = {
                        inner_return_type,
                        llvm::PointerType::get(context, 0),
                        llvm::Type::getInt1Ty(context)
                    };
                    result_struct_type = llvm::StructType::get(context, result_fields);
                    return_type = result_struct_type;
                }

                llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);

                // Create function with mangled name
                llvm::Function* func = llvm::Function::Create(
                    func_type,
                    llvm::Function::ExternalLinkage,
                    mangledName,
                    module.get()
                );

                // Skip if no body
                if (!funcDecl->body) {
                    continue;
                }

                // Create entry block
                llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
                builder.SetInsertPoint(entry);

                // Map parameters to symbol table
                size_t idx = 0;
                for (auto& arg : func->args()) {
                    if (idx < funcDecl->parameters.size()) {
                        ParameterNode* param = static_cast<ParameterNode*>(funcDecl->parameters[idx].get());
                        arg.setName(param->paramName);

                        // Create alloca for parameter
                        std::string paramTypeStr = param->typeNode ? param->typeNode->toString() : "void";
                        llvm::Type* paramType = mapTypeFromName(paramTypeStr);
                        llvm::AllocaInst* alloca = builder.CreateAlloca(paramType, nullptr, param->paramName);
                        builder.CreateStore(&arg, alloca);
                        named_values[param->paramName] = alloca;

                        // Track Aria type name for UFCS method resolution
                        var_aria_types[param->paramName] = paramTypeStr;

                        // Register in value_types so member access (self.field)
                        // can look up the struct layout
                        if (type_system) {
                            Type* ariaParamType = type_system->getStructType(paramTypeStr);
                            if (!ariaParamType) ariaParamType = type_system->getPrimitiveType(paramTypeStr);
                            if (ariaParamType) {
                                value_types[alloca] = ariaParamType;
                            }
                        }
                    }
                    idx++;
                }

                // Generate function body
                std::cerr << "[DEBUG] Generating impl method: " << mangledName << std::endl;
                codegenStatement(funcDecl->body.get());

                // Add terminator if missing
                if (!builder.GetInsertBlock()->getTerminator()) {
                    if (return_type->isVoidTy()) {
                        builder.CreateRetVoid();
                    } else if (result_struct_type) {
                        // Return default success Result{zero, null, false}
                        llvm::Value* defaultResult = llvm::UndefValue::get(result_struct_type);
                        defaultResult = builder.CreateInsertValue(defaultResult,
                            llvm::Constant::getNullValue(inner_return_type), 0);
                        defaultResult = builder.CreateInsertValue(defaultResult,
                            llvm::Constant::getNullValue(llvm::PointerType::get(context, 0)), 1);
                        defaultResult = builder.CreateInsertValue(defaultResult,
                            builder.getInt8(0), 2);
                        builder.CreateRet(defaultResult);
                    } else {
                        builder.CreateRet(llvm::Constant::getNullValue(return_type));
                    }
                }

                // Clean up symbol table
                for (const auto& param : funcDecl->parameters) {
                    ParameterNode* pnode = static_cast<ParameterNode*>(param.get());
                    named_values.erase(pnode->paramName);
                }
            }
            continue;
        }
    }
}

// =============================================================================
// Statement Code Generation
// =============================================================================

llvm::Value* aria::IRGenerator::codegenStatement(ASTNode* stmt) {
    if (!stmt) {
        return nullptr;
    }

    // Emit debug location for every statement
    if (stmt->line > 0) {
        setDebugLocation(stmt->line, stmt->column);
    }

    // ARIA-022: Prevent stack overflow from deeply nested statements
    if (++codegen_depth_ > MAX_CODEGEN_DEPTH) {
        --codegen_depth_;
        throw std::runtime_error("Codegen depth limit exceeded (256 levels). "
                                "Reduce nesting depth in your code.");
    }

    // RAII-style depth decrement on scope exit
    struct DepthGuard {
        size_t& depth;
        DepthGuard(size_t& d) : depth(d) {}
        ~DepthGuard() { --depth; }
    } guard(codegen_depth_);

    switch (stmt->type) {
        case ASTNode::NodeType::BLOCK: {
            // Block statement - generate code for each statement
            BlockStmt* block = static_cast<BlockStmt*>(stmt);
            
            std::cerr << "[DEBUG IR_GEN] BLOCK: " << block->statements.size() << " statements" << std::endl;

            // ARIA-023: Skip defer scope management when executing defers
            // to prevent iterator invalidation from vector reallocation
            bool manage_defer_scope = !executing_defers;

            if (manage_defer_scope) {
                // Push new defer scope for this block
                defer_stack.push_back(std::vector<BlockStmt*>());
            }

            llvm::Value* lastVal = nullptr;
            for (const auto& s : block->statements) {
                std::cerr << "[DEBUG IR_GEN] BLOCK: processing statement type " << static_cast<int>(s->type) << std::endl;
                lastVal = codegenStatement(s.get());
            }

            if (manage_defer_scope) {
                // Execute defers at block exit (LIFO order)
                executeScopeDefers();

                // Pop defer scope
                defer_stack.pop_back();
            }

            return lastVal;
        }
        
        case ASTNode::NodeType::RETURN: {
            // Return statement - execute all defers before returning
            ReturnStmt* ret = static_cast<ReturnStmt*>(stmt);

            // Execute all defer blocks (LIFO order)
            executeFunctionDefers();

            // Phase 4.6: Async functions branch to suspend block instead of direct return
            if (current_func_is_async && current_coro_suspend_block) {
                if (ret->value) {
                    llvm::Value* retVal = codegenExpression(ret->value.get());
                    // Store return value to promise if available
                    if (retVal && current_async_promise && current_async_result_type) {
                        llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(current_async_result_type);
                        llvm::Value* result = llvm::UndefValue::get(resultStruct);
                        llvm::Type* valueFieldType = resultStruct->getElementType(0);
                        if (retVal->getType() != valueFieldType && retVal->getType()->isIntegerTy()
                            && valueFieldType->isIntegerTy()) {
                            retVal = builder.CreateIntCast(retVal, valueFieldType, true, "async.ret.cast");
                        }
                        result = builder.CreateInsertValue(result, retVal, 0, "result.val");
                        result = builder.CreateInsertValue(result,
                            llvm::ConstantPointerNull::get(
                                llvm::cast<llvm::PointerType>(resultStruct->getElementType(1))),
                            1, "result.err");
                        result = builder.CreateInsertValue(result, builder.getInt8(0), 2, "result.is_error");
                        builder.CreateStore(result, current_async_promise);
                    }
                }
                builder.CreateBr(current_coro_suspend_block);
            } else {
                // Normal synchronous return
                if (ret->value) {
                    // Check for Result{val:..., err:..., is_error:...} literal syntax
                    // This is an explicit Result construction — a ToS operator.
                    // The programmer takes responsibility for building the Result manually.
                    ObjectLiteralExpr* objLit = dynamic_cast<ObjectLiteralExpr*>(ret->value.get());
                    if (objLit && objLit->type_name == "Result") {
                        llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                        llvm::Type* returnType = currentFunc->getReturnType();
                        
                        if (returnType->isStructTy()) {
                            llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(returnType);
                            
                            if (resultStruct->getNumElements() == 3 &&
                                resultStruct->getElementType(1)->isPointerTy() &&
                                resultStruct->getElementType(2)->isIntegerTy(8)) {
                                
                                // Build Result struct from named fields
                                llvm::Value* result = llvm::UndefValue::get(resultStruct);
                                
                                for (const auto& field : objLit->fields) {
                                    llvm::Value* fieldVal = codegenExpression(field.value.get());
                                    if (!fieldVal) continue;
                                    
                                    if (field.name == "val") {
                                        llvm::Type* valType = resultStruct->getElementType(0);
                                        if (fieldVal->getType() != valType) {
                                            if (fieldVal->getType()->isIntegerTy() && valType->isIntegerTy()) {
                                                fieldVal = builder.CreateIntCast(fieldVal, valType, true, "result.val.cast");
                                            }
                                        }
                                        result = builder.CreateInsertValue(result, fieldVal, 0, "result.val");
                                    } else if (field.name == "err") {
                                        llvm::Type* errType = resultStruct->getElementType(1);
                                        if (fieldVal->getType() != errType) {
                                            if (fieldVal->getType()->isIntegerTy()) {
                                                fieldVal = builder.CreateIntToPtr(fieldVal, errType, "result.err.cast");
                                            } else if (fieldVal->getType()->isPointerTy()) {
                                                // Already a pointer — compatible
                                            }
                                        }
                                        result = builder.CreateInsertValue(result, fieldVal, 1, "result.err");
                                    } else if (field.name == "is_error") {
                                        llvm::Type* flagType = resultStruct->getElementType(2);
                                        if (fieldVal->getType() != flagType) {
                                            if (fieldVal->getType()->isIntegerTy()) {
                                                fieldVal = builder.CreateIntCast(fieldVal, flagType, false, "result.flag.cast");
                                            }
                                        }
                                        result = builder.CreateInsertValue(result, fieldVal, 2, "result.is_error");
                                    }
                                }
                                
                                builder.CreateRet(result);
                                return nullptr;
                            }
                        }
                        
                        throw std::runtime_error(
                            "Line " + std::to_string(ret->line) + ", Column " + std::to_string(ret->column) +
                            ": Result{...} literal used but function does not return a Result type.");
                    }
                    
                    llvm::Value* retVal = codegenExpression(ret->value.get());
                    if (retVal) {
                        llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                        llvm::Type* returnType = currentFunc->getReturnType();

                        // Check if return type is a Result struct {value, error*, is_error}
                        if (returnType->isStructTy()) {
                            llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(returnType);
                            
                            // Result types have exactly 3 fields: {T, ptr, i8}
                            if (resultStruct->getNumElements() == 3 &&
                                resultStruct->getElementType(1)->isPointerTy() &&
                                resultStruct->getElementType(2)->isIntegerTy(8)) {
                                
                                // This is a Result return type.
                                // 'return' only accepts a value that is already a Result struct.
                                // For bare values, use pass(value) or fail(error) instead.
                                if (retVal->getType() == returnType) {
                                    // Value is already a Result struct - pass through directly
                                    builder.CreateRet(retVal);
                                    return nullptr;
                                }
                                
                                // Bare value returned in a Result-returning function - ERROR
                                // Users must use pass(value) for success or fail(error) for errors.
                                int line = ret->line;
                                int col = ret->column;
                                throw std::runtime_error(
                                    "Line " + std::to_string(line) + ", Column " + std::to_string(col) + 
                                    ": 'return' cannot return a bare value in an Aria function. "
                                    "Use pass(value) for success or fail(error) for errors. "
                                    "'return' is only valid with an already-constructed Result object "
                                    "or a Result{val:..., err:..., is_error:...} literal.");
                                return nullptr;
                            }
                        }
                        
                        // Not a Result type - handle regular type conversions
                        if (retVal->getType() != returnType) {
                            // Handle integer conversions
                            if (retVal->getType()->isIntegerTy() && returnType->isIntegerTy()) {
                                llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(retVal->getType());
                                llvm::IntegerType* retIntTy = llvm::cast<llvm::IntegerType>(returnType);

                                if (valIntTy->getBitWidth() < retIntTy->getBitWidth()) {
                                    // Sign extend for widening
                                    retVal = builder.CreateSExt(retVal, returnType, "ret_sext");
                                } else if (valIntTy->getBitWidth() > retIntTy->getBitWidth()) {
                                    // Truncate for narrowing
                                    retVal = builder.CreateTrunc(retVal, returnType, "ret_trunc");
                                }
                            }
                        }
                        builder.CreateRet(retVal);
                    }
                } else {
                    builder.CreateRetVoid();
                }
            }
            return nullptr;
        }
        
        case ASTNode::NodeType::EXPRESSION_STMT: {
            // Expression statement
            ExpressionStmt* exprStmt = static_cast<ExpressionStmt*>(stmt);
            return codegenExpression(exprStmt->expression.get());
        }
        
        case ASTNode::NodeType::VAR_DECL: {
            // Variable declaration
            VarDeclStmt* varDecl = static_cast<VarDeclStmt*>(stmt);

            // ================================================================
            // FUNCTION POINTER VARIABLE: (retType)(params):name = lambda
            // ================================================================
            if (varDecl->typeNode &&
                varDecl->typeNode->type == ASTNode::NodeType::FUNCTION_TYPE) {
                aria::FunctionType* funcPtrType = static_cast<aria::FunctionType*>(varDecl->typeNode.get());

                // Allocate fat pointer {ptr, ptr} on stack
                llvm::Type* ptrTy = llvm::PointerType::get(context, 0);
                llvm::StructType* fatPtrTy = llvm::StructType::get(context, {ptrTy, ptrTy});
                llvm::AllocaInst* fpAlloca = builder.CreateAlloca(fatPtrTy, nullptr, varDecl->varName);
                named_values[varDecl->varName] = fpAlloca;

                // Determine base return type string for closure call info
                std::string retTypeStr = funcPtrType->returnType ? funcPtrType->returnType->toString() : "void";
                // Store as "func_ptr:retType" so closure call path can look up return type
                var_aria_types[varDecl->varName] = "func_ptr:" + retTypeStr;

                // Generate the lambda body as an internal function
                if (varDecl->initializer &&
                    varDecl->initializer->type == ASTNode::NodeType::LAMBDA) {
                    LambdaExpr* lambda = static_cast<LambdaExpr*>(varDecl->initializer.get());

                    // Generate unique internal function name
                    static int fp_counter = 0;
                    std::string fnName = "_funcptr_" + varDecl->varName + "_" + std::to_string(fp_counter++);

                    auto savedIP = builder.saveIP();
                    auto outer_named = named_values;
                    auto outer_types = var_aria_types;
                    named_values.clear();
                    var_aria_types.clear();

                    // Build: env_ptr (i8*) + explicit param types
                    std::vector<llvm::Type*> paramTypes;
                    paramTypes.push_back(llvm::PointerType::get(context, 0)); // hidden env ptr
                    for (const auto& paramNode : lambda->parameters) {
                        ParameterNode* pn = static_cast<ParameterNode*>(paramNode.get());
                        std::string pts = pn->typeNode ? pn->typeNode->toString() : "int64";
                        paramTypes.push_back(mapTypeFromName(pts));
                    }

                    // Return type — wrap in Result<T> like module-level functions
                    std::string lambdaRetStr = lambda->returnTypeName.empty() ? retTypeStr : lambda->returnTypeName;
                    llvm::Type* innerRet = lambdaRetStr == "void" ? llvm::Type::getVoidTy(context)
                                                                   : mapTypeFromName(lambdaRetStr);
                    llvm::Type* actualRet;
                    if (innerRet->isVoidTy()) {
                        actualRet = innerRet;
                    } else {
                        // Result<T>: { T, i8*, i1 }
                        actualRet = llvm::StructType::get(context, {
                            innerRet,
                            llvm::PointerType::get(context, 0),
                            llvm::Type::getInt1Ty(context)
                        });
                    }

                    llvm::FunctionType* fnType = llvm::FunctionType::get(actualRet, paramTypes, false);
                    llvm::Function* fn = llvm::Function::Create(
                        fnType, llvm::Function::InternalLinkage, fnName, module.get());

                    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", fn);
                    builder.SetInsertPoint(entry);

                    // Map env ptr arg (skip it, unnamed)
                    auto argIt = fn->arg_begin();
                    argIt->setName("env");
                    ++argIt;

                    // Map explicit parameters to allocas
                    size_t paramIdx = 0;
                    for (; argIt != fn->arg_end(); ++argIt, ++paramIdx) {
                        if (paramIdx < lambda->parameters.size()) {
                            ParameterNode* pn = static_cast<ParameterNode*>(lambda->parameters[paramIdx].get());
                            argIt->setName(pn->paramName);
                            llvm::AllocaInst* a = builder.CreateAlloca(argIt->getType(), nullptr, pn->paramName);
                            builder.CreateStore(&*argIt, a);
                            named_values[pn->paramName] = a;
                            var_aria_types[pn->paramName] = pn->typeNode ? pn->typeNode->toString() : "";
                        }
                    }

                    // Generate body using IRGenerator::codegenStatement (handles PASS/FAIL)
                    if (lambda->body) {
                        codegenStatement(lambda->body.get());
                    }

                    // Default terminator if body didn't provide one
                    if (!builder.GetInsertBlock()->getTerminator()) {
                        if (actualRet->isVoidTy()) {
                            builder.CreateRetVoid();
                        } else {
                            builder.CreateRet(llvm::UndefValue::get(actualRet));
                        }
                    }

                    // Restore outer scope
                    named_values = outer_named;
                    var_aria_types = outer_types;
                    builder.restoreIP(savedIP);

                    // Build fat pointer and store into alloca
                    llvm::Value* funcAsPtr = builder.CreateBitCast(fn, ptrTy);
                    llvm::Value* nullEnv = llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(ptrTy));
                    builder.CreateStore(funcAsPtr,
                        builder.CreateStructGEP(fatPtrTy, fpAlloca, 0, "method_field"));
                    builder.CreateStore(nullEnv,
                        builder.CreateStructGEP(fatPtrTy, fpAlloca, 1, "env_field"));

                    // Restore fat pointer alloca mapping (cleared above)
                    named_values[varDecl->varName] = fpAlloca;
                    var_aria_types[varDecl->varName] = "func_ptr:" + retTypeStr;
                }
                // FUNCTION_TYPE handled — fall to return below
                return nullptr;
            }

            // ================================================================
            // NORMAL VARIABLE DECLARATION (non-function-pointer)
            // ================================================================

            // Get the type name - might be in typeName or need to extract from typeNode
            std::string actualTypeName = varDecl->typeName;
            if (actualTypeName.empty() && varDecl->typeNode) {
                // Extract type from typeNode (SimpleType)
                if (varDecl->typeNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    SimpleType* simpleType = static_cast<SimpleType*>(varDecl->typeNode.get());
                    actualTypeName = simpleType->typeName;
                }
            }
            
            // Check if this is an optional type
            bool isOptional = !actualTypeName.empty() && actualTypeName.back() == '?';
            
            // DEBUG: Print the actual type name
            // llvm::errs() << "DEBUG: Variable '" << varDecl->varName << "' has type: '" << actualTypeName << "'\n";
            
            // Allocate stack space for the variable with proper type mapping
            llvm::Type* varType = mapTypeFromName(actualTypeName);
            
            // ================================================================
            // v0.2.35: Borrow variables ($$i/$$m) — alias the original
            // ================================================================
            if (varDecl->isBorrowImm || varDecl->isBorrowMut) {
                if (varDecl->initializer && 
                    varDecl->initializer->type == ASTNode::NodeType::IDENTIFIER) {
                    std::string target_name = 
                        static_cast<IdentifierExpr*>(varDecl->initializer.get())->name;
                    auto it = named_values.find(target_name);
                    if (it != named_values.end()) {
                        named_values[varDecl->varName] = it->second;
                        var_aria_types[varDecl->varName] = actualTypeName;
                        std::cerr << "[DEBUG] Borrow " << varDecl->varName 
                                  << " aliased to " << target_name << std::endl;
                        return nullptr;
                    }
                }
                // Fallthrough if target not found (shouldn't happen after type checker)
            }
            
            llvm::AllocaInst* alloca = builder.CreateAlloca(varType, nullptr, varDecl->varName);
            
            // Emit debug info for local variable
            if (debug_enabled && di_builder && getCurrentDebugScope()) {
                llvm::DIType* di_type = nullptr;
                if (type_system) {
                    auto* aria_type_for_debug = type_system->getPrimitiveType(actualTypeName);
                    if (aria_type_for_debug) {
                        di_type = mapDebugType(aria_type_for_debug);
                    }
                }
                if (!di_type) {
                    // Fallback: create basic type from LLVM type size
                    unsigned bits = varType->getPrimitiveSizeInBits();
                    if (bits == 0) bits = module->getDataLayout().getTypeSizeInBits(varType);
                    di_type = di_builder->createBasicType(actualTypeName, bits, llvm::dwarf::DW_ATE_signed);
                }
                auto* di_var = di_builder->createAutoVariable(
                    getCurrentDebugScope(), varDecl->varName, di_file,
                    varDecl->line, di_type);
                di_builder->insertDeclare(alloca, di_var, di_builder->createExpression(),
                    llvm::DILocation::get(context, varDecl->line, varDecl->column, getCurrentDebugScope()),
                    builder.GetInsertBlock());
            }

            // P0: Apply explicit alignment from #[align(N)] attribute if specified
            if (varDecl->alignment > 0) {
                alloca->setAlignment(llvm::Align(varDecl->alignment));
            }
            // Apply default alignment for special types
            else if (actualTypeName == "int128" || actualTypeName == "uint128") {
                alloca->setAlignment(llvm::Align(16));
            }
            
            // Track the Aria type of this alloca (needed for member access, vectors, and TBB overflow detection)
            if (type_system) {
                Type* aria_type = nullptr;
                
                // Check for vector types FIRST (before primitives, since getPrimitiveType creates entries!)
                if (actualTypeName.find("vec") != std::string::npos) {
                    // Extract component type and dimension from type name
                    Type* componentType = type_system->getPrimitiveType("flt64");  // default
                    int dimension = 2;  // default
                    
                    if (actualTypeName.find("vec2") != std::string::npos) dimension = 2;
                    else if (actualTypeName.find("vec3") != std::string::npos) dimension = 3;
                    else if (actualTypeName.find("vec9") != std::string::npos) dimension = 9;
                    
                    if (actualTypeName[0] == 'd') componentType = type_system->getPrimitiveType("flt64");
                    else if (actualTypeName[0] == 'f') componentType = type_system->getPrimitiveType("flt32");
                    else if (actualTypeName[0] == 'i') componentType = type_system->getPrimitiveType("int32");
                    else if (actualTypeName[0] == 'v') componentType = type_system->getPrimitiveType("flt64");
                    
                    aria_type = type_system->getVectorType(componentType, dimension);
                }
                // CRITICAL FIX: Check for Result<T> BEFORE struct/primitive
                // getPrimitiveType() accepts any string, so "Result<int32>" would wrongly become PRIMITIVE
                else if (actualTypeName.find("Result<") == 0) {
                    std::cerr << "[DEBUG VARDECL] Detected Result type variable: " << actualTypeName << std::endl;
                    // Extract inner type: "Result<int32>" -> "int32"
                    size_t start = actualTypeName.find('<') + 1;
                    size_t end = actualTypeName.find('>');
                    if (end != std::string::npos && end > start) {
                        std::string innerTypeStr = actualTypeName.substr(start, end - start);
                        std::cerr << "[DEBUG VARDECL] Result inner type: " << innerTypeStr << std::endl;
                        aria::sema::Type* innerType = type_system->getPrimitiveType(innerTypeStr);
                        if (!innerType) {
                            innerType = type_system->getStructType(innerTypeStr);
                        }
                        if (innerType) {
                            std::cerr << "[DEBUG VARDECL] Found inner type, calling getResultType()" << std::endl;
                            aria_type = type_system->getResultType(innerType);
                            if (aria_type) {
                                std::cerr << "[DEBUG VARDECL] getResultType() returned type with kind: " << static_cast<int>(aria_type->getKind()) << std::endl;
                            } else {
                                std::cerr << "[DEBUG VARDECL] ERROR: getResultType() returned nullptr!" << std::endl;
                            }
                        } else {
                            std::cerr << "[DEBUG VARDECL] ERROR: Could not find inner type: " << innerTypeStr << std::endl;
                        }
                    }
                } else if (isOptional && actualTypeName.size() >= 2) {
                    // Optional type: strip trailing '?' and look up base type, then wrap
                    std::string baseTypeName = actualTypeName.substr(0, actualTypeName.size() - 1);
                    Type* baseType = type_system->getStructType(baseTypeName);
                    if (!baseType) {
                        baseType = type_system->getPrimitiveType(baseTypeName);
                    }
                    if (baseType) {
                        aria_type = type_system->getOptionalType(baseType);
                    }
                } else {
                    // Not a vector or Result, try struct then primitive
                    aria_type = type_system->getStructType(actualTypeName);
                    // If direct lookup fails, try mangled generic name: Vec<int8> -> Vec_int8
                    if (!aria_type && actualTypeName.find('<') != std::string::npos) {
                        size_t lt = actualTypeName.find('<');
                        std::string baseName = actualTypeName.substr(0, lt);
                        std::string argsStr = actualTypeName.substr(lt + 1);
                        if (!argsStr.empty() && argsStr.back() == '>')
                            argsStr.pop_back();
                        // Build mangled name: Base_arg1_arg2
                        std::string mangledName = baseName;
                        std::istringstream argStream(argsStr);
                        std::string arg;
                        while (std::getline(argStream, arg, ',')) {
                            // Trim whitespace
                            size_t s = arg.find_first_not_of(" \t");
                            size_t e = arg.find_last_not_of(" \t");
                            if (s != std::string::npos)
                                arg = arg.substr(s, e - s + 1);
                            mangledName += "_" + arg;
                        }
                        aria_type = type_system->getStructType(mangledName);
                    }
                    if (!aria_type) {
                        aria_type = type_system->getPrimitiveType(actualTypeName);
                    }
                }
                
                if (aria_type) {
                    value_types[alloca] = aria_type;
                    std::cerr << "[DEBUG VARDECL] Registered value_types for '" << varDecl->varName << "' with type kind: " << static_cast<int>(aria_type->getKind()) << std::endl;
                }
            }
            
            // CRITICAL FIX: Also track type name for exotic type routing
            // ExprCodegen::getExprExoticTypeName() checks var_aria_types to route
            // modulo operations to runtime functions (aria_tryte_mod, aria_nyte_mod)
            var_aria_types[varDecl->varName] = actualTypeName;
            std::cerr << "[DEBUG] Registered var_aria_types[" << varDecl->varName << "] = " << actualTypeName << std::endl;
            
            // Generate initializer if present
            if (varDecl->initializer) {
                std::cerr << "[DEBUG IR_GEN] Variable '" << varDecl->varName << "' HAS initializer, generating..." << std::endl;
                llvm::Value* initVal = codegenExpression(varDecl->initializer.get());
                std::cerr << "[DEBUG IR_GEN] Initializer generated, value = " << (void*)initVal << std::endl;
                if (initVal) {
                    // For optional types, need to construct the { i1, T } struct
                    if (isOptional) {
                        // Check if initializer is NIL keyword
                        bool isNil = false;
                        // Check for NIL - can be IDENTIFIER or LITERAL with explicit_type = "NIL"
                        if (varDecl->initializer->type == ASTNode::NodeType::IDENTIFIER) {
                            IdentifierExpr* idExpr = static_cast<IdentifierExpr*>(varDecl->initializer.get());
                            isNil = (idExpr->name == "NIL");
                        } else if (varDecl->initializer->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* litExpr = static_cast<LiteralExpr*>(varDecl->initializer.get());
                            isNil = (litExpr->explicit_type == "NIL");
                        }
                        
                        llvm::Value* optionalValue;
                        if (isNil) {
                            // Create { i1 false, T undef } for NIL
                            optionalValue = createOptionalNone(varType);
                        } else {
                            // Create { i1 true, T value } for actual value
                            // Need to extract wrapped type from optional struct type
                            llvm::StructType* optStructTy = llvm::cast<llvm::StructType>(varType);
                            llvm::Type* wrappedType = optStructTy->getElementType(1);
                            
                            // Cast initVal to wrapped type if necessary
                            if (initVal->getType() != wrappedType) {
                                if (initVal->getType()->isIntegerTy() && wrappedType->isIntegerTy()) {
                                    llvm::IntegerType* initIntTy = llvm::cast<llvm::IntegerType>(initVal->getType());
                                    llvm::IntegerType* wrapIntTy = llvm::cast<llvm::IntegerType>(wrappedType);
                                    
                                    if (initIntTy->getBitWidth() > wrapIntTy->getBitWidth()) {
                                        initVal = builder.CreateTrunc(initVal, wrappedType, "init_trunc");
                                    } else {
                                        initVal = builder.CreateSExt(initVal, wrappedType, "init_sext");
                                    }
                                }
                            }
                            
                            optionalValue = createOptionalSome(initVal);
                        }
                        
                        builder.CreateStore(optionalValue, alloca);
                    } else {
                        // Non-optional type - cast initializer to match variable type if necessary
                        if (initVal->getType() != varType) {
                            // P0: Auto-unwrap Optional<T> or Result<T> from function returns.
                            // Both Optional {i1, ptr} and Result {T, ptr, i8} are struct types —
                            // merge them into ONE branch so the else-if chain doesn't skip Result<T>
                            // when varType happens to be a pointer (e.g. string variables).
                            if (initVal->getType()->isStructTy()) {
                                llvm::StructType* initStructTy = llvm::cast<llvm::StructType>(initVal->getType());
                                // Check if this is an Optional struct: {i1, ptr} assigned to a ptr variable
                                if (varType->isPointerTy() &&
                                    initStructTy->getNumElements() == 2 &&
                                    initStructTy->getElementType(0)->isIntegerTy(1) &&
                                    initStructTy->getElementType(1)->isPointerTy()) {
                                    // Extract the pointer (field 1) from Optional
                                    initVal = builder.CreateExtractValue(initVal, 1, "unwrap_optional_ptr");
                                    std::cerr << "[DEBUG IR_GEN] Auto-unwrapped Optional<ptr> for variable '" 
                                              << varDecl->varName << "'" << std::endl;
                                }
                                // Result<T> unwrap: pub funcs return Result<T> = {T, ptr, i8}.
                                // When assigning to a T variable, extract element 0.
                                // This covers direct pub-func-call assignments like:
                                //   uint64:x = some_pub_func();
                                //   uint128:r = uuid_nil();
                                // Previously disabled as "BUG-003" but auto-unwrapping IS
                                // correct here — the type checker should verify the call site,
                                // but the codegen path must still produce valid IR.
                                else if (initStructTy->getNumElements() == 3 &&
                                         initStructTy->getElementType(1)->isPointerTy() &&
                                         initStructTy->getElementType(2)->isIntegerTy(8) &&
                                         initStructTy->getElementType(0) == varType) {
                                    initVal = builder.CreateExtractValue(initVal, 0, "unwrap_pub_result");
                                }
                            }
                            // Check if we're promoting a literal to an LBIM struct type
                            else if (varType->isStructTy() && (initVal->getType()->isIntegerTy() || initVal->getType()->isFloatingPointTy())) {
                                llvm::StructType* struct_type = llvm::cast<llvm::StructType>(varType);
                                std::string struct_name = struct_type->getName().str();
                                
                                // If target is LBIM type, promote the literal
                                if (struct_name.find("struct.int") == 0 || struct_name.find("struct.uint") == 0 ||
                                    struct_name == "struct.flt256" || struct_name == "struct.flt512") {
                                    std::cerr << "[DEBUG IR_GEN] Promoting literal to LBIM type " << struct_name << std::endl;
                                    initVal = promoteToLBIMStruct(initVal, varType);
                                }
                            }
                            // For integer types, use trunc or sext/zext
                            else if (initVal->getType()->isIntegerTy() && varType->isIntegerTy()) {
                                llvm::IntegerType* initIntTy = llvm::cast<llvm::IntegerType>(initVal->getType());
                                llvm::IntegerType* varIntTy = llvm::cast<llvm::IntegerType>(varType);
                                
                                if (initIntTy->getBitWidth() > varIntTy->getBitWidth()) {
                                    // Truncate (narrowing conversion)
                                    initVal = builder.CreateTrunc(initVal, varType, "init_trunc");
                                } else {
                                    // Sign extend (widening conversion for signed types)
                                    initVal = builder.CreateSExt(initVal, varType, "init_sext");
                                }
                            }
                            // Phase 3.2.5: For float types, use fpext or fptrunc
                            else if (initVal->getType()->isFloatingPointTy() && varType->isFloatingPointTy()) {
                                unsigned initBits = initVal->getType()->getScalarSizeInBits();
                                unsigned varBits = varType->getScalarSizeInBits();
                                
                                if (initBits > varBits) {
                                    // Truncate to lower precision (fp128 → double, etc.)
                                    initVal = builder.CreateFPTrunc(initVal, varType, "init_fptrunc");
                                } else if (initBits < varBits) {
                                    // Extend to higher precision (double → fp128, etc.)
                                    initVal = builder.CreateFPExt(initVal, varType, "init_fpext");
                                }
                            }
                            // Int to float conversion
                            else if (initVal->getType()->isIntegerTy() && varType->isFloatingPointTy()) {
                                initVal = builder.CreateSIToFP(initVal, varType, "init_itof");
                            }
                        }
                        
                        // SPECIAL HANDLING: unknown literal (polymorphic)
                        // If initializer is unknown, generate sentinel of declared type
                        bool isUnknown = false;
                        if (varDecl->initializer->type == ASTNode::NodeType::LITERAL) {
                            LiteralExpr* litExpr = static_cast<LiteralExpr*>(varDecl->initializer.get());
                            if (std::holds_alternative<std::string>(litExpr->value)) {
                                const std::string& str = std::get<std::string>(litExpr->value);
                                isUnknown = (str == "unknown");
                            }
                        }
                        
                        if (isUnknown) {
                            // Generate unknown sentinel for declared type
                            // unknown is polymorphic - adapts to variable type
                            if (varType->isIntegerTy()) {
                                llvm::IntegerType* intType = llvm::cast<llvm::IntegerType>(varType);
                                unsigned width = intType->getBitWidth();
                                // unknown sentinel is signed maximum (opposite of ERR min)
                                initVal = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
                                std::cerr << "[DEBUG] Generated unknown sentinel for type: ";
                                varType->print(llvm::errs());
                                std::cerr << std::endl;
                            } else {
                                // For non-integer types, generate zero for now
                                // TODO: Handle float/string unknown
                                initVal = llvm::Constant::getNullValue(varType);
                            }
                        }
                        
                        // ANY TYPE AUTO-BOXING: When the declared variable is `any` type,
                        // we box the initializer value into the {ptr, i64} fat pointer struct.
                        // - Allocate stack space for the value
                        // - Store the value into the allocation
                        // - Build the any struct: {data_ptr, sizeof(value)}
                        if (actualTypeName == "any" || actualTypeName == "wild any" || actualTypeName == "wildx any") {
                            llvm::Type* initType = initVal->getType();
                            // If already a pointer, use it directly with size 0 (unknown)
                            llvm::Value* dataPtr;
                            llvm::Value* dataSize;
                            if (initType->isPointerTy()) {
                                dataPtr = initVal;
                                dataSize = llvm::ConstantInt::get(builder.getInt64Ty(), 0);
                            } else {
                                // Auto-box: alloca for the value, store it, use the alloca ptr
                                llvm::AllocaInst* boxAlloca = builder.CreateAlloca(initType, nullptr, varDecl->varName + ".boxed");
                                builder.CreateStore(initVal, boxAlloca);
                                dataPtr = boxAlloca;
                                // Get sizeof(T) from the data layout
                                uint64_t typeSize = module->getDataLayout().getTypeAllocSize(initType);
                                dataSize = llvm::ConstantInt::get(builder.getInt64Ty(), typeSize);
                            }
                            // Build the {ptr, i64} struct
                            llvm::Type* anyStructType = llvm::StructType::get(context, {
                                llvm::PointerType::get(context, 0),
                                builder.getInt64Ty()
                            });
                            llvm::Value* anyVal = llvm::UndefValue::get(anyStructType);
                            anyVal = builder.CreateInsertValue(anyVal, dataPtr, 0, "any.ptr");
                            anyVal = builder.CreateInsertValue(anyVal, dataSize, 1, "any.size");
                            builder.CreateStore(anyVal, alloca);
                        }
                        // ARRAY INIT FIX: When the declared variable is a fixed-size array
                        // [N x T] and the initializer is an ARRAY_LITERAL (returned as a
                        // flat pointer to a tmp alloca), copy element-by-element into the
                        // properly-typed alloca using two-index GEP.  This avoids storing
                        // a raw pointer value into an integer/array slot.
                        else if (varType->isArrayTy() && initVal->getType()->isPointerTy()) {
                            auto* arrType = llvm::cast<llvm::ArrayType>(varType);
                            llvm::Type* elemType = arrType->getElementType();
                            uint64_t numElems = arrType->getNumElements();
                            for (uint64_t i = 0; i < numElems; ++i) {
                                // Load element i from the tmp flat alloca
                                llvm::Value* srcPtr = builder.CreateGEP(
                                    elemType, initVal,
                                    llvm::ConstantInt::get(builder.getInt64Ty(), i),
                                    "arr.init.src");
                                llvm::Value* elem = builder.CreateLoad(elemType, srcPtr, "arr.init.elem");
                                // Store into our typed alloca via two-index GEP
                                std::vector<llvm::Value*> destIdx = {
                                    llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                                    llvm::ConstantInt::get(builder.getInt64Ty(), i)
                                };
                                llvm::Value* destPtr = builder.CreateInBoundsGEP(
                                    varType, alloca, destIdx, "arr.init.dst");
                                builder.CreateStore(elem, destPtr);
                            }
                        } else {
                            builder.CreateStore(initVal, alloca);
                        }
                    }
                }
            } else if (isOptional) {
                // No initializer for optional type - default to NIL (None)
                llvm::Value* noneValue = createOptionalNone(varType);
                builder.CreateStore(noneValue, alloca);
            }
            
            // Add to symbol table
            named_values[varDecl->varName] = alloca;

            // Track Aria type name for UFCS method resolution
            var_aria_types[varDecl->varName] = actualTypeName;

            return alloca;
        }
        
        case ASTNode::NodeType::IF: {
            // If statement with optional else
            IfStmt* ifStmt = static_cast<IfStmt*>(stmt);
            
            std::cerr << "[DEBUG IF] Generating IF statement" << std::endl;
            llvm::Value* condVal = codegenExpression(ifStmt->condition.get());
            if (!condVal) {
                std::cerr << "[DEBUG IF] ERROR: Condition codegen returned nullptr!" << std::endl;
                return nullptr;
            }
            std::cerr << "[DEBUG IF] Condition generated successfully"  << std::endl;
            
            // Auto-unwrap Result<T> struct {T, ptr, i8} when used as if-condition
            if (condVal->getType()->isStructTy()) {
                condVal = builder.CreateExtractValue(condVal, {0}, "unwrap_result_cond");
            }
            
            // Convert condition to bool by comparing to zero
            if (condVal->getType()->isIntegerTy()) {
                condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "ifcond");
            }
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            
            // Create blocks for then, else, and merge
            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", function);
            llvm::BasicBlock* elseBB = ifStmt->elseBranch ? llvm::BasicBlock::Create(context, "else") : nullptr;
            llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont");
            
            // Branch based on condition
            if (elseBB) {
                builder.CreateCondBr(condVal, thenBB, elseBB);
            } else {
                builder.CreateCondBr(condVal, thenBB, mergeBB);
            }
            
            // Generate then block
            builder.SetInsertPoint(thenBB);
            codegenStatement(ifStmt->thenBranch.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(mergeBB);
            }
            
            // Generate else block if present
            if (elseBB) {
                function->insert(function->end(), elseBB);
                builder.SetInsertPoint(elseBB);
                codegenStatement(ifStmt->elseBranch.get());
                if (!builder.GetInsertBlock()->getTerminator()) {
                    builder.CreateBr(mergeBB);
                }
            }
            
            // Continue with merge block
            function->insert(function->end(), mergeBB);
            builder.SetInsertPoint(mergeBB);
            std::cerr << "[DEBUG IF] IF statement complete" << std::endl;
            return nullptr;
        }
        
        case ASTNode::NodeType::WHILE: {
            // While loop
            WhileStmt* whileStmt = static_cast<WhileStmt*>(stmt);
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            
            // Create blocks for condition, body, and after loop
            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "whilecond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "whilebody");
            llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "afterwhile");
            
            // Push loop context (continue goes to condBB, break goes to afterBB)
            loop_stack.push_back(LoopContext(condBB, afterBB));
            
            // Jump to condition block
            builder.CreateBr(condBB);
            
            // Generate condition
            builder.SetInsertPoint(condBB);
            llvm::Value* condVal = codegenExpression(whileStmt->condition.get());
            if (!condVal) return nullptr;
            // Auto-unwrap Result<T> struct {T, ptr, i8} when used as while-condition
            if (condVal->getType()->isStructTy()) {
                condVal = builder.CreateExtractValue(condVal, {0}, "unwrap_result_cond");
            }
            if (condVal->getType()->isIntegerTy()) {
                condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "whilecond");
            }
            builder.CreateCondBr(condVal, bodyBB, afterBB);
            
            // Generate body
            function->insert(function->end(), bodyBB);
            builder.SetInsertPoint(bodyBB);
            codegenStatement(whileStmt->body.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(condBB);  // Loop back to condition
            }
            
            // Pop loop context
            loop_stack.pop_back();
            
            // Continue after loop
            function->insert(function->end(), afterBB);
            builder.SetInsertPoint(afterBB);
            return nullptr;
        }
        
        case ASTNode::NodeType::FOR: {
            ForStmt* forStmt = static_cast<ForStmt*>(stmt);
            
            if (forStmt->isRangeBased) {
                // Range-based for loop: for (var in range) { body }
                
                // Generate the range expression
                llvm::Value* rangePtr = codegenExpression(forStmt->rangeExpr.get());
                if (!rangePtr) {
                    return nullptr;
                }
                
                // Extract range components: {start, end, isExclusive}
                llvm::Type* rangeType = llvm::StructType::get(
                    context,
                    {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt1Ty()}
                );
                
                llvm::Value* startPtr = builder.CreateStructGEP(rangeType, rangePtr, 0, "range.start.ptr");
                llvm::Value* endPtr = builder.CreateStructGEP(rangeType, rangePtr, 1, "range.end.ptr");
                llvm::Value* exclusivePtr = builder.CreateStructGEP(rangeType, rangePtr, 2, "range.exclusive.ptr");
                
                llvm::Value* startVal = builder.CreateLoad(builder.getInt64Ty(), startPtr, "range.start");
                llvm::Value* endVal = builder.CreateLoad(builder.getInt64Ty(), endPtr, "range.end");
                llvm::Value* isExclusive = builder.CreateLoad(builder.getInt1Ty(), exclusivePtr, "range.exclusive");
                
                // Create iterator variable
                llvm::Value* iteratorVar = builder.CreateAlloca(builder.getInt64Ty(), nullptr, forStmt->iteratorName);
                builder.CreateStore(startVal, iteratorVar);
                
                // Add iterator to symbol table
                named_values[forStmt->iteratorName] = iteratorVar;
                
                // Adjust end value if inclusive (end+1 for comparison)
                llvm::Value* limit = builder.CreateSelect(
                    isExclusive,
                    endVal,
                    builder.CreateAdd(endVal, llvm::ConstantInt::get(builder.getInt64Ty(), 1)),
                    "loop.limit"
                );
                
                llvm::Function* function = builder.GetInsertBlock()->getParent();
                
                // Create blocks
                llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "forrange.cond", function);
                llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "forrange.body");
                llvm::BasicBlock* updateBB = llvm::BasicBlock::Create(context, "forrange.update");
                llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "afterforrange");
                
                // Push loop context
                loop_stack.push_back(LoopContext(updateBB, afterBB));
                
                // Jump to condition
                builder.CreateBr(condBB);
                
                // Generate condition: iterator < limit
                builder.SetInsertPoint(condBB);
                llvm::Value* currentVal = builder.CreateLoad(builder.getInt64Ty(), iteratorVar, forStmt->iteratorName);
                llvm::Value* condVal = builder.CreateICmpSLT(currentVal, limit, "forrange.cond");
                builder.CreateCondBr(condVal, bodyBB, afterBB);
                
                // Generate body
                function->insert(function->end(), bodyBB);
                builder.SetInsertPoint(bodyBB);
                codegenStatement(forStmt->body.get());
                if (!builder.GetInsertBlock()->getTerminator()) {
                    builder.CreateBr(updateBB);
                }
                
                // Generate update: iterator++
                function->insert(function->end(), updateBB);
                builder.SetInsertPoint(updateBB);
                llvm::Value* nextVal = builder.CreateAdd(
                    builder.CreateLoad(builder.getInt64Ty(), iteratorVar, forStmt->iteratorName),
                    llvm::ConstantInt::get(builder.getInt64Ty(), 1),
                    "forrange.next"
                );
                builder.CreateStore(nextVal, iteratorVar);
                builder.CreateBr(condBB);
                
                // Pop loop context
                loop_stack.pop_back();
                
                // Remove iterator from symbol table
                named_values.erase(forStmt->iteratorName);
                
                // Continue after loop
                function->insert(function->end(), afterBB);
                builder.SetInsertPoint(afterBB);
                return nullptr;
            }
            
            // C-style for loop: for (init; cond; update) { body }
            
            // TBB LOOP SAFETY (GAP_2 Fix): Extract counter variable info for sentinel checking
            std::string counter_var_name;
            std::string counter_type_name;
            bool has_tbb_counter = false;
            
            std::cout << "[TBB GUARD DEBUG] Checking for TBB counter in for loop...\n" << std::flush;
            
            if (forStmt->initializer && forStmt->initializer->type == ASTNode::NodeType::VAR_DECL) {
                VarDeclStmt* init_decl = static_cast<VarDeclStmt*>(forStmt->initializer.get());
                counter_var_name = init_decl->varName;
                counter_type_name = init_decl->typeName;
                
                std::cout << "[TBB GUARD DEBUG] Counter: " << counter_var_name << ", Type: " << counter_type_name << "\n" << std::flush;
                
                // Check if counter is TBB type
                has_tbb_counter = (counter_type_name == "tbb8" || counter_type_name == "tbb16" ||
                                  counter_type_name == "tbb32" || counter_type_name == "tbb64");
                
                std::cout << "[TBB GUARD DEBUG] Is TBB counter: " << has_tbb_counter << "\n" << std::flush;
            }
            
            // Generate initializer
            if (forStmt->initializer) {
                codegenStatement(forStmt->initializer.get());
            }
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            
            // Create blocks
            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "forcond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "forbody");
            llvm::BasicBlock* updateBB = llvm::BasicBlock::Create(context, "forupdate");
            llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "afterfor");
            
            // Push loop context (continue goes to updateBB, break goes to afterBB)
            loop_stack.push_back(LoopContext(updateBB, afterBB));
            
            // Jump to condition
            builder.CreateBr(condBB);
            
            // Generate condition
            builder.SetInsertPoint(condBB);
            
            // TBB LOOP SAFETY (GAP_2 Fix): Check for ERR sentinel BEFORE user condition
            // This prevents infinite loops when counter overflows to ERR value
            std::cout << "[TBB GUARD DEBUG] In condition block, has_tbb_counter=" << has_tbb_counter << ", has condition=" << (forStmt->condition != nullptr) << "\n" << std::flush;
            
            if (has_tbb_counter && forStmt->condition) {
                std::cout << "[TBB GUARD DEBUG] Entering sentinel guard injection...\n" << std::flush;
                
                // Get counter variable alloca
                auto counter_it = named_values.find(counter_var_name);
                std::cout << "[TBB GUARD DEBUG] Looking for '" << counter_var_name << "' in named_values...\n" << std::flush;
                std::cout << "[TBB GUARD DEBUG] Found: " << (counter_it != named_values.end()) << "\n" << std::flush;
                
                if (counter_it != named_values.end()) {
                    std::cout << "[TBB GUARD DEBUG] INJECTING SENTINEL GUARD!\n" << std::flush;
                    
                    llvm::Value* counter_alloca = counter_it->second;
                    
                    // Load current counter value
                    llvm::Type* counter_type = mapTypeFromName(counter_type_name);
                    llvm::Value* counter_val = builder.CreateLoad(counter_type, counter_alloca, "counter_val");
                    
                    // Get ERR sentinel for this TBB type
                    llvm::Value* err_sentinel = nullptr;
                    if (counter_type_name == "tbb8") {
                        err_sentinel = llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), -128, true);
                    } else if (counter_type_name == "tbb16") {
                        err_sentinel = llvm::ConstantInt::get(llvm::Type::getInt16Ty(context), -32768, true);
                    } else if (counter_type_name == "tbb32") {
                        err_sentinel = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), -2147483648LL, true);
                    } else if (counter_type_name == "tbb64") {
                        err_sentinel = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), INT64_MIN, true);
                    }
                    
                    // Check if counter == ERR
                    llvm::Value* is_err = builder.CreateICmpEQ(counter_val, err_sentinel, "is_err_sentinel");
                    
                    // Create user condition block
                    llvm::BasicBlock* userCondBB = llvm::BasicBlock::Create(context, "forcond.user", function);
                    
                    // If ERR, exit loop immediately; otherwise check user condition
                    builder.CreateCondBr(is_err, afterBB, userCondBB);
                    
                    std::cout << "[TBB GUARD DEBUG] Sentinel guard complete. Created conditional branch.\n" << std::flush;
                    
                    // Continue in user condition block
                    builder.SetInsertPoint(userCondBB);
                }
            }
            
            if (forStmt->condition) {
                llvm::Value* condVal = codegenExpression(forStmt->condition.get());
                if (!condVal) return nullptr;
                condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "forcond");
                builder.CreateCondBr(condVal, bodyBB, afterBB);
            } else {
                // No condition = infinite loop
                builder.CreateBr(bodyBB);
            }
            
            // Generate body
            function->insert(function->end(), bodyBB);
            builder.SetInsertPoint(bodyBB);
            codegenStatement(forStmt->body.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(updateBB);
            }
            
            // Generate update
            function->insert(function->end(), updateBB);
            builder.SetInsertPoint(updateBB);
            
            std::cout << "[FOR DEBUG] Update block - has update: " << (forStmt->update != nullptr) << "\n" << std::flush;
            
            if (forStmt->update) {
                std::cout << "[FOR DEBUG] Generating update expression...\n" << std::flush;
                std::cout << "[FOR DEBUG] Update node type: " << static_cast<int>(forStmt->update->type) << "\n" << std::flush;
                codegenExpression(forStmt->update.get());  // FIXED: Use codegenExpression not codegenStatement
                std::cout << "[FOR DEBUG] Update expression complete\n" << std::flush;
            } else {
                std::cout << "[FOR DEBUG] NO UPDATE EXPRESSION!\n" << std::flush;
            }
            builder.CreateBr(condBB);  // Loop back
            
            // Pop loop context
            loop_stack.pop_back();
            
            // Continue after loop
            function->insert(function->end(), afterBB);
            builder.SetInsertPoint(afterBB);
            return nullptr;
        }
        
        case ASTNode::NodeType::BREAK: {
            // Break statement - jump to loop exit
            BreakStmt* breakStmt = static_cast<BreakStmt*>(stmt);
            (void)breakStmt;
            
            if (loop_stack.empty()) {
                // Error: break outside of loop
                // TODO: Emit proper error message
                return nullptr;
            }
            
            // Jump to the break target (loop exit)
            llvm::BasicBlock* breakTarget = loop_stack.back().breakTarget;
            builder.CreateBr(breakTarget);
            
            // Create unreachable block after break (for subsequent code)
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            llvm::BasicBlock* afterBreakBB = llvm::BasicBlock::Create(context, "afterbreak", function);
            builder.SetInsertPoint(afterBreakBB);
            
            return nullptr;
        }
        
        case ASTNode::NodeType::CONTINUE: {
            // Continue statement - jump to loop start/update
            ContinueStmt* continueStmt = static_cast<ContinueStmt*>(stmt);
            (void)continueStmt;
            
            if (loop_stack.empty()) {
                // Error: continue outside of loop
                // TODO: Emit proper error message
                return nullptr;
            }
            
            // Jump to the continue target (loop condition/update)
            llvm::BasicBlock* continueTarget = loop_stack.back().continueTarget;
            builder.CreateBr(continueTarget);
            
            // Create unreachable block after continue (for subsequent code)
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            llvm::BasicBlock* afterContinueBB = llvm::BasicBlock::Create(context, "aftercontinue", function);
            builder.SetInsertPoint(afterContinueBB);
            
            return nullptr;
        }
        
        case ASTNode::NodeType::DEFER: {
            // Defer statement - register block for later execution at scope exit
            DeferStmt* deferStmt = static_cast<DeferStmt*>(stmt);
            
            if (defer_stack.empty()) {
                // Error: defer outside of scope
                // TODO: Emit proper error message
                return nullptr;
            }
            
            // Get the defer block and add to current scope
            BlockStmt* defer_block = static_cast<BlockStmt*>(deferStmt->block.get());
            defer_stack.back().push_back(defer_block);
            
            // Note: Actual execution happens at scope exit via executeScopeDefers()
            return nullptr;
        }
        
        case ASTNode::NodeType::STRUCT_DECL: {
            // Struct declaration - type is already registered in TypeChecker
            // IR generation happens lazily in mapType() when the type is first used
            // No code needs to be generated for the declaration itself
            return nullptr;
        }
        
        case ASTNode::NodeType::ENUM_DECL: {
            // Enum declaration - variants should already be registered in processModuleDeclarations
            // This case handles enums declared inside function bodies (edge case)
            EnumDeclStmt* enumDecl = static_cast<EnumDeclStmt*>(stmt);

            // Register each variant in the enum_constants map
            for (const auto& [variantName, variantValue] : enumDecl->variants) {
                std::string fullName = enumDecl->enumName + "." + variantName;
                enum_constants[fullName] = variantValue;
            }

            // No code needs to be generated for the declaration itself
            // Enum values are compile-time constants that will be inlined
            return nullptr;
        }

        case ASTNode::NodeType::EXTERN: {
            // Extern block - generate declarations for all contained functions
            ExternStmt* externStmt = static_cast<ExternStmt*>(stmt);

            // Helper lambda to map FFI type names to LLVM types
            auto mapFFIType = [this](ASTNode* typeNode) -> llvm::Type* {
                if (!typeNode) return builder.getVoidTy();

                // Handle SimpleType (type name as string)
                if (typeNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                    SimpleType* simpleType = static_cast<SimpleType*>(typeNode);
                    std::string typeName = simpleType->typeName;

                    // Check for pointer types (ending with *)
                    bool isPointer = false;
                    while (!typeName.empty() && typeName.back() == '*') {
                        isPointer = true;
                        typeName.pop_back();
                    }

                    // Map base type
                    llvm::Type* baseType = nullptr;
                    if (typeName == "void") baseType = builder.getVoidTy();
                    else if (typeName == "int8" || typeName == "byte") baseType = builder.getInt8Ty();
                    else if (typeName == "int16") baseType = builder.getInt16Ty();
                    else if (typeName == "int32") baseType = builder.getInt32Ty();
                    else if (typeName == "int64") baseType = builder.getInt64Ty();
                    else if (typeName == "float32" || typeName == "flt32") baseType = builder.getFloatTy();
                    else if (typeName == "float64" || typeName == "flt64") baseType = builder.getDoubleTy();
                    else {
                        // Unknown type - treat as opaque (ptr)
                        return builder.getPtrTy();
                    }

                    if (isPointer || baseType == builder.getVoidTy()) {
                        return builder.getPtrTy();
                    }
                    return baseType;
                }

                // Default to ptr for unknown types
                return builder.getPtrTy();
            };

            for (const auto& decl : externStmt->declarations) {
                if (decl->type == ASTNode::NodeType::FUNC_DECL) {
                    // Generate extern function declaration
                    FuncDeclStmt* funcDecl = static_cast<FuncDeclStmt*>(decl.get());

                    // Get return type
                    llvm::Type* returnType = mapFFIType(funcDecl->returnType.get());

                    // Get parameter types
                    std::vector<llvm::Type*> paramTypes;
                    for (const auto& param : funcDecl->parameters) {
                        if (param->type == ASTNode::NodeType::PARAMETER) {
                            ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                            llvm::Type* paramType = mapFFIType(paramNode->typeNode.get());
                            paramTypes.push_back(paramType);
                        }
                    }

                    // Create function type
                    llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);

                    // Create or get function declaration
                    llvm::Function* func = module->getFunction(funcDecl->funcName);
                    if (!func) {
                        func = llvm::Function::Create(
                            funcType,
                            llvm::Function::ExternalLinkage,
                            funcDecl->funcName,
                            module.get()
                        );
                    } else {
                        // P0: Update linkage to external for extern declarations
                        // This handles the case where a function was called before its extern declaration
                        func->setLinkage(llvm::Function::ExternalLinkage);
                    }

                    // FFI-STRING-RETURN: Track extern functions that return string type.
                    if (funcDecl->returnType &&
                        funcDecl->returnType->type == ASTNode::NodeType::TYPE_ANNOTATION) {
                        SimpleType* retType = static_cast<SimpleType*>(funcDecl->returnType.get());
                        if (retType->typeName == "string") {
                            var_aria_types["__ffi_ret_" + funcDecl->funcName] = "string";
                        }
                    }
                }
                else if (decl->type == ASTNode::NodeType::OPAQUE_STRUCT) {
                    // Opaque structs map to ptr (void*) in IR - no struct needed
                    // The type name is registered for use in type resolution
                    OpaqueStructDecl* opaqueDecl = static_cast<OpaqueStructDecl*>(decl.get());
                    // Register as opaque type (handled by type system)
                    (void)opaqueDecl; // Suppress unused warning
                }
                else if (decl->type == ASTNode::NodeType::STRUCT_DECL) {
                    // Struct declaration in extern block - register the struct type
                    // Handled same as regular struct declarations
                }
            }
            return nullptr;
        }

        case ASTNode::NodeType::OPAQUE_STRUCT: {
            // Opaque struct declaration - no IR generation needed
            // The type is registered in the type system and maps to ptr/void*
            return nullptr;
        }

        case ASTNode::NodeType::PICK: {
            // Pick statement - pattern matching
            PickStmt* pickStmt = static_cast<PickStmt*>(stmt);
            
            // Evaluate selector once
            llvm::Value* selector = codegenExpression(pickStmt->selector.get());
            if (!selector) {
                throw std::runtime_error("Failed to generate selector for pick statement");
            }
            
            llvm::Function* func = builder.GetInsertBlock()->getParent();
            llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "pick.end");
            
            // Generate check and body blocks for each case
            std::vector<llvm::BasicBlock*> check_blocks;
            std::vector<llvm::BasicBlock*> body_blocks;
            
            for (size_t i = 0; i < pickStmt->cases.size(); i++) {
                llvm::BasicBlock* check_block = llvm::BasicBlock::Create(context, "case.check");
                llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "case.body");
                check_blocks.push_back(check_block);
                body_blocks.push_back(body_block);
            }

            // Register labeled blocks for fall() support
            std::map<std::string, llvm::BasicBlock*> labeled_blocks;
            for (size_t i = 0; i < pickStmt->cases.size(); i++) {
                PickCase* pc = static_cast<PickCase*>(pickStmt->cases[i].get());
                if (!pc->label.empty()) {
                    labeled_blocks[pc->label] = body_blocks[i];
                }
            }
            pick_context_stack.push_back(labeled_blocks);
            
            // Branch to first check
            if (!check_blocks.empty()) {
                builder.CreateBr(check_blocks[0]);
            } else {
                builder.CreateBr(end_block);
            }
            
            // Generate code for each case
            for (size_t i = 0; i < pickStmt->cases.size(); i++) {
                PickCase* pick_case = static_cast<PickCase*>(pickStmt->cases[i].get());
                llvm::BasicBlock* check_block = check_blocks[i];
                llvm::BasicBlock* body_block = body_blocks[i];
                llvm::BasicBlock* next_check = (i + 1 < check_blocks.size()) ? check_blocks[i + 1] : end_block;
                
                // Check block: evaluate pattern match
                check_block->insertInto(func);
                builder.SetInsertPoint(check_block);
                
                // Check for wildcard pattern (*)
                bool is_wildcard = false;
                if (pick_case->pattern->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* ident = static_cast<IdentifierExpr*>(pick_case->pattern.get());
                    if (ident->name == "*") {
                        is_wildcard = true;
                    }
                }
                
                if (is_wildcard) {
                    // Wildcard always matches
                    builder.CreateBr(body_block);
                } else {
                    // Generate pattern value
                    std::cerr << "[DEBUG PICK] Pattern node type: " << static_cast<int>(pick_case->pattern->type) << std::endl;
                    llvm::Value* pattern_val = codegenExpression(pick_case->pattern.get());
                    if (!pattern_val) {
                        // Failed to generate pattern - might be wildcard represented differently
                        // Just skip to next (treat as wildcard)
                        builder.CreateBr(body_block);
                        goto skip_comparison;
                    }
                    
                    // Match types if needed
                    if (selector->getType() != pattern_val->getType()) {
                        std::cerr << "[DEBUG PICK] Type mismatch - selector: ";
                        selector->getType()->print(llvm::errs());
                        std::cerr << " pattern: ";
                        pattern_val->getType()->print(llvm::errs());
                        std::cerr << std::endl;
                        
                        // If types are fundamentally incompatible, treat as wildcard fallthrough
                        if (!selector->getType()->isIntegerTy() || !pattern_val->getType()->isIntegerTy()) {
                            std::cerr << "[DEBUG PICK] Non-integer types - treating as wildcard" << std::endl;
                            builder.CreateBr(body_block);
                            goto skip_comparison;
                        }
                        
                        llvm::IntegerType* selIntTy = llvm::cast<llvm::IntegerType>(selector->getType());
                        llvm::IntegerType* patIntTy = llvm::cast<llvm::IntegerType>(pattern_val->getType());
                        if (patIntTy->getBitWidth() < selIntTy->getBitWidth()) {
                            std::cerr << "[DEBUG PICK] Extending pattern from " << patIntTy->getBitWidth() << " to " << selIntTy->getBitWidth() << std::endl;
                            pattern_val = builder.CreateSExt(pattern_val, selector->getType(), "pat_sext");
                        } else if (patIntTy->getBitWidth() > selIntTy->getBitWidth()) {
                            std::cerr << "[DEBUG PICK] Truncating pattern from " << patIntTy->getBitWidth() << " to " << selIntTy->getBitWidth() << std::endl;
                            pattern_val = builder.CreateTrunc(pattern_val, selector->getType(), "pat_trunc");
                        }
                    }
                    
                    // Compare selector with pattern
                    llvm::Value* match_result;
                    if (selector->getType()->isIntegerTy()) {
                        std::cerr << "[DEBUG PICK MATCH] Comparing selector type: ";
                        selector->getType()->print(llvm::errs());
                        std::cerr << " with pattern type: ";
                        pattern_val->getType()->print(llvm::errs());
                        std::cerr << std::endl;
                        
                        // Debug: print constant values if available
                        if (llvm::ConstantInt* selConst = llvm::dyn_cast<llvm::ConstantInt>(selector)) {
                            std::cerr << "[DEBUG PICK] Selector constant value: " << selConst->getSExtValue() << std::endl;
                        }
                        if (llvm::ConstantInt* patConst = llvm::dyn_cast<llvm::ConstantInt>(pattern_val)) {
                            std::cerr << "[DEBUG PICK] Pattern constant value: " << patConst->getSExtValue() << std::endl;
                        }
                        
                        match_result = builder.CreateICmpEQ(selector, pattern_val, "match");
                    } else {
                        throw std::runtime_error("Unsupported selector type in pick");
                    }
                    
                    // Branch based on match
                    builder.CreateCondBr(match_result, body_block, next_check);
                }
                
skip_comparison:
                
                // Body block: execute case body
                body_block->insertInto(func);
                builder.SetInsertPoint(body_block);
                codegenStatement(pick_case->body.get());
                
                // Branch to end if no terminator
                if (!builder.GetInsertBlock()->getTerminator()) {
                    builder.CreateBr(end_block);
                }
            }
            
            // End block (continuation after pick)
            end_block->insertInto(func);
            builder.SetInsertPoint(end_block);
            // Pop pick context
            pick_context_stack.pop_back();
            return nullptr;
        }

        case ASTNode::NodeType::PASS: {
            // Pass statement - return success with value
            // Builds Result{val: value, err: NULL, is_error: false}
            PassStmt* passStmt = static_cast<PassStmt*>(stmt);
            
            // Execute all defer blocks (LIFO order) before returning
            executeFunctionDefers();
            
            // Generate code for the value expression
            llvm::Value* value = codegenExpression(passStmt->value.get());
            if (value) {
                // Phase 4.6: Async functions store Result to promise and branch to suspend
                if (current_func_is_async && current_async_promise && current_async_result_type) {
                    llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(current_async_result_type);
                    llvm::Value* result = llvm::UndefValue::get(resultStruct);

                    // Field 0: value (with type conversion)
                    llvm::Type* valueFieldType = resultStruct->getElementType(0);
                    if (value->getType() != valueFieldType) {
                        if (value->getType()->isIntegerTy() && valueFieldType->isIntegerTy()) {
                            value = builder.CreateIntCast(value, valueFieldType, true, "async.val.cast");
                        } else if (value->getType()->isStructTy() && !valueFieldType->isStructTy()) {
                            // Unwrap nested Result or optional
                            llvm::StructType* valSt = llvm::cast<llvm::StructType>(value->getType());
                            if (valSt->getNumElements() == 3 && valSt->getElementType(1)->isPointerTy()
                                && valSt->getElementType(2)->isIntegerTy(8)) {
                                value = builder.CreateExtractValue(value, {0}, "async.unwrap.result");
                            }
                        }
                    }
                    result = builder.CreateInsertValue(result, value, 0, "result.val");

                    // Field 1: error = NULL
                    result = builder.CreateInsertValue(result,
                        llvm::ConstantPointerNull::get(
                            llvm::cast<llvm::PointerType>(resultStruct->getElementType(1))),
                        1, "result.err");

                    // Field 2: is_error = false
                    result = builder.CreateInsertValue(result, builder.getInt8(0), 2, "result.is_error");

                    // Store Result to promise
                    builder.CreateStore(result, current_async_promise);

                    // Branch to final suspend (NOT ret)
                    builder.CreateBr(current_coro_suspend_block);
                    return nullptr;
                }

                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::Type* returnType = currentFunc->getReturnType();
                
                // P1-4: Check postconditions before returning
                if (current_func_decl && !current_func_decl->postconditions.empty()) {
                    // Store return value in 'result' variable for postcondition evaluation
                    named_values["result"] = value;
                    
                    for (size_t i = 0; i < current_func_decl->postconditions.size(); i++) {
                        ASTNodePtr postcond = current_func_decl->postconditions[i];
                        
                        // Generate the condition expression
                        llvm::Value* condValue = codegenExpression(postcond.get());
                        
                        if (condValue) {
                            // Create basic blocks for contract pass/fail
                            llvm::BasicBlock* postcondOkBB = llvm::BasicBlock::Create(context, "postcond.ok", currentFunc);
                            llvm::BasicBlock* postcondFailBB = llvm::BasicBlock::Create(context, "postcond.fail", currentFunc);
                            
                            // Branch based on postcondition
                            builder.CreateCondBr(condValue, postcondOkBB, postcondFailBB);
                            
                            // Postcondition failed: return error Result
                            builder.SetInsertPoint(postcondFailBB);
                            
                            if (returnType->isStructTy()) {
                                llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(returnType);
                                llvm::Value* errorMsg = builder.CreateGlobalString(
                                    "Postcondition #" + std::to_string(i + 1) + " failed", 
                                    "postcond.err.msg");
                                
                                llvm::Value* errorResult = llvm::UndefValue::get(resultStruct);
                                errorResult = builder.CreateInsertValue(errorResult,
                                    llvm::Constant::getNullValue(resultStruct->getElementType(0)), 0, "err.val");
                                errorResult = builder.CreateInsertValue(errorResult,
                                    errorMsg, 1, "err.ptr");
                                errorResult = builder.CreateInsertValue(errorResult,
                                    builder.getInt8(1), 2, "err.is_error");
                                builder.CreateRet(errorResult);
                            }
                            
                            // Postcondition passed: continue
                            builder.SetInsertPoint(postcondOkBB);
                        }
                    }
                    
                    // Clean up 'result' from symbol table
                    named_values.erase("result");
                }
                
                // Check if return type is a Result struct {value, error*, is_error}
                if (returnType->isStructTy()) {
                    llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(returnType);
                    
                    // Build Result object: {value, NULL, false}
                    llvm::Value* result = llvm::UndefValue::get(resultStruct);
                    
                    // Field 0: value (may need type conversion)
                    llvm::Type* valueFieldType = resultStruct->getElementType(0);
                    if (value->getType() != valueFieldType) {
                        // Unwrap optional { i1, T } structs from extern pointer returns
                        if (value->getType()->isStructTy() && !valueFieldType->isStructTy()) {
                            llvm::StructType* valSt = llvm::cast<llvm::StructType>(value->getType());
                            if (valSt->getNumElements() == 2 && valSt->getElementType(0)->isIntegerTy(1)) {
                                // Optional wrapper { i1, T } — extract field 1 (the value)
                                value = builder.CreateExtractValue(value, {1}, "unwrap_optional_for_pass");
                            } else if (valSt->getNumElements() == 3 &&
                                       valSt->getElementType(1)->isPointerTy() &&
                                       valSt->getElementType(2)->isIntegerTy(8)) {
                                // Result wrapper { T, ptr, i8 } — extract field 0
                                value = builder.CreateExtractValue(value, {0}, "unwrap_result_for_pass");
                            }
                        }
                        if (value->getType()->isIntegerTy() && valueFieldType->isIntegerTy()) {
                            llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(value->getType());
                            llvm::IntegerType* fldIntTy = llvm::cast<llvm::IntegerType>(valueFieldType);
                            if (valIntTy->getBitWidth() < fldIntTy->getBitWidth()) {
                                value = builder.CreateSExt(value, valueFieldType, "val_sext");
                            } else if (valIntTy->getBitWidth() > fldIntTy->getBitWidth()) {
                                value = builder.CreateTrunc(value, valueFieldType, "val_trunc");
                            }
                        }
                        // LBIM promotion: integer literal (e.g. i128 from u128 suffix) into LBIM struct slot
                        else if (value->getType()->isIntegerTy() && valueFieldType->isStructTy()) {
                            llvm::StructType* fldStruct = llvm::cast<llvm::StructType>(valueFieldType);
                            std::string sn = fldStruct->getName().str();
                            if (sn.find("struct.int") == 0 || sn.find("struct.uint") == 0) {
                                value = promoteToLBIMStruct(value, valueFieldType);
                            }
                            // Optional wrapping: value is T, field expects { i1, T }
                            else if (fldStruct->getNumElements() == 2 &&
                                     fldStruct->getElementType(0)->isIntegerTy(1)) {
                                llvm::Type* innerType = fldStruct->getElementType(1);
                                llvm::Value* castVal = value;
                                if (value->getType() != innerType) {
                                    if (value->getType()->isIntegerTy() && innerType->isIntegerTy()) {
                                        castVal = builder.CreateIntCast(value, innerType, true, "opt_wrap_cast");
                                    }
                                }
                                llvm::Value* opt = llvm::UndefValue::get(fldStruct);
                                opt = builder.CreateInsertValue(opt, builder.getTrue(), 0, "opt.has_val");
                                opt = builder.CreateInsertValue(opt, castVal, 1, "opt.val");
                                value = opt;
                            }
                        }
                        // Optional wrapping for non-integer types (pointers, floats)
                        else if (!value->getType()->isIntegerTy() && valueFieldType->isStructTy()) {
                            llvm::StructType* fldStruct = llvm::cast<llvm::StructType>(valueFieldType);
                            if (fldStruct->getNumElements() == 2 &&
                                fldStruct->getElementType(0)->isIntegerTy(1)) {
                                llvm::Type* innerType = fldStruct->getElementType(1);
                                if (value->getType() == innerType) {
                                    llvm::Value* opt = llvm::UndefValue::get(fldStruct);
                                    opt = builder.CreateInsertValue(opt, builder.getTrue(), 0, "opt.has_val");
                                    opt = builder.CreateInsertValue(opt, value, 1, "opt.val");
                                    value = opt;
                                }
                            }
                        }
                    }
                    result = builder.CreateInsertValue(result, value, 0, "result.val");
                    
                    // Field 1: error = NULL
                    llvm::Value* nullError = llvm::ConstantPointerNull::get(
                        llvm::cast<llvm::PointerType>(resultStruct->getElementType(1)));
                    result = builder.CreateInsertValue(result, nullError, 1, "result.err");
                    
                    // Field 2: is_error = false
                    llvm::Value* falseVal = builder.getInt8(0);
                    result = builder.CreateInsertValue(result, falseVal, 2, "result.is_error");
                    
                    builder.CreateRet(result);
                } else {
                    // Fallback: old behavior for non-Result returns (extern functions, etc.)
                    if (value->getType() != returnType) {
                        if (value->getType()->isIntegerTy() && returnType->isIntegerTy()) {
                            llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(value->getType());
                            llvm::IntegerType* retIntTy = llvm::cast<llvm::IntegerType>(returnType);
                            if (valIntTy->getBitWidth() < retIntTy->getBitWidth()) {
                                value = builder.CreateSExt(value, returnType, "ret_sext");
                            } else if (valIntTy->getBitWidth() > retIntTy->getBitWidth()) {
                                value = builder.CreateTrunc(value, returnType, "ret_trunc");
                            }
                        }
                    }
                    builder.CreateRet(value);
                }
            }
            return nullptr;
        }
        
        case ASTNode::NodeType::FAIL: {
            // Fail statement - return failure with error code
            // Builds Result{val: zero, err: error_code, is_error: true}
            FailStmt* failStmt = static_cast<FailStmt*>(stmt);
            
            // Execute all defer blocks (LIFO order) before returning
            executeFunctionDefers();
            
            // Generate code for the error code expression
            llvm::Value* errorCode = codegenExpression(failStmt->errorCode.get());
            if (errorCode) {
                // Phase 4.6: Async functions store error Result to promise and suspend
                if (current_func_is_async && current_async_promise && current_async_result_type) {
                    llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(current_async_result_type);
                    llvm::Value* result = llvm::UndefValue::get(resultStruct);

                    // Field 0: value = zero
                    llvm::Type* valueFieldType = resultStruct->getElementType(0);
                    result = builder.CreateInsertValue(result,
                        llvm::Constant::getNullValue(valueFieldType), 0, "result.val");

                    // Field 1: error = inttoptr(error_code)
                    llvm::Value* errorPtr;
                    if (errorCode->getType()->isIntegerTy()) {
                        errorPtr = builder.CreateIntToPtr(errorCode,
                            llvm::PointerType::get(context, 0), "err_ptr");
                    } else {
                        errorPtr = errorCode;
                    }
                    result = builder.CreateInsertValue(result, errorPtr, 1, "result.err");

                    // Field 2: is_error = true
                    result = builder.CreateInsertValue(result, builder.getInt8(1), 2, "result.is_error");

                    builder.CreateStore(result, current_async_promise);
                    builder.CreateBr(current_coro_suspend_block);
                    return nullptr;
                }

                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::Type* returnType = currentFunc->getReturnType();
                
                // Check if return type is a Result struct {value, error*, is_error}
                if (returnType->isStructTy()) {
                    llvm::StructType* resultStruct = llvm::cast<llvm::StructType>(returnType);
                    
                    // Build Result object: {zero_value, error_ptr, true}
                    llvm::Value* result = llvm::UndefValue::get(resultStruct);
                    
                    // Field 0: value = zero/default (TODO: proper nil value for non-integer types)
                    llvm::Type* valueFieldType = resultStruct->getElementType(0);
                    llvm::Value* zeroValue = llvm::Constant::getNullValue(valueFieldType);
                    result = builder.CreateInsertValue(result, zeroValue, 0, "result.val");
                    
                    // Field 1: error = cast error_code to void*
                    // For now, store error code as inttoptr
                    // TODO: Proper TBB error type system
                    llvm::Value* errorPtr;
                    if (errorCode->getType()->isIntegerTy()) {
                        errorPtr = builder.CreateIntToPtr(errorCode, 
                            llvm::PointerType::get(context, 0), "err_ptr");
                    } else {
                        // Already a pointer or other type - cast to void*
                        errorPtr = builder.CreateBitCast(errorCode,
                            llvm::PointerType::get(context, 0), "err_ptr");
                    }
                    result = builder.CreateInsertValue(result, errorPtr, 1, "result.err");
                    
                    // Field 2: is_error = true
                    llvm::Value* trueVal = builder.getInt8(1);
                    result = builder.CreateInsertValue(result, trueVal, 2, "result.is_error");
                    
                    builder.CreateRet(result);
                } else {
                    // Fallback: old behavior for non-Result returns
                    if (errorCode->getType() != returnType) {
                        if (errorCode->getType()->isIntegerTy() && returnType->isIntegerTy()) {
                            llvm::IntegerType* valIntTy = llvm::cast<llvm::IntegerType>(errorCode->getType());
                            llvm::IntegerType* retIntTy = llvm::cast<llvm::IntegerType>(returnType);
                            if (valIntTy->getBitWidth() < retIntTy->getBitWidth()) {
                                errorCode = builder.CreateSExt(errorCode, returnType, "ret_sext");
                            } else if (valIntTy->getBitWidth() > retIntTy->getBitWidth()) {
                                errorCode = builder.CreateTrunc(errorCode, returnType, "ret_trunc");
                            }
                        }
                    }
                    builder.CreateRet(errorCode);
                }
            }
            return nullptr;
        }
        
        case ASTNode::NodeType::TILL: {
            // Till loop: till(limit, step) { body }
            TillStmt* tillStmt = static_cast<TillStmt*>(stmt);
            
            // Evaluate limit and step
            llvm::Value* limitVal = codegenExpression(tillStmt->limit.get());
            llvm::Value* stepVal = codegenExpression(tillStmt->step.get());
            if (!limitVal || !stepVal) return nullptr;
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            llvm::Type* counterType = limitVal->getType();
            
            // Create basic blocks
            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "till.cond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "till.body");
            llvm::BasicBlock* incBB = llvm::BasicBlock::Create(context, "till.inc");
            llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "till.end");
            
            // Save predecessor block
            llvm::BasicBlock* predBB = builder.GetInsertBlock();
            
            // Jump to condition
            builder.CreateBr(condBB);
            
            // Condition block with PHI node for $
            builder.SetInsertPoint(condBB);
            llvm::PHINode* counterPhi = builder.CreatePHI(counterType, 2, "$");
            llvm::Value* initVal = llvm::ConstantInt::get(counterType, 0);
            counterPhi->addIncoming(initVal, predBB);
            
            // Save old $ value
            llvm::Value* oldDollar = nullptr;
            auto it = named_values.find("$");
            if (it != named_values.end()) {
                oldDollar = it->second;
            }
            
            // Create alloca for $ variable
            llvm::AllocaInst* dollarAlloca = builder.CreateAlloca(counterType, nullptr, "$");
            builder.CreateStore(counterPhi, dollarAlloca);
            named_values["$"] = dollarAlloca;
            
            // Condition: $ != limit
            llvm::Value* condVal = builder.CreateICmpNE(counterPhi, limitVal, "till.cond");
            builder.CreateCondBr(condVal, bodyBB, afterBB);
            
            // Push loop context
            loop_stack.push_back(LoopContext(incBB, afterBB));
            
            // Generate body
            function->insert(function->end(), bodyBB);
            builder.SetInsertPoint(bodyBB);
            codegenStatement(tillStmt->body.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(incBB);
            }
            
            // Increment block
            function->insert(function->end(), incBB);
            builder.SetInsertPoint(incBB);
            llvm::Value* currentVal = builder.CreateLoad(counterType, dollarAlloca, "$");
            llvm::Value* nextVal = builder.CreateAdd(currentVal, stepVal, "$.next");
            counterPhi->addIncoming(nextVal, incBB);
            builder.CreateBr(condBB);
            
            // Pop loop context
            loop_stack.pop_back();
            
            // Restore old $ value
            if (oldDollar) {
                named_values["$"] = oldDollar;
            } else {
                named_values.erase("$");
            }
            
            // Continue after loop
            function->insert(function->end(), afterBB);
            builder.SetInsertPoint(afterBB);
            return nullptr;
        }
        
        case ASTNode::NodeType::LOOP: {
            // Loop: loop(start, limit, step) { body }
            LoopStmt* loopStmt = static_cast<LoopStmt*>(stmt);
            
            // Evaluate start, limit, and step
            llvm::Value* startVal = codegenExpression(loopStmt->start.get());
            llvm::Value* limitVal = codegenExpression(loopStmt->limit.get());
            llvm::Value* stepVal = codegenExpression(loopStmt->step.get());
            if (!startVal || !limitVal || !stepVal) return nullptr;
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            llvm::Type* counterType = startVal->getType();
            
            // Create basic blocks
            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "loop.cond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "loop.body");
            llvm::BasicBlock* incBB = llvm::BasicBlock::Create(context, "loop.inc");
            llvm::BasicBlock* afterBB = llvm::BasicBlock::Create(context, "loop.end");
            
            // Save predecessor block
            llvm::BasicBlock* predBB = builder.GetInsertBlock();
            
            // Jump to condition
            builder.CreateBr(condBB);
            
            // Condition block with PHI node for $
            builder.SetInsertPoint(condBB);
            llvm::PHINode* counterPhi = builder.CreatePHI(counterType, 2, "$");
            counterPhi->addIncoming(startVal, predBB);
            
            // Save old $ value
            llvm::Value* oldDollar = nullptr;
            auto it = named_values.find("$");
            if (it != named_values.end()) {
                oldDollar = it->second;
            }
            
            // Create alloca for $ variable
            llvm::AllocaInst* dollarAlloca = builder.CreateAlloca(counterType, nullptr, "$");
            builder.CreateStore(counterPhi, dollarAlloca);
            named_values["$"] = dollarAlloca;
            
            // Condition: $ != limit
            llvm::Value* condVal = builder.CreateICmpNE(counterPhi, limitVal, "loop.cond");
            builder.CreateCondBr(condVal, bodyBB, afterBB);
            
            // Push loop context
            loop_stack.push_back(LoopContext(incBB, afterBB));
            
            // Generate body
            function->insert(function->end(), bodyBB);
            builder.SetInsertPoint(bodyBB);
            codegenStatement(loopStmt->body.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(incBB);
            }
            
            // Increment block
            function->insert(function->end(), incBB);
            builder.SetInsertPoint(incBB);
            llvm::Value* currentVal = builder.CreateLoad(counterType, dollarAlloca, "$");
            llvm::Value* nextVal = builder.CreateAdd(currentVal, stepVal, "$.next");
            counterPhi->addIncoming(nextVal, incBB);
            builder.CreateBr(condBB);
            
            // Pop loop context
            loop_stack.pop_back();
            
            // Restore old $ value
            if (oldDollar) {
                named_values["$"] = oldDollar;
            } else {
                named_values.erase("$");
            }
            
            // Continue after loop
            function->insert(function->end(), afterBB);
            builder.SetInsertPoint(afterBB);
            return nullptr;
        }
        
        case ASTNode::NodeType::WHEN: {
            // When loop: when(cond) { body } then { } end { }
            WhenStmt* whenStmt = static_cast<WhenStmt*>(stmt);
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            
            // Create completion flag
            llvm::AllocaInst* completedFlag = builder.CreateAlloca(builder.getInt1Ty(), nullptr, "when.completed");
            builder.CreateStore(builder.getInt1(false), completedFlag);
            
            // Create basic blocks
            llvm::BasicBlock* condBB = llvm::BasicBlock::Create(context, "when.cond", function);
            llvm::BasicBlock* bodyBB = llvm::BasicBlock::Create(context, "when.body");
            llvm::BasicBlock* decisionBB = llvm::BasicBlock::Create(context, "when.decision");
            llvm::BasicBlock* thenBB = whenStmt->then_block ? llvm::BasicBlock::Create(context, "when.then") : nullptr;
            llvm::BasicBlock* endBB = whenStmt->end_block ? llvm::BasicBlock::Create(context, "when.end") : nullptr;
            llvm::BasicBlock* exitBB = llvm::BasicBlock::Create(context, "when.exit");
            
            // Jump to condition
            builder.CreateBr(condBB);
            
            // Push loop context
            loop_stack.push_back(LoopContext(condBB, decisionBB));
            
            // Condition block
            builder.SetInsertPoint(condBB);
            llvm::Value* condVal = codegenExpression(whenStmt->condition.get());
            if (!condVal) return nullptr;
            condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "when.cond");
            builder.CreateCondBr(condVal, bodyBB, decisionBB);
            
            // Body block
            function->insert(function->end(), bodyBB);
            builder.SetInsertPoint(bodyBB);
            codegenStatement(whenStmt->body.get());
            if (!builder.GetInsertBlock()->getTerminator()) {
                // Mark body ran at least once before looping
                builder.CreateStore(builder.getInt1(true), completedFlag);
                builder.CreateBr(condBB);
            }
            
            // Decision block
            function->insert(function->end(), decisionBB);
            builder.SetInsertPoint(decisionBB);
            llvm::Value* completed = builder.CreateLoad(builder.getInt1Ty(), completedFlag, "completed");
            
            if (thenBB && endBB) {
                builder.CreateCondBr(completed, thenBB, endBB);
            } else if (thenBB) {
                builder.CreateCondBr(completed, thenBB, exitBB);
            } else if (endBB) {
                builder.CreateBr(endBB);
            } else {
                builder.CreateBr(exitBB);
            }
            
            // Then block
            if (thenBB) {
                function->insert(function->end(), thenBB);
                builder.SetInsertPoint(thenBB);
                codegenStatement(whenStmt->then_block.get());
                if (!builder.GetInsertBlock()->getTerminator()) {
                    builder.CreateBr(exitBB);
                }
            }
            
            // End block
            if (endBB) {
                function->insert(function->end(), endBB);
                builder.SetInsertPoint(endBB);
                codegenStatement(whenStmt->end_block.get());
                if (!builder.GetInsertBlock()->getTerminator()) {
                    builder.CreateBr(exitBB);
                }
            }
            
            // Pop loop context
            loop_stack.pop_back();
            
            // Exit block
            function->insert(function->end(), exitBB);
            builder.SetInsertPoint(exitBB);
            return nullptr;
        }

        // ========================================================================
        // WP 005: Trait System
        // ========================================================================

        case ASTNode::NodeType::TRAIT_DECL: {
            // Trait declarations are compile-time only - no IR to generate
            return nullptr;
        }

        case ASTNode::NodeType::IMPL_DECL: {
            // Generate functions for each method in the implementation
            ImplDeclStmt* impl = static_cast<ImplDeclStmt*>(stmt);
            for (const auto& method : impl->methods) {
                FuncDeclStmt* func = dynamic_cast<FuncDeclStmt*>(method.get());
                if (!func) continue;

                // Save original name
                std::string originalName = func->funcName;

                // Mangle: TypeName_methodName for UFCS
                func->funcName = impl->typeName + "_" + originalName;

                // Generate the function (inline codegen here since we don't have separate method)
                // Build parameter types
                std::vector<llvm::Type*> paramTypes;
                for (const auto& param : func->parameters) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                    std::string paramTypeStr = paramNode->typeNode ? paramNode->typeNode->toString() : "void";
                    paramTypes.push_back(mapTypeFromName(paramTypeStr));
                }

                // Get return type
                std::string returnTypeStr = func->returnType ? func->returnType->toString() : "void";
                llvm::Type* returnType = mapTypeFromName(returnTypeStr);

                // Create function type and function
                llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, paramTypes, false);
                llvm::Function* llvmFunc = llvm::Function::Create(
                    funcType, llvm::Function::ExternalLinkage, func->funcName, module.get());

                // Set parameter names
                unsigned idx = 0;
                for (auto& arg : llvmFunc->args()) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(func->parameters[idx].get());
                    arg.setName(paramNode->paramName);
                    idx++;
                }

                // Create entry block and generate body
                llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", llvmFunc);
                builder.SetInsertPoint(entry);

                // Add parameters to named_values
                idx = 0;
                for (auto& arg : llvmFunc->args()) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(func->parameters[idx].get());
                    std::string paramTypeStr = paramNode->typeNode ? paramNode->typeNode->toString() : "void";
                    llvm::Type* paramType = mapTypeFromName(paramTypeStr);
                    llvm::AllocaInst* alloca = builder.CreateAlloca(paramType, nullptr, paramNode->paramName);
                    builder.CreateStore(&arg, alloca);
                    named_values[paramNode->paramName] = alloca;
                    idx++;
                }

                // Generate body
                if (func->body) {
                    codegenStatement(func->body.get());
                }

                // Add implicit void return if needed
                if (!builder.GetInsertBlock()->getTerminator()) {
                    if (returnType->isVoidTy()) {
                        builder.CreateRetVoid();
                    } else {
                        builder.CreateRet(llvm::Constant::getNullValue(returnType));
                    }
                }

                // Restore original name
                func->funcName = originalName;

                // Clear named_values for next function
                for (const auto& param : func->parameters) {
                    ParameterNode* paramNode = static_cast<ParameterNode*>(param.get());
                    named_values.erase(paramNode->paramName);
                }
            }
            return nullptr;
        }

        case ASTNode::NodeType::FUNC_DECL: {
            // Local function declaration inside a function body (lambda/nested function).
            //
            // Two-path execution:
            //   1. Module-level FUNC_DECLs are handled by processModuleDeclarations.
            //   2. This case handles FUNC_DECLs that appear as statements inside a
            //      function body — they create a normal LLVM module-level function
            //      (LLVM has no nested functions) with InternalLinkage, then restore
            //      the outer function's builder insert point so code generation of
            //      subsequent statements is unaffected.
            //
            // The generated function uses the same Result<T> return convention as
            // all other Aria functions, so the caller can use the ? unwrap operator.

            FuncDeclStmt* localFunc = static_cast<FuncDeclStmt*>(stmt);

            // Skip generic templates — specialisations are handled by monomorphisation
            if (!localFunc->genericParams.empty()) {
                return nullptr;
            }

            // No body → extern-style declaration — nothing to generate locally
            if (!localFunc->body) {
                return nullptr;
            }

            // ---- Save outer context ----------------------------------------
            // Save builder insert point so subsequent outer statements still go
            // into the right basic block after this inner function is created.
            auto savedIP = builder.saveIP();

            // Save outer symbol tables; the inner function gets a fresh scope
            std::map<std::string, llvm::Value*>  outer_named_values  = named_values;
            std::map<std::string, std::string>   outer_var_aria_types = var_aria_types;
            named_values.clear();
            var_aria_types.clear();

            // ---- Build LLVM parameter types --------------------------------
            std::vector<llvm::Type*> inner_param_types;
            for (const auto& param : localFunc->parameters) {
                ParameterNode* pn = static_cast<ParameterNode*>(param.get());
                std::string pstr = pn->typeNode ? pn->typeNode->toString() : "void";
                inner_param_types.push_back(mapTypeFromName(pstr));
            }

            // ---- Build LLVM return type (Result<T> wrapper) ----------------
            std::string rstr = localFunc->returnType ? localFunc->returnType->toString() : "void";
            llvm::Type* inner_raw_ret = mapTypeFromName(rstr);

            llvm::Type*         inner_ret_type      = inner_raw_ret;
            llvm::StructType*   inner_result_struct  = nullptr;

            if (!inner_raw_ret->isVoidTy()) {
                // Regular Aria functions return Result<T> = {T, void*, i8}
                std::vector<llvm::Type*> rf = {
                    inner_raw_ret,
                    llvm::PointerType::get(context, 0),   // error (void*)
                    builder.getInt8Ty()                   // is_error (i8)
                };
                inner_result_struct = llvm::StructType::get(context, rf);
                inner_ret_type      = inner_result_struct;
            }

            llvm::FunctionType* inner_func_type =
                llvm::FunctionType::get(inner_ret_type, inner_param_types, false);

            // ---- Create the LLVM function ----------------------------------
            // Use InternalLinkage for locals (visible only within this translation unit).
            llvm::Function* inner_func = llvm::Function::Create(
                inner_func_type,
                llvm::Function::InternalLinkage,
                localFunc->funcName,
                module.get()
            );

            // ---- Entry block + parameter allocas ---------------------------
            llvm::BasicBlock* inner_entry =
                llvm::BasicBlock::Create(context, "entry", inner_func);
            builder.SetInsertPoint(inner_entry);

            size_t pidx = 0;
            for (auto& arg : inner_func->args()) {
                if (pidx < localFunc->parameters.size()) {
                    ParameterNode* pn =
                        static_cast<ParameterNode*>(localFunc->parameters[pidx].get());
                    arg.setName(pn->paramName);

                    std::string paramTypeStr = pn->typeNode ? pn->typeNode->toString() : "void";
                    llvm::Type* paramType = mapTypeFromName(paramTypeStr);

                    llvm::AllocaInst* alloca =
                        builder.CreateAlloca(paramType, nullptr, pn->paramName);
                    builder.CreateStore(&arg, alloca);

                    named_values[pn->paramName]    = alloca;
                    var_aria_types[pn->paramName]  = paramTypeStr;

                    // Register in value_types for member-access resolution
                    if (type_system) {
                        Type* ariaType = type_system->getStructType(paramTypeStr);
                        if (!ariaType) ariaType = type_system->getPrimitiveType(paramTypeStr);
                        if (ariaType) value_types[alloca] = ariaType;
                    }
                }
                ++pidx;
            }

            // ---- Generate body ---------------------------------------------
            // codegenStatement(BLOCK) handles PASS, FAIL, and all other Aria
            // statement types correctly — this is the same path used by
            // processModuleDeclarations for top-level functions.
            codegenStatement(localFunc->body.get());

            // ---- Default terminator if the body left no terminator ---------
            llvm::BasicBlock* inner_last = builder.GetInsertBlock();
            if (inner_last && !inner_last->getTerminator()) {
                if (inner_ret_type->isVoidTy()) {
                    builder.CreateRetVoid();
                } else if (inner_result_struct) {
                    // Return default success Result{zero, NULL, false}
                    llvm::Value* def = llvm::UndefValue::get(inner_result_struct);
                    def = builder.CreateInsertValue(
                        def, llvm::Constant::getNullValue(inner_raw_ret), 0);
                    def = builder.CreateInsertValue(
                        def,
                        llvm::ConstantPointerNull::get(
                            llvm::cast<llvm::PointerType>(
                                inner_result_struct->getElementType(1))),
                        1);
                    def = builder.CreateInsertValue(def, builder.getInt8(0), 2);
                    builder.CreateRet(def);
                } else {
                    builder.CreateRet(llvm::Constant::getNullValue(inner_ret_type));
                }
            }

            // ---- Restore outer context -------------------------------------
            named_values    = outer_named_values;
            var_aria_types  = outer_var_aria_types;
            builder.restoreIP(savedIP);

            return nullptr;
        }

        case ASTNode::NodeType::FALL: {
            // fall() statement: explicit fallthrough to labeled pick arm
            FallStmt* fallStmt = static_cast<FallStmt*>(stmt);
            if (pick_context_stack.empty()) {
                throw std::runtime_error("fall() statement must be inside a pick statement");
            }
            const auto& ctx = pick_context_stack.back();
            auto it = ctx.find(fallStmt->target_label);
            if (it == ctx.end()) {
                throw std::runtime_error("fall() target '" + fallStmt->target_label + "' not found");
            }
            if (!builder.GetInsertBlock()->getTerminator()) {
                builder.CreateBr(it->second);
            }
            return nullptr;
        }

        case ASTNode::NodeType::COMPTIME_BLOCK: {
            // Comptime block: all evaluation was done at compile time.
            // Nothing to emit at IR level — side effects (const definitions)
            // have already been processed by the type checker.
            return nullptr;
        }

        default:
            // Other statement types not yet implemented
            return nullptr;
    }
}

// =============================================================================
// Expression Code Generation
// =============================================================================

llvm::Value* aria::IRGenerator::codegenExpression(ASTNode* expr) {
    if (!expr) {
        return nullptr;
    }
    
    switch (expr->type) {
        case ASTNode::NodeType::LITERAL: {
            // Literal expression
            LiteralExpr* lit = static_cast<LiteralExpr*>(expr);
            
            // ========================================================================
            // PHASE 4: ZERO IMPLICIT CONVERSION - Use Explicit Type
            // ========================================================================
            // If literal has explicit type suffix (u32, i64, f32, etc.), use it!
            // SKIP this shortcut when hasRawValue() is true FOR INTEGER types: the
            // int64_t value field is 0 for overflow literals (e.g. 18446744073709551616u128
            // > UINT64_MAX), and using it would produce a constant zero.
            // For FLOAT types (f32, f64), the double variant is always accurate, so
            // we can use Phase 4 even when hasRawValue is true.
            {
                bool skipPhase4 = lit->explicit_type.empty();
                if (!skipPhase4 && lit->hasRawValue()) {
                    // Allow float suffixes through (double variant is accurate)
                    llvm::Type* checkType = getLLVMTypeFromSuffix(lit->explicit_type);
                    if (!checkType || !checkType->isFloatingPointTy()) {
                        skipPhase4 = true;  // Integer with raw value — skip
                    }
                }
                if (!skipPhase4) {
                    // Get LLVM type from explicit suffix
                    llvm::Type* llvm_type = getLLVMTypeFromSuffix(lit->explicit_type);
                    
                    if (llvm_type) {
                        // Handle integer types
                        if (llvm_type->isIntegerTy()) {
                            int64_t val = std::get<int64_t>(lit->value);
                            bool is_signed = isSuffixSigned(lit->explicit_type);
                            return llvm::ConstantInt::get(llvm_type, val, is_signed);
                        }
                        // Handle float types
                        else if (llvm_type->isFloatingPointTy()) {
                            double val = std::get<double>(lit->value);
                            return llvm::ConstantFP::get(llvm_type, val);
                        }
                    }
                    // If we can't find type, fall through to old behavior
                }
            }
            
            // Phase 3.2.5: Check for high-precision literal with raw string value
            if (lit->hasRawValue()) {
                const std::string& raw = lit->getRawValue();
                
                // Detect if this is a float or int from the raw string
                bool is_float = (raw.find('.') != std::string::npos || 
                                raw.find('e') != std::string::npos || 
                                raw.find('E') != std::string::npos ||
                                raw == "inf" || raw == "-inf" || raw == "nan");
                
                if (is_float) {
                    // High-precision float literal - use FLT128 if many digits
                    aria::semantic::FloatPrecision precision;
                    if (raw.length() > 17) {  // More digits than float64 can represent
                        precision = aria::semantic::FloatPrecision::FLT128;
                    } else {
                        precision = aria::semantic::FloatPrecision::FLT64;
                    }
                    
                    // Convert using LiteralConverter
                    auto apfloat_opt = aria::semantic::LiteralConverter::convertFloatLiteral(raw, precision);
                    if (apfloat_opt) {
                        llvm::Value* result = llvm::ConstantFP::get(context, *apfloat_opt);
                        return result;
                    }
                    // Fall through to variant handling on conversion failure
                } else {
                    // High-precision integer literal - use INT128 if large
                    unsigned bit_width = 64;
                    bool is_signed = true;
                    
                    // Check if value needs more than 64 bits (more than 19 digits)
                    if (raw.length() > 19) {
                        bit_width = 128;
                    }
                    
                    // Convert using LiteralConverter
                    auto apint_opt = aria::semantic::LiteralConverter::convertIntLiteral(raw, bit_width, is_signed);
                    if (apint_opt) {
                        llvm::Value* result = llvm::ConstantInt::get(context, *apint_opt);
                        return result;
                    }
                    // Fall through to variant handling on conversion failure
                }
            }
            
            // Standard precision: Handle different literal types using variant
            // Check for NULL/NIL literal (monostate)
            if (std::holds_alternative<std::monostate>(lit->value)) {
                // Check if it's NIL or NULL based on explicit_type
                if (lit->explicit_type == "NIL") {
                    // NIL literal - unit type (empty struct)
                    llvm::StructType* nilType = llvm::StructType::getTypeByName(context, "struct.NIL");
                    if (!nilType) {
                        nilType = llvm::StructType::create(context, {}, "struct.NIL");
                    }
                    return llvm::ConstantStruct::get(nilType, {});
                } else {
                    // NULL literal - return null pointer constant
                    // For pointer types, this creates a proper null pointer
                    return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
                }
            } else if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                // Default to int64 for integer literals to match Aria's default integer type
                // Type inference during semantic analysis determines the actual type needed
                return llvm::ConstantInt::get(builder.getInt64Ty(), val);
            } else if (std::holds_alternative<double>(lit->value)) {
                double val = std::get<double>(lit->value);
                return llvm::ConstantFP::get(builder.getDoubleTy(), val);
            } else if (std::holds_alternative<bool>(lit->value)) {
                bool val = std::get<bool>(lit->value);
                return builder.getInt1(val);
            } else if (std::holds_alternative<std::string>(lit->value)) {
                // String literal - check for special literals first
                const std::string& str = std::get<std::string>(lit->value);
                
                // Phase 5.1: Handle special unknown literal
                if (str == "unknown") {
                    // unknown sentinel (opposite of ERR, uses max value)
                    llvm::Type* target_type = llvm::Type::getInt32Ty(context);
                    unsigned width = target_type->getIntegerBitWidth();
                    std::cerr << "[DEBUG IR_GEN] unknown literal - generating sentinel i" << width << std::endl;
                    return llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
                }
                
                // Phase 5.1: Handle special ERR literal
                if (str == "ERR") {
                    // ERR sentinel (minimum signed value)
                    llvm::Type* target_type = llvm::Type::getInt32Ty(context);
                    unsigned width = target_type->getIntegerBitWidth();
                    std::cerr << "[DEBUG IR_GEN] ERR literal - generating sentinel i" << width << std::endl;
                    return llvm::ConstantInt::get(context, llvm::APInt::getSignedMinValue(width));
                }
                
                // Normal string literal - create global AriaString struct (Phase 4.3)
                
                // Create a global constant for the string data  
                llvm::Constant* str_data = llvm::ConstantDataArray::getString(context, str, true);
                llvm::GlobalVariable* str_gv = new llvm::GlobalVariable(
                    *module,
                    str_data->getType(),
                    true,  // isConstant
                    llvm::GlobalValue::PrivateLinkage,
                    str_data,
                    ".str.data"
                );
                
                // Get or create AriaString struct type
                llvm::StructType* aria_string_type = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (!aria_string_type) {
                    std::vector<llvm::Type*> fields = {
                        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                        llvm::Type::getInt64Ty(context)
                    };
                    aria_string_type = llvm::StructType::create(context, fields, "struct.AriaString");
                }
                
                // Create a global AriaString struct constant
                std::vector<llvm::Constant*> struct_values = {
                    llvm::ConstantExpr::getPointerCast(str_gv, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)),
                    builder.getInt64(str.length())
                };
                llvm::Constant* string_struct = llvm::ConstantStruct::get(aria_string_type, struct_values);
                
                llvm::GlobalVariable* string_gv = new llvm::GlobalVariable(
                    *module,
                    aria_string_type,
                    true,  // isConstant
                    llvm::GlobalValue::PrivateLinkage,
                    string_struct,
                    ".str"
                );
                
                // Return the global AriaString struct (will be loaded when needed)
                return string_gv;
            }
            return nullptr;
        }
        
        case ASTNode::NodeType::IDENTIFIER: {
            // Identifier - lookup in symbol table
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
            
            auto it = named_values.find(ident->name);
            if (it != named_values.end()) {
                llvm::Value* val = it->second;
                
                // If it's a pointer (alloca or parameter), load it
                if (val->getType()->isPointerTy()) {
                    // CRITICAL FIX: Function arguments stored directly as pointer values ARE
                    // the value — not a pointer to a value that needs loading. This is the case
                    // for string parameters (string = ptr to AriaString), which are stored in
                    // named_values as the LLVM Argument itself, NOT as an alloca.
                    // Without this check the load defaults to i32, mismatching the ptr type of
                    // a string literal on the other side of a comparison (ICmpInst assert).
                    if (llvm::isa<llvm::Argument>(val)) {
                        auto typeIt = value_types.find(val);
                        (void)typeIt;
                        // (value_types entry was already registered during parameter setup)
                        return val;
                    }
                    // GlobalVariable: look up the stored Aria type to determine load type,
                    // then emit a properly-typed load (avoids the i32-default fallback that
                    // was causing loads of wrong type from bool/string globals).
                    if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(val)) {
                        llvm::Type* loadType = gv->getValueType();
                        // Prefer var_aria_types lookup for semantic accuracy
                        auto ariaIt = var_aria_types.find(ident->name);
                        if (ariaIt != var_aria_types.end()) {
                            llvm::Type* ariaType = mapTypeFromName(ariaIt->second);
                            if (ariaType) loadType = ariaType;
                        }
                        llvm::Value* loaded = builder.CreateLoad(loadType, val, ident->name);
                        auto typeIt = value_types.find(val);
                        if (typeIt != value_types.end()) value_types[loaded] = typeIt->second;
                        return loaded;
                    }
                    // Get the allocated type from the alloca
                    llvm::Type* loadType = builder.getInt32Ty();  // Default
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(val)) {
                        loadType = allocaInst->getAllocatedType();
                        std::cerr << "[DEBUG] Alloca '" << ident->name << "' getAllocatedType()=" 
                                  << (loadType->isStructTy() ? "STRUCT" : "NOT_STRUCT") << std::endl;
                        if (loadType->isStructTy()) {
                            llvm::StructType* st = llvm::cast<llvm::StructType>(loadType);
                            if (st->hasName()) {
                                std::cerr << "[DEBUG]   Allocated type name: " << st->getName().str() << std::endl;
                            }
                        }
                    }
                    
                    // Create the load instruction
                    llvm::Value* loaded = builder.CreateLoad(loadType, val, ident->name);
                    
                    std::cerr << "[DEBUG] Loaded variable '" << ident->name << "' type=" 
                              << (loaded->getType()->isStructTy() ? "STRUCT" : "NOT_STRUCT") << std::endl;
                    if (loaded->getType()->isStructTy()) {
                        llvm::StructType* st = llvm::cast<llvm::StructType>(loaded->getType());
                        if (st->hasName()) {
                            std::cerr << "[DEBUG]   Struct name: " << st->getName().str() << std::endl;
                        }
                    }
                    
                    // Track the Aria type for the loaded value (same as alloca)
                    auto typeIt = value_types.find(val);
                    if (typeIt != value_types.end()) {
                        value_types[loaded] = typeIt->second;
                    }
                    
                    return loaded;
                }
                
                // Otherwise return the value directly (e.g., function parameter)
                return val;
            }
            
            return nullptr;
        }
        
        case ASTNode::NodeType::CALL: {
            // Function call
            CallExpr* call = static_cast<CallExpr*>(expr);
            
            
            // CRITICAL FIX (Dec 31, 2024): Unconditionally delegate ALL calls to ExprCodegen
            // The legacy IRGenerator print() implementation doesn't understand modern types:
            // - LBIM (int256 as struct { [4 x i64] })
            // - AriaString (struct { i8*, i64 })
            // - TBB types with ERR sentinels
            // ExprCodegen has the correct, specification-compliant implementations.
            backend::ExprCodegen expr_codegen(context, builder, module.get(), named_values, var_aria_types, type_system);
            return expr_codegen.codegenCall(call);
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            // Unary operation
            UnaryExpr* unary = static_cast<UnaryExpr*>(expr);
            
            // Special handling for address-of operator (@)
            // MUST be before general operand codegen because we need the ADDRESS not the VALUE
            if (unary->op.type == frontend::TokenType::TOKEN_AT) {
                // Address-of: @variable
                // Operand must be an lvalue (identifier)
                if (unary->operand->type != ASTNode::NodeType::IDENTIFIER) {
                    return nullptr;  // Can only take address of variables
                }
                
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(unary->operand.get());
                auto it = named_values.find(ident->name);
                if (it == named_values.end()) {
                    return nullptr;  // Variable not found
                }
                
                // Return the alloca pointer itself (the ADDRESS), not the loaded value
                llvm::Value* address = it->second;
                
                // If the value is NOT an alloca (e.g., a function parameter stored as
                // a raw SSA value), we need to spill it to a temporary alloca so that
                // the address-of operator has a valid memory location to return.
                if (!llvm::isa<llvm::AllocaInst>(address)) {
                    llvm::AllocaInst* tmp = builder.CreateAlloca(
                        address->getType(), nullptr, ident->name + ".addr");
                    builder.CreateStore(address, tmp);
                    return tmp;
                }
                
                return address;
            }
            
            // Special handling for dereference operator (<-)
            // Blueprint style: arrow shows data flow FROM pointer TO value
            if (unary->op.type == frontend::TokenType::TOKEN_LEFT_ARROW) {
                // Dereference: value <- ptr
                // Generate the pointer value first
                llvm::Value* ptr = codegenExpression(unary->operand.get());
                if (!ptr) return nullptr;
                
                if (!ptr->getType()->isPointerTy()) {
                    return nullptr;  // Type checker should have caught this
                }
                
                // Determine the pointee type
                // For opaque pointers in LLVM 20, we need to track the type separately
                llvm::Type* pointeeType = nullptr;
                
                // Try to infer from the pointer's source
                if (unary->operand->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* ident = static_cast<IdentifierExpr*>(unary->operand.get());
                    auto it = named_values.find(ident->name);
                    if (it != named_values.end()) {
                        llvm::Value* ptrAlloca = it->second;
                        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(ptrAlloca)) {
                            // The alloca holds a pointer, get what that pointer points to
                            // For a variable like "int32->:ptr", the alloca type is "ptr"
                            // We need to look at the Aria type system to know what it points to
                            
                            // Check if we have Aria type info for this variable
                            auto ariaTypeIt = var_aria_types.find(ident->name);
                            if (ariaTypeIt != var_aria_types.end()) {
                                std::string ariaTypeName = ariaTypeIt->second;
                                // Type name is like "int32@" or "int32->" - extract base type
                                if (ariaTypeName.size() > 2 && 
                                    (ariaTypeName.back() == '@' || 
                                     (ariaTypeName.size() > 1 && ariaTypeName.substr(ariaTypeName.size()-2) == "->"))) {
                                    // Strip the pointer suffix
                                    std::string baseTypeName;
                                    if (ariaTypeName.back() == '@') {
                                        baseTypeName = ariaTypeName.substr(0, ariaTypeName.size() - 1);
                                    } else {
                                        baseTypeName = ariaTypeName.substr(0, ariaTypeName.size() - 2);
                                    }
                                    // Map to LLVM type
                                    pointeeType = mapTypeFromName(baseTypeName);
                                }
                            }
                        }
                    }
                }
                
                // Fallback: default to i32 if we can't determine the type
                if (!pointeeType) {
                    pointeeType = builder.getInt32Ty();
                }
                
                // Load the value from the pointer (dereference)
                llvm::Value* derefValue = builder.CreateLoad(pointeeType, ptr, "deref");
                return derefValue;
            }
            
            // Special handling for increment/decrement operators
            if (unary->op.type == frontend::TokenType::TOKEN_PLUS_PLUS ||
                unary->op.type == frontend::TokenType::TOKEN_MINUS_MINUS) {
                
                // Operand must be an lvalue (identifier)
                if (unary->operand->type != ASTNode::NodeType::IDENTIFIER) {
                    return nullptr;
                }
                
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(unary->operand.get());
                auto it = named_values.find(ident->name);
                if (it == named_values.end()) {
                    return nullptr;  // Variable not found
                }
                
                llvm::Value* var = it->second;
                
                // Get the variable type
                llvm::Type* varElemType = nullptr;
                if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                    varElemType = allocaInst->getAllocatedType();
                } else {
                    varElemType = builder.getInt32Ty();
                }
                
                // Load current value
                llvm::Value* currentVal = builder.CreateLoad(varElemType, var, ident->name);
                
                // Increment or decrement
                llvm::Value* one = llvm::ConstantInt::get(varElemType, 1);
                llvm::Value* newVal = nullptr;
                if (unary->op.type == frontend::TokenType::TOKEN_PLUS_PLUS) {
                    // Layer 1 Safety: Safe increment returns Unknown on overflow
                    newVal = generateSafeAdd(currentVal, one, "inctmp");
                } else {
                    // Layer 1 Safety: Safe decrement returns Unknown on underflow
                    newVal = generateSafeSub(currentVal, one, "dectmp");
                }
                
                // Store new value
                builder.CreateStore(newVal, var);
                
                // Return new value (prefix behavior - could add postfix support later)
                return newVal;
            }
            
            llvm::Value* operand = codegenExpression(unary->operand.get());
            if (!operand) return nullptr;
            
            switch (unary->op.type) {
                case frontend::TokenType::TOKEN_MINUS: {
                    // Check if operand is a numeric type (frac*, tfp*, vec9)
                    Type* operandType = nullptr;
                    auto typeIt = value_types.find(operand);
                    if (typeIt != value_types.end()) {
                        operandType = typeIt->second;
                    }
                    
                    bool isNumericType = false;
                    PrimitiveType* primType = nullptr;
                    if (operandType && operandType->isPrimitive()) {
                        primType = static_cast<PrimitiveType*>(operandType);
                        const std::string& name = primType->getName();
                        if (name.find("frac") == 0 || name.find("tfp") == 0 ||
                            name == "vec9" || name == "dvec9") {
                            isNumericType = true;
                        }
                    }
                    
                    if (isNumericType && primType) {
                        // Numeric type negation - call aria_*_neg runtime function
                        std::string typeName = primType->getName();
                        std::string funcName = "aria_" + typeName + "_neg";
                        
                        std::cerr << "[DEBUG] Generated numeric negation call: " << funcName << std::endl;
                        
                        // Get LLVM types
                        llvm::Type* numericLLVMType = operand->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        
                        // Build function type: void neg(result*, a*)
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {ptrType, ptrType},
                            false
                        );
                        
                        llvm::FunctionCallee negFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        // Allocate space for operand and result
                        llvm::Value* operandAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "neg_operand");
                        llvm::Value* resultAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "neg_result");
                        
                        // Store operand
                        builder.CreateStore(operand, operandAlloca);
                        
                        // Call negation function
                        builder.CreateCall(negFunc, {resultAlloca, operandAlloca});
                        
                        // Load and return result
                        llvm::Value* result = builder.CreateLoad(numericLLVMType, resultAlloca, "neg_value");
                        
                        // Store result type in value_types map
                        value_types[result] = operandType;
                        
                        return result;
                    }
                    
                    // Standard negation for non-numeric types
                    if (operand->getType()->isFloatingPointTy()) {
                        return builder.CreateFNeg(operand, "fnegtmp");
                    }
                    return builder.CreateNeg(operand, "negtmp");
                }
                
                case frontend::TokenType::TOKEN_BANG: {
                    // Logical NOT
                    // If operand is a Result/struct type (e.g. { i1, ptr, i8 } for bool),
                    // extract the value field (index 0) before the boolean comparison.
                    if (operand->getType()->isStructTy()) {
                        operand = builder.CreateExtractValue(operand, {0}, "result_val");
                    }
                    if (operand->getType() != llvm::Type::getInt1Ty(context)) {
                        operand = builder.CreateICmpNE(operand, llvm::ConstantInt::get(operand->getType(), 0), "tobool");
                    }
                    return builder.CreateNot(operand, "nottmp");
                }
                
                case frontend::TokenType::TOKEN_TILDE:
                    // Bitwise NOT
                    return builder.CreateNot(operand, "nottmp");

                case frontend::TokenType::TOKEN_HASH: {
                    // ARIA-016: Pin operator (#) - pins a GC object in memory
                    // This prevents the GC from relocating the object, making it
                    // safe to pass to FFI/DMA operations that need a stable address.
                    //
                    // The operand must be a pointer to a GC-managed object.
                    // Returns the same pointer (now guaranteed to be stable).

                    if (!operand->getType()->isPointerTy()) {
                        // Error: cannot pin non-pointer type
                        return nullptr;
                    }

                    // Get or declare the aria_gc_pin runtime function
                    llvm::Function* pinFunc = module->getFunction("aria_gc_pin");
                    if (!pinFunc) {
                        // Declare: void aria_gc_pin(void* ptr)
                        llvm::FunctionType* pinFuncType = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {builder.getPtrTy()},
                            false
                        );
                        pinFunc = llvm::Function::Create(
                            pinFuncType,
                            llvm::Function::ExternalLinkage,
                            "aria_gc_pin",
                            module.get()
                        );
                    }

                    // Cast operand to void* if needed and call pin function
                    llvm::Value* ptrToPin = builder.CreateBitCast(operand, builder.getPtrTy());
                    builder.CreateCall(pinFunc, {ptrToPin});

                    // Return the original pointer (now pinned)
                    // The type transitions from managed to "wild" at the AST level
                    return operand;
                }

                default:
                    return nullptr;
            }
        }
        
        case ASTNode::NodeType::RANGE: {
            // Range expression: start..end or start...end
            // For now, we'll create a simple struct {start, end, isExclusive}
            // This can be used by for loops and slicing operations
            RangeExpr* range = static_cast<RangeExpr*>(expr);
            
            llvm::Value* start = codegenExpression(range->start.get());
            llvm::Value* end = codegenExpression(range->end.get());
            
            if (!start || !end) {
                return nullptr;
            }
            
            // Create a {i64, i64, i1} struct for the range
            llvm::Type* rangeType = llvm::StructType::get(
                context,
                {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt1Ty()}
            );
            
            // Create alloca for the range struct
            llvm::Value* rangeAlloca = builder.CreateAlloca(rangeType, nullptr, "range");
            
            // Store start value
            llvm::Value* startPtr = builder.CreateStructGEP(rangeType, rangeAlloca, 0, "range.start.ptr");
            llvm::Value* startExt = builder.CreateSExtOrTrunc(start, builder.getInt64Ty(), "start.ext");
            builder.CreateStore(startExt, startPtr);
            
            // Store end value
            llvm::Value* endPtr = builder.CreateStructGEP(rangeType, rangeAlloca, 1, "range.end.ptr");
            llvm::Value* endExt = builder.CreateSExtOrTrunc(end, builder.getInt64Ty(), "end.ext");
            builder.CreateStore(endExt, endPtr);
            
            // Store isExclusive flag
            llvm::Value* exclusivePtr = builder.CreateStructGEP(rangeType, rangeAlloca, 2, "range.exclusive.ptr");
            builder.CreateStore(builder.getInt1(range->isExclusive), exclusivePtr);
            
            // Return the pointer to the range struct
            return rangeAlloca;
        }
        
        case ASTNode::NodeType::BINARY_OP: {
            // Binary operation
            BinaryExpr* binop = static_cast<BinaryExpr*>(expr);
            
            // Special handling for assignment operators (=, +=, -=, *=, /=, %=)
            if (binop->op.type == frontend::TokenType::TOKEN_EQUAL ||
                binop->op.type == frontend::TokenType::TOKEN_PLUS_EQUAL ||
                binop->op.type == frontend::TokenType::TOKEN_MINUS_EQUAL ||
                binop->op.type == frontend::TokenType::TOKEN_STAR_EQUAL ||
                binop->op.type == frontend::TokenType::TOKEN_SLASH_EQUAL ||
                binop->op.type == frontend::TokenType::TOKEN_PERCENT_EQUAL) {
                
                // Assignment: left must be an lvalue (identifier or index expression)
                if (binop->left->type == ASTNode::NodeType::INDEX) {
                    // Array/Vector/SIMD element assignment: arr[i] = value, vec[i] = value, simd[i] = value
                    IndexExpr* indexExpr = static_cast<IndexExpr*>(binop->left.get());
                    
                    // Multi-dimensional array assignment: matrix[i][j] = val
                    // Walk the nested INDEX chain to the root identifier, collecting all indices.
                    if (indexExpr->array->type == ASTNode::NodeType::INDEX) {
                        std::vector<ASTNode*> all_indices;
                        all_indices.push_back(indexExpr->index.get()); // outermost index
                        ASTNode* nd_chain = indexExpr->array.get();
                        while (nd_chain->type == ASTNode::NodeType::INDEX) {
                            IndexExpr* inner_ix = static_cast<IndexExpr*>(nd_chain);
                            all_indices.push_back(inner_ix->index.get());
                            nd_chain = inner_ix->array.get();
                        }
                        if (nd_chain->type == ASTNode::NodeType::IDENTIFIER) {
                            IdentifierExpr* root_ident = static_cast<IdentifierExpr*>(nd_chain);
                            auto nd_it = named_values.find(root_ident->name);
                            if (nd_it != named_values.end()) {
                                auto* nd_alloca = llvm::dyn_cast<llvm::AllocaInst>(nd_it->second);
                                if (nd_alloca) {
                                    llvm::Type* nd_alloc_ty = nd_alloca->getAllocatedType();
                                    if (nd_alloc_ty->isArrayTy()) {
                                        // all_indices=[outerIdx,...] collected outermost first.
                                        // GEP needs [0, outermost, ..., innermost] — iterate in reverse.
                                        std::vector<llvm::Value*> gep_idx = {
                                            llvm::ConstantInt::get(builder.getInt64Ty(), 0)
                                        };
                                        for (int k = (int)all_indices.size() - 1; k >= 0; --k) {
                                            llvm::Value* idx_v = codegenExpression(all_indices[k]);
                                            if (!idx_v) return nullptr;
                                            if (!idx_v->getType()->isIntegerTy(64))
                                                idx_v = builder.CreateSExtOrTrunc(
                                                    idx_v, builder.getInt64Ty(), "idx.i64");
                                            gep_idx.push_back(idx_v);
                                        }
                                        // Descend type hierarchy to find element type
                                        llvm::Type* nd_elem_ty = nd_alloc_ty;
                                        for (size_t k = 0; k < all_indices.size(); ++k) {
                                            if (!nd_elem_ty->isArrayTy()) { nd_elem_ty = nullptr; break; }
                                            nd_elem_ty = llvm::cast<llvm::ArrayType>(nd_elem_ty)->getElementType();
                                        }
                                        if (nd_elem_ty) {
                                            llvm::Value* nd_rhs = codegenExpression(binop->right.get());
                                            if (!nd_rhs) return nullptr;
                                            // Unwrap Result<T> → T for array element stores
                                            if (nd_rhs->getType()->isStructTy() && nd_rhs->getType() != nd_elem_ty) {
                                                llvm::StructType* rs = llvm::cast<llvm::StructType>(nd_rhs->getType());
                                                if (rs->getNumElements() == 3 &&
                                                    rs->getElementType(1)->isPointerTy() &&
                                                    rs->getElementType(2)->isIntegerTy(8) &&
                                                    rs->getElementType(0) == nd_elem_ty) {
                                                    nd_rhs = builder.CreateExtractValue(nd_rhs, {0}, "unwrap_result_arr");
                                                }
                                            }
                                            if (nd_rhs->getType() != nd_elem_ty) {
                                                if (nd_rhs->getType()->isIntegerTy() && nd_elem_ty->isIntegerTy())
                                                    nd_rhs = builder.CreateIntCast(nd_rhs, nd_elem_ty, true, "rhs.cast");
                                            }
                                            llvm::Value* nd_ptr = builder.CreateInBoundsGEP(
                                                nd_alloc_ty, nd_alloca, gep_idx, "arrnd.assign.ptr");
                                            builder.CreateStore(nd_rhs, nd_ptr);
                                            return nd_rhs;
                                        }
                                    }
                                }
                            }
                        }
                        return nullptr;
                    }

                    // Struct field array write: obj.array_field[i] = value
                    if (indexExpr->array->type == ASTNode::NodeType::MEMBER_ACCESS) {
                        MemberAccessExpr* memberExpr =
                            static_cast<MemberAccessExpr*>(indexExpr->array.get());
                        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
                            IdentifierExpr* objIdent =
                                static_cast<IdentifierExpr*>(memberExpr->object.get());
                            auto obj_it = named_values.find(objIdent->name);
                            if (obj_it != named_values.end()) {
                                llvm::Value* structPtr = obj_it->second;
                                llvm::Type* sAllocTy = nullptr;
                                if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(structPtr))
                                    sAllocTy = ai->getAllocatedType();
                                else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(structPtr))
                                    sAllocTy = gv->getValueType();
                                if (sAllocTy && sAllocTy->isStructTy()) {
                                    int fldIdx = -1;
                                    auto type_it = value_types.find(obj_it->second);
                                    if (type_it != value_types.end() &&
                                        type_it->second->getKind() == TypeKind::STRUCT) {
                                        StructType* sType =
                                            static_cast<StructType*>(type_it->second);
                                        const auto& flds = sType->getFields();
                                        for (size_t fi = 0; fi < flds.size(); ++fi) {
                                            if (flds[fi].name == memberExpr->member) {
                                                fldIdx = static_cast<int>(fi);
                                                break;
                                            }
                                        }
                                    }
                                    if (fldIdx < 0) {
                                        // Fallback: look up struct type by Aria type name
                                        auto var_it = var_aria_types.find(objIdent->name);
                                        if (var_it != var_aria_types.end()) {
                                            sema::StructType* semaST =
                                                type_system->getStructType(var_it->second);
                                            if (semaST)
                                                fldIdx = semaST->getFieldIndex(memberExpr->member);
                                        }
                                    }
                                    if (fldIdx >= 0) {
                                        llvm::StructType* llvmSTy =
                                            llvm::cast<llvm::StructType>(sAllocTy);
                                        llvm::Type* fieldTy =
                                            llvmSTy->getElementType(fldIdx);
                                        if (fieldTy->isArrayTy()) {
                                            llvm::Type* elemTy =
                                                llvm::cast<llvm::ArrayType>(fieldTy)
                                                    ->getElementType();
                                            llvm::Value* fieldPtr =
                                                builder.CreateStructGEP(
                                                    sAllocTy, structPtr, fldIdx,
                                                    memberExpr->member + ".field.ptr");
                                            llvm::Value* idx_v = codegenExpression(
                                                indexExpr->index.get());
                                            if (!idx_v) return nullptr;
                                            if (!idx_v->getType()->isIntegerTy(64))
                                                idx_v = builder.CreateSExtOrTrunc(
                                                    idx_v, builder.getInt64Ty(),
                                                    "idx.i64");
                                            std::vector<llvm::Value*> gepIdx = {
                                                llvm::ConstantInt::get(
                                                    builder.getInt64Ty(), 0),
                                                idx_v};
                                            llvm::Value* elemPtr =
                                                builder.CreateInBoundsGEP(
                                                    fieldTy, fieldPtr, gepIdx,
                                                    memberExpr->member + ".welem.ptr");
                                            llvm::Value* rhs =
                                                codegenExpression(binop->right.get());
                                            if (!rhs) return nullptr;
                                            if (rhs->getType() != elemTy) {
                                                if (rhs->getType()->isIntegerTy() &&
                                                    elemTy->isIntegerTy())
                                                    rhs = builder.CreateIntCast(
                                                        rhs, elemTy, true, "rhs.cast");
                                            }
                                            builder.CreateStore(rhs, elemPtr);
                                            return rhs;
                                        }
                                    }
                                }
                            }
                        }
                        // MEMBER_ACCESS base but couldn't handle (non-array field, etc.)
                        return nullptr;
                    }
                    // Only handle identifier base for non-MEMBER_ACCESS cases
                    if (indexExpr->array->type != ASTNode::NodeType::IDENTIFIER) {
                        return nullptr;
                    }
                    
                    IdentifierExpr* baseIdent = static_cast<IdentifierExpr*>(indexExpr->array.get());
                    auto it = named_values.find(baseIdent->name);
                    if (it == named_values.end()) {
                        return nullptr;  // Variable not found
                    }
                    
                    llvm::Value* var = it->second;
                    Type* object_type = nullptr;
                    
                    // Get the type information
                    auto type_it = value_types.find(var);
                    if (type_it != value_types.end()) {
                        object_type = type_it->second;
                    }
                    
                    // Generate index value
                    llvm::Value* index_value = codegenExpression(indexExpr->index.get());
                    if (!index_value) return nullptr;
                    
                    // Generate right-hand side value
                    llvm::Value* rhs = codegenExpression(binop->right.get());
                    if (!rhs) return nullptr;
                    
                    // Check if it's a SIMD type
                    if (object_type && object_type->getKind() == TypeKind::SIMD) {
                        // SIMD element assignment: simd[i] = value
                        SimdType* simd_type = static_cast<SimdType*>(object_type);
                        
                        // Load current SIMD vector
                        llvm::Type* simd_llvm_type = mapType(simd_type);
                        llvm::Value* simd_val = builder.CreateLoad(simd_llvm_type, var, "simd");
                        
                        // Insert element at index: insertelement <N x T> vec, T val, i32 idx
                        llvm::Value* new_simd = builder.CreateInsertElement(simd_val, rhs, index_value, "simd.insert");
                        
                        // Store updated SIMD vector back
                        builder.CreateStore(new_simd, var);
                        
                        // Assignment returns the inserted value
                        return rhs;
                    }
                    
                    // Check if it's a vector type
                    if (object_type && object_type->getKind() == TypeKind::VECTOR) {
                        // Vector element assignment: vec[i] = value
                        VectorType* vec_type = static_cast<VectorType*>(object_type);
                        int dimension = vec_type->getDimension();
                        
                        // Load current vector
                        llvm::Type* vec_llvm_type = mapType(vec_type);
                        llvm::Value* vec_val = builder.CreateLoad(vec_llvm_type, var, "vec");
                        
                        if (dimension == 9) {
                            // vec9 is a struct - use insertvalue with dynamic selection
                            llvm::Value* result = vec_val;
                            for (int i = 0; i < 9; ++i) {
                                llvm::Value* cmp = builder.CreateICmpEQ(index_value, builder.getInt32(i), "idx.cmp");
                                llvm::Value* updated = builder.CreateInsertValue(result, rhs, {static_cast<unsigned>(i)}, "vec9.insert");
                                result = builder.CreateSelect(cmp, updated, result, "vec9.select");
                            }
                            builder.CreateStore(result, var);
                            return rhs;
                        } else {
                            // vec2, vec3 are LLVM vectors - use InsertElement
                            llvm::Value* new_vec = builder.CreateInsertElement(vec_val, rhs, index_value, "vec.insert");
                            builder.CreateStore(new_vec, var);
                            return rhs;
                        }
                    }
                    
                    // Array element assignment (pointer-based)
                    llvm::Value* array_ptr = nullptr;
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                        llvm::Type* allocated_type = allocaInst->getAllocatedType();
                        if (allocated_type->isArrayTy()) {
                            // Properly-typed fixed-size array [N x T] — use two-index GEP
                            auto* arrTy = llvm::cast<llvm::ArrayType>(allocated_type);
                            llvm::Type* elemType = arrTy->getElementType();
                            // Cast index to i64 for GEP
                            if (!index_value->getType()->isIntegerTy(64)) {
                                index_value = builder.CreateSExtOrTrunc(index_value,
                                                  builder.getInt64Ty(), "idx.i64");
                            }
                            std::vector<llvm::Value*> gep_indices = {
                                llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                                index_value
                            };
                            llvm::Value* elem_ptr = builder.CreateInBoundsGEP(
                                allocated_type, var, gep_indices, "arrayidx");
                            // Unwrap Result<T> → T for array element stores
                            if (rhs->getType()->isStructTy() && rhs->getType() != elemType) {
                                llvm::StructType* rs = llvm::cast<llvm::StructType>(rhs->getType());
                                if (rs->getNumElements() == 3 &&
                                    rs->getElementType(1)->isPointerTy() &&
                                    rs->getElementType(2)->isIntegerTy(8) &&
                                    rs->getElementType(0) == elemType) {
                                    rhs = builder.CreateExtractValue(rhs, {0}, "unwrap_result_arr");
                                }
                            }
                            // Cast rhs to element type if needed
                            if (rhs->getType() != elemType) {
                                if (rhs->getType()->isIntegerTy() && elemType->isIntegerTy()) {
                                    rhs = builder.CreateIntCast(rhs, elemType, true, "rhs.cast");
                                }
                            }
                            builder.CreateStore(rhs, elem_ptr);
                            return rhs;
                        } else if (allocated_type->isPointerTy()) {
                            array_ptr = builder.CreateLoad(allocated_type, var, baseIdent->name + ".ptr");
                        } else {
                            array_ptr = var;
                        }
                    } else {
                        array_ptr = var;
                    }
                    
                    // Determine element type from the Aria type tracked in var_aria_types.
                    // For a wild T-> pointer, var_aria_types stores "T@"; strip "@" to get T.
                    llvm::Type* elem_type = builder.getInt64Ty();  // Default fallback
                    {
                        auto at_it = var_aria_types.find(baseIdent->name);
                        if (at_it != var_aria_types.end()) {
                            std::string ariaElem = at_it->second;
                            if (!ariaElem.empty() && ariaElem.back() == '@')
                                ariaElem.pop_back();
                            if      (ariaElem == "int8"  || ariaElem == "i8")  elem_type = builder.getInt8Ty();
                            else if (ariaElem == "int16" || ariaElem == "i16") elem_type = builder.getInt16Ty();
                            else if (ariaElem == "int32" || ariaElem == "i32") elem_type = builder.getInt32Ty();
                            else if (ariaElem == "int64" || ariaElem == "i64") elem_type = builder.getInt64Ty();
                            else if (ariaElem == "flt32" || ariaElem == "f32" || ariaElem == "float32") elem_type = builder.getFloatTy();
                            else if (ariaElem == "flt64" || ariaElem == "f64" || ariaElem == "float64") elem_type = builder.getDoubleTy();
                        }
                    }
                    // Unwrap Result<T> → T when assigning to an array element.
                    // pub func calls return { T, ptr, i8 }; storing the full
                    // struct to a T-sized slot overwrites adjacent memory.
                    if (rhs->getType()->isStructTy() && rhs->getType() != elem_type) {
                        llvm::StructType* rs = llvm::cast<llvm::StructType>(rhs->getType());
                        if (rs->getNumElements() == 3 &&
                            rs->getElementType(1)->isPointerTy() &&
                            rs->getElementType(2)->isIntegerTy(8) &&
                            rs->getElementType(0) == elem_type) {
                            rhs = builder.CreateExtractValue(rhs, {0}, "unwrap_result_arr");
                        }
                    }
                    // Cast rhs to elem_type if needed
                    if (rhs->getType() != elem_type) {
                        if (rhs->getType()->isIntegerTy() && elem_type->isIntegerTy())
                            rhs = builder.CreateIntCast(rhs, elem_type, true, "rhs.cast");
                        else if (rhs->getType()->isFloatingPointTy() && elem_type->isFloatingPointTy())
                            rhs = builder.CreateFPCast(rhs, elem_type, "rhs.fpcast");
                    }
                    // Create GEP and store
                    llvm::Value* elem_ptr = builder.CreateGEP(elem_type, array_ptr, index_value, "arrayidx");
                    builder.CreateStore(rhs, elem_ptr);
                    
                    return rhs;
                }
                
                // Handle struct field assignment: obj.field = value
                if (binop->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
                    MemberAccessExpr* member = static_cast<MemberAccessExpr*>(binop->left.get());
                    
                    // Handle nested member access: obj.arr[i].field = value
                    if (member->object->type != ASTNode::NodeType::IDENTIFIER) {
                        if (member->object->type == ASTNode::NodeType::INDEX) {
                            IndexExpr* ixExpr = static_cast<IndexExpr*>(member->object.get());
                            if (ixExpr->array->type == ASTNode::NodeType::MEMBER_ACCESS) {
                                MemberAccessExpr* arrMem =
                                    static_cast<MemberAccessExpr*>(ixExpr->array.get());
                                if (arrMem->object->type == ASTNode::NodeType::IDENTIFIER) {
                                    IdentifierExpr* objId =
                                        static_cast<IdentifierExpr*>(arrMem->object.get());
                                    auto oIt = named_values.find(objId->name);
                                    if (oIt != named_values.end()) {
                                        llvm::Value* sPtr = oIt->second;
                                        llvm::Type* sATy = nullptr;
                                        if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(sPtr))
                                            sATy = ai->getAllocatedType();
                                        if (sATy && sATy->isStructTy()) {
                                            int aFIdx = -1;
                                            Type* aFType = nullptr;
                                            auto tIt = value_types.find(oIt->second);
                                            if (tIt != value_types.end() &&
                                                tIt->second->getKind() == TypeKind::STRUCT) {
                                                StructType* sT =
                                                    static_cast<StructType*>(tIt->second);
                                                const auto& fs = sT->getFields();
                                                for (size_t fi = 0; fi < fs.size(); ++fi) {
                                                    if (fs[fi].name == arrMem->member) {
                                                        aFIdx = static_cast<int>(fi);
                                                        aFType = fs[fi].type;
                                                        break;
                                                    }
                                                }
                                            }
                                            if (aFIdx < 0) {
                                                auto vIt = var_aria_types.find(objId->name);
                                                if (vIt != var_aria_types.end()) {
                                                    sema::StructType* semST =
                                                        type_system->getStructType(vIt->second);
                                                    if (semST) {
                                                        aFIdx = semST->getFieldIndex(arrMem->member);
                                                        const sema::StructType::Field* sf =
                                                            semST->getField(arrMem->member);
                                                        if (sf) aFType = sf->type;
                                                    }
                                                }
                                            }
                                            if (aFIdx >= 0) {
                                                auto* llST = llvm::cast<llvm::StructType>(sATy);
                                                llvm::Type* fTy = llST->getElementType(aFIdx);
                                                if (fTy->isArrayTy()) {
                                                    auto* aT = llvm::cast<llvm::ArrayType>(fTy);
                                                    llvm::Type* eTy = aT->getElementType();
                                                    if (eTy->isStructTy()) {
                                                        // Find nested field index
                                                        int nFIdx = -1;
                                                        Type* eAT = nullptr;
                                                        if (aFType && aFType->getKind() == TypeKind::ARRAY)
                                                            eAT = static_cast<sema::ArrayType*>(aFType)
                                                                      ->getElementType();
                                                        if (eAT && eAT->getKind() == TypeKind::STRUCT) {
                                                            StructType* eST =
                                                                static_cast<StructType*>(eAT);
                                                            const auto& eFs = eST->getFields();
                                                            for (size_t fi = 0; fi < eFs.size(); ++fi) {
                                                                if (eFs[fi].name == member->member) {
                                                                    nFIdx = static_cast<int>(fi);
                                                                    break;
                                                                }
                                                            }
                                                        }
                                                        if (nFIdx >= 0) {
                                                            // GEP chain: struct → array field → element → nested field
                                                            llvm::Value* fP = builder.CreateStructGEP(
                                                                sATy, sPtr, aFIdx,
                                                                arrMem->member + ".field.ptr");
                                                            llvm::Value* iV =
                                                                codegenExpression(ixExpr->index.get());
                                                            if (!iV) return nullptr;
                                                            if (!iV->getType()->isIntegerTy(64))
                                                                iV = builder.CreateSExtOrTrunc(
                                                                    iV, builder.getInt64Ty(), "idx.i64");
                                                            std::vector<llvm::Value*> gI = {
                                                                llvm::ConstantInt::get(
                                                                    builder.getInt64Ty(), 0),
                                                                iV};
                                                            llvm::Value* eP =
                                                                builder.CreateInBoundsGEP(
                                                                    fTy, fP, gI,
                                                                    arrMem->member + ".elem.ptr");
                                                            llvm::Value* nFP =
                                                                builder.CreateStructGEP(
                                                                    eTy, eP, nFIdx,
                                                                    member->member + ".ptr");
                                                            llvm::Value* rhs =
                                                                codegenExpression(binop->right.get());
                                                            if (!rhs) return nullptr;
                                                            builder.CreateStore(rhs, nFP);
                                                            return rhs;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        return nullptr;
                    }
                    
                    IdentifierExpr* obj_ident = static_cast<IdentifierExpr*>(member->object.get());
                    auto obj_it = named_values.find(obj_ident->name);
                    if (obj_it == named_values.end()) {
                        return nullptr;  // Object not found
                    }
                    
                    llvm::Value* object_ptr = obj_it->second;
                    
                    // Get the Aria type of the object
                    auto type_it = value_types.find(object_ptr);
                    if (type_it == value_types.end()) {
                        return nullptr;  // No type information
                    }
                    
                    Type* aria_type = type_it->second;
                    
                    // Only handle struct types for now
                    if (aria_type->getKind() != TypeKind::STRUCT) {
                        return nullptr;  // TODO: Support vector component assignment (.x = ...)
                    }
                    
                    StructType* struct_type = static_cast<StructType*>(aria_type);
                    
                    // Find the field index
                    int field_index = -1;
                    Type* field_type = nullptr;
                    const auto& fields = struct_type->getFields();
                    for (size_t i = 0; i < fields.size(); ++i) {
                        if (fields[i].name == member->member) {
                            field_index = static_cast<int>(i);
                            field_type = fields[i].type;
                            break;
                        }
                    }
                    
                    if (field_index < 0) {
                        return nullptr;  // Field not found
                    }
                    (void)field_type;
                    
                    // Get the LLVM struct type
                    llvm::Type* llvm_struct_type = mapType(struct_type);
                    
                    // Evaluate the right-hand side
                    llvm::Value* rhs = codegenExpression(binop->right.get());
                    if (!rhs) return nullptr;
                    
                    // Get pointer to the specific field
                    llvm::Value* field_ptr = builder.CreateStructGEP(
                        llvm_struct_type,
                        object_ptr,
                        field_index,
                        member->member + ".ptr"
                    );
                    
                    // Type coercion for struct field assignment
                    llvm::Type* expected_field_type = llvm::cast<llvm::StructType>(llvm_struct_type)->getElementType(field_index);
                    rhs->getType()->print(llvm::errs());
                    std::cerr << std::endl;
                    expected_field_type->print(llvm::errs());
                    std::cerr << std::endl;
                    if (rhs->getType() != expected_field_type) {
                        // Unwrap optional {i1, T} when field expects T
                        if (rhs->getType()->isStructTy()) {
                            auto* rhs_st = llvm::cast<llvm::StructType>(rhs->getType());
                            rhs_st->getElementType(1)->print(llvm::errs());
                            std::cerr << std::endl;
                            if (rhs_st->getNumElements() == 2 &&
                                rhs_st->getElementType(0)->isIntegerTy(1) &&
                                rhs_st->getElementType(1) == expected_field_type) {
                                rhs = builder.CreateExtractValue(rhs, 1, "unwrap.optional");
                            }
                        }
                        // Integer/float casts
                        if (rhs->getType() != expected_field_type) {
                            if (rhs->getType()->isIntegerTy() && expected_field_type->isIntegerTy())
                                rhs = builder.CreateIntCast(rhs, expected_field_type, true, "field.icast");
                            else if (rhs->getType()->isFloatingPointTy() && expected_field_type->isFloatingPointTy())
                                rhs = builder.CreateFPCast(rhs, expected_field_type, "field.fpcast");
                        }
                    }
                    
                    // Store the new value
                    rhs->getType()->print(llvm::errs());
                    std::cerr << std::endl;
                    builder.CreateStore(rhs, field_ptr);
                    
                    // Assignment expression returns the assigned value
                    return rhs;
                }
                
                // Handle pointer dereference assignment: <-ptr = value or *ptr = value
        if (binop->left->type == ASTNode::NodeType::UNARY_OP) {
                    UnaryExpr* unary = static_cast<UnaryExpr*>(binop->left.get());
                    
                    // Only handle dereference operators (<- and *)
                    if (unary->op.type == frontend::TokenType::TOKEN_LEFT_ARROW ||
                        unary->op.type == frontend::TokenType::TOKEN_STAR) {
                        
                        // Evaluate the pointer operand (the thing being dereferenced)
                        llvm::Value* ptr = codegenExpression(unary->operand.get());
                        if (!ptr) return nullptr;
                        
                        // Evaluate the right-hand side value
                        llvm::Value* rhs = codegenExpression(binop->right.get());
                        if (!rhs) return nullptr;
                        
                        // Store through the pointer
                        builder.CreateStore(rhs, ptr);
                        
                        // Assignment expression returns the assigned value
                        return rhs;
                    }
                }
                
                if (binop->left->type != ASTNode::NodeType::IDENTIFIER) {
                    return nullptr;  // Only simple identifiers supported
                }
                
                IdentifierExpr* lhs = static_cast<IdentifierExpr*>(binop->left.get());
                
                // Explicit discard: _ = expr — evaluate RHS for side effects only
                if (lhs->name == "_") {
                    llvm::Value* rhs = codegenExpression(binop->right.get());
                    return rhs ? rhs : llvm::UndefValue::get(builder.getVoidTy());
                }

                // Look up the variable
                auto it = named_values.find(lhs->name);
                if (it == named_values.end()) {
                    return nullptr;  // Variable not found
                }
                
                llvm::Value* var = it->second;
                llvm::Value* result = nullptr;

                // If the target is a raw (non-pointer) function parameter value,
                // promote it to a stack alloca so it can be written to.
                // Primitive-type parameters (int32, flt64, etc.) are stored directly
                // as LLVM Argument values; reassigning them requires addressable storage.
                if (!var->getType()->isPointerTy()) {
                    llvm::Function* fn = builder.GetInsertBlock()->getParent();
                    auto savedIP = builder.saveIP();
                    llvm::BasicBlock& entryBB = fn->getEntryBlock();
                    builder.SetInsertPoint(&entryBB, entryBB.begin());
                    llvm::AllocaInst* promoted = builder.CreateAlloca(
                        var->getType(), nullptr, lhs->name + ".addr");
                    builder.CreateStore(var, promoted);
                    builder.restoreIP(savedIP);
                    named_values[lhs->name] = promoted;
                    var = promoted;
                }

                // For compound assignments, compute: var = var OP rhs
                if (binop->op.type != frontend::TokenType::TOKEN_EQUAL) {
                    // Load current value - need to determine the type
                    llvm::Type* varElemType = nullptr;
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                        varElemType = allocaInst->getAllocatedType();
                    } else {
                        // For now, default to int32 if not an alloca
                        varElemType = builder.getInt32Ty();
                    }
                    
                    llvm::Value* currentVal = builder.CreateLoad(varElemType, var, lhs->name);
                    
                    // Evaluate right side
                    llvm::Value* rhs = codegenExpression(binop->right.get());
                    if (!rhs) return nullptr;
                    
                    // Perform operation
                    // BUG-002: Float compound assignments must use FAdd/FSub/FMul/FDiv.
                    // Safe-overflow intrinsics only apply to integers.
                    bool isFP = currentVal->getType()->isFloatingPointTy();
                    // ARIA-024: Check for LBIM types for compound assignment
                    unsigned numLimbs = isLBIMType(currentVal->getType());
                    switch (binop->op.type) {
                        case frontend::TokenType::TOKEN_PLUS_EQUAL:
                            if (numLimbs) {
                                result = generateLBIMAdd(currentVal, rhs, numLimbs);
                            } else if (isFP) {
                                result = builder.CreateFAdd(currentVal, rhs, "faddtmp");
                            } else {
                                // Layer 1 Safety: Safe addition returns Unknown on overflow
                                result = generateSafeAdd(currentVal, rhs, "addtmp");
                            }
                            break;
                        case frontend::TokenType::TOKEN_MINUS_EQUAL:
                            if (numLimbs) {
                                result = generateLBIMSub(currentVal, rhs, numLimbs);
                            } else if (isFP) {
                                result = builder.CreateFSub(currentVal, rhs, "fsubtmp");
                            } else {
                                // Layer 1 Safety: Safe subtraction returns Unknown on overflow
                                result = generateSafeSub(currentVal, rhs, "subtmp");
                            }
                            break;
                        case frontend::TokenType::TOKEN_STAR_EQUAL:
                            if (numLimbs) {
                                result = generateLBIMMul(currentVal, rhs, numLimbs);
                            } else if (isFP) {
                                result = builder.CreateFMul(currentVal, rhs, "fmultmp");
                            } else {
                                // Layer 1 Safety: Safe multiplication returns Unknown on overflow
                                result = generateSafeMul(currentVal, rhs, "multmp");
                            }
                            break;
                        case frontend::TokenType::TOKEN_SLASH_EQUAL:
                            if (numLimbs) {
                                result = generateLBIMDiv(currentVal, rhs, numLimbs);
                            } else if (isFP) {
                                result = builder.CreateFDiv(currentVal, rhs, "fdivtmp");
                            } else {
                                // Layer 1 Safety: Safe division returns Unknown on divide-by-zero
                                result = generateSafeSDiv(currentVal, rhs, "divtmp");
                            }
                            break;
                        case frontend::TokenType::TOKEN_PERCENT_EQUAL:
                            if (numLimbs) {
                                result = generateLBIMMod(currentVal, rhs, numLimbs);
                            } else if (isFP) {
                                result = builder.CreateFRem(currentVal, rhs, "fremtmp");
                            } else {
                                // Layer 1 Safety: Safe modulo returns Unknown on divide-by-zero
                                result = generateSafeSRem(currentVal, rhs, "modtmp");
                            }
                            break;
                        default:
                            return nullptr;
                    }
                } else {
                    // Simple assignment
                    result = codegenExpression(binop->right.get());
                    if (!result) return nullptr;
                }
                
                // BUG-026 FIX: Ensure the stored value type matches the alloca type.
                // Integer literals default to i64, but the target variable may be
                // i32/i8/etc.  Without this conversion the store is type-mismatched
                // (e.g. "store i64 5, ptr %x" into an i32 alloca), which LLVM treats
                // as undefined behaviour and may optimise away entirely.
                if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                    llvm::Type* targetType = allocaInst->getAllocatedType();
                    if (result->getType() != targetType) {
                        if (result->getType()->isIntegerTy() && targetType->isIntegerTy()) {
                            unsigned srcBits = result->getType()->getIntegerBitWidth();
                            unsigned dstBits = targetType->getIntegerBitWidth();
                            if (srcBits > dstBits) {
                                result = builder.CreateTrunc(result, targetType, "assign_trunc");
                            } else if (srcBits < dstBits) {
                                result = builder.CreateSExt(result, targetType, "assign_sext");
                            }
                        } else if (result->getType()->isFloatingPointTy() && targetType->isFloatingPointTy()) {
                            unsigned srcBits = result->getType()->getScalarSizeInBits();
                            unsigned dstBits = targetType->getScalarSizeInBits();
                            if (srcBits > dstBits) {
                                result = builder.CreateFPTrunc(result, targetType, "assign_fptrunc");
                            } else if (srcBits < dstBits) {
                                result = builder.CreateFPExt(result, targetType, "assign_fpext");
                            }
                        } else if (result->getType()->isIntegerTy() && targetType->isFloatingPointTy()) {
                            result = builder.CreateSIToFP(result, targetType, "assign_itof");
                        } else if (result->getType()->isFloatingPointTy() && targetType->isIntegerTy()) {
                            result = builder.CreateFPToSI(result, targetType, "assign_ftoi");
                        } else if (result->getType()->isIntegerTy() && targetType->isStructTy()) {
                            // LBIM promotion: integer literal into LBIM struct slot
                            llvm::StructType* st = llvm::cast<llvm::StructType>(targetType);
                            std::string sn = st->getName().str();
                            if (sn.find("struct.int") == 0 || sn.find("struct.uint") == 0) {
                                result = promoteToLBIMStruct(result, targetType);
                            }
                        } else if (result->getType()->isStructTy() && !targetType->isStructTy()) {
                            // Unwrap Optional { i1, T } or Result { T, ptr, i8 } when
                            // re-assigning to a variable whose alloca is the inner type.
                            llvm::StructType* resST = llvm::cast<llvm::StructType>(result->getType());
                            unsigned numFields = resST->getNumElements();
                            if (numFields == 2 &&
                                resST->getElementType(0)->isIntegerTy(1)) {
                                // Optional { i1, T } → extract field 1
                                result = builder.CreateExtractValue(result, {1}, "unwrap_opt_assign");
                            } else if (numFields == 3 &&
                                       resST->getElementType(1)->isPointerTy() &&
                                       resST->getElementType(2)->isIntegerTy(8)) {
                                // Result { T, ptr, i8 } → extract field 0
                                result = builder.CreateExtractValue(result, {0}, "unwrap_res_assign");
                            }
                        }
                    }
                }

                // Store the result
                builder.CreateStore(result, var);
                
                // Assignment expression returns the assigned value
                return result;
            }
            
            // Special handling for short-circuit logical operators
            // These must be handled BEFORE evaluating both operands
            if (binop->op.type == frontend::TokenType::TOKEN_AND_AND) {
                // && short-circuits: if left is false, don't evaluate right
                // Pattern: left && right
                //   if (!left) return false
                //   else return right
                
                llvm::Function* function = builder.GetInsertBlock()->getParent();
                
                // Create basic blocks: eval_right (if left is true), merge
                llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "and.rhs");
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "and.merge");
                
                // Evaluate left operand ONLY
                llvm::Value* L = codegenExpression(binop->left.get());
                if (!L) return nullptr;
                
                // Convert to boolean (i1)
                llvm::Value* LBool = builder.CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0), "and.lhs");
                
                // Remember the block where left was evaluated
                llvm::BasicBlock* leftBB = builder.GetInsertBlock();
                
                // If left is false, short-circuit to merge with false
                // If left is true, continue to evaluate right
                builder.CreateCondBr(LBool, evalRightBB, mergeBB);
                
                // Generate right evaluation block
                function->insert(function->end(), evalRightBB);
                builder.SetInsertPoint(evalRightBB);
                
                // NOW evaluate right operand (only if left was true)
                llvm::Value* R = codegenExpression(binop->right.get());
                if (!R) return nullptr;
                
                // Convert right to boolean
                llvm::Value* RBool = builder.CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0), "and.rhs");
                
                // Jump to merge
                builder.CreateBr(mergeBB);
                // Update evalRightBB in case expression changed the block
                evalRightBB = builder.GetInsertBlock();
                
                // Merge block with PHI node
                function->insert(function->end(), mergeBB);
                builder.SetInsertPoint(mergeBB);
                
                // PHI: false from leftBB (short-circuit), or RBool from evalRightBB
                llvm::PHINode* phi = builder.CreatePHI(builder.getInt1Ty(), 2, "and.result");
                phi->addIncoming(llvm::ConstantInt::get(builder.getInt1Ty(), 0), leftBB);  // false if left was false
                phi->addIncoming(RBool, evalRightBB);  // right's boolean value if left was true
                
                return phi;
            }
            
            if (binop->op.type == frontend::TokenType::TOKEN_OR_OR) {
                // || short-circuits: if left is true, don't evaluate right
                // Pattern: left || right
                //   if (left) return true
                //   else return right
                
                llvm::Function* function = builder.GetInsertBlock()->getParent();
                
                // Create basic blocks: eval_right (if left is false), merge
                llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "or.rhs");
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "or.merge");
                
                // Evaluate left operand ONLY
                llvm::Value* L = codegenExpression(binop->left.get());
                if (!L) return nullptr;
                
                // Convert to boolean (i1)
                llvm::Value* LBool = builder.CreateICmpNE(L, llvm::ConstantInt::get(L->getType(), 0), "or.lhs");
                
                // Remember the block where left was evaluated
                llvm::BasicBlock* leftBB = builder.GetInsertBlock();
                
                // If left is true, short-circuit to merge with true
                // If left is false, continue to evaluate right
                builder.CreateCondBr(LBool, mergeBB, evalRightBB);
                
                // Generate right evaluation block
                function->insert(function->end(), evalRightBB);
                builder.SetInsertPoint(evalRightBB);
                
                // NOW evaluate right operand (only if left was false)
                llvm::Value* R = codegenExpression(binop->right.get());
                if (!R) return nullptr;
                
                // Convert right to boolean
                llvm::Value* RBool = builder.CreateICmpNE(R, llvm::ConstantInt::get(R->getType(), 0), "or.rhs");
                
                // Jump to merge
                builder.CreateBr(mergeBB);
                // Update evalRightBB in case expression changed the block
                evalRightBB = builder.GetInsertBlock();
                
                // Merge block with PHI node
                function->insert(function->end(), mergeBB);
                builder.SetInsertPoint(mergeBB);
                
                // PHI: true from leftBB (short-circuit), or RBool from evalRightBB
                llvm::PHINode* phi = builder.CreatePHI(builder.getInt1Ty(), 2, "or.result");
                phi->addIncoming(llvm::ConstantInt::get(builder.getInt1Ty(), 1), leftBB);  // true if left was true
                phi->addIncoming(RBool, evalRightBB);  // right's boolean value if left was false
                
                return phi;
            }
            
            // NULL COALESCING OPERATOR (??) with short-circuit evaluation
            // Pattern: optional ?? default returns value if has value, otherwise default
            if (binop->op.type == frontend::TokenType::TOKEN_NULL_COALESCE) {
                llvm::Function* function = builder.GetInsertBlock()->getParent();
                
                // Create blocks for conditional evaluation
                llvm::BasicBlock* useRightBB = llvm::BasicBlock::Create(context, "coalesce.right");
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "coalesce.merge");
                
                // Evaluate left operand
                llvm::Value* leftVal = codegenExpression(binop->left.get());
                if (!leftVal) return nullptr;
                
                // Remember the block where left was evaluated
                llvm::BasicBlock* leftBB = builder.GetInsertBlock();
                
                // Check if left is optional type
                llvm::Value* isNull;
                llvm::Value* unwrappedLeft;
                
                if (leftVal->getType()->isStructTy()) {
                    // Left is optional type { i1 hasValue, T value }
                    // Extract hasValue field
                    llvm::Value* hasValue = builder.CreateExtractValue(leftVal, {0}, "opt.has_value");
                    isNull = builder.CreateNot(hasValue, "isNone");
                    
                    // Extract value for use if has value
                    unwrappedLeft = builder.CreateExtractValue(leftVal, {1}, "opt.value");
                } else if (leftVal->getType()->isPointerTy()) {
                    // For pointers, check against nullptr
                    isNull = builder.CreateIsNull(leftVal, "isnull");
                    unwrappedLeft = leftVal;
                } else if (leftVal->getType()->isIntegerTy()) {
                    // For integers, check against 0 (NIL sentinel)
                    isNull = builder.CreateICmpEQ(leftVal, llvm::ConstantInt::get(leftVal->getType(), 0), "isnull");
                    unwrappedLeft = leftVal;
                } else {
                    // For other types, assume not null
                    isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
                    unwrappedLeft = leftVal;
                }
                
                // If left is null, evaluate and use right; otherwise use unwrapped left
                builder.CreateCondBr(isNull, useRightBB, mergeBB);
                
                // Evaluate right side only if left was null
                function->insert(function->end(), useRightBB);
                builder.SetInsertPoint(useRightBB);
                llvm::Value* rightVal = codegenExpression(binop->right.get());
                if (!rightVal) return nullptr;
                llvm::BasicBlock* rightBB = builder.GetInsertBlock();
                builder.CreateBr(mergeBB);
                
                // Merge with phi node - use unwrapped type
                function->insert(function->end(), mergeBB);
                builder.SetInsertPoint(mergeBB);
                llvm::Type* resultType = unwrappedLeft->getType();
                llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "coalesce.result");
                phi->addIncoming(unwrappedLeft, leftBB);
                phi->addIncoming(rightVal, rightBB);
                
                return phi;
            }
            
            // For all other operators, evaluate both operands
            llvm::Value* L = codegenExpression(binop->left.get());
            llvm::Value* R = codegenExpression(binop->right.get());
            
            if (!L || !R) {
                return nullptr;
            }

            // SPECIAL HANDLING: NIL/NULL literal comparisons with optional/pointer types
            // If one side is NIL literal and other is optional, create proper OptionalNone struct
            auto* leftLiteral = dynamic_cast<LiteralExpr*>(binop->left.get());
            auto* rightLiteral = dynamic_cast<LiteralExpr*>(binop->right.get());
            bool leftIsNIL = (leftLiteral && leftLiteral->explicit_type == "NIL");
            bool rightIsNIL = (rightLiteral && rightLiteral->explicit_type == "NIL");
            
            // Check for unknown literals (polymorphic like NIL)
            bool leftIsUnknown = false;
            bool rightIsUnknown = false;
            if (leftLiteral && std::holds_alternative<std::string>(leftLiteral->value)) {
                leftIsUnknown = (std::get<std::string>(leftLiteral->value) == "unknown");
            }
            if (rightLiteral && std::holds_alternative<std::string>(rightLiteral->value)) {
                rightIsUnknown = (std::get<std::string>(rightLiteral->value) == "unknown");
            }
            
            // SPECIAL HANDLING: Three-valued logic for comparisons (Phase 5.5)
            // When ONE operand is unknown (not both), comparison result is unknown
            // Note: unknown == unknown will naturally compare sentinels and return true
            if ((leftIsUnknown || rightIsUnknown) && !(leftIsUnknown && rightIsUnknown)) {
                // ONE operand is unknown (XOR) - result is unknown for comparisons
                if (binop->op.type == frontend::TokenType::TOKEN_EQUAL_EQUAL ||
                    binop->op.type == frontend::TokenType::TOKEN_BANG_EQUAL ||
                    binop->op.type == frontend::TokenType::TOKEN_LESS ||
                    binop->op.type == frontend::TokenType::TOKEN_LESS_EQUAL ||
                    binop->op.type == frontend::TokenType::TOKEN_GREATER ||
                    binop->op.type == frontend::TokenType::TOKEN_GREATER_EQUAL) {
                    
                    std::cerr << "[DEBUG] Comparison with one unknown operand - returning unknown sentinel" << std::endl;
                    // Result is unknown - return i32 max (unknown sentinel for bool)
                    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 
                                                  llvm::APInt::getSignedMaxValue(32).getSExtValue());
                }
            }
            
            if ((leftIsNIL || rightIsNIL) && (binop->op.type == frontend::TokenType::TOKEN_EQUAL_EQUAL ||
                                              binop->op.type == frontend::TokenType::TOKEN_BANG_EQUAL)) {
                // Check if the other operand is an optional (struct with 2 fields: {i1, T})
                if (leftIsNIL && R->getType()->isStructTy()) {
                    llvm::StructType* structType = llvm::cast<llvm::StructType>(R->getType());
                    if (structType->getNumElements() == 2 && 
                        structType->getElementType(0)->isIntegerTy(1)) {
                        // Right is optional - replace left NIL with OptionalNone
                        L = createOptionalNone(R->getType());
                        std::cerr << "[DEBUG] Replaced left NIL with OptionalNone for comparison" << std::endl;
                    }
                } else if (rightIsNIL && L->getType()->isStructTy()) {
                    llvm::StructType* structType = llvm::cast<llvm::StructType>(L->getType());
                    if (structType->getNumElements() == 2 && 
                        structType->getElementType(0)->isIntegerTy(1)) {
                        // Left is optional - replace right NIL with OptionalNone
                        R = createOptionalNone(L->getType());
                        std::cerr << "[DEBUG] Replaced right NIL with OptionalNone for comparison" << std::endl;
                    }
                }

                // Handle pointer-NIL comparison: NIL generates as empty struct {},
                // but when compared to a pointer we need null pointer instead
                if (leftIsNIL && R->getType()->isPointerTy()) {
                    L = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
                    std::cerr << "[DEBUG] Replaced left NIL with null pointer for pointer comparison" << std::endl;
                } else if (rightIsNIL && L->getType()->isPointerTy()) {
                    R = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
                    std::cerr << "[DEBUG] Replaced right NIL with null pointer for pointer comparison" << std::endl;
                }

                // Handle integer-NIL comparison: NIL sentinel for integers is 0
                if (leftIsNIL && R->getType()->isIntegerTy()) {
                    L = llvm::ConstantInt::get(R->getType(), 0);
                    std::cerr << "[DEBUG] Replaced left NIL with 0 for integer comparison" << std::endl;
                } else if (rightIsNIL && L->getType()->isIntegerTy()) {
                    R = llvm::ConstantInt::get(L->getType(), 0);
                    std::cerr << "[DEBUG] Replaced right NIL with 0 for integer comparison" << std::endl;
                }
            }
            
            // SPECIAL HANDLING: unknown literal comparisons (polymorphic)
            // unknown adapts to other operand's type
            if ((leftIsUnknown || rightIsUnknown) && 
                (binop->op.type == frontend::TokenType::TOKEN_EQUAL_EQUAL ||
                 binop->op.type == frontend::TokenType::TOKEN_BANG_EQUAL)) {
                
                if (leftIsUnknown && R->getType()->isIntegerTy()) {
                    // Replace left unknown with sentinel matching right type
                    llvm::IntegerType* intType = llvm::cast<llvm::IntegerType>(R->getType());
                    unsigned width = intType->getBitWidth();
                    L = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
                    std::cerr << "[DEBUG] Replaced left unknown with sentinel matching type: ";
                    R->getType()->print(llvm::errs());
                    std::cerr << std::endl;
                } else if (rightIsUnknown && L->getType()->isIntegerTy()) {
                    // Replace right unknown with sentinel matching left type
                    llvm::IntegerType* intType = llvm::cast<llvm::IntegerType>(L->getType());
                    unsigned width = intType->getBitWidth();
                    R = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
                    std::cerr << "[DEBUG] Replaced right unknown with sentinel matching type: ";
                    L->getType()->print(llvm::errs());
                    std::cerr << std::endl;
                }
            }
            
            // SPECIAL HANDLING: unknown literal arithmetic propagation (Phase 5.3)
            // If either operand is unknown in an arithmetic operation, result is unknown
            // Check all arithmetic operators: +, -, *, /, %
            if (binop->op.type == frontend::TokenType::TOKEN_PLUS ||
                binop->op.type == frontend::TokenType::TOKEN_MINUS ||
                binop->op.type == frontend::TokenType::TOKEN_STAR ||
                binop->op.type == frontend::TokenType::TOKEN_SLASH ||
                binop->op.type == frontend::TokenType::TOKEN_PERCENT) {
                
                if (leftIsUnknown || rightIsUnknown) {
                    // Result is unknown - return sentinel of appropriate type
                    // Determine result type from the non-unknown operand, or default to int32
                    llvm::Type* resultType = nullptr;
                    
                    if (!leftIsUnknown && L->getType()->isIntegerTy()) {
                        resultType = L->getType();
                    } else if (!rightIsUnknown && R->getType()->isIntegerTy()) {
                        resultType = R->getType();
                    } else {
                        // Both unknown or non-integer types - default to int32
                        resultType = llvm::Type::getInt32Ty(context);
                    }
                    
                    // Generate unknown sentinel for result type
                    llvm::IntegerType* intType = llvm::cast<llvm::IntegerType>(resultType);
                    unsigned width = intType->getBitWidth();
                    llvm::Value* unknownResult = llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
                    
                    std::cerr << "[DEBUG] Arithmetic with unknown operand - returning unknown sentinel" << std::endl;
                    return unknownResult;
                }
            }
            
            // Check if operands are TBB types (requires overflow detection)
            Type* leftType = nullptr;
            Type* rightType = nullptr;
            
            // Try to get types from value_types map
            auto leftIt = value_types.find(L);
            if (leftIt != value_types.end()) {
                leftType = leftIt->second;
            }
            
            auto rightIt = value_types.find(R);
            if (rightIt != value_types.end()) {
                rightType = rightIt->second;
            }
            
            // Check if either operand is a TBB type
            bool isTBB = false;
            Type* tbbType = nullptr;
            if (leftType && leftType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(leftType);
                const std::string& name = prim->getName();
                if (name == "tbb8" || name == "tbb16" || name == "tbb32" || name == "tbb64") {
                    isTBB = true;
                    tbbType = leftType;
                }
            }
            if (!isTBB && rightType && rightType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(rightType);
                const std::string& name = prim->getName();
                if (name == "tbb8" || name == "tbb16" || name == "tbb32" || name == "tbb64") {
                    isTBB = true;
                    tbbType = rightType;
                }
            }
            
            // Check if either operand is a balanced ternary/nonary type
            bool isTernary = false;
            Type* ternaryType = nullptr;
            if (leftType && leftType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(leftType);
                const std::string& name = prim->getName();
                std::cerr << "[DEBUG] Left operand type: " << name << std::endl;
                if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
                    isTernary = true;
                    ternaryType = leftType;
                    std::cerr << "[DEBUG] Detected ternary type on LEFT: " << name << std::endl;
                }
            }
            if (!isTernary && rightType && rightType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(rightType);
                const std::string& name = prim->getName();
                std::cerr << "[DEBUG] Right operand type: " << name << std::endl;
                if (name == "trit" || name == "tryte" || name == "nit" || name == "nyte") {
                    isTernary = true;
                    ternaryType = rightType;
                    std::cerr << "[DEBUG] Detected ternary type on RIGHT: " << name << std::endl;
                }
            }
            
            // Check if either operand is a numeric type (frac*, tfp*, vec9)
            bool isNumericType = false;
            Type* numericType = nullptr;
            if (leftType && leftType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(leftType);
                const std::string& name = prim->getName();
                if (name.find("frac") == 0 || name.find("tfp") == 0 ||
                    name == "vec9" || name == "dvec9") {
                    isNumericType = true;
                    numericType = leftType;
                    std::cerr << "[DEBUG] Detected numeric type on LEFT: " << name << std::endl;
                }
            }
            if (!isNumericType && rightType && rightType->isPrimitive()) {
                PrimitiveType* prim = static_cast<PrimitiveType*>(rightType);
                const std::string& name = prim->getName();
                if (name.find("frac") == 0 || name.find("tfp") == 0 ||
                    name == "vec9" || name == "dvec9") {
                    isNumericType = true;
                    numericType = rightType;
                    std::cerr << "[DEBUG] Detected numeric type on RIGHT: " << name << std::endl;
                }
            }
            
            // Detect unsigned integer operands so comparisons use ICmpU* not ICmpS*
            // Signed arithmetic is identical bit-for-bit, but ordered relational ops differ.
            bool isUnsigned = false;
            auto isUnsignedName = [](const std::string& n) {
                return n == "uint8" || n == "uint16" || n == "uint32" || n == "uint64"
                    || n == "u8" || n == "u16" || n == "u32" || n == "u64";
            };
            if (leftType && leftType->isPrimitive())
                if (isUnsignedName(static_cast<PrimitiveType*>(leftType)->getName())) isUnsigned = true;
            if (!isUnsigned && rightType && rightType->isPrimitive())
                if (isUnsignedName(static_cast<PrimitiveType*>(rightType)->getName())) isUnsigned = true;
            if (!isUnsigned && binop->left->type == ASTNode::NodeType::IDENTIFIER) {
                auto* id = static_cast<IdentifierExpr*>(binop->left.get());
                auto it = var_aria_types.find(id->name);
                if (it != var_aria_types.end() && isUnsignedName(it->second)) isUnsigned = true;
            }
            if (!isUnsigned && binop->right->type == ASTNode::NodeType::IDENTIFIER) {
                auto* id = static_cast<IdentifierExpr*>(binop->right.get());
                auto it = var_aria_types.find(id->name);
                if (it != var_aria_types.end() && isUnsignedName(it->second)) isUnsigned = true;
            }
            if (!isUnsigned && leftLiteral && isUnsignedName(leftLiteral->explicit_type)) isUnsigned = true;
            if (!isUnsigned && rightLiteral && isUnsignedName(rightLiteral->explicit_type)) isUnsigned = true;

            // Detect string operands for comparison operators
            bool isStringOp = false;
            if (leftType && leftType->isPrimitive() &&
                static_cast<PrimitiveType*>(leftType)->getName() == "string")
                isStringOp = true;
            if (!isStringOp && rightType && rightType->isPrimitive() &&
                static_cast<PrimitiveType*>(rightType)->getName() == "string")
                isStringOp = true;
            if (!isStringOp && binop->left->type == ASTNode::NodeType::IDENTIFIER) {
                auto* id = static_cast<IdentifierExpr*>(binop->left.get());
                auto it = var_aria_types.find(id->name);
                if (it != var_aria_types.end() && it->second == "string") isStringOp = true;
            }
            if (!isStringOp && binop->right->type == ASTNode::NodeType::IDENTIFIER) {
                auto* id = static_cast<IdentifierExpr*>(binop->right.get());
                auto it = var_aria_types.find(id->name);
                if (it != var_aria_types.end() && it->second == "string") isStringOp = true;
            }
            if (!isStringOp && binop->left->type == ASTNode::NodeType::LITERAL) {
                auto* lit = static_cast<LiteralExpr*>(binop->left.get());
                if (std::holds_alternative<std::string>(lit->value)) {
                    const std::string& s = std::get<std::string>(lit->value);
                    if (s != "unknown" && s != "ERR") isStringOp = true;
                }
            }
            if (!isStringOp && binop->right->type == ASTNode::NodeType::LITERAL) {
                auto* lit = static_cast<LiteralExpr*>(binop->right.get());
                if (std::holds_alternative<std::string>(lit->value)) {
                    const std::string& s = std::get<std::string>(lit->value);
                    if (s != "unknown" && s != "ERR") isStringOp = true;
                }
            }

            // Helper lambda: call aria_string_compare_str and return i32 result (-1/0/1)
            auto emitStringCompare = [&]() -> llvm::Value* {
                llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                if (!ariaStrType) {
                    ariaStrType = llvm::StructType::create(context,
                        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                         llvm::Type::getInt64Ty(context)},
                        "struct.AriaString");
                }
                llvm::FunctionType* cmpFT = llvm::FunctionType::get(
                    llvm::Type::getInt32Ty(context),
                    {ariaStrType, ariaStrType}, false);
                llvm::FunctionCallee cmpFn = module->getOrInsertFunction("aria_string_compare_str", cmpFT);
                llvm::Value* lStr = L->getType()->isPointerTy()
                    ? builder.CreateLoad(ariaStrType, L, "str.cmp.lhs")
                    : L;
                llvm::Value* rStr = R->getType()->isPointerTy()
                    ? builder.CreateLoad(ariaStrType, R, "str.cmp.rhs")
                    : R;
                return builder.CreateCall(cmpFn, {lStr, rStr}, "str.cmp");
            };

            // Auto-unwrap Result<T> structs in binary expressions.
            // Aria functions return { T, ptr, i8 } but binary ops need raw T.
            auto isResultStruct = [](llvm::Type* ty) -> bool {
                if (!ty->isStructTy()) return false;
                llvm::StructType* st = llvm::cast<llvm::StructType>(ty);
                return st->getNumElements() == 3 &&
                       st->getElementType(1)->isPointerTy() &&
                       st->getElementType(2)->isIntegerTy(8);
            };
            if (isResultStruct(L->getType()) && !isResultStruct(R->getType())) {
                L = builder.CreateExtractValue(L, {0}, "unwrap_result_lhs");
                std::cerr << "[DEBUG] Auto-unwrapped Result<T> on left operand" << std::endl;
            }
            if (isResultStruct(R->getType()) && !isResultStruct(L->getType())) {
                R = builder.CreateExtractValue(R, {0}, "unwrap_result_rhs");
                std::cerr << "[DEBUG] Auto-unwrapped Result<T> on right operand" << std::endl;
            }

            // Generate appropriate operation based on operator
            switch (binop->op.type) {
                // Arithmetic operators
                case frontend::TokenType::TOKEN_PLUS:
                    // Check if either operand is a vector
                    if (leftType && leftType->getKind() == TypeKind::VECTOR) {
                        // Vector addition (vector + vector or vector + scalar)
                        VectorType* vec_type = static_cast<VectorType*>(leftType);
                        if (rightType && rightType->getKind() != TypeKind::VECTOR) {
                            // vector + scalar: broadcast scalar to vector
                            R = builder.CreateVectorSplat(vec_type->getDimension(), R, "scalar.splat");
                        }
                        // Use FAdd for floating point vectors
                        return builder.CreateFAdd(L, R, "vec.addtmp");
                    }
                    if (rightType && rightType->getKind() == TypeKind::VECTOR) {
                        // scalar + vector: broadcast scalar to vector
                        VectorType* vec_type = static_cast<VectorType*>(rightType);
                        L = builder.CreateVectorSplat(vec_type->getDimension(), L, "scalar.splat");
                        return builder.CreateFAdd(L, R, "vec.addtmp");
                    }
                    
                    if (isTBB && tbbType) {
                        // Ensure both operands are the same type as the TBB type
                        llvm::Type* tbbLLVMType = tbb_codegen.getTBBLLVMType(tbbType);
                        if (L->getType() != tbbLLVMType) {
                            L = builder.CreateIntCast(L, tbbLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != tbbLLVMType) {
                            R = builder.CreateIntCast(R, tbbLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = tbb_codegen.generateAdd(L, R, tbbType);
                        value_types[result] = tbbType;  // Track type for result
                        return result;
                    }
                    if (isTernary && ternaryType) {
                        // Ensure both operands are the same type as the ternary type
                        llvm::Type* ternaryLLVMType = ternary_codegen.getTernaryLLVMType(ternaryType);
                        if (L->getType() != ternaryLLVMType) {
                            L = builder.CreateIntCast(L, ternaryLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != ternaryLLVMType) {
                            R = builder.CreateIntCast(R, ternaryLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = ternary_codegen.generateAdd(L, R, ternaryType);
                        value_types[result] = ternaryType;  // Track type for result
                        return result;
                    }
                    if (isNumericType && numericType) {
                        // Generate runtime function call for numeric type addition
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_add";
                        
                        // frac types use sret/pointer ABI: void aria_frac32_add(Frac32*, const Frac32*, const Frac32*)
                        if (typeName.find("frac") == 0) {
                            llvm::Type* fracType = L->getType();
                            llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                            llvm::FunctionType* funcType = llvm::FunctionType::get(
                                llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                            llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                            llvm::Value* resultAlloca = builder.CreateAlloca(fracType, nullptr, "frac.add_ret");
                            llvm::Value* leftAlloca = builder.CreateAlloca(fracType, nullptr, "frac.add_l");
                            llvm::Value* rightAlloca = builder.CreateAlloca(fracType, nullptr, "frac.add_r");
                            builder.CreateStore(L, leftAlloca);
                            builder.CreateStore(R, rightAlloca);
                            builder.CreateCall(runtimeFunc, {resultAlloca, leftAlloca, rightAlloca});
                            llvm::Value* result = builder.CreateLoad(fracType, resultAlloca, typeName + "_result");
                            value_types[result] = numericType;
                            std::cerr << "[DEBUG] Generated numeric addition call: " << funcName << std::endl;
                            return result;
                        }
                        
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            numericLLVMType, {numericLLVMType, numericLLVMType}, false);
                        llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* result = builder.CreateCall(runtimeFunc, {L, R}, typeName + "_result");
                        value_types[result] = numericType;
                        std::cerr << "[DEBUG] Generated numeric addition call: " << funcName << std::endl;
                        return result;
                    }
                    // ARIA-024: LBIM addition for large integers (int128/256/512/1024)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMAdd(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 deterministic fixed-point addition
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        // SysV ABI: 32-byte struct returned via sret hidden first param
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                        llvm::FunctionCallee addFunc = module->getOrInsertFunction(
                            "aria_fix256_add", funcType);
                        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.add_ret");
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.add_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.add_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(addFunc, {resultAlloca, leftAlloca, rightAlloca});
                        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateLoad(fix256Type, resultAlloca, "fix256.add");
                    }
                    // Float addition: use FAdd for floating-point types (scalar or SIMD vector)
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        // Harmonize float types (e.g., float + double → double)
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "add_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "add_fpext");
                        }
                        return builder.CreateFAdd(L, R, "faddtmp");
                    }
                    // String concatenation: detect string operands and call runtime
                    {
                        bool leftIsStr = false, rightIsStr = false;
                        if (leftType && leftType->isPrimitive() &&
                            static_cast<PrimitiveType*>(leftType)->getName() == "string")
                            leftIsStr = true;
                        if (rightType && rightType->isPrimitive() &&
                            static_cast<PrimitiveType*>(rightType)->getName() == "string")
                            rightIsStr = true;
                        if (!leftIsStr && binop->left->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->left.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") leftIsStr = true;
                        }
                        if (!rightIsStr && binop->right->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->right.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") rightIsStr = true;
                        }
                        if (!leftIsStr && binop->left->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->left.get());
                            if (std::holds_alternative<std::string>(lit->value)) leftIsStr = true;
                        }
                        if (!rightIsStr && binop->right->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->right.get());
                            if (std::holds_alternative<std::string>(lit->value)) rightIsStr = true;
                        }
                        if (leftIsStr || rightIsStr) {
                            std::cerr << "[STRING +] Emitting aria_string_concat_simple call" << std::endl;
                            llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                            if (!ariaStrType) {
                                ariaStrType = llvm::StructType::create(context,
                                    {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                                     llvm::Type::getInt64Ty(context)},
                                    "struct.AriaString");
                            }
                            llvm::Type* strPtrTy = llvm::PointerType::get(ariaStrType, 0);
                            llvm::FunctionType* concatFT = llvm::FunctionType::get(
                                strPtrTy, {strPtrTy, strPtrTy}, false);
                            llvm::FunctionCallee concatFn = module->getOrInsertFunction("aria_string_concat_simple", concatFT);
                            // L and R are already AriaString* pointers
                            return builder.CreateCall(concatFn, {L, R}, "str.concat");
                        }
                    }
                    // Layer 1 Safety: Safe addition returns Unknown on overflow
                    return generateSafeAdd(L, R, "addtmp");
                case frontend::TokenType::TOKEN_MINUS:
                    // Check if either operand is a vector
                    if (leftType && leftType->getKind() == TypeKind::VECTOR) {
                        // Vector subtraction
                        VectorType* vec_type = static_cast<VectorType*>(leftType);
                        if (rightType && rightType->getKind() != TypeKind::VECTOR) {
                            R = builder.CreateVectorSplat(vec_type->getDimension(), R, "scalar.splat");
                        }
                        return builder.CreateFSub(L, R, "vec.subtmp");
                    }
                    if (rightType && rightType->getKind() == TypeKind::VECTOR) {
                        VectorType* vec_type = static_cast<VectorType*>(rightType);
                        L = builder.CreateVectorSplat(vec_type->getDimension(), L, "scalar.splat");
                        return builder.CreateFSub(L, R, "vec.subtmp");
                    }
                    
                    if (isTBB && tbbType) {
                        // Ensure both operands are the same type as the TBB type
                        llvm::Type* tbbLLVMType = tbb_codegen.getTBBLLVMType(tbbType);
                        if (L->getType() != tbbLLVMType) {
                            L = builder.CreateIntCast(L, tbbLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != tbbLLVMType) {
                            R = builder.CreateIntCast(R, tbbLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = tbb_codegen.generateSub(L, R, tbbType);
                        value_types[result] = tbbType;  // Track type for result
                        return result;
                    }
                    if (isTernary && ternaryType) {
                        // Ensure both operands are the same type as the ternary type
                        llvm::Type* ternaryLLVMType = ternary_codegen.getTernaryLLVMType(ternaryType);
                        if (L->getType() != ternaryLLVMType) {
                            L = builder.CreateIntCast(L, ternaryLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != ternaryLLVMType) {
                            R = builder.CreateIntCast(R, ternaryLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = ternary_codegen.generateSub(L, R, ternaryType);
                        value_types[result] = ternaryType;  // Track type for result
                        return result;
                    }
                    if (isNumericType && numericType) {
                        // Generate runtime function call for numeric type subtraction
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_sub";
                        
                        // frac types use sret/pointer ABI
                        if (typeName.find("frac") == 0) {
                            llvm::Type* fracType = L->getType();
                            llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                            llvm::FunctionType* funcType = llvm::FunctionType::get(
                                llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                            llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                            llvm::Value* resultAlloca = builder.CreateAlloca(fracType, nullptr, "frac.sub_ret");
                            llvm::Value* leftAlloca = builder.CreateAlloca(fracType, nullptr, "frac.sub_l");
                            llvm::Value* rightAlloca = builder.CreateAlloca(fracType, nullptr, "frac.sub_r");
                            builder.CreateStore(L, leftAlloca);
                            builder.CreateStore(R, rightAlloca);
                            builder.CreateCall(runtimeFunc, {resultAlloca, leftAlloca, rightAlloca});
                            llvm::Value* result = builder.CreateLoad(fracType, resultAlloca, typeName + "_result");
                            value_types[result] = numericType;
                            std::cerr << "[DEBUG] Generated numeric subtraction call: " << funcName << std::endl;
                            return result;
                        }
                        
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            numericLLVMType, {numericLLVMType, numericLLVMType}, false);
                        llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* result = builder.CreateCall(runtimeFunc, {L, R}, typeName + "_result");
                        value_types[result] = numericType;
                        std::cerr << "[DEBUG] Generated numeric subtraction call: " << funcName << std::endl;
                        return result;
                    }
                    // ARIA-024: LBIM subtraction for large integers (int128/256/512/1024)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMSub(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 deterministic fixed-point subtraction
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                        llvm::FunctionCallee subFunc = module->getOrInsertFunction(
                            "aria_fix256_sub", funcType);
                        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.sub_ret");
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.sub_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.sub_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(subFunc, {resultAlloca, leftAlloca, rightAlloca});
                        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateLoad(fix256Type, resultAlloca, "fix256.sub");
                    }
                    // Float subtraction: use FSub for floating-point types (scalar or SIMD vector)
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        // Harmonize float types (e.g., float - double → double)
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "sub_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "sub_fpext");
                        }
                        return builder.CreateFSub(L, R, "fsubtmp");
                    }
                    // Layer 1 Safety: Safe subtraction returns Unknown on overflow
                    return generateSafeSub(L, R, "subtmp");
                case frontend::TokenType::TOKEN_STAR:
                    // Check if either operand is a vector
                    if (leftType && leftType->getKind() == TypeKind::VECTOR) {
                        // Vector multiplication
                        VectorType* vec_type = static_cast<VectorType*>(leftType);
                        if (rightType && rightType->getKind() != TypeKind::VECTOR) {
                            R = builder.CreateVectorSplat(vec_type->getDimension(), R, "scalar.splat");
                        }
                        return builder.CreateFMul(L, R, "vec.multmp");
                    }
                    if (rightType && rightType->getKind() == TypeKind::VECTOR) {
                        VectorType* vec_type = static_cast<VectorType*>(rightType);
                        L = builder.CreateVectorSplat(vec_type->getDimension(), L, "scalar.splat");
                        return builder.CreateFMul(L, R, "vec.multmp");
                    }
                    
                    if (isTBB && tbbType) {
                        // Ensure both operands are the same type as the TBB type
                        llvm::Type* tbbLLVMType = tbb_codegen.getTBBLLVMType(tbbType);
                        if (L->getType() != tbbLLVMType) {
                            L = builder.CreateIntCast(L, tbbLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != tbbLLVMType) {
                            R = builder.CreateIntCast(R, tbbLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = tbb_codegen.generateMul(L, R, tbbType);
                        value_types[result] = tbbType;  // Track type for result
                        return result;
                    }
                    if (isTernary && ternaryType) {
                        // Ensure both operands are the same type as the ternary type
                        llvm::Type* ternaryLLVMType = ternary_codegen.getTernaryLLVMType(ternaryType);
                        if (L->getType() != ternaryLLVMType) {
                            L = builder.CreateIntCast(L, ternaryLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != ternaryLLVMType) {
                            R = builder.CreateIntCast(R, ternaryLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = ternary_codegen.generateMul(L, R, ternaryType);
                        value_types[result] = ternaryType;  // Track type for result
                        return result;
                    }
                    if (isNumericType && numericType) {
                        // Generate runtime function call for numeric type multiplication
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_mul";
                        
                        // frac types use sret/pointer ABI
                        if (typeName.find("frac") == 0) {
                            llvm::Type* fracType = L->getType();
                            llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                            llvm::FunctionType* funcType = llvm::FunctionType::get(
                                llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                            llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                            llvm::Value* resultAlloca = builder.CreateAlloca(fracType, nullptr, "frac.mul_ret");
                            llvm::Value* leftAlloca = builder.CreateAlloca(fracType, nullptr, "frac.mul_l");
                            llvm::Value* rightAlloca = builder.CreateAlloca(fracType, nullptr, "frac.mul_r");
                            builder.CreateStore(L, leftAlloca);
                            builder.CreateStore(R, rightAlloca);
                            builder.CreateCall(runtimeFunc, {resultAlloca, leftAlloca, rightAlloca});
                            llvm::Value* result = builder.CreateLoad(fracType, resultAlloca, typeName + "_result");
                            value_types[result] = numericType;
                            std::cerr << "[DEBUG] Generated numeric multiplication call: " << funcName << std::endl;
                            return result;
                        }
                        
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            numericLLVMType, {numericLLVMType, numericLLVMType}, false);
                        llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* result = builder.CreateCall(runtimeFunc, {L, R}, typeName + "_result");
                        value_types[result] = numericType;
                        std::cerr << "[DEBUG] Generated numeric multiplication call: " << funcName << std::endl;
                        return result;
                    }
                    // ARIA-024: LBIM multiplication for large integers (int128/256/512/1024)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMMul(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 deterministic fixed-point multiplication
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                        llvm::FunctionCallee mulFunc = module->getOrInsertFunction(
                            "aria_fix256_mul", funcType);
                        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.mul_ret");
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.mul_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.mul_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(mulFunc, {resultAlloca, leftAlloca, rightAlloca});
                        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateLoad(fix256Type, resultAlloca, "fix256.mul");
                    }
                    // Float multiplication: use FMul for floating-point types (scalar or SIMD vector)
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        // Harmonize float types (e.g., float * double → double)
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "mul_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "mul_fpext");
                        }
                        return builder.CreateFMul(L, R, "fmultmp");
                    }
                    // Unsigned integer types (uint8/16/32/64): use plain wrapping mul.
                    // Wrapping is the correct semantic for unsigned modular arithmetic.
                    // Do NOT use generateSafeMul (signed saturation) for unsigned types.
                    if (isUnsigned) {
                        return builder.CreateMul(L, R, "multmp");
                    }
                    // Layer 1 Safety: Safe multiplication returns Unknown on overflow
                    return generateSafeMul(L, R, "multmp");
                case frontend::TokenType::TOKEN_SLASH:
                    // Check if either operand is a vector
                    if (leftType && leftType->getKind() == TypeKind::VECTOR) {
                        // Vector division
                        VectorType* vec_type = static_cast<VectorType*>(leftType);
                        if (rightType && rightType->getKind() != TypeKind::VECTOR) {
                            R = builder.CreateVectorSplat(vec_type->getDimension(), R, "scalar.splat");
                        }
                        return builder.CreateFDiv(L, R, "vec.divtmp");
                    }
                    if (rightType && rightType->getKind() == TypeKind::VECTOR) {
                        VectorType* vec_type = static_cast<VectorType*>(rightType);
                        L = builder.CreateVectorSplat(vec_type->getDimension(), L, "scalar.splat");
                        return builder.CreateFDiv(L, R, "vec.divtmp");
                    }
                    
                    if (isTBB && tbbType) {
                        // Ensure both operands are the same type as the TBB type
                        llvm::Type* tbbLLVMType = tbb_codegen.getTBBLLVMType(tbbType);
                        if (L->getType() != tbbLLVMType) {
                            L = builder.CreateIntCast(L, tbbLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != tbbLLVMType) {
                            R = builder.CreateIntCast(R, tbbLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = tbb_codegen.generateDiv(L, R, tbbType);
                        value_types[result] = tbbType;  // Track type for result
                        return result;
                    }
                    if (isTernary && ternaryType) {
                        // Ensure both operands are the same type as the ternary type
                        llvm::Type* ternaryLLVMType = ternary_codegen.getTernaryLLVMType(ternaryType);
                        if (L->getType() != ternaryLLVMType) {
                            L = builder.CreateIntCast(L, ternaryLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != ternaryLLVMType) {
                            R = builder.CreateIntCast(R, ternaryLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = ternary_codegen.generateDiv(L, R, ternaryType);
                        value_types[result] = ternaryType;  // Track type for result
                        return result;
                    }
                    if (isNumericType && numericType) {
                        // Generate runtime function call for numeric type division
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_div";
                        
                        // frac types use sret/pointer ABI
                        if (typeName.find("frac") == 0) {
                            llvm::Type* fracType = L->getType();
                            llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                            llvm::FunctionType* funcType = llvm::FunctionType::get(
                                llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                            llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                            llvm::Value* resultAlloca = builder.CreateAlloca(fracType, nullptr, "frac.div_ret");
                            llvm::Value* leftAlloca = builder.CreateAlloca(fracType, nullptr, "frac.div_l");
                            llvm::Value* rightAlloca = builder.CreateAlloca(fracType, nullptr, "frac.div_r");
                            builder.CreateStore(L, leftAlloca);
                            builder.CreateStore(R, rightAlloca);
                            builder.CreateCall(runtimeFunc, {resultAlloca, leftAlloca, rightAlloca});
                            llvm::Value* result = builder.CreateLoad(fracType, resultAlloca, typeName + "_result");
                            value_types[result] = numericType;
                            std::cerr << "[DEBUG] Generated numeric division call: " << funcName << std::endl;
                            return result;
                        }
                        
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            numericLLVMType, {numericLLVMType, numericLLVMType}, false);
                        llvm::FunctionCallee runtimeFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* result = builder.CreateCall(runtimeFunc, {L, R}, typeName + "_result");
                        value_types[result] = numericType;
                        std::cerr << "[DEBUG] Generated numeric division call: " << funcName << std::endl;
                        return result;
                    }
                    // ARIA-024: LBIM division for large integers (int128/256/512/1024)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMDiv(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 deterministic fixed-point division
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            llvm::Type::getVoidTy(context), {ptrType, ptrType, ptrType}, false);
                        llvm::FunctionCallee divFunc = module->getOrInsertFunction(
                            "aria_fix256_div", funcType);
                        llvm::Value* resultAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.div_ret");
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.div_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.div_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(divFunc, {resultAlloca, leftAlloca, rightAlloca});
                        call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(2, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateLoad(fix256Type, resultAlloca, "fix256.div");
                    }
                    // Float division: use FDiv for floating-point types (scalar or SIMD vector)
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        // Harmonize float types (e.g., float / double → double)
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "div_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "div_fpext");
                        }
                        return builder.CreateFDiv(L, R, "fdivtmp");
                    }
                    // Layer 1 Safety: Safe division returns Unknown on divide-by-zero
                    return generateSafeSDiv(L, R, "divtmp");
                case frontend::TokenType::TOKEN_PERCENT:
                    std::cerr << "[DEBUG MODULO] isTBB=" << isTBB << ", isTernary=" << isTernary << std::endl;
                    // TBB modulo operator lowering
                    if (isTBB && tbbType) {
                        std::cerr << "[DEBUG MODULO] Using TBB codegen" << std::endl;
                        // Ensure both operands are the same type as the TBB type
                        llvm::Type* tbbLLVMType = tbb_codegen.getTBBLLVMType(tbbType);
                        if (L->getType() != tbbLLVMType) {
                            L = builder.CreateIntCast(L, tbbLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != tbbLLVMType) {
                            R = builder.CreateIntCast(R, tbbLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = tbb_codegen.generateMod(L, R, tbbType);
                        value_types[result] = tbbType;  // Track type for result
                        return result;
                    }
                    // CRITICAL FIX: Balanced ternary/nonary modulo (tryte/nyte)
                    // Must use ternary_codegen which calls aria_tryte_mod/aria_nyte_mod
                    // instead of native srem (which breaks homomorphism on biased values)
                    if (isTernary && ternaryType) {
                        std::cerr << "[DEBUG MODULO] Using TERNARY codegen for type: " << ternaryType->toString() << std::endl;
                        // Ensure both operands are the same type as the ternary type
                        llvm::Type* ternaryLLVMType = ternary_codegen.getTernaryLLVMType(ternaryType);
                        if (L->getType() != ternaryLLVMType) {
                            L = builder.CreateIntCast(L, ternaryLLVMType, true, "cast_lhs");
                        }
                        if (R->getType() != ternaryLLVMType) {
                            R = builder.CreateIntCast(R, ternaryLLVMType, true, "cast_rhs");
                        }
                        
                        llvm::Value* result = ternary_codegen.generateMod(L, R, ternaryType);
                        value_types[result] = ternaryType;  // Track type for result
                        return result;
                    }
                    std::cerr << "[DEBUG MODULO] Falling through to native srem" << std::endl;
                    // ARIA-024: LBIM modulo for large integers (int128/256/512)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMMod(L, R, numLimbs);
                    }
                    // Layer 1 Safety: Safe modulo returns Unknown on divide-by-zero
                    return generateSafeSRem(L, R, "modtmp");

                // Comparison operators (return i1)
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    std::cerr << "[DEBUG] TOKEN_EQUAL_EQUAL: L type=" << (L->getType()->isStructTy() ? "STRUCT" : "NOT_STRUCT") << std::endl;
                    std::cerr << "[DEBUG] TOKEN_EQUAL_EQUAL: R type=" << (R->getType()->isStructTy() ? "STRUCT" : "NOT_STRUCT") << std::endl;
                    // ARIA-024: LBIM equality comparison for large integers
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        std::cerr << "[DEBUG] LBIM equality comparison detected, numLimbs=" << numLimbs << std::endl;
                        return generateLBIMEQ(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 equality comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee eqFunc = module->getOrInsertFunction(
                            "aria_fix256_eq", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.eq_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.eq_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(eqFunc, {leftAlloca, rightAlloca}, "fix256.eq");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpNE(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.eqbool");
                    }
                    // Optional type comparison: check hasValue fields
                    if (L->getType()->isStructTy() && R->getType()->isStructTy()) {
                        llvm::StructType* leftStruct = llvm::cast<llvm::StructType>(L->getType());
                        llvm::StructType* rightStruct = llvm::cast<llvm::StructType>(R->getType());
                        // Check if both are optional structs {i1, T}
                        if (leftStruct->getNumElements() == 2 && leftStruct->getElementType(0)->isIntegerTy(1) &&
                            rightStruct->getNumElements() == 2 && rightStruct->getElementType(0)->isIntegerTy(1)) {
                            std::cerr << "[DEBUG] Comparing two optional types" << std::endl;
                            // For optional comparison, we compare hasValue fields
                            // Two optionals are equal if both are NIL or both have same value
                            llvm::Value* leftHasValue = builder.CreateExtractValue(L, 0, "left.hasValue");
                            llvm::Value* rightHasValue = builder.CreateExtractValue(R, 0, "right.hasValue");
                            
                            // Simple comparison: just check if hasValue fields match
                            // For now, only support NIL comparisons (one has value, one doesn't)
                            return builder.CreateICmpEQ(leftHasValue, rightHasValue, "optional.eq");
                        }
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        // Allocate space for operands and pass as pointers
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpEQ(cmpResult, zero, "eq_result");
                    }
                    // STRING EQUALITY: detect string types and call aria_string_equals
                    // Pointer equality for strings gives wrong results — two literals with the
                    // same content but different addresses would compare as unequal.
                    {
                        bool leftIsString = false, rightIsString = false;
                        // Check via value_types (covers function parameters registered there)
                        if (leftType && leftType->isPrimitive() &&
                            static_cast<PrimitiveType*>(leftType)->getName() == "string")
                            leftIsString = true;
                        if (rightType && rightType->isPrimitive() &&
                            static_cast<PrimitiveType*>(rightType)->getName() == "string")
                            rightIsString = true;
                        // Check via var_aria_types (covers local variables and globals)
                        if (!leftIsString && binop->left->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->left.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") leftIsString = true;
                        }
                        if (!rightIsString && binop->right->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->right.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") rightIsString = true;
                        }
                        // Check if either side is a string literal (always a string)
                        if (!leftIsString && binop->left->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->left.get());
                            if (std::holds_alternative<std::string>(lit->value)) {
                                const std::string& s = std::get<std::string>(lit->value);
                                if (s != "unknown" && s != "ERR") leftIsString = true;
                            }
                        }
                        if (!rightIsString && binop->right->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->right.get());
                            if (std::holds_alternative<std::string>(lit->value)) {
                                const std::string& s = std::get<std::string>(lit->value);
                                if (s != "unknown" && s != "ERR") rightIsString = true;
                            }
                        }
                        if (leftIsString || rightIsString) {
                            std::cerr << "[STRING ==] Emitting aria_string_equals call" << std::endl;
                            llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                            if (!ariaStrType) {
                                ariaStrType = llvm::StructType::create(context,
                                    {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                                     llvm::Type::getInt64Ty(context)},
                                    "struct.AriaString");
                            }
                            llvm::FunctionType* eqFT = llvm::FunctionType::get(
                                llvm::Type::getInt1Ty(context),
                                {ariaStrType, ariaStrType}, false);
                            llvm::FunctionCallee eqFn = module->getOrInsertFunction("aria_string_equals", eqFT);
                            // Load AriaString value from pointer (both sides are ptr to AriaString)
                            llvm::Value* lStr = L->getType()->isPointerTy()
                                ? builder.CreateLoad(ariaStrType, L, "str.lhs")
                                : L;
                            llvm::Value* rStr = R->getType()->isPointerTy()
                                ? builder.CreateLoad(ariaStrType, R, "str.rhs")
                                : R;
                            return builder.CreateCall(eqFn, {lStr, rStr}, "str.eq");
                        }
                    }
                    // Ensure operands have same type for comparison
                    if (L->getType() != R->getType()) {
                        // Promote to larger type (only for integer types)
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                        // Promote to wider float type when mixing float/double
                        else if (L->getType()->getScalarType()->isFloatingPointTy() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            unsigned lBits = L->getType()->getScalarSizeInBits();
                            unsigned rBits = R->getType()->getScalarSizeInBits();
                            if (lBits > rBits) {
                                R = builder.CreateFPExt(R, L->getType(), "cmp_fpext");
                            } else {
                                L = builder.CreateFPExt(L, R->getType(), "cmp_fpext");
                            }
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        return builder.CreateFCmpOEQ(L, R, "eqtmp");
                    } else {
                        return builder.CreateICmpEQ(L, R, "eqtmp");
                    }
                case frontend::TokenType::TOKEN_BANG_EQUAL:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    // ARIA-024: LBIM inequality comparison for large integers
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        llvm::Value* eq = generateLBIMEQ(L, R, numLimbs);
                        return builder.CreateNot(eq, "netmp");
                    }
                    // ARIA-025: fix256 inequality comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee eqFunc = module->getOrInsertFunction(
                            "aria_fix256_eq", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.ne_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.ne_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(eqFunc, {leftAlloca, rightAlloca}, "fix256.eq");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpEQ(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.nebool");
                    }
                    // Optional type comparison: check hasValue fields (same as ==, then negate)
                    if (L->getType()->isStructTy() && R->getType()->isStructTy()) {
                        llvm::StructType* leftStruct = llvm::cast<llvm::StructType>(L->getType());
                        llvm::StructType* rightStruct = llvm::cast<llvm::StructType>(R->getType());
                        // Check if both are optional structs {i1, T}
                        if (leftStruct->getNumElements() == 2 && leftStruct->getElementType(0)->isIntegerTy(1) &&
                            rightStruct->getNumElements() == 2 && rightStruct->getElementType(0)->isIntegerTy(1)) {
                            std::cerr << "[DEBUG] Comparing two optional types (!=)" << std::endl;
                            // For optional comparison, we compare hasValue fields
                            llvm::Value* leftHasValue = builder.CreateExtractValue(L, 0, "left.hasValue");
                            llvm::Value* rightHasValue = builder.CreateExtractValue(R, 0, "right.hasValue");
                            
                            // Check if hasValue fields differ (for !=)
                            return builder.CreateICmpNE(leftHasValue, rightHasValue, "optional.ne");
                        }
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpNE(cmpResult, zero, "ne_result");
                    }
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // STRING INEQUALITY: use aria_string_equals and negate
                    {
                        bool leftIsString = false, rightIsString = false;
                        if (leftType && leftType->isPrimitive() &&
                            static_cast<PrimitiveType*>(leftType)->getName() == "string")
                            leftIsString = true;
                        if (rightType && rightType->isPrimitive() &&
                            static_cast<PrimitiveType*>(rightType)->getName() == "string")
                            rightIsString = true;
                        if (!leftIsString && binop->left->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->left.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") leftIsString = true;
                        }
                        if (!rightIsString && binop->right->type == ASTNode::NodeType::IDENTIFIER) {
                            auto* id = static_cast<IdentifierExpr*>(binop->right.get());
                            auto it2 = var_aria_types.find(id->name);
                            if (it2 != var_aria_types.end() && it2->second == "string") rightIsString = true;
                        }
                        if (!leftIsString && binop->left->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->left.get());
                            if (std::holds_alternative<std::string>(lit->value)) {
                                const std::string& s = std::get<std::string>(lit->value);
                                if (s != "unknown" && s != "ERR") leftIsString = true;
                            }
                        }
                        if (!rightIsString && binop->right->type == ASTNode::NodeType::LITERAL) {
                            auto* lit = static_cast<LiteralExpr*>(binop->right.get());
                            if (std::holds_alternative<std::string>(lit->value)) {
                                const std::string& s = std::get<std::string>(lit->value);
                                if (s != "unknown" && s != "ERR") rightIsString = true;
                            }
                        }
                        if (leftIsString || rightIsString) {
                            std::cerr << "[STRING !=] Emitting aria_string_equals + negate" << std::endl;
                            llvm::StructType* ariaStrType = llvm::StructType::getTypeByName(context, "struct.AriaString");
                            if (!ariaStrType) {
                                ariaStrType = llvm::StructType::create(context,
                                    {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                                     llvm::Type::getInt64Ty(context)},
                                    "struct.AriaString");
                            }
                            llvm::FunctionType* eqFT = llvm::FunctionType::get(
                                llvm::Type::getInt1Ty(context),
                                {ariaStrType, ariaStrType}, false);
                            llvm::FunctionCallee eqFn = module->getOrInsertFunction("aria_string_equals", eqFT);
                            llvm::Value* lStr = L->getType()->isPointerTy()
                                ? builder.CreateLoad(ariaStrType, L, "str.lhs")
                                : L;
                            llvm::Value* rStr = R->getType()->isPointerTy()
                                ? builder.CreateLoad(ariaStrType, R, "str.rhs")
                                : R;
                            llvm::Value* eq = builder.CreateCall(eqFn, {lStr, rStr}, "str.eq");
                            return builder.CreateNot(eq, "str.ne");
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "ne_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "ne_fpext");
                        }
                        return builder.CreateFCmpONE(L, R, "netmp");
                    } else {
                        return builder.CreateICmpNE(L, R, "netmp");
                    }
                case frontend::TokenType::TOKEN_LESS:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    // String comparison: a < b
                    if (isStringOp) {
                        llvm::Value* cmp = emitStringCompare();
                        return builder.CreateICmpSLT(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "str.lt");
                    }
                    // ARIA-024: LBIM comparison for large integers
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMSLT(L, R, numLimbs);
                    }
                    // ARIA-025: fix256 less-than comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee ltFunc = module->getOrInsertFunction(
                            "aria_fix256_lt", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.lt_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.lt_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(ltFunc, {leftAlloca, rightAlloca}, "fix256.lt");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpNE(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.ltbool");
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpSLT(cmpResult, zero, "lt_result");
                    }
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "lt_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "lt_fpext");
                        }
                        return builder.CreateFCmpOLT(L, R, "lttmp");
                    } else if (isUnsigned) {
                        return builder.CreateICmpULT(L, R, "lttmp");
                    } else {
                        return builder.CreateICmpSLT(L, R, "lttmp");
                    }
                case frontend::TokenType::TOKEN_LESS_EQUAL:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    // String comparison: a <= b
                    if (isStringOp) {
                        llvm::Value* cmp = emitStringCompare();
                        return builder.CreateICmpSLE(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "str.le");
                    }
                    // ARIA-024: LBIM comparison for large integers
                    // L <= R is equivalent to !(R < L)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        llvm::Value* gt = generateLBIMSLT(R, L, numLimbs);
                        return builder.CreateNot(gt, "letmp");
                    }
                    // ARIA-025: fix256 less-equal comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee leFunc = module->getOrInsertFunction(
                            "aria_fix256_le", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.le_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.le_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(leFunc, {leftAlloca, rightAlloca}, "fix256.le");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpNE(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.lebool");
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpSLE(cmpResult, zero, "le_result");
                    }
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "le_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "le_fpext");
                        }
                        return builder.CreateFCmpOLE(L, R, "letmp");
                    } else if (isUnsigned) {
                        return builder.CreateICmpULE(L, R, "letmp");
                    } else {
                        return builder.CreateICmpSLE(L, R, "letmp");
                    }
                case frontend::TokenType::TOKEN_GREATER:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            // Broadcast right scalar to match left vector
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            // Broadcast left scalar to match right vector
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    // String comparison: a > b
                    if (isStringOp) {
                        llvm::Value* cmp = emitStringCompare();
                        return builder.CreateICmpSGT(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "str.gt");
                    }
                    // ARIA-024: LBIM comparison for large integers
                    // L > R is equivalent to R < L
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMSLT(R, L, numLimbs);
                    }
                    // ARIA-025: fix256 greater-than comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee gtFunc = module->getOrInsertFunction(
                            "aria_fix256_gt", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.gt_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.gt_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(gtFunc, {leftAlloca, rightAlloca}, "fix256.gt");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpNE(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.gtbool");
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpSGT(cmpResult, zero, "gt_result");
                    }
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "gt_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "gt_fpext");
                        }
                        return builder.CreateFCmpOGT(L, R, "gttmp");
                    } else if (isUnsigned) {
                        return builder.CreateICmpUGT(L, R, "gttmp");
                    } else {
                        return builder.CreateICmpSGT(L, R, "gttmp");
                    }
                case frontend::TokenType::TOKEN_GREATER_EQUAL:
                    // SIMD/Vector broadcast: Handle vector-scalar or scalar-vector comparisons
                    {
                        bool leftIsVector = L->getType()->isVectorTy();
                        bool rightIsVector = R->getType()->isVectorTy();
                        
                        if (leftIsVector && !rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(L->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, R, i);
                            }
                            R = vec;
                        } else if (!leftIsVector && rightIsVector) {
                            llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(R->getType());
                            llvm::Value* vec = llvm::UndefValue::get(vecType);
                            unsigned numElements = vecType->getElementCount().getKnownMinValue();
                            for (unsigned i = 0; i < numElements; ++i) {
                                vec = builder.CreateInsertElement(vec, L, i);
                            }
                            L = vec;
                        }
                    }
                    
                    // String comparison: a >= b
                    if (isStringOp) {
                        llvm::Value* cmp = emitStringCompare();
                        return builder.CreateICmpSGE(cmp, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "str.ge");
                    }
                    // ARIA-024: LBIM comparison for large integers
                    // L >= R is equivalent to !(L < R)
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        llvm::Value* lt = generateLBIMSLT(L, R, numLimbs);
                        return builder.CreateNot(lt, "getmp");
                    }
                    // ARIA-025: fix256 greater-equal comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee geFunc = module->getOrInsertFunction(
                            "aria_fix256_ge", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.ge_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.ge_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(geFunc, {leftAlloca, rightAlloca}, "fix256.ge");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateICmpNE(call,
                            llvm::ConstantInt::get(i32Type, 0), "fix256.gebool");
                    }
                    if (isNumericType && numericType) {
                        PrimitiveType* prim = static_cast<PrimitiveType*>(numericType);
                        std::string typeName = prim->getName();
                        std::string funcName = "aria_" + typeName + "_cmp";
                        
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* numericLLVMType = L->getType();
                        llvm::Type* ptrType = llvm::PointerType::get(numericLLVMType, 0);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(funcName, funcType);
                        
                        llvm::Value* leftAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_left");
                        llvm::Value* rightAlloca = builder.CreateAlloca(numericLLVMType, nullptr, "cmp_right");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        
                        llvm::Value* cmpResult = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "cmp");
                        llvm::Value* zero = llvm::ConstantInt::get(i32Type, 0);
                        return builder.CreateICmpSGE(cmpResult, zero, "ge_result");
                    }
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    // Use FCmp for floating point (scalar or SIMD vector), ICmp for integers
                    if (L->getType()->getScalarType()->isFloatingPointTy()) {
                        if (L->getType() != R->getType() && R->getType()->getScalarType()->isFloatingPointTy()) {
                            if (L->getType()->getScalarSizeInBits() > R->getType()->getScalarSizeInBits())
                                R = builder.CreateFPExt(R, L->getType(), "ge_fpext");
                            else
                                L = builder.CreateFPExt(L, R->getType(), "ge_fpext");
                        }
                        return builder.CreateFCmpOGE(L, R, "getmp");
                    } else if (isUnsigned) {
                        return builder.CreateICmpUGE(L, R, "getmp");
                    } else {
                        return builder.CreateICmpSGE(L, R, "getmp");
                    }
                
                // SPACESHIP OPERATOR (<=>)
                // Three-way comparison: returns -1 if left < right, 0 if equal, 1 if left > right
                case frontend::TokenType::TOKEN_SPACESHIP: {
                    // String spaceship: returns -1/0/1 directly from aria_string_compare_str
                    if (isStringOp) {
                        llvm::Value* cmp = emitStringCompare();
                        return builder.CreateSExt(cmp, llvm::Type::getInt64Ty(context), "str.spaceship");
                    }
                    // LBIM dispatch: use limb-by-limb comparison for large types
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        // spaceship = (L < R) ? -1 : ((L == R) ? 0 : 1)
                        llvm::Value* eq = generateLBIMEQ(L, R, numLimbs);
                        llvm::Value* lt = generateLBIMSLT(L, R, numLimbs);
                        llvm::Value* negOne = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), -1);
                        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
                        llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1);
                        llvm::Value* eqSel = builder.CreateSelect(eq, zero, one, "spaceship.eq_sel");
                        llvm::Value* result = builder.CreateSelect(lt, negOne, eqSel, "spaceship.result");
                        return result;
                    }
                    // ARIA-025: fix256 spaceship comparison
                    if (isFix256Type(L->getType())) {
                        llvm::Type* fix256Type = L->getType();
                        llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
                        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
                        llvm::FunctionType* funcType = llvm::FunctionType::get(
                            i32Type, {ptrType, ptrType}, false);
                        llvm::FunctionCallee cmpFunc = module->getOrInsertFunction(
                            "aria_fix256_cmp", funcType);
                        llvm::Value* leftAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.cmp_l");
                        llvm::Value* rightAlloca = builder.CreateAlloca(fix256Type, nullptr, "fix256.cmp_r");
                        builder.CreateStore(L, leftAlloca);
                        builder.CreateStore(R, rightAlloca);
                        auto* call = builder.CreateCall(cmpFunc, {leftAlloca, rightAlloca}, "fix256.cmp");
                        call->addParamAttr(0, llvm::Attribute::getWithByValType(context, fix256Type));
                        call->addParamAttr(1, llvm::Attribute::getWithByValType(context, fix256Type));
                        return builder.CreateSExt(call, llvm::Type::getInt64Ty(context), "fix256.spaceship");
                    }

                    // Ensure operands have same type
                    if (L->getType() != R->getType()) {
                        if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                            if (L->getType()->getIntegerBitWidth() > R->getType()->getIntegerBitWidth()) {
                                R = builder.CreateIntCast(R, L->getType(), true, "cmp_cast");
                            } else {
                                L = builder.CreateIntCast(L, R->getType(), true, "cmp_cast");
                            }
                        }
                    }
                    
                    // Generate: result = select(lt, -1, select(gt, 1, 0))
                    llvm::Value* ltCmp = builder.CreateICmpSLT(L, R, "spaceship.lt");
                    llvm::Value* gtCmp = builder.CreateICmpSGT(L, R, "spaceship.gt");
                    
                    llvm::Value* negOne = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), -1);
                    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
                    llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1);
                    
                    llvm::Value* gtResult = builder.CreateSelect(gtCmp, one, zero, "spaceship.gt_sel");
                    llvm::Value* result = builder.CreateSelect(ltCmp, negOne, gtResult, "spaceship.result");
                    
                    return result;
                }
                
                // Note: Logical operators (&&, ||) handled before switch via early return
                
                // Bitwise operators
                case frontend::TokenType::TOKEN_AMPERSAND:
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMAnd(L, R, numLimbs);
                    }
                    return builder.CreateAnd(L, R, "andtmp");
                case frontend::TokenType::TOKEN_PIPE:
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMOr(L, R, numLimbs);
                    }
                    return builder.CreateOr(L, R, "ortmp");
                case frontend::TokenType::TOKEN_CARET:
                    if (unsigned numLimbs = isLBIMType(L->getType())) {
                        return generateLBIMXor(L, R, numLimbs);
                    }
                    return builder.CreateXor(L, R, "xortmp");
                case frontend::TokenType::TOKEN_SHIFT_LEFT:
                    return builder.CreateShl(L, R, "shltmp");
                case frontend::TokenType::TOKEN_SHIFT_RIGHT:
                    // Bug #20 fix: use logical shift right (lshr).
                    // The type checker rejects >> on signed types, so any >>
                    // that reaches codegen has unsigned operands; fill with 0s.
                    return builder.CreateLShr(L, R, "lshrtmp");
                
                default:
                    return nullptr;
            }
            break;
        }
        
        case ASTNode::NodeType::TERNARY: {
            // Ternary expression: is condition : true_value : false_value
            // Generates: condition ? true_value : false_value
            // Uses PHI node to merge results from both branches
            TernaryExpr* ternary = static_cast<TernaryExpr*>(expr);
            
            llvm::Function* function = builder.GetInsertBlock()->getParent();
            
            // Create basic blocks for true branch, false branch, and merge
            llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "tern.true");
            llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "tern.false");
            llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "tern.cont");
            
            // Evaluate condition
            llvm::Value* condVal = codegenExpression(ternary->condition.get());
            if (!condVal) return nullptr;
            
            // Convert condition to boolean
            condVal = builder.CreateICmpNE(condVal, llvm::ConstantInt::get(condVal->getType(), 0), "terncond");
            
            // Branch based on condition
            builder.CreateCondBr(condVal, thenBB, elseBB);
            
            // Generate true branch
            function->insert(function->end(), thenBB);
            builder.SetInsertPoint(thenBB);
            llvm::Value* trueVal = codegenExpression(ternary->trueValue.get());
            if (!trueVal) return nullptr;
            builder.CreateBr(mergeBB);
            // Update thenBB in case the expression changed the current block
            thenBB = builder.GetInsertBlock();
            
            // Generate false branch
            function->insert(function->end(), elseBB);
            builder.SetInsertPoint(elseBB);
            llvm::Value* falseVal = codegenExpression(ternary->falseValue.get());
            if (!falseVal) return nullptr;
            builder.CreateBr(mergeBB);
            // Update elseBB in case the expression changed the current block
            elseBB = builder.GetInsertBlock();
            
            // Merge block with PHI node
            function->insert(function->end(), mergeBB);
            builder.SetInsertPoint(mergeBB);
            
            llvm::PHINode* phi = builder.CreatePHI(trueVal->getType(), 2, "ternphi");
            phi->addIncoming(trueVal, thenBB);
            phi->addIncoming(falseVal, elseBB);
            
            return phi;
        }
        
        case ASTNode::NodeType::UNWRAP: {
            // Two operators use UnwrapExpr:
            // ? operator: result ? default (unwraps Result<T>)
            // ?? operator: nullable ?? default (null coalescing for pointers)
            UnwrapExpr* unwrap = static_cast<UnwrapExpr*>(expr);
            
            // Generate code for the left-hand expression
            llvm::Value* leftVal = codegenExpression(unwrap->result.get());
            if (!leftVal) return nullptr;
            
            // Determine which operator based on the flag set during parsing
            if (!unwrap->isNullCoalesce) {
                // ? operator: Unwrap Result<T> or Optional<T>
                // Check if left side is an Optional type { i1, T } (2 fields)
                // vs Result type { T, ptr, i1 } (3 fields)
                bool isOptionalUnwrap = false;
                if (leftVal->getType()->isStructTy()) {
                    llvm::StructType* st = llvm::cast<llvm::StructType>(leftVal->getType());
                    // Optional: { i1, T } — 2 fields, first is i1
                    if (st->getNumElements() == 2 && st->getElementType(0)->isIntegerTy(1)) {
                        isOptionalUnwrap = true;
                    }
                }
                
                if (isOptionalUnwrap) {
                    // Optional<T>: { i1 hasValue, T value }
                    llvm::Value* hasValue = builder.CreateExtractValue(leftVal, 0, "opt.hasValue");
                    llvm::Value* isNone = builder.CreateNot(hasValue, "opt.isNone");
                    
                    llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                    llvm::BasicBlock* noneBlock = llvm::BasicBlock::Create(context, "opt_none", currentFunc);
                    llvm::BasicBlock* someBlock = llvm::BasicBlock::Create(context, "opt_some", currentFunc);
                    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(context, "opt_merge", currentFunc);
                    
                    builder.CreateCondBr(isNone, noneBlock, someBlock);
                    
                    // None block: use default value
                    builder.SetInsertPoint(noneBlock);
                    llvm::Value* defaultVal = codegenExpression(unwrap->defaultValue.get());
                    if (!defaultVal) return nullptr;
                    llvm::BasicBlock* noneExitBlock = builder.GetInsertBlock();
                    builder.CreateBr(mergeBlock);
                    
                    // Some block: extract value
                    builder.SetInsertPoint(someBlock);
                    llvm::Value* someVal = builder.CreateExtractValue(leftVal, 1, "opt.unwrap");
                    // Type-match: cast if needed (e.g., i32 vs i64 default literal)
                    if (someVal->getType() != defaultVal->getType()) {
                        if (someVal->getType()->isIntegerTy() && defaultVal->getType()->isIntegerTy()) {
                            unsigned someBits = someVal->getType()->getIntegerBitWidth();
                            unsigned defBits = defaultVal->getType()->getIntegerBitWidth();
                            if (someBits < defBits) {
                                someVal = builder.CreateSExt(someVal, defaultVal->getType(), "opt.sext");
                            } else {
                                // Truncate default to match someVal's type — rebuild in noneBlock
                                // Actually, widen someVal to match default is safer
                                someVal = builder.CreateSExt(someVal, defaultVal->getType(), "opt.sext");
                            }
                        }
                    }
                    llvm::BasicBlock* someExitBlock = builder.GetInsertBlock();
                    builder.CreateBr(mergeBlock);
                    
                    builder.SetInsertPoint(mergeBlock);
                    llvm::PHINode* phi = builder.CreatePHI(someVal->getType(), 2, "opt.result");
                    phi->addIncoming(defaultVal, noneExitBlock);
                    phi->addIncoming(someVal, someExitBlock);
                    
                    return phi;
                }
                
                // Result<T> struct: { value_type, ptr, i1 }
                // Field 0: value (T)
                // Field 1: error (ptr)
                // Field 2: is_error (i1)
                
                // Extract the is_error field (field 2)
                llvm::Value* isError = builder.CreateExtractValue(leftVal, 2, "is_error");
                
                // Create basic blocks for control flow
                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(context, "error_block", currentFunc);
                llvm::BasicBlock* successBlock = llvm::BasicBlock::Create(context, "success_block", currentFunc);
                llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(context, "merge_block", currentFunc);
                
                // Branch based on is_error
                builder.CreateCondBr(isError, errorBlock, successBlock);
                
                 // Error block: use default value
                builder.SetInsertPoint(errorBlock);
                llvm::Value* defaultVal = codegenExpression(unwrap->defaultValue.get());
                if (!defaultVal) return nullptr;
                llvm::BasicBlock* errorExitBlock = builder.GetInsertBlock();
                builder.CreateBr(mergeBlock);
                
                // Success block: extract value from Result
                builder.SetInsertPoint(successBlock);
                llvm::Value* valueField = builder.CreateExtractValue(leftVal, 0, "value");
                llvm::BasicBlock* successExitBlock = builder.GetInsertBlock();
                builder.CreateBr(mergeBlock);
                
                // Merge block: use PHI to select between default and value
                builder.SetInsertPoint(mergeBlock);
                llvm::PHINode* phi = builder.CreatePHI(valueField->getType(), 2, "unwrap_result");
                phi->addIncoming(defaultVal, errorExitBlock);
                phi->addIncoming(valueField, successExitBlock);
                
                return phi;
            } 
            else {
                // ?? operator: Null coalescing for Optional types and pointers
                
                llvm::Value* isNull;
                llvm::Value* unwrappedValue;
                llvm::Value* defaultVal = codegenExpression(unwrap->defaultValue.get());
                if (!defaultVal) return nullptr;
                
                if (leftVal->getType()->isStructTy()) {
                    // Optional type: { i1 hasValue, T value }
                    // Extract hasValue field (index 0)
                    llvm::Value* hasValue = builder.CreateExtractValue(leftVal, 0, "optional.hasValue");
                    isNull = builder.CreateNot(hasValue, "optional.isNone");
                    
                    // Extract value field (index 1) for use if has value
                    unwrappedValue = builder.CreateExtractValue(leftVal, 1, "optional.value");
                } else if (leftVal->getType()->isPointerTy()) {
                    // Pointer type: Check if pointer is null
                    llvm::Value* nullPtr = llvm::Constant::getNullValue(leftVal->getType());
                    isNull = builder.CreateICmpEQ(leftVal, nullPtr, "is_null");
                    
                    // Dereference pointer to get value (will happen in non-null block)
                    unwrappedValue = nullptr;  // Will be set in non-null block
                } else {
                    // Unsupported type for nil coalescing
                    throw std::runtime_error("Nil coalescing operator (\?\?) requires Optional or pointer type");
                }
                
                // Create basic blocks for control flow
                llvm::Function* currentFunc = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* nullBlock = llvm::BasicBlock::Create(context, "null_block", currentFunc);
                llvm::BasicBlock* nonNullBlock = llvm::BasicBlock::Create(context, "non_null_block", currentFunc);
                llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(context, "merge_block", currentFunc);
                
                // Branch based on is_null
                builder.CreateCondBr(isNull, nullBlock, nonNullBlock);
                
                // Null block: use default value
                builder.SetInsertPoint(nullBlock);
                llvm::BasicBlock* nullExitBlock = builder.GetInsertBlock();
                builder.CreateBr(mergeBlock);
                
                // Non-null block: use unwrapped value (or dereference pointer)
                builder.SetInsertPoint(nonNullBlock);
                if (leftVal->getType()->isPointerTy()) {
                    // For pointers, need to dereference
                    llvm::Type* pointeeType = defaultVal->getType();
                    unwrappedValue = builder.CreateLoad(pointeeType, leftVal, "deref_value");
                }
                // For Optional types, unwrappedValue was already extracted above
                llvm::BasicBlock* nonNullExitBlock = builder.GetInsertBlock();
                builder.CreateBr(mergeBlock);
                
                // Merge block: use PHI to select between default and unwrapped value
                builder.SetInsertPoint(mergeBlock);
                llvm::Type* resultType = defaultVal->getType();
                llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "coalesce_result");
                phi->addIncoming(defaultVal, nullExitBlock);
                phi->addIncoming(unwrappedValue, nonNullExitBlock);
                
                return phi;
            }
        }
        
        case ASTNode::NodeType::OBJECT_LITERAL: {
            // Object literal - struct instantiation: Point{ x: 10, y: 20 }
            // Special handling for numeric types that need runtime constructor calls
            ObjectLiteralExpr* objLit = static_cast<ObjectLiteralExpr*>(expr);
            
            // Check if this is one of our special numeric types that needs runtime constructor
            bool isFrac = objLit->type_name.find("frac") == 0;
            bool isTFP = objLit->type_name.find("tfp") == 0;
            bool isVec9 = objLit->type_name == "vec9";
            bool isTMatrix = objLit->type_name == "tmatrix";
            bool isTTensor = objLit->type_name == "ttensor";
            
            if (isFrac || isTFP || isVec9 || isTMatrix || isTTensor) {
                // These types use runtime constructor functions
                // Syntax: frac32{1, 1, 2} → frac32_from_parts(1, 1, 2)
                
                // Generate arguments (field values in order)
                std::vector<llvm::Value*> args;
                for (const auto& field : objLit->fields) {
                    llvm::Value* arg = codegenExpression(field.value.get());
                    if (!arg) {
                        return nullptr;  // Error in argument generation
                    }
                    args.push_back(arg);
                }
                
                // Get or declare the runtime constructor function
                std::string func_name = objLit->type_name + "_from_parts";
                llvm::Type* return_type = mapTypeFromName(objLit->type_name);
                
                // Build function type
                std::vector<llvm::Type*> param_types;
                for (llvm::Value* arg : args) {
                    param_types.push_back(arg->getType());
                }
                llvm::FunctionType* func_type = llvm::FunctionType::get(return_type, param_types, false);
                
                // Get or create function declaration
                llvm::Function* constructor_func = module->getFunction(func_name);
                if (!constructor_func) {
                    constructor_func = llvm::Function::Create(
                        func_type,
                        llvm::Function::ExternalLinkage,
                        func_name,
                        module.get()
                    );
                }
                
                // Call the constructor
                return builder.CreateCall(constructor_func, args, objLit->type_name + ".val");
            }
            
            // Regular struct initialization (non-numeric types)
            llvm::Type* struct_type = mapTypeFromName(objLit->type_name);
            if (!struct_type || !struct_type->isStructTy()) {
                // Error: not a struct type
                return nullptr;
            }
            
            // Allocate space for the struct on the stack
            llvm::AllocaInst* struct_alloca = builder.CreateAlloca(struct_type, nullptr, "struct.tmp");
            
            // Initialize each field
            // Fields are in objLit->fields (std::vector<Field>)
            for (size_t i = 0; i < objLit->fields.size(); ++i) {
                const ObjectLiteralExpr::Field& field = objLit->fields[i];
                
                // Generate the field value
                llvm::Value* field_value = codegenExpression(field.value.get());
                if (!field_value) {
                    continue;  // Skip this field on error
                }
                
                // For now, assume fields are in order (index i corresponds to struct field i)
                // TODO: Implement proper field name lookup to get correct indices
                
                // Create GEP to access the field
                llvm::Value* field_ptr = builder.CreateStructGEP(struct_type, struct_alloca, i, 
                                                                field.name + ".ptr");
                
                // Get the LLVM type of this struct field
                llvm::StructType* st = llvm::cast<llvm::StructType>(struct_type);
                llvm::Type* field_llvm_type = st->getElementType(i);
                
                // If the field is an array type but the value is a pointer (from ARRAY_LITERAL),
                // we need to copy the array contents element-by-element instead of storing a pointer.
                if (field_llvm_type->isArrayTy() && field_value->getType()->isPointerTy()) {
                    auto* arr_ty = llvm::cast<llvm::ArrayType>(field_llvm_type);
                    llvm::Type* elem_ty = arr_ty->getElementType();
                    uint64_t num_elems = arr_ty->getNumElements();
                    for (uint64_t ei = 0; ei < num_elems; ++ei) {
                        llvm::Value* src_ptr = builder.CreateGEP(
                            elem_ty, field_value,
                            llvm::ConstantInt::get(builder.getInt64Ty(), ei),
                            field.name + ".src." + std::to_string(ei));
                        llvm::Value* elem_val = builder.CreateLoad(elem_ty, src_ptr,
                            field.name + ".elem." + std::to_string(ei));
                        std::vector<llvm::Value*> gep_idx = {
                            llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                            llvm::ConstantInt::get(builder.getInt64Ty(), ei)
                        };
                        llvm::Value* dst_ptr = builder.CreateInBoundsGEP(
                            field_llvm_type, field_ptr, gep_idx,
                            field.name + ".dst." + std::to_string(ei));
                        builder.CreateStore(elem_val, dst_ptr);
                    }
                } else {
                    // Store the value directly (scalars, nested structs, etc.)
                    // Fix type mismatch: integer literals default to i64 but field may be i32
                    if (field_value->getType() != field_llvm_type) {
                        if (field_value->getType()->isIntegerTy() && field_llvm_type->isIntegerTy()) {
                            unsigned valBits = field_value->getType()->getIntegerBitWidth();
                            unsigned fldBits = field_llvm_type->getIntegerBitWidth();
                            if (valBits > fldBits) {
                                field_value = builder.CreateTrunc(field_value, field_llvm_type, field.name + ".trunc");
                            } else if (valBits < fldBits) {
                                field_value = builder.CreateSExt(field_value, field_llvm_type, field.name + ".sext");
                            }
                        } else if (field_value->getType()->isFloatingPointTy() && field_llvm_type->isFloatingPointTy()) {
                            field_value = builder.CreateFPCast(field_value, field_llvm_type, field.name + ".fpcast");
                        }
                    }
                    builder.CreateStore(field_value, field_ptr);
                }
            }
            
            // Load and return the complete struct value
            return builder.CreateLoad(struct_type, struct_alloca, "struct.val");
        }
        
        case ASTNode::NodeType::ARRAY_LITERAL: {
            // Array literal: [1, 2, 3, 4]
            ArrayLiteralExpr* arrLit = static_cast<ArrayLiteralExpr*>(expr);
            
            if (arrLit->elements.empty()) {
                // Empty array - return null pointer
                return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
            }
            
            // Determine element type from first element
            llvm::Value* first_elem = codegenExpression(arrLit->elements[0].get());
            if (!first_elem) {
                return nullptr;
            }
            llvm::Type* elem_type = first_elem->getType();
            
            // Allocate array on the stack
            size_t array_size = arrLit->elements.size();
            llvm::AllocaInst* array_alloca = builder.CreateAlloca(
                elem_type,
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), array_size),
                "array.tmp"
            );
            
            // Store first element
            llvm::Value* elem_ptr = builder.CreateGEP(elem_type, array_alloca,
                llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0));
            builder.CreateStore(first_elem, elem_ptr);
            
            // Store remaining elements
            for (size_t i = 1; i < arrLit->elements.size(); ++i) {
                llvm::Value* elem_value = codegenExpression(arrLit->elements[i].get());
                if (!elem_value) {
                    continue;  // Skip on error
                }
                
                // Create GEP to access element at index i
                elem_ptr = builder.CreateGEP(elem_type, array_alloca,
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), i));
                
                // Store the value
                builder.CreateStore(elem_value, elem_ptr);
            }
            
            // Return pointer to first element (arrays decay to pointers)
            return array_alloca;
        }
        
        case ASTNode::NodeType::POINTER_MEMBER: {
            // Pointer member access with arrow operator: ptr->member
            // This dereferences the pointer then accesses the member
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
            
            // Generate code for the pointer expression
            llvm::Value* ptr_val = codegenExpression(member->object.get());
            if (!ptr_val) {
                return nullptr;
            }
            
            // The pointer value should be an LLVM pointer - use it directly as object_ptr
            // (no need to load again since codegenExpression already gives us the pointer value)
            llvm::Value* object_ptr = ptr_val;
            
            // Look up the Aria type of the pointer
            auto type_it = value_types.find(ptr_val);
            if (type_it == value_types.end()) {
                return nullptr;
            }
            
            Type* pointer_aria_type = type_it->second;
            
            // Extract the pointee type
            if (pointer_aria_type->getKind() != TypeKind::POINTER) {
                return nullptr;  // Not a pointer type
            }
            
            sema::PointerType* ptr_type = static_cast<sema::PointerType*>(pointer_aria_type);
            Type* aria_type = ptr_type->getPointeeType();
            
            // Now handle the member access on the dereferenced pointer
            // (same logic as MEMBER_ACCESS case)
            
            // Handle struct member access
            if (aria_type->getKind() != TypeKind::STRUCT) {
                return nullptr;  // Not pointing to a struct
            }
            
            StructType* struct_type = static_cast<StructType*>(aria_type);
            
            // Find the field index
            int field_index = -1;
            Type* field_type = nullptr;
            const auto& fields = struct_type->getFields();
            for (size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].name == member->member) {
                    field_index = static_cast<int>(i);
                    field_type = fields[i].type;
                    break;
                }
            }
            
            if (field_index < 0) {
                return nullptr;  // Field not found
            }
            
            // Get the LLVM struct type
            llvm::Type* llvm_struct_type = mapType(struct_type);
            
            // Create GEP to access the field
            llvm::Value* field_ptr = builder.CreateStructGEP(
                llvm_struct_type, 
                object_ptr, 
                field_index, 
                member->member + ".ptr"
            );
            
            // Load the field value
            llvm::Type* llvm_field_type = mapType(field_type);
            return builder.CreateLoad(llvm_field_type, field_ptr, member->member);
        }
        
        case ASTNode::NodeType::MEMBER_ACCESS: {
            // Member access - enum variants, struct fields, vector components
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(expr);
            
            std::cerr << "[DEBUG MEMBER_ACCESS] Accessing member: " << member->member << std::endl;
            
            // Check if this is an enum variant access (EnumName.VARIANT)
            if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(member->object.get());
                std::string fullName = ident->name + "." + member->member;
                
                std::cerr << "[DEBUG MEMBER_ACCESS] Checking enum: " << fullName << std::endl;
                
                // Check if this is an enum variant (registered in enum_constants map)
                auto enum_it = enum_constants.find(fullName);
                if (enum_it != enum_constants.end()) {
                    // Return the constant integer value for this enum variant
                    std::cerr << "[DEBUG MEMBER_ACCESS] Found enum variant!" << std::endl;
                    return llvm::ConstantInt::get(builder.getInt64Ty(), enum_it->second);
                }
            }
            
            // Get the pointer to the object (NOT the loaded value)
            // Special handling for identifiers to get the alloca directly
            llvm::Value* object_ptr = nullptr;
            
            if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(member->object.get());
                auto it = named_values.find(ident->name);
                if (it != named_values.end()) {
                    object_ptr = it->second;  // Get the alloca directly, don't load
                    std::cerr << "[DEBUG MEMBER_ACCESS] Found identifier: " << ident->name << ", ptr type:  ";
                    object_ptr->getType()->print(llvm::errs());
                    std::cerr << std::endl;
                }
            } else {
                // For complex expressions, generate code normally
                object_ptr = codegenExpression(member->object.get());
            }
            
            if (!object_ptr) {
                std::cerr << "[DEBUG MEMBER_ACCESS] object_ptr is nullptr!" << std::endl;
                return nullptr;
            }
            
            // Look up the Aria type of this value
            auto type_it = value_types.find(object_ptr);
            if (type_it == value_types.end()) {
                // No type information available
                std::cerr << "[DEBUG MEMBER_ACCESS] No type info for object_ptr!" << std::endl;
                return nullptr;
            }
            
            std::cerr << "[DEBUG MEMBER_ACCESS] Found type info!" << std::endl;
            
            Type* aria_type = type_it->second;
            std::cerr << "[DEBUG MEMBER_ACCESS] Type kind: " << static_cast<int>(aria_type->getKind()) << std::endl;
            
            // Handle vector member access (.x, .y, .z)
            if (aria_type->getKind() == TypeKind::VECTOR) {
                VectorType* vec_type = static_cast<VectorType*>(aria_type);
                int dimension = vec_type->getDimension();
                
                // Map component name to index
                int component_index = -1;
                if (member->member == "x") component_index = 0;
                else if (member->member == "y") component_index = 1;
                else if (member->member == "z") component_index = 2;
                else if (member->member == "w") component_index = 3;
                
                if (component_index < 0 || component_index >= dimension) {
                    return nullptr;  // Invalid component for this vector
                }
                
                // Load the vector
                llvm::Type* vec_llvm_type = mapType(vec_type);
                llvm::Value* vec_val = builder.CreateLoad(vec_llvm_type, object_ptr, "vec");
                
                // Extract component
                if (dimension == 9) {
                    // vec9 is a struct - use ExtractValue
                    return builder.CreateExtractValue(vec_val, {static_cast<unsigned>(component_index)}, member->member);
                } else {
                    // vec2, vec3 are LLVM vectors - use ExtractElement
                    return builder.CreateExtractElement(vec_val, builder.getInt32(component_index), member->member);
                }
            }
            
            // PHASE 4: Handle Result<T> member access (.value, .error, .is_error)
            if (aria_type->getKind() == TypeKind::RESULT) {
                std::cerr << "[DEBUG MEMBER_ACCESS] Handling Result type member access" << std::endl;
                
                // Result<T> is represented as LLVM struct { T value, ptr error, i1 is_error }
                int field_index = -1;
                if (member->member == "value") {
                    field_index = 0;
                } else if (member->member == "error") {
                    field_index = 1;
                } else if (member->member == "is_error") {
                    field_index = 2;
                } else {
                    std::cerr << "[DEBUG MEMBER_ACCESS] Unknown Result member: " << member->member << std::endl;
                    return nullptr;
                }
                
                std::cerr << "[DEBUG MEMBER_ACCESS] Extracting Result field " << field_index << std::endl;
                
                // Result parameters come in as SSA values (not pointers)
                // Use ExtractValue to get the field
                if (object_ptr->getType()->isPointerTy()) {
                    // Result is stored in memory (e.g., local variable)
                    // Load it first, then extract
                    // Get the Result struct type from the type system
                    llvm::Type* result_llvm_type = mapType(aria_type);
                    llvm::Value* result_val = builder.CreateLoad(result_llvm_type, object_ptr, "result");
                    return builder.CreateExtractValue(result_val, field_index, member->member);
                } else {
                    // Result is an SSA value (e.g., function parameter)
                    // Extract directly
                    return builder.CreateExtractValue(object_ptr, field_index, member->member);
                }
            }
            
            // Handle optional type member access (safe navigation ?.)
            if (aria_type->getKind() == TypeKind::OPTIONAL) {
                OptionalType* opt_type = static_cast<OptionalType*>(aria_type);
                Type* inner_type = opt_type->getWrappedType();
                
                if (inner_type->getKind() != TypeKind::STRUCT) {
                    std::cerr << "[DEBUG MEMBER_ACCESS] Optional wraps non-struct, kind=" << static_cast<int>(inner_type->getKind()) << std::endl;
                    return nullptr;
                }
                
                StructType* inner_struct = static_cast<StructType*>(inner_type);
                
                // Find the field in the inner struct
                int opt_field_idx = -1;
                Type* opt_field_type = nullptr;
                const auto& opt_fields = inner_struct->getFields();
                for (size_t i = 0; i < opt_fields.size(); ++i) {
                    if (opt_fields[i].name == member->member) {
                        opt_field_idx = static_cast<int>(i);
                        opt_field_type = opt_fields[i].type;
                        break;
                    }
                }
                
                if (opt_field_idx < 0) {
                    std::cerr << "[DEBUG MEMBER_ACCESS] Field '" << member->member << "' not found in optional's inner struct" << std::endl;
                    return nullptr;
                }
                
                // Load the optional struct { i1, InnerStruct }
                llvm::Type* opt_llvm_type = mapType(aria_type);
                llvm::Value* opt_val = builder.CreateLoad(opt_llvm_type, object_ptr, "opt.load");
                
                // Extract hasValue flag (field 0)
                llvm::Value* has_value = builder.CreateExtractValue(opt_val, 0, "opt.hasValue");
                
                // Create control flow for safe navigation
                llvm::Function* func = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* accessBB = llvm::BasicBlock::Create(context, "safeNav.access", func);
                llvm::BasicBlock* nilBB = llvm::BasicBlock::Create(context, "safeNav.nil", func);
                llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "safeNav.merge", func);
                
                builder.CreateCondBr(has_value, accessBB, nilBB);
                
                // Access block: extract inner struct, get field, wrap as Optional<FieldType>
                builder.SetInsertPoint(accessBB);
                llvm::Value* inner_val = builder.CreateExtractValue(opt_val, 1, "opt.inner");
                llvm::Value* field_val = builder.CreateExtractValue(inner_val, opt_field_idx, member->member);
                
                // Wrap as Optional<FieldType>: { i1 true, FieldType value }
                llvm::Type* field_llvm_type = field_val->getType();
                llvm::Type* opt_result_type = llvm::StructType::get(context, {builder.getInt1Ty(), field_llvm_type});
                llvm::Value* some_result = llvm::UndefValue::get(opt_result_type);
                some_result = builder.CreateInsertValue(some_result, builder.getTrue(), 0, "opt.some.flag");
                some_result = builder.CreateInsertValue(some_result, field_val, 1, "opt.some.val");
                llvm::BasicBlock* accessEndBB = builder.GetInsertBlock();
                builder.CreateBr(mergeBB);
                
                // NIL block: return None { i1 false, FieldType undef }
                builder.SetInsertPoint(nilBB);
                llvm::Value* none_result = llvm::UndefValue::get(opt_result_type);
                none_result = builder.CreateInsertValue(none_result, builder.getFalse(), 0, "opt.none.flag");
                builder.CreateBr(mergeBB);
                
                // Merge with phi
                builder.SetInsertPoint(mergeBB);
                llvm::PHINode* phi = builder.CreatePHI(opt_result_type, 2, "safeNav.result");
                phi->addIncoming(some_result, accessEndBB);
                phi->addIncoming(none_result, nilBB);
                
                return phi;
            }
            
            // Handle struct member access
            if (aria_type->getKind() != TypeKind::STRUCT) {
                // Not a struct type
                std::cerr << "[DEBUG MEMBER_ACCESS] Not a struct or Result type, kind=" << static_cast<int>(aria_type->getKind()) << std::endl;
                return nullptr;
            }
            
            StructType* struct_type = static_cast<StructType*>(aria_type);
            
            // Find the field index
            int field_index = -1;
            Type* field_type = nullptr;
            const auto& fields = struct_type->getFields();
            for (size_t i = 0; i < fields.size(); ++i) {
                if (fields[i].name == member->member) {
                    field_index = static_cast<int>(i);
                    field_type = fields[i].type;
                    break;
                }
            }
            
            if (field_index < 0) {
                // Field not found
                return nullptr;
            }
            
            // Get the LLVM struct type
            llvm::Type* llvm_struct_type = mapType(struct_type);
            
            // Check if object_ptr is actually a pointer or an SSA value
            llvm::Value* field_value = nullptr;
            
            if (object_ptr->getType()->isPointerTy()) {
                // object_ptr is a pointer to the struct (e.g., local variable alloca)
                // Use GEP to get field address, then load it
                llvm::Value* field_ptr = builder.CreateStructGEP(
                    llvm_struct_type, 
                    object_ptr, 
                    field_index, 
                    member->member + ".ptr"
                );
                
                // Load the field value
                llvm::Type* llvm_field_type = mapType(field_type);
                field_value = builder.CreateLoad(llvm_field_type, field_ptr, member->member);
            } else {
                // object_ptr is an SSA value (the struct itself, e.g., function parameter)
                // Use ExtractValue to get the field directly
                field_value = builder.CreateExtractValue(object_ptr, field_index, member->member);
            }
            
            // Register the field value's type so binary ops can detect
            // exotic/ternary/TBB types and route to the correct intrinsics
            if (field_type && field_value) {
                value_types[field_value] = field_type;
            }
            
            return field_value;
        }
        
        case ASTNode::NodeType::INDEX: {
            // Array indexing: arr[i] or array slicing: arr[start..end]
            IndexExpr* indexExpr = static_cast<IndexExpr*>(expr);
            
            // Check if this is array slicing (index is a range expression)
            if (indexExpr->index->type == ASTNode::NodeType::RANGE) {
                // Array slicing: arr[start..end] or arr[start...end]
                RangeExpr* rangeExpr = static_cast<RangeExpr*>(indexExpr->index.get());
                
                // Generate code for the array (should be a pointer)
                llvm::Value* array_ptr = nullptr;
                
                // Get array pointer
                if (indexExpr->array->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* ident = static_cast<IdentifierExpr*>(indexExpr->array.get());
                    auto it = named_values.find(ident->name);
                    if (it != named_values.end()) {
                        llvm::Value* var = it->second;
                        if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                            llvm::Type* allocated_type = allocaInst->getAllocatedType();
                            if (allocated_type->isPointerTy()) {
                                array_ptr = builder.CreateLoad(allocated_type, var, ident->name + ".ptr");
                            } else {
                                array_ptr = var;
                            }
                        } else {
                            array_ptr = var;
                        }
                    }
                } else {
                    array_ptr = codegenExpression(indexExpr->array.get());
                }
                
                if (!array_ptr) {
                    return nullptr;
                }
                
                // Generate range start and end values
                llvm::Value* startVal = codegenExpression(rangeExpr->start.get());
                llvm::Value* endVal = codegenExpression(rangeExpr->end.get());
                
                if (!startVal || !endVal) {
                    return nullptr;
                }
                
                // Ensure start and end are int64 for arithmetic
                startVal = builder.CreateSExtOrTrunc(startVal, builder.getInt64Ty(), "start.i64");
                endVal = builder.CreateSExtOrTrunc(endVal, builder.getInt64Ty(), "end.i64");
                
                // Calculate slice length
                // For inclusive range (..): length = end - start + 1
                // For exclusive range (...): length = end - start
                llvm::Value* length = nullptr;
                if (rangeExpr->isExclusive) {
                    length = builder.CreateSub(endVal, startVal, "slice.len");
                } else {
                    llvm::Value* endPlusOne = builder.CreateAdd(endVal, 
                        llvm::ConstantInt::get(builder.getInt64Ty(), 1), "end.plus.one");
                    length = builder.CreateSub(endPlusOne, startVal, "slice.len");
                }
                
                // Get element type (assume int64 for now - should track this properly)
                llvm::Type* elem_type = builder.getInt64Ty();
                
                // Allocate memory for the new slice
                // Call malloc(length * sizeof(element))
                llvm::Value* elem_size = llvm::ConstantInt::get(builder.getInt64Ty(), 8); // sizeof(int64)
                llvm::Value* byte_count = builder.CreateMul(length, elem_size, "slice.bytes");
                
                llvm::Function* malloc_func = module->getFunction("malloc");
                if (!malloc_func) {
                    // Declare malloc if not already present
                    llvm::FunctionType* malloc_type = llvm::FunctionType::get(
                        builder.getPtrTy(),
                        {builder.getInt64Ty()},
                        false
                    );
                    malloc_func = llvm::Function::Create(
                        malloc_type,
                        llvm::Function::ExternalLinkage,
                        "malloc",
                        module.get()
                    );
                }
                
                llvm::Value* slice_ptr = builder.CreateCall(malloc_func, {byte_count}, "slice.ptr");
                
                // Copy elements from source array to slice
                // Create loop: for (i = 0; i < length; i++) slice[i] = arr[start + i]
                llvm::Function* function = builder.GetInsertBlock()->getParent();
                llvm::BasicBlock* loopCond = llvm::BasicBlock::Create(context, "slice.cond", function);
                llvm::BasicBlock* loopBody = llvm::BasicBlock::Create(context, "slice.body");
                llvm::BasicBlock* loopEnd = llvm::BasicBlock::Create(context, "slice.end");
                
                // Allocate loop counter
                llvm::Value* counter = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "slice.i");
                builder.CreateStore(llvm::ConstantInt::get(builder.getInt64Ty(), 0), counter);
                
                // Jump to loop condition
                builder.CreateBr(loopCond);
                
                // Loop condition: i < length
                builder.SetInsertPoint(loopCond);
                llvm::Value* i_val = builder.CreateLoad(builder.getInt64Ty(), counter, "i");
                llvm::Value* cmp = builder.CreateICmpSLT(i_val, length, "slice.cond");
                builder.CreateCondBr(cmp, loopBody, loopEnd);
                
                // Loop body: slice[i] = arr[start + i]
                function->insert(function->end(), loopBody);
                builder.SetInsertPoint(loopBody);
                llvm::Value* i_curr = builder.CreateLoad(builder.getInt64Ty(), counter, "i.curr");
                llvm::Value* src_idx = builder.CreateAdd(startVal, i_curr, "src.idx");
                
                // Load from source array
                llvm::Value* src_elem_ptr = builder.CreateGEP(elem_type, array_ptr, src_idx, "src.elem.ptr");
                llvm::Value* elem_val = builder.CreateLoad(elem_type, src_elem_ptr, "elem.val");
                
                // Store to slice
                llvm::Value* dst_elem_ptr = builder.CreateGEP(elem_type, slice_ptr, i_curr, "dst.elem.ptr");
                builder.CreateStore(elem_val, dst_elem_ptr);
                
                // Increment counter and loop back
                llvm::Value* i_next = builder.CreateAdd(i_curr, 
                    llvm::ConstantInt::get(builder.getInt64Ty(), 1), "i.next");
                builder.CreateStore(i_next, counter);
                builder.CreateBr(loopCond);
                
                // After loop
                function->insert(function->end(), loopEnd);
                builder.SetInsertPoint(loopEnd);
                
                // Return pointer to the slice
                return slice_ptr;
            }
            
            // Regular array or vector indexing: arr[i] or vec[i]
            // Generate code for the array/vector (should be a pointer)
            llvm::Value* array_ptr = nullptr;
            Type* object_type = nullptr;
            
            // Special handling for identifiers to get the alloca/pointer directly
            if (indexExpr->array->type == ASTNode::NodeType::IDENTIFIER) {
                IdentifierExpr* ident = static_cast<IdentifierExpr*>(indexExpr->array.get());
                auto it = named_values.find(ident->name);
                if (it != named_values.end()) {
                    llvm::Value* var = it->second;
                    
                    // Get the type information
                    auto type_it = value_types.find(var);
                    if (type_it != value_types.end()) {
                        object_type = type_it->second;
                    }
                    
                    // Check if it's a vector type
                    if (object_type && object_type->getKind() == TypeKind::VECTOR) {
                        // Vector indexing: vec[i]
                        VectorType* vec_type = static_cast<VectorType*>(object_type);
                        int dimension = vec_type->getDimension();
                        
                        // Generate code for the index
                        llvm::Value* index_value = codegenExpression(indexExpr->index.get());
                        if (!index_value) {
                            return nullptr;
                        }
                        
                        // Load the vector
                        llvm::Type* vec_llvm_type = mapType(vec_type);
                        llvm::Value* vec_val = builder.CreateLoad(vec_llvm_type, var, "vec");
                        
                        // Extract element
                        if (dimension == 9) {
                            // vec9 is a struct - need to use switch or GEP
                            // For dynamic indexing, use a series of selects
                            llvm::Type* component_type = mapType(vec_type->getComponentType());
                            llvm::Value* result = llvm::UndefValue::get(component_type);
                            
                            for (int i = 0; i < 9; ++i) {
                                llvm::Value* elem = builder.CreateExtractValue(vec_val, {static_cast<unsigned>(i)}, "vec9.elem");
                                llvm::Value* cmp = builder.CreateICmpEQ(index_value, builder.getInt32(i), "idx.cmp");
                                result = builder.CreateSelect(cmp, elem, result, "vec9.select");
                            }
                            return result;
                        } else {
                            // vec2, vec3 are LLVM vectors - use ExtractElement
                            return builder.CreateExtractElement(vec_val, index_value, "vec.elem");
                        }
                    }
                    
                    // Check if it's a SIMD type
                    if (object_type && object_type->getKind() == TypeKind::SIMD) {
                        // SIMD element access: simd<T, N>[i] -> T
                        SimdType* simd_type = static_cast<SimdType*>(object_type);
                        
                        // Generate code for the index
                        llvm::Value* index_value = codegenExpression(indexExpr->index.get());
                        if (!index_value) {
                            return nullptr;
                        }
                        
                        // Load the SIMD vector
                        llvm::Type* simd_llvm_type = mapType(simd_type);
                        llvm::Value* simd_val = builder.CreateLoad(simd_llvm_type, var, "simd");
                        
                        // Use LLVM's ExtractElement instruction
                        // simd<T, N> maps to <N x T>, so extractelement works directly
                        return builder.CreateExtractElement(simd_val, index_value, "simd.elem");
                    }
                    
                    // Check if it's a pointer type (arrays are stored as pointers)
                    if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                        llvm::Type* allocated_type = allocaInst->getAllocatedType();
                        if (allocated_type->isPointerTy()) {
                            // Load the pointer (array pointer stored in variable)
                            array_ptr = builder.CreateLoad(allocated_type, var, ident->name + ".ptr");
                        } else {
                            // Direct array allocation (e.g., array on stack)
                            array_ptr = var;
                        }
                    } else {
                        array_ptr = var;
                    }
                }
            } else {
                // For complex expressions, generate code normally.
                // Special case: multi-dimensional array read: mat[i][j] or deeper.
                // Walk the nested INDEX chain to find the root IDENTIFIER and collect all indices.
                {
                    std::vector<ASTNode*> nd_indices;
                    nd_indices.push_back(indexExpr->index.get()); // outermost index of this pair
                    ASTNode* nd_chain = indexExpr->array.get();
                    while (nd_chain->type == ASTNode::NodeType::INDEX) {
                        IndexExpr* inner_ix = static_cast<IndexExpr*>(nd_chain);
                        nd_indices.push_back(inner_ix->index.get());
                        nd_chain = inner_ix->array.get();
                    }
                    if (nd_chain->type == ASTNode::NodeType::IDENTIFIER) {
                        IdentifierExpr* root_ident = static_cast<IdentifierExpr*>(nd_chain);
                        auto nd_it = named_values.find(root_ident->name);
                        if (nd_it != named_values.end()) {
                            auto* nd_alloca = llvm::dyn_cast<llvm::AllocaInst>(nd_it->second);
                            if (nd_alloca) {
                                llvm::Type* nd_alloc_ty = nd_alloca->getAllocatedType();
                                if (nd_alloc_ty->isArrayTy()) {
                                    // nd_indices=[outerIdx,...innerIdx] (outermost pushed first).
                                    // GEP needs [0, outermost..innermost] — iterate in reverse.
                                    std::vector<llvm::Value*> gep_idx = {
                                        llvm::ConstantInt::get(builder.getInt64Ty(), 0)
                                    };
                                    for (int k = (int)nd_indices.size() - 1; k >= 0; --k) {
                                        llvm::Value* idx = codegenExpression(nd_indices[k]);
                                        if (!idx) return nullptr;
                                        if (!idx->getType()->isIntegerTy(64))
                                            idx = builder.CreateSExtOrTrunc(idx, builder.getInt64Ty(), "idx.i64");
                                        gep_idx.push_back(idx);
                                    }
                                    llvm::Type* nd_elem_ty = nd_alloc_ty;
                                    bool nd_valid = true;
                                    for (size_t k = 0; k < nd_indices.size(); ++k) {
                                        if (!nd_elem_ty->isArrayTy()) { nd_valid = false; break; }
                                        nd_elem_ty = llvm::cast<llvm::ArrayType>(nd_elem_ty)->getElementType();
                                    }
                                    if (nd_valid) {
                                        llvm::Value* nd_ptr = builder.CreateInBoundsGEP(
                                            nd_alloc_ty, nd_alloca, gep_idx, "arrnd.read.ptr");
                                        return builder.CreateLoad(nd_elem_ty, nd_ptr, "arrnd.read.val");
                                    }
                                }
                            }
                        }
                    }
                }
                // Struct field array read: obj.array_field[i]
                if (indexExpr->array->type == ASTNode::NodeType::MEMBER_ACCESS) {
                    MemberAccessExpr* memberExpr =
                        static_cast<MemberAccessExpr*>(indexExpr->array.get());
                    if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
                        IdentifierExpr* objIdent =
                            static_cast<IdentifierExpr*>(memberExpr->object.get());
                        auto obj_it = named_values.find(objIdent->name);
                        if (obj_it != named_values.end()) {
                            llvm::Value* structPtr = obj_it->second;
                            llvm::Type* sAllocTy = nullptr;
                            if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(structPtr))
                                sAllocTy = ai->getAllocatedType();
                            else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(structPtr))
                                sAllocTy = gv->getValueType();
                            if (sAllocTy && sAllocTy->isStructTy()) {
                                // Find field index via value_types / type_system
                                int fieldIdx = -1;
                                Type* ariaFieldType = nullptr;
                                auto type_it = value_types.find(obj_it->second);
                                if (type_it != value_types.end() &&
                                    type_it->second->getKind() == TypeKind::STRUCT) {
                                    StructType* sType =
                                        static_cast<StructType*>(type_it->second);
                                    const auto& flds = sType->getFields();
                                    for (size_t fi = 0; fi < flds.size(); ++fi) {
                                        if (flds[fi].name == memberExpr->member) {
                                            fieldIdx = static_cast<int>(fi);
                                            ariaFieldType = flds[fi].type;
                                            break;
                                        }
                                    }
                                }
                                if (fieldIdx < 0) {
                                    // Fallback: look up struct type by Aria type name
                                    auto var_it = var_aria_types.find(objIdent->name);
                                    if (var_it != var_aria_types.end()) {
                                        sema::StructType* semaST =
                                            type_system->getStructType(var_it->second);
                                        if (semaST) {
                                            fieldIdx = semaST->getFieldIndex(memberExpr->member);
                                            const sema::StructType::Field* semField =
                                                semaST->getField(memberExpr->member);
                                            if (semField)
                                                ariaFieldType = semField->type;
                                        }
                                    }
                                }
                                if (fieldIdx >= 0) {
                                    llvm::StructType* llvmSTy =
                                        llvm::cast<llvm::StructType>(sAllocTy);
                                    llvm::Type* fieldTy =
                                        llvmSTy->getElementType(fieldIdx);
                                    if (fieldTy->isArrayTy()) {
                                        auto* arrTy =
                                            llvm::cast<llvm::ArrayType>(fieldTy);
                                        llvm::Type* elemTy = arrTy->getElementType();
                                        llvm::Value* fieldPtr =
                                            builder.CreateStructGEP(
                                                sAllocTy, structPtr, fieldIdx,
                                                memberExpr->member + ".field.ptr");
                                        llvm::Value* idx_v =
                                            codegenExpression(indexExpr->index.get());
                                        if (!idx_v) return nullptr;
                                        if (!idx_v->getType()->isIntegerTy(64))
                                            idx_v = builder.CreateSExtOrTrunc(
                                                idx_v, builder.getInt64Ty(), "idx.i64");
                                        std::vector<llvm::Value*> gepIdx = {
                                            llvm::ConstantInt::get(
                                                builder.getInt64Ty(), 0),
                                            idx_v};
                                        llvm::Value* elemPtr =
                                            builder.CreateInBoundsGEP(
                                                fieldTy, fieldPtr, gepIdx,
                                                memberExpr->member + ".elem.ptr");
                                        llvm::Value* loadedElem = builder.CreateLoad(
                                            elemTy, elemPtr,
                                            memberExpr->member + ".elem");

                                        // Register Aria element type so subsequent
                                        // member access (e.g. cloud.pts[0].x) can
                                        // find type info for the loaded struct element.
                                        if (ariaFieldType &&
                                            ariaFieldType->getKind() == TypeKind::ARRAY) {
                                            Type* elemAriaType =
                                                static_cast<sema::ArrayType*>(ariaFieldType)
                                                    ->getElementType();
                                            if (elemAriaType)
                                                value_types[loadedElem] = elemAriaType;
                                        }

                                        return loadedElem;
                                    }
                                }
                            }
                        }
                    }
                }
                array_ptr = codegenExpression(indexExpr->array.get());
            }
            
            if (!array_ptr) {
                return nullptr;
            }
            
            // Generate code for the index
            llvm::Value* index_value = codegenExpression(indexExpr->index.get());
            if (!index_value) {
                return nullptr;
            }
            
            // Get element type (arrays decay to pointers)
            llvm::Type* elem_type = nullptr;
            if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(array_ptr)) {
                llvm::Type* alloc_ty = alloca->getAllocatedType();
                if (alloc_ty->isArrayTy()) {
                    // Properly-typed fixed-size array [N x T] — use two-index GEP
                    auto* arrTy = llvm::cast<llvm::ArrayType>(alloc_ty);
                    elem_type = arrTy->getElementType();
                    // Cast index to i64 for GEP
                    if (!index_value->getType()->isIntegerTy(64)) {
                        index_value = builder.CreateSExtOrTrunc(index_value,
                                          builder.getInt64Ty(), "idx.i64");
                    }
                    std::vector<llvm::Value*> gep_indices = {
                        llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                        index_value
                    };
                    llvm::Value* elem_ptr = builder.CreateInBoundsGEP(
                        alloc_ty, array_ptr, gep_indices, "arrayidx");
                    return builder.CreateLoad(elem_type, elem_ptr, "elem");
                } else {
                    elem_type = alloc_ty;
                }
            } else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(array_ptr)) {
                // Global variable array: use the GlobalVariable's value type for proper element type
                llvm::Type* gvType = gv->getValueType();
                if (gvType->isArrayTy()) {
                    auto* arrTy = llvm::cast<llvm::ArrayType>(gvType);
                    elem_type = arrTy->getElementType();
                    if (!index_value->getType()->isIntegerTy(64)) {
                        index_value = builder.CreateSExtOrTrunc(index_value,
                                          builder.getInt64Ty(), "idx.i64");
                    }
                    std::vector<llvm::Value*> gep_indices = {
                        llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                        index_value
                    };
                    llvm::Value* elem_ptr = builder.CreateInBoundsGEP(
                        gvType, array_ptr, gep_indices, "arrayidx");
                    return builder.CreateLoad(elem_type, elem_ptr, "elem");
                }
                elem_type = gvType;
            } else if (auto* ptr_type = llvm::dyn_cast<llvm::PointerType>(array_ptr->getType())) {
                (void)ptr_type;
                // Look up element type from var_aria_types, same as ExprCodegen::codegenIndex.
                // For a wild T-> pointer, var_aria_types stores "T@"; strip "@" to get T.
                // Without this lookup both systems used i64 regardless of declared element type,
                // causing 8-byte stride reads from byte arrays (e.g. wild int8-> buffers).
                elem_type = llvm::Type::getInt64Ty(context);  // fallback
                if (indexExpr->array->type == ASTNode::NodeType::IDENTIFIER) {
                    auto* ident = static_cast<IdentifierExpr*>(indexExpr->array.get());
                    auto at = var_aria_types.find(ident->name);
                    if (at != var_aria_types.end()) {
                        std::string t = at->second;
                        if (!t.empty() && t.back() == '@') t.pop_back();
                        if      (t == "int8"  || t == "i8")  elem_type = builder.getInt8Ty();
                        else if (t == "int16" || t == "i16") elem_type = builder.getInt16Ty();
                        else if (t == "int32" || t == "i32") elem_type = builder.getInt32Ty();
                        else if (t == "int64" || t == "i64") elem_type = builder.getInt64Ty();
                        else if (t == "flt32" || t == "f32" || t == "float32") elem_type = builder.getFloatTy();
                        else if (t == "flt64" || t == "f64" || t == "float64") elem_type = builder.getDoubleTy();
                    }
                }
            } else {
                // Fallback to int32
                elem_type = llvm::Type::getInt32Ty(context);
            }
            
            // Create GEP to access element at index (flat pointer / non-array alloca)
            llvm::Value* elem_ptr = builder.CreateGEP(elem_type, array_ptr, index_value, "arrayidx");
            
            // Load and return the element value
            return builder.CreateLoad(elem_type, elem_ptr, "elem");
        }
        
        case ASTNode::NodeType::TEMPLATE_LITERAL: {
            // Template literal with interpolation: `Hello &{name}!`
            // Delegate to ExprCodegen::codegenTemplateLiteral for detailed implementation
            TemplateLiteralExpr* templateLit = static_cast<TemplateLiteralExpr*>(expr);

            // Create ExprCodegen instance
            backend::ExprCodegen expr_codegen(context, builder, module.get(), named_values, var_aria_types, type_system);

            // Generate template literal IR
            return expr_codegen.codegenTemplateLiteral(templateLit);
        }
        
        case ASTNode::NodeType::VECTOR_CONSTRUCTOR: {
            // Vector construction: vec2(x, y), vec3(x, y, z), vec9(...)
            VectorConstructorExpr* vecExpr = static_cast<VectorConstructorExpr*>(expr);
            
            int dimension = vecExpr->dimension;
            if (vecExpr->components.size() != static_cast<size_t>(dimension)) {
                return nullptr;  // Component count mismatch
            }
            
            // Generate code for all components
            std::vector<llvm::Value*> component_values;
            llvm::Type* component_type = nullptr;
            
            for (const auto& comp : vecExpr->components) {
                llvm::Value* comp_val = codegenExpression(comp.get());
                if (!comp_val) return nullptr;
                
                component_values.push_back(comp_val);
                
                // All components should have the same type
                if (!component_type) {
                    component_type = comp_val->getType();
                }
            }
            
            // Create the vector
            if (dimension == 9) {
                // vec9 is represented as a struct with 9 components
                std::vector<llvm::Type*> struct_types(9, component_type);
                llvm::StructType* vec9_type = llvm::StructType::get(context, struct_types);
                
                // Create undef struct and insert each component
                llvm::Value* result = llvm::UndefValue::get(vec9_type);
                for (int i = 0; i < 9; ++i) {
                    result = builder.CreateInsertValue(result, component_values[i], {static_cast<unsigned>(i)}, "vec9.insert");
                }
                return result;
            } else {
                // vec2, vec3 use LLVM fixed vectors
                llvm::Type* vec_type = llvm::FixedVectorType::get(component_type, dimension);
                
                // Create undef vector and insert each component
                llvm::Value* result = llvm::UndefValue::get(vec_type);
                for (int i = 0; i < dimension; ++i) {
                    result = builder.CreateInsertElement(result, component_values[i], builder.getInt32(i), "vec.insert");
                }
                return result;
            }
        }
        
        case ASTNode::NodeType::AWAIT: {
            // Delegate to ExprCodegen's promise-based await implementation
            backend::ExprCodegen expr_codegen(context, builder, module.get(), named_values, var_aria_types, type_system);
            return expr_codegen.codegenAwait(expr);
        }
        
        case ASTNode::NodeType::MOVE: {
            // move(x) — transfer ownership by loading the pointer value from the source
            // variable and nullifying its alloca.  The borrow checker already guarantees
            // that the source is not used again after the move at compile time.
            MoveExpr* moveExpr = static_cast<MoveExpr*>(expr);

            auto srcIt = named_values.find(moveExpr->variableName);
            if (srcIt == named_values.end()) {
                std::cerr << "[ERROR] move(): source variable '"
                          << moveExpr->variableName << "' not found in scope" << std::endl;
                return nullptr;
            }

            llvm::Value* srcAlloca = srcIt->second;

            // Determine the type stored in the source variable's alloca.
            llvm::Type* loadType = builder.getPtrTy();  // default: opaque pointer
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(srcAlloca)) {
                loadType = allocaInst->getAllocatedType();
            }

            // Load the current value from the source variable.
            llvm::Value* srcVal = builder.CreateLoad(loadType, srcAlloca, "move_src");

            // Nullify source alloca (defensive: prevents use of stale pointer even if
            // the borrow checker somehow allowed it through).
            if (loadType->isPointerTy()) {
                builder.CreateStore(
                    llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
                    srcAlloca
                );
            }

            // Propagate value_types metadata for the moved value (needed for UFCS / TBB dispatch).
            auto valTypeIt = value_types.find(srcAlloca);
            if (valTypeIt != value_types.end()) {
                value_types[srcVal] = valTypeIt->second;
            }

            std::cerr << "[DEBUG] move(): transferred ownership of '"
                      << moveExpr->variableName << "', nullified source" << std::endl;
            return srcVal;
        }

        case ASTNode::NodeType::CAST: {
            // @cast<TargetType>(expr) and @cast_unchecked<TargetType>(expr)
            // Previously missing from this switch → returned nullptr → variable stayed uninit.
            CastExpr* castExpr = static_cast<CastExpr*>(expr);

            llvm::Value* sourceValue = codegenExpression(castExpr->expression.get());
            if (!sourceValue) return nullptr;

            // mapTypeFromName understands full Aria type names like "int64", "uint32"
            llvm::Type* targetType = mapTypeFromName(castExpr->targetType);
            if (!targetType) return nullptr;

            llvm::Type* sourceType = sourceValue->getType();

            // Trivial: same type already
            if (sourceType == targetType) return sourceValue;

            bool srcIsInt   = sourceType->isIntegerTy();
            bool tgtIsInt   = targetType->isIntegerTy();
            bool srcIsFloat = sourceType->isFloatingPointTy();
            bool tgtIsFloat = targetType->isFloatingPointTy();

            // Integer → Integer
            if (srcIsInt && tgtIsInt) {
                unsigned srcBits = sourceType->getIntegerBitWidth();
                unsigned tgtBits = targetType->getIntegerBitWidth();
                if (tgtBits > srcBits) {
                    // Widening: signed types use sext, unsigned use zext
                    const std::string& tn = castExpr->targetType;
                    bool isSigned = !tn.empty() &&
                                    (tn[0] == 'i' ||
                                     tn.substr(0, 3) == "int" ||
                                     tn.substr(0, 3) == "tbb");
                    return isSigned
                        ? builder.CreateSExt(sourceValue, targetType, "cast.sext")
                        : builder.CreateZExt(sourceValue, targetType, "cast.zext");
                } else {
                    return builder.CreateTrunc(sourceValue, targetType, "cast.trunc");
                }
            }

            // Float → Float
            if (srcIsFloat && tgtIsFloat) {
                unsigned srcBits = sourceType->getPrimitiveSizeInBits();
                unsigned tgtBits = targetType->getPrimitiveSizeInBits();
                return tgtBits > srcBits
                    ? builder.CreateFPExt(sourceValue, targetType, "cast.fpext")
                    : builder.CreateFPTrunc(sourceValue, targetType, "cast.fptrunc");
            }

            // Integer → Float
            if (srcIsInt && tgtIsFloat)
                return builder.CreateSIToFP(sourceValue, targetType, "cast.sitofp");

            // Float → Integer
            if (srcIsFloat && tgtIsInt)
                return builder.CreateFPToSI(sourceValue, targetType, "cast.fptosi");

            // Pointer → Pointer (no-op in opaque pointer model)
            if (sourceType->isPointerTy() && targetType->isPointerTy())
                return sourceValue;

            std::cerr << "[WARN] Unsupported cast in codegenExpression, returning nullptr" << std::endl;
            return nullptr;
        }

        case ASTNode::NodeType::COMPTIME_EXPR: {
            // Comptime expression: emit the pre-computed constant
            ComptimeExpr* comptimeExpr = static_cast<ComptimeExpr*>(expr);
            
            if (!comptimeExpr->evaluated) {
                std::cerr << "[ERROR] comptime expression was not evaluated during type checking" << std::endl;
                return nullptr;
            }
            
            const std::string& typeName = comptimeExpr->resultTypeName;
            
            if (typeName == "bool") {
                return builder.getInt1(comptimeExpr->boolResult);
            } else if (typeName == "str") {
                return builder.CreateGlobalString(comptimeExpr->stringResult, "comptime.str");
            } else if (typeName.find("flt") == 0 || typeName.find("f") == 0) {
                // Float types
                llvm::Type* llvmType = mapTypeFromName(typeName);
                if (!llvmType) llvmType = builder.getDoubleTy();
                return llvm::ConstantFP::get(llvmType, comptimeExpr->floatResult);
            } else {
                // Integer types (int*, uint*, i*, u*, tbb*)
                llvm::Type* llvmType = mapTypeFromName(typeName);
                if (!llvmType) llvmType = builder.getInt64Ty();
                return llvm::ConstantInt::get(llvmType, comptimeExpr->intResult, /*isSigned=*/true);
            }
        }

        default:
            // Other expression types not yet implemented
            return nullptr;
    }
}

// =============================================================================
// Debug Info Generation (Phase 7.4.1)
// =============================================================================

void aria::IRGenerator::initDebugInfo(const std::string& filename, const std::string& directory) {
    if (!debug_enabled || !di_builder) {
        return;
    }
    
    // Create the compile unit
    di_file = di_builder->createFile(filename, directory);
    
    di_compile_unit = di_builder->createCompileUnit(
        llvm::dwarf::DW_LANG_C,  // Use C for now (could register DW_LANG_Aria later)
        di_file,
        "Aria Compiler v0.2.12",  // Producer
        false,                    // isOptimized
        "",                       // Flags
        0                         // Runtime version
    );
    
    // Set compile unit as root scope
    di_scope_stack.push_back(di_compile_unit);
}

void aria::IRGenerator::finalizeDebugInfo() {
    if (!debug_enabled || !di_builder) {
        return;
    }
    
    // Finalize the DIBuilder (writes all pending debug info)
    di_builder->finalize();
}

void aria::IRGenerator::setDebugLocation(unsigned line, unsigned column) {
    if (!debug_enabled || di_scope_stack.empty()) {
        return;
    }
    
    llvm::DIScope* scope = getCurrentDebugScope();
    llvm::DILocation* loc = llvm::DILocation::get(
        context,
        line,
        column,
        scope
    );
    
    builder.SetCurrentDebugLocation(llvm::DebugLoc(loc));
}

void aria::IRGenerator::clearDebugLocation() {
    if (!debug_enabled) {
        return;
    }
    
    builder.SetCurrentDebugLocation(llvm::DebugLoc());
}

void aria::IRGenerator::pushDebugScope(llvm::DIScope* scope) {
    if (!debug_enabled) {
        return;
    }
    
    di_scope_stack.push_back(scope);
}

void aria::IRGenerator::popDebugScope() {
    if (!debug_enabled || di_scope_stack.size() <= 1) {
        return;  // Never pop the compile unit
    }
    
    di_scope_stack.pop_back();
}

llvm::DIScope* aria::IRGenerator::getCurrentDebugScope() {
    if (!debug_enabled || di_scope_stack.empty()) {
        return nullptr;
    }
    
    return di_scope_stack.back();
}

llvm::DIType* aria::IRGenerator::mapDebugType(Type* aria_type) {
    if (!debug_enabled || !aria_type) {
        return nullptr;
    }
    
    // Check cache first
    std::string type_name = aria_type->toString();
    auto it = di_type_map.find(type_name);
    if (it != di_type_map.end()) {
        return it->second;
    }
    
    llvm::DIType* di_type = nullptr;
    
    switch (aria_type->getKind()) {
        case TypeKind::PRIMITIVE: {
            auto* prim = static_cast<PrimitiveType*>(aria_type);
            std::string name = prim->getName();
            unsigned bit_width = prim->getBitWidth();
            
            // Check if this is a TBB type
            if (name.rfind("tbb", 0) == 0) {
                // TBB types: Create typedef over signed integer
                // This allows LLDB formatters to recognize the type name
                llvm::DIType* base_int = di_builder->createBasicType(
                    "int" + std::to_string(bit_width),
                    bit_width,
                    llvm::dwarf::DW_ATE_signed
                );
                
                di_type = di_builder->createTypedef(
                    base_int,
                    name,        // Type name (e.g., "tbb32")
                    di_file,
                    0,           // Line number (0 for built-in types)
                    getCurrentDebugScope()
                );
            }
            // Boolean
            else if (name == "bool") {
                di_type = di_builder->createBasicType(
                    "bool",
                    1,
                    llvm::dwarf::DW_ATE_boolean
                );
            }
            // Floating point
            else if (prim->isFloatingType()) {
                di_type = di_builder->createBasicType(
                    name,
                    bit_width,
                    llvm::dwarf::DW_ATE_float
                );
            }
            // Regular integers
            else if (prim->isSignedType()) {
                di_type = di_builder->createBasicType(
                    name,
                    bit_width,
                    llvm::dwarf::DW_ATE_signed
                );
            }
            else {
                di_type = di_builder->createBasicType(
                    name,
                    bit_width,
                    llvm::dwarf::DW_ATE_unsigned
                );
            }
            break;
        }
        
        case TypeKind::POINTER: {
            auto* ptr_type = static_cast<sema::PointerType*>(aria_type);
            llvm::DIType* pointee = mapDebugType(ptr_type->getPointeeType());
            
            // Check for memory qualifier (gc vs wild)
            std::string qualifier = ptr_type->isWildPointer() ? "wild" : "gc";
            std::string ptr_name = qualifier + "_ptr";
            
            di_type = di_builder->createPointerType(
                pointee,
                64,  // Pointer size (assume 64-bit)
                0,   // Alignment
                std::nullopt,
                ptr_name
            );
            break;
        }
        
        case TypeKind::ARRAY: {
            auto* arr_type = static_cast<sema::ArrayType*>(aria_type);
            llvm::DIType* elem_type = mapDebugType(arr_type->getElementType());
            
            if (arr_type->getSize() > 0) {
                // Fixed-size array
                llvm::SmallVector<llvm::Metadata*, 1> subscripts;
                subscripts.push_back(di_builder->getOrCreateSubrange(0, arr_type->getSize()));
                
                di_type = di_builder->createArrayType(
                    arr_type->getSize() * 64,  // Size in bits (assume 64-bit elements for now)
                    0,                          // Alignment
                    elem_type,
                    di_builder->getOrCreateArray(subscripts)
                );
            } else {
                // Dynamic array (pointer)
                di_type = di_builder->createPointerType(elem_type, 64, 0, std::nullopt, "array");
            }
            break;
        }
        
        default:
            // For other types, create an unspecified type placeholder
            di_type = di_builder->createUnspecifiedType(type_name);
            break;
    }
    
    // Cache the result
    if (di_type) {
        di_type_map[type_name] = di_type;
    }
    
    return di_type;
}

// =============================================================================
// Defer Statement Support
// =============================================================================

void aria::IRGenerator::executeScopeDefers() {
    if (defer_stack.empty()) {
        return;
    }

    // ARIA-023: Set flag to prevent defer_stack modification during iteration
    bool was_executing = executing_defers;
    executing_defers = true;

    // Get the current scope's defer blocks
    std::vector<BlockStmt*>& current_scope_defers = defer_stack.back();

    // Execute in LIFO order (reverse)
    for (auto it = current_scope_defers.rbegin(); it != current_scope_defers.rend(); ++it) {
        BlockStmt* defer_block = *it;
        for (const auto& statement : defer_block->statements) {
            codegenStatement(statement.get());
        }
    }

    executing_defers = was_executing;
}

void aria::IRGenerator::executeFunctionDefers() {
    // ARIA-023: Set flag to prevent defer_stack modification during iteration
    bool was_executing = executing_defers;
    executing_defers = true;

    // Execute all scopes' defers in reverse order (inside-out)
    for (auto scope_it = defer_stack.rbegin(); scope_it != defer_stack.rend(); ++scope_it) {
        // Within each scope, execute defers in LIFO order
        for (auto defer_it = scope_it->rbegin(); defer_it != scope_it->rend(); ++defer_it) {
            BlockStmt* defer_block = *defer_it;
            for (const auto& statement : defer_block->statements) {
                codegenStatement(statement.get());
            }
        }
    }

    executing_defers = was_executing;
}

// =============================================================================
// PHASE 4: Zero Implicit Conversion - Type Suffix Helpers
// =============================================================================

llvm::Type* aria::IRGenerator::getLLVMTypeFromSuffix(const std::string& suffix) {
    // Unsigned integers
    if (suffix == "u8") return builder.getInt8Ty();
    if (suffix == "u16") return builder.getInt16Ty();
    if (suffix == "u32") return builder.getInt32Ty();
    if (suffix == "u64") return builder.getInt64Ty();
    if (suffix == "u128") return builder.getInt128Ty();
    
    // Signed integers
    if (suffix == "i8") return builder.getInt8Ty();
    if (suffix == "i16") return builder.getInt16Ty();
    if (suffix == "i32") return builder.getInt32Ty();
    if (suffix == "i64") return builder.getInt64Ty();
    if (suffix == "i128") return builder.getInt128Ty();
    
    // Floats
    if (suffix == "f32") return builder.getFloatTy();
    if (suffix == "f64") return builder.getDoubleTy();
    if (suffix == "f128") return llvm::Type::getFP128Ty(context);
    
    // TBB types (symmetric range with error sentinel)
    if (suffix == "tbb8") return builder.getInt8Ty();
    if (suffix == "tbb16") return builder.getInt16Ty();
    if (suffix == "tbb32") return builder.getInt32Ty();
    if (suffix == "tbb64") return builder.getInt64Ty();
    
    // Large integers (LBIM - struct-based to avoid LLVM bugs)
    if (suffix == "u256" || suffix == "i256") {
        // { i64, i64, i64, i64 } - 4 limbs
        std::vector<llvm::Type*> limbs = {
            builder.getInt64Ty(), builder.getInt64Ty(),
            builder.getInt64Ty(), builder.getInt64Ty()
        };
        return llvm::StructType::get(context, limbs);
    }
    if (suffix == "u512" || suffix == "i512") {
        // { i64, ... } - 8 limbs
        std::vector<llvm::Type*> limbs(8, builder.getInt64Ty());
        return llvm::StructType::get(context, limbs);
    }
    if (suffix == "u1024" || suffix == "i1024") {
        // { i64, ... } - 16 limbs
        std::vector<llvm::Type*> limbs(16, builder.getInt64Ty());
        return llvm::StructType::get(context, limbs);
    }
    if (suffix == "u2048" || suffix == "i2048") {
        // { i64, ... } - 32 limbs
        std::vector<llvm::Type*> limbs(32, builder.getInt64Ty());
        return llvm::StructType::get(context, limbs);
    }
    if (suffix == "u4096" || suffix == "i4096") {
        // { i64, ... } - 64 limbs
        std::vector<llvm::Type*> limbs(64, builder.getInt64Ty());
        return llvm::StructType::get(context, limbs);
    }
    
    // Fixed-point (deterministic physics)
    if (suffix == "fix256") {
        // Q128.128 - { i64[4] } for 256-bit fixed-point
        std::vector<llvm::Type*> limbs(4, builder.getInt64Ty());
        return llvm::StructType::get(context, limbs);
    }
    
    // Unknown suffix
    return nullptr;
}

bool aria::IRGenerator::isSuffixSigned(const std::string& suffix) {
    // Signed prefixes
    if (suffix[0] == 'i') return true;  // i8, i16, i32, i64, i128, i256, ...
    if (suffix.substr(0, 3) == "tbb") return true;  // tbb8, tbb16, tbb32, tbb64
    if (suffix.substr(0, 3) == "fix") return true;  // fix256
    
    // Unsigned prefixes
    if (suffix[0] == 'u') return false;  // u8, u16, u32, ...
    if (suffix[0] == 'f') return false;  // f32, f64, f128 (not signed/unsigned, but return false)
    
    // Default to signed for unknown
    return true;
}
