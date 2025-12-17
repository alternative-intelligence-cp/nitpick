#include "backend/ir/ir_generator.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"  // Full type definitions needed
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DerivedTypes.h>  // For FunctionType, StructType, etc.
#include <llvm/IR/DataLayout.h>     // For getTypeAllocSize
#include <cassert>

namespace aria {
using namespace sema;  // Now inside aria namespace

IRGenerator::IRGenerator(const std::string& module_name)
    : context(), 
      module(std::make_unique<llvm::Module>(module_name, context)),
      builder(context) {
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
            
            // Boolean type
            if (prim->getName() == "bool") {
                llvm_type = builder.getInt1Ty();
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
            // Integer and TBB types (TBB uses same representation as int)
            else {
                llvm_type = builder.getIntNTy(prim->getBitWidth());
            }
            break;
        }
        
        case TypeKind::POINTER: {
            auto* ptr_type = static_cast<PointerType*>(aria_type);
            llvm::Type* pointee = mapType(ptr_type->getPointeeType());
            // LLVM uses opaque pointers in newer versions
            llvm_type = llvm::PointerType::get(context, 0);
            break;
        }
        
        case TypeKind::ARRAY: {
            auto* arr_type = static_cast<ArrayType*>(aria_type);
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
        
        case TypeKind::FUNCTION: {
            // Function types: func(params) -> return
            // Reference: research_016
            auto* func_type = static_cast<FunctionType*>(aria_type);
            
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
            // Represented as { hasValue: i1, value: T, error: i8 }
            // Reference: research_016
            auto* result_type = static_cast<ResultType*>(aria_type);
            
            llvm::Type* value_type = mapType(result_type->getValueType());
            
            std::vector<llvm::Type*> result_fields = {
                builder.getInt1Ty(),    // hasValue flag
                value_type,              // Success value
                builder.getInt8Ty()     // Error code
            };
            
            llvm_type = llvm::StructType::get(context, result_fields);
            break;
        }
        
        case TypeKind::GENERIC: {
            // Generic types should be monomorphized before codegen
            // If we see one here, it's an error in the compiler pipeline
            // For now, treat as opaque pointer
            llvm_type = llvm::PointerType::get(context, 0);
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

} // namespace aria

// Define methods outside namespace to avoid ambiguity

llvm::Value* aria::IRGenerator::codegen(::ASTNode* node) {
    if (!node) {
        return nullptr;
    }
    
    // TODO: Implement codegen for different AST node types
    // This will be expanded in Phase 4.2 (Expression Code Generation)
    // and Phase 4.3 (Statement Code Generation)
    
    return nullptr;
}

llvm::Module* aria::IRGenerator::getModule() {
    return module.get();
}

void aria::IRGenerator::dump() {
    module->print(llvm::outs(), nullptr);
}
