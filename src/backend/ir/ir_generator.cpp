#include "backend/ir/ir_generator.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"  // Full type definitions needed
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/DerivedTypes.h>  // For FunctionType, StructType, etc.
#include <llvm/IR/DataLayout.h>     // For getTypeAllocSize
#include <llvm/IR/DebugLoc.h>       // For debug locations
#include <llvm/BinaryFormat/Dwarf.h>  // For DWARF constants
#include <cassert>

namespace aria {
using namespace sema;  // Now inside aria namespace

IRGenerator::IRGenerator(const std::string& module_name, bool enable_debug)
    : context(), 
      module(std::make_unique<llvm::Module>(module_name.empty() ? "aria_module" : module_name, context)),
      builder(context),
      di_builder(nullptr),
      di_compile_unit(nullptr),
      di_file(nullptr),
      debug_enabled(enable_debug) {
    // Set source filename for better debug info
    module->setSourceFileName(module_name.empty() ? "aria_module" : module_name);
    
    if (debug_enabled) {
        // Create DIBuilder (initialization deferred to initDebugInfo)
        di_builder = std::make_unique<llvm::DIBuilder>(*module);
    }
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

llvm::Value* aria::IRGenerator::codegen(aria::ASTNode* node) {
    if (!node) {
        return nullptr;
    }
    
    // Basic implementation for Phase 7.1.1-7.1.2 stub
    // Full implementation in Phase 4.2 (Expression Code Generation)
    // and Phase 4.3 (Statement Code Generation)
    
    // For multi-file compilation: Check if this module's AST contains a main function
    // by looking at the node type and checking for main in function declarations
    // For now, use module name to create unique function names
    
    // Extract module base name from full path (e.g., "utils" from "tests/integration/utils.aria")
    std::string module_name = module->getName().str();
    size_t last_slash = module_name.find_last_of("/\\");
    if (last_slash != std::string::npos) {
        module_name = module_name.substr(last_slash + 1);
    }
    size_t last_dot = module_name.find_last_of('.');
    if (last_dot != std::string::npos) {
        module_name = module_name.substr(0, last_dot);
    }
    
    // Create a module init function (or main if module name suggests it's the main file)
    bool is_main_module = (module_name.find("main") != std::string::npos || 
                           module_name == "hello");
    std::string func_name = is_main_module ? "main" : ("__" + module_name + "_init");
    
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
    
    // Return 0 for now
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
        "Aria Compiler v0.0.7",  // Producer
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
            auto* ptr_type = static_cast<PointerType*>(aria_type);
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
            auto* arr_type = static_cast<ArrayType*>(aria_type);
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
